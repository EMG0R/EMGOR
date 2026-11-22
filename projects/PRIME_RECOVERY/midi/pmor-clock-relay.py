#!/usr/bin/env python3
"""pmor-clock-relay: Midi Through -> PMOR, clock and sync only (2026-10-24).

PMOR is a clock follower. demiurge-midi-connect used to wire the whole hub
(Midi Through 14:0) into it, so PMOR also received every NEPTR sensor CC
(~80 msg/s) and its USB-MIDI input could not drain: the kernel logged
`seq_midi: MIDI output buffer overrun` ~120x/min, and a full buffer drops
clock bytes too. This relay forwards only what PMOR consumes:
  0xF8 clock, 0xFA start, 0xFB continue, 0xFC stop, 0xF2 song position,
  ch16 CC115 / CC116 / CC119 (phase, role announce, tempo/sync).
demiurge-midi-connect's clock-sink pass is disabled by a drop-in
(DEMIURGE_MIDI_CLOCK_SINKS=none) so the two don't both wire PMOR.
Reconciles every 2 s: PMOR replugged -> reopened; hub direct link removed.
Also removes hub -> Teensy MIDI / Bela / DUODECIMUS every 2 s: those never read their input.

PRECISE TEMPO (role = opgorator, PMOR is master): NEPTR's loop grid only got
tempo as the 14-bit CC120/121 number (0.022 BPM steps, ~240 ppm at 124 BPM),
and the HAT audio clock runs ~-57 ppm against system time, so a loop captured
against PMOR drifted ~14 ms/min. TempoMeter fits PMOR's own 0xF8 ticks (read
straight from its port, not the hub) over a 20 s window, fits the HAT's hw_ptr
against system time over 60 s, and sends the tempo in audio-clock terms to
csound as OSC /clk/bpm f on 127.0.0.1:7770 every 0.5 s (midi_handler.orc).
"""
import os, re, subprocess, sys, threading, time
from collections import deque
import rtmidi
from pythonosc.udp_client import SimpleUDPClient

HUB = "Midi Through"
DEV = ("OpGorator", "Daisy", "PMOR")
REALTIME = {0xF8, 0xFA, 0xFB, 0xFC, 0xF2}
SYNC_CCS = {115, 116, 119}
BEATS_PER_BAR = 4              # the one place the bar length lives (PMOR: 16 sixteenths)
TICKS_PER_16TH = 6             # SPP unit = 6 MIDI clocks
SPP_WRAP_TICKS = 16384 * TICKS_PER_16TH   # SPP is 14 bit: wraps every 4096 beats
SPP_CONT_WINDOW_S = 0.5        # a 0xFB counts as "positioned" if an SPP came this recently


def log(m):
    print(f"pmor-clock-relay: {m}", flush=True)


def find(ports, pats):
    for i, p in enumerate(ports):
        if any(s in p for s in pats):
            return i, p
    return None, None


NO_FEED = ("Teensy MIDI", "Bela", "DUODECIMUS")   # 2026-11-02: DUODECIMUS too (fed hub clock+CCs it never reads; needed replugs)


def no_feed_clients():
    try:
        out = subprocess.run(["aconnect", "-l"], capture_output=True, text=True).stdout
    except Exception:
        return []
    ids = []
    for ln in out.splitlines():
        if ln.startswith("client ") and any(f"'{n}" in ln for n in NO_FEED):
            ids.append(ln.split()[1].rstrip(":"))
    return ids


HAT_STATUS = "/proc/asound/sndrpihifiberry/pcm0p/sub0/status"
NOMINAL_SR = 48000.0


def lsq_slope(pts):
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    sxx = sum((p[0] - mx) ** 2 for p in pts)
    if sxx <= 0:
        return None
    return sum((p[0] - mx) * (p[1] - my) for p in pts) / sxx


class TempoMeter:
    """PMOR 0xF8 period (system time) x HAT sample rate (vs system time)."""
    TICK_WIN_S, SR_WIN_S = 20.0, 60.0
    SHORT_WIN_S = 3.0             # ramp-following window
    BEAT_FIT_TICKS = 96

    def __init__(self):
        self.lock = threading.Lock()
        self.ticks = deque()          # accepted tick times
        self.fits = deque(maxlen=5)   # recent tick-fit BPMs (stability guard in bpm())
        self.raw = deque(maxlen=48)   # raw recent intervals (incl. duplicates)
        self.cc_msb = None
        self.cc_bpm = None
        self.cc_t = 0.0
        self.last_t = None
        self.sr = deque()             # (tstamp, hw_ptr)
        self.trig = None
        self.shifts = 0               # tempo steps detected (debug)
        # beat counting for the loop re-lock: tick index since PMOR's 0xFA Start
        self.playing = False          # only True after a real Start (not Continue)
        self.tick_n = -1              # index of the last accepted tick since Start
        self.idx = deque()            # tick index parallel to self.ticks
        self.beats = deque()          # (beat_no, predicted beat time) to send
        # SPP (0xF2) position tracking. tick_n is a monotonically increasing
        # ABSOLUTE tick count (SPP wraps, tick_n never does); beat = tick_n // 24.
        self.pos_known = False        # tick_n came from a Start/SPP, not a guess
        self.spp_t = None             # when the last SPP arrived (for Continue)
        self.spp_fixes = 0            # times an SPP disagreed with our count
        self.spp_log_t = 0.0

    def on_tick(self, t):
        # Robust (2026-10-24 rewrite). The first version rejected "duplicate" ticks
        # against its OWN period estimate; once that estimate went high it threw
        # away real ticks, ran away downward and fed NEPTR 36-62 BPM while PMOR
        # played ~85 -> the loop varispeeded. Now duplicates are judged against
        # the median of RAW recent intervals (3% dups cannot move a median).
        with self.lock:
            if self.last_t is not None:
                dt = t - self.last_t
                if dt > 1.0:          # transport stop / replug: start over
                    self.ticks.clear(); self.raw.clear(); self.idx.clear()
                else:
                    med = sorted(self.raw)[len(self.raw) // 2] if len(self.raw) >= 24 else None
                    prev = self.raw[-1] if self.raw else None
                    self.raw.append(dt)
                    # A duplicate follows a NORMAL interval; a real tick that was
                    # merely delivered right behind a LATE one follows a long gap.
                    if med and dt < 0.3 * med and not (prev and prev > 1.3 * med):
                        self.raw[-1] = med  # keep the interval stats clean
                        return        # PMOR duplicate 0xF8
            self.last_t = t
            self.ticks.append(t)
            if self.playing:
                self.tick_n += 1
            self.idx.append(self.tick_n)
            while self.ticks and t - self.ticks[0] > self.TICK_WIN_S:
                self.ticks.popleft(); self.idx.popleft()
            self._detect_shift()
            if self.playing and self.tick_n % 24 == 0:
                self._beat(t)

    def _restart_window(self, keep):
        """Drop every tick but the newest `keep` (tick_n indices stay
        continuous). Called when the tempo has changed: a line fit over a
        window holding two tempi is wrong for TICK_WIN_S seconds."""
        while len(self.ticks) > keep:
            self.ticks.popleft(); self.idx.popleft()
        self.fits.clear()

    def _detect_shift(self):
        """Tempo STEP detector, backup path (caller holds the lock). The primary
        detector is PMOR's own tempo number (on_cc): PMOR sends CC120/121 the
        instant its tempo changes. This one only catches gear that never sends it.
        2026-11-06: was median-of-8 vs median-of-32 at 2 %. PMOR's ticks arrive on
        1 ms USB frames (intervals quantised, +-6 % of a tick at ~155 BPM), so that
        median flipped from noise alone, restarted the fit every few beats and threw
        away half the /clk/beat messages. Now: MEAN of the newest 24 vs the 48 before,
        > 5 %, and it must hold on 6 consecutive ticks."""
        if len(self.raw) < 72:
            return
        r = list(self.raw)
        new = sum(r[-24:]) / 24.0
        old = sum(r[-72:-24]) / 48.0
        if old > 0 and abs(new / old - 1) > 0.05:
            self.shift_run = getattr(self, "shift_run", 0) + 1
        else:
            self.shift_run = 0
        if getattr(self, "shift_run", 0) >= 6:
            self.shift_run = 0
            self._tempo_step()

    def _tempo_step(self):
        """PMOR changed tempo: restart the fit window at the change (caller holds the lock)."""
        r = list(self.raw)
        self._restart_window(6)
        keep = r[-8:]
        self.raw.clear(); self.raw.extend(keep)
        self.shifts += 1

    def _beat(self, t):
        """Predict this beat's true time from a line fit over the ticks counted
        since Start (index -> time), so one late-delivered tick cannot move it.
        Skipped (no send) if the fit is not ready or this tick sits more than
        30 % of a tick off the line (miscount suspicion)."""
        # LOCAL line fit: the newest BEAT_FIT_TICKS ticks only (4 beats). Over the
        # whole 20 s window a tempo RAMP bends the index->time curve (quadratic
        # term ~180 ms at 0.5 BPM/s) and every beat failed the 30 % gate.
        pts = [(n, tt) for n, tt in zip(self.idx, self.ticks) if n >= 0][-self.BEAT_FIT_TICKS:]
        if len(pts) < 48:
            return
        b = lsq_slope(pts)
        if not b:
            return
        mx = sum(p[0] for p in pts) / len(pts); my = sum(p[1] for p in pts) / len(pts)
        tb = my + b * (self.tick_n - mx)
        if abs(t - tb) > 0.5 * b:   # 2026-11-06: PMOR tick jitter is +-6 ms (0.35 tick); a miscount is a whole tick
            return
        self.beats.append((self.tick_n // 24, tb))

    def on_spp(self, pos16, now=None):
        """0xF2 Song Position Pointer: `pos16` sixteenths (14 bit) = the position
        of the NEXT tick. tick_n is set so that tick gets index n, where n is the
        value congruent to pos16*6 (mod SPP_WRAP_TICKS) NEAREST the predicted
        next index, so a wrap never makes the count jump backward. Agreeing SPP
        (the per-bar check) changes nothing. The tick window is NOT cleared
        (tempo fit survives); only the beat-fit indices are invalidated when
        the count had to be corrected."""
        now = time.monotonic() if now is None else now
        tgt = (int(pos16) & 0x3FFF) * TICKS_PER_16TH
        with self.lock:
            self.spp_t = now
            pred = self.tick_n + 1 if self.pos_known else tgt
            n = tgt + SPP_WRAP_TICKS * round((pred - tgt) / SPP_WRAP_TICKS)
            if self.pos_known and self.playing and n == pred:
                return                          # per-bar check agrees
            if self.pos_known and self.playing:
                self.spp_fixes += 1
                if now - self.spp_log_t > 10.0:
                    self.spp_log_t = now
                    log(f"SPP corrected tick count {pred} -> {n} (fix #{self.spp_fixes})")
            # ticks already in the window have no valid index relative to n
            # (first SPP mid-song, or a correction): beat fit restarts on new ticks
            self.idx = deque(-1 for _ in self.ticks)
            self.tick_n = n - 1
            self.playing = True
            self.pos_known = True
            self.beats.clear()

    def on_transport(self, st, now=None):
        now = time.monotonic() if now is None else now
        with self.lock:
            if st == 0xFA:            # Start: next tick is beat 0, tick 0
                self.playing, self.tick_n, self.pos_known = True, -1, True
                self.idx = deque(-1 for _ in self.ticks)
                self.beats.clear()
                self.spp_t = None
            elif st == 0xFB:          # Continue: only with an SPP just before it
                if self.spp_t is not None and now - self.spp_t <= SPP_CONT_WINDOW_S:
                    self.playing = True
                else:                 # old firmware: position unknown
                    self.playing = False
                    self.beats.clear()
                self.spp_t = None
            else:                     # Stop
                self.playing = False
                self.beats.clear()
                self.spp_t = None

    def on_cc(self, cc, val):
        """PMOR's own 14-bit tempo number (ch16 CC120 MSB, CC121 LSB): the guard."""
        if cc == 120:
            self.cc_msb = val
        elif cc == 121 and self.cc_msb is not None:
            bpm = 40 + (self.cc_msb * 128 + val) * (360.0 / 16383.0)
            prev = self.cc_bpm
            self.cc_bpm = bpm
            self.cc_t = time.monotonic()
            # PRIMARY tempo-step detector: PMOR's own number moved by > 0.5 %.
            if prev and abs(bpm / prev - 1) > 0.005:
                with self.lock:
                    self._tempo_step()

    def period_locked(self, pts=None):
        """Seconds per tick: least-squares fit of tick time against tick count
        (duplicates already removed, so count = real ticks; a late-delivered
        tick is one outlier among hundreds)."""
        pts = list(self.ticks) if pts is None else pts
        if len(pts) < 96:
            return None
        return lsq_slope(list(enumerate(pts)))

    def sample_hat(self):
        try:
            txt = open(HAT_STATUS).read()
            ts = float(re.search(r"^tstamp\s*:\s*([\d.]+)", txt, re.M).group(1))
            hw = int(re.search(r"^hw_ptr\s*:\s*(\d+)", txt, re.M).group(1))
            tr = re.search(r"^trigger_time\s*:\s*([\d.]+)", txt, re.M).group(1)
        except Exception:
            return
        if tr != self.trig or (self.sr and hw < self.sr[-1][1]):
            self.sr.clear(); self.trig = tr          # stream restarted
        if not self.sr or ts > self.sr[-1][0]:
            self.sr.append((ts, hw))
        while self.sr and ts - self.sr[0][0] > self.SR_WIN_S:
            self.sr.popleft()

    def hat_rate(self):
        if len(self.sr) < 5 or self.sr[-1][0] - self.sr[0][0] < 8:
            return NOMINAL_SR                        # not enough yet: assume nominal
        r = lsq_slope(list(self.sr))
        return r if r and abs(r / NOMINAL_SR - 1) < 0.0003 else NOMINAL_SR

    def bpm(self):
        with self.lock:
            if not self.ticks or time.monotonic() - self.ticks[-1] > 1.0:
                return None
            span = self.ticks[-1] - self.ticks[0]
            cc_bpm, cc_age = self.cc_bpm, time.monotonic() - self.cc_t
            if cc_bpm is None or cc_age > 6.0:
                return None
            scale = NOMINAL_SR / self.hat_rate()
            per_long = self.period_locked() if span >= 10.0 else None
            # short window (RAMP / just after a step): newest SHORT_WIN_S seconds
            tl = self.ticks[-1]
            short = [x for x in self.ticks if tl - x <= self.SHORT_WIN_S]
            per_short = self.period_locked(short) if span >= self.SHORT_WIN_S else None
            if per_long and per_short and abs(per_short / per_long - 1) > 1000e-6:
                # tempo is MOVING (ramp) or stepped without tripping the step
                # detector: the long fit is a blend. Restart the window at the
                # short one and follow it.
                self._restart_window(len(short))
                per_long = None
        # tempo moving / window just restarted: short fit if it exists and
        # agrees with PMOR's own number to 1.5 %, else the CC number. Never
        # None here, so gkClkExtLive stays up through the change.
        if not per_long:
            if per_short:
                b = 60.0 / (per_short * 24)
                if abs(b / cc_bpm - 1) < 0.015:
                    return b * scale
            return cc_bpm * scale
        bpm_sys = 60.0 / (per_long * 24)
        # GUARD: PMOR's own tempo number is its real tempo rounded to 0.022 BPM,
        # so the true value lies within +-0.011 of it. A fit outside +-0.015 is a
        # miscount (offline test: ~1 in 6 windows off by 300-800 ppm) -> send
        # nothing; NEPTR falls back to the CC number within 3 s. Worst case is
        # therefore the CC's own rounding, never worse.
        # 2026-11-02: the +-0.015 guard rejected PMOR FOREVER: its crystal runs
        # ~200 ppm off its own number (fit 92.756 vs CC 92.738 = 0.019), so no
        # /clk/bpm was ever sent and NEPTR's loop grid (gkClkExtLive, /clk/bpm
        # only since a7b87ce) saw no live clock with PMOR plugged in. Now: guard
        # is 600 ppm (a miscount is 300-800 ppm AND unstable, so also require the
        # fit to agree with the median of the last 5 within 60 ppm); a rejected
        # fit sends PMOR's own CC tempo instead of nothing, so the grid stays live.
        self.fits.append(bpm_sys)
        med = sorted(self.fits)[len(self.fits) // 2]
        stable = len(self.fits) >= 3 and abs(bpm_sys / med - 1) < 60e-6
        if abs(bpm_sys / cc_bpm - 1) > 600e-6 or not stable:
            return cc_bpm * scale
        # beat = 60/bpm_sys system seconds = that many * hat_rate samples;
        # the looper uses sr*60/bpm with sr = 48000, so scale accordingly
        return bpm_sys * scale


def tempo_thread(meter, role):
    mi = rtmidi.MidiIn(rtmidi.API_LINUX_ALSA, "pmor-tempo-in")
    mi.ignore_types(sysex=True, timing=False, active_sense=True)
    osc = SimpleUDPClient("127.0.0.1", 7770)
    opened = None
    def on_ev(ev, _):
        m = ev[0]
        if not m:
            return
        if m[0] == 0xF8:
            meter.on_tick(time.monotonic())
        elif m[0] == 0xBF and len(m) > 2:
            meter.on_cc(m[1], m[2])
        elif m[0] == 0xF2 and len(m) > 2:
            meter.on_spp(m[1] | (m[2] << 7))
        elif m[0] in (0xFA, 0xFB, 0xFC):
            meter.on_transport(m[0])
    last_log = 0
    while True:
        j, name = find(mi.get_ports(), DEV)
        if name != opened:
            if mi.is_port_open():
                mi.close_port()
            opened = None
            if j is not None:
                mi.open_port(j, "pmor-ticks"); opened = name
                # python-rtmidi's close_port() cancels the callback: set it on EVERY
                # open, or a PMOR replug leaves the queue filling ("message queue
                # limit reached", 44k lines on 2026-10-24) and the tempo frozen.
                mi.set_callback(on_ev)
                log(f"tempo meter on {name}")
        meter.sample_hat()
        b = meter.bpm()
        if b and role[0] == "opgorator":
            osc.send_message("/clk/bpm", round(b, 4))
            if time.time() - last_log > 60:
                log(f"/clk/bpm {b:.4f} (hat {meter.hat_rate():.3f} Hz, {len(meter.ticks)} ticks)")
                last_log = time.time()
        # beats for the loop re-lock: drained every 50 ms between the 0.5 s
        # tempo updates; elapsed samples are computed at send time, so a
        # queued beat is never mis-timed, just later. Only sent while the
        # tempo is locked (b) and PMOR is master.
        for _ in range(10):
            while meter.beats:
                bn, tb = meter.beats.popleft()
                if b and role[0] == "opgorator":
                    osc.send_message("/clk/beat", [int(bn), float((time.monotonic() - tb) * meter.hat_rate())])
                    osc.send_message("/clk/bar", [int(bn) // BEATS_PER_BAR, int(bn) % BEATS_PER_BAR])
            time.sleep(0.05)


def read_role():
    """~/demiurge/sync.conf role (demiurge-sync). demiurge = Pi is clock master,
    so PMOR gets clock; opgorator = PMOR is master, so no clock is sent to it."""
    try:
        for ln in open(os.path.expanduser("~/demiurge/sync.conf")):
            k, _, v = ln.partition("=")
            if k.strip() == "role":
                return v.strip()
    except OSError:
        pass
    return "demiurge"


def main():
    mi = rtmidi.MidiIn(rtmidi.API_LINUX_ALSA, "pmor-relay-in")
    mi.ignore_types(sysex=True, timing=False, active_sense=True)
    mo = rtmidi.MidiOut(rtmidi.API_LINUX_ALSA, "pmor-relay-out")
    out_name = None
    role = [read_role()]
    threading.Thread(target=tempo_thread, args=(TempoMeter(), role), daemon=True).start()

    def cb(ev, _):
        msg = ev[0]
        if not msg or out_name is None:
            return
        s = msg[0]
        if s in REALTIME and role[0] != "demiurge":
            return              # PMOR is master: never echo clock back into it
        if s in REALTIME or (s == 0xBF and len(msg) > 1 and msg[1] in SYNC_CCS):
            try:
                mo.send_message(msg)
            except Exception:
                pass

    i, _ = find(mi.get_ports(), (HUB,))
    if i is None:
        log("Midi Through not found"); sys.exit(1)
    mi.open_port(i, "from-hub")
    mi.set_callback(cb)
    log("listening on Midi Through")
    while True:
        ports = mo.get_ports()
        j, name = find(ports, DEV)
        if name != out_name:
            if mo.is_port_open():
                mo.close_port()
            out_name = None
            if j is not None:
                mo.open_port(j, "to-pmor")
                out_name = name
                log(f"forwarding clock/sync -> {name}")
            else:
                log("PMOR not present")
        if out_name:
            # drop a direct hub -> PMOR subscription if anything re-adds it
            client = out_name.rsplit(" ", 1)[-1]
            r = subprocess.run(["aconnect", "-d", "14:0", client], capture_output=True)
            if r.returncode == 0:
                log(f"removed direct 14:0 -> {client}")
        # Hub -> device links that must not exist: TEENSYY.ino only drains and
        # discards USB-MIDI input (`while (usbMIDI.read()) {}`), too slowly for
        # the ~140 msg/s hub stream -> `seq_midi: MIDI output buffer overrun`
        # ~2/s (measured 2026-10-24: 30/12 s linked, 0/10 s unlinked). The old
        # udev demiurge-midi-connect.sh wires every card both ways and re-adds it.
        for c in no_feed_clients():
            if subprocess.run(["aconnect", "-d", "14:0", f"{c}:0"], capture_output=True).returncode == 0:
                log(f"removed hub 14:0 -> {c} (device never reads it)")
        role[0] = read_role()
        time.sleep(2)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""HAT watchdog v2 (PROPOSED, NOT DEPLOYED): v1 plus "running but silent / misaligned".

v1 only sees the ALSA DMA state and hw_ptr. Measured 2026-10-22: after a kernel
`dma dma2chanN: BUG: IRQ with no descriptors` the HAT stays RUNNING with hw_ptr
advancing while the DAC output is silent for 30-60 s, until PipeWire restarts the
stream on its own. v1 never fires on that. v2 adds a loopback pilot:

  playback_AUX6 <- 17250 Hz  @ -52 dBFS   ... cable OUT7->IN7 ...   capture_AUX6
  playback_AUX7 <- 19500 Hz  @ -52 dBFS   ... cable OUT8->IN8 ...   capture_AUX7

Both are exact 64-frame patterns (23/64 and 13/32 cycles per sample), so the
stream is a pure looped table with no phase jump. Detector = Goertzel per 0.25 s
window on each capture channel at both pilot frequencies:
  OK         own pilot present, other pilot absent (>=30 dB down) on both channels
  SILENT     neither pilot present on either channel          (output dead)
  MISALIGNED a pilot present on the wrong channel, or only one channel alive
A fault must persist BAD_S (0.4 s if the kernel just logged the DMA BUG) before
`hat-reset` is called. The detector arms only after ARM_OK consecutive OK windows,
so a missing/unplugged cable never causes a reset. Resets are rate limited.
It only uses pw-play / pw-record / pw-link (PipeWire clients); never opens the HAT.
The pilot nodes are persistent (no create/destroy churn) and are re-linked after
every HAT reopen. Pilot lands in csound input7/8 -> PMOR strip at -52 dBFS, above
16 kHz: measure with --selftest before deploying.

Modes:  (none)      run as daemon (replaces hat-watchdog.py)
        --selftest  pilot for ~6 s, print per-window levels, never reset, exit
        --dry       daemon logic but only log would-be resets
"""
import math, os, signal, struct, subprocess, sys, threading, time

CARD = "/proc/asound/sndrpihifiberry"
OUT = "alsa_output.platform-soc_107c000000_sound.pro-output-0"
INP = "alsa_input.platform-soc_107c000000_sound.pro-input-0"
PLAY, REC = "primewd_play", "primewd_rec"
FS = 48000
F_L, F_R = 17250.0, 19500.0
AMP_DBFS = -52.0
WIN = 12000                       # 0.25 s
OK_DB = -75.0                     # pilot peak dBFS counted as present
REJECT_DB = 30.0                  # other pilot must be this far below
ARM_OK = 8                        # consecutive OK windows before a loop counts as known-good (2 s)
GRACE_S = 3.0                     # after the card (re)opens, wait this long before judging the pilot
FORGET_AFTER = 2                  # consecutive resets that don't bring a known-good loop back -> forget it (cable pulled)
BAD_S, BAD_S_KERNEL = 1.5, 0.4
COOLDOWN_S, MAX_RESETS, MAX_WINDOW_S = 20.0, 12, 600.0   # 2026-11-02: 4 gave up mid-set (thermal xruns restart the stream every ~2 min); a reset is a ~0.3-1 s gap
POLL_S, STALL_S = 0.05, 0.75          # 50 ms poll (2026-10-24): stream-restart reset fires within ~50 ms
LOG = os.environ.get("HAT_WD_LOG", os.path.expanduser("~/PRIME_RECOVERY/logs/hat-watchdog.log"))
DRY = "--dry" in sys.argv


def log(msg):
    line = time.strftime("%F %T ") + msg
    print(line, flush=True)
    try:
        with open(LOG, "a") as f:
            f.write(line + "\n")
    except OSError:
        pass


def sh(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


# ------------------------------------------------------------------ v1 checks
def alsa_status(d):
    try:
        txt = open(f"{CARD}/pcm0{d}/sub0/status").read()
    except OSError:
        return None, None
    st, hw = "CLOSED", None
    for ln in txt.splitlines():
        if ln.startswith("state:"):
            st = ln.split(":", 1)[1].strip()
        elif ln.startswith("hw_ptr"):
            hw = int(ln.split(":", 1)[1])
    return st, hw


def trigger_time(d):
    """ALSA trigger_time: changes every time the stream is (re)started."""
    try:
        for ln in open(f"{CARD}/pcm0{d}/sub0/status"):
            if ln.startswith("trigger_time:"):
                return ln.split(":", 1)[1].strip()
    except OSError:
        pass
    return None


# ------------------------------------------------------------------ kernel hint
class Kmsg(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.last = 0.0

    def run(self):
        while True:
            try:
                p = subprocess.Popen(["dmesg", "--follow-new", "--notime"], stdout=subprocess.PIPE, text=True)
                for ln in p.stdout:
                    if "IRQ with no descriptors" in ln:
                        self.last = time.time()
                        log("kernel: " + ln.strip())
            except Exception:
                pass
            time.sleep(2)


# ------------------------------------------------------------------ pilot
def make_pattern():
    a = 10 ** (AMP_DBFS / 20) * 2147483647
    fr = bytearray()
    for n in range(64):
        l = int(round(a * math.sin(2 * math.pi * F_L * n / FS)))
        r = int(round(a * math.sin(2 * math.pi * F_R * n / FS)))
        fr += struct.pack("<ii", l, r)
    return bytes(fr)


def goertzel(x, f):
    w = 2 * math.pi * f / FS
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in x:
        s0 = v + c * s1 - s2
        s2, s1 = s1, s0
    p = s1 * s1 + s2 * s2 - c * s1 * s2
    amp = 2 * math.sqrt(max(p, 0.0)) / len(x)            # peak amplitude, full scale = 1
    return 20 * math.log10(amp + 1e-12)


class Pilot:
    def __init__(self):
        self.play = self.rec = None
        self.stop = False
        self.buf = bytearray()
        self.lock = threading.Lock()
        self.last_data = 0.0

    def start(self):
        nice = ["taskset", "-c", "0-2", "nice", "-n", "10"]
        self.play = subprocess.Popen(nice + ["pw-play", "-a", "--target", "0", "--rate", str(FS), "--format", "s32",
                                             "--channels", "2", "--latency", "512",
                                             "-P", f"node.name={PLAY} node.dont-reconnect=true", "-"],
                                     stdin=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.rec = subprocess.Popen(nice + ["pw-record", "-a", "--target", "0", "--rate", str(FS), "--format", "s32",
                                            "--channels", "2", "--latency", "512",
                                            "-P", f"node.name={REC} node.dont-reconnect=true", "-"],
                                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        threading.Thread(target=self._feed, daemon=True).start()
        threading.Thread(target=self._drain, daemon=True).start()

    def _feed(self):
        pat = make_pattern() * 64                       # 4096 frames per write
        try:
            while not self.stop:
                self.play.stdin.write(pat); self.play.stdin.flush()
        except (BrokenPipeError, ValueError, OSError):
            pass

    def _drain(self):
        try:
            while not self.stop:
                b = self.rec.stdout.read(4096)
                if not b:
                    break
                with self.lock:
                    self.buf += b
                    del self.buf[:max(0, len(self.buf) - 8 * WIN * 8)]
                    self.last_data = time.time()
        except (ValueError, OSError):
            pass

    def alive(self):
        return self.play and self.rec and self.play.poll() is None and self.rec.poll() is None

    def link(self):
        """idempotent: (re)create the four links; quiet if they already exist."""
        have = sh(["pw-link", "-l"]).stdout
        want = [(f"{PLAY}:output_FL", f"{OUT}:playback_AUX6"), (f"{PLAY}:output_FR", f"{OUT}:playback_AUX7"),
                (f"{INP}:capture_AUX6", f"{REC}:input_FL"), (f"{INP}:capture_AUX7", f"{REC}:input_FR")]
        lines = have.splitlines()
        for a, b in want:
            present = any(lines[i].strip() == a and i + 1 < len(lines) and lines[i + 1].strip() == f"|-> {b}"
                          for i in range(len(lines)))
            if not present:
                sh(["pw-link", a, b])

    def unlink_and_stop(self):
        self.stop = True
        for a, b in [(f"{PLAY}:output_FL", f"{OUT}:playback_AUX6"), (f"{PLAY}:output_FR", f"{OUT}:playback_AUX7"),
                     (f"{INP}:capture_AUX6", f"{REC}:input_FL"), (f"{INP}:capture_AUX7", f"{REC}:input_FR")]:
            sh(["pw-link", "-d", a, b])
        for p in (self.play, self.rec):
            if p and p.poll() is None:
                p.terminate()

    def window(self):
        """newest complete 0.25 s window as ([L],[R]) floats, or None. Consumes it."""
        with self.lock:
            if time.time() - self.last_data > 0.5 or len(self.buf) < WIN * 8:
                return None
            raw = bytes(self.buf[-WIN * 8:])
            self.buf.clear()
        v = struct.unpack(f"<{WIN * 2}i", raw)
        return [x / 2147483648 for x in v[0::2]], [x / 2147483648 for x in v[1::2]]


def classify(win):
    """returns (verdict, levels dict)"""
    if win is None:
        return "NODATA", {}
    L, R = win
    lv = {"L@fL": goertzel(L, F_L), "L@fR": goertzel(L, F_R), "R@fL": goertzel(R, F_L), "R@fR": goertzel(R, F_R)}
    l_ok = lv["L@fL"] > OK_DB and lv["L@fR"] < lv["L@fL"] - REJECT_DB
    r_ok = lv["R@fR"] > OK_DB and lv["R@fL"] < lv["R@fR"] - REJECT_DB
    lv["_l_ok"], lv["_r_ok"] = l_ok, r_ok
    if l_ok and r_ok:
        return "OK", lv
    if max(v for k, v in lv.items() if not k.startswith("_")) < OK_DB:
        return "SILENT", lv
    return "MISALIGNED", lv


def selftest():
    p = Pilot(); p.start()
    t_end = time.time() + 6
    try:
        for _ in range(20):
            time.sleep(0.2)
            if len(sh(["pw-link", "-o"]).stdout.split(PLAY)) > 2:
                break
        p.link()
        mon = subprocess.Popen(["pw-record", "-a", "--target", "0", "--rate", str(FS), "--format", "s32", "--channels", "2",
                                "-P", "node.name=primewd_mon node.dont-reconnect=true", "-"],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        time.sleep(0.4)
        sh(["pw-link", f"{OUT}:monitor_AUX0", "primewd_mon:input_FL"]); sh(["pw-link", f"{OUT}:monitor_AUX1", "primewd_mon:input_FR"])
        print("window verdicts (peak dBFS L@17.25k L@19.5k R@17.25k R@19.5k):")
        while time.time() < t_end:
            time.sleep(0.3)
            v, lv = classify(p.window())
            print(v, {k: round(x, 1) for k, x in lv.items()})
        raw = mon.stdout.read(WIN * 8 * 2)
        v = struct.unpack(f"<{len(raw)//4}i", raw[:len(raw)//4*4]); v = [x / 2147483648 for x in v][-WIN * 2:]
        for nm, ch in (("main L", v[0::2]), ("main R", v[1::2])):
            print(f"pilot leak into engine output {nm}: 17.25k {goertzel(ch, F_L):.1f} dBFS, 19.5k {goertzel(ch, F_R):.1f} dBFS")
    finally:
        sh(["pw-link", "-d", f"{OUT}:monitor_AUX0", "primewd_mon:input_FL"]); sh(["pw-link", "-d", f"{OUT}:monitor_AUX1", "primewd_mon:input_FR"])
        try:
            mon.terminate()
        except Exception:
            pass
        p.unlink_and_stop()
        time.sleep(0.3)
        left = [l for l in sh(["pw-link", "-l"]).stdout.splitlines() if "primewd" in l]
        print("leftover links:", left)


def main():
    if "--selftest" in sys.argv:
        return selftest()
    kmsg = Kmsg(); kmsg.start()
    pilot = Pilot(); pilot.start()
    log(f"watchdog v2 up{' (DRY)' if DRY else ''}")
    last = {"p": (None, time.time()), "c": (None, time.time())}
    bad_since = cool_until = 0.0
    # Per-loop memory: a loop (L = OUT7->IN7, R = OUT8->IN8) becomes known-good after
    # ARM_OK clean windows and STAYS known-good across card closes/reopens (engine
    # restarts), so a card that comes back already wedged is still caught. A dead
    # loop (missing cable) is simply never learned; the other one still guards.
    known, ok_runs, failed_resets = set(), {"L": 0, "R": 0}, 0
    resets_t, grace_until, was_closed = [], time.time() + GRACE_S, False
    kmsg_handled = 0.0
    trig_last, own_reset_until = trigger_time("p"), 0.0
    next_win = next_link = 0.0
    pilot_state = "NODATA"
    while True:
        now = time.time()
        reasons = []
        # --- v1: DMA state / hw_ptr
        for d in ("p", "c"):
            st, hw = alsa_status(d)
            prev_hw, moved_at = last[d]
            if hw is not None and hw != prev_hw:
                last[d] = (hw, now)
            elif now - moved_at > STALL_S and st == "RUNNING":
                reasons.append(f"{'out' if d == 'p' else 'in'} hw_ptr stalled {now - moved_at:.1f}s")
            if st not in ("RUNNING", None, "CLOSED"):
                reasons.append(f"{'out' if d == 'p' else 'in'} state {st}")
        # --- v2: pilot
        if not pilot.alive():
            pilot = Pilot(); pilot.start(); grace_until = time.time() + GRACE_S; time.sleep(1)
        if now >= next_link:
            pilot.link(); next_link = now + 2.0
        if now >= next_win:
            v, lv = classify(pilot.window())
            next_win = now + 0.25
            if v != "NODATA" and v != pilot_state:
                log(f"pilot {pilot_state} -> {v} {({k: round(x, 1) for k, x in lv.items() if not k.startswith('_')})} known={sorted(known)}")
                pilot_state = v
            st_p, _ = alsa_status("p")
            if st_p in (None, "CLOSED"):                 # card closed on purpose / mid-reset: judge after reopen
                was_closed = True
            elif was_closed:
                was_closed, grace_until = False, now + GRACE_S
            if v != "NODATA" and not was_closed:     # NODATA = recorder hiccup, neither good nor bad
                for lane, ok in (("L", lv["_l_ok"]), ("R", lv["_r_ok"])):
                    ok_runs[lane] = ok_runs[lane] + 1 if ok else 0
                    if ok_runs[lane] >= ARM_OK:
                        if lane not in known:
                            known.add(lane); log(f"pilot loop {lane} known-good (armed)")
                        failed_resets = 0
                lost = [ln for ln in sorted(known) if not lv["_" + ln.lower() + "_ok"]]
                if lost and now >= grace_until:
                    reasons.append(f"pilot {v} (loop {'+'.join(lost)} lost)")
        # Kernel DMA fault with no known-good loop to judge by (loopback cable out):
        # the fault is followed by 18-57 s of silent output (measured 2026-10-22/23),
        # a reset costs ~4 s, so reset on the fault itself after 0.5 s.
        if kmsg.last > kmsg_handled and now - kmsg.last >= 0.5:
            if not known:
                reasons.append("kernel DMA IRQ-with-no-descriptors (no pilot loop to judge)")
                bad_since = bad_since or (now - BAD_S)
            kmsg_handled = kmsg.last
        # PipeWire restarted the HAT playback stream by itself (xrun recovery): the
        # step before every silent-DAC wedge seen 2026-10-22/23, and the 10:00 cut on
        # 2026-10-23 never came back on its own. Our own resets and engine restarts
        # (card closed) are excluded. Only acts when no pilot loop can judge.
        tt = trigger_time("p")
        if tt is not None and tt != trig_last:
            try:                               # hat-reset run by hand / UI button / us
                ext_reset = now - os.path.getmtime(f"/run/user/{os.getuid()}/hat-reset.stamp") < 15.0
            except OSError:
                ext_reset = False
            # A deliberate engine (re)start also restarts the HAT stream, and a
            # reset fired then snapshots the links before the launcher has made
            # them (2026-10-24: "reopened, 4 HAT links"). Ignore restarts within
            # 20 s of demiurge.service becoming active.
            if not ext_reset:
                r = sh(["systemctl", "show", "-p", "ActiveEnterTimestampMonotonic", "--value", "demiurge"])
                try:
                    ext_reset = time.monotonic() - int(r.stdout.strip() or 0) / 1e6 < 20.0
                except ValueError:
                    pass
            if not was_closed and not ext_reset and now >= own_reset_until and trig_last is not None:
                log(f"HAT playback stream restarted by PipeWire (trigger {trig_last} -> {tt})")
                if not known:
                    reasons.append("unexpected HAT stream restart (no pilot loop to judge)")
                    bad_since = bad_since or (now - BAD_S)
            trig_last = tt
        if reasons and now >= cool_until:
            bad_since = bad_since or now
            need = BAD_S_KERNEL if now - kmsg.last < 5 else BAD_S
            if now - bad_since >= need:
                resets_t = [t for t in resets_t if now - t < MAX_WINDOW_S]
                if len(resets_t) >= MAX_RESETS:
                    log(f"HAT DROP ({'; '.join(reasons)}) but {MAX_RESETS} resets in {MAX_WINDOW_S:.0f}s: not resetting")
                    cool_until, bad_since = now + 60, 0.0
                else:
                    log(f"HAT DROP ({'; '.join(reasons)}) -> hat-reset{' [DRY]' if DRY else ''}")
                    if not DRY:
                        r = sh(["/usr/local/bin/hat-reset"])
                        log("  " + (r.stdout.strip() or r.stderr.strip()))
                    resets_t.append(now)
                    own_reset_until = time.time() + 10.0
                    cool_until, bad_since = time.time() + COOLDOWN_S, 0.0
                    grace_until = time.time() + GRACE_S
                    if any("pilot" in r for r in reasons):
                        failed_resets += 1
                        if failed_resets > FORGET_AFTER:
                            log(f"pilot loops {sorted(known)} not back after {FORGET_AFTER} resets: forgetting them (cable pulled?)")
                            known.clear(); failed_resets = 0
                    ok_runs = {"L": 0, "R": 0}
                    for d in last:
                        last[d] = (None, time.time())
        elif not reasons:
            bad_since = 0.0
        time.sleep(POLL_S)


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, lambda *a: sys.exit(0))
    main()

#!/home/gorpi7v5/kivy-venv/bin/python
"""cardinal-ctl: Cardinal supervisor + radio-slot patch switcher + body-control router.

PROPOSED, NOT INSTALLED, NOT RUN (2026-10-23). Written by -C- PRIME NEPTR prep job.
`cardinal-ctl.py --selftest` runs the pure logic only (no ALSA, no spawn, no audio).

SLOTS (radio buttons). Eight body buttons select a patch directly, ch1 CC61-68 (body_control_map.md):
    slot1 A=65  slot2 B=66  slot3 X=67  slot4 Y=68  slot5 UP=61  slot6 DOWN=62  slot7 LEFT=63  slot8 RIGHT=64
  press (value 127) = switch to that slot; empty slot or the active slot = no-op. slots.json maps
  slot -> .vcv; button CCs live there too, so the UP/RIGHT capture ambiguity is a one-line edit
  (see ALWAYS_ON_PLAN.md). NEPTR reads none of CC61-68.

BACKENDS (slots.json "backend"; plan section 5 explains the risk ranking)
  restart : one Cardinal, SIGTERM + respawn with the new autosave. Slow (~4-8 s), graph churn, safest to reason about.
  osc     : one Cardinal, UDP /load blob (needs the GUI "Enable OSC remote control" toggle). UNVERIFIED.
  multi   : one Cardinal per used slot, preloaded; only the active one is linked and thawed, idle ones are
            SIGSTOPped. EXPERIMENTAL: a frozen client is only safe if PipeWire is not scheduling it.
            See ALWAYS_ON_PLAN.md section 5 before enabling.

BODY ROUTING (all emitted to the private "Cardinal Macros" ALSA port, which only Cardinal is linked to):
    ch9 CC30  probability  <- top-left encoder TURN (ch1 CC26), relative -> accumulated 0..127, persisted
    ch9 CC31  manual trig  <- arcade (ch1 CC60) 127/0, suppressed while the top-left encoder is HELD
    ch9 CC32  tone         <- accel X (ch1 CC31)  smoothed, deadbanded, range-scaled
    ch9 CC33  filter       <- accel Y (ch1 CC32)
    ch9 CC34  2nd tone     <- TOF1 (ch1 CC30)
    ch9 CC35  send/amount  <- TOF2 (ch1 CC36)
    ch9 CC36/37 FSR0/FSR1, CC38/39 joystick X/Y, CC53 accel Z  (spare, MIDI-learn)
  TAP TEMPO is NOT sent here. "Top-left encoder click held + arcade" is detected in serial_bridge.py
  (proposed/serial_bridge_tap_gate.patch), which already owns the CC113 tap and its timing. Sending it from two
  places would be two mechanisms for one job. This daemon only (a) knows the encoder is held and (b) suppresses
  CC31 during the gesture.
  ch1 CC29 (DUODECIMUS pedal) is never read, mirrored or sent.
"""
from __future__ import annotations

import json
import math
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HOME = os.path.expanduser("~")
SLOTS_FILE = os.environ.get("CARDINAL_SLOTS", f"{HERE}/slots.json")
STATE_DIR = os.environ.get("CARDINAL_STATE_DIR", f"{HOME}/.local/state/cardinal-ctl")
STATE_FILE = f"{STATE_DIR}/state.json"
PAUSE_FILE = f"{STATE_DIR}/PAUSE"          # written by cardinal-guard: nothing runs while it exists
RUN_HOME = os.environ.get("CARDINAL_HOME", f"{HOME}/.local/state/cardinal-home")
CARDINAL_BIN = os.environ.get("CARDINAL_BIN", "/opt/demiurge/cardinal/bin/Cardinal")
GRAPH_CONF = os.environ.get("DEMIURGE_GRAPH_CONF", f"{HOME}/demiurge/graph.conf")
HAT_OUT = "alsa_output.platform-soc_107c000000_sound.pro-output-0"
MIDI_IN_HINT = "Midi Through"
MACRO_PORT = "Cardinal Macros"
REALTIME = ("clock", "start", "continue", "stop")
LINK_SH = f"{HERE}/cardinal-link.sh"
CRASH_BACKOFF = (2, 4, 8, 15, 30)

CH1, CH9 = 0, 8
CC_TL_ROT, CC_TL_CLICK, CC_ARCADE = 26, 76, 60
DEFAULT_SLOTS = {
    "backend": "restart",
    "patch_dir": f"{HOME}/demiurge_vcv/instruments",
    "default_slot": 5,
    "debounce_s": 0.15,
    "min_gap_s": 3.0,
    "slots": [
        {"slot": 1, "button": "A",     "cc": 65, "patch": "01_instrument-A.vcv"},
        {"slot": 2, "button": "B",     "cc": 66, "patch": "02_instrument-B.vcv"},
        {"slot": 3, "button": "X",     "cc": 67, "patch": "03_instrument-C.vcv"},
        {"slot": 4, "button": "Y",     "cc": 68, "patch": "04_instrument-D.vcv"},
        {"slot": 5, "button": "UP",    "cc": 61, "patch": "05_flsamp.vcv"},
        {"slot": 6, "button": "DOWN",  "cc": 62, "patch": None},
        {"slot": 7, "button": "LEFT",  "cc": 63, "patch": None},
        {"slot": 8, "button": "RIGHT", "cc": 64, "patch": None},
    ],
    # input = raw 0..127 as the bridge sends it (accel_scale: +-17000 -> 0..127 centered 64; tof_scale: 2..46 cm -> 0..127,
    # out-of-range frames are not sent at all, so the last value is held)
    "shaping": {
        "accel_x": {"in_cc": 31, "out_cc": 32, "lo": 24, "hi": 104, "tau_s": 0.06, "deadband": 1, "center_dz": 0.03},
        "accel_y": {"in_cc": 32, "out_cc": 33, "lo": 24, "hi": 104, "tau_s": 0.06, "deadband": 1, "center_dz": 0.03},
        "tof1":    {"in_cc": 30, "out_cc": 34, "lo": 4,  "hi": 100, "tau_s": 0.10, "deadband": 2, "center_dz": 0.0},
        "tof2":    {"in_cc": 36, "out_cc": 35, "lo": 4,  "hi": 100, "tau_s": 0.10, "deadband": 2, "center_dz": 0.0},
        "accel_z": {"in_cc": 33, "out_cc": 53, "lo": 24, "hi": 104, "tau_s": 0.06, "deadband": 1, "center_dz": 0.03},
        "fsr0":    {"in_cc": 34, "out_cc": 36, "lo": 0,  "hi": 127, "tau_s": 0.02, "deadband": 1, "center_dz": 0.0},
        "fsr1":    {"in_cc": 35, "out_cc": 37, "lo": 0,  "hi": 127, "tau_s": 0.02, "deadband": 1, "center_dz": 0.0},
        "joy_x":   {"in_cc": 27, "out_cc": 38, "lo": 0,  "hi": 127, "tau_s": 0.02, "deadband": 1, "center_dz": 0.0},
        "joy_y":   {"in_cc": 28, "out_cc": 39, "lo": 0,  "hi": 127, "tau_s": 0.02, "deadband": 1, "center_dz": 0.0},
    },
}


def log(msg: str) -> None:
    print(f"[cardinal-ctl] {msg}", flush=True)


# ------------------------------------------------------------------ pure logic
def load_cfg(path: str = SLOTS_FILE) -> dict:
    cfg = json.loads(json.dumps(DEFAULT_SLOTS))
    try:
        with open(path) as f:
            user = json.load(f)
        for k, v in user.items():
            if k == "shaping":
                for n, sv in v.items():
                    cfg["shaping"].setdefault(n, {}).update(sv)
            else:
                cfg[k] = v
    except FileNotFoundError:
        pass
    except Exception as e:
        log(f"slots.json unreadable ({e}); using defaults")
    return cfg


def slot_for_cc(cfg: dict, cc: int):
    for s in cfg["slots"]:
        if s["cc"] == cc:
            return s
    return None


def patch_path(cfg: dict, slot: dict):
    p = slot.get("patch")
    if not p:
        return None
    full = p if os.path.isabs(p) else os.path.join(cfg["patch_dir"], p)
    return full if os.path.isfile(full) else None


class Shaper:
    """raw 0..127 -> calibrated range -> one-pole smooth -> center snap -> deadband -> 0..127 (or None)."""
    def __init__(self, lo, hi, tau_s, deadband, center_dz=0.0, min_dt=0.02, **_):
        self.lo, self.hi, self.tau, self.db, self.cdz, self.min_dt = float(lo), float(hi), tau_s, deadband, center_dz, min_dt
        self.s = None; self.t = None; self.out = None; self.t_out = -1e9

    def feed(self, raw: int, now: float):
        n = (raw - self.lo) / max(1e-6, self.hi - self.lo)
        n = 0.0 if n < 0 else 1.0 if n > 1 else n
        if self.s is None:
            self.s, self.t = n, now
        else:
            dt = max(0.0, now - self.t); self.t = now
            a = 1.0 - math.exp(-dt / self.tau) if self.tau > 0 else 1.0
            self.s += a * (n - self.s)
        v = self.s
        if self.cdz and abs(v - 0.5) < self.cdz:
            v = 0.5
        o = int(round(v * 127))
        if self.out is not None and abs(o - self.out) < self.db and o not in (0, 127):
            return None                                   # deadband: swallow sub-threshold jitter
        if o == self.out or now - self.t_out < self.min_dt:
            return None                                   # unchanged / 50 Hz rate limit
        self.out, self.t_out = o, now
        return o


class Accum:
    """Relative encoder (CW=127, CCW=0 per body_control_map.md) -> absolute 0..127."""
    def __init__(self, value=64, step=4):
        self.value, self.step = int(value), step

    def feed(self, v: int) -> int:
        self.value = max(0, min(127, self.value + (self.step if v >= 64 else -self.step)))
        return self.value


class SlotSelector:
    """Radio logic with debounce + minimum gap. press() records; due() yields the slot to switch to."""
    def __init__(self, active, debounce, min_gap):
        self.active, self.want = active, None
        self.debounce, self.min_gap = debounce, min_gap
        self.t_press = 0.0; self.t_switch = -1e9

    def press(self, slot: int, now: float):
        if slot != self.active:
            self.want, self.t_press = slot, now
        else:
            self.want = None                               # pressing the active slot cancels a pending change

    def due(self, now: float):
        if self.want is None or now - self.t_press < self.debounce or now - self.t_switch < self.min_gap:
            return None
        s, self.want = self.want, None
        self.t_switch = now
        return s


class Router:
    """One incoming MIDI CC -> actions. ("slot", n) | ("send", chan0, cc, val). No I/O."""
    def __init__(self, cfg, st=None):
        self.cfg = cfg
        self.prob = Accum((st or {}).get("prob", 64))
        self.shapers = {n: Shaper(**c) for n, c in cfg["shaping"].items()}
        self.by_in = {c["in_cc"]: (n, c["out_cc"]) for n, c in cfg["shaping"].items()}
        self.tl_held = False
        self.arcade_sent = False

    def on_cc(self, chan: int, cc: int, val: int, now: float):
        if chan != CH1:                                    # ch9 is ours, ch16 is the clock bus, ch7 DUODECIMUS
            return []
        out = []
        s = slot_for_cc(self.cfg, cc)
        if s is not None:
            if val == 127:
                out.append(("slot", s["slot"]))
            return out
        if cc == CC_TL_CLICK:
            self.tl_held = val >= 64
        elif cc == CC_TL_ROT:
            if not self.tl_held:                           # a turn while held is not probability
                out.append(("send", CH9, 30, self.prob.feed(val)))
        elif cc == CC_ARCADE:
            if val == 127 and not self.tl_held:
                self.arcade_sent = True
                out.append(("send", CH9, 31, 127))
            elif val == 0 and self.arcade_sent:            # always release what we pressed
                self.arcade_sent = False
                out.append(("send", CH9, 31, 0))
        elif cc in self.by_in:
            name, out_cc = self.by_in[cc]
            o = self.shapers[name].feed(val, now)
            if o is not None:
                out.append(("send", CH9, out_cc, o))
        return out


def osc_blob_msg(path: str, blob: bytes) -> bytes:
    def pad(b: bytes) -> bytes:
        return b + b"\0" * (-len(b) % 4)
    return pad(path.encode() + b"\0") + pad(b",b\0") + struct.pack(">i", len(blob)) + pad(blob)


def graph_block(pairs):
    """Managed graph.conf block: links of the ACTIVE Cardinal instance only."""
    return ["# >>> cardinal-ctl (managed; active Cardinal instance only)"] + \
           [f"connect = {s} -> {d}" for s, d in (pairs or [])] + ["# <<< cardinal-ctl"]


def splice_block(text: str, block: list) -> str:
    a, b = "# >>> cardinal-ctl", "# <<< cardinal-ctl"
    lines = text.splitlines()
    i = next((i for i, l in enumerate(lines) if l.startswith(a)), None)
    if i is not None:
        j = next((j for j in range(i, len(lines)) if lines[j].startswith(b)), i)
        lines[i:j + 1] = block
    else:
        lines += block
    return "\n".join(lines) + "\n"


def parse_links(text: str) -> dict:
    """`pw-link -l` text -> {port: [peers]} (peers include direction arrows stripped)."""
    d, cur = {}, None
    for ln in text.splitlines():
        if ln and not ln.startswith(" "):
            cur = ln.strip(); d.setdefault(cur, [])
        elif cur and ("|->" in ln or "|<-" in ln):
            d[cur].append(ln.split(">", 1)[1].strip() if "|->" in ln else ln.split("-", 1)[1].lstrip("- ").strip().lstrip("<").strip())
    return d


# ------------------------------------------------------------------ state
def load_state() -> dict:
    try:
        with open(STATE_FILE) as f:
            return json.load(f)
    except Exception:
        return {}


def save_state(st: dict) -> None:
    try:
        os.makedirs(STATE_DIR, exist_ok=True)
        tmp = STATE_FILE + ".tmp"
        with open(tmp, "w") as f:
            json.dump(st, f)
        os.replace(tmp, STATE_FILE)
    except Exception as e:
        log(f"state save failed: {e}")


def sh(*a, t=5):
    try:
        return subprocess.run(a, capture_output=True, text=True, timeout=t)
    except Exception:
        return None


# ------------------------------------------------------------------ child processes
class Child:
    def __init__(self, tag: str, home: str):
        self.tag, self.home = tag, home
        self.proc = None
        self.crashes = 0; self.started = 0.0; self.next_try = 0.0
        self.frozen = False; self.t_ready = 1e18

    def env(self, idle_unscheduled: bool) -> dict:
        e = dict(os.environ)
        e["HOME"] = self.home
        e["LIBGL_ALWAYS_SOFTWARE"] = "1"
        e["LD_LIBRARY_PATH"] = "/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack" + (":" + e["LD_LIBRARY_PATH"] if e.get("LD_LIBRARY_PATH") else "")
        props = "node.autoconnect=false node.dont-reconnect=true node.async=true node.lock-quantum=true node.lock-rate=true"
        if idle_unscheduled:
            props += " node.always-process=false"          # unlinked => not scheduled (multi backend relies on this)
        e["PIPEWIRE_PROPS"] = props
        e.pop("PIPEWIRE_LATENCY", None)
        cfgdir = os.path.join(HERE, "pw")
        if os.path.exists(os.path.join(cfgdir, "jack.conf")) and os.environ.get("CARDINAL_USE_JACK_CONF", "1") == "1":
            e["PIPEWIRE_CONFIG_DIR"] = cfgdir
        return e

    def start(self, patch: str, idle_unscheduled=False) -> None:
        dst = os.path.join(self.home, "Documents/Cardinal/autosave/patch.vcv")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(patch, dst)                        # original never touched
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1280x800x24", CARDINAL_BIN]
        self.proc = subprocess.Popen(cmd, env=self.env(idle_unscheduled), stdin=subprocess.DEVNULL, start_new_session=True)
        self.started = time.monotonic(); self.frozen = False
        log(f"[{self.tag}] started {os.path.basename(patch)} pgid={self.proc.pid}")

    def cardinal_pid(self):
        """The Cardinal process inside this child's session (not xvfb-run / Xvfb)."""
        if not self.proc:
            return None
        for d in os.listdir("/proc"):
            if d.isdigit():
                try:
                    if os.getpgid(int(d)) == self.proc.pid and os.readlink(f"/proc/{d}/exe") == CARDINAL_BIN:
                        return int(d)
                except OSError:
                    pass
        return None

    def freeze(self):
        pid = self.cardinal_pid()
        if pid:
            os.kill(pid, signal.SIGSTOP); self.frozen = True

    def thaw(self):
        pid = self.cardinal_pid()
        if pid:
            os.kill(pid, signal.SIGCONT)
        self.frozen = False

    def stop(self, grace=4.0) -> None:
        p = self.proc
        if not p:
            return
        try:
            os.killpg(p.pid, signal.SIGCONT); os.killpg(p.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            p.wait(grace)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL); p.wait()
        self.proc = None

    def alive(self) -> bool:
        return self.proc is not None and self.proc.poll() is None


def pw_ports_for_pid(pid):
    """{'audio_out': [pw-link names], 'midi_in': [...]} for the PipeWire node with application.process.id == pid
    (pw-dump, read-only). Names are 'node.name:port.name' as pw-link prints them."""
    try:
        d = json.loads(subprocess.run(["pw-dump"], capture_output=True, text=True, timeout=5).stdout)
    except Exception:
        return None
    nodes = {o["id"]: o["info"]["props"] for o in d if o.get("type") == "PipeWire:Interface:Node"
             and str(o["info"]["props"].get("application.process.id")) == str(pid)}
    res = {"audio_out": [], "midi_in": []}
    for o in d:
        if o.get("type") != "PipeWire:Interface:Port":
            continue
        p = o["info"]["props"]
        if p.get("node.id") not in nodes:
            continue
        full = f'{nodes[p["node.id"]].get("node.name")}:{p.get("port.name")}'
        fmt = str(p.get("format.dsp", ""))
        if p.get("port.direction") == "out" and "audio" in fmt:
            res["audio_out"].append(full)
        elif p.get("port.direction") == "in" and "midi" in fmt:
            res["midi_in"].append(full)
    res["audio_out"].sort(); res["midi_in"].sort()
    return res


# ------------------------------------------------------------------ backends
class SingleBackend:
    """restart / osc: one Cardinal. Switch = respawn or /load."""
    def __init__(self, cfg, mode):
        self.cfg, self.mode = cfg, mode
        self.child = Child("main", RUN_HOME)
        self.active = None

    def boot(self, slot):
        self.active = slot["slot"]
        self.child.start(patch_path(self.cfg, slot))

    def switch(self, slot):
        path = patch_path(self.cfg, slot)
        self.active = slot["slot"]
        if self.mode == "osc" and self.child.alive() and osc_load(path):
            return
        self.child.stop(); self.child.start(path)

    def supervise(self, now, paused):
        c = self.child
        if paused:
            if c.proc is not None:
                log("PAUSE file present: stopping Cardinal"); c.stop()
            c.next_try = now + 5.0
            return
        if c.proc is not None and not c.alive():
            rc = c.proc.poll(); c.proc = None
            c.crashes = c.crashes + 1 if now - c.started < 30 else 1
            w = CRASH_BACKOFF[min(c.crashes - 1, len(CRASH_BACKOFF) - 1)]
            c.next_try = now + w; log(f"child exited rc={rc}; restart in {w}s")
        if c.proc is None and now >= c.next_try and self.active:
            c.start(patch_path(self.cfg, next(s for s in self.cfg["slots"] if s["slot"] == self.active)))

    def linkable(self):
        return self.child.alive()

    def stop_all(self):
        self.child.stop()


class MultiBackend:
    """One Cardinal per used slot. EXPERIMENTAL. Invariants this code keeps:
       1. a node is frozen only after proof (pw-link -l) that it has no audio or MIDI links;
       2. a node is linked only after it has been thawed THAW_WAIT_S;
       3. at most one instance is linked at a time;
       4. idle instances run node.always-process=false, so an unlinked client is not scheduled."""
    STAGGER_S, THAW_WAIT_S, UNLINK_SETTLE_S, PARK_AFTER_S = 12.0, 0.25, 0.4, 20.0

    def __init__(self, cfg):
        self.cfg = cfg
        self.kids = {}; self.active = None
        self.queue = []; self.next_boot = 0.0

    def boot(self, slot):
        self.active = slot["slot"]
        self.queue = [slot["slot"]] + [s["slot"] for s in self.cfg["slots"] if s["slot"] != slot["slot"] and patch_path(self.cfg, s)]

    def _slot(self, n):
        return next(s for s in self.cfg["slots"] if s["slot"] == n)

    def linkable(self):
        return False                                       # links are made by switch(), never by the heartbeat

    def supervise(self, now, paused):
        if paused:
            for c in self.kids.values():
                c.stop()
            self.kids.clear(); self.queue = [self.active] if self.active else []
            return
        if self.queue and now >= self.next_boot:
            n = self.queue.pop(0)
            c = self.kids.setdefault(n, Child(f"slot{n}", f"{RUN_HOME}/slot{n}"))
            c.start(patch_path(self.cfg, self._slot(n)), idle_unscheduled=(n != self.active))
            c.t_ready = now + self.PARK_AFTER_S
            self.next_boot = now + self.STAGGER_S
        for n, c in list(self.kids.items()):
            if c.proc is not None and not c.alive():
                log(f"[slot{n}] died rc={c.proc.poll()}; requeue"); c.proc = None
                if n not in self.queue:
                    self.queue.append(n)
            elif c.alive() and n != self.active and not c.frozen and c.t_ready <= now:
                if self._links_of(c) == []:
                    c.freeze(); log(f"[slot{n}] parked (SIGSTOP)")

    def _links_of(self, c):
        """List of this instance's links, [] only when PROVEN none; None when unknown."""
        pid = c.cardinal_pid()
        ports = pw_ports_for_pid(pid) if pid else None
        out = sh("pw-link", "-l")
        if not ports or not out or out.returncode != 0:
            return None
        mine = set(ports["audio_out"] + ports["midi_in"])
        links = parse_links(out.stdout)
        found = [(p, q) for p in mine for q in links.get(p, [])]
        return found

    def switch(self, slot):
        n = slot["slot"]
        new, old = self.kids.get(n), self.kids.get(self.active)
        if not new or not new.alive():
            log(f"slot {n} not loaded yet; ignored"); return False
        if new.frozen:
            new.thaw(); time.sleep(self.THAW_WAIT_S)
        pid = new.cardinal_pid()
        ports = pw_ports_for_pid(pid) if pid else None
        if not ports or len(ports["audio_out"]) < 2:
            log(f"slot {n}: ports not found; staying on slot {self.active}")
            if n != self.active and self._links_of(new) == []:
                new.freeze()
            return False
        outs = [(ports["audio_out"][0], f"{HAT_OUT}:playback_AUX0"), (ports["audio_out"][1], f"{HAT_OUT}:playback_AUX1")]
        self._write_graph(outs)                            # managed block = new instance only
        for s, d in outs:
            sh("pw-link", s, d)
        src = self._macro_src()
        if src and ports["midi_in"]:
            sh("pw-link", "-p", src, ports["midi_in"][0])  # passive link: does not keep the node active by itself
        if old is not None and old is not new:
            for _ in range(4):
                self._unlink_all(old); time.sleep(self.UNLINK_SETTLE_S)
                if self._links_of(old) == []:
                    old.freeze(); log(f"[slot{self.active}] parked (SIGSTOP)"); break
            else:
                log(f"[slot{self.active}] still linked or unknown after retries; NOT frozen")
        self.active = n
        return True

    def _macro_src(self):
        o = sh("pw-link", "-o")
        return next((l.strip() for l in o.stdout.splitlines() if MACRO_PORT in l), None) if o else None

    def _unlink_all(self, c):
        for p, q in (self._links_of(c) or []):             # only links that touch THIS instance's ports
            sh("pw-link", "-d", p, q)
            sh("pw-link", "-d", q, p)

    def _write_graph(self, outs):
        try:
            try:
                text = open(GRAPH_CONF).read()
            except FileNotFoundError:
                text = ""
            tmp = GRAPH_CONF + ".cardinal-ctl.tmp"
            with open(tmp, "w") as f:
                f.write(splice_block(text, graph_block(outs)))
            os.replace(tmp, GRAPH_CONF)
        except OSError as e:
            log(f"graph.conf write failed: {e}")

    def stop_all(self):
        for c in self.kids.values():
            c.stop()


def osc_load(path: str, port=2228) -> bool:
    try:
        data = open(path, "rb").read()
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(2.0)
        s.sendto(osc_blob_msg("/load", data), ("127.0.0.1", port))
        try:
            s.recvfrom(1024); return True
        except socket.timeout:
            return False
    except Exception as e:
        log(f"osc load failed: {e}"); return False


# ------------------------------------------------------------------ main
def open_midi(on_msg):
    import mido
    inn = next((n for n in mido.get_input_names() if MIDI_IN_HINT.lower() in n.lower()), None)
    if not inn:
        raise OSError("Midi Through not found")
    out_macro = mido.open_output(MACRO_PORT, virtual=True, client_name=MACRO_PORT)
    inp = mido.open_input(inn, callback=on_msg)
    return inp, out_macro


def main() -> int:
    import mido
    cfg = load_cfg(); st = load_state()
    by_n = {s["slot"]: s for s in cfg["slots"]}
    start_n = st.get("slot") if st.get("slot") in by_n and patch_path(cfg, by_n[st.get("slot")]) else cfg["default_slot"]
    if not patch_path(cfg, by_n[start_n]):
        log("no loadable patch in the default slot; exiting 2"); return 2
    be = MultiBackend(cfg) if cfg["backend"] == "multi" else SingleBackend(cfg, cfg["backend"])
    router = Router(cfg, st)
    sel = SlotSelector(start_n, cfg["debounce_s"], 0.3 if cfg["backend"] == "multi" else cfg["min_gap_s"])
    stop = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stop.set()); signal.signal(signal.SIGINT, lambda *_: stop.set())
    ports = {}; dirty = [0.0]

    def on_msg(msg):                                       # rtmidi thread
        om = ports.get("macro")
        if msg.type in REALTIME:
            if om: om.send(msg)                            # clock/transport to Cardinal as-is
            return
        if msg.type != "control_change":
            return
        now = time.monotonic()
        for act in router.on_cc(msg.channel, msg.control, msg.value, now):
            if act[0] == "slot":
                s = by_n[act[1]]
                if patch_path(cfg, s) is None:
                    log(f"slot {act[1]} ({s['button']}) empty: ignored")
                else:
                    sel.press(act[1], now)
            elif act[0] == "send" and om:
                om.send(mido.Message("control_change", channel=act[1], control=act[2], value=act[3]))
                if act[2] == 30:
                    dirty[0] = now

    inp = None; last_link = 0.0
    be.boot(by_n[start_n])
    while not stop.is_set():
        now = time.monotonic()
        if inp is None:
            try:
                inp, ports["macro"] = open_midi(on_msg); log("midi up")
            except Exception as e:
                if now > getattr(main, "_w", 0):
                    log(f"midi not ready ({e}); retry"); main._w = now + 30
        be.supervise(now, os.path.exists(PAUSE_FILE))
        n = sel.due(now)
        if n is not None:
            t0 = time.monotonic()
            ok = be.switch(by_n[n])
            if ok is not False:
                sel.active = n
                save_state({"slot": n, "prob": router.prob.value})
            log(f"slot {n} ({by_n[n]['button']}) backend={cfg['backend']} ok={ok is not False} took {1000*(time.monotonic()-t0):.0f} ms")
        if dirty[0] and now - dirty[0] > 2.0:
            save_state({"slot": sel.active, "prob": router.prob.value}); dirty[0] = 0.0
        if be.linkable() and now - last_link > 5.0 and os.path.exists(LINK_SH):
            last_link = now
            sh(LINK_SH, "--once")                          # MIDI link heartbeat (audio comes from graph.conf)
        time.sleep(0.02)
    be.stop_all()
    return 0


# ------------------------------------------------------------------ selftest
def selftest() -> int:
    cfg = load_cfg("/nonexistent"); r = Router(cfg); t = 0.0
    assert r.on_cc(CH1, 65, 127, t) == [("slot", 1)] and r.on_cc(CH1, 65, 0, t) == []        # press edge only
    assert r.on_cc(CH1, 61, 127, t) == [("slot", 5)] and r.on_cc(CH1, 64, 127, t) == [("slot", 8)]
    assert r.on_cc(CH1, 60, 127, t) == [("send", CH9, 31, 127)]                              # arcade is not a slot
    assert r.on_cc(CH1, 29, 90, t) == []                                                      # DUODECIMUS pedal never touched
    r = Router(cfg)
    assert r.on_cc(CH1, 26, 127, t) == [("send", CH9, 30, 68)] and r.on_cc(CH1, 26, 0, t) == [("send", CH9, 30, 64)]
    r.on_cc(CH1, 76, 127, t)                                                                  # top-left held
    assert r.on_cc(CH1, 26, 127, t) == [] and r.on_cc(CH1, 60, 127, t) == [] and r.on_cc(CH1, 60, 0, t) == []
    r.on_cc(CH1, 76, 0, t)
    assert r.on_cc(CH1, 60, 127, t) == [("send", CH9, 31, 127)]
    r.on_cc(CH1, 76, 127, t)                                                                  # encoder pressed after arcade
    assert r.on_cc(CH1, 60, 0, t) == [("send", CH9, 31, 0)]                                  # no stuck trigger
    for ch in (CH9, 15, 6):
        assert r.on_cc(ch, 65, 127, t) == []                                                  # own/clock/ch7 ignored
    s_ = Shaper(24, 104, 0.06, 1, 0.03)
    outs = [s_.feed(64, 0.0)] + [s_.feed(v, 0.1 + 0.03 * i) for i, v in enumerate((65, 63, 64, 65, 64))]
    assert outs[0] == 64 and all(o is None for o in outs[1:]), outs                           # jitter swallowed
    o = None
    for i in range(20):
        o = s_.feed(104, 1.0 + 0.03 * i) or o
    assert o == 127, o                                                                        # full tilt reaches the rail
    tof = Shaper(4, 100, 0.10, 2)
    assert tof.feed(4, 0.0) == 0 and tof.feed(5, 0.1) is None
    sel = SlotSelector(5, 0.15, 3.0)
    sel.press(1, 0.0); sel.press(2, 0.05)
    assert sel.due(0.1) is None and sel.due(0.3) == 2
    sel.active = 2; sel.press(3, 0.4); assert sel.due(1.0) is None and sel.due(3.5) == 3      # min gap, then fires
    sel.active = 3; sel.press(3, 4.0); assert sel.want is None
    cfg["patch_dir"] = "/nonexistent"
    assert patch_path(cfg, cfg["slots"][7]) is None and patch_path(cfg, cfg["slots"][0]) is None
    base = "connect = a:b -> c:d\n"
    t1 = splice_block(base, graph_block([("Cardinal:o1", "hat:p0")]))
    t2 = splice_block(t1, graph_block([("Cardinal-01:o1", "hat:p0")]))
    assert t2.count("# >>> cardinal-ctl") == 1 and "Cardinal-01:o1" in t2 and "Cardinal:o1" not in t2 and t2.startswith(base)
    L = parse_links("Cardinal:out1\n  |-> hat:AUX0\ncsound6:output1\n  |-> hat:AUX0\nMidi-Bridge:x\n  |-> Cardinal:events-in\nCardinal:events-in\n  |<- Midi-Bridge:x\n")
    assert L["Cardinal:out1"] == ["hat:AUX0"] and L["Cardinal:events-in"] == ["Midi-Bridge:x"], L
    assert osc_blob_msg("/load", b"abcde") == b"/load\0\0\0,b\0\0\0\0\0\x05abcde\0\0\0"
    print("selftest ok")
    return 0


if __name__ == "__main__":
    sys.exit(selftest() if "--selftest" in sys.argv else main())

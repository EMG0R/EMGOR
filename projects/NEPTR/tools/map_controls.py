#!/usr/bin/env python3
"""Guided capture of what every NEPTR body control actually sends.

Walks you through each control in a fixed order and records the raw MIDI
that arrives for each gesture. The result is ground truth for fixing the
mappings, not a guess from the firmware source.

Listens (read-only) to the body's two paths at once: the serial bridge
('NEPTR Serial Bridge') and the Teensy's own USB MIDI ('Teensy MIDI'), so
you can see which path each control uses. It never sends anything.

    ~/kivy-venv/bin/python ~/NEPTR_phase4/tools/map_controls.py
    ... --only "arcade"      # re-test steps whose name contains a string
    ... --timeout 30         # seconds to wait for each gesture (default 25)

Do each gesture once, then let go. The script moves on after about 1 s
of quiet. A step you skip (timeout) is recorded as NOTHING.
Output: ~/NEPTR_phase4/docs/control_capture_<timestamp>.json plus a table.
"""
import argparse, json, os, sys, time
import mido

QUIET = 0.9          # s of silence that ends a gesture
SETTLE = 0.6         # s of silence required before arming the next step

ENCODERS = ["top left", "top right"] + [f"bottom {i}" for i in range(1, 6)]
STEPS = []
for e in ENCODERS:
    STEPS += [f"encoder {e}: turn CLOCKWISE (a few clicks)",
              f"encoder {e}: turn COUNTER-clockwise (a few clicks)",
              f"encoder {e}: CLICK (press)"]
STEPS += [f"button {d}" for d in ("UP", "DOWN", "LEFT", "RIGHT")]
STEPS += [f"button {b}" for b in ("A", "B", "X", "Y")]
STEPS += ["ARCADE button"]

SOURCES = ("NEPTR Serial Bridge", "Teensy MIDI")


def open_ports():
    ports = []
    for name in mido.get_input_names():
        for s in SOURCES:
            if name.startswith(s):
                ports.append((s, mido.open_input(name)))
    if not ports:
        sys.exit("no body MIDI port found (is neptr-bridge running / Teensy plugged in?)")
    return ports


# Controls that stream while untouched (joystick/sensor jitter): key -> (lo, hi)
# resting range, learned in calibrate(). Their messages only count when the
# value leaves that range by more than MARGIN.
IDLE = {}
MARGIN = 10


def raw_poll(ports):
    out = []
    for tag, p in ports:
        for m in p.iter_pending():
            if m.type in ("clock", "active_sensing", "start", "stop", "continue", "songpos"):
                continue
            out.append((tag, m))
    return out


# Only the body's CONTROLS count (TEENSYY.ino ENC_CC / BTN_CC / ENCBTN_CC /
# JOY_BTN_CC). Sensors never do: joystick axes 27/28, expression 29, TOF 30/36,
# accel 31-33, FSR 34/35, mic envelopes 78/79.
CONTROL_CCS = set(range(20, 27)) | set(range(60, 69)) | set(range(70, 78))


def poll(ports):
    out = []
    for tag, m in raw_poll(ports):
        if m.type == "control_change" and m.control not in CONTROL_CCS:
            continue
        k, v = key(tag, m), value(m)
        if k in IDLE and v is not None:
            lo, hi = IDLE[k]
            if lo - MARGIN <= v <= hi + MARGIN:
                continue
        out.append((tag, m))
    return out


def calibrate(ports, secs=3.0):
    print(f"calibrating: hands OFF all controls for {secs:.0f} s ...", flush=True)
    t0 = time.time()
    while time.time() - t0 < secs:
        for tag, m in raw_poll(ports):
            k, v = key(tag, m), value(m)
            if v is None:
                continue
            lo, hi = IDLE.get(k, (v, v))
            IDLE[k] = (min(lo, v), max(hi, v))
        time.sleep(0.005)
    # A key that swung over most of its range was a control being touched
    # (relative encoders send 0/127), not idle jitter: never ignore those.
    for k in [k for k, (lo, hi) in IDLE.items() if hi - lo > 64]:
        print(f"    (moved during calibration, NOT ignored: {k})", flush=True)
        del IDLE[k]
    for k, (lo, hi) in IDLE.items():
        print(f"    idle stream ignored: {k}  resting {lo}..{hi}", flush=True)


def key(tag, m):
    num = getattr(m, "control", getattr(m, "note", getattr(m, "program", None)))
    return f"{tag} | {m.type} ch{m.channel + 1 if hasattr(m, 'channel') else '-'} #{num}"


def value(m):
    for a in ("value", "velocity", "pitch"):
        if hasattr(m, a):
            return getattr(m, a)
    return None


def capture(ports, timeout):
    t0 = time.time()
    got, last = [], None
    while True:
        evs = poll(ports)
        now = time.time()
        if evs:
            got += [(now, t, m) for t, m in evs]
            last = now
        if last and now - last > QUIET:
            break
        if not got and timeout and now - t0 > timeout:
            break
        time.sleep(0.005)
    summ = {}
    for _, t, m in got:
        k = key(t, m)
        summ.setdefault(k, []).append(value(m))
    return summ


def wait_quiet(ports):
    last = time.time()
    while time.time() - last < SETTLE:
        if poll(ports):
            last = time.time()
        time.sleep(0.01)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default=None)
    ap.add_argument("--timeout", type=float, default=25)
    a = ap.parse_args()
    steps = [s for s in STEPS if not a.only or a.only.lower() in s.lower()]
    ports = open_ports()
    print("listening on:", ", ".join(t for t, _ in ports), flush=True)
    print("starting in 5 s: hands off EVERYTHING (pedal too)", flush=True); time.sleep(5)
    calibrate(ports)
    results = []
    for i, step in enumerate(steps, 1):
        wait_quiet(ports)
        print(f"\n[{i}/{len(steps)}] >>> {step}", flush=True)
        s = capture(ports, a.timeout)
        if not s:
            print("    NOTHING received", flush=True)
        for k, vals in s.items():
            uniq = sorted(set(v for v in vals if v is not None))
            vs = uniq if len(uniq) <= 8 else uniq[:4] + ["..."] + uniq[-3:]
            print(f"    {k}  x{len(vals)}  values {vs}", flush=True)
        results.append({"step": step, "events": {k: v for k, v in s.items()}})
    ts = time.strftime("%Y%m%d-%H%M%S")
    path = os.path.expanduser(f"~/NEPTR_phase4/docs/control_capture_{ts}.json")
    with open(path, "w") as f:
        json.dump(results, f, indent=1)
    print("\nsaved", path)
    # collisions: one MIDI key answering for two different controls
    seen = {}
    for r in results:
        for k in r["events"]:
            seen.setdefault(k, []).append(r["step"])
    dup = {k: v for k, v in seen.items()
           if len({s.split(':')[0] for s in v}) > 1}
    if dup:
        print("\nSHARED by more than one control (mapping collisions):")
        for k, v in dup.items():
            print("  ", k, "<-", " | ".join(v))


if __name__ == "__main__":
    main()

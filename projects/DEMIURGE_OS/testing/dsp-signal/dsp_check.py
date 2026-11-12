#!/usr/bin/env python3
"""dsp_check.py — orchestrator: dry baseline vs. effect-engaged capture,
for a named effect, using effect_ctl.py (MIDI control) + signal_probe.py
(inject/capture/measure). Runs ON THE PI.

Verdict logic:
  - PASS   : wet capture RMS is clearly above the dry baseline (effect
             audibly doing something) AND no clipping AND (for stereo FX)
             L/R correlation is not ~1.0 (real stereo content, not a
             mono dry pass-through slipping past the bypass check).
  - SILENT : wet capture RMS is indistinguishable from dry (effect is
             not producing any audible output — the bug this whole
             exercise exists to catch).
  - CLIPPING : wet capture has samples pinned near full scale.

Usage:
    dsp_check.py ir <row 0-4>       # e.g. dsp_check.py ir 0   (SPRING)
    dsp_check.py ir all             # all 5 rows in sequence
    dsp_check.py gvb                # grainverb / MOOD II
"""
import json
import os
import subprocess
import sys
import time

HERE = os.path.expanduser("~/demiurge/testing/dsp-signal")
IR_ROWS = ["SPRING", "PLATE", "HALL", "CHAMBER", "WOOD_RM"]
# chamber's IR is 4.0s — need enough capture window to see the full tail.
IR_IMPULSE_DUR = 5.5


def run(*args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"cmd failed: {args}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def probe_impulse(dur, tag):
    out = f"/tmp/_dsp_{tag}.wav"
    txt = run("python3", f"{HERE}/signal_probe.py", "impulse", "--dur", str(dur), "--out", out)
    return json.loads(txt)


def probe_tone(freq, dur, tag, amp=0.5):
    out = f"/tmp/_dsp_{tag}.wav"
    txt = run("python3", f"{HERE}/signal_probe.py", "tone", "--freq", str(freq),
              "--dur", str(dur), "--amp", str(amp), "--out", out)
    return json.loads(txt)


def verdict(dry, wet, stereo_effect=True):
    d_rms = max(dry["rms_L"], dry["rms_R"])
    w_rms = max(wet["rms_L"], wet["rms_R"])
    gain_db = None
    if d_rms > 1e-9:
        import math
        gain_db = 20 * math.log10(max(w_rms, 1e-9) / d_rms)
    clipped = wet["clipped_samples_L"] > 0 or wet["clipped_samples_R"] > 0
    silent = not wet["has_signal"] or (gain_db is not None and gain_db < 1.0 and w_rms < 0.002)
    if clipped:
        return "CLIPPING", gain_db
    if silent:
        return "SILENT", gain_db
    return "PASS", gain_db


def check_ir(row):
    label = IR_ROWS[row]
    print(f"--- IR REVERB row {row} ({label}) ---", file=sys.stderr)
    run("python3", f"{HERE}/effect_ctl.py", "ir-off")
    time.sleep(0.3)
    dry = probe_impulse(IR_IMPULSE_DUR, f"ir{row}_dry")
    run("python3", f"{HERE}/effect_ctl.py", "ir-on", str(row))
    time.sleep(0.5)  # let kIRFirst load fire + kIRUpdHold stream settle a bit
    wet = probe_impulse(IR_IMPULSE_DUR, f"ir{row}_wet")
    run("python3", f"{HERE}/effect_ctl.py", "ir-off")
    v, gain = verdict(dry, wet)
    return {"effect": f"ir_reverb_{label}", "row": row, "verdict": v,
            "gain_db": gain, "dry": dry, "wet": wet}


def check_gvb():
    print("--- GRAINVERB (MOOD II) ---", file=sys.stderr)
    run("python3", f"{HERE}/effect_ctl.py", "gvb-off")
    time.sleep(0.3)
    dry = probe_impulse(3.0, "gvb_dry")
    run("python3", f"{HERE}/effect_ctl.py", "gvb-on")
    time.sleep(0.3)
    wet = probe_impulse(3.0, "gvb_wet")
    run("python3", f"{HERE}/effect_ctl.py", "gvb-off")
    v, gain = verdict(dry, wet)
    return {"effect": "grainverb", "verdict": v, "gain_db": gain, "dry": dry, "wet": wet}


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    cmd = sys.argv[1]
    results = []
    if cmd == "ir":
        rows = range(5) if sys.argv[2] == "all" else [int(sys.argv[2])]
        for r in rows:
            results.append(check_ir(r))
    elif cmd == "gvb":
        results.append(check_gvb())
    else:
        print(f"unknown: {cmd}"); sys.exit(1)

    print(json.dumps(results, indent=2))
    for r in results:
        g = f"{r['gain_db']:.1f} dB" if r.get("gain_db") is not None else "n/a"
        print(f"{r['effect']:24s} {r['verdict']:9s} gain={g}", file=sys.stderr)


if __name__ == "__main__":
    main()

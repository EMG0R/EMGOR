#!/usr/bin/env python3
"""crackle_detect.py — measure crackle in what the rig ACTUALLY outputs.

WHY THIS EXISTS
---------------
Two optimisation passes on 2026-09-15 made the audio measurably better by their
own metrics (-41% grainverb CPU, 8x smaller IR load bursts, fewer mode
transitions) and AUDIBLY WORSE by ear, and both had to be rolled back. The
offline host bench could not see the fault at all: its click detector read ZERO
hits at device levels.

The reason is that every metric used so far was measured on something that is
not the signal the player hears — CPU time, xrun counters, mode-transition
counts, or synthetic bench material at 10x the real level. This tool measures
the one thing that matters: the samples leaving the rig.

WHAT IT DETECTS
---------------
Crackle is a DISCONTINUITY: a sample-to-sample jump far larger than the signal's
own local slew rate. That is deliberately not "a big sample" (loud music is full
of those) and not "a big jump" in absolute terms (a hard pick attack is a big
jump). It is a jump that does not belong to the local waveform.

So each sample's first difference is compared against a ROBUST estimate of the
local first difference — a median over a window around it. Median, not mean,
because the outliers we are hunting would otherwise inflate the very threshold
meant to catch them. A sample is a click when

    |d[n]|  >  thresh * median(|d|) over the surrounding window

and that ratio is reported as the click's severity, so a run can be compared
against another run rather than just "some clicks happened".

Two more faults are reported separately because they are not discontinuities and
would otherwise be missed:
  * DROPOUTS — runs of exact zeros in the middle of audio (a stalled buffer)
  * CLIPS    — runs at or beyond full scale (which crackle can be mistaken for)

HOW TO USE IT
-------------
On the Pi, with the rig running and the effect under test engaged:

    python3 crackle_detect.py --seconds 20 --label "blooper on"

Play normally for the whole window. Then toggle the effect off and run it again
with a different --label. The numbers are only meaningful as a COMPARISON: an
A/B against the same playing, not an absolute score.

    python3 crackle_detect.py --compare a.json b.json

IMPORTANT HONESTY
-----------------
A clean report does NOT prove the rig sounds good — this finds discontinuities,
not every kind of ugliness (a grain engine repeating a period sounds terrible
and is perfectly continuous). A dirty report DOES prove something real is
broken. Treat it as a detector of one specific failure, not a verdict.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

try:
    import numpy as np
except ImportError:
    sys.exit("numpy is required: sudo apt install python3-numpy")


DEFAULT_TARGET = "alsa_output.platform-soc_107c000000_sound.pro-output-0"


def system_snapshot() -> dict:
    """PipeWire error counters + csound's deadline pressure.

    This is what separates the two causes of a click, and they need opposite
    fixes. If the system STARVED (an xrun), the graph missed a deadline and the
    hole in the audio is the symptom — that is a scheduling/CPU problem. If the
    error counters did NOT move and there are still clicks, the buffers were
    delivered on time and something inside the DSP wrote a discontinuity — no
    amount of CPU headroom will fix that.

    Measured 2026-09-15: csound peaked at 0.66 busy/quantum with the counters
    frozen across a full minute, which is what proved an earlier crackle was
    NOT starvation and sent the investigation into the DSP instead.
    """
    out = {"err": {}, "busy": {}}
    try:
        txt = subprocess.run(["pw-top", "-b", "-n", "2"], capture_output=True,
                             text=True, timeout=15).stdout
    except Exception:
        return out
    for line in txt.splitlines():
        f = line.split()
        if len(f) < 9:
            continue
        name = f[-1]
        if "csound" not in name and "pro-output" not in name:
            continue
        try:
            # pw-top columns: S ID QUANT RATE WAIT BUSY W/Q B/Q ERR ... NAME
            out["err"][name] = int(f[8])
            out["busy"][name] = float(f[7])
        except (ValueError, IndexError):
            continue
    return out


def record(seconds: float, target: str, rate: int, keep: str | None = None) -> tuple[np.ndarray, int]:
    """Capture the output sink's MONITOR — i.e. exactly what goes to the DAC.

    Recorded through pw-record rather than ALSA directly so we tap the graph at
    the same point the speakers do, after every stage of the chain. pw-record
    writes WAV; sox converts to raw float32 because python's `wave` module
    cannot read float WAV, and float is what we want (an int path would clip
    the very transients under investigation).
    """
    if shutil.which("pw-record") is None:
        sys.exit("pw-record not found")
    if shutil.which("sox") is None:
        sys.exit("sox not found")

    tmp = tempfile.mkdtemp(prefix="crackle-")
    wav = os.path.join(tmp, "cap.wav")
    raw = os.path.join(tmp, "cap.f32")
    try:
        # .monitor is the sink's output tap; a sink node itself is not capturable
        mon = target if target.endswith(".monitor") else target + ".monitor"
        cmd = ["pw-record", "--target", mon, "--rate", str(rate),
               "--channels", "2", "--format", "f32", wav]
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)
        try:
            proc.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

        if not os.path.exists(wav) or os.path.getsize(wav) < 1024:
            err = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
            sys.exit(f"capture produced nothing. pw-record said:\n{err}\n"
                     f"Check the target name with: pw-cli ls Node | grep node.name")

        subprocess.run(["sox", wav, "-t", "f32", "-r", str(rate),
                        "-c", "2", raw], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        data = np.fromfile(raw, dtype=np.float32)
        if data.size < 2:
            sys.exit("capture was empty")
        return data.reshape(-1, 2), rate
    finally:
        if keep:
            try:
                shutil.copy2(raw, keep)
                print(f"  raw capture kept -> {keep}")
            except Exception as e:  # noqa: BLE001
                print(f"  could not keep capture: {e}")
        shutil.rmtree(tmp, ignore_errors=True)


def analyse(x: np.ndarray, rate: int, thresh: float, win: int) -> dict:
    """Find discontinuities in one channel of float audio."""
    d = np.abs(np.diff(x))
    if d.size == 0:
        return {"clicks": 0, "events": [], "median_slew": 0.0}

    # Robust local slew estimate. A box-filtered ABSOLUTE difference would be
    # dragged upward by the clicks themselves; a median over a decimated grid is
    # cheap and immune to that. The grid is interpolated back so every sample
    # gets a local reference without an O(n*win) median.
    step = max(win // 4, 1)
    starts = np.arange(0, d.size, step)
    meds = np.array([np.median(d[s:s + win]) if d[s:s + win].size else 0.0
                     for s in starts])
    # a silent window has median 0; floor it so we never divide by zero and
    # never call a tiny wobble in silence a click
    floor = max(float(np.median(d)) * 0.1, 1e-6)
    meds = np.maximum(meds, floor)
    local = np.interp(np.arange(d.size), starts, meds)

    ratio = d / local
    hits = np.flatnonzero(ratio > thresh)

    # Collapse adjacent samples into ONE event: a single click is several
    # samples wide, and counting each of them would inflate the score by a
    # factor that depends on the click's shape rather than on how many there are.
    events = []
    if hits.size:
        gap = int(rate * 0.003)          # 3 ms — closer than this is one click
        group_start = hits[0]
        prev = hits[0]
        peak = ratio[hits[0]]
        for i in hits[1:]:
            if i - prev > gap:
                events.append((group_start, float(peak)))
                group_start = i
                peak = ratio[i]
            else:
                peak = max(peak, ratio[i])
            prev = i
        events.append((group_start, float(peak)))

    return {
        "clicks": len(events),
        "events": [{"t": round(i / rate, 4), "severity": round(s, 1)}
                   for i, s in events],
        "median_slew": float(np.median(d)),
    }


def find_dropouts(x: np.ndarray, rate: int, min_ms: float = 1.0) -> int:
    """Runs of exact zeros inside audio — a stalled or unfilled buffer."""
    z = (x == 0.0)
    if not z.any():
        return 0
    n = int(rate * min_ms / 1000.0)
    # count runs of >= n consecutive zeros
    d = np.diff(np.concatenate(([0], z.view(np.int8), [0])))
    starts = np.flatnonzero(d == 1)
    ends = np.flatnonzero(d == -1)
    return int(np.sum((ends - starts) >= n))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--target", default=DEFAULT_TARGET)
    ap.add_argument("--rate", type=int, default=48000)
    ap.add_argument("--thresh", type=float, default=12.0,
                    help="click when |d| exceeds this many times the local "
                         "median |d| (default 12)")
    ap.add_argument("--win", type=int, default=2048,
                    help="local-slew window in samples (default 2048, ~43ms)")
    ap.add_argument("--label", default="", help="what was engaged, for the A/B")
    ap.add_argument("--keep", default=None, help="copy the raw f32 capture to this path for waveform inspection")
    ap.add_argument("--json", default="", help="write the full result here")
    ap.add_argument("--compare", nargs=2, metavar=("A", "B"),
                    help="compare two saved --json runs and exit")
    args = ap.parse_args()

    if args.compare:
        a = json.load(open(args.compare[0]))
        b = json.load(open(args.compare[1]))
        print(f"{'':28}{'A: ' + (a['label'] or args.compare[0]):>22}"
              f"{'B: ' + (b['label'] or args.compare[1]):>22}")
        for k, lbl in (("clicks_per_min", "clicks / minute"),
                       ("worst_severity", "worst severity"),
                       ("dropouts", "dropouts"),
                       ("clips", "clipped runs")):
            print(f"{lbl:>28}{a.get(k, 0):>22}{b.get(k, 0):>22}")
        ac, bc = a.get("clicks_per_min", 0), b.get("clicks_per_min", 0)
        if ac or bc:
            better = "B" if bc < ac else ("A" if ac < bc else "tie")
            print(f"\n  cleaner: {better}")
        return

    sys_before = system_snapshot()
    print(f"recording {args.seconds:g}s from {args.target}.monitor — PLAY NOW")
    x, rate = record(args.seconds, args.target, args.rate, keep=args.keep)
    sys_after = system_snapshot()
    dur = x.shape[0] / rate
    print(f"captured {dur:.1f}s, {x.shape[0]} frames\n")

    peak = float(np.max(np.abs(x))) if x.size else 0.0
    if peak < 1e-4:
        print("  ** the capture is SILENT — nothing was playing, so this run "
              "says nothing. Check the target and that audio is running. **\n")

    result = {"label": args.label, "seconds": round(dur, 2), "peak": round(peak, 4)}
    total_clicks = 0
    worst = 0.0
    for ch in range(x.shape[1]):
        r = analyse(x[:, ch], rate, args.thresh, args.win)
        total_clicks += r["clicks"]
        worst = max(worst, max([e["severity"] for e in r["events"]], default=0.0))
        result[f"ch{ch}"] = r
        name = "L" if ch == 0 else "R"
        print(f"  {name}: {r['clicks']:4d} clicks   "
              f"median slew {r['median_slew']:.2e}")
        for e in r["events"][:8]:
            print(f"       t={e['t']:7.3f}s  severity {e['severity']:.0f}x")
        if r["clicks"] > 8:
            print(f"       ... and {r['clicks'] - 8} more")

    drops = sum(find_dropouts(x[:, c], rate) for c in range(x.shape[1]))
    clips = int(np.sum(np.abs(x) >= 0.999))

    result["clicks"] = total_clicks
    result["clicks_per_min"] = round(total_clicks / dur * 60, 1) if dur else 0
    result["worst_severity"] = round(worst, 1)
    result["dropouts"] = drops
    result["clips"] = clips

    # ---- was the system starved, or did the DSP write the discontinuity? ----
    xrun_delta = 0
    busy_peak = 0.0
    for name, after in sys_after.get("err", {}).items():
        xrun_delta += max(0, after - sys_before.get("err", {}).get(name, after))
    for name, b in sys_after.get("busy", {}).items():
        busy_peak = max(busy_peak, b)
    result["xrun_delta"] = xrun_delta
    result["busy_peak"] = round(busy_peak, 2)

    print(f"\n  system: xruns during capture {xrun_delta}   "
          f"csound busy/quantum {busy_peak:.2f}"
          f"   ({'STARVED' if busy_peak > 0.9 else 'headroom ok'})")
    if total_clicks and xrun_delta == 0:
        print("  -> clicks with NO xruns: the buffers arrived on time, so this "
              "is a DSP discontinuity.\n     More CPU headroom will not fix it; "
              "find what writes the step.")
    elif total_clicks and xrun_delta:
        print("  -> clicks WITH xruns: the graph missed deadlines. Fix the "
              "starvation first;\n     any DSP theory is unfalsifiable until "
              "the counters stop moving.")

    print(f"\n  TOTAL {total_clicks} clicks  "
          f"({result['clicks_per_min']}/min)   "
          f"worst {worst:.0f}x   dropouts {drops}   clipped {clips}")
    if args.label:
        print(f"  label: {args.label}")
    print("\n  Compare runs rather than reading one in isolation:")
    print("    crackle_detect.py --seconds 20 --label 'X off' --json off.json")
    print("    crackle_detect.py --seconds 20 --label 'X on'  --json on.json")
    print("    crackle_detect.py --compare off.json on.json")

    if args.json:
        with open(args.json, "w") as fh:
            json.dump(result, fh, indent=1)
        print(f"\n  wrote {args.json}")


if __name__ == "__main__":
    main()

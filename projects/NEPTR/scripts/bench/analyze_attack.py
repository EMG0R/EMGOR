#!/usr/bin/env python3
"""Attack v2 bench: run attack_bench.csd scenarios, verify onset detection.

Usage: python3 analyze_attack.py [--csound /path/to/csound] [--outdir /tmp]

Scenarios (must mirror attack_bench.csd):
  0 guitar run    12 picks @ 10/s, ringing overlap      -> expect 12 triggers
  1 tremolo       20 picks @ 15/s, same pitch, ring     -> expect 20 triggers
  2 low bass      12 soft notes B0/E1 @ 8/s             -> expect 12 triggers
  3 legato        pitch steps, constant amplitude       -> expect >= 8 triggers
  4 noise floor   -70dB noise only                      -> expect 0 triggers

Also checks swell shape on scenario 0: gain must rise 0.1->0.9 in roughly
gkATTK_atkMs after the final trigger.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

BENCH = Path(__file__).parent / "attack_bench.csd"

SCENARIOS = {
    0: dict(name="guitar run 10/s", notes=[0.5 + n * 0.1 for n in range(12)],
            lo=12, hi=12),
    1: dict(name="tremolo 15/s", notes=[0.5 + n * 0.0667 for n in range(20)],
            lo=20, hi=20),
    2: dict(name="low bass B0/E1 8/s", notes=[0.5 + n * 0.125 for n in range(12)],
            lo=12, hi=12),
    3: dict(name="legato pitch steps", notes=[0.5 + n * 0.125 for n in range(12)],
            lo=8, hi=13),
    4: dict(name="noise floor", notes=[], lo=0, hi=0),
}

# no ^ anchor: csound prefixes printks lines with ANSI escapes ("\x1b[m")
ATK_RE = re.compile(r"ATK t=([\d.]+) g=([\d.]+) c=(\d+)")


def run_scenario(csound, scn, outdir):
    cmd = [csound, "-m0", "-d", f"--omacro:SCN={scn}",
           "-o", f"{outdir}/atk_scn{scn}.wav", "-W", str(BENCH)]
    proc = subprocess.run(cmd, cwd=BENCH.parent, capture_output=True,
                          text=True, timeout=120)
    log = proc.stdout + proc.stderr
    samples = [(float(t), float(g), int(c)) for t, g, c in ATK_RE.findall(log)]
    if proc.returncode != 0 and not samples:
        print(log[-3000:])
        sys.exit(f"csound failed on SCN={scn} (rc={proc.returncode})")
    return samples


def trigger_times(samples):
    """Times where the trigger counter increments."""
    out, prev = [], 0
    for t, _g, c in samples:
        if c > prev:
            out.extend([t] * (c - prev))
            prev = c
    return out


def match_notes(trigs, notes, tol=0.045):
    """Greedy match triggers to ground-truth note times."""
    unmatched_notes, used = [], [False] * len(trigs)
    for nt in notes:
        hit = None
        for i, tt in enumerate(trigs):
            if not used[i] and -0.005 <= tt - nt <= tol + 0.02:
                hit = i
                break
        if hit is None:
            unmatched_notes.append(nt)
        else:
            used[hit] = True
    spurious = [trigs[i] for i, u in enumerate(used) if not u]
    return unmatched_notes, spurious


def swell_check(samples, atk_ms=150):
    """After the LAST trigger, gain must rise 0.1->0.9 in ~atk_ms."""
    trigs = trigger_times(samples)
    if not trigs:
        return None
    t0 = trigs[-1]
    seg = [(t, g) for t, g, _c in samples if t >= t0]
    t10 = next((t for t, g in seg if g >= 0.1), None)
    t90 = next((t for t, g in seg if g >= 0.9), None)
    if t10 is None or t90 is None:
        return False, None
    rise = (t90 - t10) * 1000
    return (0.3 * atk_ms <= rise <= 2.0 * atk_ms), rise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csound", default="csound")
    ap.add_argument("--outdir", default="/tmp")
    ap.add_argument("--scn", type=int, default=None, help="single scenario")
    args = ap.parse_args()

    scns = [args.scn] if args.scn is not None else sorted(SCENARIOS)
    failures = 0
    for scn in scns:
        spec = SCENARIOS[scn]
        samples = run_scenario(args.csound, scn, args.outdir)
        trigs = trigger_times(samples)
        n = len(trigs)
        ok = spec["lo"] <= n <= spec["hi"]
        missed, spurious = match_notes(trigs, spec["notes"])
        detail = ""
        if spec["notes"]:
            if missed:
                detail += f" missed@{['%.2f' % m for m in missed]}"
            if spurious:
                detail += f" spurious@{['%.2f' % s for s in spurious]}"
            if scn != 3 and (missed or spurious):
                ok = False
        status = "PASS" if ok else "FAIL"
        if not ok:
            failures += 1
        print(f"SCN{scn} {spec['name']:<22} triggers={n:3d} "
              f"(want {spec['lo']}-{spec['hi']})  {status}{detail}")
        if scn == 0 and trigs:
            res = swell_check(samples)
            if res is not None:
                s_ok, rise = res
                if rise is not None:
                    print(f"      swell 10->90%: {rise:.0f}ms (atk=150ms)  "
                          f"{'PASS' if s_ok else 'FAIL'}")
                else:
                    print("      swell 10->90%: never completed  FAIL")
                if not s_ok:
                    failures += 1
    print("=" * 60)
    print("ALL PASS" if failures == 0 else f"{failures} FAILURE(S)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate the P-EXPR encoder branches — give every expression destination a
free encoder, so the pedal is no longer the only way to reach it.

THE PROBLEM THIS SOLVES
-----------------------
Each expression destination (see handler/expr_targets.src.orc) is written by the
pedal and nothing else. Reach it without a pedal and you are stuck. Meanwhile
most effects leave several of the seven encoder rotations unused.

Rotations are gkLastToggle-keyed — P1..P7 of the FOCUSED effect, independent of
menu and slot (see handler/handler_menus.orc's header). So "which encoder is
free" is a question about an EFFECT, not a menu: which of the 7 rotation CCs has
no `gkLastToggle == <id>` branch. This script computes that per effect and hangs
the effect's expression parameter on the first free one — its "P-EXPR" knob.

Encoder and pedal then drive the SAME global: the pedal writes absolutely, the
encoder accumulates. Neither is removed, either one works. There is existing
precedent for exactly this dual control — menu 0's note reads "CC22 (slot enc)
also sweeps drive so you can nudge a new home position".

PURELY ADDITIVE
---------------
Output goes to handler/dispatch/gen/expr_rot_<cc>.orc, which the hand-written
rotation files #include just before their chain's closing endif. No existing
branch is edited, so nothing that works today can change behaviour.

STEP SIZES ARE FEEL, AND FEEL IS NOT DERIVABLE
----------------------------------------------
The pedal maps its whole travel onto the range; an encoder needs a per-detent
increment. That number is taste, so it is a table here (STEPS) rather than
something inferred. Defaults by category:

  0..1 continuous   0.02   -> 50 detents end to end
  pitch (semitones) 1.0    -> one semitone per detent
  rate / time       ENC_ROT_EXP, multiplicative — fine at the bottom, fast at
                            the top, which is how these actually get used

Retune by editing STEPS and re-running. Nothing else needs to change.

USAGE
    python3 tools/gen_expr_encoders.py            # write
    python3 tools/gen_expr_encoders.py --check    # verify only (CI)
    python3 tools/gen_expr_encoders.py --report   # print the assignment table

Run from the csound/ directory.
"""

import argparse
import json
import os
import pathlib
import re
import sys

HERE = pathlib.Path(".")
TARGETS_JSON = HERE / "handler" / "expr_targets.json"
OUT_DIR = HERE / "handler" / "dispatch" / "gen"

# Rotation CCs in the order we prefer to claim them. Slot rotations first, then
# the two menu-encoder rotations (P2/P3) — those are the "other aspects" knobs
# and are the most often already spoken for.
ROT_ORDER = [24, 25, 26, 21, 22, 20, 23]

ROT_FILE = {24: "rot_slot0_cc24.orc", 25: "rot_slot1_cc25.orc",
            26: "rot_slot2_cc26.orc", 21: "rot_slot3_cc21.orc",
            22: "rot_slot4_cc22.orc", 20: "rot_p2_cc20.orc",
            23: "rot_p3_cc23.orc"}

# Destinations deliberately NOT given an encoder, with the reason. Each is a
# real finding, not an oversight — see the report output.
EXCLUDE = {
    30: "preset pseudo-target (front-of-chain volume). gkLastToggle is never 30 "
        "— only a preset sets gkLastToggle29 = 30 — so a branch would be dead.",
    45: "ONWARD already uses ALL SEVEN rotations. No free encoder exists.",
    72: "FREEZE engages by gesture and sets only gkLastToggle29 = 72, never "
        "gkLastToggle, so NO rotation branch for it can ever fire. It already "
        "has three (rot_p2_cc20, rot_p3_cc23, rot_slot2_cc26) and they are "
        "unreachable today. Pre-existing bug; fix is one line in FREEZE's "
        "engage branch. Not done here — this generator stays purely additive.",
}

# id -> csound line(s) for the encoder. `None` = derive from the range.
# Anything not listed uses the linear default computed in derive_step().
STEPS = {
    # Pitch, in semitones — one per detent.
    3:  "$ENC_ROT(gkSemi'1'-12'12)",
    4:  "$ENC_ROT(gkGPSsemis'1'-12'12)",
    5:  "$ENC_ROT(gkFormantShift'1'-12'12)",
    28: "$ENC_ROT(gkFormsShift'1'-12'12)",
    27: "$ENC_ROT(gkSemiPF'1'-12'12)",
    # Rate/time — multiplicative, so the bottom of the range stays usable.
    # gkARModFreq's pedal law is 0.05*pow(200,kNorm)*25, i.e. 1.25 .. 250.
    11: "$ENC_ROT_EXP(gkARModFreq'1.1'1.25'250)",
    # gkDelayMS pedal law is kNorm*700000. ~1.08^85 spans it.
    15: "$ENC_ROT_EXP(gkDelayMS'1.08'1000'700000)",
    # H90 range depends on the preset bank, exactly as the pedal does.
    26: ("if (gkPresetBank == 1) then\n"
         "  $ENC_ROT(gkSemiH90'1'0'12)\n"
         "else\n"
         "  $ENC_ROT(gkSemiH90'1'-12'12)\n"
         "endif"),
    # Leslie writes an accumulator then derives the speed from it — mirror the
    # pedal body so both paths agree.
    10: ("$ENC_ROT(gkLeslieSpeedAcc'0.02'0'1)\n"
         "gkLeslieSpeed = 0.1 + gkLeslieSpeedAcc * 9.9"),
}


def parse_body(body):
    """-> (var, lo, hi) for a single-assignment linear body, else (var|None,...)."""
    lines = [l.strip() for l in body.split("\n")
             if l.strip() and not l.strip().startswith(";")]
    if len(lines) != 1:
        return (None, None, None)
    m = re.fullmatch(r"(gk\w+)\s*=\s*(.+)", lines[0])
    if not m:
        return (None, None, None)
    var, e = m.group(1), m.group(2).strip()
    if e in ("kNorm", "1 - kNorm"):
        return (var, 0.0, 1.0)
    mm = re.fullmatch(r"kNorm \* ([\d.]+) - ([\d.]+)", e)
    if mm:
        lo = -float(mm.group(2))
        return (var, lo, float(mm.group(1)) + lo)
    mm = re.fullmatch(r"kNorm \* ([\d.]+)", e)
    if mm:
        return (var, 0.0, float(mm.group(1)))
    return (var, None, None)


def derive_step(tid, var, lo, hi):
    if tid in STEPS:
        return STEPS[tid]
    if var is None or lo is None:
        return None
    return "$ENC_ROT(%s'0.02'%g'%g)" % (var, lo, hi)


def build():
    targets = json.loads(TARGETS_JSON.read_text())["targets"]

    # Which rotation CCs each toggle id already uses. PARAM_CC is generated from
    # these same .orc sources by DEMIURGE_OS/web/gen_neptr_model.py.
    # neptr_model.py lives in the DEMIURGE_OS web tree, which sits beside this
    # repo under a different name on each machine (DEMIURGE_OS on the dev box,
    # _______DEMIURGE on the Pi). Search instead of hardcoding — a baked-in
    # absolute path made --check die on the Pi, which is exactly where the
    # drift guard needs to run.
    here = pathlib.Path(__file__).resolve()
    roots = [pathlib.Path(os.environ["DEMIURGE_WEB"])] if os.environ.get("DEMIURGE_WEB") else []
    for up in (here.parents[2], here.parents[3]):
        roots += [up.parent / "DEMIURGE_OS" / "web", up.parent / "_______DEMIURGE" / "web",
                  up / "web"]
    # The installed copy on the device, which is where it actually lives there
    # (demiurge-web.service runs /opt/demiurge/web/demiurge_web.py).
    roots.append(pathlib.Path("/opt/demiurge/web"))
    for r in roots:
        if (r / "neptr_model.py").exists():
            sys.path.insert(0, str(r))
            break
    else:
        sys.exit("cannot find neptr_model.py (looked in: %s). Set DEMIURGE_WEB."
                 % ", ".join(str(r) for r in roots))
    import neptr_model as nm
    used = {}
    for (_code, _p), (cc, tid) in nm.PARAM_CC.items():
        used.setdefault(tid, set()).add(cc)

    assigned, skipped = [], []
    for t in targets:
        tid = t["id"]
        if tid in EXCLUDE:
            skipped.append((tid, t["writes"], EXCLUDE[tid]))
            continue
        free = [c for c in ROT_ORDER if c not in used.get(tid, set())]
        if not free:
            skipped.append((tid, t["writes"], "no free rotation"))
            continue
        var, lo, hi = parse_body(t["body"])
        line = derive_step(tid, var, lo, hi)
        if line is None:
            skipped.append((tid, t["writes"],
                            "body is not a simple assignment and has no STEPS "
                            "entry — add one"))
            continue
        assigned.append({"id": tid, "cc": free[0], "code": line,
                         "label": t["label"], "writes": t["writes"]})
    return assigned, skipped


def render(cc, entries):
    out = ["    ; ---- GENERATED by tools/gen_expr_encoders.py — DO NOT EDIT ----",
           "    ; P-EXPR: the expression destination of each focused effect, on its",
           "    ; first free rotation. Same global the pedal writes; encoder",
           "    ; accumulates, pedal writes absolute. Both live, neither removed.",
           ""]
    for e in entries:
        out.append("      elseif (gkLastToggle == %d) then" % e["id"])
        lbl = (e["label"] or "").strip()
        if lbl:
            out.append("        ; P-EXPR: %s" % lbl[:66])
        for l in e["code"].split("\n"):
            out.append("        " + l)
    out.append("    ; ---- end generated ----")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    if not TARGETS_JSON.exists():
        sys.exit("run me from the csound/ directory (no %s)" % TARGETS_JSON)

    assigned, skipped = build()
    by_cc = {}
    for e in assigned:
        by_cc.setdefault(e["cc"], []).append(e)

    if args.report:
        print("%-4s %-18s %-5s %s" % ("id", "writes", "enc", "encoder line"))
        print("-" * 90)
        for e in sorted(assigned, key=lambda x: x["cc"]):
            print("%-4d %-18s CC%-3d %s" % (e["id"], ",".join(e["writes"])[:18],
                                            e["cc"], e["code"].split("\n")[0][:52]))
        print("\nassigned: %d   skipped: %d" % (len(assigned), len(skipped)))
        for tid, writes, why in skipped:
            print("\n  SKIP id %d (%s):\n    %s" % (tid, ",".join(writes), why))
        return

    files = {cc: render(cc, ents) for cc, ents in by_cc.items()}

    if args.check:
        bad = [cc for cc, text in files.items()
               if not (OUT_DIR / ("expr_rot_%d.orc" % cc)).exists()
               or (OUT_DIR / ("expr_rot_%d.orc" % cc)).read_text() != text]
        if bad:
            sys.exit("STALE, re-run: %s" % bad)
        print("up to date (%d branches across %d rotations)"
              % (len(assigned), len(files)))
        return

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for cc, text in files.items():
        (OUT_DIR / ("expr_rot_%d.orc" % cc)).write_text(text)
    print("wrote %d files, %d P-EXPR branches" % (len(files), len(assigned)))
    for cc in sorted(files):
        print("  expr_rot_%-3d -> %-22s %d branch(es)"
              % (cc, ROT_FILE[cc], len(by_cc[cc])))
    print("skipped %d (run --report for reasons)" % len(skipped))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate handler/expr_registry.orc + expr_targets.json from the expression
destination table.

WHY THIS EXISTS
---------------
`handler/expr_targets.src.orc` is a chain of `elseif (gkLastToggle29 == N)`
branches: "if the last effect to claim the pedal was N, write this global from
kNorm". That is a mapping TABLE that happens to be written as code, and because
it is code, the pedal is the only thing that can ever drive those parameters.

This script turns it into a callable: one UDO, `ExprSet(kId, kNorm)`. The pedal
becomes `ExprSet gkLastToggle29, kNorm` — one line — and the very same entry
point serves an OSC `/p` write, so a LiDAR, the gyro, a joystick axis or a web
fader can drive any pedal destination with no engine change.

THE VERBATIM RULE
-----------------
Branch bodies are copied CHARACTER FOR CHARACTER, comments included. Nothing is
re-modelled as (variable, min, max). This matters:

  * id 26 (H90) branches on gkPresetBank for a different range per bank.
  * id 10 (Leslie) writes two globals, the second derived from the first.

A (var, min, max) model would have silently broken both. Lifting the body keeps
every range expression -- `kNorm * 24 - 12`, `0.05 * pow(200, kNorm) * 25`,
`1 - kNorm` -- exactly where it already was, so equivalence is structural rather
than something to be re-tested by ear.

Bodies reference `kNorm`, so the UDO's value argument is NAMED `kNorm` and the
bodies compile unedited.

USAGE
    python3 tools/gen_expr_registry.py            # write both outputs
    python3 tools/gen_expr_registry.py --check    # verify, don't write (CI)

Run from the csound/ directory. Re-run after editing the source table.
"""

import argparse
import json
import pathlib
import re
import sys

SRC = pathlib.Path("handler/expr_targets.src.orc")
ORC_OUT = pathlib.Path("handler/expr_registry.orc")
JSON_OUT = pathlib.Path("handler/expr_targets.json")

BRANCH_RE = re.compile(r"^\s*(?:els)?e?if \(gkLastToggle29 == (\d+)\) then\s*$")


def parse(text):
    """-> [(id, [body lines verbatim]), ...] in source order."""
    lines = text.split("\n")
    starts = [(i, int(m.group(1)))
              for i, l in enumerate(lines)
              for m in [BRANCH_RE.match(l)] if m]
    if not starts:
        sys.exit("no gkLastToggle29 branches found in %s" % SRC)

    # The chain's closing `endif` is the first bare `endif` at the dispatch's
    # own indent level after the last branch. Branch bodies may contain their
    # own nested if/endif (id 26 does), so track depth rather than matching the
    # first `endif` we see.
    depth = 0
    close = None
    for i in range(starts[-1][0] + 1, len(lines)):
        s = lines[i].strip()
        if re.match(r"^(if|while|until)\b.*\bthen\s*$", s) or s.startswith("if ("):
            depth += 1
        elif s == "endif" or s.startswith("endif"):
            if depth == 0:
                close = i
                break
            depth -= 1
    if close is None:
        sys.exit("could not find the branch chain's closing endif")

    bounds = [i for i, _ in starts] + [close]
    return [(starts[k][1], lines[bounds[k] + 1:bounds[k + 1]])
            for k in range(len(starts))]


def label_and_vars(body):
    """Leading comment lines become the human label; assigned gk* are the
    parameters this target actually moves."""
    label = []
    for l in body:
        s = l.strip()
        if s.startswith(";"):
            label.append(s.lstrip("; ").rstrip())
        elif s:
            break
    variables = []
    for l in body:
        m = re.match(r"\s*(gk\w+)\s*=", l)
        if m and m.group(1) not in variables:
            variables.append(m.group(1))
    return " ".join(label), variables


def render_orc(entries, src_sha):
    out = []
    w = out.append
    w(";==============================================================")
    w("; EXPR_REGISTRY.ORC — GENERATED. DO NOT HAND-EDIT.")
    w(";==============================================================")
    w("; Source:    handler/expr_targets.src.orc  (sha256 %s)" % src_sha)
    w("; Generator: tools/gen_expr_registry.py")
    w(";")
    w("; ExprSet(kId, kNorm) writes the expression destination selected by kId.")
    w("; kId is a toggle id — the value gkLastToggle29 holds — so the pedal is")
    w(";     ExprSet gkLastToggle29, kNorm")
    w("; and an OSC /p write is the same call with an id chosen by the router.")
    w("; The pedal is no longer privileged: it is one source among many.")
    w(";")
    w("; Every branch body below is copied verbatim from the source block,")
    w("; comments included. Ranges are unchanged by construction. To alter a")
    w("; destination, edit the SOURCE block and re-run the generator.")
    w(";")
    w("; id 0 = no destination (dry, or a preset that declines the pedal).")
    w("; %d destinations." % len(entries))
    w(";==============================================================")
    w("")
    w("opcode ExprSet, 0, kk")
    w("  kId, kNorm xin")
    w("")
    for n, (tid, body) in enumerate(entries):
        w("  %s (kId == %d) then" % ("if" if n == 0 else "elseif", tid))
        for l in body:
            w(("  " + l).rstrip() if l.strip() else "")
        if n == len(entries) - 1:
            w("  endif")
    w("")
    w("endop")
    w("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="verify the committed outputs match; write nothing")
    args = ap.parse_args()

    if not SRC.exists():
        sys.exit("run me from the csound/ directory (no %s here)" % SRC)

    import hashlib
    raw = SRC.read_bytes()
    src_sha = hashlib.sha256(raw).hexdigest()[:16]
    entries = parse(raw.decode())

    orc = render_orc(entries, src_sha)
    targets = []
    for tid, body in entries:
        label, variables = label_and_vars(body)
        targets.append({"id": tid, "label": label, "writes": variables,
                        "body": "\n".join(body).rstrip()})
    js = json.dumps({"source_sha256_16": src_sha, "targets": targets}, indent=2) + "\n"

    if args.check:
        bad = [p for p, want in ((ORC_OUT, orc), (JSON_OUT, js))
               if not p.exists() or p.read_text() != want]
        if bad:
            sys.exit("STALE, re-run the generator: %s"
                     % ", ".join(str(p) for p in bad))
        print("up to date (%d destinations)" % len(entries))
        return

    ORC_OUT.write_text(orc)
    JSON_OUT.write_text(js)
    print("%s: %d destinations" % (ORC_OUT, len(entries)))
    print("%s: %d targets" % (JSON_OUT, len(targets)))


if __name__ == "__main__":
    main()

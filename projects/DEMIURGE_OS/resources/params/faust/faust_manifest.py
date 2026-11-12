#!/usr/bin/env python3
"""faust_manifest.py -- Faust `-json` output -> a Demiurge pool manifest (docs/parameters.md §4).

    faust_manifest.py dparam_example.dsp --stage faust-ex --port 9007 [-o ~/demiurge/params/faust-ex.json]
    faust_manifest.py --json dparam_example.dsp.json --stage faust-ex --port 9007

Per slider/entry/button/checkbox:
  path     /<stage>/<shortname>           (shortname = label with the metadata stripped)
  address  the [osc:/alias lo hi] alias if the label has one (Faust maps lo..hi onto the slider
           itself; "native" only if lo..hi is not 0..1), else Faust's default OSC address /<dsp name>/<path>.
  native   {min,max,curve,unit} when there is no alias and the range is not 0..1 (the pool's
           osc sink denormalizes); curve "exp" for [scale:log] / [scale:exp] with min > 0.
  default  the slider init, NORMALIZED to 0..1 (the pool wire is always 0..1).
Bargraphs are outputs, not parameters: skipped.
Needs the `faust` binary unless --json is given. Pure stdlib.
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

INPUTS = {"hslider", "vslider", "nentry", "button", "checkbox"}


def walk(items, out):
    for it in items:
        if "items" in it:
            walk(it["items"], out)
        elif it.get("type") in INPUTS:
            out.append(it)


def meta(it):
    d = {}
    for m in it.get("meta", []):
        d.update(m)
    return d


def build(doc, stage, port, host="127.0.0.1"):
    items = []
    walk(doc.get("ui", []), items)
    params = []
    for it in items:
        m = meta(it)
        name = (it.get("shortname") or it["label"]).replace(" ", "_")
        lo, hi = float(it.get("min", 0)), float(it.get("max", 1))
        init = float(it.get("init", lo))
        p = {"path": f"/{stage}/{name}", "label": it["label"]}
        curve = "exp" if m.get("scale") in ("log", "exp") and lo > 0 else "lin"
        if it["type"] in ("button", "checkbox"):
            lo, hi, init = 0.0, 1.0, float(it.get("init", 0))
        alias = m.get("osc")
        if alias:
            parts = alias.split()
            p["address"] = parts[0]                   # Faust maps the alias range onto the slider itself
            if len(parts) >= 3 and (float(parts[1]), float(parts[2])) != (0.0, 1.0):
                # alias range is not 0..1: the pool must send values in THAT range
                p["native"] = {"min": float(parts[1]), "max": float(parts[2]), "curve": "lin"}
            p["default"] = round((init - lo) / (hi - lo), 6) if hi > lo else 0.0
        else:
            p["address"] = it["address"]
            if (lo, hi) != (0.0, 1.0):
                p["native"] = {"min": lo, "max": hi, "curve": curve}
                if m.get("unit"):
                    p["native"]["unit"] = m["unit"]
            if hi > lo:
                if curve == "exp":
                    p["default"] = round(math.log(init / lo) / math.log(hi / lo), 6)
                else:
                    p["default"] = round((init - lo) / (hi - lo), 6)
            else:
                p["default"] = 0.0
        params.append(p)
    return {"stage": stage, "port": port, "host": host, "version": 1, "params": params}


def faust_json(dsp):
    """Run `faust -json` in a scratch dir."""
    dsp = os.path.abspath(dsp)
    with tempfile.TemporaryDirectory() as td:
        # faust writes <file>.dsp.json NEXT TO THE SOURCE, so compile a copy in the scratch dir
        local = os.path.join(td, os.path.basename(dsp))
        shutil.copy(dsp, local)
        r = subprocess.run(["faust", "-json", "-I", os.path.dirname(dsp), local, "-o", os.path.join(td, "o.cpp")],
                           cwd=td, capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            sys.exit("faust failed:\n" + r.stderr)
        jp = [f for f in os.listdir(td) if f.endswith(".json")]
        if not jp:
            sys.exit("faust produced no json")
        with open(os.path.join(td, jp[0])) as f:
            return json.load(f)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dsp", nargs="?")
    ap.add_argument("--json", help="an existing faust -json file instead of compiling")
    ap.add_argument("--stage", required=True)
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("-o", "--out", help="write here (default stdout)")
    a = ap.parse_args()
    if a.json:
        with open(a.json) as f:
            doc = json.load(f)
    elif a.dsp:
        doc = faust_json(a.dsp)
    else:
        ap.error("give a .dsp or --json")
    text = json.dumps(build(doc, a.stage, a.port), indent=2) + "\n"
    if a.out:
        tmp = a.out + ".tmp"
        with open(tmp, "w") as f:
            f.write(text)
        os.replace(tmp, a.out)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()

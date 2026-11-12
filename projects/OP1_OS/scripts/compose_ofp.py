#!/usr/bin/env python3
"""compose_ofp.py: op1-sdk compose.py plus our own feature directories (vendor/ stays untouched).

    .venv/bin/python scripts/compose_ofp.py recipes/probe_v1.json stock/op1_246.op1 [--no-build]

Extra feature dirs searched (a feature = a dir holding feature.json): src/*/<name>/ and src/<area>/ itself.
After a build the image is copied to build/<recipe name>.op1 with a .sha256 file.
Needs PATH to contain toolchain/bfin/bin (the .sh wrapper scripts/compose_ofp.sh does that)."""
import glob, hashlib, json, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK = os.path.join(ROOT, "vendor", "op1-sdk")
sys.path.insert(0, os.path.join(SDK, "tools"))
import compose  # noqa: E402

OURS = {}
for pat in ("src/*/feature.json", "src/*/*/feature.json"):
    for p in glob.glob(os.path.join(ROOT, pat)):
        OURS[json.load(open(p))["name"]] = os.path.dirname(p)

_orig_load = compose.load_feature
_orig_all = compose.all_features


def load_feature(name):
    entry = name if isinstance(name, dict) else {"name": name}
    if entry["name"] in OURS:
        f = json.load(open(os.path.join(OURS[entry["name"]], "feature.json")))
        f["dir"] = OURS[entry["name"]]
        return f
    return _orig_load(name)


compose.load_feature = load_feature
compose.all_features = lambda: sorted(set(_orig_all()) | set(OURS))

if __name__ == "__main__":
    args = [a for a in sys.argv[1:]]
    sys.argv = [sys.argv[0]] + args
    recipe = next((a for a in args if a.endswith(".json")), None)
    compose.main()
    if recipe and "--no-build" not in args:
        name = json.load(open(recipe))["name"]
        src = os.path.join(SDK, "build", "apps", name, "build", name + ".op1")
        dst = os.path.join(ROOT, "build", name + ".op1")
        shutil.copy(src, dst)
        h = hashlib.sha256(open(dst, "rb").read()).hexdigest()
        open(dst + ".sha256", "w").write(f"{h}  {name}.op1\n")
        print(f"{dst}\n  sha256 {h}\n  {os.path.getsize(dst):,} bytes")

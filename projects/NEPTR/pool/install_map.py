#!/usr/bin/env python3
"""Install NEPTR's rig-level pool maps into ~/demiurge/sensors.json.

    python3 pool/install_map.py [--dry-run] [--sensors PATH]

Merges pool/neptr_default.map.json (`maps` and `profiles`) into sensors.json:
entries are matched by `id` (maps) / `name` (profiles), replaced in place if
present, appended if not. Idempotent. Everything else in sensors.json (sources,
other maps) is left alone. The old file is kept as sensors.json.bak-<stamp>.
demiurge-io hot-reloads sensors.json; no restart needed.
Stdlib only.
"""
import argparse
import json
import os
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_SENSORS = Path.home() / "demiurge" / "sensors.json"


def merge(doc: dict, src: dict) -> dict:
    doc.setdefault("sources", [])
    for key, ident in (("maps", "id"), ("profiles", "name")):
        have = doc.setdefault(key, [])
        index = {e.get(ident): i for i, e in enumerate(have)}
        for e in src.get(key, []):
            if e[ident] in index:
                have[index[e[ident]]] = e
            else:
                index[e[ident]] = len(have)
                have.append(e)
    return doc


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sensors", type=Path, default=DEFAULT_SENSORS)
    ap.add_argument("--map", type=Path, default=HERE / "neptr_default.map.json")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    src = json.loads(a.map.read_text())
    doc = json.loads(a.sensors.read_text()) if a.sensors.exists() else {"sources": [], "maps": []}
    new = merge(doc, src)
    text = json.dumps(new, indent=2) + "\n"
    if a.dry_run:
        sys.stdout.write(text)
        return 0
    if a.sensors.exists():
        shutil.copy2(a.sensors, str(a.sensors) + time.strftime(".bak-%Y%m%d-%H%M%S"))
    a.sensors.parent.mkdir(parents=True, exist_ok=True)
    tmp = a.sensors.with_suffix(".json.tmp")
    tmp.write_text(text)
    os.replace(tmp, a.sensors)
    print(f"installed {len(src.get('maps', []))} maps, {len(src.get('profiles', []))} profiles -> {a.sensors}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

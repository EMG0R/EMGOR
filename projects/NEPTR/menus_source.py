"""Menu data for the pygame UI, read from menus.json (the single source of truth
for menu ORDER, edited by the web page at #/neptr/menus).

    from menus_source import load_menus, menus_in_cycle_order

load_menus() returns the menus indexed by menu id (position == id), each
{"id", "name", "slots": [codes...], "hidden": bool, "conditional": bool}.
menus_in_cycle_order() returns the visible ones in engine nav order.
Ids are never renumbered; only the cycle order and visibility change.
"""
import json
import os

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "menus.json")


def _read(path=None):
    with open(path or PATH) as fh:
        return json.load(fh)["menus"]


def load_menus(path=None):
    """All menus, indexed by id (list position == menu id)."""
    out = {}
    for m in _read(path):
        d = dict(m)
        d.setdefault("hidden", False)
        d.setdefault("conditional", False)
        out[d["id"]] = d
    return [out[i] for i in sorted(out)]


def menus_in_cycle_order(path=None):
    """Visible menus in the engine's nav order (the order of the file)."""
    return [m for m in
            ({**x, "hidden": x.get("hidden", False),
              "conditional": x.get("conditional", False)} for x in _read(path))
            if not m["hidden"]]

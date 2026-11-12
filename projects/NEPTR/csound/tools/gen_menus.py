#!/usr/bin/env python3
"""Generate handler/gen/menu_cycle.orc from neptr/menus.json.

menus.json is the single source of truth for menu ORDER (and which menus are
hidden from the nav cycle). The order of the "menus" array IS the cycle order.
Menu ids are never renumbered - dispatch branches test literal ids.

  gen_menus.py          write handler/gen/menu_cycle.orc
  gen_menus.py --check  exit 1 if the generated file is stale or menus.json
                        is invalid (writes nothing)
"""
import json, os, sys, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CSOUND = os.path.dirname(HERE)
MENUS_JSON = os.path.join(os.path.dirname(CSOUND), "menus.json")
OUT = os.path.join(CSOUND, "handler", "gen", "menu_cycle.orc")
TABLE_SIZE = 32          # capacity of giMenuCycle (live reload may grow into it)
NSLOTS = 5


def load(path=MENUS_JSON):
    with open(path) as fh:
        return json.load(fh)


def validate(doc):
    """Return an error string, or None. Shared by the web backend."""
    if not isinstance(doc, dict) or not isinstance(doc.get("menus"), list):
        return "menus.json needs a 'menus' list"
    seen = set()
    for m in doc["menus"]:
        if not isinstance(m, dict):
            return "menu entries must be objects"
        i = m.get("id")
        if isinstance(i, bool) or not isinstance(i, int) or not 0 <= i < TABLE_SIZE:
            return "bad menu id: %r" % (i,)
        if i in seen:
            return "duplicate menu id %d" % i
        seen.add(i)
        if not isinstance(m.get("name"), str) or not m["name"]:
            return "menu %d needs a name" % i
        if not isinstance(m.get("slots"), list):
            return "menu %d needs a slots list" % i
        if "hidden" in m and not isinstance(m["hidden"], bool):
            return "menu %d: hidden must be a bool" % i
    cyc = cycle(doc)
    if not cyc:
        return "at least one menu must be visible"
    vis = [m for m in doc["menus"] if not m.get("hidden")]
    seen_cond = False
    for m in vis:
        if m.get("conditional"):
            seen_cond = True
        elif seen_cond:
            return "conditional menus (%s) must come after all always-on menus" % (
                ", ".join(c["name"] for c in vis if c.get("conditional")))
    return None


def cycle(doc):
    return [m["id"] for m in doc["menus"] if not m.get("hidden")]


def render(doc):
    cyc = cycle(doc)
    digest = hashlib.sha256(json.dumps(doc, sort_keys=True).encode()).hexdigest()[:16]
    names = ", ".join("%d %s" % (m["id"], m["name"]) for m in doc["menus"]
                      if not m.get("hidden"))
    return """;==============================================================
; MENU_CYCLE.ORC - GENERATED. DO NOT HAND-EDIT.
;==============================================================
; Source:    menus.json  (sha256 %s)
; Generator: tools/gen_menus.py
;
; giMenuCycle holds the menu ids nav.orc walks, in order. Hidden menus are
; simply absent. The table has room for %d entries; only the first
; giMenuCycleN are live. gkMenuCycleN is the live (k-rate) length, so
; menu_reload.orc can change the order over OSC without a restart.
; Menu ids are never renumbered - dispatch branches test literal ids.
; Order: %s
;==============================================================
giMenuCycle ftgen 0, 0, -%d, -2, %s
giMenuCycleN = %d
gkMenuCycleN init %d
#define MENU_CYCLE_N #%d#
""" % (digest, TABLE_SIZE, names, TABLE_SIZE,
       ", ".join(str(i) for i in cyc + [0] * (TABLE_SIZE - len(cyc))),
       len(cyc), len(cyc), len(cyc))


def generate(menus_json=MENUS_JSON, out=OUT):
    doc = load(menus_json)
    err = validate(doc)
    if err:
        raise ValueError(err)
    text = render(doc)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as fh:
        fh.write(text)
    return text


def main(argv):
    try:
        doc = load()
        err = validate(doc)
    except (OSError, ValueError) as e:
        err = str(e)
    if err:
        print("gen_menus: invalid menus.json: %s" % err, file=sys.stderr)
        return 1
    text = render(doc)
    if "--check" in argv:
        try:
            cur = open(OUT).read()
        except OSError:
            cur = None
        if cur != text:
            print("gen_menus: STALE %s - run tools/gen_menus.py" % OUT,
                  file=sys.stderr)
            return 1
        print("gen_menus: ok (%d menus in cycle)" % len(cycle(doc)))
        return 0
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(text)
    print("gen_menus: wrote %s (%d menus in cycle)" % (OUT, len(cycle(doc))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

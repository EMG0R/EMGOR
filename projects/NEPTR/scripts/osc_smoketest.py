#!/usr/bin/env python3
"""OSC smoke test for the NEPTR Phase 4 UI.

Walks EVERY menu, EVERY effect and EVERY parameter defined in the UI at
`127.0.0.1:8000`. For each effect it opens the param view, sweeps each
param from min -> max across a few steps (so the slider physically moves),
then bypasses the effect so the UI dims. Confirms end-to-end coverage:
every encoder / expression-pedal addressable value renders correctly.

Usage:
    ~/kivy-venv/bin/python ~/NEPTR_phase4/scripts/osc_smoketest.py
"""

import time
from pythonosc.udp_client import SimpleUDPClient

HOST, PORT = "127.0.0.1", 8000

# Mirror of MENUS + EFFECT_PARAMS in neptrPhase4_UI.py. Kept in sync by
# copy-paste — if you add an effect to the UI, add it here too.
# "state"-only effects get an empty sweep list (the on/off wrapper already
# exercises their state); "—" placeholder slots are omitted.
MENUS = [
    (0, "PRESETS", [
        ("sld", []),
        ("hvy", []),
        ("pha", [("rate", 0, 40), ("depth", 0, 1), ("fback", 0, 1), ("stages", 1, 24)]),
        ("trm", []),
        ("amb", []),
    ]),
    (1, "VOLUME", [
        ("tape", [("amt", 0, 1), ("fail", 0, 1), ("model", 0, 1)]),
        ("atk", [("mix", 0, 1), ("time", 10, 2000), ("sens", 0, 1), ("shape", 0, 1), ("range", 0, 1), ("retrig", 20, 150)]),
        ("les", [("speed", 0.1, 10), ("depth", 0, 1)]),
        ("rm",  [("freq", 0.05, 2500), ("depth", 0, 1)]),
        ("vol", [("level", 0, 1)]),
    ]),
    (2, "PITCH", [
        ("h90", [("semi", -12, 12), ("mix", 0, 1)]),
        ("pf",  [("semi", -12, 12), ("mix", 0, 1)]),
        ("gps", [("semi", -12, 12), ("mix", 0, 1), ("fback", 0, 1), ("dly", 0, 0.1)]),
        ("ps",  [("semi", -12, 12), ("mix", 0, 100)]),
        ("sdl", [("mix", 0, 1), ("maxdel", 0, 1), ("fback", 0, 1)]),
    ]),
    (3, "FILTERING", [
        ("wah", [("freq", 0, 1)]),
        ("mog", [("sens", 0, 1), ("res", 0, 0.95)]),
        ("bin", [("pedal", 0, 1)]),
        ("fmt", [("shift", -12, 12), ("mix", 0, 1)]),
        ("fms", [("shift", -12, 12), ("mix", 0, 1)]),
    ]),
    (4, "MOD", [
        ("fla", [("rate", 0, 40), ("depth", 0, 0.01), ("fback", -1, 1)]),
        ("pha", [("rate", 0, 40), ("depth", 0, 1), ("fback", 0, 1), ("stages", 1, 24)]),
        ("cho", [("rate", 0, 7), ("depth", 0, 1), ("mix", 0, 1)]),
        ("vib", [("rate", 0, 10), ("depth", 0, 0.03), ("fback", 0, 0.7)]),
    ]),
    (5, "DRIVE", [
        ("muf", [("drive", 0, 1), ("tone", 0, 1), ("mids", 0, 1), ("voice", 0, 1)]),
        ("dhg", [("drive", 0, 1), ("tone", 0, 1), ("blend", 0, 1), ("tight", 0, 1)]),
        ("rmz", [("drive", 0, 1), ("tone", 0, 1), ("midf", 0, 1), ("midg", 0, 1)]),
        ("mar", [("drive", 0, 1), ("tone", 0, 1), ("pres", 0, 1), ("tight", 0, 1)]),
        ("od",  [("drive", 0, 1), ("tone", 0, 1), ("morph", 0, 1), ("blend", 0, 1)]),
    ]),
    (6, "TIME", [
        ("mod", [("clock", 0, 1), ("mix", 0, 1), ("dens", 0, 1400), ("dly", 0, 6)]),
        ("grn", [("mix", 0, 1)]),
        ("vrb", [("room", 0, 0.999), ("mix", 0, 1)]),
        ("hab", [("mix", 0, 1), ("size", 0, 1), ("repeats", 0, 1), ("scan", 0, 1)]),
        ("dly", [("time", 0, 700000), ("mix", 0, 1), ("fback", 0, 1.2), ("semi", -5, 12)]),
    ]),
    (7, "GLITCH", [
        ("fsh", [("mix", 0, 1)]),
        ("sqm", [("mix", 0, 1)]),
        ("wgd", [("mix", 0, 1)]),
        ("bbc", [("mix", 0, 1)]),
    ]),
    (8, "NAM", [
        ("nam0", [("wet", 0, 1)]),
        ("nam1", [("wet", 0, 1)]),
        ("nam2", [("wet", 0, 1)]),
        ("nam3", [("wet", 0, 1)]),
        ("nam4", [("wet", 0, 1)]),
    ]),
]

STEPS = 5           # values per param (min, ..., max)
PARAM_DWELL = 0.06  # seconds between steps
MENU_DWELL = 0.35   # seconds between menu / effect switches


def sweep(c, code, params):
    for name, mn, mx in params:
        addr = f"/{code}_{name}"
        for i in range(STEPS):
            t = i / (STEPS - 1)
            v = mn + (mx - mn) * t
            c.send_message(addr, v)
            time.sleep(PARAM_DWELL)


def main():
    c = SimpleUDPClient(HOST, PORT)
    total_effects = sum(len(e) for _, _, e in MENUS)
    total_params = sum(len(p) for _, _, e in MENUS for _, p in e)
    print(f"sweeping {total_effects} effects / {total_params} params → {HOST}:{PORT}")

    for menu_i, menu_name, effects in MENUS:
        c.send_message("/menu", menu_i)
        time.sleep(MENU_DWELL)
        for code, params in effects:
            c.send_message(f"/{code}_state", 0)   # ON -> param view
            time.sleep(MENU_DWELL)
            sweep(c, code, params)
            c.send_message(f"/{code}_state", 1)   # BYPASS -> dim
            time.sleep(MENU_DWELL * 0.6)
        print(f"  [{menu_name}] done")

    # Preset highlight pass on the PRESETS menu (-1=dry, 1=overdrive,
    # 2=solid-dist, 3=ambient).
    c.send_message("/menu", 0)
    time.sleep(MENU_DWELL)
    for p in (1, 2, 3, -1):
        c.send_message("/preset", p)
        time.sleep(MENU_DWELL)

    print("smoke test complete — every menu, every effect, every param")


if __name__ == "__main__":
    main()

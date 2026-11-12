#!/usr/bin/env python3
"""author_show_presets.py - build the DUODECIMUS show bank (surf) from the
pre-show export, deterministically. Re-runnable; writes BOTH bank files.

Source: presets/surf/export-20261101-before.json (old slots 1 VIBRATO,
2 CHORUS, 5 AMBIENT REVERB). Base = old slot 1 with every effect bypassed;
amp/NAM/master values identical in every slot so recall never jumps in level.
Pedal owner (state slot 304, stored owner+1) = VOLUME (target 1) in every slot.
Usage: author_show_presets.py [--bank DIR] [--dry]
"""
import json, os, sys, argparse
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import preset_bank as pb

ap = argparse.ArgumentParser()
ap.add_argument("--bank", default=os.path.expanduser("~/demiurge/presets/surf"))
ap.add_argument("--src", default=os.path.join(HERE, "..", "presets", "surf", "export-20261101-before.json"))
ap.add_argument("--dry", action="store_true")
a = ap.parse_args()

names = pb.load_map()
old = json.load(open(a.src))["slots"]
base = dict(old["1"])
for n in names:
    if "ypass" in n and n not in ("gkVOLpedalBypass", "gkNAMBypass"):
        base[n] = 1.0                       # everything bypassed (1 = bypassed)
base.pop("gkVOLpedalBypass", None)           # 0 = volume pedal stage ACTIVE
base["gkBLP_bypass"] = 1.0                   # blooper never baked in
for k in [k for k in base if k.startswith("gkVIB_")]:
    pass  # vib params kept from old slot 1 (used by VIBRATO)

def group(slot, prefixes):
    return {k: v for k, v in old[slot].items() if k.startswith(prefixes)}

def mk(**over):
    d = dict(base); d.update(over); return d

verb = {"gkverbBypass": 0.0, **group("5", ("gkverbMix", "gkReverbFB", "gkReverbDamp"))}
SL = {
 2:  ("CLEAN", mk()),
 4:  ("VIBRATO", mk(gkVIB_bypass=0.0)),
 6:  ("CHORUS", mk(gkBypassChorus=0.0, **group("2", ("gkChorusMix", "gkChorusRate", "gkChorusDepth")))),
 12: ("AMBIENT REVERB", mk(**verb)),
 # 2026-11-01 Emory: "slower, less mix, tape delay vibe, chill" -> long dark repeats
 # Emory's existing TAPE DELAY (tapedelay.orc), nothing added.
 10: ("TAPE DELAY", mk(gkTdlBypass=0.0, gkTdlMix=0.18, gkTdlFeedback=0.45, gkTdlTimeMs=560.0,
                       gkTdlFiltCtr=1100.0, gkTdlFiltQ=0.5, gkTdlHeadMode=5.0)),
 3:  ("DRIVE", mk(gkOD_bypass=0.0, gkOD_drive=0.45, gkOD_tone=0.5)),
 5:  ("SWELL", mk(gkATTK_bypass=0.0, gkATTK_mix=1.0, gkATTK_atkMs=600.0, gkATTK_sens=0.5, **verb)),
 8:  ("LESLIE", mk(gkLeslieBypass=0.0, gkLeslieSpeed=0.8, gkLeslieDepth=0.7)),
 9:  ("SPECTRAL", mk(gkSpecDelBypass=0.0, gkSpecDelMix=0.5, gkSpecDelMaxDel=0.3, gkSpecDelFB=0.6, **verb)),
}
unknown = sorted({k for _, d in SL.values() for k in d} - set(names))
if unknown: sys.exit("unknown params %s" % unknown)
for p in pb.bank_files(a.bank):
    bk = pb.Bank(p)
    for s, (nm, d) in SL.items():
        bk.clear(s)
        for k, v in d.items(): bk.set(s, names[k], v)
        bk.set(s, 304, 2.0)                 # pedal owner = VOLUME (target 1, stored +1)
        bk.set_occupied(s, True)
    for s in (1, 7, 11):                    # amp / vocals / blooper encoders: not presets
        bk.clear(s)
    # ONBOARD page (slots 13..17, its own bank since 2026-11-01): Emory's original
    # three, duplicated verbatim from the pre-show export at their old positions.
    for s, src in ((13, "1"), (14, "2"), (17, "5")):
        bk.clear(s)
        for k, v in old[src].items(): bk.set(s, names[k], v)
        bk.set(s, 304, 2.0)                 # pedal owner = VOLUME
        bk.set_occupied(s, True)
    if not a.dry: bk.save()
    print(("dry " if a.dry else "wrote ") + p, sorted(SL), "+ onboard 13,14,17")

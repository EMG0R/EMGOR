#!/usr/bin/env python3
"""gen_effect_catalog.py - ONE effect-id table for the engine and the web app.

Reads the engine's own toggle code (csound/handler/dispatch/toggle_slot*.orc),
the encoder code (rot_*.orc, for parameter names and ranges), and the state
map (state_persist.orc), and writes:

    presets/effects.json                web app catalogue (id, name, menu, params)
    csound/preset_apply.gen.orc         UDOs the engine includes:
        PresetApplyFx   k  - effect ON, exactly the hand toggle-ON branch
        PresetOffFx     k  - effect OFF
        PresetSweepOff     - every GUITAR effect off (recall start)
        PresetScan      k  - edge scan feeding the activation-order ring

Because the ON bodies are copied VERBATIM from the toggle branches (printks
dropped, the FX_TOGGLE flip replaced by "bypass = 0, gkLastToggle = id"), a
sequence step claims the expression pedal / switches radio partners / resets
gesture vars exactly like a press. Re-run after touching any toggle branch:

    python3 tools/gen_effect_catalog.py          (writes both files)
    python3 tools/gen_effect_catalog.py --check  (exit 1 if they are stale)

Step ids are the engine's toggle ids. NAM (37) is NOT in this table: it is a
separate step type (amp profile + in/out gain).
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DISP = os.path.join(ROOT, "csound", "handler", "dispatch")
OUT_JSON = os.path.join(ROOT, "presets", "effects.json")
OUT_ORC = os.path.join(ROOT, "csound", "preset_apply.gen.orc")

# Generative / performance effects: a preset recall leaves them exactly as they
# are (state_preset_exclude_*.inc.orc). The recall sweep skips them; an
# explicit step can still switch one on.
GENERATIVE = {56, 48, 20, 23, 24, 25}

SM_NAMES = ["AVERAGE", "BLUR", "DRONE", "FREEZE", "GATEHOLD", "WEAVE", "SHIMMER",
            "SHUFFLE", "PULSING", "HARMONIZ", "BINSHIFT", "PITCH", "STRETCH",
            "TRACING", "FILTBANK", "GLIDEFLT", "GRANUL", "EMERGE", "PARTGLD"]

PRETTY = {
    "T": "TONE EQ", "DIFFUSI": "DIFFUSION", "gkONW_bypass": "ONWARD", "BCRUSHER": "BINCRUSHER",
    "RES": "RESONATOR", "BIN REDU": "BIN REDUCTION",  # printks label overrides
    "H90": "H90 PITCH", "OLD PS": "OLD PITCH SHIFT", "RESONATR": "RESONATOR",
    "COMPRESS": "COMPRESSOR", "TRACKSYN": "TRACK SYNTH", "PLLFUZZ": "PLL FUZZ",
}


def menu_names():
    import json as j
    d = j.load(open(os.path.join(ROOT, "menus.json")))["menus"]
    return {m["id"]: m["name"] for m in d}


def state_map():
    pat = re.compile(r"(gk[A-Za-z0-9_]+)\s*!?=\s*tab:k\(\s*(\d+)\s*,\s*giNeptrState\s*\)")
    names = {}
    for line in open(os.path.join(ROOT, "csound", "state_persist.orc")):
        m = pat.search(line)
        if m:
            names.setdefault(m.group(1), int(m.group(2)))
    return names


def indent(s):
    return len(s) - len(s.lstrip(" "))


def parse_toggles():
    """{id: dict(var, menu, name, body[list of lines])}"""
    found = {}
    for fn in sorted(os.listdir(DISP)):
        if not fn.startswith("toggle_slot"):
            continue
        lines = open(os.path.join(DISP, fn)).read().split("\n")
        # block starts
        starts = [i for i, l in enumerate(lines)
                  if re.match(r"\s*elseif \(gkCurrentMenu == (\d+)\) then", l)]
        starts.append(len(lines))
        for a, b in zip(starts, starts[1:]):
            mm = re.match(r"\s*elseif \(gkCurrentMenu == (\d+)\) then", lines[a])
            menu = int(mm.group(1))
            blk = lines[a + 1:b]
            for j, l in enumerate(blk):
                m = re.match(r"\s*\$FX_TOGGLE\((\w+)'(\d+)\)", l)
                if not m:
                    continue
                var, tid = m.group(1), int(m.group(2))
                # ON branch: next "if (var < 0.5) then"
                k = j + 1
                while k < len(blk) and not re.match(r"\s*if \(%s < 0\.5\) then" % var, blk[k]):
                    k += 1
                if k >= len(blk):
                    continue
                ind = indent(blk[k])
                body, name = [], None
                k += 1
                while k < len(blk):
                    t = blk[k]
                    if t.strip() and indent(t) == ind and re.match(r"\s*(else|elseif|endif)\b", t):
                        break
                    pm = re.match(r'\s*printks "([^"\\]*?):\s*ON\b', t)
                    if pm and name is None:
                        name = pm.group(1).strip(": ")
                    k += 1
                    if re.match(r"\s*printks\b", t) or not t.strip() or t.strip().startswith(";"):
                        continue
                    body.append(re.sub(r"\s*;.*$", "", t).rstrip())
                ent = dict(var=var, menu=menu, name=name, body=body, file=fn)
                old = found.get(tid)
                if old is None or len(body) > len(old["body"]):
                    found[tid] = ent
    return found


def parse_params(names):
    """{toggle id: [ {var, min, max, log} ]} from the encoder files."""
    out = {}
    for fn in sorted(os.listdir(DISP)):
        if not fn.startswith("rot_"):
            continue
        cur = None
        for l in open(os.path.join(DISP, fn)):
            m = re.match(r"\s*elseif \(gkLastToggle == (\d+)\) then", l)
            if m:
                cur = int(m.group(1))
                continue
            if re.match(r"\s*(if|elseif) \(gkCurrentMenu", l):
                cur = None
            m = re.search(r"\$ENC_ROT(_EXP)?\((\w+)'([^']+)'([^']+)'([^)]+)\)", l)
            if m and cur is not None:
                var = m.group(2)
                if var not in names:
                    continue
                try:
                    lo, hi = float(m.group(4)), float(m.group(5))
                except ValueError:
                    # EXP form: (var'fac'min'max) -> groups 3,4,5 = fac,min,max
                    continue
                if m.group(1):
                    lo, hi = float(m.group(4)), float(m.group(5))
                lst = out.setdefault(cur, [])
                if not any(p["var"] == var for p in lst):
                    lst.append(dict(var=var, min=lo, max=hi, log=bool(m.group(1))))
    return out


def main():
    check = "--check" in sys.argv
    names = state_map()
    menus = menu_names()
    tog = parse_toggles()
    params = parse_params(names)

    fx = []  # list of dict(id,name,menu,on_cond,on_body[],off_stmts[],generative,var)
    for tid, e in sorted(tog.items()):
        nm = PRETTY.get(e["name"] or "", e["name"] or e["var"])
        # a preset owns the knob values: drop the hand toggle's knob re-defaults
        # (ONWARD resets seven params on every engage). Keep bypass/radio-partner,
        # claim, gesture-var (_expr) and control-flow lines.
        keep = re.compile(r"(?i)bypass|gkLastToggle|_expr\b|Pedal|^\s*(if|endif|else|elseif)\b")
        body = [l if keep.search(l) else "; (hand toggle only) " + l.strip() for l in e["body"]]
        fx.append(dict(id=tid, name=nm, menu=menus.get(e["menu"], str(e["menu"])),
                       cond="%s < 0.5" % e["var"],
                       on=["%s = 0" % e["var"], "gkLastToggle = %d" % tid] + [b.strip() for b in body],
                       off=["%s = 1" % e["var"]], var=e["var"]))
    # ---- hand-written entries (not FX_TOGGLE branches) ----
    fx.append(dict(id=1, name="VOLUME", menu="VOLUME", cond="gkVOLpedalBypass < 0.5",
                   on=["gkVOLpedalBypass = 0", "gkLastToggle = 1", "gkLastToggle29 = 1"],
                   off=["gkVOLpedalBypass = 1"], var="gkVOLpedalBypass"))
    for r, (tid, nm) in enumerate([(49, "IR SPRING"), (50, "IR PLATE"), (51, "IR HALL"),
                                   (52, "IR CHAMBER"), (53, "IR WOOD RM")]):
        fx.append(dict(id=tid, name=nm, menu="IR VERB",
                       cond="(gkIRBypass < 0.5 && gkIRActiveRow == %d)" % r,
                       on=["gkIRActiveRow = %d" % r, "gkIRBypass = 0", "gkLastToggle = %d" % tid],
                       off=["if (gkIRActiveRow == %d) then" % r, "gkIRBypass = 1", "endif"],
                       var="gkIRBypass"))
    fx.append(dict(id=36, name="HABIT", menu="CHASE BLISS", cond="gkHabitBypass < 0.5",
                   on=["gkHabitBypass = 0", "gkHab_rec = 1", "gkLastToggle = 36", "gkLastToggle29 = 36"],
                   off=["gkHabitBypass = 1", "gkHab_rec = 1"], var="gkHabitBypass"))
    fx.append(dict(id=72, name="FREEZE", menu="DIGIVERB", cond="gkFrzBypass < 0.5",
                   on=["gkFrzBypass = 0"], off=["gkFrzBypass = 1"], var="gkFrzBypass"))
    for i, nm in enumerate(SM_NAMES):
        tid = 75 + i
        fx.append(dict(id=tid, name="SM " + nm, menu="SOUNDMAGIC",
                       cond="tab:k(%d, giSMon) >= 0.5" % i,
                       on=["tablew 1, %d, giSMon" % i, "gkLastToggle = %d" % tid, "gkLastToggle29 = %d" % tid],
                       off=["tablew 0, %d, giSMon" % i], var="giSMon[%d]" % i))
    byid = {}
    for f in fx:                      # later (hand-written) entries win
        byid[f["id"]] = f
    fx = [byid[k] for k in sorted(byid)]
    bad = [f["id"] for f in fx if f["id"] == 37]
    assert not bad

    # ---------------- JSON ----------------
    cat = []
    for f in fx:
        ps = [p for p in params.get(f["id"], [])]
        cat.append(dict(id=f["id"], name=f["name"], menu=f["menu"],
                        var=(f["var"] if f["var"] in names else None),
                        generative=f["id"] in GENERATIVE,
                        claims_pedal=any("gkLastToggle29" in l for l in f["on"]),
                        params=[dict(name=p["var"], index=names[p["var"]], min=p["min"],
                                     max=p["max"], log=p["log"]) for p in ps]))
    js = json.dumps(dict(version=1, step_types=dict(effect=1, vocal=2, nam=3),
                         nam_rows=["AC30", "DELUXE", "FENDER", "TWEED", "CRUNCH"],
                         effects=cat), indent=1) + "\n"

    # ---------------- ORC ----------------
    o = []
    A = o.append
    A(";==============================================================")
    A("; preset_apply.gen.orc - GENERATED by tools/gen_effect_catalog.py. DO NOT HAND-EDIT.")
    A("; Included by state_persist.orc before instr 97. See docs/neptr_web.md.")
    A(";==============================================================")
    A("")
    A("; ON = the hand toggle-ON branch (bypass 0, gkLastToggle = id, the branch's own")
    A("; claim / radio-partner / gesture-reset lines), minus the flip and the printks.")
    A("opcode PresetApplyFx, 0, k")
    A("  kId xin")
    first = True
    for f in fx:
        A("  %s (kId == %d) then   ; %s" % ("if" if first else "elseif", f["id"], f["name"]))
        first = False
        for l in f["on"]:
            A("    " + l)
    A("  endif")
    A("endop")
    A("")
    A("opcode PresetOffFx, 0, k")
    A("  kId xin")
    first = True
    for f in fx:
        A("  %s (kId == %d) then   ; %s" % ("if" if first else "elseif", f["id"], f["name"]))
        first = False
        for l in f["off"]:
            A("    " + l)
    A("  endif")
    A("endop")
    A("")
    A("; every non-generative GUITAR effect off (the start of a sequenced recall)")
    A("opcode PresetSweepOff, 0, 0")
    for f in fx:
        if f["id"] in GENERATIVE:
            continue
        if f["id"] in (49, 50, 51, 52, 53):
            continue
        for l in f["off"]:
            A("  " + l)
    A("  gkIRBypass = 1")
    A("endop")
    A("")
    A("; edge scan: feeds the activation-order ring (PresetSeqPush/Drop in preset_seq.orc).")
    A("; kMode 0 = follow edges, 1 = resync (prev := now, no push), 2 = rebuild ring")
    A("; from the current state in id order, VOLUME last.")
    A("opcode PresetScan, 0, k")
    A("  kMode xin")
    A("  if (kMode == 2) then")
    A("    PresetSeqClear")
    A("  endif")
    for f in fx:
        if f["id"] == 1:
            continue
        A("  kOn = (%s) ? 1 : 0" % f["cond"])
        A("  if (kOn != tab:k(%d, giSeqPrev)) then" % f["id"])
        A("    tablew kOn, %d, giSeqPrev" % f["id"])
        A("    if (kMode == 0) then")
        A("      if (kOn == 1) then")
        A("        PresetSeqPush %d" % f["id"])
        A("      else")
        A("        PresetSeqDrop %d" % f["id"])
        A("      endif")
        A("    endif")
        A("  endif")
        A("  if (kMode == 2 && kOn == 1) then")
        A("    PresetSeqPush %d" % f["id"])
        A("  endif")
    A("  kOn = (gkVOLpedalBypass < 0.5) ? 1 : 0")
    A("  if (kOn != tab:k(1, giSeqPrev)) then")
    A("    tablew kOn, 1, giSeqPrev")
    A("    if (kMode == 0) then")
    A("      if (kOn == 1) then")
    A("        PresetSeqPush 1")
    A("      else")
    A("        PresetSeqDrop 1")
    A("      endif")
    A("    endif")
    A("  endif")
    A("  if (kMode == 2 && kOn == 1) then")
    A("    PresetSeqPush 1")
    A("  endif")
    A("endop")
    orc = "\n".join(o) + "\n"

    if check:
        ok = (os.path.exists(OUT_JSON) and open(OUT_JSON).read() == js and
              os.path.exists(OUT_ORC) and open(OUT_ORC).read() == orc)
        print("up to date" if ok else "STALE")
        sys.exit(0 if ok else 1)
    open(OUT_JSON, "w").write(js)
    open(OUT_ORC, "w").write(orc)
    print("%d effects, %d with params -> %s, %s" %
          (len(cat), sum(1 for c in cat if c["params"]), OUT_JSON, OUT_ORC))


if __name__ == "__main__":
    main()

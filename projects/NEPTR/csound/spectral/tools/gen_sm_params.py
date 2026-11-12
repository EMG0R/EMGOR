#!/usr/bin/env python3
"""Generate the SoundMagic parameter registry from the research catalog.

Source of truth chain (one direction only):
  docs/research/soundmagic_spectral_catalog.md   (exact AU params, read from the binaries)
    + OVERLAY below (our effect codes, rack order, hero picks, pedal pick)
  -> csound/spectral/sm_params.json                (UI + tests read this)
  -> csound/spectral/sm_params.orc                 (engine tables: min/max/default, names)

Run from neptr/:   python3 csound/spectral/tools/gen_sm_params.py [--check]
Layout: idx = fx * 32 + p.  fx 0..18 spectral (rack order), 19..22 time-domain, 23 spare.
Param order within an effect = AU parameter order (the catalog table's # column).
"""
import json, re, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
NEPTR = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CATALOG = os.path.join(NEPTR, "docs", "research", "soundmagic_spectral_catalog.md")
OUT_JSON = os.path.join(NEPTR, "csound", "spectral", "sm_params.json")
OUT_ORC = os.path.join(NEPTR, "csound", "spectral", "sm_params.orc")
# Hero/pedal picks live here once it exists (curated from the web page, see
# web/ #/neptr/spectral). OVERLAY below is only the seed written on first run.
HEROES_JSON = os.path.join(NEPTR, "csound", "spectral", "sm_heroes.json")

STRIDE = 32
N_FX = 24
STAMP = 1

# (fx id, code, catalog section title, UDO name, kind, heroes[(param name, role)], pedal param name)
# roles: P1..P7 = encoder rotation order (P1 is also the tile mini-fader).
OVERLAY = [
    (0,  "AVERAGE",  "Spectral Averaging",      "SM_Averaging",    "spectral", ["Number of frames", "Brightness"], "Number of frames"),
    (1,  "BLUR",     "Spectral Blurring",       "SM_Blurring",     "spectral", ["Blur amount", "Blur variance", "LFO rate"], "Blur amount"),
    (2,  "DRONE",    "Spectral DroneMaker",     "SM_DroneMaker",   "spectral", ["Interpolation length", "Interp length variance", "Brightness"], "Interpolation length"),
    (3,  "FREEZE",   "Spectral Freezing",       "SM_Freezing",     "spectral", ["Freeze factor", "Threshold"], "Freeze factor"),
    (4,  "GATEHOLD", "Spectral Gate And Hold",  "SM_GateAndHold",  "spectral", ["Threshold", "Hold length", "Decay factor"], "Threshold"),
    (5,  "WEAVE",    "Spectral Weave",          "SM_Weave",        "spectral", ["Threshold", "Thread life", "Hold length"], "Threshold"),
    (6,  "SHIMMER",  "Spectral Shimmer",        "SM_Shimmer",      "spectral", ["Jitter amount", "High-frequency jitter", "Delay variance"], "Jitter amount"),
    (7,  "SHUFFLE",  "Spectral Shuffle",        "SM_Shuffle",      "spectral", ["Shuffle factor", "Shuffle range", "Bins per group"], "Shuffle factor"),
    (8,  "PULSING",  "Spectral Pulsing",        "SM_Pulsing",      "spectral", ["On duration", "Off duration", "On multiplier freq"], "On duration"),
    (9,  "HARMONIZ", "Spectral Harmonizer",     "SM_Harmonizer",   "spectral", ["Interval 1", "Interval 2", "Wet/Dry Mix"], "Interval 1"),
    (10, "BINSHIFT", "Spectral Bin Shift",      "SM_BinShift",     "spectral", ["Bins to Shift", "Feedback", "Shifted multiplier"], "Bins to Shift"),
    (11, "PITCH",    "Spectral Pitch Shift",    "SM_PitchShift",   "spectral", ["Pitch shift", "Feedback"], "Pitch shift"),
    (12, "STRETCH",  "Spectral Stretch",        "SM_Stretch",      "spectral", ["Alpha stretch", "Beta stretch", "Beta stretch variance"], "Alpha stretch"),
    (13, "TRACING",  "Spectral Tracing",        "SM_Tracing",      "spectral", ["Number of bins 1", "Tracing type", "Threshold"], "Number of bins 1"),
    (14, "FILTBANK", "Spectral Filterbank",     "SM_Filterbank",   "spectral", ["Base frequency", "Interval", "Number of filters"], "Base frequency"),
    (15, "GLIDEFLT", "Spectral Gliding Filters","SM_GlidingFilters","spectral", ["Pitch glide per second", "Number of filters", "Filter width"], "Pitch glide per second"),
    (16, "GRANUL",   "Spectral Granulation",    "SM_Granulation",  "spectral", ["Grain length", "Grain delay", "Bin shift"], "Grain length"),
    (17, "EMERGE",   "Spectral Emergence",      "SM_Emergence",    "spectral", ["Fade in (windows)", "Hold (windows)", "Fuzziness"], "Fade in (windows)"),
    (18, "PARTGLD",  "Spectral Partial Glide",  "SM_PartialGlide", "spectral", ["Pitch shift per second", "Pitch shift variance", "Mirror Bin"], "Pitch shift per second"),
    (19, "CHORUS",   "Chorus",                  "SM_Chorus",       "time",     ["Delay", "Delay Variance", "Feedback"], "Delay"),
    (20, "GRAINSTR", "Grain Streamer",          "SM_GrainStreamer","time",     ["Freeze probability", "Grain repetitions", "Grain length at start"], "Freeze probability"),
    (21, "IDEEFIX",  "Idee Fixer",              "SM_IdeeFixer",    "time",     ["Grain duration", "Grain spacing", "Grain transposition"], "Grain duration"),
    (22, "SUPERFB",  "Super Filterbank",        "SM_SuperFilterbank","time",   ["Fundamental frequency", "Number of filters", "Bandwidth"], "Fundamental frequency"),
]

PHASE1 = {"FREEZE", "BLUR", "AVERAGE", "BINSHIFT", "PITCH", "SHUFFLE"}

FFT_SIZES = [512, 1024, 2048, 4096, 8192, 16384, 32768, 65536]

ROW_RE = re.compile(r"^\|\s*(\d+)\s*\|\s*(.*?)\s*\|\s*([-\d.]+)\s*\|\s*([-\d.]+)\s*\|\s*([-\d.]+)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*$")
ENUM_RE = re.compile(r"\[(\d+)\]([^;\[]+);")


def parse_catalog(path):
    sections = {}
    cur = None
    for line in open(path, encoding="utf-8"):
        if line.startswith("## "):
            cur = line[3:].strip()
            sections[cur] = []
            continue
        m = ROW_RE.match(line)
        if m and cur:
            num, name, mn, mx, df, unit, notes = m.groups()
            enum = None
            if "values:" in notes:
                labels = ENUM_RE.findall(notes.split("values:", 1)[1])
                if labels:
                    enum = [lab.strip() for _, lab in sorted(labels, key=lambda t: int(t[0]))]
            kind = "enum" if enum else ("bool" if unit == "on/off" else "float")
            if name == "FFT Size":
                kind = "enum"
                enum = [str(s) for s in FFT_SIZES]
            sections[cur].append({
                "p": int(num), "name": name, "min": float(mn), "max": float(mx),
                "default": float(df), "unit": unit, "kind": kind, "enum": enum,
                "note": "" if "values:" in notes else notes,
            })
    return sections


def overlay_picks():
    """{code: {"heroes": [name|None]*<=7 (index = slot P1..P7), "pedal": name}}"""
    return {code: {"heroes": list(heroes), "pedal": pedal}
            for _fx, code, _t, _u, _k, heroes, pedal in OVERLAY}


def load_picks():
    if os.path.exists(HEROES_JSON):
        with open(HEROES_JSON, encoding="utf-8") as fh:
            return json.load(fh)
    return overlay_picks()


def _trim(lst):
    while lst and lst[-1] == -1:
        lst.pop()
    return lst


def build():
    sections = parse_catalog(CATALOG)
    picks = load_picks()
    effects = []
    for fx, code, title, udo, kind, _h, _p in OVERLAY:
        heroes = picks[code]["heroes"]
        pedal = picks[code]["pedal"]
        if len(heroes) > 7:
            sys.exit(f"{code}: more than 7 heroes")
        named = [h for h in heroes if h is not None]
        if len(set(named)) != len(named):
            sys.exit(f"{code}: duplicate hero")
        if title not in sections:
            sys.exit(f"catalog section missing: {title}")
        params = sections[title]
        if not params:
            sys.exit(f"no params parsed for {title}")
        if len(params) > STRIDE:
            sys.exit(f"{title}: {len(params)} params > stride {STRIDE}")
        names = [q["name"] for q in params]
        for h in named + [pedal]:
            if h not in names:
                sys.exit(f"{title}: hero/pedal '{h}' not a parameter ({names})")
        for i, q in enumerate(params):
            if q["p"] != i:
                sys.exit(f"{title}: param numbering gap at {i}")
            q["idx"] = fx * STRIDE + i
        effects.append({
            "fx": fx, "code": code, "title": title, "udo": udo, "kind": kind,
            "toggle_id": 75 + fx, "phase": 1 if code in PHASE1 else (3 if kind == "time" else 2),
            # built = a kernel file csound/spectral/sm_<udo lowercased minus 'sm_'>.orc exists
            "built": os.path.exists(os.path.join(os.path.dirname(OUT_ORC), udo.lower() + ".orc")),
            # index = slot P1..P7, -1 = empty slot, trailing empties trimmed
            "heroes": _trim([-1 if h is None else names.index(h) for h in heroes]),
            "pedal": names.index(pedal),
            "params": params,
        })
    return {
        "stride": STRIDE, "n_fx": N_FX, "table_size": STRIDE * N_FX, "stamp": STAMP,
        "stamp_slot": STRIDE * N_FX - 1, "toggle_base": 75,
        "fft_sizes": FFT_SIZES, "credit": "SoundMagic Spectral by Michael Norris (michaelnorris.info)",
        "effects": effects,
    }


def orc_text(reg):
    size = reg["table_size"]
    mins = [0.0] * size; maxs = [1.0] * size; defs = [0.0] * size
    for e in reg["effects"]:
        for q in e["params"]:
            mins[q["idx"]] = q["min"]; maxs[q["idx"]] = q["max"]; defs[q["idx"]] = q["default"]
    defs[reg["stamp_slot"]] = float(reg["stamp"])
    mins[reg["stamp_slot"]] = 0.0; maxs[reg["stamp_slot"]] = 1e9

    def ftgen(name, vals):
        body = ", ".join(("%g" % v) for v in vals)
        return f"{name} ftgen 0, 0, -{size}, -2, {body}\n"

    out = []
    out.append("; GENERATED by csound/spectral/tools/gen_sm_params.py from docs/research/soundmagic_spectral_catalog.md\n")
    out.append("; DO NOT EDIT. Layout: idx = fx*32 + p. Param order = AU parameter order (Michael Norris, SoundMagic Spectral v1.5).\n")
    out.append(f"#define SM_STRIDE #{reg['stride']}#\n#define SM_NFX #{reg['n_fx']}#\n#define SM_TABLE_SIZE #{size}#\n")
    out.append(f"#define SM_STAMP #{reg['stamp']}#\n#define SM_STAMP_SLOT #{reg['stamp_slot']}#\n#define SM_TOGGLE_BASE #{reg['toggle_base']}#\n")
    for e in reg["effects"]:
        out.append(f"#define SM_{e['code']} #{e['fx']}#\n")
        for q in e["params"]:
            ident = re.sub(r"[^A-Za-z0-9]+", "_", q["name"]).strip("_").upper()
            out.append(f"#define SMP_{e['code']}_{ident} #{q['idx']}#\n")
    out.append(ftgen("giSMPmin", mins))
    out.append(ftgen("giSMPmax", maxs))
    out.append(ftgen("giSMPdef", defs))
    # hero/pedal tables: giSMhero[fx*8 + k] = param idx or -1 ; giSMpedal[fx] = param idx
    hero = [-1] * (reg["n_fx"] * 8); pedal = [-1] * reg["n_fx"]; fftp = [-1] * reg["n_fx"]
    for e in reg["effects"]:
        for k, p in enumerate(e["heroes"]):
            if p >= 0:
                hero[e["fx"] * 8 + k] = e["fx"] * STRIDE + p
        pedal[e["fx"]] = e["fx"] * STRIDE + e["pedal"]
        for q in e["params"]:
            if q["name"] == "FFT Size":
                fftp[e["fx"]] = q["idx"]
    out.append(f"giSMhero ftgen 0, 0, -{len(hero)}, -2, " + ", ".join(str(v) for v in hero) + "\n")
    out.append(f"giSMpedal ftgen 0, 0, -{len(pedal)}, -2, " + ", ".join(str(v) for v in pedal) + "\n")
    out.append(f"giSMfftp ftgen 0, 0, -{len(fftp)}, -2, " + ", ".join(str(v) for v in fftp) + "\n")
    out.append("; param names (strset index = idx)\n")
    for e in reg["effects"]:
        for q in e["params"]:
            out.append(f'strset {q["idx"]}, "{e["code"]}:{q["name"]}"\n')
    return "".join(out)


def main():
    if not os.path.exists(HEROES_JSON) and "--check" not in sys.argv:
        with open(HEROES_JSON, "w", encoding="utf-8") as fh:
            json.dump(overlay_picks(), fh, indent=1); fh.write("\n")
        print(f"seeded {HEROES_JSON} from OVERLAY")
    reg = build()
    js = json.dumps(reg, indent=1)
    orc = orc_text(reg)
    if "--check" in sys.argv:
        ok = (os.path.exists(OUT_JSON) and open(OUT_JSON).read() == js and
              os.path.exists(OUT_ORC) and open(OUT_ORC).read() == orc)
        print("sm_params: up to date" if ok else "sm_params: STALE — rerun generator")
        sys.exit(0 if ok else 1)
    open(OUT_JSON, "w").write(js)
    open(OUT_ORC, "w").write(orc)
    n = sum(len(e["params"]) for e in reg["effects"])
    print(f"wrote {OUT_JSON} and {OUT_ORC}: {len(reg['effects'])} effects, {n} params")


if __name__ == "__main__":
    main()

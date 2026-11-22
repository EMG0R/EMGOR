#!/home/gorpi7v5/kivy-venv/bin/python
"""Build a Rack-on-gorpi7v5 copy of one of MacBert's .vcv patches.

    wire_patch.py <src.vcv> <dst.vcv>

Originals in ~/demiurge_vcv are never modified. Applied to the copy:
  1. Fundamental Mixer has ONE output (id 0); MacBert's cables used id 6, which
     Rack drops silently, so no envelope ever got a gate (dead-silent patch).
  2. AudioInterface2 -> JACK (driver 4) device "Built-in Audio Pro" (the HAT's
     JACK name under PipeWire); the Mac save had Core Audio / "vcv".
  3. MIDI_CV <- "Midi Through" (demiurge clock, 24 PPQN), CLOCK out -> the master
     Clocked's BPM input; that Clocked set to running, BPM detection, ppqn 24.
  4. MIDICC_CV <- "VCV Macros" ch9 (vcv_macros.py), slots = CC30..35; slot 1
     (CC31, arcade) replaces every Fundamental Push output (manual trigger).
  5. MIDIMap <- "VCV Macros" ch9: CC30 -> every Branches probability (param 0),
     CC32 -> Plaits TIMBRE / Braids TIMBRE, CC33 -> Plaits MORPH / Ripples FREQ.
ALSA client numbers change across reboots, so device names are resolved now.
"""
import copy, json, os, re, subprocess, sys, tempfile

MIXER_OUT = 0
CLOCKED_BPM_IN, MIDICV_CLOCK_OUT = 6, 7
MACRO_CCS = [30, 31, 32, 33, 34, 35, 36, 37, 38] + [-1] * 7   # slot 8 = CC38 arcade R   # slot i -> MIDICC output i
# (plugin, model) -> {macro cc: paramId}
PARAM_MAP = {
    ("AudibleInstruments", "Branches"): {30: 0},
    # Plaits has no filter; its LPG colour (9) shapes the note envelope, which made
    # accel Y feel like an ADSR (Emory). Accel Y -> MORPH (5) instead; envelopes = joystick.
    ("AudibleInstruments", "Plaits"):   {32: 4, 33: 5},
    ("AudibleInstruments", "Braids"):   {32: 3, 33: 5},    # TIMBRE, COLOR
    # Ripples params are RES 0, FREQ 1: accel Y was on 0 = resonance sweeps
    # (squeal), not the filter. FREQ (2026-10-27).
    ("AudibleInstruments", "Ripples"):  {33: 1},
    ("AudibleInstruments", "Rings"):    {32: 4, 33: 3},    # BRIGHTNESS, STRUCTURE
}


def alsa_name(client_name):
    """RtMidi ALSA port name: '<client>:<port> <c>:<p>' for the first port of client."""
    txt = subprocess.run(["aconnect", "-l"], capture_output=True, text=True).stdout
    m = re.search(r"client (\d+): '" + re.escape(client_name) + r"'.*?\n\s+(\d+) '([^']*?)\s*'", txt, re.S)
    if not m:
        sys.exit(f"ALSA client {client_name!r} not found")
    return f"{client_name}:{m.group(3)} {m.group(1)}:{m.group(2)}"


def unpack(path, dst):
    subprocess.run(["tar", "--zstd", "-C", dst, "-xf", path], check=True)


def pack(src, path):
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        tar = subprocess.Popen(["tar", "-C", src, "-cf", "-", "."], stdout=subprocess.PIPE)
        subprocess.run(["zstd", "-q", "-19"], stdin=tar.stdout, stdout=f, check=True)
        tar.wait()
    os.replace(tmp, path)


def main(src, dst):
    through, macros = alsa_name("Midi Through"), alsa_name("VCV Macros")
    work = tempfile.mkdtemp(prefix="wire_", dir=os.path.expanduser("~/vcv"))
    unpack(src, work)
    pj = os.path.join(work, "patch.json")
    d = json.load(open(pj))
    # 0. MacBert's Core MIDI slugs don't exist in Rack ("Could not find module
    #    Core/MIDI_CV"), so those modules never loaded. Real Rack 2 slugs:
    for m in d["modules"]:
        if m["plugin"] == "Core":
            m["model"] = {"MIDI_CV": "MIDIToCVInterface", "MIDICC_CV": "MIDICCToCVInterface",
                          "MIDIMap": "MIDI-Map"}.get(m["model"], m["model"])
    # 0b. Synthesis Technology E350 (Morphing Terrarium) is proprietary: no source
    #     to build for arm64 and no library login on the Pi, so patch B's two
    #     voices loaded as nothing (no tone). Stand-in: Plaits on its WAVETABLE
    #     engine (5), the closest morphing-wavetable VCO: E350 in 0 (V/oct) ->
    #     Plaits NOTE 7, in 4 (morph CV) -> MORPH 3 with the MORPH CV
    #     attenuverter at 1, out 0 -> OUT 0. Pi-only; the Mac patch keeps its E350.
    e350 = {m["id"] for m in d["modules"] if m["plugin"] == "SynthesisTechnology" and m["model"] == "E350"}
    for m in d["modules"]:
        if m["id"] in e350:
            m.update({"plugin": "AudibleInstruments", "model": "Plaits", "version": "2.0.0",
                      "data": {"lowCpu": False, "model": 5},
                      "params": [{"id": i, "value": v} for i, v in
                                 enumerate([0, 0, 0, 0.5, 0.5, 0.5, 0, 0, 1.0, 0.5, 0.5])]})
    for c in list(d["cables"]):
        if c["inputModuleId"] in e350:
            if c["inputId"] in (0, 4):
                c["inputId"] = 7 if c["inputId"] == 0 else 3
            else:
                d["cables"].remove(c)
        if c["outputModuleId"] in e350 and c["outputId"] != 0:
            d["cables"].remove(c)
    if e350:
        print(f"E350 -> Plaits wavetable: {len(e350)}")
    mods = {m["id"]: m for m in d["modules"]}
    by = lambda model: [m for m in d["modules"] if m["model"] == model]
    cables = d["cables"]
    next_id = max([c["id"] for c in cables] + [0]) + 1

    def cable(out_mod, out_id, in_mod, in_id):
        nonlocal next_id
        cables.append({"id": next_id, "outputModuleId": out_mod, "outputId": out_id,
                       "inputModuleId": in_mod, "inputId": in_id, "color": "#a855f7"})
        next_id += 1

    # 1. Mixer output id
    n = 0
    for c in cables:
        m = mods[c["outputModuleId"]]
        if m["plugin"] == "Fundamental" and m["model"] == "Mixer" and c["outputId"] != MIXER_OUT:
            c["outputId"] = MIXER_OUT; n += 1
    print(f"mixer cables fixed: {n}")

    # 1b. output level -20% (Emory: slightly distorted): VCA-1s feeding the audio out
    for c in cables:
        if mods[c["inputModuleId"]]["model"] == "AudioInterface2":
            src = mods[c["outputModuleId"]]
            if src["model"] == "VCA-1" and not src.get("_lvl"):
                for p in src["params"]:
                    if p["id"] == 0:
                        p["value"] *= 0.8
                src["_lvl"] = True
    for m in d["modules"]:
        m.pop("_lvl", None)

    # 2. audio
    for m in by("AudioInterface2"):
        a = m.setdefault("data", {}).setdefault("audio", {})
        a.update({"driver": 4, "deviceName": "Built-in Audio Pro", "sampleRate": 48000.0,
                  "blockSize": 128, "inputOffset": 0, "outputOffset": 0})

    # 2b. stereo out on Rack outports 0/1 (= csound6:input9/10 = rack L/R). The Mac
    #     patches put their audio on Core Audio channels that mean nothing here:
    #     A in3+in1, B in2+in1, C in1 twice (EMGOR 2026-10-27: "just showing on the
    #     right channel"). Cables into an AudioInterface keep their source; the
    #     first goes to in0 (L), the second to in1 (R); one cable is doubled so a
    #     mono voice lands on both sides; any further ones are dropped.
    for m in by("AudioInterface2"):
        outs = [c for c in cables if c["inputModuleId"] == m["id"]]
        if not outs:
            continue
        if len(outs) == 1:
            o = outs[0]
            cable(o["outputModuleId"], o["outputId"], m["id"], 1)
            outs = [c for c in cables if c["inputModuleId"] == m["id"]]
        for side, c in enumerate(outs[:2]):
            c["inputId"] = side
        for c in outs[2:]:
            cables.remove(c)
        n += 1

    # 2c. NOTES: every WeightedQuantizer = equal weight on the 7 notes of C major,
    #     0 elsewhere (EMGOR 2026-10-27: "full equal probability in major scale").
    #     The module picks the note from frac(input) by cumulative weight, so equal
    #     weights only mean equal odds if frac(input) is uniform: feed every
    #     quantizer's S&H row from WHITE noise (Gaussian, sd 3.5 V x the 8vert
    #     gain >= ~1 V -> frac uniform to ~1e-8). Pink/red rows were correlated,
    #     and B/C/D's second voice had NO source at all (RND off -> 0 V -> one note).
    MAJOR = (0, 2, 4, 5, 7, 9, 11)
    noise = by("Noise")
    qn = 0
    for q in by("WeightedQuantizer"):
        for p in q["params"]:
            if p["id"] < 12:
                p["value"] = 5.0 if p["id"] in MAJOR else 0.0
        for c in [c for c in cables if c["inputModuleId"] == q["id"] and c["inputId"] != 1]:
            cables.remove(c)                                  # no CV on the weights
        for c in [c for c in cables if c["inputModuleId"] == q["id"] and c["inputId"] == 1]:
            v = mods[c["outputModuleId"]]                     # 8vert row
            row = c["outputId"]
            for c2 in [c2 for c2 in cables if c2["inputModuleId"] == v["id"] and c2["inputId"] == row]:
                sh = mods[c2["outputModuleId"]]
                if sh["model"] != "SHASR" or not noise:
                    continue
                k = c2["outputId"]
                for c3 in [c3 for c3 in cables if c3["inputModuleId"] == sh["id"] and c3["inputId"] == k]:
                    cables.remove(c3)
                # one Noise per quantizer: two voices sampling the same white
                # noise on the same clock edge would land on the same note
                if qn < len(noise):
                    src = noise[qn]
                else:
                    src = copy.deepcopy(noise[0])
                    src["id"] = max(m["id"] for m in d["modules"]) + 1
                    src["pos"] = [src["pos"][0], src["pos"][1] + 1]
                    d["modules"].append(src); mods[src["id"]] = src; noise.append(src)
                cable(src["id"], 0, sh["id"], k)
        qn += 1
    print(f"quantizers -> C major equal: {len(by('WeightedQuantizer'))}, {len(noise)} noise sources")

    # 3. clock
    midicv = by("MIDIToCVInterface")[0]
    midicv["data"] = {"midi": {"driver": 2, "deviceName": through, "channel": -1}}
    taken = {(c["inputModuleId"], c["inputId"]) for c in cables}
    masters = [m for m in by("Clocked") if (m["id"], CLOCKED_BPM_IN) not in taken]
    for m in masters:
        cable(midicv["id"], MIDICV_CLOCK_OUT, m["id"], CLOCKED_BPM_IN)
    for m in by("Clocked"):
        m.setdefault("data", {}).update({"running": True})
    for m in masters:
        m["data"].update({"bpmDetectionMode": True, "ppqn": 24})
    print(f"clock -> {len(masters)} master Clocked")

    # 4. arcade replaces Push
    cc = by("MIDICCToCVInterface")[0]
    cc["data"] = {"ccs": MACRO_CCS, "midi": {"driver": 2, "deviceName": macros, "channel": 8},
                  "smooth": False}
    push_ids = {m["id"] for m in by("Push")}
    n = 0
    for c in cables:
        if c["outputModuleId"] in push_ids:
            c["outputModuleId"], c["outputId"] = cc["id"], 1; n += 1
    # 4a. arcade ALTERNATES L / R voice (EMGOR 2026-10-27). vcv_macros.py sends
    #     odd presses on CC31 (slot 1), even on CC38 (slot 8). A trigger cable
    #     whose target only reaches the audio out's R input (in1, step 2b) moves
    #     to slot 8; L-side and both-side targets stay on slot 1.
    ai = by("AudioInterface2")
    def sides(mid):
        seen, todo, got = set(), [mid], set()
        while todo:
            x = todo.pop()
            if x in seen:
                continue
            seen.add(x)
            for c in cables:
                if c["outputModuleId"] == x:
                    if ai and c["inputModuleId"] == ai[0]["id"]:
                        got.add(c["inputId"])
                    else:
                        todo.append(c["inputModuleId"])
        return got
    nr = 0
    for c in cables:
        if c["outputModuleId"] == cc["id"] and c["outputId"] == 1 and sides(c["inputModuleId"]) == {1}:
            c["outputId"] = 8; nr += 1
    print(f"push cables -> arcade: {n} ({nr} on the R voice)")

    # 4b. LFOs -> lidar macros (Emory: modulation comes from the body, not free
    #     LFOs). Each LFO module's output cables move to MIDICC slot 4 (CC34,
    #     TOF 1) or slot 5 (CC35, TOF 2), alternating per LFO module. Noise /
    #     Random / SHASR stay: they are the generative note engine, not macros.
    lfos = [m for m in d["modules"] if m["plugin"] == "Fundamental" and m["model"] in ("LFO", "LFO2")]
    #     Envelopes go on the JOYSTICK (Emory): an LFO cable into an ADSR's attack
    #     CV (in 0) -> slot 6 (CC36, joystick X), decay CV (in 1) -> slot 7 (CC37,
    #     joystick Y). Every other LFO cable -> lidar slot 4/5 alternating per LFO.
    lfo_ids = {m["id"] for m in lfos}
    other = [m["id"] for m in lfos]
    n = {"joy": 0, "lidar": 0}
    for c in cables:
        if c["outputModuleId"] not in lfo_ids:
            continue
        if mods[c["inputModuleId"]]["model"] == "ADSR" and c["inputId"] in (0, 1):
            c["outputId"] = 6 + c["inputId"]; n["joy"] += 1
        else:
            c["outputId"] = 4 + (other.index(c["outputModuleId"]) % 2); n["lidar"] += 1
        c["outputModuleId"] = cc["id"]
    print(f"lfo cables -> joystick {n['joy']}, lidar {n['lidar']}")

    # 5. knob maps
    maps = []
    for m in d["modules"]:
        for ccn, pid in PARAM_MAP.get((m["plugin"], m["model"]), {}).items():
            maps.append({"cc": ccn, "moduleId": m["id"], "paramId": pid})
    mm = by("MIDI-Map")[0]
    mm["data"] = {"maps": maps, "smooth": True,
                  "midi": {"driver": 2, "deviceName": macros, "channel": 8}}
    print(f"midimap: {len(maps)} maps")

    json.dump(d, open(pj, "w"), indent=1)
    pack(work, dst)
    subprocess.run(["rm", "-rf", work])
    print("wrote", dst, "| clock:", through, "| macros:", macros)


if __name__ == "__main__":
    main(*sys.argv[1:3])

#!/usr/bin/env python3
"""OP FREAK v2 integration bench: composes recipes/opfreak_v2.json (every feature's hooks chained by the composer),
loads the real image code into the Blackfin simulator and drives the composed entry points against stub TE functions.

    PATH=toolchain/bfin/bin:$PATH .venv/bin/python tests/v2sim/run_v2sim.py

Checks (driver.c): stock drwave untouched; tag 1 = THE PURP audible/bounded, 6th voice safe; engine-list rename;
T keys left to TE; PURP pushes/turns/pages/GEN/presets; sequencer opens only on the tombola screen; all six modes make notes;
RUN/STOP; probe toggle survives. Screenshots to build/demos/v2_*.png. Not a hardware test."""
import json, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SDK = os.path.join(ROOT, "vendor", "op1-sdk")
sys.path.insert(0, os.path.join(SDK, "sim"))
sys.path.insert(0, os.path.join(SDK, "tools"))
os.environ.setdefault("OP1_SDK", SDK)
from simtest import asm_bin, c_bin, run_model, run_gdb, composed  # noqa: E402
from build import compile_project  # noqa: E402

# our feature dirs (the composer only knows vendor/op1-sdk/features)
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import compose_ofp  # noqa: E402,F401  (patches compose.load_feature)

OUT = os.path.join(SDK, "build", "bench", "v2sim")
os.makedirs(OUT, exist_ok=True)
RECIPE = json.load(open(os.path.join(ROOT, "recipes", "opfreak_v2.json")))
DRIVER_AT, STACK = 0x01F00000, 0x01F7FFFC
STUBS = {0x0197C870: 0, 0x0197A826: 1, 0x01A00D14: 2, 0x01A85750: 3, 0x01A85840: 4}
SHOTS = [("v2_purp_osc", 0), ("v2_purp_matrix", 1), ("v2_purp_presets", 2), ("v2_purp_allpages", 3),
         ("v2_seq_open", 4), ("v2_seq_cellular", 5)]


def main():
    proj, cfg = composed(RECIPE["features"], RECIPE["name"] + "_sim")
    code, syms, elf = compile_project(proj, cfg)
    print(f"image code: {len(code)} bytes at 0x01e00000")
    assert len(code) < DRIVER_AT - 0x01E00000
    inc = ["-I", os.path.join(ROOT, "src", "seq"), "-I", os.path.join(ROOT, "src", "engines", "purp"),
           "-I", os.path.join(ROOT, "src", "common")]
    want = ["_eh_te_render", "_eh_te_note_on", "_compose_init", "_compose_engine_name", "_compose_on_key", "_compose_on_knob",
            "_compose_frame", "_compose_seq_tick", "_compose_kb_note_on", "_compose_kb_note_off", "_S", "_open_", "_g_pu",
            "_pp_page", "_pp_preset"]
    driver, d = c_bin([os.path.join(HERE, "crt.S"), os.path.join(HERE, "driver.c")], DRIVER_AT, os.path.join(OUT, "driver"),
                      {"STACK": STACK, **{w: syms[w] for w in want}}, cflags=["-O1"] + inc)
    segs = [(0x01E00000, code), (DRIVER_AT, driver)]
    segs += [(0x03000000 + n * 0x40000, bytes(320 * 512)) for n in range(6)]
    for addr, which in STUBS.items():
        stub, _ = asm_bin(os.path.join(HERE, "te_stubs.S"), addr, os.path.join(OUT, f"stub{which}"),
                          {"WHICH": which, "DRWAVE_RENDER": d["_drwave_render"], "DRWAVE_NOTE_ON": d["_drwave_note_on"],
                           "LOG_SAVE": d["_log_save"], "LOG_NOTE_ON": d["_log_note_on"], "LOG_NOTE_OFF": d["_log_note_off"]},
                          entry="_stub")
        segs.append((addr, stub))
    elf_run = os.path.join(OUT, "v2sim_run.elf")
    r = run_model(elf_run, segs, d["_start"], timeout=900)
    sym = ["_result_fails", "_purp_peak", "_purp_cycles", "_seq_cycles", "_frame_cycles"]
    vals = run_gdb(elf_run, [f"*(unsigned*){d[s]:#x}" for s in sym]
                   + [f"*(unsigned*){d['_seq_notes_by_mode'] + 4 * i:#x}" for i in range(6)]
                   + [f"*(int*){d['_fail_lines'] + 4 * i:#x}" for i in range(8)],
                   [(n, 0x03000000 + k * 0x40000) for n, k in SHOTS], timeout=900)
    if len(vals) >= 19 and any(int(x) for x in vals[11:19]):
        print("failed CHECK lines in tests/v2sim/driver.c: " + " ".join(x for x in vals[11:19] if int(x)))
    if len(vals) >= 11:
        print(f"failed check bits: {vals[0]}  (bit n = driver.c CHECK(n))")
        print(f"PURP peak {vals[1]}; cycles: PURP voice block {vals[2]}, seq tick {vals[3]}, PURP screen frame {vals[4]}")
        print("seq note-ons in ~12 s per mode (TURING EUCLID MARKOV GAMELAN CELLULAR TOTAL): " + " ".join(vals[5:11]))
    demo = os.path.join(SDK, "build", "demo")
    os.makedirs(os.path.join(ROOT, "build", "demos"), exist_ok=True)
    for n, _ in SHOTS:
        if os.path.exists(os.path.join(demo, n + ".png")):
            shutil.copy(os.path.join(demo, n + ".png"), os.path.join(ROOT, "build", "demos", n + ".png"))
    print("PASS" if r.returncode == 0 else f"FAIL (exit {r.returncode})")
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()

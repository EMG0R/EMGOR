#!/usr/bin/env python3
"""Draw the probe's two pages on the Blackfin simulator with made-up numbers and save screenshots; check the toggle.

    PATH=toolchain/bfin/bin:$PATH .venv/bin/python src/platform/ofp_probe/sim/run_probesim.py
PNGs: vendor/op1-sdk/build/demo/ofp_probe_p1.png / _p2.png (copied to build/demos/). Not a hardware test."""
import os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
SDK = os.path.join(ROOT, "vendor", "op1-sdk")
sys.path.insert(0, os.path.join(SDK, "sim"))
from simtest import c_bin, run_model, run_gdb  # noqa: E402

PROBE = os.path.normpath(os.path.join(HERE, ".."))
OUT = os.path.join(SDK, "build", "bench", "probesim")
os.makedirs(OUT, exist_ok=True)
DRIVER_AT, STACK = 0x01F00000, 0x01F7FFFC


def main():
    srcs = [os.path.join(HERE, "crt.S"), os.path.join(HERE, "driver.c"), os.path.join(PROBE, "ofp_core.c"),
            os.path.join(SDK, "lib", "gfx.c"), os.path.join(SDK, "lib", "rt.c")]
    code, d = c_bin(srcs, DRIVER_AT, os.path.join(OUT, "driver"), {"STACK": STACK},
                    cflags=["-O2", "-I", PROBE, "-I", os.path.join(PROBE, ".."), "-DOFP_SIM=1"])
    elf = os.path.join(OUT, "probesim_run.elf")
    segs = [(DRIVER_AT, code), (0x03000000, bytes(320 * 512)), (0x03100000, bytes(320 * 512)),
            (0xFF801000, bytes(0x1000))]
    r = run_model(elf, segs, d["_start"], timeout=300)
    vals = run_gdb(elf, [f"/x *(unsigned*){d['_result']:#x}"], [("ofp_probe_p2", 0x03000000), ("ofp_probe_p1", 0x03100000)])
    print("key toggle checks:", "PASS" if r.returncode == 0 and vals and int(vals[0], 16) == 0 else f"FAIL ({vals}, exit {r.returncode})")
    demo = os.path.join(SDK, "build", "demo")
    os.makedirs(os.path.join(ROOT, "build", "demos"), exist_ok=True)
    for n in ("ofp_probe_p1", "ofp_probe_p2"):
        if os.path.exists(os.path.join(demo, n + ".png")):
            shutil.copy(os.path.join(demo, n + ".png"), os.path.join(ROOT, "build", "demos", n + ".png"))
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()

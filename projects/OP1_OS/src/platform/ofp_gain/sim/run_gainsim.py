#!/usr/bin/env python3
"""Run ofp_gain_block on the Blackfin simulator (bf524) and compare its output checksum with the host build.

    .venv/bin/python src/platform/ofp_gain/sim/run_gainsim.py        (needs PATH to contain toolchain/bfin/bin)

Checks: the Blackfin compile of the DSP is bit-identical to the host test's arithmetic; prints cycles per 64-frame
block (CYCLES register in the simulator: an instruction-count model, not hardware timing: UNVERIFIED as a cost)."""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
SDK = os.path.join(ROOT, "vendor", "op1-sdk")
sys.path.insert(0, os.path.join(SDK, "sim"))
from simtest import c_bin, run_model, run_gdb  # noqa: E402

GAIN = os.path.normpath(os.path.join(HERE, ".."))
OUT = os.path.join(SDK, "build", "bench", "gainsim")
os.makedirs(OUT, exist_ok=True)
DRIVER_AT, STACK = 0x01F00000, 0x01F7FFFC


def host_sum():
    exe = os.path.join(OUT, "xcheck_host")
    src = os.path.join(OUT, "xcheck_host.c")
    open(src, "w").write('#include <stdio.h>\n#include "xcheck.h"\nint main(void){printf("%u\\n", xcheck_run(0, 0));return 0;}\n')
    subprocess.run(["cc", "-O1", "-I", GAIN, "-o", exe, src, os.path.join(GAIN, "ofp_gain_dsp.c"),
                    os.path.join(GAIN, "ofp_gain_tab.c")], check=True)
    return int(subprocess.run([exe], capture_output=True, text=True, check=True).stdout.strip())


def main():
    code, d = c_bin([os.path.join(HERE, "crt.S"), os.path.join(HERE, "driver.c"),
                     os.path.join(GAIN, "ofp_gain_dsp.c"), os.path.join(GAIN, "ofp_gain_tab.c")],
                    DRIVER_AT, os.path.join(OUT, "driver"), {"STACK": STACK}, cflags=["-O2", "-I", GAIN])
    elf = os.path.join(OUT, "gainsim_run.elf")
    r = run_model(elf, [(DRIVER_AT, code)], d["_start"], timeout=300)
    vals = run_gdb(elf, [f"/x *(unsigned*){d['_sum']:#x}"] + [f"*(unsigned*){d['_cyc'] + 4 * i:#x}" for i in range(8)],
                   timeout=300)
    if r.returncode or len(vals) < 9:
        sys.exit(f"FAIL: simulator exit {r.returncode}, values {vals}")
    sim = int(vals[0], 16)
    host = host_sum()
    print(f"checksum host {host:#010x} sim {sim:#010x}")
    print("cycles per 64-frame block at positions 0/60/100/101/130/170/190/200:", ", ".join(vals[1:9]))
    if sim != host:
        sys.exit("FAIL: Blackfin build differs from the host arithmetic")
    print("PASS")


if __name__ == "__main__":
    main()

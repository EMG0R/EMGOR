#!/usr/bin/env python3
"""Run the PURP / LIL DRUMMER cores in the SDK's Blackfin simulator and print cycles per sample.
    .venv/bin/python src/engines/sim/run_bench.py      (needs scripts/prep_env.sh done; touches only build/)"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
SDK = os.path.join(ROOT, "vendor", "op1-sdk")
os.environ.setdefault("PATH", "")
os.environ["PATH"] = os.path.join(ROOT, "toolchain", "bfin", "bin") + os.pathsep + os.environ["PATH"]
sys.path.insert(0, os.path.join(SDK, "sim"))
from simtest import c_bin, run_model, run_gdb  # noqa: E402

OUT = os.path.join(ROOT, "build", "bench_engines")
os.makedirs(OUT, exist_ok=True)
ENG = os.path.join(ROOT, "src", "engines")
COM = os.path.join(ROOT, "src", "common")
AT, STACK = 0x01F00000, 0x01F7FFFC
CF = ["-O2", "-fno-tree-loop-distribute-patterns", "-I", ENG, "-I", os.path.join(ENG, "..")]
CF += ["-I", os.path.join(ENG, "purp"), "-I", os.path.join(ENG, "lildrummer")]
NAMES = ["c_pu_1v_st", "c_pu_1v_mono", "c_pu_5v_growl", "c_pu_5v_pad", "c_pu_5v_noise", "c_pu_5v_res",
         "c_pu_voice_block", "c_ld_kick", "c_ld_snare", "c_ld_clap", "c_ld_hat", "c_ld_cow", "c_ld_zap", "c_chk_pu", "c_chk_ld"]


def main():
    srcs = [os.path.join(HERE, "crt.S"), os.path.join(HERE, "bench_sim.c"),
            os.path.join(ENG, "purp", "purp.c"), os.path.join(ENG, "lildrummer", "lildrummer.c"),
            os.path.join(COM, "op_common.c")]
    code, d = c_bin(srcs, AT, os.path.join(OUT, "bench"), {"STACK": STACK}, cflags=CF)
    print(f"image: {len(code)} bytes")
    elf = os.path.join(OUT, "bench_run.elf")
    r = run_model(elf, [(AT, code)], d["_start"], timeout=900)
    vals = run_gdb(elf, [f"*(unsigned*){d['_' + n]:#x}" for n in NAMES] + [f"/x *(unsigned*){d['_fails']:#x}"], timeout=900)
    if len(vals) != len(NAMES) + 1:
        print("gdb read failed", vals); sys.exit(2)
    res = dict(zip(NAMES, vals))
    print("fail bits:", vals[-1], "exit", r.returncode)
    for n in NAMES:
        v = res[n]
        if n.startswith("c_chk"):
            v = f"0x{int(v) & 0xffffffff:08x}"
        print(f"{n:18s} {v}")
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()

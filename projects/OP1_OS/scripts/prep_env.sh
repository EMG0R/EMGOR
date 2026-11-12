#!/usr/bin/env bash
# Environment prep for vendor/op1-sdk tests (fixes: numpy for drumtrim; hooktest .bin artifacts).
set -euo pipefail
cd "$(dirname "$0")/.."; ROOT=$PWD
.venv/bin/pip install -q numpy
export PATH=$ROOT/toolchain/bfin/bin:$PATH
cd vendor/op1-sdk
[ -e toolchain ] || ln -s ../../toolchain toolchain
python3 tools/build.py examples/hello "$ROOT/stock/op1_246.op1" >/dev/null
cd sim/hooktest
bfin-elf-as -o /tmp/cs.o check_stub.S && bfin-elf-ld -Ttext=0x01a42344 -o /tmp/cs.elf /tmp/cs.o 2>/dev/null && bfin-elf-objcopy -O binary /tmp/cs.elf check_stub.bin
bfin-elf-as -o /tmp/dr.o driver.S && bfin-elf-ld -Ttext=0x01f00000 -o /tmp/dr.elf /tmp/dr.o 2>/dev/null && bfin-elf-objcopy -O binary /tmp/dr.elf driver.bin

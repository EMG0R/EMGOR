#!/usr/bin/env bash
# usage: scripts/compose_ofp.sh recipes/<name>.json   (builds build/<name>.op1 from stock/op1_246.op1; never flashes)
set -euo pipefail
cd "$(dirname "$0")/.."; ROOT=$PWD
export PATH=$ROOT/toolchain/bfin/bin:$PATH
[ -e vendor/op1-sdk/toolchain ] || ln -s ../../toolchain vendor/op1-sdk/toolchain
exec "$ROOT/.venv/bin/python" "$ROOT/scripts/compose_ofp.py" "$1" "$ROOT/stock/op1_246.op1"

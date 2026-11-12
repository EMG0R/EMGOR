#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."; ROOT=$PWD
export PATH=$ROOT/toolchain/bfin/bin:$PATH
(cd vendor/op1-sdk; "$ROOT/.venv/bin/python" tools/compose.py "$ROOT/recipes/proof_v0.json" "$ROOT/stock/op1_246.op1")
cp vendor/op1-sdk/build/apps/op1os_proof_v0/build/op1os_proof_v0.op1 build/
(cd build && shasum -a 256 op1os_proof_v0.op1 | tee op1os_proof_v0.op1.sha256)

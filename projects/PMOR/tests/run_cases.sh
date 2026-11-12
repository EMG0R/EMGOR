#!/usr/bin/env bash
# run_cases.sh RENDER_BIN OUTDIR : renders every tests/cases/*.scn with RENDER_BIN into OUTDIR/<case>.f32
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; bin="$1"; out="$2"; mkdir -p "$out"
TS=""; command -v taskset >/dev/null 2>&1 && TS="taskset -c 0-2"
for f in "$here"/cases/*.scn; do n=$(basename "$f" .scn); nice -n19 ${TS:-} "$bin" "$f" "$out/$n.f32"; done

#!/usr/bin/env bash
# test_golden.sh RENDER_BIN : renders every tests/cases/*.scn and requires the sha256 of each output to equal
# tests/golden/MANIFEST.txt (captured from the PRE-extraction ff86e4c engine). Also: no NaN/inf in any render.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; bin="$1"; out="${2:-$(mktemp -d)}"
stat="${F32STAT:-$(dirname "$bin")/f32stat}"
. "$here/golden_lib.sh"
"$here/run_cases.sh" "$bin" "$out" || { echo "FAIL: render error"; exit 1; }
"$here/make_manifest.sh" "$out" > "$out/MANIFEST.new" || exit 1
fail=0; n=0
while read -r h name samples peak rms bad; do
  n=$((n+1))
  golden_ok "$here/golden/MANIFEST.txt" "$name" "$h" "$samples" "$peak" "$rms" || { echo "MISMATCH $name"; fail=$((fail+1)); }
  [ "$bad" = "0" ] || { echo "NONFINITE $name"; fail=$((fail+1)); }
done < "$out/MANIFEST.new"
want_n=$(count_lines "$here/golden/MANIFEST.txt")
[ "$n" = "$want_n" ] || { echo "case count $n != manifest $want_n"; fail=$((fail+1)); }
if [ $fail -eq 0 ]; then echo "test_golden: PASS ($n/$n renders match the pre-extraction goldens: bit-identical on Linux, peak/RMS within GOLDEN_TOL elsewhere)"; else echo "test_golden: $fail FAILURES"; exit 1; fi

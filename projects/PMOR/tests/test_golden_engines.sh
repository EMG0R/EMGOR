#!/usr/bin/env bash
# test_golden_engines.sh RENDER_BIN [OUT]: the NEW drum presets (16..23 of every engine) + depth-knob variants.
# Cases live in tests/cases_engines/, hashes in tests/golden/MANIFEST_engines.txt (the 217 classic goldens are untouched).
#   --update   rewrite the manifest from the current renders (review the diff before committing)
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; upd=0; [ "${1:-}" = "--update" ] && { upd=1; shift; }
bin="$1"; out="${2:-$(mktemp -d)}"; mkdir -p "$out"
. "$here/golden_lib.sh"
TS=""; command -v taskset >/dev/null 2>&1 && TS="taskset -c 0-2"
for f in "$here"/cases_engines/*.scn; do n=$(basename "$f" .scn); nice -n19 $TS "$bin" "$f" "$out/$n.f32" || { echo "FAIL: render $n"; exit 1; }; done
"$here/make_manifest.sh" "$out" > "$out/MANIFEST.new" || exit 1
if [ $upd = 1 ]; then cp "$out/MANIFEST.new" "$here/golden/MANIFEST_engines.txt"; echo "updated $(count_lines "$here/golden/MANIFEST_engines.txt") goldens"; exit 0; fi
fail=0; n=0
while read -r h name samples peak rms bad; do
  n=$((n+1)); golden_ok "$here/golden/MANIFEST_engines.txt" "$name" "$h" "$samples" "$peak" "$rms" || { echo "MISMATCH $name"; fail=$((fail+1)); }
  [ "$bad" = "0" ] || { echo "NONFINITE $name"; fail=$((fail+1)); }
done < "$out/MANIFEST.new"
want_n=$(count_lines "$here/golden/MANIFEST_engines.txt")
[ "$n" = "$want_n" ] || { echo "case count $n != manifest $want_n"; fail=$((fail+1)); }
[ $fail -eq 0 ] && echo "test_golden_engines: PASS ($n/$n)" || { echo "test_golden_engines: $fail FAILURES"; exit 1; }

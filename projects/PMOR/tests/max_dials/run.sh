#!/usr/bin/env bash
# run.sh -- (also runs run_load.sh + run_gate.sh) headless Max 9 check of the dial/umenu value conversion in max/devices/pmor_ui.js (macOS + Max.app only; SKIP otherwise).
# A [dial] takes `set` in STEPS and a [umenu] in item index; PMOR sends schema values. Expected log lines are asserted.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/../.."
[ -d /Applications/Max.app ] || { echo "SKIP: no Max.app"; exit 0; }
w="$(mktemp -d)"; trap 'rm -rf "$w"' EXIT
cp "$root/max/devices/pmor_ui.js" "$w/"
sed "s#@LOG@#$w/log.txt#" "$here/logger.js.in" > "$w/logger.js"
python3 "$here/make_ui_test.py" "$w/ui.maxpat"
( cd "$w" && open -n -W -a /Applications/Max.app ui.maxpat ) & mp=$!
for i in $(seq 1 40); do grep -q DONE "$w/log.txt" 2>/dev/null && break; sleep 1; done
sleep 1; kill $mp 2>/dev/null
fail=0
for want in "s8 2200" "s9 0.25" "p2052 1" "p2053 17" "p4137 2"; do
  grep -qx "$want" "$w/log.txt" 2>/dev/null && echo "  ok   $want" || { echo "  FAIL missing '$want'"; fail=$((fail+1)); }
done
[ $fail -eq 0 ] && echo "max_dials: PASS (5/5)" || { echo "max_dials: $fail FAIL"; cat "$w/log.txt" 2>/dev/null | head; exit 1; }
# more headless checks of the saved devices: clean console on load (no deleted patchcords), editors draw nothing while closed
bash "$here/run_load.sh" || exit 1
bash "$here/run_gate.sh" || exit 1

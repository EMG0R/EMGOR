#!/usr/bin/env bash
# run_gate.sh -- headless Max 9: an editor draws/updates NOTHING while closed (pmor_ui.js + pmor_browser.js `gated`), draws again on open,
# and pmor_edit.js tracks the editor window (show -> editor 1 + dumpparams/pool/presets, window closed -> editor 0). SKIP without Max.app.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/../.."
[ -d /Applications/Max.app ] || { echo "SKIP: no Max.app"; exit 0; }
. "$here/maxrun.sh"
w="$(cd "$(mktemp -d /tmp/pmor_gate.XXXXXX)" && pwd -P)"; trap "[ -n \"\${KEEP:-}\" ] || rm -rf \"$w\"" EXIT
cp "$root"/max/devices/pmor_*.js "$w/"; cp "$here/tester.js" "$w/"
sed "s#@LOG@#$w/log.txt#" "$here/logger.js.in" > "$w/logger.js"; sed "s#@LOG@#$w/log.txt#" "$here/counter.js.in" > "$w/counter.js"
python3 "$here/make_gate_test.py" "$w/gate.maxpat"
maxrun_open "$w/gate.maxpat" 40 "$w/log.txt"
L="$w/log.txt"; fail=0
grep -q "^DONE" "$L" 2>/dev/null || { echo "max_gate: FAIL (test patcher never finished; no DONE in log)"; exit 1; }
seg() { awk -v a="$1" -v b="$2" 'BEGIN{on=(a=="")} $0=="mark "a{on=1;next} $0=="mark "b{on=0} on' "$L"; }   # lines between two marks
chk() { local name="$1" got="$2" want="$3"; [ "$got" = "$want" ] && echo "  ok   $name ($got)" || { echo "  FAIL $name: got '$got' want '$want'"; fail=$((fail+1)); }; }
chk "ui: SETs while editor closed" "$(seg "" closed_end | grep -c '^SET ')" 0
chk "ui: replay on open (p2052, p2053)" "$(seg closed_end replay_end | grep -c '^SET ')" 2
chk "ui: replay carries latest values" "$(seg closed_end replay_end | grep '^SET ' | sort | tr '\n' '|')" "SET p2052 1|SET p2053 17|"
chk "ui: live param while open" "$(seg replay_end open_end | grep -c '^SET p2052 0.5')" 1
chk "compat: listed dial enabled" "$(seg replay_end open_end | grep '^ATTR p2054' | cut -d' ' -f2-4)" "p2054 ignoreclick 0"
chk "compat: missing toggle disabled + dimmed" "$(seg replay_end open_end | grep '^ATTR p2055')" "ATTR p2055 ignoreclick 1 checkedcolor 0.43,0.38,0.63,1"
chk "compat: dial keeps colour" "$(seg replay_end open_end | grep '^ATTR p2054' | cut -d' ' -f6-)" "0.6,0.9,0.6,1"
chk "ui: SETs after close" "$(seg open_end closed2_end | grep -c '^SET ')" 0
chk "browser: outlet lines while closed" "$(seg "" closed_end | grep -c '^B0 ')" 0
chk "browser: redraw on open" "$(seg closed_end replay_end | grep -c '^B0 \(clear\|append\)')" 3
chk "browser: lines after close" "$(seg open_end closed2_end | grep -c '^B0 ')" 0
chk "edit: show -> editor 1" "$(seg closed2_end shown_end | grep -c '^E editor 1')" 1
chk "edit: dumpparams/pool/presets requested" "$(seg closed2_end shown_end | grep -c '^E \(dumpparams\|pool\|presets\)')" 3
chk "edit: window closed -> editor 0" "$(seg shown_end closed_window_end | grep -c '^E editor 0')" 1
chk "edit: window closed -> button reset (S set 0)" "$(seg shown_end closed_window_end | grep -c '^S 0')" 1
chk "edit: reopen -> editor 1 + button on" "$(seg closed_window_end reshown_end | grep -c '^\(E editor 1\|S 1\)')" 2
chk "edit: window rectangle reported (R~l~t~r~b)" "$(seg closed2_end shown_end | grep -c '^R R~[-0-9]*~[-0-9]*~[-0-9]*~[-0-9]*$')" 1
chk "edit: hide -> editor 0 + button off" "$(seg reshown_end hidden_end | grep -c '^\(E editor 0\|S 0\)')" 2
[ $fail -eq 0 ] && echo "max_gate: PASS" || { echo "max_gate: $fail FAIL"; cat "$L" | head -40; exit 1; }

#!/usr/bin/env bash
# run.sh [scenario...] -- headless Max 9 end-to-end test of the saved PMOR devices (max/devices/*.maxpat) against the simulated PMOR
# (tools/simpmor_udp) over loopback UDP via `open udp<port>` of pmor_port.js. Scenarios: a b c d e f g (default: all; h,i not written).
#   a connect/ready/synced/patch   b dial -> SET_PARAM -> value back   c presets   d pool + assign (EDIT)   e STORE_RETRY
#   f disconnect/reconnect         g cable yank mid-transfer          h cache dirs (sessiondir / cachedir)   i assign from STUDIO + status line
# One Max launch per scenario (hard watchdog 90 s each). Scratch: $E2E_DIR (default mktemp). macOS + Max.app + installed externals, else SKIP (exit 0).
# Exit 0 = all PASS (or SKIP), 1 = any FAIL.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$(cd "$here/../.." && pwd)"
MAX=/Applications/Max.app; LIB="$HOME/Documents/Max 9/Library"
[ -d "$MAX" ] && [ -d "$LIB/pmor.proto.mxo" ] && [ -d "$LIB/pmor.core~.mxo" ] || { echo "max_e2e: SKIP (needs Max.app and pmor.*.mxo in ~/Documents/Max 9/Library; run max/build_externals.sh)"; exit 0; }
S="${E2E_DIR:-$(mktemp -d /tmp/pmor_e2e.XXXXXX)}"; mkdir -p "$S"
( [ -d "$root/build_host" ] || cmake -S "$root" -B "$root/build_host" >/dev/null ) && cmake --build "$root/build_host" --target simpmor_udp >/dev/null || { echo "max_e2e: FAIL build simpmor_udp"; exit 1; }
SIM="$root/build_host/simpmor_udp"
# hermetic copy of the REAL device files so js helpers resolve next to the patchers, not from a stale Library copy (stage_devices.py: byte copy,
# except the core's port messages if the shipped core still uses the shadowed `open`/`close`)
rm -rf "$S/devices"; eval "$(python3 "$here/stage_devices.py" "$root/max/devices" "$S/devices")"
[ "${SYNCBUG:-0}" = 1 ] && echo "max_e2e: NOTE shipped core feeds synced -> [pool] msg -> proto (floods); staged copy drops that message box"
[ "${PORTBUG:-0}" = 1 ] && echo "max_e2e: NOTE shipped core wires open/close (js built-ins); staged copy uses openport/closeport"
for f in "$root"/max/devices/*.js; do cmp -s "$f" "$S/devices/$(basename "$f")" || { echo "max_e2e: FAIL staging $f"; exit 1; }; done
cp "$here/driver.js" "$S/driver.js"
scns=("$@"); [ ${#scns[@]} -eq 0 ] && scns=(a b c d e f g)
tp=0; tf=0; badrun=0; port=47100
# Max reopens the patchers of the last dead instance (Crash Recovery/maxworkspace-<pid>.txt): stale test patchers of other runs would
# run alongside ours (and some quit Max). Drop the recovery logs of DEAD instances before each launch; live ones are never touched.
CR="$HOME/Library/Application Support/Cycling '74/Max 9/Crash Recovery"
clean_recovery() { for f in "$CR"/maxworkspace-*.txt; do [ -e "$f" ] || continue; p="${f##*maxworkspace-}"; p="${p%.txt}"; kill -0 "$p" 2>/dev/null || rm -f "$f"; done; }
maxpids() { ps -axo pid=,comm= | awk '$2 ~ /Max\.app\/Contents\/MacOS\/Max$/ {print $1}' | sort; }
for s in "${scns[@]}"; do
  port=$((port+10)); L="$S/log_$s.txt"; SL="$S/sim_$s.log"; C="$S/cache_$s"; rm -rf "$L" "$SL" "$C"; mkdir -p "$C"
  flags=(--pooled --log "$SL" --quiet)
  [ "$s" = g ] && flags+=(--drop-after-bytes "${YANK_BYTES:-26000}")
  "$SIM" $port "${flags[@]}" & simpid=$!
  python3 "$here/make_patcher.py" "$S/test_$s.maxpat" "$s" "$L" $port "$SL" "$S/devices" "$C" "${PORTBUG:-0}" "${SYNCBUG:-0}" || { kill $simpid; echo "max_e2e: FAIL make_patcher"; exit 1; }
  clean_recovery; before="$(maxpids)"
  ( cd "$S" && open -n -a "$MAX" "test_$s.maxpat" ) 
  t=0; while [ $t -lt 90 ]; do grep -q '^DONE' "$L" 2>/dev/null && break; sleep 1; t=$((t+1)); done
  grep -q '^DONE' "$L" 2>/dev/null || echo "FAIL $s: no DONE inside 90 s (Max hung or never ran the driver)" >> "$L"
  sleep 2
  after="$(maxpids)"; for p in $after; do echo "$before" | grep -qx "$p" || kill "$p" 2>/dev/null; done      # only Max instances started here
  sleep 1; for p in $after; do echo "$before" | grep -qx "$p" || kill -9 "$p" 2>/dev/null; done
  clean_recovery; kill $simpid 2>/dev/null; wait $simpid 2>/dev/null
  p=$(grep -c '^PASS' "$L" 2>/dev/null); f=$(grep -c '^FAIL' "$L" 2>/dev/null); p=${p:-0}; f=${f:-0}
  echo "== scenario $s: $p pass, $f fail   (log $L, sim log $SL)"; grep -E '^FAIL' "$L" | sed 's/^/     /'
  tp=$((tp+p)); tf=$((tf+f)); grep -q '^DONE' "$L" || badrun=$((badrun+1))
done
echo "max_e2e: $tp pass, $tf fail, $badrun scenario(s) did not finish   (scratch $S)"
[ $tf -eq 0 ] && [ $badrun -eq 0 ]

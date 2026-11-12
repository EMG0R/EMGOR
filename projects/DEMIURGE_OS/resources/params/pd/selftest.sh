#!/usr/bin/env bash
# usage: selftest.sh ENGINE_PORT POOL_PORT   (exit 0 PASS, 1 FAIL, 77 SKIP)
set -u
D="$(cd "$(dirname "$0")" && pwd)"; EP=${1:-19203}; PP=${2:-19103}; T=${DEMIURGE_ST_TIMEOUT:-12}
command -v pd >/dev/null || { echo "SKIP: pd not installed"; exit 77; }
cd "$D" || exit 1
LOG=$(mktemp)
# -nosound -nomidi -noprefs: opens no audio/MIDI device, reads no user prefs.
# -path: so the abstractions resolve; -send: env -> patch (Pd has no getenv).
nice -n 19 timeout -k 1 $((T+5)) pd -nogui -nosound -nomidi -noprefs -stderr -path "$D" \
  -send "demiurge-param-port $EP; demiurge-pool-port $PP" -open "$D/selftest.pd" >"$LOG" 2>&1 &
EPID=$!
trap 'kill $EPID 2>/dev/null; wait $EPID 2>/dev/null; rm -f "$LOG"' EXIT
python3 "$D/../testpool.py" selftest --pool-port "$PP" --engine-port "$EP" --timeout "$T"; rc=$?
[ $rc -ne 0 ] && { echo "--- pd log tail:"; tail -8 "$LOG"; }
exit $rc

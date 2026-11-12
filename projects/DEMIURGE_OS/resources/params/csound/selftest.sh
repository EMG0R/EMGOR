#!/usr/bin/env bash
# usage: selftest.sh ENGINE_PORT POOL_PORT   (exit 0 PASS, 1 FAIL, 77 SKIP)
set -u
D="$(cd "$(dirname "$0")" && pwd)"; EP=${1:-19201}; PP=${2:-19101}; T=${DEMIURGE_ST_TIMEOUT:-12}
command -v csound >/dev/null || { echo "SKIP: csound not installed"; exit 77; }
cd "$D" || exit 1
LOG=$(mktemp); 
nice -n 19 timeout -k 1 $((T+5)) csound --omacro:DEMIURGE_PARAM_PORT=$EP --omacro:DEMIURGE_POOL_PORT=$PP selftest.csd >"$LOG" 2>&1 &
EPID=$!
trap 'kill $EPID 2>/dev/null; wait $EPID 2>/dev/null; rm -f "$LOG"' EXIT
python3 "$D/../testpool.py" selftest --pool-port "$PP" --engine-port "$EP" --timeout "$T"; rc=$?
[ $rc -ne 0 ] && { echo "--- csound log tail:"; tail -5 "$LOG"; }
exit $rc

#!/usr/bin/env bash
# usage: selftest.sh ENGINE_PORT POOL_PORT   (exit 0 PASS, 1 FAIL, 77 SKIP)
set -u
D="$(cd "$(dirname "$0")" && pwd)"; EP=${1:-19205}; PP=${2:-19105}; T=${DEMIURGE_ST_TIMEOUT:-12}
command -v node >/dev/null || { echo "SKIP: node not installed"; exit 77; }
cd "$D" || exit 1
LOG=$(mktemp)
DEMIURGE_PARAM_PORT=$EP DEMIURGE_POOL_PORT=$PP nice -n 19 timeout -k 1 $((T+5)) node selftest.mjs >"$LOG" 2>&1 &
EPID=$!
trap 'kill $EPID 2>/dev/null; wait $EPID 2>/dev/null; rm -f "$LOG"' EXIT
python3 "$D/../testpool.py" selftest --pool-port "$PP" --engine-port "$EP" --timeout "$T"; rc=$?
[ $rc -ne 0 ] && { echo "--- node log tail:"; tail -5 "$LOG"; }
exit $rc

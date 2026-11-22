#!/usr/bin/env bash
# usage: selftest.sh ENGINE_PORT POOL_PORT   (exit 0 PASS, 1 FAIL, 77 SKIP)
set -u
D="$(cd "$(dirname "$0")" && pwd)"; EP=${1:-19204}; PP=${2:-19104}; T=${DEMIURGE_ST_TIMEOUT:-12}
command -v sclang >/dev/null || { echo "SKIP: sclang not installed"; exit 77; }
cd "$D" || exit 1
LOG=$(mktemp)
# sclang only: NO scsynth/server is booted, so no audio device is touched.
# QT offscreen + -D (daemon mode) = no window/IDE needed over ssh.
# -l: an empty config so ~/.config/SuperCollider/startup.scd (which might boot a server) is not run.
echo "" > "$LOG.yaml"
QT_QPA_PLATFORM=offscreen DEMIURGE_PARAM_PORT=$EP DEMIURGE_POOL_PORT=$PP \
  nice -n 19 timeout -k 1 $((T+5)) sclang -D -l "$LOG.yaml" "$D/selftest.scd" >"$LOG" 2>&1 &
EPID=$!
trap 'kill $EPID 2>/dev/null; wait $EPID 2>/dev/null; rm -f "$LOG" "$LOG.yaml"' EXIT
python3 "$D/../testpool.py" selftest --pool-port "$PP" --engine-port "$EP" --timeout "$T"; rc=$?
[ $rc -ne 0 ] && { echo "--- sclang log tail:"; tail -8 "$LOG"; }
exit $rc

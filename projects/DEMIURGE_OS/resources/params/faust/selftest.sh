#!/usr/bin/env bash
# usage: selftest.sh ENGINE_PORT POOL_PORT   (exit 0 PASS, 1 FAIL, 77 SKIP)
# Real OSC round-trip when a C++ toolchain + Faust OSC libs exist; otherwise manifest-only.
set -u
D="$(cd "$(dirname "$0")" && pwd)"; EP=${1:-19206}; PP=${2:-19106}; T=${DEMIURGE_ST_TIMEOUT:-12}
command -v faust >/dev/null || { echo "SKIP: faust not installed"; exit 77; }
W=$(mktemp -d); trap 'kill ${EPID:-0} 2>/dev/null; wait ${EPID:-0} 2>/dev/null; rm -rf "$W"' EXIT

# 1. manifest generator (compile-only, always runs)
python3 "$D/faust_manifest.py" "$D/dparam_example.dsp" --stage test --port "$EP" -o "$W/m.json" || { echo "FAIL: faust_manifest.py"; exit 1; }
python3 - "$W/m.json" <<'PY' || { echo "FAIL: manifest content wrong"; exit 1; }
import json, sys
m = json.load(open(sys.argv[1]))
p = {x["path"]: x for x in m["params"]}
assert p["/test/cutoff"]["address"] == "/cutoff" and "native" not in p["/test/cutoff"], p
assert p["/test/freq"]["native"]["curve"] == "exp", p
PY

# 2. real OSC: faust's built-in OSC (OSCUI) receives, arch reports /pout. Needs g++ + libOSCFaust + liblo.
HAVE_OSC=1
command -v g++ >/dev/null || HAVE_OSC=0
ls /usr/include/faust/gui/OSCUI.h /usr/include/lo/lo.h >/dev/null 2>&1 || HAVE_OSC=0
if [ $HAVE_OSC = 0 ]; then
  echo "PASS: manifest generator only (no g++/faust OSC libs: OSC round-trip not exercised)"; exit 0
fi
( cd "$W" && faust -a "$D/faust_pool_arch.cpp" "$D/dparam_example.dsp" -o f.cpp \
  && nice -n 19 g++ -O1 -std=c++17 f.cpp -o f -lOSCFaust -llo -lpthread ) >"$W/build.log" 2>&1 \
  || { echo "FAIL: arch build"; tail -5 "$W/build.log"; exit 1; }
DEMIURGE_STAGE=test DEMIURGE_PARAM_PORT=$EP DEMIURGE_POOL_PORT=$PP nice -n 19 timeout -k 1 $((T+5)) "$W/f" >"$W/run.log" 2>&1 &
EPID=$!
python3 "$D/faust_osc_check.py" --pool-port "$PP" --engine-port "$EP" --timeout "$T"; rc=$?
[ $rc -ne 0 ] && { echo "--- faust engine log tail:"; tail -5 "$W/run.log"; }
exit $rc

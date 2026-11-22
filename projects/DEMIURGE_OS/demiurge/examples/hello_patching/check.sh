#!/usr/bin/env bash
# check.sh -- offline two-way parameter round trip for the seven hello_patching stages.
#
#   bash check.sh                   all seven stages
#   bash check.sh chuck pd          just these (strudel chuck pd sc faust cpp csound)
#   bash check.sh --timeout 20      per-stage wait for the echo (default 15 s)
#   bash check.sh softmap           the hello_softmap csound voice (extra, not in the seven)
#
# For each stage: start the fake pool (resources/params/testpool.py) on a test port, launch
# the stage's real file SILENTLY with the same env/argv the launcher builds
# (src/demiurge-launcher-rs/src/params.rs), send `/p <path> 0.6` to its port and require the
# matching `/pout <path> 0.6` back.
#
# SAFE ON A LIVE RIG: opens no audio device, makes no JACK/PipeWire client, restarts no
# service. Silent modes: csound -+rtaudio=null, chuck --silent, pd -nosound -nomidi,
# sclang with DEMIURGE_NO_AUDIO=1 (no scsynth), cpp_crush with DEMIURGE_NO_AUDIO=1 (no JACK,
# no ALSA seq), faust built with the reference arch (no audio I/O), Strudel evaluated in a
# plain node scheduler (no audio, no MIDI). UDP ports 18400-18499 only (--port-base N).
# Every engine runs `nice -n 19` in its own process group, killed on exit.
#
# Result per stage:  PASS  echo seen
#                    FAIL  engine died / build failed / tool missing (see log tail)
#                    HANG  engine still running, no echo within the timeout
# Exit 0 only if every selected stage PASSed; otherwise 1. Logs: $LOGDIR (kept on failure).
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TIMEOUT=15
PORT_BASE=${CHECK_PORT_BASE:-18400}
VALUE=0.6
STAGES=()
while [ $# -gt 0 ]; do
  case "$1" in
    --timeout) TIMEOUT=$2; shift 2 ;;
    --port-base) PORT_BASE=$2; shift 2 ;;
    -h|--help) sed -n 2,26p "$0"; exit 0 ;;
    strudel|chuck|pd|sc|faust|cpp|csound|softmap) STAGES+=("$1"); shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
[ ${#STAGES[@]} -eq 0 ] && STAGES=(strudel chuck pd sc faust cpp csound)
if [ "$PORT_BASE" -lt 18000 ] || [ "$PORT_BASE" -gt 18900 ]; then
  echo "port base must be inside 18000-18899" >&2; exit 2
fi

# ---- locate the hook library ($DEMIURGE_RESOURCES, repo checkout, or an install) ----
RES=""
for c in "${DEMIURGE_RESOURCES:-}" "$HERE/../../../resources" "$HERE/../../resources" \
         "$HOME/demiurge/resources" /opt/demiurge/resources; do
  [ -n "$c" ] && [ -f "$c/params/testpool.py" ] && { RES="$(cd "$c" && pwd)"; break; }
done
[ -z "$RES" ] && { echo "FAIL: cannot find resources/params/testpool.py (set DEMIURGE_RESOURCES)"; exit 1; }
POOLPY="$RES/params/testpool.py"
STRUDEL_HOME="${STRUDEL_HOME:-$HOME/demiurge/strudel}"
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

LOGDIR="$(mktemp -d "${TMPDIR:-/tmp}/hello_patching_check.XXXXXX")"
PGID=""
cleanup() { killgroup; if [ "${KEEP:-0}" != 1 ]; then rm -rf "$LOGDIR"; fi; }
killgroup() {
  if [ -n "$PGID" ]; then
    kill -TERM -- "-$PGID" 2>/dev/null; sleep 0.3; kill -KILL -- "-$PGID" 2>/dev/null
    PGID=""
  fi
}
FAILS=0
trap 'killgroup; exit 130' INT TERM
trap cleanup EXIT

# Start "$@" detached in its own session/process group. Sets ENG_PID and PGID.
start_engine() {  # name cmd...
  local name=$1; shift
  setsid nice -n 19 timeout -k 2 $((TIMEOUT + 25)) "$@" >"$LOGDIR/$name.log" 2>&1 < /dev/null &
  ENG_PID=$!
  PGID=$ENG_PID
}

# Echo check. mode p: engine hears `/p <path> <v>`. mode faust: engine hears Faust's own OSC
# address `/faust_delay/feedback <v>` (what the pool's osc sink sends) and must answer /pout.
echo_check() {  # path engine_port pool_port mode
  python3 - "$RES/params" "$1" "$2" "$3" "$4" "$VALUE" "$TIMEOUT" <<'PY'
import sys, time, socket
sys.path.insert(0, sys.argv[1])
import testpool
path, ep, pp, mode, val, tmo = sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5], float(sys.argv[6]), float(sys.argv[7])
pool = testpool.TestPool(pp); pool.start()
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
end = time.time() + tmo
while time.time() < end:
    if mode == "faust":
        sock.sendto(testpool.encode("/faust_delay/feedback", val), ("127.0.0.1", ep))
    else:
        pool.send_p(ep, path, val)
    if pool.wait_pout(path, val, timeout=0.25):
        print(f"{path}={val} echoed"); pool.stop(); sys.exit(0)
print(f"no matching /pout for {path}={val} within {tmo:.0f}s (saw: {[(p, round(v, 3)) for _, p, v in pool.pouts][-3:]})")
pool.stop(); sys.exit(1)
PY
}

report() {  # stage verdict detail
  printf '%-4s  %-8s %s\n' "$2" "$1" "$3"
  [ "$2" = PASS ] || FAILS=$((FAILS + 1))
}

run_stage() {  # stage path mode -- engine command is pre-built in ENGINE_CMD array
  local stage=$1 path=$2 mode=$3 t0 out rc
  t0=$(date +%s)
  start_engine "$stage" "${ENGINE_CMD[@]}"
  sleep 0.4
  out=$(echo_check "$path" "$EP" "$PP" "$mode"); rc=$?
  if [ $rc -eq 0 ]; then
    report "$stage" PASS "$out ($(( $(date +%s) - t0 ))s)"
  elif kill -0 "$ENG_PID" 2>/dev/null; then
    report "$stage" HANG "engine alive, $out"
    tail -n 5 "$LOGDIR/$stage.log" | sed 's/^/        | /'
  else
    report "$stage" FAIL "engine exited early; $out"
    tail -n 8 "$LOGDIR/$stage.log" | sed 's/^/        | /'
  fi
  killgroup
}

need() {  # stage tool...
  local stage=$1; shift
  for t in "$@"; do
    command -v "$t" >/dev/null 2>&1 || { report "$stage" FAIL "tool missing: $t"; return 1; }
  done
}

i=0
for stage in "${STAGES[@]}"; do
  EP=$((PORT_BASE + i)); PP=$((PORT_BASE + 50 + i)); i=$((i + 1))
  # the env contract the launcher sets (docs/parameters.md 11.3)
  export DEMIURGE_RESOURCES="$RES" DEMIURGE_PARAM_PORT=$EP DEMIURGE_POOL_HOST=127.0.0.1 DEMIURGE_POOL_PORT=$PP
  export DEMIURGE_STAGE="$stage"
  unset DEMIURGE_NO_AUDIO
  case "$stage" in
    strudel)
      need strudel node || continue
      [ -d "$STRUDEL_HOME/node_modules/@strudel/core" ] || { report strudel FAIL "no Strudel install at $STRUDEL_HOME"; continue; }
      W="$LOGDIR/strudel"; mkdir -p "$W"; ln -sfn "$STRUDEL_HOME/node_modules" "$W/node_modules"
      # The runner's evaluate path (core + mini + transpiler) with a no-op output: no audio
      # context, no ALSA MIDI. Queries a cycle every 100 ms so the pattern's density signal
      # (which calls the hook's get/out) runs. @strudel/tonal is skipped: not needed here.
      cat > "$W/harness.mjs" <<'JS'
import fs from 'node:fs';
import { performance } from 'node:perf_hooks';
const core = await import('@strudel/core');
const mini = await import('@strudel/mini');
const transpiler = await import('@strudel/transpiler');
await core.evalScope(core, mini);
mini.miniAllStrings();
const t0 = performance.now();
const { evaluate, scheduler } = core.repl({
  defaultOutput: () => {}, getTime: () => (performance.now() - t0) / 1000, transpiler: transpiler.transpiler });
await evaluate(fs.readFileSync(process.argv[2], 'utf8'));
let c = 0;
setInterval(() => { try { scheduler.pattern.queryArc(c, c + 1); c++; } catch (e) { console.error('query', e.message); } }, 100);
JS
      ENGINE_CMD=(node "$W/harness.mjs" "$HERE/strudel_drive.strudel")
      run_stage strudel /strudel/density p ;;
    chuck)
      need chuck chuck || continue
      # hook file first (class must exist), like the launcher's pre_args
      ENGINE_CMD=(chuck --silent "$RES/params/chuck/DemiurgeParams.ck" "$HERE/chuck_synth.ck")
      run_stage chuck /chuck/chorus p ;;
    pd)
      need pd pd || continue
      # options before the patch (Pd treats everything after it as files)
      ENGINE_CMD=(pd -nogui -nosound -nomidi -noprefs -stderr -path "$RES/params/pd"
                  -send "demiurge-param-port $EP; demiurge-pool-port $PP" "$HERE/pd_gate.pd")
      run_stage pd /pd/vibrato p ;;
    sc)
      need sc sclang || continue
      : > "$LOGDIR/empty.yaml"   # empty language config: skip ~/.config startup.scd
      export DEMIURGE_NO_AUDIO=1 QT_QPA_PLATFORM=offscreen
      ENGINE_CMD=(sclang -D -l "$LOGDIR/empty.yaml" "$HERE/sc_grain.scd")
      run_stage sc /sc/density p ;;
    faust)
      need faust faust g++ || continue
      ls /usr/include/faust/gui/OSCUI.h /usr/include/lo/lo.h >/dev/null 2>&1 \
        || { report faust FAIL "need faust OSC + liblo headers (libfaust-dev, liblo-dev)"; continue; }
      W="$LOGDIR/faust"; mkdir -p "$W"
      # Two-way Faust is only possible through the reference arch (the stock faust2jackconsole
      # binary cannot say /pout; see README "Known gaps"). It has no audio I/O.
      # (the arch feeds zeroed input buffers, so effects like faust_delay are fine)
      cp "$RES/params/faust/faust_pool_arch.cpp" "$W/arch.cpp"
      if ! ( cd "$W" && faust -a "$W/arch.cpp" "$HERE/faust_delay.dsp" -o f.cpp \
             && nice -n 19 timeout 300 g++ -O1 -std=c++17 f.cpp -o f -lOSCFaust -llo -lpthread ) >"$LOGDIR/faust-build.log" 2>&1; then
        report faust FAIL "arch build failed"; tail -n 8 "$LOGDIR/faust-build.log" | sed 's/^/        | /'; continue
      fi
      ENGINE_CMD=("$W/f")
      run_stage faust /faust/feedback faust ;;
    cpp)
      need cpp g++ || continue
      W="$LOGDIR/cpp"; mkdir -p "$W"   # build outside the examples dir
      if ! nice -n 19 timeout 300 g++ -O2 -o "$W/cpp_crush" "$HERE/cpp_crush.cpp" -ljack -lasound -lm -lpthread \
           >"$LOGDIR/cpp-build.log" 2>&1; then
        report cpp FAIL "build failed (libjack-dev / libasound2-dev?)"; tail -n 8 "$LOGDIR/cpp-build.log" | sed 's/^/        | /'; continue
      fi
      export DEMIURGE_NO_AUDIO=1
      ENGINE_CMD=("$W/cpp_crush")
      run_stage cpp /cpp/crush p ;;
    csound)
      need csound csound || continue
      # the launcher passes -+ignore_csopts=1 and the omacros after the file; null rtaudio here
      ENGINE_CMD=(csound -d -+ignore_csopts=1 -+rtaudio=null -o dac "$HERE/csound_ringmod.csd"
                  "--omacro:DEMIURGE_PARAM_PORT=$EP" "--omacro:DEMIURGE_POOL_PORT=$PP" "--env:INCDIR=$RES")
      run_stage csound /csound/reverb p ;;
    softmap)   # optional extra (not in the default seven): ../hello_softmap/softmap.csd
      need softmap csound || continue
      ENGINE_CMD=(csound -d -+ignore_csopts=1 -+rtaudio=null -o dac "$HERE/../hello_softmap/softmap.csd"
                  "--omacro:DEMIURGE_PARAM_PORT=$EP" "--omacro:DEMIURGE_POOL_PORT=$PP" "--env:INCDIR=$RES")
      run_stage softmap /csound/cutoff p ;;
  esac
done

echo
if [ "$FAILS" = 0 ]; then
  echo "hello_patching check: ALL ${#STAGES[@]} PASS"; exit 0
fi
echo "hello_patching check: $FAILS of ${#STAGES[@]} did not pass (logs: $LOGDIR)"; KEEP=1; exit 1

#!/usr/bin/env bash
# testing/stock-check/run.sh -- does every language's parameter hook work on THIS machine?
# Runnable over ssh on a stock Pi (no Claude agent, maybe no USB audio, HDMI only).
#
#   bash run.sh                       all languages
#   bash run.sh --only csound         one language (csound chuck pd supercollider faust strudel cardinal)
#                                     or --only hello_csound|hello_chuck|hello_pd|hello_sc|hello_faust|hello_strudel|hello_patching
#                                     or --only uss | uss_nonlinear_daylight | uss_supersaw | uss_lockin
#   bash run.sh --timeout 30          per-test wall-clock limit in seconds (default 20)
#   bash run.sh --repo /path/to/repo  repo root (default: two dirs above this script)
#   bash run.sh --port-base 19300     tests use UDP ports base+0..base+19 (default 19200; keep within 19000-19999)
#
# Per language: engine present? -> run resources/params/<lang>/selftest.sh, which starts the engine
# SILENT/OFFLINE, sends /p to it through resources/params/testpool.py and requires the engine to answer /pout.
#   PASS  engine echoed the value        FAIL  error / no echo        HANG  per-test timeout exceeded
#   SKIP  engine not installed, or not built yet (reason shown)        MISSING  (examples) file not found
# Then (full run, or --only hello_<lang>): hello_<lang>/hello_nonlinear_daylight.* is started silent the way the launcher
# starts it and driven through the REAL demiurge-io pool objects (pool_e2e.py): /p x3 in, /pout x3 back, pool records them;
# and hello_patching/check.sh runs its seven stages. Faust uses the reference arch (needs g++ + OSC libs, else SKIP).
# Never opens an audio device, never restarts a service, only UDP ports 19000-19999 (never the pool's 9102).
# Exit 0 iff no FAIL and no HANG. Results: table on stdout + results.tsv (kind name result seconds detail).
set -u
export PYTHONDONTWRITEBYTECODE=1
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
TIMEOUT=20; ONLY=""; BASE=19200; OUT="$HERE/results.tsv"
while [ $# -gt 0 ]; do
  case "$1" in
    --only) ONLY="$2"; shift 2;;
    --timeout) TIMEOUT="$2"; shift 2;;
    --repo) REPO="$(cd "$2" && pwd)"; shift 2;;
    --port-base) BASE="$2"; shift 2;;
    --out) OUT="$2"; shift 2;;
    -h|--help) sed -n 2,22p "$0"; exit 0;;
    *) echo "unknown arg: $1 (see --help)" >&2; exit 2;;
  esac
done
case "$TIMEOUT$BASE" in *[!0-9]*) echo "--timeout and --port-base must be integers" >&2; exit 2;; esac
[ "$BASE" -ge 19000 ] && [ "$BASE" -le 19980 ] || { echo "--port-base must be 19000..19980" >&2; exit 2; }
P="$REPO/resources/params"
[ -d "$P" ] || { echo "no $P (use --repo)" >&2; exit 2; }
command -v python3 >/dev/null || { echo "python3 is required (testpool.py)" >&2; exit 2; }
INNER=$((TIMEOUT - 8)); [ $INNER -lt 3 ] && INNER=3

# lang:engine-binary ("-" = nothing to run yet)
LANGS="csound:csound chuck:chuck pd:pd supercollider:sclang faust:faust strudel:node cardinal:-"
rows=(); fails=0; idx=0
add() { rows+=("$1"$'\t'"$2"$'\t'"$3"$'\t'"$4"$'\t'"$5"); }

run_one() {   # lang -> sets RES SECS DETAIL
  local lang="$1" bin="$2" t0 out rc pid dl
  RES=""; DETAIL=""; SECS=0
  [ -x "$P/$lang/selftest.sh" ] || [ -f "$P/$lang/selftest.sh" ] || { RES=FAIL; DETAIL="no selftest.sh"; return; }
  if [ "$bin" != "-" ] && ! command -v "$bin" >/dev/null 2>&1; then RES=SKIP; DETAIL="$bin not installed"; return; fi
  out=$(mktemp); t0=$(date +%s)
  DEMIURGE_ST_TIMEOUT=$INNER setsid bash "$P/$lang/selftest.sh" $((BASE + idx)) $((BASE + 100 + idx)) >"$out" 2>&1 </dev/null &
  pid=$!; dl=$((t0 + TIMEOUT))
  while kill -0 $pid 2>/dev/null; do
    if [ "$(date +%s)" -ge $dl ]; then
      kill -TERM -$pid 2>/dev/null; sleep 1; kill -KILL -$pid 2>/dev/null
      wait $pid 2>/dev/null; SECS=$(( $(date +%s) - t0 ))
      RES=HANG; DETAIL="exceeded ${TIMEOUT}s; last: $(tail -n1 "$out" | cut -c1-120)"; rm -f "$out"; return
    fi
    sleep 0.2
  done
  wait $pid; rc=$?; SECS=$(( $(date +%s) - t0 ))
  kill -TERM -$pid 2>/dev/null     # stray children of a finished test, if any
  local first; first=$(grep -m1 -E '^(PASS|FAIL|SKIP)' "$out" | cut -c1-160)
  [ -z "$first" ] && first=$(tail -n1 "$out" | cut -c1-160)
  case $rc in
    0)  RES=PASS; DETAIL="${first#PASS: }";;
    77) RES=SKIP; DETAIL="${first#SKIP: }";;
    *)  RES=FAIL; DETAIL="rc=$rc ${first#FAIL: }";;
  esac
  rm -f "$out"
}

for pair in $LANGS; do
  lang="${pair%%:*}"; bin="${pair#*:}"
  if [ -n "$ONLY" ] && [ "$ONLY" != "$lang" ]; then idx=$((idx+1)); continue; fi
  run_one "$lang" "$bin"
  add lang "$lang" "$RES" "$SECS" "$DETAIL"
  case "$RES" in FAIL|HANG) fails=$((fails+1));; esac
  idx=$((idx+1))
done

# ---- hello_<lang> nonlinear daylight: a real /p -> /pout round trip through the REAL pool objects ----
# (pool_e2e.py = demiurge_io ParamRegistry + OscSink + Pool) against the example file, started the way
# the launcher starts it (argv/env from src/demiurge-launcher-rs/src/params.rs), silent/offline.
EX=""
for d in "$REPO/demiurge/examples" "$HOME/demiurge/examples" /opt/demiurge/examples; do [ -d "$d" ] && { EX="$d"; break; }; done
SRC="$REPO/src/demiurge-io"
STRUDEL_HOME="${STRUDEL_HOME:-$HOME/demiurge/strudel}"

hello_one() {   # lang -> sets RES SECS DETAIL
  local lang="$1" ep="$2" pp="$3" dir file stage W out t0 rc pid dl first
  RES=""; DETAIL=""; SECS=0
  case "$lang" in
    csound) dir=hello_csound; file=hello_nonlinear_daylight.csd;;
    chuck) dir=hello_chuck; file=hello_nonlinear_daylight.ck;;
    pd) dir=hello_pd; file=hello_nonlinear_daylight.pd;;
    sc) dir=hello_supercollider; file=hello_nonlinear_daylight.scd;;
    faust) dir=hello_faust; file=hello_nonlinear_daylight.dsp;;
    strudel) dir=hello_strudel; file=hello_nonlinear_daylight.strudel;;
  esac
  stage="nld-$lang"
  if [ -z "$EX" ] || [ ! -f "$EX/$dir/$file" ]; then RES=MISSING; DETAIL="$dir/$file not found"; return; fi
  [ -d "$SRC/demiurge_io" ] || { RES=SKIP; DETAIL="no $SRC (demiurge_io needed for the pool objects)"; return; }
  local bin; case "$lang" in sc) bin=sclang;; strudel) bin=node;; faust) bin=faust;; *) bin=$lang;; esac
  command -v "$bin" >/dev/null 2>&1 || { RES=SKIP; DETAIL="$bin not installed"; return; }
  W=$(mktemp -d); out="$W/out.log"; t0=$(date +%s)
  local -a CMD; local extra=()
  export DEMIURGE_RESOURCES="$REPO/resources" DEMIURGE_PARAM_PORT=$ep DEMIURGE_POOL_HOST=127.0.0.1 \
         DEMIURGE_POOL_PORT=$pp DEMIURGE_STAGE="$stage" DEMIURGE_STAGE_INDEX=0
  unset DEMIURGE_NO_AUDIO
  local R="$REPO/resources" F="$EX/$dir/$file"
  case "$lang" in
    csound) CMD=(csound -d -+ignore_csopts=1 -+rtaudio=null -o dac "$F"
                 "--omacro:DEMIURGE_PARAM_PORT=$ep" "--omacro:DEMIURGE_POOL_PORT=$pp" "--env:INCDIR=$R");;
    chuck) CMD=(chuck --silent "$R/params/chuck/DemiurgeParams.ck" "$F");;
    pd) CMD=(pd -nogui -nosound -nomidi -noprefs -stderr -path "$R/params/pd"
             -send "demiurge-param-port $ep; demiurge-pool-port $pp" "$F");;
    sc) : > "$W/empty.yaml"; export DEMIURGE_NO_AUDIO=1 QT_QPA_PLATFORM=offscreen
        CMD=(sclang -D -l "$W/empty.yaml" "$F");;
    faust)
      command -v g++ >/dev/null && ls /usr/include/faust/gui/OSCUI.h /usr/include/lo/lo.h >/dev/null 2>&1 \
        || { RES=SKIP; DETAIL="no g++ / faust OSC / liblo headers (reference arch needs them)"; rm -rf "$W"; return; }
      # The stock faust2jackconsole binary cannot send /pout; the reference arch (no audio I/O) can.
      if ! ( cd "$W" && faust -a "$R/params/faust/faust_pool_arch.cpp" "$F" -o f.cpp \
             && nice -n 19 timeout 240 g++ -O1 -std=c++17 f.cpp -o f -lOSCFaust -llo -lpthread ) >"$W/build.log" 2>&1; then
        RES=FAIL; DETAIL="arch build failed: $(tail -n1 "$W/build.log" | cut -c1-120)"; SECS=$(( $(date +%s) - t0 )); rm -rf "$W"; return
      fi
      CMD=("$W/f"); extra=(--address-prefix /nld); t0=$(date +%s);;
    strudel)
      [ -d "$STRUDEL_HOME/node_modules/@strudel/core" ] || { RES=SKIP; DETAIL="no Strudel install at $STRUDEL_HOME"; rm -rf "$W"; return; }
      mkdir -p "$W/s"; cp "$HERE/strudel_harness.mjs" "$W/s/"; ln -sfn "$STRUDEL_HOME/node_modules" "$W/s/node_modules"
      CMD=(node "$W/s/strudel_harness.mjs" "$F");;
  esac
  setsid nice -n 19 timeout -k 1 $((TIMEOUT + 5)) "${CMD[@]}" >"$W/engine.log" 2>&1 </dev/null &
  pid=$!
  timeout -k 1 "$TIMEOUT" python3 "$HERE/pool_e2e.py" --src "$SRC" --stage "$stage" --engine-port "$ep" --pool-port "$pp" \
      --path "/$stage/density" --path "/$stage/tone" --path "/$stage/length" --value 0.6 \
      --timeout "$INNER" ${extra[@]+"${extra[@]}"} >"$out" 2>&1 </dev/null
  rc=$?; SECS=$(( $(date +%s) - t0 ))
  kill -TERM -$pid 2>/dev/null; sleep 0.3; kill -KILL -$pid 2>/dev/null; wait $pid 2>/dev/null
  first=$(grep -m1 -E '^(PASS|FAIL)' "$out" | cut -c1-200); [ -z "$first" ] && first=$(tail -n1 "$out" | cut -c1-160)
  case $rc in
    0) RES=PASS; DETAIL="${first#PASS: }";;
    124|137) RES=HANG; DETAIL="exceeded ${TIMEOUT}s";;
    *) RES=FAIL; DETAIL="${first#FAIL: } | engine: $(tail -n2 "$W/engine.log" | tr '\n' ' ' | cut -c1-140)";;
  esac
  rm -rf "$W"
}

if [ -z "$ONLY" ] || [ "${ONLY#hello_}" != "$ONLY" ]; then
  hidx=0
  for n in csound chuck pd sc faust strudel; do
    if [ -n "$ONLY" ] && [ "$ONLY" != "hello_$n" ]; then hidx=$((hidx+1)); continue; fi
    hello_one "$n" $((BASE + 40 + hidx)) $((BASE + 140 + hidx))
    add hello "hello_$n" "$RES" "$SECS" "$DETAIL"
    case "$RES" in FAIL|HANG) fails=$((fails+1));; esac
    hidx=$((hidx+1))
  done
fi

# hello_patching/check.sh: the seven-stage two-way check (own ports 18400+, own silent engines)
if [ -z "$ONLY" ] || [ "$ONLY" = hello_patching ]; then
  CS=""; for d in "$REPO/demiurge/examples" "$HOME/demiurge/examples" /opt/demiurge/examples; do [ -f "$d/hello_patching/check.sh" ] && { CS="$d/hello_patching/check.sh"; break; }; done
  if [ -z "$CS" ]; then add hello hello_patching MISSING 0 "check.sh not found"
  else
    out=$(mktemp); t0=$(date +%s)
    setsid env DEMIURGE_RESOURCES="$REPO/resources" bash "$CS" --timeout "$INNER" >"$out" 2>&1 </dev/null &
    pid=$!; dl=$((t0 + TIMEOUT * 15))
    while kill -0 $pid 2>/dev/null; do
      if [ "$(date +%s)" -ge $dl ]; then kill -TERM -$pid 2>/dev/null; sleep 1; kill -KILL -$pid 2>/dev/null; break; fi
      sleep 0.5
    done
    wait $pid 2>/dev/null; rc=$?; SECS=$(( $(date +%s) - t0 ))
    last=$(grep -E 'hello_patching check' "$out" | tail -n1 | cut -c1-160)
    if [ "$(date +%s)" -ge $dl ]; then RES=HANG; last="exceeded $((TIMEOUT * 15))s"
    elif [ $rc -eq 0 ]; then RES=PASS
    elif grep -qE '^(FAIL|HANG)' "$out"; then RES=FAIL; last="$last | $(grep -E '^(FAIL|HANG)' "$out" | head -n3 | cut -c1-80 | tr '\n' ';')"
    else RES=FAIL; last="rc=$rc $(tail -n1 "$out" | cut -c1-120)"; fi
    add hello hello_patching "$RES" "$SECS" "$last"
    case "$RES" in FAIL|HANG) fails=$((fails+1));; esac
    rm -f "$out"
  fi
fi

# ---- USS instruments (uss/check.sh): manifest + offline render (non-silent, no clipping) + /p->/pout round trip
# Rows uss_<instrument>. --only uss runs just these; --only uss_<name> runs one. Own ports 18300+ (check.sh default; never 9102).
if [ -z "$ONLY" ] || [ "${ONLY%%_*}" = uss ]; then
  UC="$REPO/uss/check.sh"
  if [ ! -f "$UC" ]; then add uss uss MISSING 0 "uss/check.sh not found"
  else
    uargs=(); [ -n "$ONLY" ] && [ "$ONLY" != uss ] && uargs=(--only "${ONLY#uss_}")
    while IFS=$'\t' read -r un ures us ud; do
      [ -z "$un" ] && continue
      add uss "$un" "$ures" "$us" "$ud"
      case "$ures" in FAIL|HANG) fails=$((fails+1));; esac
    done < <(setsid bash "$UC" --tsv --repo "$REPO" --timeout $((TIMEOUT * 4)) "${uargs[@]+"${uargs[@]}"}" 2>/dev/null </dev/null)
  fi
fi

printf '%s\n' "kind	name	result	seconds	detail" > "$OUT"
for r in "${rows[@]}"; do printf '%s\n' "$r" >> "$OUT"; done
echo "demiurge stock-check  host=$(hostname)  $(date '+%F %T')  timeout=${TIMEOUT}s  repo=$REPO"
printf '%-9s %-20s %-8s %5s  %s\n' KIND NAME RESULT SECS DETAIL
for r in "${rows[@]}"; do IFS=$'\t' read -r k n res s d <<<"$r"; printf '%-9s %-20s %-8s %5s  %s\n' "$k" "$n" "$res" "$s" "$d"; done
echo "results: $OUT"
if [ $fails -eq 0 ]; then echo "OVERALL: OK (no FAIL/HANG)"; exit 0; else echo "OVERALL: $fails FAIL/HANG"; exit 1; fi

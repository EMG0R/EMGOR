#!/usr/bin/env bash
# uss/check.sh -- is every USS instrument playable on THIS machine, offline and silent?
#
#   bash check.sh                     all instruments
#   bash check.sh --only supersaw     one (nonlinear_daylight supersaw lockin)
#   bash check.sh --timeout 90        per-stage wall-clock limit, seconds (default 90)
#   bash check.sh --port-base 18300   UDP ports base+i (engine) and base+100+i (fake pool); keep within 18000-18999
#   bash check.sh --tsv               machine-readable: name<TAB>result<TAB>secs<TAB>detail (used by stock-check/run.sh)
#   bash check.sh --repo DIR          repo root (default: parent of this dir)
#
# Per instrument, three stages; the row is PASS only if all pass:
#   manifest  params/<stage>.json loads into the REAL demiurge_io ParamRegistry: 7 common params, all hard,
#             7 hard_maps, every hard-map source is a known id, NO encoder (e1-7, eb1-7) is ever a source
#   syntax+render  the engine runs OFFLINE (csound -+rtaudio=null -o file.wav / chuck --silent + rec.ck, a
#             WvOut tap on dac) for N seconds: no compile error, and the wav is non-silent (RMS > 0.01),
#             peak <= 1.0, and < 0.001% of samples at full scale
#   roundtrip /p -> /pout through the real pool objects (testing/stock-check/pool_e2e.py) on all 7 params
# FAIL = a stage failed   HANG = timeout   SKIP = engine not installed (rc 0)
# Never opens an audio device, never touches JACK/PipeWire, never restarts a service; UDP 18000-18999 only.
set -u
export PYTHONDONTWRITEBYTECODE=1
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"; TIMEOUT=90; ONLY=""; BASE=18300; TSV=0
while [ $# -gt 0 ]; do case "$1" in
  --only) ONLY="$2"; shift 2;; --timeout) TIMEOUT="$2"; shift 2;; --port-base) BASE="$2"; shift 2;;
  --tsv) TSV=1; shift;; --repo) REPO="$(cd "$2" && pwd)"; shift 2;;
  -h|--help) sed -n 2,22p "$0"; exit 0;; *) echo "unknown arg $1" >&2; exit 2;; esac; done
[ "$BASE" -ge 18000 ] && [ "$BASE" -le 18890 ] || { echo "--port-base must be 18000..18890" >&2; exit 2; }
U="$REPO/uss"; R="$REPO/resources"; SRC="$REPO/src/demiurge-io"; E2E="$REPO/testing/stock-check/pool_e2e.py"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
# name:engine:file:render-seconds
INSTS="nonlinear_daylight:csound:nonlinear_daylight.csd:40 supersaw:chuck:supersaw.ck:45 lockin:chuck:lockin.ck:30"
fails=0; idx=0; rows=()

engine_cmd() {   # kind file ep pp mode(render|live) out secs -> sets CMD array
  local kind="$1" f="$2" ep="$3" pp="$4" mode="$5" out="$6" secs="$7"
  if [ "$kind" = csound ]; then
    local c="$W/run_$ep.csd"; sed "s/86400/$secs/g" "$f" > "$c"      # finite score for offline renders
    if [ "$mode" = render ]; then
      CMD=(csound -d -+ignore_csopts=1 -+rtaudio=null -W -o "$out" "--omacro:DEMIURGE_PARAM_PORT=$ep" "--omacro:DEMIURGE_POOL_PORT=$pp" "--omacro:USS_SEED=7" "--env:INCDIR=$R" "$c")
    else   # real-time pacing on the null backend: opens no device
      CMD=(csound -d -+ignore_csopts=1 -+rtaudio=null -o dac "--omacro:DEMIURGE_PARAM_PORT=$ep" "--omacro:DEMIURGE_POOL_PORT=$pp" "--env:INCDIR=$R" "$f")
    fi
  else
    if [ "$mode" = render ]; then
      CMD=(chuck --silent "$R/params/chuck/DemiurgeParams.ck" "$f" "$U/rec.ck:$secs:$out")
    else CMD=(chuck --silent "$R/params/chuck/DemiurgeParams.ck" "$f"); fi
  fi
}

run_inst() {   # name kind file secs -> RES DETAIL
  local name="$1" kind="$2" file="$3" secs="$4" d="$U/instruments/$1" ep=$((BASE + idx)) pp=$((BASE + 100 + idx)) st="uss-$1"
  RES=PASS; DETAIL=""
  command -v "$kind" >/dev/null 2>&1 || { RES=SKIP; DETAIL="$kind not installed"; return; }
  # --- manifest
  local m; m=$(python3 -I - "$SRC" "$d/params/$st.json" "$d/$file" <<'PY' 2>&1
import sys, json, re
sys.dont_write_bytecode = True
sys.path.insert(0, sys.argv[1])
from demiurge_io.pool import ParamRegistry
doc = json.load(open(sys.argv[2])); eng = open(sys.argv[3]).read()
names = ["density", "tone", "length", "space", "intensity", "pitch", "trig"]
reg = ParamRegistry("/nonexistent")
n = reg.load_manifest(doc)
st = doc["stage"]
bad = []
if n != 7: bad.append("loaded %d/7 params" % n)
for nm in names:
    p = "/%s/%s" % (st, nm)
    if not reg.is_hard(p): bad.append("%s not hard" % p)
    if p not in eng and not (('"%s/"' % ("/" + st)) in eng and ('"%s"' % nm) in eng): bad.append("%s not in engine file" % p)
hm = reg.hard_maps()
if len(hm) != 7: bad.append("%d hard_maps (want 7)" % len(hm))
enc = re.compile(r"/(e|eb)[1-7]$")
for m in hm:
    if enc.search(m["source"]) or re.search(r"/hw/midi/1/(2[0-6]|7[0-6])$", m["source"]): bad.append("encoder source %s" % m["source"])
    if m["target"]["param"] not in ["/%s/%s" % (st, x) for x in names]: bad.append("map target %s" % m["target"]["param"])
print("; ".join(bad) if bad else "ok"); sys.exit(1 if bad else 0)
PY
  ) || { RES=FAIL; DETAIL="manifest: $m"; return; }
  # --- syntax + offline render
  local wav="$W/$name.wav" log="$W/$name.log" rc
  engine_cmd "$kind" "$d/$file" "$ep" "$pp" render "$wav" "$secs"
  ( export DEMIURGE_PARAM_PORT=$ep DEMIURGE_POOL_PORT=$pp USS_NO_MIDI=1
    setsid nice -n 19 timeout -k 2 "$TIMEOUT" "${CMD[@]}" >"$log" 2>&1 </dev/null ); rc=$?
  if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then RES=HANG; DETAIL="render exceeded ${TIMEOUT}s"; return; fi
  if grep -qE "syntax error|^error|Parsing failed|cannot compile|used before defined|not declared|^Csound.*failed|\\[chuck\\].*error|^.*\\.ck:line" "$log" || [ ! -s "$wav" ]; then
    RES=FAIL; DETAIL="compile/render error: $(grep -E 'error|undefined|not declared|line' "$log" | head -n2 | tr '\n' ' ' | cut -c1-140)"; return; fi
  local ws; ws=$(python3 -I "$U/wavstat.py" "$wav") || { RES=FAIL; DETAIL="render audio check failed: $ws"; return; }
  # --- round trip /p -> /pout (real pool objects)
  [ -f "$E2E" ] || { RES=FAIL; DETAIL="no $E2E"; return; }
  engine_cmd "$kind" "$d/$file" "$ep" "$pp" live "" 0
  ( export DEMIURGE_PARAM_PORT=$ep DEMIURGE_POOL_HOST=127.0.0.1 DEMIURGE_POOL_PORT=$pp DEMIURGE_STAGE=$st USS_NO_MIDI=1
    setsid nice -n 19 timeout -k 1 $((TIMEOUT + 5)) "${CMD[@]}" >"$W/$name.live.log" 2>&1 </dev/null ) &
  local pid=$!
  local pa=(); for x in density tone length space intensity pitch trig; do pa+=(--path "/$st/$x"); done
  local out; out=$(timeout -k 1 "$TIMEOUT" python3 "$E2E" --src "$SRC" --stage "$st" --engine-port "$ep" --pool-port "$pp" "${pa[@]}" --value 0.6 --timeout $((TIMEOUT - 10 > 8 ? 20 : 8)) 2>&1); rc=$?
  { kill -TERM -$pid; pkill -TERM -P $pid; sleep 0.3; pkill -KILL -P $pid; wait $pid; } 2>/dev/null
  if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then RES=HANG; DETAIL="roundtrip exceeded ${TIMEOUT}s"; return; fi
  [ $rc -eq 0 ] || { RES=FAIL; DETAIL="roundtrip: $(echo "$out" | head -n1 | cut -c1-120)"; return; }
  DETAIL="render ${secs}s $ws; roundtrip 7/7"
}

for item in $INSTS; do
  IFS=: read -r name kind file secs <<<"$item"
  if [ -n "$ONLY" ] && [ "$ONLY" != "$name" ]; then idx=$((idx+1)); continue; fi
  t0=$(date +%s); run_inst "$name" "$kind" "$file" "$secs"; s=$(( $(date +%s) - t0 ))
  rows+=("uss_$name"$'\t'"$RES"$'\t'"$s"$'\t'"$DETAIL")
  case "$RES" in FAIL|HANG) fails=$((fails+1));; esac
  idx=$((idx+1))
done
if [ $TSV -eq 1 ]; then printf '%s\n' "${rows[@]}"
else
  printf '%-22s %-6s %5s  %s\n' NAME RESULT SECS DETAIL
  for r in "${rows[@]}"; do IFS=$'\t' read -r n res s d <<<"$r"; printf '%-22s %-6s %5s  %s\n' "$n" "$res" "$s" "$d"; done
  [ $fails -eq 0 ] && echo "OVERALL: OK" || echo "OVERALL: $fails FAIL/HANG"
fi
[ $fails -eq 0 ]

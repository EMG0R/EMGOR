#!/usr/bin/env bash
# drumbench.sh -- measure the drum voice loop on the hardware (DWT cycles). Needs a firmware built with `make DRUMBENCH=1` (schema param DBENCH).
#   tools/drumbench.sh PORT sweep MODE [N]       every preset alone (n pads of its group), MODE 1 = original voice, 2 = synth engine  -> "mode sel name cyc/voice-sample"
#   tools/drumbench.sh PORT run MODE SEL N SECS  one setting: prints STATUS cpu / cpu_max / dcyc
# dcyc = max cycles per 64-sample block / cycles per voice-sample / blocks / voice-samples. Budget: 10000 cycles per sample at 480 MHz / 48 kHz (640000 per block).
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"; PORT="$1"; shift
case "$PORT" in *4488*) echo "refusing Bela port"; exit 2;; esac
CTL="$HERE/build_host/pmorctl"; ID=7044
cmd="$1"; shift
if [ "$cmd" = sweep ]; then
  MODE="$1"; N="${2:-1}"; args=("HELLO EDIT 1" "STATUS")
  for sel in $(seq 0 95); do args+=("SET_PARAM $ID $((MODE*1000000 + sel*100 + N))" "STATUS" "sleep 1.2" "STATUS"); done
  args+=("SET_PARAM $ID 0" "BYE")
  "$CTL" port "$PORT" script "${args[@]}" 2>&1 | python3 -I -c '
import re,sys
rows=[l for l in sys.stdin if "dcyc=" in l]
# rows: [first STATUS] then pairs (reset, measure)
rows=rows[1:]
mode=int(sys.argv[1])
for sel in range(96):
    m=re.search(r"dcyc=(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)", rows[sel*2+1]) if sel*2+1 < len(rows) else None
    c=re.search(r"cpu=(\d+) cpu_max=(\d+)", rows[sel*2+1]) if m else None
    print(mode, sel, m.group(2) if m else "?", "proc", m.group(5) if m else "?", "idleblk", m.group(6) if m else "?", "max_block", m.group(1) if m else "?", "cpu_max", c.group(2) if c else "?")
' "$MODE"
else
  MODE="$1"; SEL="$2"; N="$3"; SECS="${4:-4}"
  "$CTL" port "$PORT" script "HELLO EDIT 1" "STATUS" "SET_PARAM $ID $((MODE*1000000 + SEL*100 + N))" "STATUS" "sleep $SECS" "STATUS" "SET_PARAM $ID 0" "BYE" 2>&1 | grep -oE "cpu=[0-9]+ cpu_max=[0-9]+|dcyc=[0-9/]+|audio_overruns=[0-9]+" | tail -3
fi

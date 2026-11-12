#!/bin/bash
# run.sh -- permission-free Live dry run. Terminal only: no AppleScript, no screenshots, no accessibility/automation APIs.
#   ./run.sh            launch Live with dryrun.als, wait for the result, quit Live by signal
#   ./run.sh --dry      print every command, run nothing
#   ./run.sh --check    validate the kit's files (no Live, no Max)
# Exit: 0 result.ok true and the device ran inside Live | 1 checks failed | 2 refused (Live already running / files bad) | 3 timeout | 4 device ran but LiveAPI was absent
# Never run this while EMGOR's Live is open: it refuses (pgrep -x Live) and quits only a Live process it started itself.
DIR="$(cd "$(dirname "$0")" && pwd)"
LIVE_APP="Ableton Live 12 Suite"
SET="$DIR/dryrun.als"
AMXD="$DIR/PMOR dryrun.amxd"
RESULT=/tmp/pmor_dryrun_result.json
DONE=/tmp/pmor_dryrun_done
POLL_S=120      # wait for the done file
QUIT_S=20       # wait for Live to exit after SIGTERM
MODE=run
case "$1" in --dry) MODE=dry;; --check) MODE=check;; "") ;; *) echo "usage: run.sh [--dry|--check]"; exit 2;; esac

if [ "$MODE" = check ]; then
  rc=0
  for f in "$SET" "$AMXD" "$DIR/pmor_dryrun.js" "$DIR/check.py"; do [ -f "$f" ] || { echo "FAIL missing $f"; rc=1; }; done
  python3 "$DIR/check.py" amxd "$AMXD" || rc=1
  python3 "$DIR/check.py" als "$SET" "$AMXD" || rc=1
  python3 "$DIR/check.py" sample || rc=1
  if command -v xmllint >/dev/null; then gunzip -c "$SET" | xmllint --noout - && echo "xmllint ok" || rc=1; fi
  if [ -f "$RESULT" ]; then echo "existing $RESULT:"; python3 "$DIR/check.py" result "$RESULT"; fi
  [ -d "/Applications/$LIVE_APP.app" ] && echo "Live app present" || { echo "FAIL /Applications/$LIVE_APP.app missing"; rc=1; }
  pgrep -x Live >/dev/null && echo "note: Live is running now (a real run would refuse)"
  [ $rc -eq 0 ] && echo "CHECK OK" || echo "CHECK FAILED"
  exit $rc
fi

# run <cmd...>: print it; execute unless --dry
run() { echo "+ $*"; [ "$MODE" = dry ] && return 0; "$@"; }

if [ "$MODE" = run ]; then
  if pgrep -x Live >/dev/null; then echo "REFUSED: Live is already running (pid $(pgrep -x Live | tr '\n' ' ')). Not touching it."; exit 2; fi
  python3 "$DIR/check.py" als "$SET" "$AMXD" >/dev/null && python3 "$DIR/check.py" amxd "$AMXD" >/dev/null || { echo "REFUSED: kit files invalid, run ./run.sh --check"; exit 2; }
else
  echo "+ pgrep -x Live    # refuse to start if Live is already running"
fi

BEFORE="$(pgrep -x Live | sort | tr '\n' ' ')"
run rm -f "$RESULT" "$DONE"
run open -a "$LIVE_APP" "$SET"
if [ "$MODE" = dry ]; then
  echo "+ poll for $DONE up to ${POLL_S}s (1 s steps; also stop early if the started Live exits)"
  echo "+ python3 -m json.tool $RESULT"
  echo "+ kill -TERM <pids of Live that did not exist before>   # then wait up to ${QUIT_S}s"
  echo "+ kill -KILL <same pids>   # only if still alive after ${QUIT_S}s"
  echo "+ python3 $DIR/check.py result $RESULT --require-live   # exit code"
  exit 0
fi

started() { comm -13 <(printf '%s\n' $BEFORE | sort) <(pgrep -x Live | sort); }
quit_live() {
  local pids; pids="$(started)"
  [ -z "$pids" ] && return 0
  for p in $pids; do echo "+ kill -TERM $p"; kill -TERM "$p" 2>/dev/null; done
  for i in $(seq 1 $QUIT_S); do sleep 1; [ -z "$(started)" ] && { echo "Live exited after SIGTERM (${i}s)"; return 0; }; done
  for p in $(started); do echo "Live ignored SIGTERM for ${QUIT_S}s (save sheet or hang?): + kill -KILL $p (the pid this script started)"; kill -KILL "$p" 2>/dev/null; done
  sleep 2
}

t=0; got=0
while [ $t -lt $POLL_S ]; do
  [ -f "$DONE" ] && { got=1; break; }
  [ $t -gt 15 ] && [ -z "$(started)" ] && { echo "Live exited before the device reported"; break; }
  sleep 1; t=$((t+1))
done

if [ $got -eq 1 ]; then
  echo "device reported after ~${t}s"
  python3 -m json.tool "$RESULT"
  python3 "$DIR/check.py" result "$RESULT" --require-live; rc=$?
else
  echo "TIMEOUT: no $DONE after ${t}s"; rc=3
fi
quit_live
exit $rc

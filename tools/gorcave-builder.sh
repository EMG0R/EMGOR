#!/usr/bin/env bash
# gorcave-builder.sh — unattended backlog loop on gorcave. One `claude -p` run per queue item; stops on STOP file, empty queue, low disk, or 3 consecutive failures.
# Start:  nohup ~/EMGOR/tools/gorcave-builder.sh >> ~/nmg-builder/runner.log 2>&1 &
# Stop:   touch ~/nmg-builder/STOP        Status: tail ~/nmg-builder/runner.log ; git -C ~/EMGOR log --oneline -5 gorcave-backlog
set -u
REPO=/home/server/EMGOR; ST=/home/server/nmg-builder; mkdir -p "$ST"; cd "$REPO" || exit 1
MODEL=${NMG_MODEL:-sonnet}; MAXRUNS=${NMG_MAXRUNS:-40}; fails=0; n=0
log(){ echo "[$(date -u +%FT%TZ)] $*"; }
while [ $n -lt $MAXRUNS ]; do
  [ -f "$ST/STOP" ] && { log "STOP file present, exiting"; exit 0; }
  avail=$(df --output=avail -m / | tail -1 | tr -d ' ')
  if [ "$avail" -lt 700 ]; then git gc --prune=now -q; avail=$(df --output=avail -m / | tail -1 | tr -d ' '); [ "$avail" -lt 700 ] && { log "disk ${avail}MB < 700MB, exiting"; exit 2; }; fi
  git fetch -q origin && git checkout -q gorcave-backlog && git pull -q --ff-only origin gorcave-backlog
  grep -q '^- \[ \]' docs/GORCAVE-QUEUE.md || { log "queue empty, exiting"; exit 0; }
  item=$(grep -m1 '^- \[ \]' docs/GORCAVE-QUEUE.md | cut -c7-60)
  n=$((n+1)); log "run $n: $item (disk ${avail}MB)"
  timeout 3h claude -p --model "$MODEL" --dangerously-skip-permissions --max-turns 400 \
    "Read docs/GORCAVE-BUILD-HANDOFF.md fully and follow it exactly. Do ONE queue item: the first '- [ ]' in docs/GORCAVE-QUEUE.md. Mark it in progress, do it, log it, mark it done or blocked, commit, push origin gorcave-backlog, then stop." \
    > "$ST/run-$(date -u +%Y%m%dT%H%M%SZ).log" 2>&1
  rc=$?
  git add -A && git commit -qm "builder: autosave after run $n (rc=$rc)" 2>/dev/null && git push -q origin gorcave-backlog 2>/dev/null
  if [ $rc -ne 0 ]; then fails=$((fails+1)); log "run $n rc=$rc (fails=$fails)"; [ $fails -ge 3 ] && { log "3 consecutive failures, exiting"; exit 3; }; else fails=0; log "run $n ok"; fi
  sleep 20
done
log "max runs reached"

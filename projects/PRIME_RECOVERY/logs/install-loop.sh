#!/bin/bash
# Re-run the idempotent installer until it reaches the Phase 8 prompt (or finishes).
# The prompt is interactive, so a run sitting at it never exits and the loop waits.
for i in $(seq 1 30); do
  L=$HOME/PRIME_RECOVERY/logs/bootstrap-$(date +%Y%m%d-%H%M%S).log
  echo $L > $HOME/PRIME_RECOVERY/logs/CURRENT
  echo "== loop run $i -> $L"
  ( cd ~ && DEVICE=gorpi7v5 ROLE=instrument HARDWARE=pi5-16gb REPO_DIR=$HOME/demiurge bash ~/demiurge/bootstrap.sh 2>&1 | tee $L; exit ${PIPESTATUS[0]} )
  rc=$?
  echo "== bootstrap exited rc=$rc" | tee -a $L
  [ $rc -eq 0 ] && { echo "== LOOP DONE"; break; }
  until getent hosts deb.debian.org >/dev/null; do sleep 10; done
  sleep 15
done

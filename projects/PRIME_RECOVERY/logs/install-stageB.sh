#!/bin/bash
# Post-Phase-8: finish phases 9-12, RNBO skipped, claude CLI only. Retries on network failure.
export PATH="$HOME/.cargo/bin:$HOME/.npm-global/bin:$HOME/.local/bin:$PATH" DEBIAN_FRONTEND=noninteractive
for i in $(seq 1 20); do
  L=$HOME/PRIME_RECOVERY/logs/bootstrap-$(date +%Y%m%d-%H%M%S).log; echo $L > $HOME/PRIME_RECOVERY/logs/CURRENT
  ( cd ~ && DEVICE=gorpi7v5 ROLE=instrument HARDWARE=pi5-16gb REPO_DIR=$HOME/demiurge DEMIURGE_SKIP_PHASES=10 DEMIURGE_AGENT_CLIS=claude bash ~/demiurge/bootstrap.sh 2>&1 | tee $L; exit ${PIPESTATUS[0]} )
  rc=$?; echo "== bootstrap exited rc=$rc" | tee -a $L
  [ $rc -eq 0 ] && { echo "== STAGEB DONE"; break; }
  until getent hosts deb.debian.org >/dev/null; do sleep 10; done; sleep 15
done

#!/bin/bash
# PRIME install autopilot (gorpi7v5). Survives Claude restarts and the Phase 8 reboot.
#   stageA: wait for the Phase 8 prompt in tmux gorpi7v5:installloop, do the
#           pre-reboot guards, arm the post-reboot resume unit, answer yes / NO-overclock.
#   stageB: (run by prime-install-resume.service after the reboot) apply the
#           skip-phases patch, finish phases 9-12 without RNBO and extra CLIs.
# Guards are the ones EMGOR/-A- set on 2026-10-21: netguard timer disabled (hotspot
# saboteur), demiurge.service NOT enabled (engine first start is a deliberate act),
# no hifiberry-dacplusadcpro line (the HAT probes from EEPROM), overclock declined.
set -u
export PATH="$HOME/.cargo/bin:$HOME/.npm-global/bin:$HOME/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
R=$HOME/PRIME_RECOVERY; LOG=$R/logs/autopilot.log; PANE=gorpi7v5:installloop
log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }
bus() { cod send gorcave "gorpi7v5 (-C-) autopilot: $*" >/dev/null 2>&1 || true; }

guards() {
  for u in demiurge-netguard.timer demiurge-netguard.service; do
    systemctl list-unit-files "$u" >/dev/null 2>&1 && sudo systemctl disable --now "$u" 2>&1 | tee -a "$LOG"
  done
  if systemctl is-enabled demiurge.service >/dev/null 2>&1; then
    sudo systemctl disable demiurge.service 2>&1 | tee -a "$LOG"; log "demiurge.service disabled"
  fi
  if grep -q '^dtoverlay=hifiberry-dacplusadcpro' /boot/firmware/config.txt; then
    sudo cp -a /boot/firmware/config.txt "$R/preinstall-backup/config.txt.with-dacplusadcpro.$(date +%s)"
    sudo sed -i 's/^dtoverlay=hifiberry-dacplusadcpro/#dtoverlay=hifiberry-dacplusadcpro  # removed: HAT probes via EEPROM/' /boot/firmware/config.txt
    sync; log "REMOVED dtoverlay=hifiberry-dacplusadcpro"
  fi
  log "netguard: $(systemctl is-enabled demiurge-netguard.timer 2>&1) / demiurge: $(systemctl is-enabled demiurge.service 2>&1) / overlay lines: $(grep -c 'hifiberry' /boot/firmware/config.txt)"
}

stageA() {
  log "stageA: waiting for Phase 8 prompt"
  until tmux capture-pane -p -t "$PANE" 2>/dev/null | grep -q 'Apply boot config'; do sleep 10; done
  log "Phase 8 prompt reached"
  guards
  sudo install -m 0644 "$R/autopilot/prime-install-resume.service" /etc/systemd/system/
  sudo systemctl daemon-reload && sudo systemctl enable prime-install-resume.service 2>&1 | tee -a "$LOG"
  sync
  bus "Phase 8 reached. guards applied, resume unit armed. answering yes (base block) / NO overclock, rebooting."
  tmux send-keys -t "$PANE" "yes" Enter
  for i in $(seq 1 60); do
    tmux capture-pane -p -t "$PANE" | grep -q 'OVERCLOCK' && { tmux send-keys -t "$PANE" "no" Enter; log "overclock declined"; break; }
    sleep 2
  done
  log "stageA done; Phase 8 reboots the board"
}

stageB() {
  log "stageB: post-reboot, cmdline: $(grep -o 'isolcpus=[^ ]*' /proc/cmdline)"
  cd "$HOME/demiurge"
  if ! grep -q DEMIURGE_SKIP_PHASES setup-script/setup-demiurge.sh; then
    git apply "$R/patches/0002-skip-phases-agent-clis.patch" && git add setup-script/setup-demiurge.sh && \
    git -c user.name=EMG0R -c user.email=maintainer@example.invalid commit -q -m "setup: DEMIURGE_SKIP_PHASES and DEMIURGE_AGENT_CLIS env switches

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && log "skip patch applied"
  fi
  until getent hosts deb.debian.org >/dev/null; do sleep 10; done
  for i in $(seq 1 20); do
    L=$R/logs/bootstrap-$(date +%Y%m%d-%H%M%S).log; echo "$L" > "$R/logs/CURRENT"
    ( cd ~ && DEVICE=gorpi7v5 ROLE=instrument HARDWARE=pi5-16gb REPO_DIR=$HOME/demiurge \
      DEMIURGE_SKIP_PHASES=10 DEMIURGE_AGENT_CLIS=claude bash ~/demiurge/bootstrap.sh </dev/null >"$L" 2>&1 )
    rc=$?; echo "== bootstrap exited rc=$rc" >> "$L"; log "stageB run $i rc=$rc"
    [ $rc -eq 0 ] && break
    until getent hosts deb.debian.org >/dev/null; do sleep 10; done; sleep 15
  done
  guards
  sudo systemctl disable prime-install-resume.service 2>&1 | tee -a "$LOG"
  echo done > "$R/autopilot/INSTALL_DONE"
  bus "install finished (rc=$rc). demiurge.service left disabled; engine not started."
}

case "${1:-}" in stageA) stageA ;; stageB) stageB ;; *) echo "usage: $0 stageA|stageB"; exit 2 ;; esac

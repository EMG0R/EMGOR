#!/bin/bash
#
# DEMIURGE — UI + aux launcher (config-driven).
# Install to: /usr/local/bin/demiurge-ui-launch.sh
# Called by:  neptr-ui.service  (ExecStart=, Type=simple)
#
# This is the SINGLE source of truth for what the on-device UI runs. Instead of
# hard-coding the program in the unit, it reads live.conf so the user-facing
# config file is central control:
#
#   ui      = on | off    master toggle for the UI + all aux commands
#   ui_cmd  = <command>    FULL command for the main UI (interpreter + args)
#   ui_also = <list>       any number of EXTRA full commands, one per indented
#                          line, launched alongside the UI
#
# Behaviour:
#   * ui = off            -> log + exit 0  (service goes inactive, nothing runs)
#   * ui = on, no ui_cmd  -> log + exit 0  (nothing to run)
#   * ui = on, ui_cmd set -> start every ui_also command in the background, then
#                            exec ui_cmd as the unit's main process.
#
# Because neptr-ui.service uses the default control-group kill, stopping or
# restarting the unit tears down the UI AND every aux child together. The unit
# is also PartOf=demiurge.service, so `systemctl restart demiurge` relaunches
# the whole set.
#
# FAIL-SAFE: if live.conf can't be found, default ui=on (matches the historical
# always-on behaviour) and fall back to the legacy UI command.
#
# Re-read after editing live.conf:  sudo systemctl restart neptr-ui
#   (or restart the whole stack:    sudo systemctl restart demiurge)
#
set -u

log() { echo "[demiurge-ui] $*"; }

# Legacy fallback command (only used if live.conf has ui=on but no ui_cmd, or
# config can't be resolved at all). $HOME is the service's own User= (set by
# neptr-ui.service, templated at install time), so this follows the rig's
# actual user with no literal.
FALLBACK_CMD="$HOME/kivy-venv/bin/python $HOME/NEPTR_phase4/neptrPhase4_UI.py"

# ── resolve live.conf the SAME way the gate / power applier do ──
LIVE=""
BOOT=/boot/firmware/demiurge.conf
if [ -f "$BOOT" ]; then
    LIVE="$(sed -n 's/^[[:space:]]*launch[[:space:]]*=[[:space:]]*//p' "$BOOT" | sed 's/#.*//' | tr -d '[:space:]' | head -1)"
fi
if [ -z "$LIVE" ] || [ ! -f "$LIVE" ]; then
    for h in /home/*/demiurge/live.conf /root/demiurge/live.conf; do
        [ -f "$h" ] && LIVE="$h" && break
    done
fi

if [ -z "$LIVE" ] || [ ! -f "$LIVE" ]; then
    log "no live.conf found — defaulting ui=on, fallback command (fail-safe)"
    exec $FALLBACK_CMD
fi

# ── ui = on | off (default on) ──
UI="$(sed -n 's/^[[:space:]]*ui[[:space:]]*=[[:space:]]*//p' "$LIVE" | sed 's/#.*//' | tr -d '[:space:]' | head -1)"
[ -z "$UI" ] && UI=on
case "$UI" in
    off|OFF|Off|0|no|false)
        log "live.conf=$LIVE  ui=$UI  -> UI DISABLED (exit 0, nothing runs)"
        exit 0
        ;;
esac

# ── ui_cmd = full command for the main UI ──
# Strip only a trailing " # comment" (space-hash) so commands keep their args.
UI_CMD="$(sed -n 's/^[[:space:]]*ui_cmd[[:space:]]*=[[:space:]]*//p' "$LIVE" | sed 's/[[:space:]]#.*//' | sed 's/[[:space:]]*$//' | head -1)"

# ── ui_also = newline list of extra full commands (indented continuation) ──
# Grab indented, non-comment, non-blank lines following `ui_also =` until the
# next column-0 line (a new key or section header).
mapfile -t UI_ALSO < <(awk '
  /^[[:space:]]*ui_also[[:space:]]*=/ {
    grab=1
    sub(/^[^=]*=/, "")
    rest=$0; gsub(/^[[:space:]]+/, "", rest); gsub(/[[:space:]]+$/, "", rest)
    if (rest != "" && rest !~ /^#/) print rest
    next
  }
  grab==1 {
    if ($0 ~ /^[^[:space:]]/) { grab=0; next }   # column-0 line ends the block
    line=$0; gsub(/^[[:space:]]+/, "", line); gsub(/[[:space:]]+$/, "", line)
    if (line == "" || line ~ /^#/) next
    print line
  }
' "$LIVE")

# ── launch every aux command in the background (same cgroup) ──
for cmd in "${UI_ALSO[@]:-}"; do
    [ -z "$cmd" ] && continue
    log "aux: $cmd"
    bash -c "$cmd" &
done

# ── exec the main UI as the unit's main process ──
if [ -z "$UI_CMD" ]; then
    log "live.conf=$LIVE  ui=on but ui_cmd empty — using fallback command"
    UI_CMD="$FALLBACK_CMD"
fi
log "live.conf=$LIVE  ui=on  -> exec: $UI_CMD"
exec bash -c "$UI_CMD"

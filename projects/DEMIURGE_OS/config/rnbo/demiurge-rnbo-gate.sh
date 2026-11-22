#!/bin/bash
#
# DEMIURGE — RNBO runner gate.
# Install to: /usr/local/bin/demiurge-rnbo-gate.sh
# Called by:  rnbo-runner.service as  ExecCondition=  (evaluated on every start).
#
# Decides whether the always-on RNBO runner (rnbooscquery) should actually come
# up. The runner is otherwise unconditionally enabled at boot and burns CPU/power
# even with no RNBO patch in use. This gate keeps it down unless RNBO is wanted.
#
# Reads ~/demiurge/live.conf and runs the runner when EITHER:
#
#   * a *.rnbo token appears in the `chain =` block  (a patch is in use), OR
#   * rnbo = on                                       (explicit user opt-in, so
#                                                      Max can push/compile a
#                                                      patch with nothing yet in
#                                                      the chain)
#
# ExecCondition semantics: exit 0 -> runner starts; exit 1 -> unit is skipped
# CLEANLY (status "condition failed", NOT "failed", and Restart= is not armed).
#
# FAIL-CLOSED by design (opposite of the audio gate): if live.conf is missing or
# `rnbo` is unset and no *.rnbo is in the chain, the runner stays DOWN. That is
# the whole point — no RNBO config, no RNBO process.
#
# Re-read after editing live.conf:  sudo systemctl restart rnbo-runner
#
set -u

log() { echo "[demiurge-rnbo-gate] $*"; }

# ── resolve live.conf the SAME way the launcher does ──
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
    log "no live.conf found -> RNBO runner stays DOWN (fail-closed)"
    exit 1
fi

# 1) explicit opt-in:  rnbo = on
RNBO="$(sed -n 's/^[[:space:]]*rnbo[[:space:]]*=[[:space:]]*//p' "$LIVE" | sed 's/#.*//' | tr -d '[:space:]' | head -1)"
case "$RNBO" in
    on|ON|On|1|yes|true|YES|True)
        log "live.conf=$LIVE  rnbo=$RNBO  -> runner ENABLED (explicit opt-in)"
        exit 0
        ;;
esac

# 2) a *.rnbo patch is named in the chain = block (comments stripped). The block
#    is the `chain =` line plus its indented continuation lines, up to the next
#    top-level `key =` line or EOF.
CHAIN_BLOCK="$(awk '
    /^[[:space:]]*chain[[:space:]]*=/{f=1; print; next}
    f && /^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=/{f=0}
    f{print}
' "$LIVE")"
if printf '%s\n' "$CHAIN_BLOCK" | sed 's/#.*//' | grep -qiE '\.rnbo([[:space:]]|$)'; then
    log "live.conf=$LIVE  .rnbo in chain -> runner ENABLED (patch in use)"
    exit 0
fi

log "live.conf=$LIVE  rnbo=${RNBO:-<unset>}  no .rnbo in chain -> runner DOWN"
exit 1

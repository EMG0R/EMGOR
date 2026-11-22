#!/bin/sh
# HOME's AUDIO mode: start the audio instrument and block until it is done.
#
# WHY THIS ASKS SYSTEMD INSTEAD OF RUNNING THE UI ITSELF
#
# live.conf's `ui_cmd` is only the PROGRAM. How the audio UI actually starts is
# owned by neptr-ui.service, and the unit carries things the command line does
# not: SDL_VIDEODRIVER=kmsdrm, XDG_RUNTIME_DIR, HOME,
# WorkingDirectory=NEPTR_phase4, the video/render/input supplementary groups,
# and the `ui_also` aux commands demiurge-ui-launch.sh starts alongside it.
#
# Re-exec'ing ui_cmd from the video engine reproduced none of that, and on the
# device the UI came up ROTATED SIDEWAYS (observed 2026-08-14). That is the
# "two mechanisms for one job" failure exactly: the second one is always
# subtly different, and the difference shows up on stage. So HOME asks the
# owner. One way to start the audio UI, and it is the same one the boot
# sequence uses.
#
# Blocking is the point: HOME drops DRM master before running this and takes it
# back when it returns, so this must not exit until the audio mode is actually
# finished. SIGTERM (HOME's return-to-HOME gesture) stops the unit and returns,
# which is what frees the display again.
set -u

UNIT=neptr-ui.service

# demiurge.service (the actual audio engine — csound/chuck/etc via the
# launcher) is deliberately NOT enabled at boot (2026-08-23:
# `systemctl disable demiurge.service`). HOME is now the only thing that
# starts it — until AUDIO is chosen here, the engine never comes up, which is
# the whole point of the mode selector: choosing VIDEO or GAMES must mean
# audio genuinely never starts, not just "the UI for it doesn't show."
#
# neptr-ui.service is PartOf=demiurge.service, but PartOf only propagates
# STOP/restart from demiurge -> neptr-ui, never START. So starting neptr-ui
# alone (the old behavior here, back when demiurge auto-started at boot) no
# longer brings audio up at all — it has to be started explicitly, first.
AUDIO_UNIT=demiurge.service

# `systemctl` direct first, sudo -n second: the same pattern the arcade's
# ui-release/ui-restore use, so a rig set up for one is set up for both.
sc() {
    systemctl "$@" 2>/dev/null || sudo -n systemctl "$@" 2>/dev/null
}

stop_unit() {
    sc stop "$UNIT"
    # Do not return until the fd is really gone: the caller's drmSetMaster
    # races the kernel reaping this unit's DRM fd, and every 100 ms spent here
    # is 100 ms it does not spend retrying.
    i=0
    while systemctl is-active --quiet "$UNIT" && [ "$i" -lt 40 ]; do
        sleep 0.25
        i=$((i + 1))
    done
    # Audio mode is over: also stop the engine, or it keeps running silently
    # with no UI attached to it, which is exactly the "audio is running when
    # it shouldn't be" bug this whole change exists to close. Fire-and-forget
    # — nothing here needs to block on the engine's own shutdown, and DRM
    # handoff timing only cares about neptr-ui's fd above.
    sc stop "$AUDIO_UNIT"
}

trap 'stop_unit; exit 0' TERM INT

if ! sc start "$AUDIO_UNIT"; then
    echo "[mode-audio] could not start $AUDIO_UNIT (need root or NOPASSWD systemctl)" >&2
    exit 1
fi

if ! sc start "$UNIT"; then
    echo "[mode-audio] could not start $UNIT (need root or NOPASSWD systemctl)" >&2
    exit 1
fi

# Poll rather than `systemctl start --wait`: the unit is Type=simple and
# long-lived, so there is nothing to wait ON. What we need to know is when it
# goes inactive -- the audio mode ending.
#
# DEBOUNCE (2026-08-21): a `systemctl restart demiurge` -- which a buffer-size
# (quantum) change triggers -- momentarily makes neptr-ui INACTIVE because it is
# PartOf=demiurge.service. That transient bounce is NOT the end of audio mode.
# Without a grace window the loop returned here, HOME re-grabbed DRM master, and
# the rig flipped to VIDEO on every buffer-size change (then neptr-ui vs
# neptr-video raced for drmSetMaster). The intentional return-to-HOME still
# works unchanged: it arrives as SIGTERM and is handled by the trap above,
# independent of this loop. So only SUSTAINED inactivity (~10s: a real crash
# that systemd did not restart) ends audio mode here.
while :; do
    while systemctl is-active --quiet "$UNIT"; do
        sleep 0.5
    done
    # neptr-ui went inactive -- wait out a possible restart bounce before
    # concluding audio mode ended.
    back=0
    grace=0
    while [ "$grace" -lt 20 ]; do
        sleep 0.5
        grace=$((grace + 1))
        if systemctl is-active --quiet "$UNIT"; then
            back=1
            break
        fi
    done
    [ "$back" = 1 ] || break
done

# Sustained-crash exit (no SIGTERM, so the trap above never fired): still tear
# the engine down on the way out, same as an explicit return-to-HOME does.
stop_unit

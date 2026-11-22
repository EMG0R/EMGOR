#!/bin/sh
# HOME's GAMES mode: run the arcade and block until the player leaves it.
#
# THE RULE THIS SCRIPT EXISTS TO KEEP
#
#   HOME has already dropped DRM master by the time this runs, so from here
#   until this script returns there is NOTHING on the panel that HOME drew and
#   no HOME input being serviced. A mode that reaches a state it cannot be
#   asked to leave therefore does not look like a broken mode -- it looks like
#   a frozen instrument, and the only way out is the power lead. That happened.
#
#   So every path through this script is bounded. It refuses BEFORE handing
#   anything over if the arcade cannot possibly work, it gives up if RetroArch
#   never appears, it gives up if RetroArch vanishes and the arcade does not
#   notice, and on the way out it reaps RetroArch and makes sure the audio UI
#   was not started behind HOME's back. Returning with a bad exit code is a
#   fine outcome; HOME prints it and you are back on the menu. Not returning
#   is not an outcome at all.
#
# WHAT RUNS THE GAME
#
#   The arcade console in ~/demiurge/games -- arcade_supervisor.py -- via
#   games_console.py next to this script. That driver imports the supervisor
#   and runs it; it does not reimplement it. The virtual pad, the button roles,
#   the ROM/core choice, the audio freeze and the jb-hold exit gesture all
#   stay where they already live. See games_console.py's header for the two
#   things HOME overrides and why.
#
#   The supervisor owns the Teensy serial port, and a serial port has exactly
#   one reader. That is why this cannot be "launch RetroArch and also read the
#   stick" -- an earlier cut did launch RetroArch directly and had no controls
#   at all, which is also why it could not be quit.
#
# Exit codes (HOME shows them as "GAMES EXITED (n)"):
#   0  the player left the console normally
#   2  the arcade is not installed where we expect it
#   3  retroarch is not installed
#   4  the configured ROM or core is missing
#   5  there is no control source -- no Teensy AND no demiurge-io on the bus,
#      so no pad and no exit gesture
#   6  the virtual pad cannot be created (uinput / python3-evdev)
#   7  something else already owns the serial port
#   8  RetroArch never came up
#   9  RetroArch went away and the arcade did not return
set -u

GAMES_DIR="${NV_GAMES_DIR:-${HOME}/demiurge/games}"
CONF="${GAMES_DIR}/games.conf"
SUPERVISOR="${GAMES_DIR}/arcade_supervisor.py"

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DRIVER="${SCRIPT_DIR}/games_console.py"

# How long RetroArch may take to appear, and how long it may be missing while
# the arcade is still running before we conclude it is not coming back.
START_TIMEOUT="${NV_GAMES_START_TIMEOUT:-45}"
GONE_GRACE="${NV_GAMES_GONE_GRACE:-10}"

say() { echo "[mode-games] $*" >&2; }

conf() {
    # `key = value`, '#' comments -- the same trivial format the rest of the
    # stack uses. Last assignment wins.
    sed -n "s/[[:space:]]*#.*//; s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" \
        "$CONF" 2>/dev/null | tail -1
}

# ── preflight ───────────────────────────────────────────────────────────────
#
# Every one of these is a condition under which the old script would have
# handed the display over and then sat on a black screen forever. They are
# cheap, and they run BEFORE anything is started, so a refusal costs nothing
# but a line on the menu.
preflight() {
    [ -f "$SUPERVISOR" ] || {
        say "no arcade at $GAMES_DIR (expected arcade_supervisor.py)"
        return 2
    }
    [ -f "$DRIVER" ] || {
        say "missing $DRIVER -- deploy is incomplete"
        return 2
    }
    [ -f "${SCRIPT_DIR}/control_bus.py" ] || {
        say "missing ${SCRIPT_DIR}/control_bus.py -- deploy is incomplete"
        return 2
    }

    RETROARCH="${NV_GAMES_RETROARCH:-$(command -v retroarch 2>/dev/null)}"
    [ -n "$RETROARCH" ] && [ -x "$RETROARCH" ] || {
        say "retroarch is not installed (see games/docs/INSTALL.md)"
        return 3
    }

    # The arcade decides WHICH rom and core; we only check the answer exists,
    # because RetroArch's failure for a missing ROM is a menu nobody can drive.
    rom="$(conf default_game)"
    core="$(conf default_core)"
    if [ -n "$rom" ]; then
        [ -f "${GAMES_DIR}/roms/${rom}" ] || {
            say "default_game '${rom}' is not in ${GAMES_DIR}/roms"
            return 4
        }
    fi
    if [ -n "$core" ]; then
        [ -f "$core" ] || { say "default_core '${core}' is missing"; return 4; }
    fi

    # Controls. The rule is unchanged and is the whole reason this check
    # exists: no controls means no pad AND no exit gesture, which is a mode
    # that cannot be left. What HAS changed is that a Teensy is no longer the
    # only way to have controls -- see games_console.py's header. So the check
    # is now "is there SOME control source", not "is there a tty".
    port="${NV_GAMES_SERIAL:-$(conf serial_port)}"
    [ -n "$port" ] || port=/dev/ttyACM0
    CONTROL="${NV_GAMES_CONTROL:-auto}"
    if [ "$CONTROL" = auto ]; then
        if [ -e "$port" ]; then CONTROL=serial; else CONTROL=bus; fi
    fi
    if [ "$CONTROL" = serial ]; then
        [ -e "$port" ] || {
            say "no Teensy at ${port} -- without the stick there is no way out of a game"
            return 5
        }
    else
        # The bus path's equivalent of "is the stick plugged in": demiurge-io
        # is what CONDITIONS and fans out every control, real or virtual, so
        # with it down the pad receives nothing and the console is once again
        # a room with no door. Refuse before handing the display over.
        #
        # Checked with systemctl where there is one, and skipped entirely
        # where there is not (running this by hand on a dev box), because a
        # refusal that cannot be satisfied is worse than no check.
        # NV_GAMES_SYSTEMCTL exists so the refusal itself can be exercised on
        # a machine that has no systemd -- an untested refusal is a refusal
        # that fires at the wrong time and nobody finds out until the display
        # is already handed over.
        SYSTEMCTL="${NV_GAMES_SYSTEMCTL:-systemctl}"
        if command -v "$SYSTEMCTL" >/dev/null 2>&1 &&
           "$SYSTEMCTL" list-unit-files demiurge-io.service >/dev/null 2>&1; then
            "$SYSTEMCTL" is-active --quiet demiurge-io.service 2>/dev/null || {
                say "controls come from the DEMIURGE bus (no Teensy at ${port}),"
                say "  but demiurge-io.service is not running -- nothing would"
                say "  reach the pad and there would be no way out of a game."
                say "  Start it:  sudo systemctl start demiurge-io"
                return 5
            }
        fi
        say "controls: DEMIURGE bus on :${NV_GAMES_OSC_PORT:-8005} (no Teensy at ${port})"
    fi
    export NV_GAMES_CONTROL="$CONTROL"

    uinput="${NV_GAMES_UINPUT:-/dev/uinput}"
    [ -w "$uinput" ] || {
        say "${uinput} is not writable -- the virtual pad cannot be created"
        say "  (needs the 'input' group + the udev rule from games/bin/games-install.sh)"
        return 6
    }
    "${NV_GAMES_PYTHON:-python3}" -c 'import evdev' 2>/dev/null || {
        say "python3-evdev is missing -- the virtual pad cannot be created"
        return 6
    }

    # One reader per serial port. If the arcade daemon is already up it owns
    # the Teensy, and a second reader gets a stick that answers to neither.
    if systemctl is-active --quiet demiurge-arcade.service 2>/dev/null; then
        say "demiurge-arcade.service is running and owns ${port}."
        say "  It already has its own way into a game (a launch CODE on the"
        say "  footswitches). Stop it to reach games from HOME instead."
        return 7
    fi

    return 0
}

if [ "${1:-}" = "--preflight" ]; then
    preflight
    exit $?
fi

# Whether the audio UI was already up before GAMES started. Under HOME it is
# down (HOME stopped it, or it never ran); anywhere else it may not be, and
# teardown must not take it away from someone who was using it.
UI_WAS_ACTIVE=0
systemctl is-active --quiet neptr-ui.service 2>/dev/null && UI_WAS_ACTIVE=1

# ── teardown ────────────────────────────────────────────────────────────────
#
# Runs on every exit path, including SIGTERM from HOME's return-to-HOME
# gesture. Order matters and mirrors the arcade's own: close RetroArch first
# (it holds the CRTC), then make sure the display is genuinely free before we
# let HOME try to take master back.
reaped=0
teardown() {
    [ "$reaped" = 1 ] && return
    reaped=1

    # SIGTERM to the whole group, not SIGKILL: the arcade has to reach its own
    # _exit_console -- close RetroArch cleanly so it drmDropMasters, close the
    # pad, thaw the audio -- or HOME inherits the reclaim race and the synth
    # stays frozen.
    if [ -n "${DRIVER_PID:-}" ]; then
        kill -TERM "-${DRIVER_PID}" 2>/dev/null || kill -TERM "$DRIVER_PID" 2>/dev/null
        i=0
        while kill -0 "$DRIVER_PID" 2>/dev/null && [ "$i" -lt 60 ]; do
            sleep 0.25
            i=$((i + 1))
        done
        if kill -0 "$DRIVER_PID" 2>/dev/null; then
            # The arcade's serial read can block with the Teensy unplugged, and
            # a blocked reader never sees its own SIGTERM flag. Kill it -- but
            # then WE are responsible for everything it did not clean up.
            say "arcade did not exit on SIGTERM -- SIGKILL"
            kill -KILL "-${DRIVER_PID}" 2>/dev/null || kill -KILL "$DRIVER_PID" 2>/dev/null
        fi
    fi

    # Unconditional, because a SIGKILLed arcade leaves RetroArch orphaned and
    # still holding DRM master -- HOME would then never get the display back,
    # which is a frozen instrument by another route. -x, never -f.
    pkill -TERM -x retroarch 2>/dev/null
    i=0
    while pgrep -x retroarch >/dev/null 2>&1 && [ "$i" -lt 32 ]; do
        sleep 0.25
        i=$((i + 1))
    done
    pkill -KILL -x retroarch 2>/dev/null

    # SIGCONT is a no-op on anything that was never stopped, so this is safe to
    # run always -- and it is the only thing standing between a SIGKILLed
    # arcade and a permanently frozen synth.
    [ -x "${GAMES_DIR}/bin/demiurge-thaw" ] && "${GAMES_DIR}/bin/demiurge-thaw" >/dev/null 2>&1

    # The audio UI must NOT be running when we return: HOME is about to take
    # DRM master back, and two KMS clients racing for one master is the state
    # the rig froze in. games_console.py suppresses the arcade's ui-restore,
    # so this should never fire -- it fires if some other path started it.
    #
    # Conditioned on it having been DOWN when we started, so running this
    # script by hand on a rig where the audio UI is legitimately up does not
    # silently take the instrument away from you.
    if [ "${UI_WAS_ACTIVE:-0}" = 0 ] &&
       systemctl is-active --quiet neptr-ui.service 2>/dev/null; then
        say "neptr-ui came up during GAMES -- stopping it so HOME can have the display"
        systemctl stop neptr-ui.service 2>/dev/null ||
            sudo -n systemctl stop neptr-ui.service 2>/dev/null
    fi
}

on_term() { teardown; exit "${EXIT_CODE:-0}"; }
trap on_term TERM INT
trap teardown EXIT

# ── run ─────────────────────────────────────────────────────────────────────
preflight || exit $?

# KMSDRM, not X/Wayland: there is no compositor, and HOME has just handed the
# card over. (The arcade strips these itself too; doing it here as well means
# the environment is right no matter which of the two is read.)
unset DISPLAY WAYLAND_DISPLAY PIPEWIRE_NODE
: "${XDG_RUNTIME_DIR:=/run/user/$(id -u)}"
export XDG_RUNTIME_DIR

say "handing off to the arcade console at ${GAMES_DIR}"
"${NV_GAMES_PYTHON:-python3}" "$DRIVER" "$GAMES_DIR" &
DRIVER_PID=$!

# Wait for RetroArch to actually appear. Without this, every failure inside the
# arcade that leaves it running-but-idle reads as a hung instrument.
i=0
limit=$(( START_TIMEOUT * 4 ))
while ! pgrep -x retroarch >/dev/null 2>&1; do
    if ! kill -0 "$DRIVER_PID" 2>/dev/null; then
        wait "$DRIVER_PID"
        EXIT_CODE=$?
        say "arcade exited before RetroArch started (${EXIT_CODE})"
        exit "$EXIT_CODE"
    fi
    if [ "$i" -ge "$limit" ]; then
        say "RetroArch never came up in ${START_TIMEOUT}s -- giving up"
        EXIT_CODE=8
        exit 8
    fi
    sleep 0.25
    i=$((i + 1))
done
say "RetroArch is up -- GAMES is live"

# Block until the player leaves the console, but keep checking that there is
# still a game to leave. The arcade notices RetroArch dying by itself and
# exits; if it is wedged on a dead serial port it never will, and this is the
# net under that.
gone=0
gone_limit=$(( GONE_GRACE * 4 ))
while kill -0 "$DRIVER_PID" 2>/dev/null; do
    if pgrep -x retroarch >/dev/null 2>&1; then
        gone=0
    else
        gone=$((gone + 1))
        if [ "$gone" -ge "$gone_limit" ]; then
            say "RetroArch has been gone ${GONE_GRACE}s and the arcade has not returned"
            EXIT_CODE=9
            exit 9
        fi
    fi
    sleep 0.25
done

wait "$DRIVER_PID"
EXIT_CODE=$?
say "arcade returned (${EXIT_CODE}) -- back to HOME"
exit "$EXIT_CODE"

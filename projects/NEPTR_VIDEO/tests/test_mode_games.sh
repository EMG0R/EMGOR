#!/bin/sh
# GAMES-mode preflight: the refusals that keep the rig from freezing.
#
# Every case below is a condition under which the previous version of
# mode-games.sh handed the display to a program that could not work and then
# waited on it forever -- with HOME's DRM master already dropped, so the panel
# was stuck on whatever was last drawn and no input this stack owns could end
# it. The instrument had to be power-cycled.
#
# That failure is invisible to the engine's own tests: it is not a wrong pixel
# or a wrong note, it is a script that does not return. So the decision "can
# the arcade possibly work from here" is pulled out into `--preflight`, and
# this pins it -- no display, no serial port, no RetroArch, no Pi.
#
# Same no-framework shape as the C++ suites: assert, count, print, exit 1 on
# the first failure that matters.
set -u

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SCRIPT="${HERE}/../deploy/mode-games.sh"

fails=0
checks=0

# --- fixtures ---------------------------------------------------------------
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM

GAMES="$TMP/games"
mkdir -p "$GAMES/roms" "$GAMES/bin"
# The arcade's own files, only as far as preflight looks at them.
: > "$GAMES/arcade_supervisor.py"
: > "$GAMES/roms/galaga.zip"
: > "$TMP/fbneo_libretro.so"

# Stand-ins for the things that only exist on the device.
printf '#!/bin/sh\nexit 0\n' > "$TMP/retroarch"; chmod +x "$TMP/retroarch"
printf '#!/bin/sh\nexit 0\n' > "$TMP/python3";   chmod +x "$TMP/python3"
: > "$TMP/ttyACM0"
: > "$TMP/uinput"

write_conf() {
    cat > "$GAMES/games.conf" <<EOF
# fixture games.conf
default_game = ${1-galaga.zip}
default_core = ${2-$TMP/fbneo_libretro.so}
serial_port = ${3-$TMP/ttyACM0}
EOF
}

# Run the preflight with everything healthy unless the caller overrides a var.
run() {
    env NV_GAMES_DIR="$GAMES" \
        NV_GAMES_RETROARCH="$TMP/retroarch" \
        NV_GAMES_PYTHON="$TMP/python3" \
        NV_GAMES_UINPUT="$TMP/uinput" \
        "$@" /bin/sh "$SCRIPT" --preflight >/dev/null 2>&1
    echo $?
}

expect() {
    label="$1"; want="$2"; got="$3"
    checks=$((checks + 1))
    if [ "$got" != "$want" ]; then
        echo "FAIL: $label -- expected $want, got $got"
        fails=$((fails + 1))
    fi
}

# --- a healthy rig passes ---------------------------------------------------
# The point of a refusal is that it is specific. If the happy path did not
# pass, every case below would "pass" for the wrong reason.
write_conf
expect "healthy rig is allowed through" 0 "$(run)"

# --- 2: the arcade is not there ---------------------------------------------
# HOME must not drop into a mode whose program does not exist. This is the
# cheapest refusal there is and it was not being made.
mv "$GAMES/arcade_supervisor.py" "$TMP/stash.py"
expect "missing arcade_supervisor.py refuses" 2 "$(run)"
mv "$TMP/stash.py" "$GAMES/arcade_supervisor.py"

# --- 3: retroarch is not installed ------------------------------------------
expect "missing retroarch refuses" 3 "$(run NV_GAMES_RETROARCH=$TMP/nope)"

# --- 4: the configured ROM / core is missing --------------------------------
# RetroArch's answer to a missing ROM is its own menu, which on this rig nobody
# can drive: no keyboard, and the pad is bound to game controls. So it is a
# black screen you cannot leave. Refuse instead.
write_conf "notthere.zip"
expect "missing ROM refuses" 4 "$(run)"

write_conf "galaga.zip" "$TMP/no-such-core.so"
expect "missing core refuses" 4 "$(run)"

# --- 5: no controls ----------------------------------------------------------
# This is the freeze itself. No controls means the supervisor has no stick to
# read, which means no pad AND no jb-hold exit -- a game you cannot play and
# cannot quit, with HOME's display already handed away.
#
# The rule is unchanged; what a "control source" IS has widened. A Teensy is
# one. The DEMIURGE control bus -- demiurge-io's conditioned /sensor fan-out,
# which carries the web UI's virtual panel as well as any real board -- is the
# other. So an absent tty is only fatal when serial is the declared source.
write_conf "galaga.zip" "$TMP/fbneo_libretro.so" "$TMP/no-such-tty"
expect "absent serial port refuses when serial is FORCED" 5 \
       "$(run NV_GAMES_CONTROL=serial)"

# On the bus, an absent tty is not a refusal at all -- that is the entire
# point: a rig with no board reaches its games from the phone.
printf '#!/bin/sh\ncase "$1" in list-unit-files) exit 0;; is-active) exit 0;; esac\nexit 0\n' \
    > "$TMP/systemctl-up"; chmod +x "$TMP/systemctl-up"
expect "absent serial port is fine on the bus" 0 \
       "$(run NV_GAMES_CONTROL=bus NV_GAMES_SYSTEMCTL=$TMP/systemctl-up)"
expect "no Teensy auto-selects the bus rather than refusing" 0 \
       "$(run NV_GAMES_SYSTEMCTL=$TMP/systemctl-up)"

# But the bus has its own way of being absent, and it is the same failure by
# another route: with demiurge-io down nothing conditions or fans out a
# control, real or virtual, so the pad receives nothing and the console is
# again a room with no door. That must refuse BEFORE the display is handed
# over, not after.
printf '#!/bin/sh\ncase "$1" in list-unit-files) exit 0;; is-active) exit 3;; esac\nexit 0\n' \
    > "$TMP/systemctl-down"; chmod +x "$TMP/systemctl-down"
expect "bus with demiurge-io DOWN refuses" 5 \
       "$(run NV_GAMES_CONTROL=bus NV_GAMES_SYSTEMCTL=$TMP/systemctl-down)"

# A box with no systemd (a dev machine) must not be refused by a check it
# could never satisfy.
expect "bus with no systemctl at all is allowed through" 0 \
       "$(run NV_GAMES_CONTROL=bus NV_GAMES_SYSTEMCTL=$TMP/no-such-systemctl)"

# A present tty still means serial under auto, so a rig that HAS its board
# keeps reading it and nothing about the hardware path changed.
write_conf
expect "a present serial port still auto-selects serial" 0 "$(run)"

# The bus reader is part of the deploy; without it the bus path is a mode that
# starts and then cannot be driven.
mv "$HERE/../deploy/control_bus.py" "$TMP/stash_bus.py"
expect "missing control_bus.py refuses" 2 "$(run)"
mv "$TMP/stash_bus.py" "$HERE/../deploy/control_bus.py"

# --- 6: the pad cannot be created -------------------------------------------
write_conf
expect "unwritable uinput refuses" 6 "$(run NV_GAMES_UINPUT=$TMP/no-such-uinput)"

printf '#!/bin/sh\nexit 1\n' > "$TMP/python-noevdev"; chmod +x "$TMP/python-noevdev"
expect "missing python3-evdev refuses" 6 "$(run NV_GAMES_PYTHON=$TMP/python-noevdev)"

# --- config reading ---------------------------------------------------------
# `key = value` with '#' comments and last-assignment-wins, the same shape the
# rest of the stack uses. A parser that silently read the wrong ROM name would
# show up here as a spurious code 4.
cat > "$GAMES/games.conf" <<EOF
default_game = wrong.zip      # a commented tail must not become part of the value
serial_port = $TMP/ttyACM0
default_core = $TMP/fbneo_libretro.so
default_game = galaga.zip
EOF
expect "trailing comments and last-wins are honoured" 0 "$(run)"

# --- report -----------------------------------------------------------------
if [ "$fails" -ne 0 ]; then
    echo "mode-games: $fails/$checks checks FAILED"
    exit 1
fi
echo "mode-games: $checks checks passed"

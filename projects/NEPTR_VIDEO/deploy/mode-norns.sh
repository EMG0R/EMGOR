#!/bin/sh
# HOME's NORNS mode: run the monome norns sandbox and block until it is done.
#
# Same contract as mode-audio.sh and mode-games.sh: HOME drops DRM master
# before running this and takes it back when it returns, so this must NOT exit
# until the mode is really over, and it must not exit 0 having done nothing
# visible -- a mode that returns instantly with a black screen looks like a
# crash.
#
# WHY THIS REFUSES EARLY RATHER THAN LAUNCHING A PARTIAL RIG
#
# norns is three processes that must come up in order (grid bridge, then
# matron, then the engine), and a half-started one takes the display with it.
# Until every piece is genuinely installed this script says so on stdout and
# returns a distinct non-zero code, which HOME turns into a message on the
# menu instead of a handover. "NORNS is not built yet" is a fine thing for the
# menu to say; a black screen you have to power-cycle is not.
set -u

say() { echo "[norns] $*"; }

NORNS_DIR="${NORNS_DIR:-$HOME/norns}"
MATRON="$NORNS_DIR/build/matron/matron"

# ── preflight ───────────────────────────────────────────────────────────────
# Checked in the order a human would: is it built, is the bridge here, is the
# patch defined. Each failure names the step that fixes it.
if [ ! -x "$MATRON" ]; then
    say "matron is not built at $MATRON"
    say "run: norns/runtime/install-norns.sh"
    exit 70
fi

say "NORNS mode is not wired up yet -- matron is built but the launch path is not"
say "this entry is deliberately refusing rather than taking the display"
exit 71

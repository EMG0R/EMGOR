#!/bin/bash
#
# Build monome norns (matron + crone) on the NEPTR Pi.
#
# Run ON the Pi:  bash norns/runtime/install-norns.sh
#
# Idempotent. Safe to re-run: apt is a no-op when the packages are already
# there, the clone becomes a pull, and waf builds incrementally.
#
# It installs and builds ONLY. It never starts a service, never touches the
# audio engine, and never takes the display -- same rule as deploy-video.sh,
# for the same reason: taking the display is a decision a human makes on
# purpose.
#
# ── WHY THIS IS NOT JUST "./waf configure" ──────────────────────────────────
#
# Three things bite on Debian Trixie / Pi 5 / arm64, and all three were found
# the hard way on 2026-09-17. Each one fails with an error that names the
# wrong culprit, so they are fixed here rather than rediscovered:
#
#   1. norns ships waf 2.0.14, which imports `imp`. Python 3.12 REMOVED `imp`,
#      and Trixie has 3.13, so the bundled waf dies with ModuleNotFoundError
#      before it evaluates a single dependency. Upgraded to waf 2.0.27 below.
#      The wscript needs no changes -- waf 2.0.x is API-stable.
#
#   2. `nng` is NOT `libnanomsg`. They are different libraries with confusable
#      names (nng is "nanomsg next generation"); installing libnanomsg-dev
#      satisfies nothing and leaves configure saying "nng: not found" while a
#      nanomsg package is plainly installed. The package wanted is libnng-dev.
#
#   3. --desktop is what makes this buildable at all here. It swaps the
#      SPI ssd1322 OLED for an SDL2 window and the GPIO encoders for SDL
#      input. Both are upstream code paths, not patches. See norns/docs.
#
#   4. Two source patches are genuinely required to compile at all on this
#      platform -- an ARMv7-only compiler flag, and a typedef that collides
#      with liblo. They live in runtime/patches/ as separate, commented,
#      idempotent files rather than inline here, so they can be read and
#      reviewed on their own and so a `waf distclean` or re-clone does not
#      quietly lose them. Each explains its own failure mode; each refuses
#      loudly if upstream moves the code out from under it.
#
set -u

NORNS_DIR="${NORNS_DIR:-$HOME/norns-build/norns}"
WAF_VERSION=2.0.27

# Resolved BEFORE anything cd's anywhere. BASH_SOURCE is relative to the
# invoking directory, so reading it after `cd "$NORNS_DIR"` resolves the patch
# directory against the norns tree instead of this script's own location and
# every patch path comes out wrong.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_DIR="$SCRIPT_DIR/patches"

say() { echo "[install-norns] $*"; }
die() { echo "[install-norns] FAILED: $*" >&2; exit 1; }

# ── dependencies ────────────────────────────────────────────────────────────
# supercollider + sc3-plugins are the ENGINE side: most norns scripts are a
# Lua front end over a SuperCollider engine, and without sc3-plugins the
# engines load but their UGens are missing, which presents as a script that
# starts and then makes no sound.
say "installing build dependencies"
sudo apt-get update -qq || die "apt update"
sudo apt-get install -y --no-install-recommends \
    build-essential pkg-config git cmake curl \
    lua5.3 liblua5.3-dev \
    libcairo2-dev libevdev-dev libudev-dev \
    libgpiod-dev libnng-dev libsdl2-dev \
    liblo-dev libsndfile1-dev libasound2-dev \
    libjack-jackd2-dev libncurses-dev libmonome-dev \
    libavahi-compat-libdnssd-dev \
    supercollider supercollider-dev sc3-plugins \
    || die "apt install"

# ── source ──────────────────────────────────────────────────────────────────
mkdir -p "$(dirname "$NORNS_DIR")"
if [ ! -d "$NORNS_DIR/.git" ]; then
    say "cloning norns to $NORNS_DIR"
    git clone --recursive --depth 1 https://github.com/monome/norns.git "$NORNS_DIR" \
        || die "clone"
else
    say "updating $NORNS_DIR"
    git -C "$NORNS_DIR" pull --recurse-submodules || die "pull"
fi

cd "$NORNS_DIR" || die "cd $NORNS_DIR"

# ── waf upgrade (reason 1 above) ────────────────────────────────────────────
# Keyed on the version string so a re-run after upstream fixes their waf does
# not silently keep overwriting a newer one with ours.
if ! grep -q "VERSION=\"$WAF_VERSION\"" waf 2>/dev/null; then
    say "replacing bundled waf with $WAF_VERSION (bundled one needs Python <3.12)"
    [ -f waf.orig ] || cp waf waf.orig
    curl -fsSL -o waf.new "https://waf.io/waf-$WAF_VERSION" || die "waf download"
    grep -q "VERSION=\"$WAF_VERSION\"" waf.new || die "waf download looks wrong"
    chmod +x waf.new && mv waf.new waf
    rm -rf .waf3-* .lock-waf* build
fi

# ── source patches (reason 4 above) ─────────────────────────────────────────
# Applied every run; each is a no-op once applied. A patch whose anchor has
# vanished ABORTS the install rather than building something subtly different
# from what was tested.
say "applying source patches"
python3 "$PATCH_DIR/01-aarch64-flags.py"  "$NORNS_DIR/norns/wscript" \
    || die "patch 01 (aarch64 flags)"
python3 "$PATCH_DIR/02-liblo-typedef.py"  "$NORNS_DIR/matron/src/event_types.h" \
    || die "patch 02 (liblo typedef)"
python3 "$PATCH_DIR/03-sdl-void-casts.py" \
    "$NORNS_DIR/matron/src/hardware/screen/sdl.cc" \
    || die "patch 03 (sdl void casts)"
python3 "$PATCH_DIR/04-sdl-designated-init.py" \
    "$NORNS_DIR/matron/src/hardware/screen/sdl.cc" screen \
    || die "patch 04 (sdl designated init, screen)"
python3 "$PATCH_DIR/04-sdl-designated-init.py" \
    "$NORNS_DIR/matron/src/hardware/input/sdl.cc" input \
    || die "patch 04 (sdl designated init, input)"
python3 "$PATCH_DIR/05-crone-string-include.py" \
    "$NORNS_DIR/crone/src/BufDiskWorker.h" \
    || die "patch 05 (crone string include)"
python3 "$PATCH_DIR/06-desktop-drop-gpio.py" \
    "$NORNS_DIR/norns/wscript" "$NORNS_DIR/matron/src/hardware/io.cc" \
    || die "patch 06 (desktop drops gpio backends)"

# ── build ───────────────────────────────────────────────────────────────────
say "configuring (--desktop: SDL screen + SDL input)"
./waf configure --desktop || die "configure -- see build/config.log"

say "building"
./waf build -j"$(nproc)" || die "build"

[ -x "$NORNS_DIR/build/matron/matron" ] || die "matron did not get built"
say "matron:  $NORNS_DIR/build/matron/matron"
[ -x "$NORNS_DIR/build/crone/crone" ] && say "crone:   $NORNS_DIR/build/crone/crone"
say "done. NOTHING was started -- HOME's NORNS entry is what runs it."

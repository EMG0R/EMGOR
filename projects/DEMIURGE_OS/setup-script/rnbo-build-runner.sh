#!/bin/bash
# rnbo-build-runner.sh — RNBO integration, Phase 0 / Step 5b.
#
# Clones Cycling '74's rnbo.oscquery.runner from source and builds it with
# on-device compile ENABLED (SUPPORT_COMPILE=On -> the preprocessor define
# RNBOOSCQUERY_ENABLE_COMPILE gets set by CMakeLists.txt itself — that define
# is NOT a settable cmake option/cache var in its own right, only the
# resulting compiled-in behaviour). This is the piece the prebuilt apt
# package (rnbo-install-runner.sh) deliberately ships DISABLED, so a source
# build is the only way to get on-device compile.
#
# Requires rnbo-build-deps.sh to have already run (apt toolchain + pinned
# conan 1.66.0 on PATH via ~/.local/bin, default conan profile pinned to
# c++20 / libstdc++11).
#
# THE MISSING PIECE, confirmed against the actual upstream source
# (github.com/Cycling74/rnbo.oscquery.runner, tag v1.4.5-9, CMakeLists.txt):
#   RNBO.cpp itself (the proprietary RNBO C++ engine) is NOT in this repo and
#   is NOT public — normally you'd copy it from a Max/RNBO install's
#   .../Packages/RNBO/source/rnbo directory. Headless, we instead pull it
#   from Cycling '74's public conan remote (added automatically by
#   common/conan.cmake as "cycling-public" -> https://conan-public.cycling74.com)
#   by setting -DRNBO_CONAN_VERSION + -DRNBO_CONAN_TAG. Verified reachable
#   and hosting "rnbo/1.4.5@c74/stable" (curl the remote's v1 search API to
#   re-check: https://conan-public.cycling74.com/v1/conans/search?q=rnbo*).
#   1.4.5 is pinned deliberately: it matches the newest rnbooscquery
#   candidate (1.4.5-9) visible in Cycling '74's own apt repo (Phase Step 1),
#   so the source build we run stays version-aligned with what apt would
#   otherwise have installed prebuilt.
#
# Run as the normal user (no sudo needed for the build itself):
#   bash rnbo-build-runner.sh
#
# Output: $HOME/rnbo.oscquery.runner/build/bin/rnbooscquery
set -euo pipefail

export PATH="$HOME/.local/bin:$PATH"
hash -r

# ruby: upstream README lists it as a build requirement ("ruby 2.0+ to run
# the compile script") and the runner's own packaging (CMakeLists.txt
# CPACK_DEBIAN_PACKAGE_DEPENDS, when SUPPORT_COMPILE is on) lists it as a
# RUNTIME dependency too — the on-device compile path shells out to a
# ruby-based build script at request time, not just at our build time here.
# rnbo-build-deps.sh does not install it (it's a build-runner-specific need,
# not a generic toolchain one), so make sure it's present before we go on.
if ! command -v ruby >/dev/null 2>&1; then
    echo "=== installing ruby (runtime + build dependency for on-device compile) ==="
    sudo apt-get install -y ruby
fi

REPO_DIR="$HOME/rnbo.oscquery.runner"
REPO_URL="https://github.com/Cycling74/rnbo.oscquery.runner.git"
# Pinned to the exact tag whose CMakeLists.txt we read to write this script,
# and whose version string (1.4.5-9) matches the newest apt candidate.
TAG="v1.4.5-9"
JOBS="${RNBO_BUILD_JOBS:-$(nproc)}"

echo "=== conan sanity check ==="
command -v conan >/dev/null || {
    echo "rnbo-build-runner: conan not on PATH — run rnbo-build-deps.sh first" >&2
    exit 1
}
conan --version

echo "=== clone/update rnbo.oscquery.runner @ $TAG ==="
if [[ -d "$REPO_DIR/.git" ]]; then
    git -C "$REPO_DIR" fetch --tags origin
    git -C "$REPO_DIR" checkout "$TAG"
else
    git clone --branch "$TAG" --depth 1 "$REPO_URL" "$REPO_DIR"
fi
# No git submodules in this repo (verified: no .gitmodules at this tag) — the
# RNBO C++ source is fetched via conan below, not via submodule.

echo "=== configure (cmake) ==="
mkdir -p "$REPO_DIR/build"
(
    cd "$REPO_DIR/build"
    # -mcpu=cortex-a76: Pi 5's CPU, per upstream README-rpi.md "PI 5 notes".
    # -DWITH_DBUS=Off: same section — DEMIURGE also has no use for the
    # self-update-over-dbus path (the whole point of rnbo-runner.service's
    # ExecCondition gate + apt-mark hold is that DEMIURGE owns updates, not
    # the runner itself).
    # -DSUPPORT_COMPILE=On: explicit even though it's the project default,
    # so a future upstream default flip doesn't silently disable on-device
    # compile for us.
    # -DRNBO_CONAN_VERSION / -DRNBO_CONAN_TAG: see header comment above —
    # this is what actually resolves RNBO_DIR headlessly.
    CC=gcc CXX=g++ \
    ASMFLAGS="-mcpu=cortex-a76" CFLAGS="-mcpu=cortex-a76" CXXFLAGS="-mcpu=cortex-a76" \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_DBUS=Off \
        -DSUPPORT_COMPILE=On \
        -DRNBO_CONAN_VERSION=1.4.5 \
        -DRNBO_CONAN_TAG=c74/stable
)

echo "=== build (cmake --build, -j$JOBS) ==="
echo "This compiles the runner AND every missing conan dependency (libossia,"
echo "boost, sqlitecpp, ...) from source — expect this to take a while on a"
echo "Pi 5 the first time (tens of minutes), much faster on re-runs once"
echo "conan's local cache is warm."
cmake --build "$REPO_DIR/build" -j"$JOBS"

BIN="$REPO_DIR/build/bin/rnbooscquery"
if [[ -x "$BIN" ]]; then
    echo ""
    echo "STEP 5b OK — built: $BIN"
    "$BIN" --help >/dev/null 2>&1 || true
else
    echo "STEP 5b PROBLEM — build finished but $BIN is missing/not executable." >&2
    exit 1
fi

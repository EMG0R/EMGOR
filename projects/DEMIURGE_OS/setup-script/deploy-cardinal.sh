#!/bin/bash
# deploy-cardinal.sh — install Cardinal (DISTRHO's GPL VCV Rack 2 fork) on the Pi 5
# so a .vcv patch runs as a JACK client beside Csound. Run ON the Pi:
#
#   CONFIRM_NOT_LIVE_AUDIO=yes ./setup-script/deploy-cardinal.sh
#   DRY_RUN=1 ./setup-script/deploy-cardinal.sh        # print the plan, change nothing
#
# Env knobs:
#   CONFIRM_NOT_LIVE_AUDIO=yes  REQUIRED. Downloading 1 GB and (worse) compiling pins
#                               the CPU and WILL xrun a live performance.
#   DRY_RUN=1                   print the plan only (no guard needed, nothing touched)
#   CARDINAL_VERSION=26.02      release tag (DISTRHO/Cardinal)
#   CARDINAL_MODE=auto|release|source   auto = prebuilt aarch64 tarball, else source
#   CARDINAL_PREFIX=/opt/demiurge/cardinal
#   JOBS=2                      make -j (match deploy-nam.sh; RAM is the limit)
#
# NAMING (researched, see docs/cardinal.md): there is NO upstream binary called
# "cardinal-headless". Upstream's JACK standalone is `Cardinal` (bin/Cardinal).
# `make HEADLESS=true` builds the GUI-less PLUGIN formats (lv2/vst2/vst3/clap) and
# the static `loader`, not the JACK standalone. So this script installs the real
# `Cardinal` binary and a thin wrapper /opt/demiurge/bin/cardinal-headless that runs
# it (under xvfb-run when there is no DISPLAY) so the §10 check
# `cardinal-headless --version` has something to call.
#
# Heavy steps run under `nice -n19 ionice -c3`. Nothing here touches systemd or
# any audio service. Idempotent: re-running with the same version is a no-op.
set -euo pipefail

DRY_RUN="${DRY_RUN:-0}"
CARDINAL_VERSION="${CARDINAL_VERSION:-26.02}"
CARDINAL_MODE="${CARDINAL_MODE:-auto}"
CARDINAL_PREFIX="${CARDINAL_PREFIX:-/opt/demiurge/cardinal}"
JOBS="${JOBS:-2}"
BIN_LINK_DIR="/opt/demiurge/bin"
WRAPPER="$BIN_LINK_DIR/cardinal-headless"
REL_URL="https://github.com/DISTRHO/Cardinal/releases/download/${CARDINAL_VERSION}/Cardinal-linux-aarch64-${CARDINAL_VERSION}.tar.gz"
SRC_URL="https://github.com/DISTRHO/Cardinal.git"
WORK="${CARDINAL_WORK:-/var/tmp/cardinal-build}"
LOWPRI="nice -n19 ionice -c3"

# Run via sudo or not: derive the Pi user like the other deploy scripts.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"

run() { # print always; execute only when not dry-run
    echo "+ $*"
    [[ "$DRY_RUN" == 1 ]] || "$@"
}

# ---------------------------------------------------------------------------
# 0. LOUD GUARD
# ---------------------------------------------------------------------------
if [[ "$DRY_RUN" != 1 && "${CONFIRM_NOT_LIVE_AUDIO:-}" != "yes" ]]; then
    cat >&2 <<'MSG'
##########################################################################
#  REFUSING TO RUN.                                                      #
#  This downloads ~1 GB and may compile Cardinal. That pins the CPU and  #
#  WILL xrun a live Demiurge performance. NEVER run during a gig/set.    #
#  Confirm no live audio is running, then re-run with:                   #
#      CONFIRM_NOT_LIVE_AUDIO=yes ./deploy-cardinal.sh                   #
#  (DRY_RUN=1 prints the plan without doing anything.)                   #
##########################################################################
MSG
    exit 3
fi

[[ "$(uname -m)" == "aarch64" ]] || { echo "not aarch64 ($(uname -m)); this script is Pi 5 only" >&2; exit 1; }

SUDO=""
[[ $EUID -eq 0 ]] || SUDO="sudo"

echo "=== DEMIURGE Cardinal deploy (v$CARDINAL_VERSION, mode=$CARDINAL_MODE, dry-run=$DRY_RUN) ==="

# ---------------------------------------------------------------------------
# 1. Idempotence: already at this version?
# ---------------------------------------------------------------------------
STAMP="$CARDINAL_PREFIX/.installed-version"
if [[ -f "$STAMP" && "$(cat "$STAMP")" == "$CARDINAL_VERSION" && -x "$WRAPPER" ]]; then
    echo "Cardinal $CARDINAL_VERSION already installed ($CARDINAL_PREFIX). Nothing to do."
    echo "(delete $STAMP to force a reinstall)"
    DONE_ALREADY=1
else
    DONE_ALREADY=0
fi

if [[ "$DONE_ALREADY" == 0 ]]; then

# ---------------------------------------------------------------------------
# 2. Plan: prebuilt release if it exists, else source
# ---------------------------------------------------------------------------
USE=source
if [[ "$CARDINAL_MODE" == release || "$CARDINAL_MODE" == auto ]]; then
    if [[ "$DRY_RUN" == 1 ]]; then
        echo "plan: HEAD-check $REL_URL; use it if present, else build from source"
        USE=release
    elif curl -fsIL --max-time 20 "$REL_URL" >/dev/null 2>&1; then
        USE=release
    elif [[ "$CARDINAL_MODE" == release ]]; then
        echo "no prebuilt aarch64 release at $REL_URL" >&2; exit 1
    fi
fi
echo "--- install method: $USE"

run $SUDO mkdir -p "$CARDINAL_PREFIX" "$BIN_LINK_DIR" "$WORK"
run $SUDO chown "$PI_USER":"$PI_USER" "$WORK"

# Runtime deps (both paths). Cardinal standalone is GL + JACK (pipewire-jack here).
RUNTIME_PKGS="xvfb libgl1 libx11-6 libxext6 libxrandr2 libxcursor1 libfftw3-double3 libsndfile1 liblo7 libmagic1 libarchive13 libjansson4 libsamplerate0 libspeexdsp1"

if [[ "$USE" == release ]]; then
    # -----------------------------------------------------------------------
    # 3a. Prebuilt: download, extract, locate the JACK standalone `Cardinal`
    # -----------------------------------------------------------------------
    run $SUDO apt-get install -y --no-install-recommends $RUNTIME_PKGS
    run $LOWPRI curl -fL --retry 3 -o "$WORK/cardinal.tar.gz" "$REL_URL"
    run rm -rf "$WORK/extract"; run mkdir -p "$WORK/extract"
    run $LOWPRI tar -xzf "$WORK/cardinal.tar.gz" -C "$WORK/extract"
    if [[ "$DRY_RUN" != 1 ]]; then
        # Tarball layout is unverified: find the standalone by name, not path.
        BIN="$(find "$WORK/extract" -type f -name Cardinal -perm -u+x | head -n1)"
        [[ -n "$BIN" ]] || { echo "no executable 'Cardinal' in tarball; layout changed, inspect $WORK/extract" >&2; exit 1; }
        SRCDIR="$(dirname "$BIN")"
        echo "--- found $BIN"
        $SUDO rm -rf "$CARDINAL_PREFIX/bin"; $SUDO mkdir -p "$CARDINAL_PREFIX/bin"
        $SUDO cp -a "$SRCDIR"/. "$CARDINAL_PREFIX/bin/"
    else echo "+ (locate 'Cardinal' in tarball; copy its dir to $CARDINAL_PREFIX/bin)"; fi
else
    # -----------------------------------------------------------------------
    # 3b. Source: shallow recursive clone at the tag, JACK standalone target
    # -----------------------------------------------------------------------
    run $SUDO apt-get install -y --no-install-recommends git build-essential cmake pkg-config wget python3 \
        libdbus-1-dev libgl1-mesa-dev liblo-dev libfftw3-dev libmagic-dev libsndfile1-dev \
        libx11-dev libxcursor-dev libxext-dev libxrandr-dev libarchive-dev libjansson-dev \
        libsamplerate0-dev libspeexdsp-dev libjack-jackd2-dev $RUNTIME_PKGS
    if [[ ! -d "$WORK/Cardinal/.git" ]]; then
        run $LOWPRI git clone --recursive --depth 1 --shallow-submodules --branch "$CARDINAL_VERSION" "$SRC_URL" "$WORK/Cardinal"
    fi
    # `make` default target builds bin/Cardinal (JACK standalone) + plugins. NOT
    # HEADLESS=true: that drops the GL/X11 deps the standalone needs.
    run $LOWPRI make -C "$WORK/Cardinal" -j"$JOBS" jack
    if [[ "$DRY_RUN" != 1 ]]; then
        [[ -x "$WORK/Cardinal/bin/Cardinal" ]] || { echo "build finished but bin/Cardinal missing; try 'make -j$JOBS' (default target) manually" >&2; exit 1; }
        $SUDO rm -rf "$CARDINAL_PREFIX/bin"; $SUDO mkdir -p "$CARDINAL_PREFIX/bin"
        $SUDO cp -a "$WORK/Cardinal/bin/." "$CARDINAL_PREFIX/bin/"
    else echo "+ (copy bin/ to $CARDINAL_PREFIX/bin)"; fi
fi

# ---------------------------------------------------------------------------
# 4. Wrapper: cardinal-headless -> real `Cardinal`, xvfb when no DISPLAY.
#    Patch loading flags are unverified; extra args pass straight through.
# ---------------------------------------------------------------------------
echo "--- installing $WRAPPER"
if [[ "$DRY_RUN" != 1 ]]; then
    $SUDO tee "$WRAPPER" >/dev/null <<WRAP
#!/bin/bash
# cardinal-headless — wrapper for the upstream JACK standalone 'Cardinal'.
# Runs under a virtual X display when there is no real one. Written by deploy-cardinal.sh.
set -e
CARDINAL="$CARDINAL_PREFIX/bin/Cardinal"
if [[ -z "\${DISPLAY:-}" && -z "\${WAYLAND_DISPLAY:-}" ]]; then
    exec xvfb-run -a "\$CARDINAL" "\$@"
fi
exec "\$CARDINAL" "\$@"
WRAP
    $SUDO chmod 0755 "$WRAPPER"
    echo "$CARDINAL_VERSION" | $SUDO tee "$STAMP" >/dev/null
else echo "+ write $WRAPPER and $STAMP"; fi

fi # DONE_ALREADY

# ---------------------------------------------------------------------------
# 5. Verify (milestone: `cardinal-headless --version`). Cheap, no audio started.
# ---------------------------------------------------------------------------
echo "--- verify"
if [[ "$DRY_RUN" == 1 ]]; then
    echo "+ timeout 20 $WRAPPER --version   (falls back to --help, then ldd, if --version is unsupported)"
    echo "DRY RUN complete: nothing was changed."
    exit 0
fi
set +e
timeout 20 "$WRAPPER" --version >"$WORK/version.out" 2>&1
RC=$?
set -e
head -n5 "$WORK/version.out"
if [[ "$RC" -ne 0 ]]; then
    echo "--version rc=$RC (flag may be unsupported by the DPF standalone); trying --help"
    timeout 20 "$WRAPPER" --help 2>&1 | head -n10 || true
fi
MISSING="$(ldd "$CARDINAL_PREFIX/bin/Cardinal" 2>/dev/null | grep 'not found' || true)"
if [[ -n "$MISSING" ]]; then
    echo "MISSING LIBS:"; echo "$MISSING"; exit 1
fi
file "$CARDINAL_PREFIX/bin/Cardinal" || true
echo "CARDINAL-INSTALLED  (binary: $CARDINAL_PREFIX/bin/Cardinal, wrapper: $WRAPPER)"

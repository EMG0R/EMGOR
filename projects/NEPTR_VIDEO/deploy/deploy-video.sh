#!/bin/bash
#
# NEPTR VIDEO — deploy Mac -> Pi.
#
#   ./deploy/deploy-video.sh                 # default host
#   ./deploy/deploy-video.sh pi@otherbox     # somewhere else
#   NV_DEST=~/video ./deploy/deploy-video.sh # somewhere else on the Pi
#
# Syncs the source, configures if needed, builds, and runs the test suites.
# Idempotent: run it as often as you like. The build directory is NOT synced
# and NOT deleted -- CMake object files are host-specific, and wiping them
# every deploy would turn a two-second incremental build into a two-minute one.
#
# It deliberately does NOT start, install or enable anything. Taking the
# display is a decision with a consequence (see deploy/README.md: only one
# process can hold DRM master), so it stays a thing a human does on purpose.
#
set -u

HOST="${1:-gorpi7@demiurge}"
DEST="${NV_DEST:-NEPTR_VIDEO}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

say() { echo "[deploy] $*"; }
die() { echo "[deploy] FAILED: $*" >&2; exit 1; }

say "source $SRC"
say "target $HOST:~/$DEST"

ssh -o ConnectTimeout=10 "$HOST" true || die "cannot ssh to $HOST"

# ── build dependencies ──────────────────────────────────────────────────────
# Checked before installing so the common case (already there) costs one ssh
# and never touches apt.
say "checking build dependencies"
MISSING=$(ssh "$HOST" 'for p in cmake g++ libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev; do
    dpkg -s "$p" >/dev/null 2>&1 || printf "%s " "$p"
done')
if [ -n "${MISSING// /}" ]; then
    say "installing: $MISSING"
    ssh -t "$HOST" "sudo apt-get install -y $MISSING" || die "dependency install"
else
    say "dependencies present"
fi

# ── source ──────────────────────────────────────────────────────────────────
# --delete so a file removed on the Mac is removed on the Pi: a stale source
# file that still compiles is the worst kind of deploy bug. build/ is excluded
# from the delete as well as the copy, or --delete would remove it every run.
say "syncing source"
rsync -a --delete \
    --exclude 'build/' --exclude '.git/' --exclude '.cache/' \
    "$SRC/" "$HOST:$DEST/" || die "rsync"

# ── build ───────────────────────────────────────────────────────────────────
say "building"
ssh "$HOST" "cd '$DEST' && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build -j\$(nproc)" \
    || die "build"

say "running tests"
ssh "$HOST" "cd '$DEST' && ctest --test-dir build --output-on-failure" || die "tests"

cat <<EOF

[deploy] done.

  Run it (the display must be free -- see deploy/README.md):
    ssh $HOST
    sudo systemctl stop neptr-ui
    cd $DEST && ./build/neptr-video
    sudo systemctl start neptr-ui     # hand the display back

  Measure instead of watching:
    NV_SELFTEST=1 ./build/neptr-video
EOF

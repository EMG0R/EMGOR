#!/bin/bash
# Builds a distributable tarball of omniplex-cli FOR a Raspberry Pi, run ON
# that Raspberry Pi (not cross-compiled from macOS).
#
# Why build on-device instead of cross-compiling: cpal/midir/serialport
# link against system C libraries (ALSA, udev, libasound2-dev) that need
# to match the target's actual sysroot. Cross-compiling that from macOS
# to aarch64-linux-gnu means hand-assembling a matching sysroot, which is
# far more fragile than just letting the Pi compile itself -- it has the
# spare cycles and it's the simplest thing that reliably works.
#
# Usage (on the Pi): ./packaging/pi/build.sh
# Output: dist/omniplex-<version>-linux-<arch>.tar.gz

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "error: this script is meant to run ON the Raspberry Pi (Linux), not cross-compiled." >&2
    exit 1
fi

ARCH="$(uname -m)"
VERSION="$(grep -m1 '^version' Cargo.toml | sed -E 's/version = "(.*)"/\1/')"

echo "==> Checking system dependencies (ALSA, udev headers)"
MISSING=()
dpkg -s libasound2-dev >/dev/null 2>&1 || MISSING+=("libasound2-dev")
dpkg -s libudev-dev >/dev/null 2>&1 || MISSING+=("libudev-dev")
if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo "Installing missing packages: ${MISSING[*]}"
    sudo apt-get update -y
    sudo apt-get install -y "${MISSING[@]}"
fi

if ! command -v cargo >/dev/null 2>&1; then
    echo "error: cargo not found. Install Rust first:" >&2
    echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y" >&2
    echo "  source \$HOME/.cargo/env" >&2
    exit 1
fi

echo "==> Building omniplex-cli release binary (v$VERSION, $ARCH) on-device"
cargo build --release -p omniplex-cli

BIN="$REPO_ROOT/target/release/omniplex"
if [[ ! -x "$BIN" ]]; then
    echo "error: expected release binary at $BIN, not found" >&2
    exit 1
fi

STAGE="$REPO_ROOT/target/pi-package/omniplex-$VERSION-linux-$ARCH"
DIST_DIR="$REPO_ROOT/dist"
rm -rf "$REPO_ROOT/target/pi-package"
mkdir -p "$STAGE/bin" "$DIST_DIR"

cp "$BIN" "$STAGE/bin/omniplex"
cp "$SCRIPT_DIR/install.sh" "$STAGE/install.sh"
cp "$SCRIPT_DIR/uninstall.sh" "$STAGE/uninstall.sh"
cp "$SCRIPT_DIR/omniplex.service" "$STAGE/omniplex.service"
chmod 755 "$STAGE/bin/omniplex" "$STAGE/install.sh" "$STAGE/uninstall.sh"

OUT_TAR="$DIST_DIR/omniplex-$VERSION-linux-$ARCH.tar.gz"
tar -C "$REPO_ROOT/target/pi-package" -czf "$OUT_TAR" "omniplex-$VERSION-linux-$ARCH"

echo ""
echo "==> Done: $OUT_TAR"
echo ""
echo "On the target Pi: tar xzf omniplex-$VERSION-linux-$ARCH.tar.gz && cd omniplex-$VERSION-linux-$ARCH && ./install.sh"

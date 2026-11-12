#!/bin/bash
# rnbo-build-deps.sh — RNBO integration, Phase 0 / Step 5a.
#
# Installs the toolchain to build rnbo.oscquery.runner FROM SOURCE with
# on-device compile enabled (SUPPORT_COMPILE=On -> -DRNBOOSCQUERY_ENABLE_COMPILE).
# The prebuilt apt package ships compile DISABLED, so a source build is the
# only way to get the standard on-device-compile behavior.
#
# Run as the normal user (it calls sudo for apt, pip --user for conan):
#   bash rnbo-build-deps.sh
#
# UNDO: apt remove the -dev packages if desired; pip uninstall conan.
set -euo pipefail

echo "=== apt build dependencies (sudo) ==="
sudo apt-get install -y \
    libavahi-compat-libdnssd-dev build-essential libssl-dev libjack-jackd2-dev \
    libdbus-1-dev libxml2-dev libgmock-dev libsdbus-c++-dev libsndfile1-dev \
    cmake ccache git python3-pip

echo "=== conan 1.61.0 (user-level, C74's pinned version) ==="
pip3 install --break-system-packages --user "conan==1.66.0"
export PATH="$HOME/.local/bin:$PATH"
hash -r
conan --version

echo "=== conan default profile (detect, then pin c++20 / libstdc++11) ==="
conan profile new default --detect --force || true
conan profile update settings.compiler.libcxx=libstdc++11 default || true
conan profile update settings.compiler.cppstd=20 default || true
echo "--- profile ---"
conan profile show default || true

echo ""
echo "STEP 5a done. NOTE: if conan flags GCC 14 as unknown, that's expected on"
echo "Trixie — we patch conan's settings.yml in the next step."
echo 'Ensure ~/.local/bin is on PATH:  export PATH="$HOME/.local/bin:$PATH"'

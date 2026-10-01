#!/bin/bash
# build-on-pi.sh — build and install csound-nam.so on Raspberry Pi 5.
# run from the directory containing CMakeLists.txt.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== csound-nam build ==="
echo "Platform: $(uname -m)"

# ---------------------------------------------------------------------------
# clone NeuralAmpModelerCore + init its submodules if not present
# ---------------------------------------------------------------------------
if [ ! -f "libs/NeuralAmpModelerCore/NAM/dsp.h" ]; then
    echo "--- Cloning NeuralAmpModelerCore..."
    mkdir -p libs
    git clone --depth 1 https://github.com/sdatkinson/NeuralAmpModelerCore.git \
        libs/NeuralAmpModelerCore
fi

if [ ! -f "libs/NeuralAmpModelerCore/Dependencies/eigen/Eigen/Dense" ]; then
    echo "--- Initialising Eigen submodule..."
    git -C libs/NeuralAmpModelerCore submodule update --init --depth 1 \
        Dependencies/eigen
fi

if [ ! -d "libs/NeuralAmpModelerCore/Dependencies/AudioDSPTools" ] || \
   [ -z "$(ls -A libs/NeuralAmpModelerCore/Dependencies/AudioDSPTools 2>/dev/null)" ]; then
    echo "--- Initialising AudioDSPTools submodule..."
    git -C libs/NeuralAmpModelerCore submodule update --init --depth 1 \
        Dependencies/AudioDSPTools
fi

# ---------------------------------------------------------------------------
# build dependencies (skip if already installed — allows non-TTY SSH runs)
# ---------------------------------------------------------------------------
MISSING_PKGS=""
for PKG in cmake build-essential libcsound64-dev; do
    dpkg -s "$PKG" >/dev/null 2>&1 || MISSING_PKGS="$MISSING_PKGS $PKG"
done
if [ -n "$MISSING_PKGS" ]; then
    echo "--- Installing:$MISSING_PKGS"
    sudo apt-get install -y --no-install-recommends $MISSING_PKGS
else
    echo "--- Build dependencies already installed"
fi

# ---------------------------------------------------------------------------
# verify C++20 atomic<shared_ptr<T>> is available (required by slimmable.h)
# ---------------------------------------------------------------------------
cat > /tmp/nam_cxx20_test.cpp << 'TESTEOF'
#include <memory>
#include <atomic>
std::atomic<std::shared_ptr<int>> x;
int main() { return 0; }
TESTEOF
if ! g++ -std=c++20 /tmp/nam_cxx20_test.cpp -o /tmp/nam_cxx20_test 2>/tmp/nam_cxx20_test.err; then
    echo "ERROR: C++20 atomic<shared_ptr<T>> not available with current g++"
    cat /tmp/nam_cxx20_test.err
    g++ --version
    exit 1
fi
rm -f /tmp/nam_cxx20_test /tmp/nam_cxx20_test.cpp /tmp/nam_cxx20_test.err
echo "--- C++20 atomic<shared_ptr<T>>: OK"

# ---------------------------------------------------------------------------
# configure and build
# ---------------------------------------------------------------------------
# wipe any stale CMakeCache (e.g. a build dir copied from another machine).
rm -rf build

echo "--- Configuring..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "--- Building ($(nproc) cores)..."
cmake --build build -j"$(nproc)"

# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------
echo "--- Installing plugin..."
sudo cmake --install build

echo ""
echo "=== Done ==="
PLUGIN=$(find /usr/local/lib/csound -name 'csound-nam.so' 2>/dev/null | head -1)
echo "Plugin: ${PLUGIN:-NOT FOUND}"
echo "Test:   cd $(pwd) && csound --opcode-lib=build/csound-nam.so test/test_nam_passthrough.csd"

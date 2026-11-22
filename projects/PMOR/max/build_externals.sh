#!/usr/bin/env bash
# build_externals.sh [WORKDIR] -- builds pmor.core~.mxo and pmor.proto.mxo (arm64) with Min-DevKit and installs them to ~/Documents/Max 9/Library.
# WORKDIR (default /tmp/pmor_ext) holds min-devkit + a PMOR_V3 symlink to this repo (the CMake files expect PMOR_V3 beside min-devkit).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-/tmp/pmor_ext}"; mkdir -p "$W"
ln -sfn "$here" "$W/PMOR_V3"
[ -d "$W/min-devkit" ] || git clone -q --depth 1 --recurse-submodules --shallow-submodules https://github.com/Cycling74/min-devkit.git "$W/min-devkit"
for n in pmor.core_tilde pmor.proto; do mkdir -p "$W/min-devkit/source/projects/$n"; cp "$here/max/$n/"* "$W/min-devkit/source/projects/$n/"; done
mkdir -p "$W/min-devkit/build"; cd "$W/min-devkit/build"
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release .. > cfg.log 2>&1
cmake --build . --target pmor.core_tilde pmor.proto -j8 > build.log 2>&1 || { tail -30 build.log; exit 1; }
L="$HOME/Documents/Max 9/Library"; mkdir -p "$L"
for x in pmor.core~.mxo pmor.proto.mxo; do rm -rf "$L/$x"; cp -R "$W/min-devkit/externals/$x" "$L/$x"; done
echo "installed pmor.core~.mxo + pmor.proto.mxo -> $L"

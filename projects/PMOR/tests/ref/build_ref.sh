#!/usr/bin/env bash
# Builds the REFERENCE renderer: the ORIGINAL pre-extraction engine sources (git commit $1, default ff86e4c),
# unmodified, compiled on the host with hardware stubs. Output: $2/render_ref (default ~/pmor-deps/ref_build).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; repo="$here/../.."
commit="${1:-ff86e4c}"; out="${2:-$HOME/pmor-deps/ref_build}"
rm -rf "$out"; mkdir -p "$out/src"
git -C "$repo" archive "$commit" engine config.h usb; cp "$repo/pmor_core/third_party/demiurge-transport.h" "$out/src/" | tar -x -C "$out/src"
mkdir -p "$out/src/hw"; cp "$here/stubs/hw/hw.h" "$out/src/hw/hw.h"
cd "$out/src"
# the ONLY edit to the original sources: ARM32 "dmb" barrier -> host-assemblable form (single-threaded, no value effect)
sed -i 's/__asm volatile("dmb" ::: "memory")/__asm volatile("" ::: "memory")/' engine/clock.cpp
srcs=$(ls engine/*.cpp | grep -v -e engine/transport_link.cpp)
nice -n19 taskset -c 0-2 g++ -std=gnu++14 -O2 -ffp-contract=off -fno-fast-math -w -DPMOR_REF \
   -I"$here/stubs" -I. -Iengine -Iusb -Itests_inc -I"$here/../../pmor_core/third_party" -I"$here/.." \
   $srcs "$here/stubs/ref_stubs.cpp" "$here/../scenario.cpp" "$here/../render_main.cpp" -o "$out/render_ref"
echo "built $out/render_ref from $commit"

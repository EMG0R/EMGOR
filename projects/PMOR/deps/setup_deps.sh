#!/usr/bin/env bash
# setup_deps.sh [DIR] : recreate the PMOR_V3 firmware build dependencies exactly as on the Mac (deps/DEPS_mac.txt).
# DIR defaults to $PMOR_DEPS or ~/pmor-deps. Idempotent: existing checkouts are left alone.
#   libDaisy 85172e2b (+local patch), DaisySP a0494a3a (+LGPL submodule), TinyUSB e88fc441 (+local patch),
#   ARM GCC 10.3-2021.10 (the Mac's toolchain; aarch64-linux host build for the Pi) -> DIR/tc
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
D="${1:-${PMOR_DEPS:-$HOME/pmor-deps}}"; mkdir -p "$D"; cd "$D"
if [ ! -d libDaisy ]; then
  git clone -q https://github.com/electro-smith/libDaisy; git -C libDaisy checkout -q 85172e2b5c9abea1bf56dcd5d2c7b3f5717eb66b
  git -C libDaisy apply "$here/libDaisy-85172e2b-local.patch"; git -C libDaisy submodule update --init --depth 1
fi
if [ ! -d DaisySP ]; then
  git clone -q https://github.com/electro-smith/DaisySP; git -C DaisySP checkout -q a0494a3adb67f549e18dfd71a35fa656f65b38b6
  git -C DaisySP submodule update --init
fi
if [ ! -d tinyusb ]; then
  git clone -q https://github.com/hathach/tinyusb; git -C tinyusb checkout -q e88fc441ddcaaa5abd4f4673ef2bf29499522dc0
  git -C tinyusb apply "$here/tinyusb-e88fc441-local.patch"
fi
tc="$D/tc/gcc-arm-none-eabi-10.3-2021.10"
if [ ! -x "$tc/bin/arm-none-eabi-gcc" ]; then
  mkdir -p "$D/tc"; arch="$(uname -m)"
  case "$arch" in aarch64) pkg=aarch64-linux;; x86_64) pkg=x86_64-linux;; *) echo "no prebuilt toolchain for $arch"; exit 1;; esac
  curl -L -C - -o "$D/tc/t.tar.bz2" "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-$pkg.tar.bz2"
  bzip2 -t "$D/tc/t.tar.bz2"; tar -xjf "$D/tc/t.tar.bz2" -C "$D/tc"
fi
export PATH="$tc/bin:$PATH"
[ -f libDaisy/build/libdaisy.a ]  || make -C libDaisy -j3 >/dev/null
[ -f DaisySP/build/libdaisysp.a ] || make -C DaisySP -j3 >/dev/null
echo "deps ready in $D ($(arm-none-eabi-gcc --version | head -1))"

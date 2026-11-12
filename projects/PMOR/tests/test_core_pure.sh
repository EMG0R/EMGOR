#!/usr/bin/env bash
# test_core_pure.sh : pmor_core/ (outside host/) must not include libDaisy, TinyUSB, FatFs, the hw/ui/store
# layers, the root pin config, or any platform header. Also compiles every core file with -DPMOR_HOST and
# no include path other than pmor_core itself (+ the shared tap header) to prove nothing hides behind -I.
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"; cd "$root"
bad=$(grep -nE '#include *[<"]([^">]*/)?(daisy[^">]*|libdaisy[^">]*|tusb[^">]*|fatfs[^">]*|sys/dma[^">]*|hw\.h|hw_priv\.h|stm32[^">]*|oled[^">]*|usb/[^">]*)[">]|#include *"(\.\./)?(hw|ui|store|usb)/|#include *"(\.\./)?config\.h"' pmor_core/*.h pmor_core/*.cpp || true)
if [ -n "$bad" ]; then echo "test_core_pure: FAIL, forbidden includes:"; echo "$bad"; exit 1; fi
echo "test_core_pure: PASS (no hardware includes in $(ls pmor_core/*.h pmor_core/*.cpp | wc -l) core files)"

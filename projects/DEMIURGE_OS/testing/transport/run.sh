#!/usr/bin/env bash
# Host-only measurement of src/demiurge-transport.h. No ALSA, no audio, no hardware.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cc -std=c99 -Wall -Wextra -Werror -O2 -I "$here/../../src" -o "$tmp/test_transport" "$here/test_transport.c" -lm
# the same header must also be valid C++ (clock.cpp and demiurge-clock.cpp are C++)
printf '#include "demiurge-transport.h"\nint main(){dt_tap_t s;dt_tap_reset(&s);double b;return dt_tap_feed(&s,0,&b)==DT_TAP_ARMED?0:1;}\n' > "$tmp/cpp.cpp"
c++ -std=c++17 -Wall -Wextra -Werror -I "$here/../../src" -o "$tmp/cpp_check" "$tmp/cpp.cpp"
"$tmp/cpp_check"
"$tmp/test_transport"
echo
echo "== daemon transport behaviour: the real demiurge-clock.cpp, ALSA stubbed, sends recorded =="
c++ -std=c++17 -O1 -Wall -Wextra -I "$here/../clock/stub" -I "$here/../../src" -include "$here/../clock/stub/compat.h" \
    -o "$tmp/test_daemon_tap" "$here/test_daemon_tap.cpp" -lpthread
"$tmp/test_daemon_tap"

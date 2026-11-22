#!/bin/bash
# deploy-rtjack.sh — install the patched librtjack.so (run via sudo on the Pi).
set -e
# The patched plugin ships in this checkout (config/csound/librtjack.so), so
# resolve it relative to this script: no home-directory or user name involved
# (the old /home/$PI_USER/_______DEMIURGE path did not exist on the rebuilt rig).
DST=/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/librtjack.so
SRC="$(cd "$(dirname "$0")" && pwd)/../config/csound/librtjack.so"
test -f "$SRC" || { echo "missing $SRC (run build-rtjack-fix.sh first)"; exit 1; }
[ -f "$DST.orig" ] || cp -a "$DST" "$DST.orig"
install -m0644 "$SRC" "$DST"
echo "INSTALLED patched librtjack.so (orig at $DST.orig)"

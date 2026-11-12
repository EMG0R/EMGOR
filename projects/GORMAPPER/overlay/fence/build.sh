#!/bin/sh
# Build the GORMAPPER cursor-fence helper. Needs the Xcode command line tools
# (swiftc), no Xcode project. Produces overlay/fence/fence.
set -e
cd "$(dirname "$0")"
swiftc -O fence.swift -o fence
echo "built: $(pwd)/fence"
echo "note: the fence needs a one-time Accessibility permission on first run"
echo "      (System Settings → Privacy & Security → Accessibility)"

#!/bin/bash
# Build a launchable TheWatch.app bundle from the SwiftPM executable.
#
#   ./make-app.sh            # release build -> ./TheWatch.app
#   ./make-app.sh debug      # debug build
#
# The bundle is what you double-click / add to Login Items. macOS reads the
# on-disk Info.plist (LSUIElement + TCC usage strings) from the bundle.
set -euo pipefail
cd "$(dirname "$0")"

CONFIG="${1:-release}"
APP="TheWatch.app"
BIN_NAME="TheWatchMac"

echo "==> swift build -c $CONFIG"
swift build -c "$CONFIG"
BIN_PATH="$(swift build -c "$CONFIG" --show-bin-path)/$BIN_NAME"

echo "==> assembling $APP"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
cp "$BIN_PATH" "$APP/Contents/MacOS/$BIN_NAME"
cp "Resources/Info.plist" "$APP/Contents/Info.plist"

# Ad-hoc sign so TCC (Bluetooth / Automation / Accessibility) can attribute
# grants to a stable code identity. Replace "-" with a real Developer ID to
# distribute.
echo "==> codesign (ad-hoc)"
codesign --force --deep --sign - \
  --identifier "com.emgor.thewatch.mac" \
  "$APP" 2>/dev/null || echo "   (codesign skipped/failed — app still launches)"

echo "==> done: $(pwd)/$APP"
echo "    launch with:  open $APP     (or add it to Login Items)"

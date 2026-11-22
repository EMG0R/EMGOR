#!/bin/bash
# GOR_CONTROL_PANEL.command
# Double-click launcher for the portable/remote GOR Lab Control Panel.
# Picks a Tk-capable Python (macOS system python3 can hard-panic on Tk 8.5,
# so we prefer a python.org / Homebrew build with Tk 8.6+), then runs the app.

cd "$(dirname "$0")" || exit 1
APP="gor_control_panel.py"

# Candidate interpreters, in preference order.
CANDIDATES=(
  "/Library/Frameworks/Python.framework/Versions/3.14/bin/python3"
  "/Library/Frameworks/Python.framework/Versions/3.13/bin/python3"
  "/Library/Frameworks/Python.framework/Versions/3.12/bin/python3"
  "/opt/homebrew/bin/python3.11"
  "/usr/local/bin/python3"
  "python3"
  "/usr/bin/python3"
)

pick=""
pick_tk=""
for c in "${CANDIDATES[@]}"; do
  command -v "$c" >/dev/null 2>&1 || [ -x "$c" ] || continue
  ver="$("$c" -c 'import tkinter,sys; sys.stdout.write(str(tkinter.TkVersion))' 2>/dev/null)"
  if [ -n "$ver" ]; then
    # Prefer the first interpreter whose Tk is >= 8.6 (avoids the 8.5 panic).
    if [ "$ver" = "8.6" ] || [ "$ver" = "8.7" ] || [ "$ver" = "9.0" ]; then
      pick="$c"; pick_tk="$ver"; break
    fi
    # remember the first Tk-capable one as a fallback
    if [ -z "$pick" ]; then pick="$c"; pick_tk="$ver"; fi
  fi
done

if [ -z "$pick" ]; then
  echo ""
  echo "  No Tk-capable Python was found on this Mac."
  echo "  tkinter is required. Fix with ONE of:"
  echo "    - Install python.org's macOS build (ships Tk 8.6), or"
  echo "    - brew install python-tk"
  echo ""
  echo "  Press any key to close."
  read -r -n 1 _
  exit 1
fi

echo "Launching GOR Control Panel with: $pick  (Tk $pick_tk)"
exec "$pick" "$APP"

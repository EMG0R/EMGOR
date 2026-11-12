#!/bin/sh
# oFOOTS v2 bench monitor. Web MIDI needs a secure context; localhost counts.
cd "$(dirname "$0")" || exit 1
echo "oFOOTS v2 bench monitor -> http://localhost:${1:-8123}/"
exec python3 -m http.server "${1:-8123}"

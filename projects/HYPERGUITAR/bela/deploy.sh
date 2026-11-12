#!/bin/bash
# Deploy + run hyperGuitar6chPan on the Bela Gem Multi.
# Usage: ./deploy.sh [bela-host]   (default: bela.local, falls back to 192.168.7.2 / 192.168.6.2)
set -e

PROJECT=hyperGuitar6chPan
SRC="$(cd "$(dirname "$0")" && pwd)/$PROJECT"

find_host() {
	for h in "$1" bela.local 192.168.7.2 192.168.6.2; do
		[ -z "$h" ] && continue
		if ssh -o ConnectTimeout=4 -o StrictHostKeyChecking=accept-new "root@$h" true 2>/dev/null; then
			echo "$h"
			return 0
		fi
	done
	return 1
}

HOST=$(find_host "$1") || { echo "Can't reach the Bela. Is it plugged in over USB?"; exit 1; }
echo "Bela found at $HOST"

ssh "root@$HOST" "mkdir -p ~/Bela/projects/$PROJECT"
scp "$SRC"/*.cpp "$SRC"/*.h "root@$HOST:~/Bela/projects/$PROJECT/"

SUP=/root/hyperGuitar-supervisor.sh
echo "Building (supervisor-aware: won't race the auto-relaunch loop)..."
# Stop the supervisor loop FIRST, or it respawns the old binary the instant
# `make stop` kills it - two processes then fight over the audio device.
ssh "root@$HOST" "pkill -f $SUP 2>/dev/null; make -C ~/Bela stop 2>/dev/null; sleep 1; make -C ~/Bela PROJECT=$PROJECT CL='-p 64' Bela"
echo "Build done. Relaunching under the supervisor..."
ssh "root@$HOST" "test -x $SUP && (setsid nohup $SUP >/dev/null 2>&1 </dev/null &) || echo 'no supervisor installed - run make run manually'"
sleep 2
ssh "root@$HOST" "ps aux | grep -E 'supervisor|$PROJECT -J' | grep -v grep"

#!/bin/bash
# hyperguitar_1v2 — phase 2 of the SD rebuild. Run from the Mac once the Bela
# has booted the freshly flashed stock Bela Gem image (bela-gem.img.xz, v1.0.2)
# and shows up over USB (root@192.168.7.2 / bela.local).
#
#   bash bela/sdcard/hyperguitar_1v2/restore.sh [host]
#
# Idempotent: safe to rerun. Puts the card in the exact state the dead card
# was in on 2026-11-03 (commit b305164): project source, supervisor loop +
# systemd unit, bela_startup disabled, USB gadget script with UAC2 audio,
# usbaudio-bridge built + installed, mix.conf with EMGOR's dialled values,
# clock set from the Mac, project built with -p 64, supervisor started.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BELA="$(cd "$HERE/../.." && pwd)"               # repo bela/ dir
PROJECT=hyperGuitar6chPan
SRC="$BELA/$PROJECT"
SSH="ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
SCP="scp -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"

find_host() {
	for h in "$1" 192.168.7.2 bela.local 192.168.6.2; do
		[ -z "$h" ] && continue
		if $SSH "root@$h" true 2>/dev/null; then echo "$h"; return 0; fi
	done
	return 1
}
echo "[1/9] waiting for the Bela over USB..."
for i in $(seq 1 60); do
	HOST=$(find_host "$1") && break
	sleep 2
done
[ -n "${HOST:-}" ] || { echo "Bela not reachable after 120 s. Card in, USB to the Mac, blue LEDs on?"; exit 1; }
echo "      Bela at $HOST"
R="root@$HOST"

echo "[2/9] clock (Bela has no RTC; the old card showed Oct 01)"
$SSH "$R" "date -s '$(date -u '+%Y-%m-%d %H:%M:%S')' >/dev/null; timedatectl set-ntp true 2>/dev/null || true"

echo "[3/9] project source -> ~/Bela/projects/$PROJECT"
$SSH "$R" "mkdir -p ~/Bela/projects/$PROJECT"
$SCP "$SRC"/*.cpp "$SRC"/*.h "$R:~/Bela/projects/$PROJECT/"
$SCP "$BELA/device-backup/hyperGuitar6chPan-device-copy/settings.json" "$R:~/Bela/projects/$PROJECT/settings.json"
$SCP "$HERE/hyperguitar-mix.conf" "$R:~/Bela/projects/$PROJECT/hyperguitar-mix.conf"

echo "[4/9] supervisor loop + unit; bela_startup disabled"
$SCP "$BELA/hyperGuitar-supervisor.sh" "$R:/root/hyperGuitar-supervisor.sh"
$SCP "$HERE/hyperguitar-supervisor.service" "$R:/etc/systemd/system/hyperguitar-supervisor.service"
$SSH "$R" "chmod +x /root/hyperGuitar-supervisor.sh; systemctl disable --now bela_startup.service 2>/dev/null || true; systemctl daemon-reload; systemctl enable hyperguitar-supervisor.service"

echo "[5/9] USB gadget script with UAC2 audio function (from device-backup, 2026-07-26)"
$SSH "$R" "cp -n /usr/bin/bela-start-acm-ncm-rndis-old-gadget /usr/bin/bela-start-acm-ncm-rndis-old-gadget.stock 2>/dev/null || true"
$SCP "$BELA/device-backup/bela-start-acm-ncm-rndis-old-gadget" "$R:/usr/bin/bela-start-acm-ncm-rndis-old-gadget"
$SCP "$BELA/device-backup/bela-start-usb-gadgets" "$R:/usr/bin/bela-start-usb-gadgets"
$SSH "$R" "chmod +x /usr/bin/bela-start-acm-ncm-rndis-old-gadget /usr/bin/bela-start-usb-gadgets"

echo "[6/9] usbaudio-bridge: build from bridge.c, install unit"
$SSH "$R" "mkdir -p /root/usbaudio-bridge"
$SCP "$BELA/usbaudio-bridge/bridge.c" "$R:/root/usbaudio-bridge/bridge.c"
$SCP "$BELA/usbaudio-bridge/usbaudio-bridge.service" "$R:/etc/systemd/system/usbaudio-bridge.service"
$SSH "$R" "cd /root/usbaudio-bridge && gcc -O2 -o /usr/bin/usbaudio-bridge bridge.c -lasound -lrt -lpthread && systemctl daemon-reload && systemctl enable usbaudio-bridge.service"

echo "[7/9] build $PROJECT (-p 64) — a few minutes on first build"
$SSH "$R" "make -C ~/Bela stop 2>/dev/null; make -C ~/Bela PROJECT=$PROJECT CL='-p 64' Bela 2>&1 | tail -3"

echo "[8/9] card identity marker"
$SSH "$R" "echo 'hyperguitar_1v2 — flashed $(date '+%Y-%m-%d') from bela-gem.img.xz v1.0.2 + hyperGuitar commit $(git -C "$BELA" rev-parse --short HEAD 2>/dev/null)' > /root/HYPERGUITAR_CARD.txt"

echo "[9/9] start the supervisor, verify"
$SSH "$R" "systemctl start hyperguitar-supervisor.service; sleep 6; systemctl is-active hyperguitar-supervisor.service; pgrep -fa '^\./hyperGuitar6chPan' || echo 'process not up yet'; tail -3 /root/hyperGuitar-live.log 2>/dev/null"

echo
echo "Mac == Bela parity check:"
for f in "$SRC"/*.cpp "$SRC"/*.h; do
	b=$(basename "$f")
	m=$(md5 -q "$f"); d=$($SSH "$R" "md5sum ~/Bela/projects/$PROJECT/$b | cut -d' ' -f1")
	[ "$m" = "$d" ] || echo "  MISMATCH $b"
done
echo "done. Unplug/replug the USB once so the Mac re-enumerates the UAC2 audio device."

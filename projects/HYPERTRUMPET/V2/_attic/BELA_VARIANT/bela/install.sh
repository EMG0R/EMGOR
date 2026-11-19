#!/bin/bash
# HYPERTRUMPET V2 - Bela Gem Stereo boot/USB install. Run from the Mac AFTER the
# freshly flashed Gem image has booted and answers on root@192.168.7.2.
#   bash install.sh [--host H] [--hostname NAME] [--project-dir DIR] [--no-build] [--start]
# Idempotent: every step compares before writing; reruns change nothing.
# NOT RUN YET: the board is not flashed. Does NOT restart the gadget (that
# would drop the ssh link) and does NOT start the instrument unless --start.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
HOST=192.168.7.2; NEWNAME=""; PROJDIR=""; BUILD=1; START=0
while [ $# -gt 0 ]; do case "$1" in
	--host) HOST=$2; shift;; --hostname) NEWNAME=$2; shift;;
	--project-dir) PROJDIR=$2; shift;; --no-build) BUILD=0;; --start) START=1;;
	*) echo "unknown arg $1"; exit 2;; esac; shift; done
[ -z "$PROJDIR" ] && [ -d "$HERE/hyperTrumpet" ] && PROJDIR="$HERE/hyperTrumpet"
SSHO="-o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
R="root@$HOST"
echo "[1/8] preflight $HOST"
for i in $(seq 1 30); do ssh $SSHO "$R" true 2>/dev/null && break; sleep 2; done
ssh $SSHO "$R" true || { echo "no ssh to $R"; exit 1; }
ssh $SSHO "$R" 'echo "model: $(tr -d "\0" </proc/device-tree/model 2>/dev/null)"; uname -r; echo "kernel gadget cfg: $(ls /sys/kernel/config/usb_gadget 2>&1|tr "\n" " ")"; aplay -l 2>&1 | head -8; command -v gcc >/dev/null || { echo "FATAL: gcc missing"; exit 1; }; test -d /root/Bela || { echo "FATAL: /root/Bela missing - not a Bela image?"; exit 1; }'
echo "      ^ CHECK: a Gem Stereo card should list 2 in / 2 out (arecord -l / Bela IDE). If the log says 'unrecognized Bela hardware' the cape is not seated."

echo "[2/8] stage files"
ssh $SSHO "$R" 'rm -rf /root/ht-stage && mkdir -p /root/ht-stage'
tar -C "$HERE" -cf - hypertrumpet.env hyperTrumpet-supervisor.sh hypertrumpet-supervisor.service \
	usbaudio-bridge usb-gadget set-usb-subnet.sh verify.sh | ssh $SSHO "$R" 'tar -C /root/ht-stage -xf -'

echo "[3/8] apply on device"
ssh $SSHO "$R" NEWNAME="$NEWNAME" bash -s <<'REMOTE'
set -eu
S=/root/ht-stage; CH=0
put() { # put SRC DEST MODE  -> install only if different
	if [ -f "$2" ] && cmp -s "$1" "$2"; then echo "  = $2"; else install -D -m "$3" "$1" "$2"; echo "  + $2"; CH=1; fi; }
echo "-- env (never overwrite an edited file)"
[ -f /etc/default/hypertrumpet ] && echo "  = /etc/default/hypertrumpet (kept)" || { install -m 644 $S/hypertrumpet.env /etc/default/hypertrumpet; echo "  + /etc/default/hypertrumpet"; }
echo "-- USB gadget scripts (stock backed up once as *.stock)"
for f in bela-start-acm-ncm-rndis-old-gadget bela-start-usb-gadgets; do
	[ -f /usr/bin/$f ] && [ ! -f /usr/bin/$f.stock ] && cp -p /usr/bin/$f /usr/bin/$f.stock && echo "  stock saved: $f.stock"
done
# Sanity: warn if the stock gadget script on THIS image looks structurally different
# from the one these were derived from (hyperGuitar's, 2026-03-25 Gem image).
if [ -f /usr/bin/bela-start-acm-ncm-rndis-old-gadget.stock ]; then
	n=$(diff /usr/bin/bela-start-acm-ncm-rndis-old-gadget.stock $S/usb-gadget/bela-start-acm-ncm-rndis-old-gadget | grep -c '^[<>]' || true)
	echo "  diff lines stock vs ours: $n (expected ~25 for the hyperGuitar-era image; far more = review before reboot)"
fi
put $S/usb-gadget/bela-start-acm-ncm-rndis-old-gadget /usr/bin/bela-start-acm-ncm-rndis-old-gadget 755
put $S/usb-gadget/bela-start-usb-gadgets /usr/bin/bela-start-usb-gadgets 755
GADGET_CHANGED=$CH; CH=0
echo "-- bridge"
mkdir -p /root/usbaudio-bridge
put $S/usbaudio-bridge/bridge.c /root/usbaudio-bridge/bridge.c 644; B1=$CH; CH=0
put $S/usbaudio-bridge/hypertrumpet_usbring.h /root/usbaudio-bridge/hypertrumpet_usbring.h 644; B2=$CH; CH=0
if [ ! -x /usr/bin/usbaudio-bridge ] || [ $B1 = 1 ] || [ $B2 = 1 ]; then
	gcc -O2 -Wall -I/root/usbaudio-bridge -o /usr/bin/usbaudio-bridge /root/usbaudio-bridge/bridge.c -lasound -lrt -lpthread && echo "  built /usr/bin/usbaudio-bridge"
else echo "  = bridge binary current"; fi
put $S/usbaudio-bridge/usbaudio-bridge.service /etc/systemd/system/usbaudio-bridge.service 644
echo "-- supervisor"
put $S/hyperTrumpet-supervisor.sh /root/hyperTrumpet-supervisor.sh 755
put $S/hypertrumpet-supervisor.service /etc/systemd/system/hypertrumpet-supervisor.service 644
put $S/set-usb-subnet.sh /root/set-usb-subnet.sh 755
put $S/verify.sh /root/ht-verify.sh 755
mkdir -p /root/Bela/projects/hyperTrumpet
echo "-- Bela's own startup mechanism: OFF (supervisor owns boot)"
systemctl disable --now bela_startup.service 2>/dev/null || true
mkdir -p /opt/Bela; printf 'ACTIVE=0\n' > /opt/Bela/startup_env   # same content `make nostartup` writes
[ -e /opt/Bela/local/bela_startup.sh ] && echo "  WARNING: /opt/Bela/local/bela_startup.sh exists (stock override hook) - remove it, it duplicates the supervisor"
systemctl daemon-reload
systemctl enable usbaudio-bridge.service hypertrumpet-supervisor.service 2>&1 | sed 's/^/  /'
if [ -n "${NEWNAME:-}" ] && [ "$(hostname)" != "$NEWNAME" ]; then
	hostnamectl set-hostname "$NEWNAME"; sed -i "s/^127\.0\.1\.1.*/127.0.1.1\t$NEWNAME/" /etc/hosts
	grep -q "^127.0.1.1" /etc/hosts || printf '127.0.1.1\t%s\n' "$NEWNAME" >> /etc/hosts
	echo "  hostname -> $NEWNAME (mDNS name $NEWNAME.local after reboot)"
fi
[ -f /root/HYPERTRUMPET_CARD.txt ] || echo "hypertrumpet card: boot/USB infra installed $(date +%F)" > /root/HYPERTRUMPET_CARD.txt
sync
echo "GADGET_CHANGED=$GADGET_CHANGED"
REMOTE

echo "[4/8] project source"
if [ -n "$PROJDIR" ] && ls "$PROJDIR"/*.cpp >/dev/null 2>&1; then
	scp $SSHO "$PROJDIR"/*.cpp "$PROJDIR"/*.h "$R:/root/Bela/projects/hyperTrumpet/" 2>/dev/null || scp $SSHO "$PROJDIR"/*.cpp "$R:/root/Bela/projects/hyperTrumpet/"
	[ -f "$PROJDIR/settings.json" ] && scp $SSHO "$PROJDIR/settings.json" "$R:/root/Bela/projects/hyperTrumpet/"
	cmp -s "$HERE/usbaudio-bridge/hypertrumpet_usbring.h" "$PROJDIR/hypertrumpet_usbring.h" 2>/dev/null || echo "  WARNING: project's hypertrumpet_usbring.h differs from bridge's (or is missing) - ring layouts MUST match"
	if [ $BUILD = 1 ]; then
		echo "[5/8] build (-p per /etc/default/hypertrumpet)"
		ssh $SSHO "$R" '. /etc/default/hypertrumpet; make -C ~/Bela stop 2>/dev/null; make -C ~/Bela PROJECT=hyperTrumpet CL="-p ${HT_BLOCK:-64}" Bela 2>&1 | tail -4'
	fi
else
	echo "  no project dir with .cpp (pass --project-dir); infra only. Supervisor idles until /root/Bela/projects/hyperTrumpet/hyperTrumpet exists."
fi
echo "[6/8] bridge service"
ssh $SSHO "$R" 'systemctl start usbaudio-bridge.service; sleep 2; systemctl is-active usbaudio-bridge.service'
echo "[7/8] supervisor"
if [ $START = 1 ]; then ssh $SSHO "$R" 'systemctl start hypertrumpet-supervisor.service; sleep 5; systemctl is-active hypertrumpet-supervisor.service';
else echo "  not started (add --start, or reboot). Launching the instrument is your call."; fi
echo "[8/8] done. REBOOT the Bela once so the new gadget (4-ch UAC2, product string, hostname) is built, then run: ssh root@$HOST /root/ht-verify.sh"

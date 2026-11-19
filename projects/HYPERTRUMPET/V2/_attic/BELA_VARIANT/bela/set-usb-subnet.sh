#!/bin/bash
# Give the trumpet Bela its own USB-network subnet so it can sit on the same
# Mac as the hyperGuitar Bela (both default to 192.168.7.2 -> clash).
#   bash set-usb-subnet.sh [--host 192.168.7.2] [--subnet 9]            # report only
#   bash set-usb-subnet.sh [--host ...] [--subnet 9] --apply            # rewrite + backup
# STATUS: the mechanism that assigns 192.168.7.2 on the Gem (Debian 12) image
# was NOT verified from docs (older Bela images used /etc/network/interfaces,
# usb1=NCM 192.168.7.2, usb0=RNDIS 192.168.6.2). So this script FINDS the files
# containing 192.168.7. and lists them; --apply sed-replaces only those, after
# copying them to /root/ht-net-backup/. It does not reboot.
set -euo pipefail
HOST=192.168.7.2; SUB=9; APPLY=0
while [ $# -gt 0 ]; do case "$1" in --host) HOST=$2; shift;; --subnet) SUB=$2; shift;; --apply) APPLY=1;; esac; shift; done
SSHO="-o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
ssh $SSHO root@$HOST APPLY=$APPLY SUB=$SUB bash -s <<'REMOTE'
set -eu
FILES=$(grep -rIl '192\.168\.7\.' /etc/network /etc/systemd/network /etc/NetworkManager /etc/dnsmasq.d /etc/dnsmasq.conf /etc/dhcp /etc/default /opt/Bela /usr/bin/bela-* /usr/bin/bb-* /etc/avahi 2>/dev/null | grep -v '\.stock$' || true)
echo "files mentioning 192.168.7.x:"; echo "${FILES:-  (none found - mechanism unknown; check: ip -br a; nmcli con; networkctl)}"
for f in $FILES; do echo "--- $f"; grep -n '192\.168\.7\.' $f; done
ip -br addr | grep -E 'usb|rndis|ncm' || true
[ "$APPLY" = 1 ] && [ -n "$FILES" ] || { echo "(report only)"; exit 0; }
mkdir -p /root/ht-net-backup
for f in $FILES; do cp -p --parents $f /root/ht-net-backup/; sed -i "s/192\.168\.7\./192.168.$SUB./g" $f; done
sync; echo "applied: 192.168.7.x -> 192.168.$SUB.x. Backup in /root/ht-net-backup. REBOOT, then reconnect at 192.168.$SUB.2."
echo "Mac side: if the board runs a DHCP server the Mac picks 192.168.$SUB.1 itself; else set the new USB interface manually to 192.168.$SUB.1/24."
echo "If you lose the link: SD card in Mac can't edit ext4 rootfs; use the Bela's serial/ACM console or reflash + install.sh."
REMOTE

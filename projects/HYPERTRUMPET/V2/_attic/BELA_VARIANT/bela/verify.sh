#!/bin/bash
# Run ON the Bela (installed as /root/ht-verify.sh):  ssh root@192.168.7.2 /root/ht-verify.sh
# Read-only. Prints PASS/FAIL/INFO lines.
ok(){ echo "PASS  $*"; }; no(){ echo "FAIL  $*"; }; inf(){ echo "INFO  $*"; }
G=/sys/kernel/config/usb_gadget/g_multi
[ -d $G ] && ok "gadget g_multi exists" || no "no g_multi gadget"
[ -n "$(cat $G/UDC 2>/dev/null)" ] && ok "gadget bound to UDC $(cat $G/UDC)" || no "gadget not bound"
for f in rndis.usb0 ncm.usb0 acm.usb0 uac2.usb0; do [ -d $G/functions/$f ] && ok "function $f" || no "function $f missing"; done
inf "p_chmask=$(cat $G/functions/uac2.usb0/p_chmask 2>/dev/null) c_chmask=$(cat $G/functions/uac2.usb0/c_chmask 2>/dev/null) p_srate=$(cat $G/functions/uac2.usb0/p_srate 2>/dev/null)"
inf "product string: $(cat $G/strings/0x409/product 2>/dev/null)"
aplay -l 2>/dev/null | grep -qi UAC2Gadget && ok "ALSA card UAC2Gadget present" || no "no UAC2Gadget ALSA card"
inf "audio card(s):"; aplay -l 2>&1 | grep -v -i uac2 | sed 's/^/        /'
ip -br addr | grep -qE '192\.168\.[0-9]+\.2' && ok "usb net up: $(ip -br addr | grep -E '192\.168\.[0-9]+\.2' | head -1)" || no "no 192.168.x.2 address"
for u in usbaudio-bridge hypertrumpet-supervisor; do
	systemctl is-enabled $u >/dev/null 2>&1 && ok "$u enabled" || no "$u not enabled"
	systemctl is-active $u >/dev/null 2>&1 && ok "$u active" || inf "$u not active"
done
systemctl is-enabled bela_startup >/dev/null 2>&1 && no "bela_startup ENABLED (must be disabled)" || ok "bela_startup not enabled"
grep -q 'ACTIVE=0' /opt/Bela/startup_env 2>/dev/null && ok "startup_env ACTIVE=0" || inf "startup_env: $(cat /opt/Bela/startup_env 2>/dev/null)"
[ -e /dev/shm/hypertrumpet_usbaudio ] && ok "shm ring exists (instrument running)" || inf "no shm ring (instrument not running)"
inf "bridge log:"; journalctl -u usbaudio-bridge -n 6 --no-pager 2>/dev/null | sed 's/^/        /'
inf "underruns in live log: $(grep -c 'Underrun detected' /root/hyperTrumpet-live.log 2>/dev/null || echo n/a)"
inf "kernel USB errors: $(journalctl -k 2>/dev/null | grep -cE 'usb .*disconnect|error -71')"

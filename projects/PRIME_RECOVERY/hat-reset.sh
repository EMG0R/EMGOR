#!/bin/bash
# Reopen the DAC8x HAT in PipeWire (profile off -> pro-audio) without touching csound.
# Finds the HAT's device id by card name each time (ids change across reboots).
# Tell hat-watchdog-v2 this stream restart is deliberate (it ignores restarts for 10 s after).
#
# FAST PATH (2026-10-24): the old version slept a fixed 1 s, then waited in 1 s
# steps for someone else to recreate the links (~4 s of silence). Now: snapshot
# the HAT links first, poll at 20 ms for the node to go and come back, and relink
# the snapshot ourselves the moment the ports exist.
touch "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/hat-reset.stamp" 2>/dev/null
T0=$(date +%s%N)
# pw-cli lookup: 25 ms (the old wpctl inspect loop took 760 ms, all before the reset)
D=$(pw-cli ls Device 2>/dev/null | awk '/^[ \t]*id [0-9]+,/{id=$2; sub(",","",id)}
      /device.name = "alsa_card.platform-soc_107c000000_sound"/{print id; exit}')
[ -n "$D" ] || { echo "hat-reset: HAT device not found"; exit 1; }
# "out_port in_port" pairs touching the HAT, from pw-link -l's tree
SNAP=$(pw-link -l 2>/dev/null | awk '
  /^[^ \t|]/ { port=$0; next }
  /\|->/ { sub(/^[ \t]*\|-> /,""); print port " " $0 }' | grep -E 'pro-(output|input)')
hat_up() { pw-link -o 2>/dev/null | grep -q 'pro-output-0:monitor_AUX7' && pw-link -i 2>/dev/null | grep -q 'pro-output-0:playback_AUX7'; }
T1=$(date +%s%N)
wpctl set-profile "$D" 0
for i in $(seq 1 50); do hat_up || break; sleep 0.02; done
wpctl set-profile "$D" 4
for i in $(seq 1 150); do hat_up && break; sleep 0.02; done
T2=$(date +%s%N)
# relink; capture ports can appear a few ms after playback, so retry what failed
want=$(grep -c . <<< "$SNAP")
for k in $(seq 1 25); do
  while read -r a b; do [ -n "$a" ] && pw-link "$a" "$b" 2>/dev/null; done <<< "$SNAP"
  have=$(pw-link -l 2>/dev/null | awk '/^[^ \t|]/{p=$0;next} /\|->/{sub(/^[ \t]*\|-> /,""); print p " " $0}' | grep -cFxf <(echo "$SNAP"))
  [ "$have" -ge "$want" ] && break; sleep 0.02
done
T3=$(date +%s%N)
n=$(pw-link -l 2>/dev/null | grep -cE '^\s+\|(->|<-).*(pro-output|pro-input)')
echo "hat-reset: device $D reopened, $n HAT links; find $(( (T1-T0)/1000000 )) ms, gap $(( (T3-T1)/1000000 )) ms (reopen $(( (T2-T1)/1000000 )), relink $(( (T3-T2)/1000000 )))"

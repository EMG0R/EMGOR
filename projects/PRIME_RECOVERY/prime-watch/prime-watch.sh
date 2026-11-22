#!/bin/bash
# prime-watch: bring the engine (demiurge) and UI (neptr-ui) back if down/hung.
# Stands down while a game/foreground app owns the screen. Runs as root.
# Usage: prime-watch.sh [--dry-run] [--once]    (dry-run: log only, never act)
# Pause by hand:  touch /run/prime-watch.pause
DRY=0; ONCE=0
for a in "$@"; do case $a in --dry-run) DRY=1;; --once) ONCE=1;; esac; done
INTERVAL=2; NEED=2; MAXR=3; WINDOW=600
export XDG_RUNTIME_DIR=/run/user/1000
FOREIGN='^(retroarch|emulationstation|kodi|kodi.bin|mpv|vlc|chromium|chromium-browser|firefox|neptr_video|NEPTR_VIDEO)$'
declare -A bad; restarts=(); gaveup=0
log(){ echo "prime-watch: $*"; }

proc_state(){ awk '{print $3}' "/proc/$1/stat" 2>/dev/null; }
restart_ok(){ # rate limit; echo 0/1
  local now=$(date +%s) keep=() t
  for t in "${restarts[@]}"; do (( now - t < WINDOW )) && keep+=("$t"); done
  restarts=("${keep[@]}"); (( ${#restarts[@]} < MAXR ))
}
act(){ # $1=unit $2=reason
  if (( DRY )); then log "DRY-RUN would restart $1: $2"; return; fi
  if ! restart_ok; then
    (( gaveup )) || log "CRITICAL: $MAXR restarts in ${WINDOW}s already; NOT restarting $1 ($2). Manual attention needed."
    gaveup=1; return
  fi
  gaveup=0; restarts+=("$(date +%s)")
  log "RESTART $1: $2"; systemctl restart "$1"
}
fail(){ # $1=key $2=unit $3=reason ; returns after counting
  bad[$1]=$(( ${bad[$1]:-0} + 1 ))
  log "check FAIL [$1] (${bad[$1]}/$NEED): $3"
  if (( ${bad[$1]} >= NEED )); then act "$2" "$3"; bad[$1]=0; fi
}
ok(){ bad[$1]=0; }

check_engine(){
  local why=""
  systemctl is-active --quiet demiurge || { [ "$(systemctl is-active demiurge)" = activating ] || why="demiurge.service not active"; }
  if [ -z "$why" ]; then
    local cg pid found="" st
    cg=$(systemctl show demiurge -p ControlGroup --value)
    for pid in $(cat "/sys/fs/cgroup$cg/cgroup.procs" 2>/dev/null); do
      if tr '\0' ' ' < /proc/$pid/cmdline 2>/dev/null | grep -q 'demiurge-csound-host'; then found=$pid; fi
    done
    if [ -z "$found" ]; then why="no csound process in demiurge cgroup"
    else
      st=$(proc_state "$found")
      [[ $st == [TtDZ] ]] && why="csound pid $found state $st"
    fi
  fi
  # csound6 node = the host's pw-csound6 thread; read /proc, never talk to PipeWire
  if [ -z "$why" ]; then
    grep -qx 'pw-csound6' /proc/$found/task/*/comm 2>/dev/null || why="csound6 node thread missing"
  fi
  if [ -n "$why" ]; then fail engine demiurge "$why"; else ok engine; fi
}
# Patch switch: the launcher reloads live.conf and swaps engines itself. With no
# .csd in the chain csound is legitimately absent. A csound launch takes ~6 s to
# its node (measured 2026-11-01), so a missing csound is forgiven only within
# SWITCH_GRACE s of the launcher's last "launch:" line; otherwise normal NEED.
SWITCH_GRACE=12
csd_in_chain(){ awk '/^chain *=/{f=1;next} f&&/^[^ \t]/{f=0} f' /home/gorpi7v5/demiurge/live.conf | grep -q '\.csd'; }
check_engine_gated(){
  csd_in_chain || { ok engine; return; }
  check_engine
  # only on a failing check: was there a launch just now? (journal read, off the healthy path)
  if (( ${bad[engine]:-0} > 0 )) && journalctl -u demiurge --since "-${SWITCH_GRACE}s" -o cat -q 2>/dev/null | grep -q 'launch: .*(csound)'; then
    log "csound launching (patch switch), grace"; bad[engine]=0
  fi
  # Launcher startup (2026-11-05): before csound it runs the USB audio bounce +
  # 3 s WP wait (~7-15 s). Restarting inside that window killed the bounce between
  # unbind and bind, looped, and wedged the body Teensy. Forgive while the
  # launcher is younger than START_GRACE s or a bounce is running.
  if (( ${bad[engine]:-0} > 0 )); then
    local t0 up; t0=$(systemctl show demiurge -p ActiveEnterTimestampMonotonic --value)
    up=$(awk '{printf "%d", $1*1000000}' /proc/uptime)
    if pgrep -f demiurge-audio-bounce >/dev/null || (( t0 > 0 && up - t0 < START_GRACE*1000000 )); then
      log "launcher starting (bounce/launch), grace"; bad[engine]=0
    fi
  fi
}
START_GRACE=45
check_ui(){
  local why="" pid st
  if ! systemctl is-active --quiet neptr-ui; then [ "$(systemctl is-active neptr-ui)" = activating ] || why="neptr-ui.service not active"
  else
    pid=$(systemctl show neptr-ui -p MainPID --value)
    if [ "$pid" = 0 ] || [ ! -d /proc/$pid ]; then why="neptr-ui main process gone"
    else st=$(proc_state "$pid"); [[ $st == [TtDZ] ]] && why="neptr-ui pid $pid state $st"; fi
  fi
  if [ -n "$why" ]; then fail ui neptr-ui "$why"; else ok ui; fi
}
screen_on(){ # only meaningful if a backlight/bl_power exists (this panel has none in sysfs)
  local d
  for d in /sys/class/backlight/*; do
    [ -e "$d/bl_power" ] || continue
    if [ "$(cat $d/bl_power)" != 0 ]; then
      (( DRY )) && log "DRY-RUN would unblank $d" || { echo 0 > "$d/bl_power"; log "unblanked $d"; }
    fi
    [ "$(cat $d/brightness)" = 0 ] && ! (( DRY )) && { cat "$d/max_brightness" > "$d/brightness"; log "restored brightness $d"; }
  done
}
standdown(){
  [ -e /run/prime-watch.pause ] && { echo "pause file"; return 0; }
  local n; n=$(ps -eo comm= | grep -E "$FOREIGN" | head -1)
  [ -n "$n" ] && { echo "foreground app: $n"; return 0; }
  return 1
}

# USB SELF-HEAL (2026-11-02, EMGOR in his window: "U CAN REBIND USB IF IT IS
# TWEAKING"). Supersedes the old never-rebind rule for these devices only.
# Two failure shapes seen on this board (weak 5 V, 4.90 V):
#  a) STUCK: enumerated, but no interface has a driver (body Teensy 08:25:23,
#     "can't set config #1, error -110": no MIDI, no serial, controls dead)
#     -> toggle the device's `authorized` (0 then 1) = clean re-enumeration.
#  b) DEAD PORT: "device not accepting address / descriptor read error -71"
#     looping (DUODECIMUS on 1-2) -> power-cycle that one port via usbN-portM/disable.
# Per device: 2 consecutive bad checks (~20 s apart), max 3 heals / 10 min.
USB_DEVS="NEPTR:19910760 DUODECIMUS:16396190 PMOR:004700493333511832383435"
declare -A usbbad usbheal
usb_heal_ok(){ # $1 key; rate limit 3/600 s
  local now=$(date +%s) keep="" t n=0
  for t in ${usbheal[$1]}; do (( now - t < 600 )) && { keep+="$t "; n=$((n+1)); }; done
  usbheal[$1]="$keep"; (( n < 3 ))
}
usb_heal(){ # $1 key $2 how $3 path $4 reason
  if (( DRY )); then log "DRY-RUN would heal USB $1 ($2 $3): $4"; return; fi
  usb_heal_ok "$1" || { log "CRITICAL: USB $1 healed 3x in 10 min, leaving it ($4)"; return; }
  usbheal[$1]+="$(date +%s) "
  log "USB HEAL $1 via $2 $3: $4"
  if [ "$2" = authorized ]; then
    echo 0 > "$3/authorized"; sleep 1; echo 1 > "$3/authorized"
  else
    echo 1 > "$3"; sleep 2; echo 0 > "$3"
  fi
}
check_usb(){
  local ent key ser d dev why bound i
  for ent in $USB_DEVS; do
    key=${ent%%:*}; ser=${ent#*:}; dev=""
    for d in /sys/bus/usb/devices/*; do
      [ "$(cat $d/serial 2>/dev/null)" = "$ser" ] && { dev=$d; break; }
    done
    why=""
    if [ -n "$dev" ]; then
      bound=0
      for i in "$dev":1.*; do [ -e "$i/driver" ] && bound=1; done
      (( bound )) || why="enumerated but no interface driver bound"
    fi
    if [ -n "$why" ]; then
      usbbad[$key]=$(( ${usbbad[$key]:-0} + 1 ))
      log "usb FAIL [$key] (${usbbad[$key]}/2): $why"
      (( ${usbbad[$key]} >= 2 )) && { usb_heal "$key" authorized "$dev" "$why"; usbbad[$key]=0; }
    else
      usbbad[$key]=0
    fi
  done
  # dead ports: address/descriptor failures in the last 20 s
  local p port
  for p in $(journalctl -k --since -20s -o cat -q 2>/dev/null | sed -nE 's/^usb ([0-9]+-[0-9]+): (device not accepting address|device descriptor read.*error -71|Device not responding).*/\1/p' | sort -u); do
    port="/sys/bus/usb/devices/usb${p%%-*}/${p%%-*}-0:1.0/usb${p%%-*}-port${p#*-}/disable"
    [ -e "$port" ] || continue
    usbbad[port$p]=$(( ${usbbad[port$p]:-0} + 1 ))
    log "usb FAIL [port $p] (${usbbad[port$p]}/2): enumeration errors"
    (( ${usbbad[port$p]} >= 2 )) && { usb_heal "port$p" port "$port" "enumeration errors on $p"; usbbad[port$p]=0; }
  done
}
USB_EVERY=10; usbtick=9

# AUDIO HAT PATH (2026-11-02, EMGOR: "make sure system wise this never happens").
# 12:00 boot after a dirty power cut: ALSA open of the HAT failed (EINVAL), PipeWire
# never made pro-output-0/pro-input-0, the launcher fell back to the virtual
# demiurge-sink and csound played into nothing. Engine "healthy", zero audio.
# Check (every ~20 s, with the USB check): HAT nodes exist AND csound6:output1 is
# linked to the HAT. Escalation, each step only after the previous failed:
#   1 restart wireplumber+pipewire (user session), then demiurge   -> re-probe + rewire
#   2 if the HAT nodes still don't exist after that: reboot, at most once per 30 min
#     (stamp in /var/lib/prime-watch survives the reboot, so no boot loop)
HAT_OUT=alsa_output.platform-soc_107c000000_sound.pro-output-0
HAT_IN=alsa_input.platform-soc_107c000000_sound.pro-input-0
HATSTAMP=/var/lib/prime-watch/last-hat-reboot
hatbad=0; hatstep=0
pw_user(){ runuser -u gorpi7v5 -- env XDG_RUNTIME_DIR=/run/user/1000 "$@"; }
check_hat(){
  local why="" nodes links
  # Back off while anything else is acting on the engine (2026-11-02: at 12:39 this
  # check fired mid manual restart and restarted PipeWire on top of it): pause file,
  # engine check already failing, or csound launched in the last 30 s.
  [ -e /run/prime-watch.pause ] && { hatbad=0; return; }
  (( ${bad[engine]:-0} > 0 )) && { hatbad=0; return; }
  journalctl -u demiurge --since -30s -o cat -q 2>/dev/null | grep -q 'launch: .*(csound)' && { hatbad=0; return; }
  nodes=$(pw_user timeout 3 pw-cli ls Node 2>/dev/null | grep -c -e "node.name = \"$HAT_OUT\"" -e "node.name = \"$HAT_IN\"")
  if (( nodes < 2 )); then why="HAT nodes missing ($nodes/2)"
  else
    links=$(pw_user timeout 3 pw-link -l 2>/dev/null | awk -v p="$HAT_OUT:playback_AUX0" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&/csound6:output1/{print}' | wc -l)
    (( links >= 1 )) || why="csound not linked to HAT"
  fi
  if [ -z "$why" ]; then hatbad=0; hatstep=0; return; fi
  hatbad=$((hatbad + 1)); log "hat FAIL ($hatbad/2): $why"
  (( hatbad < 2 )) && return
  hatbad=0
  if (( DRY )); then log "DRY-RUN would recover HAT step $((hatstep+1)): $why"; return; fi
  if (( hatstep == 0 )); then
    hatstep=1
    log "HAT RECOVER 1: restart pipewire/wireplumber, then demiurge ($why)"
    touch /run/prime-watch.pause
    pw_user systemctl --user restart wireplumber pipewire pipewire-pulse
    sleep 4; systemctl restart demiurge; sleep 10
    rm -f /run/prime-watch.pause
  elif [[ $why == "HAT nodes missing"* ]]; then
    mkdir -p "${HATSTAMP%/*}"
    local last=$(cat "$HATSTAMP" 2>/dev/null || echo 0) now=$(date +%s)
    if (( now - last > 1800 )); then
      log "HAT RECOVER 2: HAT still not opening after pipewire restart -> REBOOT ($why)"
      echo "$now" > "$HATSTAMP"; sync; systemctl reboot
    else
      log "CRITICAL: HAT not opening and already auto-rebooted <30 min ago; needs a full power-off (unplug 10 s)"
    fi
  else
    log "HAT RECOVER 1b: nodes present but csound unlinked -> restart demiurge ($why)"
    touch /run/prime-watch.pause; systemctl restart demiurge; sleep 10; rm -f /run/prime-watch.pause
  fi
}

log "start dry=$DRY interval=${INTERVAL}s need=$NEED max=$MAXR/${WINDOW}s"
# BOOT GRACE (2026-11-02): with no After= ordering the watcher starts before the
# engine and UI, saw "not active" and restarted them mid-boot. Wait until the
# system is 90 s up, so a restart only ever happens to something that ran and died.
BOOT_GRACE=90
while (( $(cut -d. -f1 /proc/uptime) < BOOT_GRACE )); do sleep 2; done
log "boot grace over, watching"
while :; do
  if r=$(standdown); then
    [ "$last" != "$r" ] && log "standing down ($r)"; last=$r; bad=()
  else
    [ -n "$last" ] && log "resuming checks"; last=""
    check_engine_gated; check_ui; screen_on
    usbtick=$(( (usbtick + 1) % USB_EVERY )); (( usbtick == 0 )) && { check_usb; check_hat; }
    (( DRY )) && log "dry pass: engine_fail=${bad[engine]:-0} ui_fail=${bad[ui]:-0}"
  fi
  (( ONCE )) && exit 0
  sleep $INTERVAL
done

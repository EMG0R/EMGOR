#!/usr/bin/env bash
# pi-monitor.sh — live Pi status monitor for DEMIURGE
# Polls demiurge.local every 2s: link state, detailed temperature, CPU & GPU load.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Single source for the Pi rig's user/host is pi-rig.conf at the repo root.
# Override with PI_USER / PI_HOST env vars if yours differ for one run — no
# edits required; edit pi-rig.conf once to rename the rig for good.
[[ -f "$SCRIPT_DIR/../pi-rig.conf" ]] && source "$SCRIPT_DIR/../pi-rig.conf"
HOST="${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}"
USER="${PI_USER:-${PI_USER_DEFAULT:-gorpi7}}"
INTERVAL="${PI_MON_INTERVAL:-2}"
PING_TIMEOUT_MS=400
SSH_CONNECT_TIMEOUT=2
SERIAL_PORT="${PI_MON_SERIAL_PORT:-/dev/ttyACM0}"   # Teensy serial the Pi reads

# Sound playback on state changes
SOUNDS_DIR="${PI_MON_SOUNDS_DIR:-$SCRIPT_DIR/sounds}"
SND_CONNECTED="$SOUNDS_DIR/pi_connected.wav"
SND_DISCONNECTED="$SOUNDS_DIR/pi_disconnected.wav"
SND_BIG_LOAD="$SOUNDS_DIR/pi_big_load.wav"
SOUND_ENABLED="${PI_MON_SOUND:-1}"

# Big-load thresholds (transition-triggered)
BIG_LOAD_CPU_PCT="${PI_MON_BIG_LOAD_CPU:-80}"     # aggregate CPU %
BIG_LOAD_TEMP_C="${PI_MON_BIG_LOAD_TEMP:-75}"     # SoC °C
BIG_LOAD_COOLDOWN_S="${PI_MON_BIG_LOAD_COOLDOWN:-30}"

C_RESET=$'\033[0m'
C_DIM=$'\033[2m'
C_BOLD=$'\033[1m'
C_GREEN=$'\033[38;5;46m'
C_RED=$'\033[38;5;196m'
C_YELLOW=$'\033[38;5;220m'
C_ORANGE=$'\033[38;5;208m'
C_CYAN=$'\033[38;5;51m'
C_BLUE=$'\033[38;5;39m'
C_MAGENTA=$'\033[38;5;201m'
C_GREY=$'\033[38;5;245m'
C_WHITE=$'\033[38;5;255m'

CTL_DIR="${TMPDIR:-/tmp}/pi-monitor-$$"
CTL_PATH="$CTL_DIR/ssh-%h-%p-%r"
mkdir -p "$CTL_DIR"
chmod 700 "$CTL_DIR"

# Background serial streamer writes the latest complete Teensy frame here (one
# line, atomically replaced) so the display can repaint it several times a
# second without re-running the heavy SSH probe.
SERIAL_FILE="$CTL_DIR/serial.latest"
SERIAL_READER_PID=""
: > "$SERIAL_FILE"

TTY_SAVED=""
if [[ -t 0 ]]; then
  TTY_SAVED=$(stty -g 2>/dev/null || true)
  # -icanon -echo: char-at-a-time, no echo. min 0 time 2: a raw read() returns
  # after 0.2s (VTIME) with 0 bytes if no key, or immediately on a keypress. The
  # wait loop reads keys with `dd` (which honors VTIME) rather than bash's
  # `read -n1` (which re-blocks the tty and ignores these settings, and bash 3.2
  # has no fractional `read -t`). This gives a ~5Hz non-blocking tick. Restored
  # in cleanup.
  stty -icanon -echo min 0 time 2 2>/dev/null || true
fi

cleanup() {
  [[ -n "$SERIAL_READER_PID" ]] && kill "$SERIAL_READER_PID" 2>/dev/null
  ssh -o ControlPath="$CTL_PATH" -O exit "$USER@$HOST" 2>/dev/null
  rm -rf "$CTL_DIR" 2>/dev/null
  [[ -n "$TTY_SAVED" ]] && stty "$TTY_SAVED" 2>/dev/null
  printf '\033[?25h\n'
  exit 0
}
trap cleanup INT TERM EXIT

printf '\033[?25l'   # hide cursor
clear

prev_total=0
prev_idle=0
prev_v3d_total=0
prev_v3d_idle=0
declare -a prev_core_total prev_core_idle
ssh_master_up=0
LAST_SERIAL=""            # most recent Teensy serial frame (from the stream file)
SERIAL_REGION_READY=0     # 1 once render() has drawn+anchored the serial block

# Reboot-key state (two-press confirm so a stray 'r' doesn't take the Pi down)
ARMED_REBOOT=0
ARMED_AT=0
ARM_WINDOW_S=5
REBOOT_MSG=""

# State-change tracking (UNKNOWN until first observation, so we don't blare on startup)
link_state="UNKNOWN"      # UNKNOWN | UP | DOWN
load_state="UNKNOWN"      # UNKNOWN | NORMAL | BIG
last_big_load_play=0      # epoch seconds of last big-load chime (cooldown)

play_sound() {
  local file="$1"
  [[ "$SOUND_ENABLED" != "1" ]] && return
  [[ -f "$file" ]] || return
  command -v afplay >/dev/null 2>&1 || return
  ( afplay "$file" >/dev/null 2>&1 & ) >/dev/null 2>&1
}

bring_up_ssh_master() {
  # Try to (re)establish a multiplexed SSH master in the background, with a strict timeout.
  ssh -o ControlMaster=auto \
      -o ControlPath="$CTL_PATH" \
      -o ControlPersist=30 \
      -o ConnectTimeout="$SSH_CONNECT_TIMEOUT" \
      -o BatchMode=yes \
      -o ServerAliveInterval=2 \
      -o ServerAliveCountMax=2 \
      -o StrictHostKeyChecking=accept-new \
      -fN "$USER@$HOST" 2>/dev/null
}

ssh_master_alive() {
  ssh -o ControlPath="$CTL_PATH" -O check "$USER@$HOST" 2>/dev/null
}

ping_host() {
  # Returns: "OK <ms>" or "FAIL"
  local out ms
  out=$(ping -c 1 -W "$PING_TIMEOUT_MS" "$HOST" 2>/dev/null) || { echo "FAIL"; return; }
  ms=$(echo "$out" | awk -F'time=' '/time=/{print $2}' | awk '{print $1}')
  [[ -z "$ms" ]] && { echo "FAIL"; return; }
  echo "OK $ms"
}

# Remote one-shot probe. Echoes a series of K=V lines for easy parsing.
REMOTE_PROBE='
echo "TZ0=$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null)"
echo "VC_CPU=$(vcgencmd measure_temp 2>/dev/null)"
echo "VC_PMIC=$(vcgencmd measure_temp pmic 2>/dev/null)"
echo "CLK_ARM=$(vcgencmd measure_clock arm 2>/dev/null)"
echo "CLK_V3D=$(vcgencmd measure_clock v3d 2>/dev/null)"
echo "ARM_MAX_KHZ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null)"
echo "VOLT_CORE=$(vcgencmd measure_volts core 2>/dev/null)"
echo "VOLT_SDRC=$(vcgencmd measure_volts sdram_c 2>/dev/null)"
echo "EXT5V=$(vcgencmd pmic_read_adc 2>/dev/null | awk "/EXT5V_V/ {split(\$0,p,\"=\"); gsub(/V\$/,\"\",p[2]); printf \"%.2f\", p[2]}")"
echo "THROTTLED=$(vcgencmd get_throttled 2>/dev/null)"
echo "UPTIME=$(awk "{print \$1}" /proc/uptime)"
echo "LOAD=$(cut -d" " -f1-3 /proc/loadavg)"
echo "MEM=$(free -m | awk "/Mem:/ {print \$3\" \"\$2}")"
echo "V3D_STATS_BEGIN"
cat /sys/class/devfreq/*v3d*/trans_stat 2>/dev/null
echo "V3D_STATS_END"

# --- rolling 60s windows, state kept Pi-side in /dev/shm ---------------------
# THROTTLE: vcgencmd "now" bits (0xF) are instantaneous and the "occurred" bits
# are sticky since boot — neither is "last minute". Sample the now-bits every
# probe (2s cadence) into a history file, prune to 60s, OR the window together.
NOW_E=$(date +%s)
TH_RAW=$(vcgencmd get_throttled 2>/dev/null)
TH_HEX=${TH_RAW#throttled=}
TH_HEX=${TH_HEX:-0x0}
THH=/dev/shm/pimon-th.hist
echo "$NOW_E $(( TH_HEX & 0xF ))" >> "$THH"
awk -v c="$NOW_E" "\$1 >= c-60" "$THH" > "$THH.t" 2>/dev/null && mv "$THH.t" "$THH"
TH_1M=0
while read -r _t _v; do TH_1M=$(( TH_1M | _v )); done < "$THH"
echo "TH_1M=$TH_1M"

# XRUNS: journal entries are timestamped, so the demiurge unit (bypass csound
# logs there) is windowed directly. Launcher stage-log files have no
# timestamps, so window those by total-count history: count now, subtract the
# count from ~60s ago. Survives log truncation (counter reset => fresh history).
XJ=$(journalctl -u demiurge --since "-60 seconds" -q --no-pager 2>/dev/null | grep -aciE "xrun|under-?run|over-?run")
XF_TOT=$(cat "$HOME/.demiurge/logs/neptrPhase4.log" "$HOME/.demiurge/logs/csound.log" 2>/dev/null | grep -aciE "xrun|under-?run|over-?run")
XH=/dev/shm/pimon-xrun.hist
touch "$XH"
awk -v c="$NOW_E" "\$1 >= c-60" "$XH" > "$XH.t" 2>/dev/null && mv "$XH.t" "$XH"
XF_BASE=$(head -n1 "$XH" | awk "{print \$2}")
echo "$NOW_E $XF_TOT" >> "$XH"
[ -z "$XF_BASE" ] && XF_BASE=$XF_TOT
if [ "$XF_TOT" -lt "$XF_BASE" ]; then echo "$NOW_E $XF_TOT" > "$XH"; XF_BASE=$XF_TOT; fi
XF_1M=$(( XF_TOT - XF_BASE ))
[ "$XF_1M" -lt 0 ] && XF_1M=0
echo "XRUN_1M=$(( XJ + XF_1M ))"
# -----------------------------------------------------------------------------

grep -E "^cpu[0-9]* " /proc/stat
'

probe_remote() {
  ssh -o ControlPath="$CTL_PATH" \
      -o ConnectTimeout="$SSH_CONNECT_TIMEOUT" \
      -o BatchMode=yes \
      "$USER@$HOST" "$REMOTE_PROBE" 2>/dev/null
}

# Persistent background serial reader. Streams the Teensy's serial port over the
# multiplexed SSH master and keeps SERIAL_FILE updated with the most recent
# COMPLETE frame (each line atomically replaces the file via tmp+mv, so the
# display never reads a half-written frame and the file never grows). Lets the
# UI repaint the serial block several times a second with no extra SSH cost.
# Best-effort: if the MIDI bridge is also reading the port, the byte stream is
# shared and frames may arrive partial — that's an inherent single-reader limit.
start_serial_reader() {
  # Already running? leave it.
  [[ -n "$SERIAL_READER_PID" ]] && kill -0 "$SERIAL_READER_PID" 2>/dev/null && return
  # -n: read stdin from /dev/null. Critical — this ssh is backgrounded, and
  # without it the ssh competes with the foreground key-reader for the
  # controlling terminal, which starves the serial stream (no frames ever land).
  ( ssh -n \
        -o ControlPath="$CTL_PATH" \
        -o ConnectTimeout="$SSH_CONNECT_TIMEOUT" \
        -o BatchMode=yes \
        -o ServerAliveInterval=2 \
        -o ServerAliveCountMax=2 \
        "$USER@$HOST" \
        "stty -F $SERIAL_PORT raw 115200 -echo 2>/dev/null; exec cat $SERIAL_PORT" 2>/dev/null \
    | while IFS= read -r _l; do
        [[ -n "$_l" ]] || continue
        printf '%s' "$_l" > "$SERIAL_FILE.tmp" 2>/dev/null && mv -f "$SERIAL_FILE.tmp" "$SERIAL_FILE" 2>/dev/null
      done ) &
  SERIAL_READER_PID=$!
}

# Tear the reader down and wipe stale state so a reconnect starts fresh: killing
# the pipeline subshell closes the read end, so the inner ssh dies of SIGPIPE on
# its next write. Clearing the file + LAST_SERIAL drops the frozen last frame so
# the display shows "reconnecting…" rather than ghost data until live frames
# resume. start_serial_reader() then respawns cleanly on the rebuilt SSH master.
stop_serial_reader() {
  [[ -n "$SERIAL_READER_PID" ]] && kill "$SERIAL_READER_PID" 2>/dev/null
  SERIAL_READER_PID=""
  : > "$SERIAL_FILE" 2>/dev/null
  LAST_SERIAL=""
}

# Parse a "frequency(0)=NNN" line into MHz
parse_freq_mhz() {
  local val="${1#*=}"
  if [[ "$val" =~ ^[0-9]+$ ]]; then
    awk -v v="$val" 'BEGIN{printf "%.1f", v/1000000}'
  else
    echo "--"
  fi
}

# Convert raw vcgencmd "temp=58.7'C" -> "58.7"
parse_temp() {
  local val="${1#*=}"
  val="${val%\'C}"
  echo "$val"
}

color_for_temp() {
  local t="$1"
  if [[ -z "$t" || "$t" == "--" ]]; then echo "$C_GREY"; return; fi
  awk -v t="$t" 'BEGIN{
    if (t+0 >= 80) print "\033[38;5;196m";
    else if (t+0 >= 70) print "\033[38;5;208m";
    else if (t+0 >= 60) print "\033[38;5;220m";
    else print "\033[38;5;46m";
  }'
}

color_for_pct() {
  local p="$1"
  if [[ -z "$p" || "$p" == "--" ]]; then echo "$C_GREY"; return; fi
  awk -v p="$p" 'BEGIN{
    if (p+0 >= 90) print "\033[38;5;196m";
    else if (p+0 >= 70) print "\033[38;5;208m";
    else if (p+0 >= 40) print "\033[38;5;220m";
    else print "\033[38;5;46m";
  }'
}

bar() {
  local pct="$1" width="${2:-20}" color="$3"
  awk -v p="$pct" -v w="$width" -v c="$color" -v r="$C_RESET" -v g="$C_GREY" 'BEGIN{
    if (p == "" || p == "--") p = 0;
    if (p > 100) p = 100; if (p < 0) p = 0;
    filled = int(p/100*w + 0.5);
    printf "%s", c;
    for (i=0;i<filled;i++) printf "█";
    printf "%s", g;
    for (i=filled;i<w;i++) printf "░";
    printf "%s", r;
  }'
}

# decode_status <throttled-hex> <window-bits>
# Arg 1: raw vcgencmd hex — only the live bits (0xF) are used; the since-boot
#        sticky "occurred" bits (0xF0000) are deliberately ignored.
# Arg 2: OR of the live bits sampled over the last 60s (TH_1M from the probe),
#        so "past" means past MINUTE, not past boot.
decode_status() {
  local hex="${1:-0x0}" win="${2:-0}"
  [[ -z "$hex" ]] && hex="0x0"
  [[ "$win" =~ ^[0-9]+$ ]] || win=0
  local v_now=$(( hex & 0xF ))
  local v_win_only=$(( win & ~v_now & 0xF ))

  local now_msgs=() win_msgs=()
  (( v_now & 0x1 ))      && now_msgs+=("UNDERVOLTAGE")
  (( v_now & 0x2 ))      && now_msgs+=("FREQ-CAPPED")
  (( v_now & 0x4 ))      && now_msgs+=("THROTTLED")
  (( v_now & 0x8 ))      && now_msgs+=("HOT-LIMITED")
  (( v_win_only & 0x1 )) && win_msgs+=("undervoltage")
  (( v_win_only & 0x2 )) && win_msgs+=("freq-cap")
  (( v_win_only & 0x4 )) && win_msgs+=("throttled")
  (( v_win_only & 0x8 )) && win_msgs+=("hot-limited")

  local out=""
  if (( ${#now_msgs[@]} )); then
    local first=1
    for m in "${now_msgs[@]}"; do
      (( first )) || out+="  "
      out+="${C_RED}${C_BOLD}⚠ ${m}${C_RESET}"
      first=0
    done
  fi
  if (( ${#win_msgs[@]} )); then
    [[ -n "$out" ]] && out+=$'\n              '
    out+="${C_YELLOW}last 60s: ${win_msgs[*]}${C_RESET}"
  fi
  [[ -z "$out" ]] && out="${C_GREEN}${C_BOLD}✓ CLEAN (60s)${C_RESET}"
  printf '%s' "$out"
}

# Returns "TOTAL IDLE" from a /proc/stat cpu line
sum_cpu_line() {
  # fields: user nice system idle iowait irq softirq steal guest guest_nice
  awk '{
    total = $2+$3+$4+$5+$6+$7+$8+$9+$10+$11;
    idle  = $5+$6;
    printf "%d %d", total, idle;
  }'
}

human_uptime() {
  local s="${1%.*}"
  local d=$(( s/86400 ))
  local h=$(( (s%86400)/3600 ))
  local m=$(( (s%3600)/60 ))
  local r=$(( s%60 ))
  if (( d > 0 )); then printf "%dd %02dh%02dm" "$d" "$h" "$m"
  elif (( h > 0 )); then printf "%dh %02dm %02ds" "$h" "$m" "$r"
  elif (( m > 0 )); then printf "%dm %02ds" "$m" "$r"
  else printf "%ds" "$r"
  fi
}

# Print the SERIAL block: the whole most-recent Teensy frame, wrapped across as
# many lines as it needs (broken on spaces between key=value tokens), with
# continuation lines indented under the value column. This is the ONLY part of
# the screen repainted at the fast serial cadence, so it must not touch any of
# render()'s delta state. Drawn at a cursor position saved by render().
print_serial_block() {
  local cols; cols=$(tput cols 2>/dev/null || echo 80)
  local avail=$(( cols - 14 )); (( avail < 24 )) && avail=24
  local sframe="${LAST_SERIAL//$'\r'/}"
  printf '    %s%-8s%s  ' "$C_GREY" "SERIAL" "$C_RESET"
  if [[ -z "$sframe" ]]; then
    printf '%s— no frame (Teensy quiet, or bridge holding the port)%s\n' "$C_DIM" "$C_RESET"
    return
  fi
  local first=1
  while IFS= read -r chunk; do
    if (( first )); then
      printf '%s%s%s\n' "$C_GREEN" "$chunk" "$C_RESET"; first=0
    else
      printf '              %s%s%s\n' "$C_GREEN" "$chunk" "$C_RESET"
    fi
  done < <(printf '%s\n' "$sframe" | fold -s -w "$avail")
}

render() {
  local ping_status="$1" probe_data="$2"
  # Globals written for state-change detection in the main loop
  RENDER_SOC_TEMP=""
  RENDER_CPU_PCT=""
  RENDER_THROTTLED_NOW=0

  # Parse ping status
  local link_color link_label link_extra
  if [[ "$ping_status" == FAIL ]]; then
    link_color="$C_RED"; link_label="OFFLINE"; link_extra="no reply"
  else
    local ms="${ping_status#OK }"
    link_color="$C_GREEN"; link_label="ONLINE"; link_extra="${ms} ms"
  fi

  # Defaults if probe failed
  local tz0="" vc_cpu="" vc_pmic="" clk_arm="" clk_v3d="" arm_max_khz=""
  local volt_core="" volt_sdrc="" ext5v="" throttled="" uptime_s="" loadavg=""
  local mem_used="" mem_total="" th_1m="" xrun_1m=""
  local -a cpu_lines=()
  local -a v3d_stat_lines=()
  local in_v3d=0

  if [[ -n "$probe_data" ]]; then
    while IFS= read -r line; do
      if [[ "$line" == "V3D_STATS_BEGIN" ]]; then in_v3d=1; continue; fi
      if [[ "$line" == "V3D_STATS_END"   ]]; then in_v3d=0; continue; fi
      if (( in_v3d )); then v3d_stat_lines+=("$line"); continue; fi
      case "$line" in
        TZ0=*)         tz0="${line#TZ0=}" ;;
        VC_CPU=*)      vc_cpu="${line#VC_CPU=}" ;;
        VC_PMIC=*)     vc_pmic="${line#VC_PMIC=}" ;;
        CLK_ARM=*)     clk_arm="${line#CLK_ARM=}" ;;
        CLK_V3D=*)     clk_v3d="${line#CLK_V3D=}" ;;
        ARM_MAX_KHZ=*) arm_max_khz="${line#ARM_MAX_KHZ=}" ;;
        VOLT_CORE=*)   volt_core="${line#VOLT_CORE=}" ;;
        VOLT_SDRC=*)   volt_sdrc="${line#VOLT_SDRC=}" ;;
        EXT5V=*)       ext5v="${line#EXT5V=}" ;;
        THROTTLED=*)   throttled="${line#THROTTLED=}" ;;
        TH_1M=*)       th_1m="${line#TH_1M=}" ;;
        XRUN_1M=*)     xrun_1m="${line#XRUN_1M=}" ;;
        UPTIME=*)      uptime_s="${line#UPTIME=}" ;;
        LOAD=*)        loadavg="${line#LOAD=}" ;;
        MEM=*)         read -r mem_used mem_total <<<"${line#MEM=}" ;;
        cpu*)          cpu_lines+=("$line") ;;
      esac
    done <<<"$probe_data"
  fi

  # Decode temperatures
  local cpu_temp_c="" pmic_temp_c=""
  if [[ -n "$tz0" && "$tz0" =~ ^[0-9]+$ ]]; then
    cpu_temp_c=$(awk -v v="$tz0" 'BEGIN{printf "%.1f", v/1000}')
  elif [[ -n "$vc_cpu" ]]; then
    cpu_temp_c=$(parse_temp "$vc_cpu")
  fi
  [[ -n "$vc_pmic" ]] && pmic_temp_c=$(parse_temp "$vc_pmic")

  # Decode clocks (MHz, integer)
  local arm_mhz="--" v3d_mhz="--" arm_max_mhz="--"
  if [[ -n "$clk_arm" ]]; then
    local _a="${clk_arm#*=}"
    [[ "$_a" =~ ^[0-9]+$ ]] && arm_mhz=$(awk -v v="$_a" 'BEGIN{printf "%.0f", v/1000000}')
  fi
  if [[ -n "$clk_v3d" ]]; then
    local _v="${clk_v3d#*=}"
    [[ "$_v" =~ ^[0-9]+$ ]] && v3d_mhz=$(awk -v v="$_v" 'BEGIN{printf "%.0f", v/1000000}')
  fi
  if [[ "$arm_max_khz" =~ ^[0-9]+$ ]]; then
    arm_max_mhz=$(awk -v v="$arm_max_khz" 'BEGIN{printf "%.0f", v/1000}')
  fi

  # Decode voltages
  local v_core="--" v_sdrc="--"
  if [[ -n "$volt_core" ]]; then v_core="${volt_core#*=}"; v_core="${v_core%V}"; fi
  if [[ -n "$volt_sdrc" ]]; then v_sdrc="${volt_sdrc#*=}"; v_sdrc="${v_sdrc%V}"; fi

  # GPU busy% from devfreq trans_stat: time spent at any non-idle OPP
  # over the elapsed wall-clock between polls. The lowest freq counts as idle.
  local v3d_idle_time=0 v3d_total_time=0
  if (( ${#v3d_stat_lines[@]} > 0 )); then
    local _stats
    _stats=$(printf '%s\n' "${v3d_stat_lines[@]}" | awk '
      /^[[:space:]]*\*?[[:space:]]*[0-9]+:/ {
        raw = $0
        sub(/^[[:space:]]*\*?[[:space:]]*/, "", raw)
        sub(/:.*$/, "", raw)
        freq = raw + 0
        time_ms = $NF + 0
        total += time_ms
        freqs[freq] = time_ms
      }
      END {
        min_f = -1
        min_k = ""
        for (f in freqs) {
          fn = f + 0
          if (min_f < 0 || fn < min_f) { min_f = fn; min_k = f }
        }
        idle = (min_k == "") ? 0 : freqs[min_k]
        printf "%d %d", idle, total
      }
    ')
    read -r v3d_idle_time v3d_total_time <<<"$_stats"
  fi

  local gpu_load="--"
  if (( prev_v3d_total > 0 && v3d_total_time > prev_v3d_total )); then
    local d_total=$(( v3d_total_time - prev_v3d_total ))
    local d_idle=$(( v3d_idle_time - prev_v3d_idle ))
    gpu_load=$(awk -v dt="$d_total" -v di="$d_idle" \
      'BEGIN{p=(dt-di)/dt*100; if(p<0)p=0; if(p>100)p=100; printf "%.0f", p}')
  fi
  prev_v3d_total="$v3d_total_time"
  prev_v3d_idle="$v3d_idle_time"

  # CPU usage from /proc/stat (aggregate + per-core)
  local cpu_total_pct="--"
  local -a per_core_pct=()
  if (( ${#cpu_lines[@]} )); then
    for line in "${cpu_lines[@]}"; do
      read -r total idle <<<"$(echo "$line" | sum_cpu_line)"
      local name; name=$(echo "$line" | awk '{print $1}')
      if [[ "$name" == "cpu" ]]; then
        local dt=$(( total - prev_total ))
        local di=$(( idle - prev_idle ))
        if (( dt > 0 )); then
          cpu_total_pct=$(awk -v dt="$dt" -v di="$di" 'BEGIN{printf "%.0f", (dt-di)/dt*100}')
        fi
        prev_total=$total; prev_idle=$idle
      else
        local idx="${name#cpu}"
        local pt="${prev_core_total[$idx]:-0}"
        local pi="${prev_core_idle[$idx]:-0}"
        local dt=$(( total - pt ))
        local di=$(( idle - pi ))
        if (( dt > 0 )); then
          per_core_pct[$idx]=$(awk -v dt="$dt" -v di="$di" 'BEGIN{printf "%.0f", (dt-di)/dt*100}')
        else
          per_core_pct[$idx]="--"
        fi
        prev_core_total[$idx]=$total
        prev_core_idle[$idx]=$idle
      fi
    done
  fi

  # Export values for state-change detection in main loop
  RENDER_SOC_TEMP="$cpu_temp_c"
  RENDER_CPU_PCT="$cpu_total_pct"
  if [[ -n "$throttled" ]]; then
    local _hex="${throttled#throttled=}"
    if [[ -n "$_hex" && "$_hex" != "0x0" ]]; then
      local _v=$((_hex))
      (( _v & 0xF )) && RENDER_THROTTLED_NOW=1
    fi
  fi

  # ----- Render -----
  printf '\033[H\033[J'
  printf '\n   %sDEMIURGE%s\n' "$C_BOLD$C_CYAN" "$C_RESET"
  printf '   %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n\n' "$C_DIM$C_CYAN" "$C_RESET"

  # LINK
  printf '    %s%-8s%s  %s● %s%s   %s%s%s\n' \
    "$C_GREY" "LINK" "$C_RESET" \
    "$link_color$C_BOLD" "$link_label" "$C_RESET" \
    "$C_DIM" "$link_extra" "$C_RESET"

  if [[ "$ping_status" == FAIL ]]; then
    printf '\n    %sPi not responding — retrying every %ss%s\n' "$C_YELLOW" "$INTERVAL" "$C_RESET"
    printf '\n   %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n' "$C_DIM$C_CYAN" "$C_RESET"
    return
  fi
  if [[ -z "$probe_data" ]]; then
    printf '\n    %sLink up but SSH probe failed%s\n' "$C_YELLOW" "$C_RESET"
    printf '\n   %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n' "$C_DIM$C_CYAN" "$C_RESET"
    return
  fi

  # STATUS — throttling / undervoltage, last-60s window (prominent)
  printf '    %s%-8s%s  %s\n' \
    "$C_GREY" "STATUS" "$C_RESET" \
    "$(decode_status "${throttled#throttled=}" "$th_1m")"

  # XRUN — audio xrun/underrun/overrun count over the last 60s (journal +
  # launcher stage logs, windowed Pi-side by the probe)
  local xcol="$C_GREY" xdisp="--"
  if [[ "$xrun_1m" =~ ^[0-9]+$ ]]; then
    xdisp="$xrun_1m"
    if (( xrun_1m > 0 )); then xcol="$C_RED$C_BOLD"; else xcol="$C_GREEN$C_BOLD"; fi
  fi
  printf '    %s%-8s%s  %s%4s%s   %sin last 60s%s\n' \
    "$C_GREY" "XRUN" "$C_RESET" \
    "$xcol" "$xdisp" "$C_RESET" \
    "$C_DIM" "$C_RESET"

  printf '\n'

  # TEMP — SoC + PMIC annotation
  local tcol; tcol=$(color_for_temp "$cpu_temp_c")
  local pcol; pcol=$(color_for_temp "$pmic_temp_c")
  local temp_pct
  temp_pct=$(awk -v t="${cpu_temp_c:-0}" 'BEGIN{p=(t-30)/55*100; if(p<0)p=0; if(p>100)p=100; printf "%.0f", p}')
  printf '    %s%-8s%s  %s%4s °C %s  %s   %spmic %s%s°C%s\n' \
    "$C_GREY" "TEMP" "$C_RESET" \
    "$tcol$C_BOLD" "${cpu_temp_c:---}" "$C_RESET" \
    "$(bar "$temp_pct" 20 "$tcol")" \
    "$C_DIM" "$pcol" "${pmic_temp_c:---} " "$C_RESET"

  # CPU — aggregate + ARM clock annotation. When current ARM MHz is more than
  # 100 MHz below the configured max, it's being capped — flag it in orange.
  local ccol; ccol=$(color_for_pct "$cpu_total_pct")
  local arm_cap_marker=""
  local arm_max_label="$arm_max_mhz"
  if [[ "$arm_mhz" =~ ^[0-9]+$ && "$arm_max_mhz" =~ ^[0-9]+$ ]]; then
    if awk -v c="$arm_mhz" -v m="$arm_max_mhz" 'BEGIN{exit !(c+0 < m-100)}'; then
      arm_cap_marker=" ${C_ORANGE}⬇ capped${C_RESET}"
    fi
  fi
  printf '    %s%-8s%s  %s%4s %% %s  %s   %sarm %s%s / %s MHz%s%s\n' \
    "$C_GREY" "CPU" "$C_RESET" \
    "$ccol$C_BOLD" "$cpu_total_pct" "$C_RESET" \
    "$(bar "$cpu_total_pct" 20 "$ccol")" \
    "$C_DIM" "$C_WHITE" "$arm_mhz" "$arm_max_label" "$C_RESET" "$arm_cap_marker"

  # Per-core compact
  local ncores=${#per_core_pct[@]}
  if (( ncores > 0 )); then
    printf '              '
    for ((i=0; i<ncores; i++)); do
      local p="${per_core_pct[$i]:---}"
      local col; col=$(color_for_pct "$p")
      printf '%sc%d%s %s%3s%%%s  ' "$C_DIM" "$i" "$C_RESET" "$col" "$p" "$C_RESET"
    done
    printf '\n'
  fi

  # GPU — V3D clock position with annotation
  local gcol; gcol=$(color_for_pct "$gpu_load")
  printf '    %s%-8s%s  %s%4s %% %s  %s   %sv3d %s%s MHz%s\n' \
    "$C_GREY" "GPU" "$C_RESET" \
    "$gcol$C_BOLD" "$gpu_load" "$C_RESET" \
    "$(bar "$gpu_load" 20 "$gcol")" \
    "$C_DIM" "$C_WHITE" "$v3d_mhz" "$C_RESET"

  # MEM
  local mem_pct="--"
  local mem_used_g="--" mem_total_g="--"
  if [[ "$mem_used" =~ ^[0-9]+$ && "$mem_total" =~ ^[0-9]+$ && "$mem_total" -gt 0 ]]; then
    mem_pct=$(awk -v u="$mem_used" -v t="$mem_total" 'BEGIN{printf "%.0f", u/t*100}')
    mem_used_g=$(awk -v u="$mem_used" 'BEGIN{printf "%.1f", u/1024}')
    mem_total_g=$(awk -v t="$mem_total" 'BEGIN{printf "%.1f", t/1024}')
  fi
  local mcol; mcol=$(color_for_pct "$mem_pct")
  printf '    %s%-8s%s  %s%4s %% %s  %s%s / %s GiB%s\n' \
    "$C_GREY" "MEM" "$C_RESET" \
    "$mcol$C_BOLD" "$mem_pct" "$C_RESET" \
    "$C_DIM" "$mem_used_g" "$mem_total_g" "$C_RESET"

  # LOAD
  printf '    %s%-8s%s  %s%s%s\n' \
    "$C_GREY" "LOAD" "$C_RESET" \
    "$C_WHITE" "${loadavg:---}" "$C_RESET"

  # VOLT — input rail (EXT5V) is the brown-out canary; core/sdram fall after it.
  # EXT5V color: red <4.70 (close to undervoltage threshold 4.63), orange <4.80,
  # green otherwise. This is the line to watch when tuning the LED brightness.
  local ext5v_disp="--"
  local ext5v_col="$C_GREY"
  if [[ "$ext5v" =~ ^[0-9.]+$ ]]; then
    ext5v_disp="$ext5v"
    ext5v_col=$(awk -v v="$ext5v" 'BEGIN{
      if (v+0 < 4.70) print "\033[38;5;196m";
      else if (v+0 < 4.80) print "\033[38;5;208m";
      else print "\033[38;5;46m";
    }')
  fi
  printf '    %s%-8s%s  %sext5v %s%s V%s  %s·%s  %score %s%s V%s  %s·%s  %ssdram %s%s V%s\n' \
    "$C_GREY" "VOLT" "$C_RESET" \
    "$C_DIM" "$ext5v_col$C_BOLD" "$ext5v_disp" "$C_RESET" \
    "$C_DIM" "$C_RESET" \
    "$C_DIM" "$C_WHITE" "$v_core" "$C_RESET" \
    "$C_DIM" "$C_RESET" \
    "$C_DIM" "$C_WHITE" "$v_sdrc" "$C_RESET"

  # UPTIME
  local up_h; up_h=$(human_uptime "$uptime_s")
  printf '    %s%-8s%s  %s%s%s\n' \
    "$C_GREY" "UPTIME" "$C_RESET" \
    "$C_WHITE" "$up_h" "$C_RESET"

  printf '\n   %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n' "$C_DIM$C_CYAN" "$C_RESET"

  if (( ARMED_REBOOT == 1 )); then
    printf '   %s⚠ press R again to REBOOT  ·  any other key cancels%s\n' \
      "$C_RED$C_BOLD" "$C_RESET"
  elif [[ -n "$REBOOT_MSG" ]]; then
    printf '   %s⚠ %s%s\n' "$C_YELLOW$C_BOLD" "$REBOOT_MSG" "$C_RESET"
  fi

  printf '   %s%s · every %ss · [r] reboot · [q] quit%s\n' \
    "$C_DIM" "$USER@$HOST" "$INTERVAL" "$C_RESET"

  # Serial block goes last and is cursor-anchored so the wait loop can repaint
  # just this region a few times a second without disturbing the stats above.
  printf '\n'
  printf '\033[s'        # save cursor at the serial block's origin
  print_serial_block
  SERIAL_REGION_READY=1
}

# Repaint only the serial block, in place, from the saved cursor anchor. Cheap
# (no SSH, no render-state mutation) so it can run several times per second.
refresh_serial_block() {
  (( SERIAL_REGION_READY == 1 )) || return
  printf '\033[u\033[J'   # restore to the saved anchor, clear from there down
  print_serial_block
}

do_reboot() {
  ssh -o ControlPath="$CTL_PATH" \
      -o ConnectTimeout="$SSH_CONNECT_TIMEOUT" \
      -o BatchMode=yes \
      "$USER@$HOST" "sudo /sbin/reboot" >/dev/null 2>&1 &
  REBOOT_MSG="reboot sent — Pi going down"
}

handle_key() {
  local k="$1"
  local now_e
  now_e=$(date +%s)
  case "$k" in
    r|R)
      if (( ARMED_REBOOT == 1 && now_e - ARMED_AT <= ARM_WINDOW_S )); then
        do_reboot
        ARMED_REBOOT=0
      else
        ARMED_REBOOT=1
        ARMED_AT=$now_e
      fi
      ;;
    q|Q)
      cleanup
      ;;
    *)
      ARMED_REBOOT=0
      REBOOT_MSG=""
      ;;
  esac
}

# Kick off SSH master once up front
bring_up_ssh_master

while :; do
  start=$(date +%s)

  ping_status=$(ping_host)

  if [[ "$ping_status" == FAIL ]]; then
    probe=""
    SERIAL_REGION_READY=0
    stop_serial_reader                           # drop the stale stream; respawn on reconnect
  else
    if ! ssh_master_alive; then
      bring_up_ssh_master
    fi
    probe=$(probe_remote)
    start_serial_reader                          # (re)start the background stream if not running
    LAST_SERIAL=$(cat "$SERIAL_FILE" 2>/dev/null)
  fi

  render "$ping_status" "$probe"

  # ---- state-change audio cues ----
  # Link state transitions
  if [[ "$ping_status" == FAIL ]]; then
    new_link="DOWN"
  else
    new_link="UP"
  fi
  if [[ "$link_state" != "UNKNOWN" && "$new_link" != "$link_state" ]]; then
    case "$new_link" in
      UP)   play_sound "$SND_CONNECTED" ;;
      DOWN) play_sound "$SND_DISCONNECTED" ;;
    esac
  fi
  link_state="$new_link"

  # Big-load transition (only when we have valid telemetry)
  if [[ "$new_link" == "UP" && -n "$probe" ]]; then
    is_big=0
    if [[ "$RENDER_CPU_PCT" =~ ^[0-9.]+$ ]]; then
      awk -v p="$RENDER_CPU_PCT" -v t="$BIG_LOAD_CPU_PCT" 'BEGIN{exit !(p+0 >= t+0)}' && is_big=1
    fi
    if [[ "$RENDER_SOC_TEMP" =~ ^[0-9.]+$ ]]; then
      awk -v p="$RENDER_SOC_TEMP" -v t="$BIG_LOAD_TEMP_C" 'BEGIN{exit !(p+0 >= t+0)}' && is_big=1
    fi
    (( RENDER_THROTTLED_NOW == 1 )) && is_big=1

    if (( is_big == 1 )); then
      new_load="BIG"
    else
      new_load="NORMAL"
    fi
    if [[ "$load_state" != "UNKNOWN" && "$new_load" == "BIG" && "$load_state" != "BIG" ]]; then
      now_epoch=$(date +%s)
      if (( now_epoch - last_big_load_play >= BIG_LOAD_COOLDOWN_S )); then
        play_sound "$SND_BIG_LOAD"
        last_big_load_play=$now_epoch
      fi
    fi
    load_state="$new_load"
  fi

  # Wait out the remainder of the interval, ticking ~4x/second to repaint the
  # serial block in place from the background stream (a few updates per second)
  # and to poll keys non-blockingly so 'r' arms/confirms a reboot and 'q' quits.
  wait_end=$(( start + INTERVAL ))
  while :; do
    now_t=$(date +%s)
    (( now_t >= wait_end )) && break

    # Read one key via dd, which honors the tty VTIME (0.2s): this both paces
    # the loop (~5Hz) and stays responsive to keypresses, without the bash
    # `read -n1` blocking problem. Empty when no key was pressed.
    key=$(dd bs=1 count=1 2>/dev/null)
    [[ -n "$key" ]] && handle_key "$key"

    # Pull the newest serial frame and repaint just that region, in place.
    new_serial=$(cat "$SERIAL_FILE" 2>/dev/null)
    if [[ "$new_serial" != "$LAST_SERIAL" ]]; then
      LAST_SERIAL="$new_serial"
      refresh_serial_block
    fi

    # Auto-disarm after the window expires so the banner doesn't linger
    if (( ARMED_REBOOT == 1 )); then
      now_t=$(date +%s)
      if (( now_t - ARMED_AT > ARM_WINDOW_S )); then
        ARMED_REBOOT=0
        render "$ping_status" "$probe"
      fi
    fi
    # Full re-render immediately after a keypress so the banner updates
    if [[ -n "$key" ]]; then
      render "$ping_status" "$probe"
    fi
  done
done

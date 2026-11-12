#!/bin/bash
#
# DEMIURGE M8 — Thermal watchdog
# Install to: /usr/local/bin/demiurge-thermal-watchdog.sh
#
# Runs on a timer (see demiurge-thermal-watchdog.timer). Logs temperature
# and throttle state to the journal. If either crosses a danger threshold
# it writes a big WARN line — makes journalctl -u demiurge-thermal-watchdog
# the single place to look when checking overclock safety.
#
# Thresholds:
#   temp >= 80C        → warn (soft)
#   temp >= 85C        → CRITICAL (hardware throttle imminent)
#   throttled != 0x0   → log the decoded throttle flags
#
set -u

TEMP_WARN="${TEMP_WARN:-80.0}"
TEMP_CRIT="${TEMP_CRIT:-85.0}"

temp_raw=$(vcgencmd measure_temp 2>/dev/null || echo "temp=0.0'C")
temp_c="${temp_raw#temp=}"
temp_c="${temp_c%\'C}"

throttled=$(vcgencmd get_throttled 2>/dev/null || echo "throttled=0x0")
throttled_hex="${throttled#throttled=}"

level="INFO"
if awk "BEGIN{ exit !($temp_c >= $TEMP_CRIT) }"; then
    level="CRITICAL"
elif awk "BEGIN{ exit !($temp_c >= $TEMP_WARN) }"; then
    level="WARN"
fi

# Decode throttle bits (see RPi firmware docs)
decode=""
if [ "$throttled_hex" != "0x0" ]; then
    val=$((throttled_hex))
    (( val & 0x1     )) && decode+=" undervolt_now"
    (( val & 0x2     )) && decode+=" arm_capped_now"
    (( val & 0x4     )) && decode+=" throttled_now"
    (( val & 0x8     )) && decode+=" soft_tempt_limit_now"
    (( val & 0x10000 )) && decode+=" undervolt_occurred"
    (( val & 0x20000 )) && decode+=" arm_capped_occurred"
    (( val & 0x40000 )) && decode+=" throttled_occurred"
    (( val & 0x80000 )) && decode+=" soft_tempt_limit_occurred"
    level="WARN"
fi

printf '[%s] temp=%sC throttled=%s%s\n' "$level" "$temp_c" "$throttled_hex" "$decode"

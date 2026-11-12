#!/bin/bash
# Live power/thermal probe: catches PSU droop before a silent reset. 1 line / 2 s, synced every 10 lines.
F=$HOME/PRIME_RECOVERY/logs/power-$(date +%Y%m%d-%H%M%S).log; n=0
while :; do
  echo "$(date +%T) $(vcgencmd get_throttled) $(vcgencmd pmic_read_adc EXT5V_V | awk '{print $2}') $(vcgencmd measure_temp) load=$(cut -d' ' -f1 /proc/loadavg)" >> $F
  n=$((n+1)); [ $((n%10)) -eq 0 ] && sync $F; sleep 2
done

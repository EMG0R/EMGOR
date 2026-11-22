#!/bin/bash
# Flush engine state files to the SD card within ~2 s of every save.
# csound's ftsavek only reaches the page cache; the kernel writes it back
# after vm.dirty_expire_centisecs (30 s). A dirty power cut loses that window.
# fsync of ~20 KB per change is negligible. Runs idle-class, no audio impact.
D="$HOME/demiurge/state"
P="$HOME/demiurge/presets"
declare -A last
while true; do
  for f in "$D"/*.ftab "$P"/*/bank*.ftab "$P"/*/seq*.ftab; do
    [ -f "$f" ] || continue
    m=$(stat -c %Y.%y "$f" 2>/dev/null) || continue
    if [ "${last[$f]}" != "$m" ]; then last[$f]=$m; sync "$f" 2>/dev/null; fi
  done
  sleep 2
done

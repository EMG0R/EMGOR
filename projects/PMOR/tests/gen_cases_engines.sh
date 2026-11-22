#!/usr/bin/env bash
# Regenerates tests/cases_engines/*.scn: new drum presets 16..23 per engine, and the 4 depth knobs per engine.
set -euo pipefail
d="$(cd "$(dirname "$0")" && pwd)/cases_engines"; rm -rf "$d"; mkdir -p "$d"
GP=(8 10 2 0)   # a pad inside KICK, SNARE, HAT, PERC
for g in 0 1 2 3; do for s in $(seq 16 23); do
  { echo "seconds 1.5"; echo "set drums.pad.${GP[$g]}.sound $s"; echo "at 0.0 drum ${GP[$g]} 1.0"
    if [ $g = 3 ] && [ $s -le 18 ]; then echo "at 0.7 drum ${GP[$g]} 0.8"; fi; } > "$d/drum_${g}_$s.scn"
done; done
for g in 0 1 2 3; do for k in 0 1 2 3; do
  { echo "seconds 1.2"; echo "set drums.eng.$g.$k 0.7"; echo "set drums.pad.${GP[$g]}.sound 2"; echo "at 0.0 drum ${GP[$g]} 1.0"; } > "$d/eng_${g}_$k.scn"
done; done
for a in 0 1 2 3 4 5 6 7; do for pr in 1 2 3 4; do
  { echo "seconds 2.5"; echo "set syn.algo $a"; echo "syn_load $a $pr"
    echo "at 0.0 synth_on 4 0.9"; echo "at 0.0 synth_on 9 0.7"; echo "at 1.4 synth_off 4"; echo "at 1.4 synth_off 9"; } > "$d/synth_${a}_$pr.scn"
done; done
ls "$d" | wc -l

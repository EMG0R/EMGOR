#!/bin/bash
#
# chiller_ambient.sh — activate the Chiller Ambient preset on NEPTR Phase 4.
#
# Sends the full activation sequence from presets/chiller_ambient.md to
# Midi Through with human-like timing (slight random jitter per step).
#
# Run on the Pi:
#   bash ~/NEPTR_phase4/presets/chiller_ambient.sh
#
# Or from the Mac over SSH:
#   ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/presets/chiller_ambient.sh'

set -euo pipefail

MIDI="$HOME/NEPTR_phase4/scripts/claude_midi.sh"
[ -x "$MIDI" ] || { echo "ERROR: claude_midi.sh not found at $MIDI" >&2; exit 1; }

# cc <num> <val>  — send one CC on default ch1
cc() { bash "$MIDI" cc "$1" "$2"; }

# jitter_sleep <base_ms> <spread_ms>
# Sleeps base_ms ± random(0..spread_ms) milliseconds.
jitter_sleep() {
    local base=$1 spread=$2
    local ms=$(( base - spread + RANDOM % (2 * spread + 1) ))
    (( ms < 15 )) && ms=15
    sleep "$(awk -v ms="$ms" 'BEGIN{printf "%.3f", ms/1000}')"
}

# enc_steps <count> <cc_num> <val>
# Sends <count> encoder steps with human-like inter-step timing (~30–45ms).
enc_steps() {
    local n=$1 ccn=$2 val=$3 i
    for (( i=0; i<n; i++ )); do
        cc "$ccn" "$val"
        jitter_sleep 35 12
    done
}

echo "=== Chiller Ambient — activating ==="
echo "    (all effects on menu 4)"
echo ""

# 1. Navigate to menu 4 (CC73 ×4, ~310ms between presses)
echo "1/9  Menu 4 navigate (CC73 ×4)..."
for _ in 1 2 3 4; do
    cc 73 127
    jitter_sleep 310 45
done
jitter_sleep 400 100

# 2. Clock ON — CC68 val127 held 6s, then release
echo "2/9  Clock ON (CC68 hold 6s)..."
cc 68 127
sleep 6
cc 68 0
jitter_sleep 700 150

# 3. Reverb ON (menu4 slot2) — wait 2.5s
echo "3/9  Reverb ON (CC76)..."
cc 76 127
jitter_sleep 2500 200

# 4. Granular ON (menu4 slot3) — wait 2.5s
echo "4/9  Granular ON (CC71)..."
cc 71 127
jitter_sleep 2500 200

# 5. Granular mix 100% (CC21 val=65 ×60 CW steps, init 0.5 → 1.0)
echo "5/9  Granular mix 100% (CC21 ×60)..."
enc_steps 60 21 65
jitter_sleep 550 120

# 6. Spectral Delay ON (menu4 slot1, sets lastToggle=16) — wait 2.5s
echo "6/9  Spectral Delay ON (CC75)..."
cc 75 127
jitter_sleep 2500 200

# 7. SDL mix 100% (CC25 val=65 ×60 CW steps, init 0.5 → 1.0)
echo "7/9  SDL mix 100% (CC25 ×60)..."
enc_steps 60 25 65
jitter_sleep 550 120

# 8. SDL maxdel shortest (CC20 val=63 ×200 CCW steps, init 0.3 → 0)
echo "8/9  SDL maxdel shortest (CC20 ×200)..."
enc_steps 200 20 63
jitter_sleep 450 100

# 9. SDL feedback 0.99 (CC23 val=65 ×19 CW steps, 0.8 init + 19×0.01)
echo "9/9  SDL feedback 0.99 (CC23 ×19)..."
enc_steps 19 23 65

echo ""
echo "=== Chiller Ambient — ACTIVE ==="
echo "    reverb + granular (100%) + SDL (100%, shortest time, FB=0.99)"

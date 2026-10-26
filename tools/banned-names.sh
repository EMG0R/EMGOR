#!/usr/bin/env bash
# Pre-push guard: fail (exit 1) on NEW third-party character/game names in js/ and docs/.
# Working names (MARO/JOSHI/STEEV/SONIK, POKEMANS, GOR BALL) are not matched.
# Pre-existing mentions are grandfathered in tools/banned-names.baseline (file<TAB>line text,
# no line numbers). Regenerate with: tools/banned-names.sh --baseline  (review the diff!)
cd "$(dirname "$0")/.." || exit 2
BASE=tools/banned-names.baseline
# case-insensitive terms
CI='nintendo|mario|luigi|yoshi|bowser|zelda|hyrule|pok[eé]mon|pikachu|kirby|metroid|samus|donkey kong|smash bros|sega|sonic|knuckles|eggman|robotnik|mojang|minecraft|creeper|enderman|herobrine|zombie pigman|redstone|goomba|koopa|chaos emerald|power star|gotta go fast'
# case-sensitive terms (common words otherwise)
CS='\b(Link|Peach|Tails|Toad|Steve|Notch)\b'
SKIP='^(tools/banned-names|docs/GORCAVE-BUILD-HANDOFF.md|docs/GORCAVE-QUEUE.md|docs/GORCAVE-LOG.md|docs/mashup-plan.md)'
scan() {
  { grep -rIiE "$CI" js docs 2>/dev/null; grep -rIE "$CS" js docs 2>/dev/null; } |
    grep -vE "$SKIP" | grep -v 'banned-ok' |
    awk -F: '{f=$1; sub(/^[^:]*:/,""); print f "\t" $0}' | sort -u
}
if [ "$1" = "--baseline" ]; then scan > "$BASE"; echo "baseline: $(wc -l < "$BASE") lines"; exit 0; fi
new=$(scan | grep -vxFf "$BASE" 2>/dev/null)
if [ -n "$new" ]; then echo "banned third-party names (new):"; echo "$new" | cut -c1-200; exit 1; fi
echo "banned-names: clean"

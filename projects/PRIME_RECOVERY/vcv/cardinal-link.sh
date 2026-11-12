#!/bin/bash
# cardinal-link.sh -- idempotent linker for Cardinal. PROPOSED, NOT RUN.
#
# Links ONLY:
#   MIDI : "Cardinal Macros" (cardinal-ctl's private virtual port) -> Cardinal:events-in
#   AUDIO: Cardinal out L/R -> HAT playback_AUX0/AUX1      (only with --audio / CARDINAL_LINK_AUDIO=1)
# Never deletes a link (no `pw-link -d`), never touches csound6:*, never links Cardinal inputs,
# never links anything else. Re-running is a no-op when the links exist, and re-creates them
# after a HAT reset / Cardinal restart (cardinal-ctl calls `--once` every 5 s while Cardinal is up).
#
# PREFERRED audio path is graph.conf (graph.conf.cardinal.proposed): the launcher's
# scrub_hw_playback() deletes any link into a hardware playback port that is not in
# graph.conf/desired, so a link made only by this script gets removed on the next graph event.
# graph.conf lines are also what survive a HAT stream reset. --audio is for a one-off test only.
#
# Usage: cardinal-link.sh --once [--audio]     (default mode; nothing loops here)
#        cardinal-link.sh --dry                (print what it WOULD link; still only lists the graph)
set -u
AUDIO="${CARDINAL_LINK_AUDIO:-0}"; DRY=0
for a in "$@"; do case "$a" in --audio) AUDIO=1;; --dry) DRY=1;; --once) ;; esac; done
HAT_OUT="alsa_output.platform-soc_107c000000_sound.pro-output-0"
CLIENT="${CARDINAL_JACK_CLIENT:-Cardinal}"

outs="$(pw-link -o 2>/dev/null)"; ins="$(pw-link -i 2>/dev/null)"; links="$(pw-link -l 2>/dev/null)"
[[ -z "$outs" ]] && exit 0                       # PipeWire not answering: do nothing

linked() {  # linked SRC DST -> 0 if the link exists
  awk -v s="$1" -v d="$2" '
    /^[^ |]/ {cur=$0; sub(/[ \t]+$/,"",cur); next}
    /\|->/ {t=$0; sub(/^[ \t]*\|-> /,"",t); sub(/[ \t]+$/,"",t); if (cur==s && t==d) f=1}
    END{exit f?0:1}' <<<"$links"
}
link() {  # link SRC DST [pw-link flags]
  linked "$1" "$2" && return 0
  if [[ $DRY == 1 ]]; then echo "would link: $1 -> $2"; return 0; fi
  pw-link ${3:-} "$1" "$2" 2>/dev/null
}

# MIDI
msrc="$(grep -m1 'Cardinal Macros' <<<"$outs")"
mdst="$(grep -m1 -E "^${CLIENT}:(events-in|midi|.*midi.*in)" <<<"$ins")"
[[ -n "$msrc" && -n "$mdst" ]] && link "$msrc" "$mdst" -p      # passive: does not by itself keep Cardinal active

# AUDIO (opt-in)
if [[ "$AUDIO" == 1 ]]; then
  mapfile -t co < <(grep -E "^${CLIENT}:" <<<"$outs" | grep -iE 'out|output|playback' | grep -viE 'midi|events' | sort -V | head -2)
  if [[ ${#co[@]} -ge 2 ]]; then
    link "${co[0]}" "${HAT_OUT}:playback_AUX0"
    link "${co[1]}" "${HAT_OUT}:playback_AUX1"
  fi
fi
exit 0

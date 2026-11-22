#!/bin/bash
#
# claude_midi.sh — SSH-friendly MIDI injector for NEPTR Phase 4.
#
# Lets a remote operator (typically Claude over SSH) send MIDI events
# directly to the running csound effect engine without touching the
# Teensy. Works by writing raw MIDI bytes to the kernel "Midi Through"
# rawmidi port; csound (-Ma) is already listening on it, alongside the
# Teensy.
#
# Channels follow the Teensy convention (1-indexed in human language,
# 0-indexed on the wire). All Teensy buttons live on ch1; the master
# clock broadcast lives on ch16.
#
# Usage:
#   ./claude_midi.sh cc <num> <val> [ch]     # send a CC (default ch1)
#   ./claude_midi.sh on  <note> <vel> [ch]   # send a note on
#   ./claude_midi.sh off <note> [ch]         # send a note off
#   ./claude_midi.sh tap <cc> [ch]           # quick press+release of a button CC
#   ./claude_midi.sh raw <hex bytes…>        # send arbitrary raw bytes
#   ./claude_midi.sh ports                   # show available ALSA seq ports
#   ./claude_midi.sh map                     # cheat-sheet of useful CCs
#
# Examples:
#   ./claude_midi.sh tap 74        # tap Enc5 button (slot-0 effect toggle)
#   ./claude_midi.sh cc 119 64 16  # nudge master clock to ~120 BPM
#   ./claude_midi.sh cc 118 80     # ask demiurge-clock to set ~265 BPM
#   ./claude_midi.sh tap 68        # virtual press of arcade button (CC68)
#
# Exit non-zero on missing tools or unreachable port; safe to script.

set -euo pipefail

PORT_NAME="Midi Through"
DEFAULT_CHAN=1   # 1..16

die()   { echo "claude_midi: $*" >&2; exit 1; }
need()  { command -v "$1" >/dev/null || die "missing dependency: $1"; }

need aseqsend

# Resolve the Midi Through ALSA seq port (e.g. "14:0"). The "Midi
# Through" port is a kernel seq client (not rawmidi), so amidi can't
# reach it; aseqsend can. csound and synthi.ck are both subscribed to
# this port via aconnect, so a single send fans out to both.
port_addr() {
    aseqsend -l 2>/dev/null \
        | awk -v want="$PORT_NAME" 'index($0, want) {
            for (i = 1; i <= NF; i++) {
                if ($i ~ /^[0-9]+:[0-9]+$/) { print $i; exit }
            }
        }'
}

PORT="$(port_addr || true)"
[ -n "$PORT" ] || die "no ALSA seq port matching '$PORT_NAME' — check 'aseqsend -l'"

# 1-indexed channel → 4-bit nibble
chan_nibble() {
    local ch="${1:-$DEFAULT_CHAN}"
    [ "$ch" -ge 1 ] && [ "$ch" -le 16 ] || die "channel must be 1..16"
    printf '%X' "$((ch - 1))"
}

send_hex() {
    # This build of aseqsend only accepts a file path for -s (the
    # "hex encoded bytes" form in --help is a documentation lie — it
    # silently exits 0 after printing a "cannot open" warning, so the
    # caller has no way to notice the dropped event). Write the bytes
    # to a temp file and feed that.
    local tmp
    tmp="$(mktemp)" || die "mktemp failed"
    # shellcheck disable=SC2086
    printf '%b' "$(printf '\\x%s' $*)" > "$tmp"
    if ! aseqsend -p "$PORT" -s "$tmp" >/dev/null 2>&1; then
        rm -f "$tmp"
        die "aseqsend failed (port=$PORT bytes='$*')"
    fi
    rm -f "$tmp"
}

cmd="${1:-}"; shift || true
case "$cmd" in
    cc)
        num="$1"; val="$2"; ch="${3:-$DEFAULT_CHAN}"
        n="$(chan_nibble "$ch")"
        send_hex "B${n} $(printf '%02X' "$num") $(printf '%02X' "$val")"
        echo "sent CC ch${ch} #${num} = ${val}"
        ;;
    on)
        note="$1"; vel="$2"; ch="${3:-$DEFAULT_CHAN}"
        n="$(chan_nibble "$ch")"
        send_hex "9${n} $(printf '%02X' "$note") $(printf '%02X' "$vel")"
        echo "sent NoteOn ch${ch} note=${note} vel=${vel}"
        ;;
    off)
        note="$1"; ch="${2:-$DEFAULT_CHAN}"
        n="$(chan_nibble "$ch")"
        send_hex "8${n} $(printf '%02X' "$note") 00"
        echo "sent NoteOff ch${ch} note=${note}"
        ;;
    tap)
        num="$1"; ch="${2:-$DEFAULT_CHAN}"
        n="$(chan_nibble "$ch")"
        send_hex "B${n} $(printf '%02X' "$num") 7F"
        sleep 0.05
        send_hex "B${n} $(printf '%02X' "$num") 00"
        echo "tapped CC ch${ch} #${num}"
        ;;
    raw)
        [ "$#" -gt 0 ] || die "raw requires hex bytes"
        send_hex "$*"
        echo "sent raw: $*"
        ;;
    ports)
        aseqsend -l
        ;;
    map|"")
        cat <<'MAP'
NEPTR Phase 4 — useful MIDI CCs (channel 1 unless noted)

  Effect-toggle buttons  (encoder buttons, group "tap"):
    CC74  Enc5 — slot 0 effect toggle
    CC75  Enc6 — slot 1 effect toggle
    CC76  Enc7 — slot 2 effect toggle
    CC71  Enc2 — slot 3 effect toggle
    CC72  Enc3 — slot 4 effect toggle

  Menu navigation (encoder rotation, send 127=CW / 0=CCW once per step):
    CC20 / CC23 — Enc1 / Enc4 rotation (menu prev / next)

  Encoder rotations (slot params):
    CC24 (Enc5) / CC25 (Enc6) / CC26 (Enc7) / CC21 (Enc2) / CC22 (Enc3)

  Arcade button (Teensy button 9):
    CC68  press = 127, release = 0 — controls ChucK clock + note trigger

  Master clock (channel 16):
    CC119 ch16  BPM broadcast (read-only — clock writes this)
    CC118 ch16  BPM steer     (write to nudge demiurge-clock; 0..127 → 40..400)

  See csound/midi_handler.orc for the full mapping table.

Examples:
  claude_midi.sh tap 74            # toggle slot-0 effect (e.g. VOLUME)
  claude_midi.sh tap 68            # virtual arcade button press (plays note)
  claude_midi.sh cc 23 127         # menu: next
  claude_midi.sh cc 118 96 16      # demiurge-clock → ~310 BPM
MAP
        ;;
    *)
        die "unknown command: $cmd (try: cc | on | off | tap | raw | ports | map)"
        ;;
esac

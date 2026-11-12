#!/usr/bin/env bash
# iso_pmor_midi.sh SECONDS : keep PMOR's ALSA MIDI port disconnected from Midi Through (and so from the NEPTR
# engine hub) for SECONDS. demiurge-midi-connect.service reconnects within ~1 s, so this re-cuts every 50 ms.
# Bounded by `timeout`; the service restores the normal routing by itself afterwards. Test fixture only.
exec timeout "${1:-20}" bash -c 'while :; do aconnect -d 14:0 28:0 2>/dev/null; aconnect -d 28:0 14:0 2>/dev/null; sleep 0.05; done'

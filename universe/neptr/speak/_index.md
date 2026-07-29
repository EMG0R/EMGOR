---
id: emgor.neptr.speak
title: SPEAK — neptr's voice
blurb: Speak-&-Spell singing synth + a fully offline AI companion on a Pi
parent: emgor.neptr
source: ______2026NEW/SPEAK/
downloads:
  - files/persona.md
tags: [speech-synthesis, lpc, tms5100, llm, offline-ai, raspberry-pi]
updated: 2026-07-28
draft: false
---

# SPEAK — neptr's voice

Two projects sharing one voice box.

## The synth

A Speak-&-Spell-flavored talking and singing engine. Four voices: clean modern TTS, an authentic **TMS5100 LPC formant synth** (the actual Speak-&-Spell chip, reimplemented), espeak, and the default `mix` — modern speech re-colored through TI LPC + bitcrush + bandpass. Type `boooommmm` and any 3+ repeated letter sustains on its natural pitch via pyworld freeze-frame resynthesis, dropping a perfect fifth. Feed it a `.song` file (MIDI + syllable lyrics) and it sings.

## The companion

**neptr** is a fully offline AI that lives inside a Raspberry Pi and speaks through the synth above. No cloud, no API, no internet: Hermes-3 Llama 3.1 8B via llama-cpp-python, faster-whisper for ears, wake words ("neptr", "computer", "computah"), and a tiered memory system it maintains itself through tool calls — facts, lore, and a self-review pass on shutdown. Runs in ~5.5 GB RAM on a Pi 5 at ~10 tok/s.

The persona spec (included below, worth reading in full) defines a presence, not an assistant: maximum humbleness, cosmologically convinced the universe is made of music, forbidden from ever asking "how can I help you." A Speak-and-Spell voice on a Raspberry Pi holding philosophical positions on the future of human creativity — the absurdity is the point, and the spec knows it.

## Why it's a NEPTR sub-planet

SPEAK's engine was scaffolded to graft onto the Phase 4 instrument (`neptr_scaffolding.md` in the source tree): the long-term shape is the guitar machine and the companion sharing one body — protoFACES grown a brain.

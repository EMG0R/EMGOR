# OP FREAK v2 on-device guide (first real image)

Image: `build/opfreak_v2.op1` (recipe `recipes/opfreak_v2.json`, boot text `OP FREAK`). Nothing here has run on a device: every line below is
UNVERIFIED on hardware (host + Blackfin-simulator tested only, `tests/v2sim`). Flash and restore steps: README.md (same as the proof image, other file name).

What is in it: v1 (CHORD+, WARP, TRIO) + the probe overlay + THE PURP + the generative sequencer. Not in it: gain stage, LEGACY banks, purple theme,
clean samplers, LIL DRUMMER (reasons at the end).

## Probe overlay (unchanged)
Off by default. **shift + speech**: off, page 1, page 2, off. Read `TX PK` and `VOL K` on page 1 and write them down (they gate the gain stage).

## THE PURP
**Reach it.** Synth mode, open the synth engine list the same way you pick WARP or TRIO (it is the same list; engine rows in order: ... voltage, warp, trio, **purp**, the new last row).
Pick **purp**. (Or the preset browser: folder `purp`, preset `purp`.) The engine screen turns into the PURP screen (dark purple, `THE PURP`, preset name, `P n/10`).
Any later sound slot set to `purp` is a PURP sound; all of them share one synth.

**Keys that stay TE's (not touched):** T1 (engine screen = ours), T2 = TE envelope, T3 = FX, T4 = LFO, shift+T browsers. TE owns the amp envelope (attack / decay / sustain /
release on T2), velocity, volume, FX, LFO and the 6 voices.

**Controls on the PURP screen** (engine screen only; other screens untouched):
| input | does |
|---|---|
| turn blue / green / white / orange | the value of that column (~100 clicks full range) |
| shift + turn | GEN depth (bipolar) for that parameter; turning from 0 switches GEN on (DRIFT) |
| push (no shift) | cycle that parameter's GEN mode: DRIFT, STEP, LFO1, LFO2, OFF (the tag shows under the column). Not on the MATRIX page |
| shift + blue push | next page |
| shift + green push | previous page |
| shift + white push | RESEED (new drift / sample-and-hold material, same settings) |
| shift + orange push | FM ratio snap to integers on / off (on the MATRIX page: next matrix slot 1-4) |

**Pages** (blue / green / white / orange):
1. OSC: SHAPE A, SHAPE B, DETUNE (stereo spread, inaudible: output is mono), NOISE
2. FM: RATIO, INDEX, FEEDBK, NOISE>FM
3. FILTER: CUTOFF, RES, DRIVE, ENV AMT (MOD ENV, bipolar)
4. AMP ENV: ATTACK DECAY SUSTAIN RELEASE (**no effect**: bypassed, TE envelope on T2 is the amp envelope)
5. MOD ENV: ATTACK DECAY SUSTAIN (release does nothing: no release callback in the host)
6. LFO 1: RATE, SHAPE, DEPTH, SYNC (over 50 % = TE tempo sync)
7. LFO 2: same
8. MATRIX: SOURCE, DEST, AMOUNT (centre = 0), CURVE for the shown slot (4 slots; presets use slot 1)
9. VOICE: VOICES (no effect), GLIDE (no effect), DRIFT, MACRO (one knob: FM index + detune + noise + cutoff)
10. PRESETS: blue turn scrolls the 8 factory presets (ANALOG PAD, GROWL FM, NOISE FM, SUB BASS, RES LEAD, FM BELL, DRIFT STRINGS, PURP MACRO); the choice is stored in the sound.

**Sound identity and saving.** The sound stores only its preset number (knob slots 5-8 carry tag 1 + preset). Your edits and GEN settings live in RAM: they are lost at power off and when you
move to another PURP sound (it reloads its own preset). A `*` after the preset name means edited. Saving edits is future work.

**Known limitations (all documented in docs/ENGINES.md too):**
- Mono output, TE's envelope: the host calls the engine once per TE voice, mono, 88.2 kHz. The stereo DETUNE spread and the AMP ADSR are not audible. A stereo bus and own envelope need a platform change.
- 5 voices: TE has 6 voice objects, PURP has 5; the 6th simultaneous note is silent.
- 44.1 kHz core, upsampled 2x (linear). CPU: 47k cycles per voice block in the simulator for 1 voice (5 voices ~235k of ~580k per block, 40 %). Not measured on the device: watch the probe's CPU % and
  HOT/LATE with 5 notes held; if it is hot, use presets with few voices or report it.
- GEN modes TILT X/Y and SEQ are not offered (platform inputs not wired). The wavetable slot is a stub.
- Levels and preset taste have never been auditioned by ear.

## The generative sequencer (OP FREAK SEQ)
**Open it.** Synth playmode, press the **sequencer key** (the one NOTE ROLL uses), make sure **tombola** is the picked sequencer (it is the slot we took over; tombola's own controller ignores
every key, so the gesture conflicts with nothing). Press the **orange knob (push, no shift)**: the screen turns into OP FREAK SEQ. **shift + orange push** closes it. Leaving the sequencer screen
also closes the overlay; the sequence keeps running. Stock tombola underneath makes no notes unless you put balls on it (leave it empty).
Other keys on other sequencer types (pattern, endless, finger NOTE ROLL, sketch, arpeggio) are untouched.

**While open:** T1-T4 pick a page group (press the same T again to flip sub-pages), turn = value (shift = coarse), push = the page's action. Keyboard notes you play feed MARKOV and TOTAL.
| page | blue | green | white | orange |
|---|---|---|---|---|
| T1 MODE .1 / .2 / .3 | mode params 1-4 / 5-8 / 9-12 (EUCLID uses all three) | | | |
| T2 NOTE .1 | SCALE (Lydian default) | ROOT | OCTAVE | OCT SPREAD (0-100 = 0-5 oct) |
| T2 NOTE .2 | PROB | RATCHET | LEVEL | GATE |
| T3 TIME | BPM (display; TE's tempo is the master) | RATE | SWING | CLOCK INT / EXT / MASTER |
| T4 SYSTEM | MODE | SEED | RESEED | **RUN** |
Push resets the parameter to its default; on SEED / RESEED = new seed; **blue push on T4 = next mode** (TURING, EUCLID, MARKOV, GAMELAN, CELLULAR, TOTAL; all six reached in the simulator);
**orange push on T4 = RUN / STOP**; on MARKOV's CLEAR = wipe. It plays TE's synth voice (so PURP or any engine of the selected synth sound) at TE's tempo, synth playmode only (it stops in drum playmode).
MIDI clock in/out is stubbed in the adapter (not routed to a MIDI hook yet): INT clock only for now.

**Known limitations:** the sequencer is not saved; stock sequencers are not moved to a LEGACY bank (unproven); the overlay is drawn over the tombola screen with no TE chrome; the stuck-note
guard is simulator-checked only (after stop, note-offs >= note-ons - 8).

## What is left out of this image, and why
- **Gain stage** (`ofp_gain`): output word format unverified, a +24 dB risk. Needs `TX PK` / `VOL K` from the probe first.
- **LEGACY-bank hiding of stock engines / sequencers:** unproven runtime hiding; stock engines all still sit in their original lists.
- **LIL DRUMMER:** the engine host only hooks the **drwave synth** voice (tag in knob slots 5-8). Drum voices are a separate engine (`drum`/`dbox`, 12 voices, vtable `0x0190fa64`); a drum host needs
  (1) the drum voice render + note_on pointer slots found and redirected as DRUM ROLL does, (2) a tag inside a drum preset that TE's "Invalid drum patch" validator accepts (unchecked), (3) a drum
  screen UI (key/knob/frame on the drum screen, T-key side effects untested), (4) a key-to-voice trigger. That is a new platform module, not glue. The core and adapter (`ld_sdk_*`) are ready.
- **Purple theme:** the research assumed a build-time SVG recolour. Measured on the stock files: the black in the SVGs is a fill (`#010101`, 128 uses; `#000000` 306; `#010000` in colors.svg) used for knock-out
  interiors, while the screen background is the code's clear colour (not in any SVG). Recolouring the SVG fills alone gives purple blobs on a black screen. A coherent theme needs the runtime pixel remap in
  the `frame` hook (research (f), UNVERIFIED cost), so it was skipped. (The PURP and SEQ screens do use the dark purple `#12001f`.)
- **Clean sampler rows:** needs a new factory audio file per row (`content/audio/factory_synth/<name>.raw`); whether TE's picker and sampler accept a 1-byte / empty factory sample is UNVERIFIED and a bad row
  can halt boot. Needs a device test with one spare row before shipping.
- Boot animation: only the 8-byte boot text `OP FREAK` (mechanism device-verified upstream with `op1sdk +`; this exact string unflashed).

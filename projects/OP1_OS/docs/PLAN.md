# OP FREAK plan (Emory's decisions, 2026-11-04)

The firmware is called **OP FREAK** (folder stays OP1_OS). It gets a flashy custom opening screen.

## THE PURP (flagship synth) spec
- 2 oscillators per voice, analog style (Moog-like, modern band-limited VA: polyBLEP/minBLEP, drift), split L/R for stereo detune.
- Full FM between them: non-integer ratios, through-zero; osc shape can morph toward wavetable (a slot for future tables). Growly FM + white noise as a modulation source.
- 5 voices max (to be confirmed by the CPU probe).
- Full ADSR(s), deep menu-based editing.
- Synth UI: replace the preset area with per-synth submenus (OSC / FM / MOD / ENV / FILTER...); the last page = preset scroller. Keep the 4 coloured encoders (the 1 2 3 4 workflow).

- Moog-style 4-pole ladder filter after the oscillators (resonance + drive). Locked.
- Tilt (G-sensor X/Y) hard-mappable to any parameter; LFOs accurately mappable to any parameter.

## Universal control grammar (every OP FREAK engine) — DRAFT v1
Per encoder (blue/green/white/orange):
- turn = value
- shift+turn = modulation depth on that parameter (bipolar)
- push (click) = cycle that parameter's GEN mode: OFF → DRIFT (slow seeded random walk) → STEP (seeded sample&hold, clock-synced) → LFO1 → LFO2 → TILT X → TILT Y → SEQ (follows the generative sequencer per step) → OFF
- hold push + turn = GEN rate/amount; long hold = open the mod-source picker for that param (any source incl. MIDI CC from DUODECIMUS)
- every GEN mode is seeded per preset (same preset = same motion; RESEED action for new material)
Page navigation: T1-T4 pick a page group, pressing the same T again flips sub-pages (UNVERIFIED: depends on overriding the T keys in synth mode).

## THE PURP pages (blue / green / white / orange)
- T1 SOUND: OSC (shape A, shape B, detune/stereo spread, osc↔noise mix) · FM (ratio [push = snap to integer], index, feedback, noise→FM)
- T2 SHAPE: FILTER (cutoff, resonance, drive, env amount) · AMP ENV (A D S R) · MOD ENV (A D S R)
- T3 MOVE: LFO1 (rate, shape, depth, sync) · LFO2 (same) · MATRIX (slot: source, dest, amount, curve)
- T4 VOICE: voices 1-5, glide, analog drift, PURP macro (one knob: FM index + detune + noise + cutoff) · PRESETS scroller (last page)

## LIL DRUMMER (808-ish FM drum synth) — DRAFT v1
Voices: KICK (sine + pitch env + FM click), SNARE (2 tuned ops + noise), CLAP (noise bursts), CLOSED/OPEN HAT (808-style metal cluster/FM, stereo spread), TOM/CONGA, RIM, COWBELL, ZAP (FM perc).
LOCKED: all 24 keys are fully editable, each with a MODEL parameter (kick → tom → snare → clap → rim → cowbell → hat → zap) that morphs the synthesis topology and biases parameter ranges/defaults toward that archetype's classic technique; any key can be any sound. The default kit lays models out like an 808.
Press a drum key to select its voice, then pages:
- T1 BODY: tune, decay, punch (pitch env amount), level
- T2 FM: ratio, index, feedback, noise
- T3 TONE: drive, filter, snap/transient, pan/width
- T4 KIT: choke groups, swing link, kit-level macro · PRESETS scroller
The same per-encoder GEN grammar applies, so FM ratio/index can wander per hit, seeded.

## Character + output
- Overall sound: super gritty and raw. Drive/saturation stages are first-class; nothing polished by default.
- Volume knob: output pinned at max; the knob becomes a bipolar gain trim -100..+100 (0 = unity, negative = attenuation down to silence, positive = boost). Above 0: hybrid curve. A smooth analog-style saturation first (warmer than classic OP-1 gain, same feel), crossfading into a Fruity-Soft-Clipper-style soft clipper near +100. It has a hard safety ceiling below 0 dBFS (never wraps or overs), is DC-blocked, and costs fixed CPU (never crashes or crackles).

## Look
- Purple theme: black → dark purple across the UI (OLED caveat: lit background = battery + burn-in; keep it very dark).

## Layout
- **Synth row (main):** THE PURP (deep flagship synth), clean sampler (truly empty duplicate of the stock sampler, no sounds loaded).
- **Synth LEGACY bank:** the old OP-1 exactly as it is now: stock synths + stock sampler with all of Emory's existing sounds, plus WARP, TRIO and the other SDK engines. Nothing deleted; presets keep working.
- **Drum row (main):** LIL DRUMMER (drum synth), clean drum sampler (truly empty duplicate).
- **Drum LEGACY bank:** stock drum sampler + stock drum engines with all existing kits, exactly as now.
- **FX:** stay as is (stock + SDK).
- **Generative sequencer modes (classic principles, all switchable):** TURING (looping shift register + lock knob), EUCLID (hits/steps/rotate, stackable), MARKOV (basic: every time you hit note A then note B, that transition's weight goes up; hit it more = more likely), GAMELAN (interlocking kotekan pairs), CELLULAR (automata grid → pitch/gate), and TOTAL (fully self-running generative: no input needed, slowly evolves its own material).
- Universal params across all modes: scale lock (Lydian default), probability, ratchet, swing, seed/RESEED, and OCTAVE SPREAD (linear 0 → 5 octaves, i.e. the full 0-5 V range at 1 V/oct).
- **Sequencers (main):** Emory's generative sequencer is the ONLY main one. Stock + SDK sequencers move to a LEGACY bank.

## Platform
- Stereo everywhere: 4-track stereo tape (~180 s/track, same memory as stock mono 360 s), stereo mixer, stereo mixdown. NOT built yet; v1 runs the stock mono tape.
- USB audio: root-cause the stock crackle with a probe build (underruns, clock drift, CPU peaks), then fix. NOT built yet; v1 runs stock USB audio.
- DEMIURGE integration: stereo USB audio into the Demiurge graph via live.conf (no bypass), MIDI clock both ways, sequencer can drive NEPTR.

## Prototyping
- Emory prototypes in Max (gen~ preferred: ports near line-for-line to C) or describes verbally. Built and played on the Mac first, then ported to C, simulator-tested, flashed.

## Status
- v0 proof: flashed + restored path proven on device 2026-11-04.
- v1 (WARP, TRIO, CHORD+): flashed 2026-11-04.

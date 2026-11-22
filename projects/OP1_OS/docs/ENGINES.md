# OP FREAK sound engines: THE PURP and LIL DRUMMER

Code: `src/common/` (shared layer), `src/engines/purp/`, `src/engines/lildrummer/`. Spec: `docs/PLAN.md`.
Build/test: `cd src/engines && make test demos bench bfin sim` (needs `scripts/prep_env.sh` once; outputs go to `build/`).

## Arithmetic: fixed point, not float
The OP-1's Blackfin BF524 has no FPU (softfloat is ~50-100 cycles per op) and every SDK engine (WARP, TRIO, MONO 3, LOFI)
is integer. So all DSP is integer C: audio Q15 in int32, params int16 0..32767 (bipolar centre 16384), phase uint32,
pitch = note number Q8. Same source runs on the host (clang) and bfin-elf-gcc and gives **bit-identical output**
(`checksum` test: host and Blackfin simulator both print `purp 0x287ea15a lildrummer 0xf271f01b`). No libc, no
divides in sample loops. Denormals/NaN cannot occur; boundedness is enforced by clamps and verified by fuzz under UBSan.

## src/common/op_common.[ch]
- `op_init(sr)` (44100/48000/88200/96000), `op_note_inc`, `op_inc_to_note`, `op_g_from_note` (tables from `gen_tables.py` -> `op_tables.h`).
- seeded PRNG `op_prng` (xorshift32, `op_seed`), fast sine/exp2, `op_sat3` (1.5x-0.5x^3 soft clip, C1), `op_drive`, `op_drive_asym`, `op_dc` (DC blocker).
- `op_adsr` (linear attack, squared decay/release), `op_lfo` (sine tri saw square S&H smooth; free 0.05-51 Hz or synced to bpm).
- **GEN** (`op_gen`, `op_gen_state`, `op_gen_in`): per-parameter modes OFF / DRIFT / STEP / LFO1 / LFO2 / TILT X / TILT Y / SEQ,
  bipolar `depth`, `rate` 0..127. DRIFT = seeded random walk (8 s .. 40 ms per segment), STEP = seeded sample&hold (follows
  `in.step_tick` when `in.clock_valid`, else its own clock), SEQ/TILT read the `op_gen_in` struct the platform fills
  (`pu_set_inputs` / `ld_set_inputs`). Same seed = same motion; RESEED = `pu_reseed` / `ld_reseed`.

## THE PURP (`purp.h`)
5 voices, stereo. Per voice and per output side (L chain detuned -d, R chain +d): osc B (modulator, self feedback) -> phase-modulates
osc A (through-zero PM, index up to 3 cycles, noise -> FM) -> A + B (B level falls as index rises) -> osc/noise mix -> 4-pole
trapezoidal Moog ladder (resonance, input saturator in the feedback loop, drive) -> amp. Oscillators are polyBLEP (steps) +
polyBLAMP (triangle kinks), run correctly with the phase running backwards under FM; shape morph saw -> square -> triangle; seeded
per-voice analog drift on pitch A/B and cutoff; 2 LFOs; AMP/MOD ADSR; 4-slot matrix (11 sources, 13 destinations, 4 curves);
glide; PURP macro (FM index + detune + noise + cutoff). Wavetable: `pu_set_wavetable()` is a stub slot (stored, unused).
Ratio is exponential 0.25..16 (non-integer); `pu_set_snap(1)` snaps it to integers (push on RATIO). Ladder cutoff is capped
at fs/4 (keeps stage states bounded by the input; no clamps needed). Resonance self-oscillates at 100 %.

Parameters (`pu_info[]` has names + sub-page; 4 per sub-page, encoder order blue/green/white/orange):

| page | sub-page | params |
|---|---|---|
| T1 SOUND | OSC | SHAPE A, SHAPE B, DETUNE (stereo spread), NOISE mix |
| | FM | RATIO, INDEX, FEEDBK, NOISE>FM |
| T2 SHAPE | FILTER | CUTOFF, RES, DRIVE, ENV AMT (bipolar, MOD ENV) |
| | AMP ENV | ATTACK DECAY SUSTAIN RELEASE |
| | MOD ENV | ATTACK DECAY SUSTAIN RELEASE |
| T3 MOVE | LFO 1 / LFO 2 | RATE, SHAPE, DEPTH, SYNC (>50 % = bpm sync) |
| | MATRIX | SOURCE, DEST, AMOUNT (centre 0), CURVE for the active slot (`s->mx_active`, `pu_set_matrix` for any slot) |
| T4 VOICE | VOICE | VOICES (1-5), GLIDE, DRIFT, MACRO; then the preset scroller (8 factory presets, `pu_preset`) |

API: `pu_init/pu_reseed/pu_preset/pu_set_param/pu_set_gen/pu_set_matrix/pu_set_snap/pu_set_inputs`, `pu_note_on/off/all_off`,
`pu_render(L,R,n)` (any n, output +-32767). Per-param GEN is applied block-rate in `pu_global_update`; KEY/VEL/envelope/noise/drift matrix
sources are per voice.

## LIL DRUMMER (`lildrummer.h`)
24 keys, each a full voice. Params (16, `ld_param_names`): T1 BODY tune, decay, punch, level | T2 FM ratio, index, feedback, noise |
T3 TONE drive, filter, snap, pan | T4 KIT model, choke, width, macro (macro is kit-level, `ld_set_param` ignores the key for it).
Deviation from PLAN: pan and width are two params (PAN on T3, WIDTH on T4); swing link is `kit.swing` (for the sequencer), not a knob.
**MODEL** (0..32767) morphs between 8 archetypes (KICK TOM SNARE CLAP RIM COWBELL HAT ZAP; `LD_MODEL_AT(m)` = exact archetype, fractions blend
every archetype field): body pitch + pitch-env depth/time, amp/noise decay, FM index/ratio/time, body/noise/metal-cluster weights, noise/metal
filter (Chamberlin SVF, band-pass <-> high-pass, cutoff, Q), clap burst count, click, drive. Voice = sine FM pair (+ feedback, noise>FM) +
6-square 808 metal cluster + filtered noise + click, asym drive, pan, width (decorrelated noise, phase-offset metal, mid/side). Params default
to 16384 = the archetype's classic setting (multiplicative), so any key can be any drum. Choke groups 1-7 (closed/open hat share group 1).
GEN on every param is evaluated **per hit** (STEP draws a new seeded value every hit, DRIFT walks with elapsed time), so FM ratio/index/model
wander hit to hit. `ld_default_kit` = 808 layout (kick, sub kick, snare, tight snare, clap, rim, 3 toms, conga, closed/open hat, cymbal,
cowbell, clave, maracas, 2 zaps, FM kick, FM snare, metal, bell, noise, blip); keys 10/11/15/20 ship with GEN wander.

## Performance (Blackfin simulator, `make sim`; cycles are per output sample unless noted; budget ~400 MHz)
Block budget on device: 128 samples @ 88.2 kHz = 580k cycles for everything (RESEARCH.md). Reference: MONO 3 22.9k, TRIO <=25k cycles per voice block.

| case | cycles/sample | note |
|---|---|---|
| PURP 1 voice stereo | 782 | |
| PURP 1 voice mono (DETUNE 0) | 494 | one chain |
| PURP 5 voices stereo | 2899 - 3643 (RES LEAD .. NOISE FM) | incl. shared global update; = 128-146 MHz at 44.1 kHz, 256-322 MHz at 88.2 kHz |
| PURP SDK-adapter voice block (44.1 kHz core, 64 samples, 1 voice) | 41.8k per host block | 5 voices = 209k = 36 % of the block budget |
| LIL DRUMMER per active voice, width on | KICK 335, SNARE 535, CLAP 534, HAT 791, COWBELL 690, ZAP 798 | idle keys cost 0; 4 overlapping voices ~60-100 MHz at 44.1 kHz |

Host (M4 Max, -O2, ~4.5 GHz assumed): PURP ~54-65 ns per voice-sample (~245-290 cycles), mono ~37 ns (~167); LIL DRUMMER ~22-26 ns per voice (~100-115 cycles).
Recommendation: render both cores at **44.1 kHz** and upsample 2x for the 88.2 kHz host (the adapters do); 5 PURP voices stereo fit (36 %), 5 voices mono is estimated (not measured) at ~2300 cycles/sample, ~25 %.
Further levers if the CPU probe disagrees: VOICES param (3-4), DETUNE 0 (mono chain), render PURP at 22 kHz, skip the R chain per preset.

## Testing (`make test`)
`test/fuzz.c`: unit tests (pitch accuracy, monotonic increments, G range, drive bounds, GEN determinism + RESEED, ADSR/LFO extremes) + 300 PURP and
300 LIL DRUMMER fuzz runs at 44.1/48/88.2/96 kHz: every param (and GEN mode, depth, rate, matrix slot, inputs, note 0-135, vel) at 0/1/32766/32767/16384/random,
mid-stream changes, odd block sizes; asserts output within +-32767, no rail-stuck output, no stuck voices after release, determinism per seed. Built with
`-fsanitize=undefined,address -fno-sanitize-recover=all`: any signed overflow aborts. `test/adapter_test.c` drives the SDK adapters through a stub host.
Demos (`make demos`, `build/demos/*.wav`, stereo 44.1 kHz): `purp_analog_pad`, `purp_growl_fm`, `purp_noise_fm`, `purp_filter_sweep_res` (res 92-98 %),
`purp_drift_spread`, `purp_fm_bell`, `purp_macro_rise`; `lildrummer_808_groove`, `lildrummer_fm_wander_hats`, `lildrummer_model_morph`, `lildrummer_kit_tour`.
Measured: oscillator non-harmonic energy -47 dB (saw 1 kHz), -45 dB (square), -59 dB (triangle) vs total at 44.1 kHz.

## Registering with the platform (SDK engine host)
The host (`features/enginehost`) calls `eh_render_fn(vi, out[128], knobs, inc[128], flags)` per TE voice: mono, 88.2 kHz, raw full-length note (TE applies envelope,
velocity, volume, FX). Sources to add to a feature (`sources` in feature.json, `cflags: ["-O2", "-fno-tree-loop-distribute-patterns"]`, include dirs `src/common`, `src/engines/purp`,
the enginehost dir; `src/common/op_common.c`, `purp/purp.c`, `purp/sdk_adapter.c`):
1. call `pu_sdk_init()` from the feature `init` (it does `op_init(44100)`, seeds the synth, sets `ext_env`, and `eh_register(PU_SDK_TAG, ...)`; default tag **1**, the only free host tag; override with `-DPU_SDK_TAG=n`).
2. tag/preset plumbing: a drwave sound is PURP when knob slots 5-8 hold `(tag<<12)|v` (`eh_p2`); presets/pages edit the shared `pu_synth` returned by `pu_sdk_synth()` (UI pages from `pu_info[]`, `pu_subpage_names[]`). The 8 TE knob slots are ignored apart from the tag.
3. TE owns the amp envelope in this mode (`ext_env`): AMP ADSR is bypassed, MOD ENV has no release. Pitch comes from TE's per-sample increment buffer. A stereo bus can call `pu_sdk_render_stereo()` instead.
LIL DRUMMER: `ld_sdk_init()`, `ld_sdk_trigger(key, vel)` from the drum key hook, `ld_sdk_render(out, 128)` once per audio block (mono int16 @88.2k, 2x upsample from the 44.1k core); or define `-DLD_SDK_TAG=n` for an
`eh_render_fn` (key = note - 53 wrapped to 24; only the first voice of a block mixes the kit). Which one the drum row uses depends on the platform's drum-host research. No recipes are created here.
Both cores use `op_init` globally: the whole firmware build must use one rate (44100).

## Known limits / next
- Wavetable slot is a stub; the shape knob does not morph to tables yet.
- Mono host output until the platform has a stereo bus (`pu_sdk_render_stereo`, `ld_render` are stereo already).
- Aliasing: polyBLEP is 2-sample; heavy FM above ~0.19 cycles/sample skips band-limiting on purpose (grit, and the correction is meaningless there).
- Tilt/seq/clock inputs are zero until the platform fills `op_gen_in`.
- Not auditioned by ear (analysis + fuzz only); levels and preset taste need a listen.

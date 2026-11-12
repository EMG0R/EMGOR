# OP FREAK generative sequencer (src/seq/)

Portable C core, host-tested, bfin-elf-compiled. No malloc, no libc, no float or 64-bit math in the
core (`sdk_adapter.c` reads TE_BPM as float, like NOTE ROLL). `seq_host` is 3.3 KB of static RAM;
core code is ~17 KB.

Build + test: `make -C src/seq test` (host), `make -C src/seq bfin` (cross-compile objects).
Demos land in `build/demos/seq_<mode>.mid` (type 0, 480 PPQ, 120 bpm, channel 1).

## Architecture

One host, six mode plugins behind `seq_mode_desc` (init, reseed, step, note_in, action, viz) plus a
param table. The host owns everything universal; a mode only answers "what plays on step N":
`seq_step_out` = up to 3 notes, each a scale degree (or pitch class for MARKOV), an octave-position
byte, a velocity. The host then applies: probability, ratchet, swing, gate length, scale lock, root,
octave spread, level; schedules note-on/off in a fixed 64-entry pending list; fills an event ring
(256) and the mod struct.

Clock: 96 ticks per quarter. `seq_run_frames(h, 64, 44100)` (internal, one call per audio block),
or `seq_ext_pulse()` per MIDI clock (24 PPQN = 4 ticks) with `seq_ext_start/continue/stop`.
`CLOCK` param: INT / EXT / MASTER. Clock out is a stub: set `h->clock_io` (pulse/start/stop
callbacks); in MASTER it sends 24 pulses per quarter plus start/stop. DEMIURGE or PMOR can be master
(EXT) or follower (MASTER). The adapter takes TE's tempo (`TE_BPM`) as the internal tempo.

Determinism: one seeded xorshift32* stream (`seq_rng`), a fixed number of host draws per step.
`seq_start()` rewinds and reseeds, so seed + params + note-in = identical event stream.
`seq_reseed(seed)` = new material, same params. The UI RESEED action walks the seed with an LCG.

API summary (`seq.h`): `seq_init`, `seq_set_mode`, `seq_start/stop`, `seq_run_frames`,
`seq_tick_one`, `seq_ext_*`, `seq_event_pop`, `seq_note_in`, `seq_reseed`, `seq_mod_get`,
`seq_ui_subpages/seq_ui_page/seq_encoder/seq_push`, `seq_viz_get`.

## Universal params

SCALE (lydian default, major, minor, dorian, mixolydian, phrygian, pent maj/min, blues, whole,
harm minor, slendro~, pelog~, chromatic), ROOT, OCTAVE (base C = 12 x 1..4), PROB, RATCHET (% of
steps that ratchet x2-x4), SWING (100 = odd steps half a step late), GATE (1-200 % of step),
OCT SPREAD (0-100, linear 0 -> 5 octaves; each note is displaced upward by `oct_position x spread`
scale degrees, so 100 spans the full 0-5 V at 1 V/oct above the base octave, 0 = one octave), RATE
(1/8, 1/16, 1/32, 1/8T, 1/16T), BPM, CLOCK, LEVEL. Scale lock is absolute: every emitted note
is in the scale (tested over all scales and several roots); pitches fold down an octave if they would
pass 127.

## Modes

| mode | principle | params |
|---|---|---|
| TURING | looping shift register, LOCK 100 = frozen loop, 0 = random, between = % fresh random bit; pitch/octave/gate are functions of the register so a frozen loop repeats | LENGTH 2-16, LOCK, RANGE, DENSITY |
| EUCLID | Bjorklund; up to 3 stacked layers (hits/steps/rotate), layers play chord tones (SPACING degrees apart) | LAYERS, HITS/STEPS/ROT x3, MELODY, SPACING |
| MARKOV | learns pitch-class transitions from `seq_note_in`: A then B => w[A][B] += INC; generation draws proportionally to the row; FORGET decays every weight each bar; MUTATE = % scale-tone exploration; CLEAR action wipes; empty row falls back to played-most notes; empty matrix = silence; matrix survives mode switches | FORGET, MUTATE, LEARN, INC, CLEAR |
| GAMELAN | seeded balungan split into polos + sangsih (neighbour, +OFFSET degrees) on complementary masks, gong tone on the cycle downbeat | PATTERN (IMBAL/NOROT/SYNCOP), OFFSET, CORELEN, MUTATE, RANGE, GONG, SWAP |
| CELLULAR | 1D elementary rule on a ring, or 2D Life on 8x8 with a column sweep (rows = degrees, up to 3 notes); dead/frozen grid gets re-poked | DIM, RULE, WIDTH, RATE, RANGE, INJECT, DENSITY |
| TOTAL | needs no input: 16-step motif rewritten one step at a time (EVOLVE per bar), harmonic centre drift and phrase rewrites per 8 bars, density breathing over PERIOD bars; a played note moves the centre | EVOLVE, DRIFT, DENSITY, BREATHE, LENGTH, RANGE, CHAOS, PERIOD |

## Per-step value stream (`seq_mod.h`)

Refreshed on every step, rests included. Fields: `step`, `pitch` (0-65535 over the current pitch
window), `rnd` (fresh seeded draw), `walk` (slow seeded random walk), `aux` (mode scalar: TURING
register, EUCLID layer mask, MARKOV transition weight, GAMELAN part, CELLULAR population, TOTAL
density), `phase` (position in bar), `energy` (gate density, last 16 steps), `gate`, `vel`,
`ratchet`, `notes`, `note`, `accent`. Read with `seq_mod_get(h)` or set `h->mod_cb`. Engines'
SEQ GEN mode picks one field per parameter (suggested map: pitch -> filter cutoff, rnd -> FM ratio
snap, walk -> drift, energy -> amp/drive, aux -> mode specific).

## SDK adapter (`src/seq/sdk_adapter.c`)

Follows NOTE ROLL's pattern (seq_tick, on_key filter, on_knob filter, frame, init). Compiles with
bfin-elf-gcc. Not wired into any slot: no recipe/feature.json (platform builder owns those).
Hooks it exposes: `osq_init` (init), `osq_tick` (seq_tick: runs the clock per 64-frame block, plays
events through `TE_VOICE_NOTE_ON/OFF`, synth playmode only), `osq_key`, `osq_knob`, `osq_frame`,
plus `osq_set_open()` (the platform layer decides the gesture that opens the screen / takes the
sequencer slot), `osq_note_in()` (feed keyboard notes for MARKOV/TOTAL), `osq_midi_clock/start/stop`
(USB MIDI clock in), `osq_mod()` (value stream). Untested on device; slot takeover and the
"sequencer is the only main sequencer" move of stock sequencers to LEGACY are platform work.

## Control layout proposal (PLAN.md grammar)

Four coloured encoders (blue/green/white/orange = knob 0-3); T1-T4 pick a page group, pressing the
same T flips sub-pages; shift + turn = coarse (x10 on wide ranges); push = per-knob action.

| page | knob 1 blue | knob 2 green | knob 3 white | knob 4 orange |
|---|---|---|---|---|
| T1 MODE .1 | mode param 1 | 2 | 3 | 4 |
| T1 MODE .2 / .3 | params 5-8 / 9-12 (EUCLID uses all three; others 1-2 pages) | | | |
| T2 NOTE .1 | SCALE | ROOT | OCTAVE | OCT SPREAD |
| T2 NOTE .2 | PROB | RATCHET | LEVEL | GATE |
| T3 TIME | BPM (display; TE tempo wins in the adapter) | RATE | SWING | CLOCK INT/EXT/MASTER |
| T4 SYSTEM | MODE | SEED | RESEED | RUN |

Push: reset the parameter to its default; on SEED/RESEED = new seed; on MODE = next mode; on RUN =
start/stop; on an action param (MARKOV CLEAR) = trigger. Turning right on CLEAR/RESEED also triggers.
The universal per-encoder GEN grammar (push cycles GEN mode) applies to engine parameters, not to
the sequencer's own pages, so push here is free for these actions. Screen: mode name, T group.sub,
RUN/STOP, four labelled columns, and the mode's viz grid (Turing register, Euclid rings as rows,
Markov 12x12 matrix, Gamelan polos/sangsih rows, Life grid, Total motif) with a playhead.

## Tests (src/seq/test/test_seq.c)

Euclid vs Toussaint's table (up to rotation) and maximal evenness for all 528 k<=n<=32; determinism
per mode (same seed identical, other seed different, restart identical); internal-block clock ==
tick loop, external pulses == internal; Markov weights exactly INC x plays, long-run transition
share matches the weights, more plays = more likely, FORGET monotonic, CLEAR silent, matrix survives
mode switches; scale lock over modes x scales x roots; octave spread bounds and linear growth;
probability/ratchet/swing/gate behaviour; on/off balance with no stuck notes; clock-out stub;
TOTAL evolution (adjacent-bar match 0.93, match four minutes apart 0.41).

## Known limits

- Hardware untested; adapter unverified against a real slot.
- Euclid table rotations: patterns are rotations of Toussaint's (same necklace); 6 of 14 table
  entries also match the printed rotation.
- MARKOV learns pitch classes (octave-agnostic); octave comes from OCT SPREAD.
- External clock tempo is not estimated; transport follows FA/FB/FC.
- Float conversion and 32-bit divide go through libgcc on the Blackfin (SDK links -lgcc).

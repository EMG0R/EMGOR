# HABIT Echo Collector — Implementation Sign-off

Date: 2026-06-12
Spec: `2026-06-11-habit-echo-collector-design.md` · Plan: `2026-06-11-habit-echo-collector-plan.md`
Reference: `docs/reference/Habit_Manual_Chase_Bliss.pdf` (complete official manual) + MIDI manual

## What shipped

- **HABIT** (Chase Bliss Habit port): toggle ID **36**, **instr 64**, chain position
  20.5 (DELAY → HABIT → REVERB), file `csound/effects/habit.orc`.
  Two-stage engine: 2^23-sample (~175 s) always-recording mono memory ring with a
  stochastic AUTO-scan playhead (crossfaded jumps, depth/frequency from the
  expression pedal) → 2^22-sample (~87 s) echo ring, dual-tap click-free SIZE
  (50 ms–60 s exp, never pitch-warps), tape-aged feedback loop (wow 0.45 Hz ±2 ms,
  flutter 7 Hz ±0.35 ms, HP80 + LP4500, tanh; loop gain 0–1.08 → max repeats =
  stable infinite). Memory clears on re-engage via valid-sample counters (no wipe).
- **Controls:** TIME s1 toggle (logical CC75) · slot enc CC25 = mix · CC20 = size ·
  CC23 = repeats · expression CC29 = scan. OSC `/hab_state|mix|size|repeats|scan`.
- **Menu moves:** Spectral Delay (16) → PITCH s0 (CC74/CC24); Formant (5) is
  FILTERING s3 ONLY; UI PITCH row fixed to the 5-slot s4→s0 convention.

## Verification (Mac, offline)

`scripts/bench/habit_bench.csd` + `analyze_habit.py`, 4 numeric scenarios, all PASS:
1. Echo spacing tracks SIZE (0.98 s → 0.42 s hop), **zero off-pitch windows** through
   the size change.
2. Repeats=max: RMS settles ~0.35 and holds flat 20 s, peak 0.53, no runaway/decay.
3. Scan resurrects pre-recorded phrase (ghost RMS 0.034 vs 0.00005 quiet floor).
4. Instr restart: 12.5 s of max-depth scanning after re-engage leaks **zero**
   pre-restart audio.

Full-csd syntax check green (including concurrent TAPE/NAM files), UI parses.

## Bugs found & fixed by the bench

1. **Size portk → tap flapping:** smoothing `gkHabitSize` stepped the 5 ms
   quantizer dozens of times per turn, flipping crossfade taps mid-fade → ~0.5 s
   pitch smear. Fix: size is not smoothed; tap hops are gated until the previous
   fade settles and always jump to the latest size.
2. **HP140 killed infinite loops:** −7%/pass at 220 Hz. Fix: HP→80 Hz, loop gain
   max 1.05→1.08 so tanh (not filter decay) sets the loop level.
3. `dcblock2` → `dcblock` on the wet output (dcblock2 = 254-sample FIR latency,
   per the TAPE session's finding).

## Outstanding

- **On-Pi smoke test (plan Task 8.3–8.6)** — Pi was unreachable. After
  `sync_to_pi.sh` + service restart: journal/ftgen check (~100 MB RSS jump is the
  two ring buffers, expected), Habit toggle + pedal sweep via claude_midi.sh
  (physical cc73 = TIME s1), SpecDel-from-PITCH + Formant-in-FILTERING regression,
  CPU reading engaged (budget < ~8%/core).
- By-ear pass: scan event pacing (`0.3/p²` home-wait curve) and wow/flutter depths
  are bench-tuned constants — tweak to taste in `habit.orc` if the ghosts feel too
  shy/too busy.
- Future (spec §10): SPREAD second playhead, MANUAL scan mode, FEED/COLLECT, the
  other five modifiers.

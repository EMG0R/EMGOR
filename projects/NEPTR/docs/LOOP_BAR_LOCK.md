# Loop bar lock (2026-11-06)

Goal: loops land on the correct beat of PMOR's measure, not just its tempo, and stay there through tempo changes.

## Protocol
- PMOR keeps an absolute 16th counter (abs16); bar phase = abs16 % 16. 24 x 0xF8 per beat, tick 0 on the grid beat.
- Start at the next grid beat: SPP(abs16 mod 16384) then 0xFB Continue then tick 0. 0xFA Start only when abs16 mod 16384 == 0. SPP+Continue == Start-with-position.
- Every bar: SPP right before that bar's tick 0 (position check). Stop = 0xFC.
- `~/PRIME_RECOVERY/midi/pmor-clock-relay.py` (TempoMeter): SPP = position of the NEXT tick. tick_n is monotonic; on SPP it takes the value congruent to spp*6 (mod 16384*6) nearest the prediction (wrap-safe). Agreeing SPP changes nothing; a disagreement corrects tick_n and logs (rate-limited). First SPP mid-song (restart) recovers position with no Start. Continue without a preceding SPP (<0.5 s) = old behaviour (stops beats). Tempo window is never cleared by SPP.
- Relay -> engine (127.0.0.1:7770): `/clk/beat "if" [absolute beat, samples since it]` and `/clk/bar "ii" [bar, beat_in_bar]` (BEATS_PER_BAR = 4, one constant in the relay).
- Engine (midi_handler.orc instr 99): gkLpB = PMOR absolute beat position. First acquisition or no loop playing: snap integer + fraction. Loops playing: whole-beat error is snapped only after the existing 4-in-a-row agreement; fraction is pulled smoothly (0.4 s, capped 1.5 % rate). Frozen with the clock not live; not corrected while any channel records.
- looper.orc: at first lock a loop's length is rounded to whole beats (min 1; loops < 1 beat keep the quarter-beat snap) and gkLpRecB = integer beat. Length multiple of 4 beats keeps its bar position forever. gkLpBeatSamples uses the same tempo as gkLpB while locked.
- Record start: if lock is live and the press is > 0.15 beat (LP_RECQ_LATE) after a beat, recording is armed and starts on the next whole beat; closing while armed cancels. Within 0.15 beat late it starts at once and recB rounds back to that beat.
- The old anchor-slew re-lock runs only when gkLpBLock == 0. lp_relock (CC54) is still the switch, lp_relock_ofs_ms the latency offset.

## Invariants
- gkLpRecB and each loop's start are integer beats of the absolute count. Loop phase = (gkLpB - recB) mod lenB.
- gkLpB's integer part equals PMOR's beat index once /clk/beat has been received. Without /clk/beat (old firmware) gkLpB is an arbitrary count: lock works, bar position is not meaningful.

## Bench test (needs engine restart for the orchestra and relay; Emory's call)
1. Play PMOR with a clear accent on bar 1. Watch the relay log: no "SPP corrected" lines in steady state.
2. Record a 4-beat loop starting on PMOR bar 1 (accent in the loop). Check the loop downbeat coincides with PMOR's.
3. Tap tempo on PMOR several times, up and down. Loop downbeat must stay on bar 1.
4. Stop PMOR, Start again, and Stop/Continue mid-bar: the loop downbeat must come back on bar 1.
5. Restart the relay while PMOR plays: loop should land back on bar 1 at the next per-bar SPP, within a few beats.
6. Record-start: press REC mid-beat; recording starts at the next beat (up to 0.85 beat later).
Not verified live. Left: no UI display of /clk/bar yet; no pre-roll for presses just after a beat (<0.15 beat shift is accepted).

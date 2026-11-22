; MOOD II — Chase Bliss MOOD MkII emulation, SLIP-only (v7, 2026-07-14)
; ---------------------------------------------------------------------------
; Instr 189, toggle 66, CHASE BLISS menu (11) slot 3, OSC prefix "md2".
;
; ===========================================================================
; v9 (2026-07-15) — LOOPER REMOVED FROM MOOD'S SIGNAL PATH
;
; User: "Make the mood so it doesn't actually do the clock on the looper —
; I don't like that." MOOD II no longer reads the LOOPER (menu 10) at all:
; the MD2_LpRead UDO, the per-channel gkLpState/gkLpLen/gkLpRdPos/giLpBuf
; reads, and the kLoopSum mix into the ring are all DELETED. The clock/ZOH
; decimation engine now processes the LIVE INPUT (gaMD2_send, via the ring
; buffer) and nothing else. The looper's loops keep playing back through the
; chain's own looper stage, completely unaffected by MOOD — previously MOOD
; ADDED a clock-shifted copy of them on top of that normal playback, which
; is exactly what the user didn't like. As of v9 this file shares NO globals
; with looper.orc (nor with blooper.orc / mood.orc, as before). The
; GENERATIVE scheduler still quantizes its jump timing to
; gkChaseBlissSubdivSec (chase_bliss_clock.orc) — that is a shared tempo
; tick, not audio, and stays. Everything else (v8 nine-rung -24..+24 ladder,
; downward-only grit, stepped 6dB-bucket quantizer lattice, hard-step
; transitions, SLIP jitter, bypass behavior, CC29 clock scrub) is unchanged.
; Bench scn5/scn10 were rewritten to assert the NEW contract: loop audio
; must NOT appear in MOOD's wet bus at all (scn5: loop playing + silent live
; input => wet is silent; scn10: loop + live => wet contains the shifted
; LIVE material only). Historical references to the looper feed below (v6
; note 2, THE FIX bullet 3, etc.) are kept as history; this note supersedes
; them.
;
; ===========================================================================
; v7 (2026-07-14) — THE BOTTOM-RUNG COLLAPSE BUG ("crackly silent")
;
; User, after playing v6 on the rig: "wow mood is much better sounding, it
; just has this noise to it and goes totally crackly silent vibe at the
; bottom." "The bottom" = the two most-downward CLOCK rungs, -19 and -24.
; The v6 asymmetric-grit direction was RIGHT (and is fully preserved — the
; bottom rungs are still by far the grittiest/darkest, which is the point);
; the CALIBRATION of the bit-crush quantizer was broken. Two real defects,
; both in the quantizer, both fixed here.
;
; DEFECT 1 — THE DEAD ZONE (this is the "goes totally crackly silent").
;   v6's kQStep was a FIXED ABSOLUTE amplitude: 0.0025 at home, rising to
;   0.0525 at the -24 rung. A round-to-nearest quantizer with step Q has a
;   dead zone of +-Q/2: EVERY sample whose magnitude is under Q/2 rounds to
;   exactly zero. At the -24 rung that dead zone is +-0.026 — i.e. -31.6
;   dBFS. Any signal quieter than that (a decaying note's tail, a quiet
;   passage, anything that is not a hot transient) does not get "crushed": it
;   gets DELETED, in its entirety, to digital zero. And the samples that do
;   survive get MAGNIFIED up to a full +-0.0525 step, because rounding a 0.03
;   sample to the nearest multiple of 0.0525 gives 0.0525. So the output
;   degenerates into a three-level (-Q, 0, +Q) pulse train: hard square-wave
;   switching against total silence. Measured directly (bench scn14, a
;   -30 dBFS input, on the v6 code): the two bottom rungs produced exactly
;   3 distinct output levels, with 56-58% of all samples rounded to exactly
;   zero, and an output peak of 0.0525 — LOUDER than the 0.03 input. That is
;   precisely "noise ... goes totally crackly silent". It was not audible on
;   the bench before because the bench only ever tested a hot 0.22 tone,
;   where a 0.0525 step is a legitimate ~4-bit crush; the bug only exists
;   at the levels you actually play at.
;
;   THE FIX: the quantizer step is now RELATIVE TO THE SIGNAL'S OWN RUNNING
;   PEAK rather than a fixed absolute floor (kQFrac * kLev — see the GRIT
;   block in instr 189). This is what a bit-depth reducer actually is: a
;   constant number of BITS, i.e. a constant signal-to-quantization-noise
;   ratio, not a constant absolute floor for the music to fall through. The
;   curve is referenced to giMD2_QRef = 0.22 — the hot level the v6 curve was
;   tuned and benched at — so AT THAT LEVEL THE CHARACTER IS UNCHANGED (the
;   thing the user said sounded "much better" is preserved bit-for-bit), and
;   at every other level the crush now scales with the signal instead of
;   eating it. Verified: on a seam-robust level count, v6 and v7 measure
;   9/11/13/15 and 11/11/13/15 across the four down rungs at the reference
;   level — the same crush — while at -30 dBFS the dead-zone fraction drops
;   from 0.558 to 0.059 and the level count goes from 3 to 45.
;
; DEFECT 2 — THE QUANTIZED DE-CLICK RAMP (this is the periodic "noise").
;   v6 applied the SLIP window's ~2 ms edge de-click trapezoid (kWEnv) to the
;   ring sample BEFORE the quantizer. So the quantizer ate the anti-click
;   ramp itself: a smooth fade rendered in steps of 0.0525 is not a fade, it
;   is a staircase of hard 0.05 jumps. The de-click was therefore DESTROYED
;   at exactly the rungs where the step was largest, turning every single
;   window seam into a click source — which is why it got audibly worse the
;   further down the ladder you went, and why it read as a periodic crackle
;   riding on top of the sound.
;
;   THE FIX: kWEnv (and kHitEnv) are applied AFTER quantization, where they
;   behave as the smooth gain ramps they are meant to be. The declick works
;   again, at every rung.
;
; SUPPORTING DETAIL — the level follower must be SLOW. The quantizer lattice
;   has to stay STILL for a steady signal, or the grit dissolves into noise
;   modulation. A first cut used a fast (5 ms) per-sample peak follower; but
;   the -24 rung's read output is ~55 Hz (an 18 ms period), so a 5 ms attack
;   tracks the individual half-cycles, kQStep wobbles at audio rate, and the
;   lattice slides around underneath the signal — measurably, the ZOH hold
;   fraction at the down rungs fell from ~0.8 to ~0.3. The follower is
;   therefore updated once per k-block, with 60 ms attack / 600 ms release
;   (far slower than the lowest note the ladder can produce, so a sustained
;   tone sees a fixed lattice — a real bit-crusher — while a decaying note is
;   still tracked down by its own, much slower envelope). kQStep is frozen for
;   the duration of each k-block. See the comments at iLevAtk/iLevRel.
;
; The grit CURVE itself is untouched (0.18 + 0.82*(|semi|/24)^0.55, downward
; only): monotonic and bounded on [0.18, 1.0] across the down rungs, with no
; discontinuity or singularity anywhere on the ladder — it was never the
; problem. Nothing else changes: bypass, click-free LENGTH sweep, no stale
; audio, the exact +12/-24 ladder, hard-step
; transitions, GENERATIVE duty, CC29, the Mix=0 dry null, SLIP jitter, and
; the single-rate-variable (kRate) SR-mechanism all behave exactly as before.
;
; v6 CHANGE (2026-07-14, same day as v5, after playing v5 on the real rig):
; two user notes acted on:
;   1. "The mood should only start bitcrushing when it goes below the pitch,
;      like lower." GRIT (the bit-depth quantizer) is now DOWNWARD ONLY —
;      the up rungs (+7/+12) get zero extra crush. The READ kernel is now
;      also asymmetric: down = literal zero-order-hold (unchanged, this IS
;      the hold/grit), up = linearly interpolated (removes the point-
;      sampling grain too, "cleaner and brighter going up"). See GRIT and
;      THE ENGINE headers, and the v6 comments inline at the GRIT-amount and
;      READ blocks in instr 189, for the exact implementation + rationale
;      (real-world downsampling aliasing is far more audible than
;      upsampling aliasing — this leans into that physical truth).
;   2. VERIFIED (not changed): "the clock thing should be doing the pitch
;      shifting to the loops... by modulating the sample rate." Confirmed
;      already true and unchanged by this pass — there is exactly ONE rate
;      variable, kRate = pow(2, kSemi/12), and it drives BOTH the read
;      position's velocity (kSlipPh = kSlipPh + kRate every host sample,
;      which is what kRd is ultimately built from) AND, as the literal
;      arithmetic consequence of that same position moving slower/faster
;      than 1/sample, the ZOH hold-length on the way down. There is no
;      second, parallel "pitch shift amount" anywhere in this file — pitch
;      is 100% an emergent side-effect of the read rate, per the manual and
;      per the user's own framing. See "THE ENGINE" header and the inline
;      v6 READ comment in instr 189 for the line-by-line proof.
;
; This is a SECOND, faithful-in-spirit MOOD. The rig's existing "MOOD" (instrs
; 401-405, effects/mood.orc, TIME menu) is a loose granular/pitch/bitcrush
; playground and is LEFT COMPLETELY UNTOUCHED — no shared globals with it
; (that file owns gkMOOD_*/gaMOOD_*/giMOOD_*), and none with looper.orc
; (gkLOOP_*/gkLp*) or blooper.orc (gkBLP_*). v9: the read-only looper-global
; reads that used to live here are gone entirely (see the v9 note, above).
;
; ===========================================================================
; WHY THIS IS A REWORK (three live bugs + a user-requested architecture change)
;
; The v2 build (git history) was a real MOOD MkII clone: three independent
; wet-channel modes (REVERB/DELAY/SLIP), three loop-mode selectors
; (ENV/TAPE/STRETCH), a MODIFY knob per mode, and — the root of all three
; live bugs — its OWN always-on "micro-looper": a capture ring + a
; RECORDING/PLAYING/OVERDUBBING state machine that recorded audio into its
; own buffer independently of the toggle/bypass state, because the real
; pedal's micro-looper genuinely never turns off.
;
;   BUG 1 "toggling off does not turn it off": v2 had NO bypass at all — the
;     slot button drove a gesture state machine (tap/hold), not an on/off
;     switch, and instr 189 was scheduled with `schedule 189, 0, -1` i.e.
;     ALWAYS RUNNING regardless of the toggle. "Off" was never a real state.
;   BUG 2 "moving SIZE creates crackling": v2's SIZE drove loop LENGTH *and*
;     wet TIME simultaneously in ENGINE-SAMPLE units used directly as a delay
;     tap distance; smoothing the knob (port) does not stop the tap/read
;     pointer itself jumping across content when the tap distance changes —
;     it only slows the smoothing, not the underlying pointer discontinuity
;     (classic delay-time-zipper problem, compounded by feeding straight back
;     into a recirculating buffer).
;   BUG 3 "something recorded a while ago is still going": the always-on
;     capture ring (RECORDING state) and the wet channel's recirculating
;     ring/delay/reverb buffers kept being written to every k-block forever,
;     regardless of MIX or the slot button, because nothing ever stopped
;     instr 189. A "stale" loop or tail could not decay because the engine
;     that fed it never stopped feeding it.
;
; THE FIX, per the user's explicit direction, is architectural, not a patch:
;   - MOOD II's own recording-to-buffer behaviour and its RECORDING/PLAYING/
;     OVERDUBBING play-state machine are REMOVED ENTIRELY. No self-recording
;     micro-looper, no play/stop state, nothing that runs while "off".
;   - Toggle 66 is now a REAL bypass ($FX_BYPASS, same macro as every other
;     simple effect in the rig): OFF = instr 189 is turned off
;     (`turnoff2`), nothing runs, nothing writes any buffer, output is the
;     untouched dry send. This alone fixes bug 1 and bug 3 (no instrument
;     running = no possible stale audio, ever — see signal_chain.orc).
;   - In place of the micro-looper: a RING BUFFER for LIVE INPUT (so the
;     clock/rate shifting applies to what you are playing right now).
;     HISTORY: from v2 through v8 this ring ALSO received the LOOPER's
;     (menu 10) currently-playing loop audio, summed in read-only. That feed
;     was REMOVED in v9 at the user's request ("doesn't actually do the
;     clock on the looper") — see the v9 note at the top of this file. The
;     engine now processes live input only.
;   - LENGTH (ex-SIZE) changes are only ever committed to the live window at
;     its own natural refresh boundary (see "DECLICKING LENGTH"), which
;     removes the delay-tap-jump entirely rather than just slowing it down —
;     this is the bug-2 fix.
;
; ===========================================================================
; SLIP ONLY — everything else the pedal could do is gone
;
;   The user: "the slip mode classic sound" — REVERB and DELAY modes, and the
;   OFF/REVERB/DELAY/SLIP wet-mode selector, are REMOVED. MOOD II always runs
;   as an auto-sampler (classic SLIP): it continuously records the combined
;   input into a ring and continuously replays the most recent LENGTH-sized
;   window of it, refreshing that window on a cycle.
;
;   The REVERB algorithm the user liked ("actually perfect") is preserved
;   verbatim as a NEW standalone effect, csound/effects/grainverb.orc
;   (GRAINVERB) — built, benched, and NOT wired into the menus ("for later"
;   per the user). See that file for its own header.
;
;   ENV/TAPE/STRETCH (the micro-looper's loop-mode selector) is gone with the
;   micro-looper itself. LENGTH is a normal, always-live param (the user:
;   "have length be on env so remove that option" — there is no other option
;   now, so this is moot by construction).
;
;   MODIFY is REMOVED ENTIRELY (the user: "remove that mod option thing, idk
;   what that is doing"). SLIP no longer has an independent playback-speed
;   knob — CLOCK alone drives pitch/rate now (see CLOCK, below), which is a
;   simplification the user's own framing invites: CLOCK already "sets
;   MOOD's sample rate... for BOTH halves at once", so folding SLIP's speed
;   into CLOCK removes a redundant control rather than a real one.
;
;   ROUTING (INPUT / INPUT+LOOP / LOOP) is REMOVED. MOOD II always runs
;   INPUT only (v9 — it ran INPUT+LOOP from v2 through v8; the LOOP half of
;   the feed was removed at the user's request, see the v9 note above).
;   Dropping the 3-way selector remains one fewer knob for a pedal the user
;   wants "stripped down".
;
; ===========================================================================
; LOOPER READ — REMOVED (v9, 2026-07-15)
;
;   From v2 through v8, this section documented a read-only snapshot of all
;   5 LOOPER channels' current playback (gkLpState/gkLpLen/gkLpRdPos/
;   giLpBuf{1..5}L/R via the MD2_LpRead UDO, blooper.orc-style), summed into
;   the ring alongside the live input so the clock engine processed both.
;   The user rejected that behavior ("doesn't actually do the clock on the
;   looper — I don't like that"), so the entire path — the UDO, the 5
;   per-channel reads, and the kLoopSum mix — was deleted. MOOD II now
;   touches nothing owned by looper.orc. The loops are heard only through
;   the chain's own looper stage, unaffected by MOOD.
;
;   ONE PATH THAT REMAINS (by design, and by chain topology, not by any code
;   in this file): MOOD II's wet output feeds the chain BEFORE the LOOPER's
;   input stage (signal_chain.orc: MOOD II is stage 18.5, `gaLpInL = aPrevL`
;   happens later), so recording a NEW loop while MOOD's clock shift is
;   engaged still captures the shifted LIVE audio into that loop. What no
;   longer happens is the reverse: an EXISTING loop's playback is never
;   pulled into MOOD's engine.
;
; ===========================================================================
; THE ENGINE — v5 (2026-07-14): ONE unified zero-order-hold resampler, not a
; pitch-shifter with a bolted-on crusher
;
;   RESEARCH (done directly, no skill, per user request — see report for full
;   citations): the official MOOD MkII manual describes CLOCK as ONE thing,
;   not two stacked effects: "The CLOCK knob controls everything... it moves
;   in musical, harmonized steps." / "Lowering the sample rate from 64k to
;   32k will half-speed your micro-loop as well as your Wet Channel effect."
;   / "CLOCK is tone, length, and quality, all in one... Introduce aliasing."
;   The v4 "GRIT" pass (previous commit-in-progress on this file) got the
;   DIRECTION and the drama right but implemented it as two separate stages —
;   a clean linearly-interpolated varispeed READ (the pitch-shift) with a
;   symmetric ZOH-hold-and-quantize stage BOLTED ON AFTERWARD. That is
;   exactly a "pitch shifter with a crusher layered on top", which is
;   precisely what a player can hear as separate/thin rather than "the whole
;   thing just changed sample rate" (real user feedback after playing v4 on
;   the rig). The manual's language ("all in one") says the timing change and
;   the quality change must be THE SAME OPERATION.
;
;   THE FIX: the READ side is no longer linearly interpolated. It is now a
;   literal nearest-neighbor (zero-order-hold) sample at the ring index
;   int(kRd) — no fractional blend between kI0/kI1 at all. This single change
;   makes pitch and decimation-grit the SAME arithmetic fact rather than two
;   stages:
;     - kRate < 1 (CLOCK below home): kRd advances less than 1 host-sample
;       per output sample, so int(kRd) is IDENTICAL for several consecutive
;       output samples in a row — the exact same ring sample is repeated
;       (held) that many times. This is not a simulated "grit stage"; it is
;       literally what "playing back at a lower sample rate" means, applied
;       to the actual signal being read, not a copy of it.
;     - kRate > 1 (CLOCK above home): kRd advances MORE than 1 per output
;       sample, so int(kRd) SKIPS ring indices — point-sampling rather than
;       interpolating across them, which is the up-side equivalent aliasing
;       artifact (no synthesized in-between samples invented by smoothing).
;   A small rounding-quantizer (bit-crush) is layered on the ZOH'd sample —
;   this remains a second line of code, but it is applied to the SAME sample
;   the ZOH already produced (not a separate copy), and its purpose is the
;   "quality" half of "tone, length, and quality, all in one": real
;   sample-rate-reduction devices reduce word-length as well as clock rate,
;   and the manual explicitly bundles both under CLOCK. See GRIT, below, for
;   the exact curve.
;
;   The WRITE side is always clean/full-rate (writes every host sample, no
;   hold) — decimation happens ENTIRELY on read. AS OF v6 this is no longer
;   uniform for both directions: the read side is DELIBERATELY asymmetric
;   (down = ZOH hold, up = interpolated — see THE ENGINE v6 note at the top
;   of this file, and GRIT below), per explicit user feedback after playing
;   v5 on the rig. The single rate variable driving the read POSITION is
;   still exactly one thing either way (see the v6 top-of-file note) — only
;   the sampling kernel applied to that position differs by direction.
;   LENGTH's declick math (window edge trapezoid, the cold-start safety
;   clamp) is unchanged.
;
; ===========================================================================
; GRIT — curve and quantizer (v4, DOWNWARD-ONLY as of v6)
;
;   v6 (2026-07-14): GRIT (the bit-crush layer) is now exclusively a function
;   of downward distance from home. User, after playing v5 on the rig: "the
;   mood should only start bitcrushing when it goes below the pitch, like
;   lower." Upward rungs (+7/+12) get kGrit = 0 unconditionally — kQStep
;   collapses to the same near-transparent floor used at home. This is a
;   deliberate lean into a real physical asymmetry, not an arbitrary style
;   choice: real sample-rate-reduction aliasing is far more audible/harsh
;   when downsampling (holding/repeating samples, which folds high content
;   down as strong periodic artifacts) than when the effective read rate
;   increases (point-sampling ahead skips content but does not "fold"
;   anything back down into the audible range the same way) — forcing a
;   symmetric crush curve was fighting that acoustic truth, not honouring it.
;   The read KERNEL itself is also asymmetric now (see THE ENGINE v6 note):
;   down keeps the literal ZOH hold (the grit IS the hold), up is linearly
;   interpolated (removes the point-sampling grain on top of the pitch
;   shift), so "no bitcrushing going up" is enforced at two levels — the
;   quantizer AND the read kernel — not just one.
;
;   For DOWN rungs only, kGrit remains the same curved (not linear) function
;   of distance-from-home it was in v4/v5:
;     kGrit = 0.18 + 0.82 * (|semitone| / 24) ^ 0.55      (0 exactly at home
;                                                            and at/above home)
;   24 = the down-side range floor (see CLOCK). kGrit drives the bit-crush
;   quantizer step (kQStep, ~0.0025 near-transparent at home/up .. ~0.05
;   gritty at the downward extreme). It no longer drives a separate
;   hold-length — the ZOH hold
;   length is now the literal, physical consequence of kRate itself (see THE
;   ENGINE, above), not a second parallel mechanism.
;
;   A LANDING HIT: kActiveIdx changing (knob, pedal, or GENERATIVE jump)
;   fires a short, punchy amplitude duck-and-recover (kHitEnv, ~10 ms) on the
;   wet signal, so a rung change is FELT as a hit landing.
;
;   HARD STEP (v5, 2026-07-14): user feedback after v4 — "it needs to be a
;   hard patch shifting thing not smooth clock vibe at all." The kRate PORT
;   that previously glided between rungs (120 ms in v3, 20 ms in v4) is
;   REMOVED. kRate now jumps to the new rung's ratio INSTANTLY, the same
;   k-block the rung changes. This is safe click-wise for the same reason
;   documented since v3: kRate is a RATE (a velocity the read position moves
;   at), not a position — an instant step in kRate is a kink in the read
;   position's velocity, not a discontinuity in the position (i.e. the
;   sample value) itself, so it does not reintroduce the bug-2 zipper (that
;   bug was a POSITION jump). What WAS smoothing the landing was the port,
;   not any anti-click requirement — removing it makes the transition a
;   genuine hard patch-step, exactly as requested, and the transition is now
;   sub-k-block (< ~2.7 ms at ksmps=128/48k) rather than the old ~20-120 ms
;   glide. The kHitEnv duck (above) still fires on top, so a rung change
;   reads as an immediate hit, not a slide.
;
;   None of this touches the tonal ladder (root/5th/octave rungs only), the
;   GENERATIVE duty math, the bypass/silence guarantee, or
;   the click-free LENGTH sweep.
;
; ===========================================================================
; CLOCK — quantized to a tonal ladder, root/5th/octave, ASYMMETRIC range
; (v5, 2026-07-14: range correction — see report)
;
;   User requirement (final, after iteration): CLOCK must reach AT LEAST
;   +12 semitones (1 octave) up and -24 semitones (2 octaves) down, with the
;   UP CEILING EXACTLY +12 (an earlier wider ceiling, +36, was explicitly
;   rejected by ear: "mood is still going up 2 octaves — the pitch amount
;   did not work"). Root/5th/octave stacking only, never an arbitrary ratio:
;
;     idx    0    1    2    3    4    5    6
;     st   -24  -19  -12   -7    0   +7  +12
;          8ve  5th  8ve  5th  ROOT 5th  8ve
;          down down down down      up   up
;
;   giMD2_Ladder holds these 7 semitone offsets (index 4 = home/root). The
;   mapping from gkMD2_Clock (0..1) to index is PIECEWISE LINEAR, split at
;   0.5, so 0.5 still lands exactly on home (preserving the documented
;   convention and the existing default gkMD2_Clock init 0.5) even though
;   the ladder itself is no longer symmetric:
;     kClock <= 0.5:  index = round(kClock / 0.5 * 4)     (0..4, down side)
;     kClock >  0.5:  index = round(4 + (kClock-0.5)/0.5*2) (4..6, up side)
;   This gives finer down-side resolution (5 positions across the lower half
;   of the knob/pedal travel) and coarser up-side resolution (3 positions
;   across the upper half) — deliberately, since the pedal's own character
;   (and this user's stated preference) is centered on downward
;   decimation/lo-fi, with the up side present but capped tight. Every rung
;   is still a consonant interval against the dry signal (unison, a 5th, or
;   an octave) — never an arbitrary ratio. kRate = 2^(semitone/12), so the
;   full range is 0.25x (2 octaves down) .. 2x (1 octave up).
;
; EXPRESSION PEDAL (CC29) -> CLOCK, replacing whatever it did before
;   Sticky claim gkLastToggle29 = 66, last-toggled-wins alongside ONWARD (45),
;   TRACKSYNTH (61), FEEDBACKER (63), BLOOPER (56). heel (CC=0) = bottom of
;   the ladder (2 octaves down); toe (CC=127) = top of the ladder (1 octave
;   up); gkMD2_Clock = kVal/127 directly. This is the pedal's headline move
;   (manual: "play a note and sweep the CLOCK while it decays").
;
; ===========================================================================
; GENERATIVE — a new knob, free-running and tonally quantized
;
;   gkMD2_Gen (P4), 0..1. At 0 the clock is completely static, sitting
;   wherever the knob/pedal put it ("home"). As it rises, the clock starts
;   jumping to OTHER ladder rungs and back, on its own, with increasing
;   duty; at max, it is away from home on a random (non-home) rung ~50% of
;   the time. It NEVER lands anywhere off the ladder.
;
;   Implementation is the exact renewal-process technique already proven in
;   blooper.orc's BLP_Ev (kGap = kEventLen * (1-duty)/duty * rand(0.7,1.6)):
;   an "away" excursion of random length is drawn, then the RETURN-TO-HOME
;   gap is computed from that same excursion length and the target duty
;   (0.5 * gkMD2_Gen), so the long-run fraction of time spent away converges
;   on the target duty regardless of how long any individual excursion is.
;   At Gen=0 the target duty is 0 and the process never leaves home (bench
;   requirement 6). Away-rung choice is uniform over the other 6 ladder
;   positions (never re-picks home; N=7 now, was 13 — see CLOCK).
;   kRate now steps instantly to whichever rung is active (see GRIT/HARD
;   STEP, above) — a jump between rungs (home<->away or a knob/pedal move)
;   is a hard step, by design, not a glide.
;
;   MASTER SUBDIVISION (2026-07-14): the excursion length and return-gap
;   DURATIONS (how often the clock jumps) are now quantized to the shared
;   CHASE BLISS subdivision tick (gkChaseBlissSubdivSec, csound/
;   chase_bliss_clock.orc, derived from the LOOPER's master loop length) —
;   see the inline comment at the GENERATIVE renewal-process block in instr
;   189 for the exact mechanism. The duty-cycle target math and the uniform
;   away-rung choice are unchanged.
;
; ===========================================================================
; SLIP HUMANIZATION (v5, 2026-07-14) — user feedback: "the SLIP mode grains
; are too predictable... don't up the density/rate a ton or anything"
;
;   DIAGNOSIS: the manual describes SLIP as an auto-sampler that "samples
;   your input continuously and spits it back out at a speed and direction
;   of your choosing" — it does not document any built-in randomization of
;   window position/length/retrigger timing, so the "unpredictability"
;   players describe is generally attributed to playing live over a fixed
;   mechanism, not the algorithm itself. Checked against THIS file's prior
;   implementation: it was, in fact, fully deterministic — kSlipA (the
;   window read anchor) was always set to exactly kRngWp at every wrap, and
;   kWT (the window length) was a smooth but exact function of the LENGTH
;   knob with no per-cycle variation at all. At a fixed LENGTH/CLOCK, every
;   SLIP cycle was byte-for-byte identical to the last — mechanically
;   perfect looping, which a human ear clocks quickly as "too predictable".
;
;   THE FIX (explicitly NOT touching density/rate — the retrigger cadence is
;   still governed by kWT/kRate exactly as before, on average): at each
;   window wrap, two small, independent, zero-mean random draws are taken:
;     - a position jitter on the read anchor (kSlipA), up to ~1.5% of the
;       current window length or 400 samples (~8.3 ms), whichever is
;       smaller — so consecutive cycles start reading from a very slightly
;       different point in the ring rather than the mechanically exact same
;       offset every time.
;     - a length jitter on that cycle's window (kWTCur), +/-3% around the
;       LENGTH knob's target — frozen for the whole cycle (redrawn only at
;       the next wrap) so it cannot introduce a click mid-cycle, but makes
;       consecutive cycle DURATIONS vary slightly instead of being exactly
;       identical.
;   Both are small enough that the long-run average retrigger rate and
;   window size are unchanged (zero-mean, bounded) — only the EXACT repeat
;   stops being identical cycle-to-cycle. This is baked into SLIP itself
;   (always active, independent of GENERATIVE) because the complaint was
;   about the grain mechanism, not about clock-jumping — GENERATIVE already
;   has its own, much larger-scale unpredictability (whole-rung jumps) and
;   stacking this small jitter under it is complementary, not redundant: the
;   jitter still applies during GENERATIVE's "away" excursions too, so a
;   generative jump doesn't suddenly become mechanically perfect either.
;
; ===========================================================================
; DECLICKING LENGTH (the bug-2 fix)
;
;   v2's crackle came from using SIZE directly as a delay-tap distance: any
;   change stepped the read pointer across unrelated ring content instantly
;   (the classic delay-time zipper). Here, gkMD2_Length (P3) is ported
;   (~120 ms) into the window size kWT, and the read position is computed
;   fresh every sample as "kWTCur samples behind the CURRENT write head" —
;   never from a frozen anchor that a size change could invalidate. A smooth
;   kWT change therefore produces a smooth, BOUNDED change in read position
;   (a few samples of index movement per host sample at the fastest sweep
;   the 120 ms port allows), not a jump — the same principle a
;   chorus/flanger uses for its own modulated delay time. Combined with the
;   existing edge-declick trapezoid on every window read (needed anyway for
;   the window's own seam), LENGTH can be swept continuously with nothing
;   click-class in the output — bench requirement 2 (bounded sample-to-
;   sample deltas while sweeping).
;
;   A hard safety clamp (see the engine code) additionally guarantees the
;   read can never reach back further than this instance has actually
;   written since it started — without it, a fresh engagement (or a LENGTH
;   increase happening before the ring has a full window's worth of history)
;   could read still-zero ring memory. Found by the bench during development
;   (scn4, the lowest CLOCK rung) and fixed before shipping.
;
; ===========================================================================
; CONTROLS (final, stripped-down map — self-declared below; nothing added to
; globals.orc)
;
;   toggle 66         BYPASS ($FX_BYPASS macro). OFF = instr 189 fully
;                      stopped (turnoff2) = true silence, no residual audio,
;                      dry passthrough (fixes bug 1 + bug 3).
;   gkMD2_Mix    P1   0..1  wet blend, chain-level ntrpol (mix=0 is a
;                      bit-exact dry null, same convention as before).
;   gkMD2_Clock  P2   0..1  CLOCK: tonal ladder position, 0.5 = home/root,
;                      asymmetric range (see CLOCK) -24..+12 semitones.
;   gkMD2_Length P3   0..1  LENGTH: SLIP window size, ~10 ms .. ~1.0 s.
;   gkMD2_Gen    P4   0..1  GENERATIVE: 0 static, 1 = ~50% duty away.
;   CC29 (expr)       CLOCK scrub (sticky claim 66).
;   P5/P6/P7          UNUSED / free (v2 used these for LMode/WMode/WMod,
;                      all removed). Integration can leave them unbound.
;
; OSC (change-triggered, local triple-send):
;   /md2_state (i) bypass, 1=off/0=on — rig-wide convention (blooper/habit)
;   /md2_mix (f) /md2_clock (f) /md2_length (f) /md2_gen (f)
;
; ===========================================================================
; INTEGRATION NOTES (all shared-file edits already applied in this branch —
; this section documents what changed and why, for anyone auditing the diff)
;
;   globals.orc      : UNCHANGED. All MOOD II globals remain self-declared in
;                      this file (include order: globals -> effects -> chain).
;
;   neptrPhase4.csd  : UNCHANGED (#include "effects/mood2.orc" already there).
;
;   signal_chain.orc : stage 18.5 changed from an always-on `schedule 189, 0,
;                      -1` (see BUG 1/3 above) to the standard
;                      `$FX_BYPASS(gkMD2_bypass'189'gaMD2_sendL'...'md2)`
;                      macro, same pattern as DIFFUSION (187) right after it.
;                      The mix ntrpol line is unchanged (still chain-level,
;                      still a bit-exact null at Mix=0).
;
;   handler_menus.orc: slot button (CHASE BLISS s3) changed from the raw
;                      press/release gesture-grammar dispatch (gkMD2_Btn +
;                      a CC71 release handler) to a plain
;                      `$FX_TOGGLE(gkMD2_bypass'66)`, matching every other
;                      simple toggle effect. gkMD2_Btn and the CC71 release
;                      branch are DELETED (the removed gesture no longer
;                      exists). LMode/WMode/LMod/WMod/Route encoder bindings
;                      (P3/P5/P6/P7 fills) are DELETED. NEW bindings:
;                        P3 (CC23): $ENC_ROT(gkMD2_Length'0.02'0'1)
;                        P4 (CC25): $ENC_ROT(gkMD2_Gen'0.02'0'1)
;                      P1 (CC24, Mix) and P2 (CC20, Clock) and the CC29 scrub
;                      are unchanged in wiring (still gkMD2_Mix/gkMD2_Clock);
;                      no re-wiring needed for the v5 range change either —
;                      it is purely internal quantization inside this file,
;                      still just a 0..1 float in/out.
;
;   neptrPhase4_UI.py: EFFECT_PARAMS["md2"] reduced to
;                        [("mix","Mix",0,1), ("clock","Clock",0,1),
;                         ("length","Length",0,1), ("gen","Generative",0,1)]
;                      MD2_LMODES/MD2_WMODES/MD2_STATES stepped-label tables
;                      are DELETED. /md2_state now means the SAME thing it
;                      means everywhere else (1=bypassed/0=on), so the old
;                      MOOD-II-specific /md2_state branch and _slot_badge
;                      case are DELETED too — it falls through to the
;                      generic bypass handler like every other simple toggle.
;
;   docs             : registry row unchanged (66 MOOD II instr 189, CHASE
;                      BLISS s3); note added that the effect was reworked
;                      2026-07-13/14 (SLIP-only, no micro-looper, v5 unified
;                      ZOH decimation + asymmetric range + hard-step clock).
; ---------------------------------------------------------------------------

; ---- self-declared globals ----
gkMD2_bypass init 1
gkMD2_Mix    init 0.5      ; 2026-09-25: 50% by default (was 100%)
gkMD2_Clock  init 0.5      ; 0.5 = home/root (unison) under the piecewise map
gkMD2_Length init 0.35
gkMD2_Gen    init 0
gaMD2_sendL  init 0
gaMD2_sendR  init 0
gaMD2_outL   init 0
gaMD2_outR   init 0
; bench/debug telemetry — not sent over OSC, read directly by scripts/bench
gkMD2_ClockIdx init 4    ; current quantized ladder index actually driving kRate (0..6, home=4)
gkMD2_Away     init 0    ; 1 while the GENERATIVE process is away from home
gkMD2_DbgWT       init 17424  ; current cycle's (jittered) window length in samples — bench telemetry only
gkMD2_DbgAnchorJit init 0     ; last window's anchor position jitter in samples — bench telemetry only
gkMD2_DbgGenLen   init 0      ; last subdivision-quantized GENERATIVE excursion length, seconds — bench telemetry only
gkMD2_DbgGenGap   init 0      ; last subdivision-quantized GENERATIVE return gap, seconds — bench telemetry only

; tonal ladder (v5): root/5th/octave stacking, ASYMMETRIC — floor -24 semi
; (2 octaves down), ceiling +12 semi (1 octave up), index 4 = home (see CLOCK
; header for the exact user-requirement history and the piecewise 0..1 map).
; v8 (2026-07-14): extended UP to +2 octaves at the user's request, in the same
; root/5th/octave style as the rest of the ladder. 9 rungs, home = index 4,
; now symmetric: -24 -19 -12 -7 | 0 | +7 +12 +19 +24. The up side stays CLEAN
; (kGrit = 0 for kSemi >= 0) — extending the range does not extend the crush.
; SIZE MUST BE A POWER OF 2. Csound's GEN02 silently ROUNDS DOWN a non-power-of-2
; size: `ftgen 0,0,9,-2,...` allocates ftlen=8, so index 8 (+24) never existed and
; clamped back to index 7. Size 16 with the 9 rungs followed by pad copies of the
; top rung means an over-index can only ever land on +24, never on a wrong pitch.
giMD2_GenSlow init 2.5   ; generative excursion/gap multiplier (see scheduler)
giMD2_Ladder ftgen 0, 0, 16, -2, -24, -19, -12, -7, 0, 7, 12, 19, 24, 24, 24, 24, 24, 24, 24, 24

; ---- v7 GRIT calibration constants (see "THE BOTTOM-RUNG COLLAPSE BUG") ----
; giMD2_QRef: the reference peak the v6 absolute quantizer steps were tuned
;   and benched at (the bench's own hot 0.22 tone). Dividing the v6 absolute
;   step curve by this turns it into a fraction-of-peak, so the character at
;   a hot signal is IDENTICAL to v6 (which the user liked) while every quieter
;   signal now gets the same number of BITS rather than the same absolute
;   dead zone. Changing this number rescales "how gritty is gritty" globally;
;   it does NOT change which rungs are grittier than which.
; giMD2_QFloor: the lowest peak the step is allowed to be derived from. This
;   is the denormal / divide-by-zero guard (kQStep >= giMD2_QFloor * kQFrac,
;   always strictly positive) AND the point below which the crusher stops
;   following the signal down — a true digital-silence input still quantizes
;   to exact zero (the no-stale-audio guarantee), it just can't drive the
;   step to zero on the way there.
giMD2_QRef   = 0.22
giMD2_QFloor = 0.005

; ring buffer: ~2.05 s at 48k, comfortably bigger than the widest LENGTH
; window (~1.0 s) plus the anchor lookback margin.
giMD2_RingN = 98304
giMD2_RngL  ftgen 0, 0, -giMD2_RingN, -2, 0
giMD2_RngR  ftgen 0, 0, -giMD2_RingN, -2, 0

opcode MD2_Osc3f, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
endop

opcode MD2_Osc3i, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
endop

instr 189
  ; ---- persistent engine state ----
  kRngWp  init 0        ; ring write head
  kSlipPh init 0        ; read phase within the window, 0..kWTCur
  kSlipA  init 0        ; window anchor: ring index "now" was at the last
                         ; wrap (plus a small jitter — see SLIP HUMANIZATION).
                         ; FROZEN between wraps — this is what decouples the
                         ; read RATE from the write head's own constant
                         ; 1/sample advance.
  ; This cycle's (possibly jittered) window length, frozen between wraps — see
  ; SLIP HUMANIZATION. v7: seeded from the ACTUAL current LENGTH knob at
  ; instrument start, not from a hardcoded constant. It used to init to a flat
  ; 17424 samples (~0.36 s, i.e. whatever LENGTH happened to be when that
  ; number was written down), and because kWTCur is only ever recomputed AT A
  ; WRAP, the very first SLIP cycle after engaging the effect ran at that
  ; wrong length no matter where LENGTH actually was. At slow CLOCK rungs that
  ; first cycle lasts a long time — at the -24 rung (kRate 0.25) a 17424-sample
  ; window takes 17424/0.25 = 69696 samples = 1.45 SECONDS to traverse — so
  ; engaging MOOD at a short LENGTH played a wrong-length, mostly-unsettled
  ; first window for well over a second. (This is the same cold-start family as
  ; the kWritten safety clamp below; it was previously masked at the down rungs
  ; because v6's bit-crush dead zone gated the resulting low-level junk to
  ; silence — remove the dead zone, as v7 does, and it becomes plainly visible
  ; as low-frequency mud in bench scn4's slowest rung.) Seeding from the knob
  ; makes the first cycle the right length immediately.
  kWTCur     init 480 + i(gkMD2_Length) * 47040
  kGenState  init 0     ; 0 = home, 1 = away
  kGenDead   init 0      ; perf-time of next generative decision
  kGenEvLen  init 0      ; length (s) of the current away excursion
  kAwayIdx   init 4
  kHitEnv    init 1      ; landing-hit amplitude envelope
  kPrevIdx   init 4      ; previous kActiveIdx, for the landing-hit trigger
  kLev       init 0.02   ; v7: running peak of the wet signal, drives kQStep

  ; v7 LEVEL-FOLLOWER TIME CONSTANTS — deliberately SLOW in both directions,
  ; and updated once per k-block (not per sample). This is load-bearing, not
  ; an optimization:
  ;   The quantizer lattice must stay STILL for a steady signal. If the
  ;   follower is fast enough to ripple with the waveform itself (a first cut
  ;   used a 5 ms attack — but the -24 rung's read output is ~55 Hz, an 18 ms
  ;   period, so a 5 ms attack tracks the individual half-cycles), then kQStep
  ;   wobbles at audio rate, the lattice the signal is being quantized onto
  ;   slides around underneath it, and the hold/stair character that IS the
  ;   grit dissolves into a smeared cloud of levels — measurably: hold_frac at
  ;   the down rungs fell from ~0.8 to ~0.3 and the level count exploded.
  ;   At 60 ms attack / 600 ms release the follower is far slower than the
  ;   lowest note the ladder can produce, so for a sustained tone kLev is
  ;   effectively a constant and the lattice is fixed — a real bit-crusher —
  ;   while a decaying note or a dynamic passage is still tracked over its own
  ;   (much slower) envelope, which is the whole point.
  ; kr = sr/ksmps, so the per-k-block one-pole coefficient is exp(-ksmps/(t*sr)).
  iLevAtk = exp(-(ksmps / sr) / 0.06)
  iLevRel = exp(-(ksmps / sr) / 0.60)

  ; ---- controls ----
  kClock  = limit:k(gkMD2_Clock, 0, 1)
  kLength port gkMD2_Length, 0.12, i(gkMD2_Length)
  kGen    = limit:k(gkMD2_Gen, 0, 1)

  ; (v9: the LOOPER READ block that used to sit here — 5 MD2_LpRead calls
  ; summed into kLoopSum — is deleted; the engine takes live input only.)

  ; ---- CLOCK: knob/pedal "home" index — piecewise 0..1 -> 0..6 map, split
  ; at 0.5 so home (index 4) stays under the knob's center (see CLOCK
  ; header for the exact rationale). ----
  ; v8: both halves now span 4 rungs (9-rung ladder), so the map is symmetric
  ; about the knob's center — 0.5 is still exactly home (index 4).
  if (kClock <= 0.5) then
    kBaseIdxF = kClock / 0.5 * 4
  else
    kBaseIdxF = 4 + (kClock - 0.5) / 0.5 * 4
  endif
  kBaseIdx = int(round(kBaseIdxF))
  kBaseIdx limit kBaseIdx, 0, 8

  ; ---- GENERATIVE: renewal-process excursion scheduler (see header). N=7
  ; ladder positions now (was 13) — raw draw corrected to draw from N-1
  ; slots (0..5) then skip-adjust around kBaseIdx, landing uniformly on the
  ; other 6 positions and never re-picking home. ----
  ; MASTER SUBDIVISION QUANTIZE (2026-07-14): the GENERATIVE clock-jump
  ; timing (excursion length + return gap, i.e. "how often the clock jumps")
  ; now snaps to the shared CHASE BLISS subdivision tick
  ; (gkChaseBlissSubdivSec, csound/chase_bliss_clock.orc — derived from the
  ; LOOPER's master loop length) instead of a free-running continuous draw.
  ; The renewal-process duty math (target duty = 0.5*kGen, the random
  ; length/gap draws, the uniform away-rung choice) is completely unchanged —
  ; only the two DURATIONS it produces (kGenEvLen, kGap) are rounded to the
  ; nearest whole tick (minimum one tick) before being used as deadlines.
  kSubdivSec = gkChaseBlissSubdivSec
  kNow times
  if (kNow >= kGenDead) then
    if (kGenState == 0) then
      kDuty = 0.5 * kGen
      if (kDuty > 0.0005) then
        kU  random 0, 1
        ; 2026-09-25: GENERATIVE PACE 2.5x SLOWER. This is the "speed" of
        ; MOOD's motion — how often the clock jumps away from home and how
        ; long it stays — NOT the CLOCK ladder (pitch) and NOT the master
        ; tempo. Excursion draw was 0.35..2.2 s; now 0.875..5.5 s. The return
        ; gap is derived from the excursion length below, so it scales with
        ; it and the duty (fraction of time away) is unchanged. Both still
        ; quantize to the CHASE BLISS subdivision tick.
        kGenEvLenRaw = giMD2_GenSlow * (0.35 + 1.85 * kU)
        kGenEvLen = round(kGenEvLenRaw / kSubdivSec) * kSubdivSec
        if (kGenEvLen < kSubdivSec) then
          kGenEvLen = kSubdivSec
        endif
        kU2 random 0, 1
        kIdxR = int(kU2 * 7.999)    ; v8: 9 rungs -> draw from the other 8
        if (kIdxR >= kBaseIdx) then
          kIdxR = kIdxR + 1        ; skip home, uniform over the other 8
        endif
        kAwayIdx  = kIdxR
        kGenState = 1
        kGenDead  = kNow + kGenEvLen
        gkMD2_DbgGenLen = kGenEvLen
      else
        kGenDead = kNow + 0.25     ; Gen=0: keep checking, never actually leaves
      endif
    else
      kGenState = 0
      kDuty = 0.5 * kGen
      kR random 0.7, 1.6
      if (kDuty > 0.0005) then
        kGapRaw = kGenEvLen * (1 - kDuty) / kDuty * kR
        kGap = round(kGapRaw / kSubdivSec) * kSubdivSec
        if (kGap < kSubdivSec) then
          kGap = kSubdivSec
        endif
      else
        kGap = 999999
      endif
      gkMD2_DbgGenGap = kGap
      kGenDead = kNow + kGap
    endif
  endif
  kActiveIdx = (kGenState == 1 ? kAwayIdx : kBaseIdx)
  gkMD2_ClockIdx = kActiveIdx
  gkMD2_Away     = kGenState

  ; ---- landing hit: a rung change (knob, pedal, or generative jump) fires
  ; a short punchy duck-and-recover on the wet signal, so the shift is FELT
  ; landing (see GRIT/HARD STEP header, above). ----
  if (kActiveIdx != kPrevIdx) then
    kHitEnv = 0.2
  endif
  kPrevIdx = kActiveIdx
  kHitEnv  = kHitEnv + (1 - kHitEnv) * 0.55    ; tight recovery, ~8-10 ms — a hit, not a glide

  ; ---- CLOCK: quantized rate, HARD STEP (v5) — kRate jumps to the new
  ; rung's ratio the same k-block the rung changes, no port at all. Safe
  ; click-wise because kRate is a rate/velocity, not a position (see HARD
  ; STEP header). ----
  kSemi  table kActiveIdx, giMD2_Ladder
  kRate  = pow(2, kSemi / 12)
  ; v8: ceiling raised 2.0 -> 4.0 for the new +19/+24 rungs (2 octaves up).
  ; This clamp is a safety rail, NOT a musical limit — leaving it at 2.0 while
  ; the ladder went to +24 silently crushed both new top rungs back down to
  ; +12 (they measured 440 Hz instead of 659/880). Floor stays 0.25 = -24.
  kRate  limit kRate, 0.25, 4.0     ; matches the -24..+24 semitone range

  ; ---- GRIT amount (v6, 2026-07-14): DOWNWARD ONLY. User, having played it
  ; on the rig: "the mood should only start bitcrushing when it goes below
  ; the pitch, like lower." The bit-depth-quantization crush layer is now
  ; exclusively a function of how far CLOCK sits BELOW home (floor -24 semi);
  ; the upward rungs (+7/+12) get kGrit = 0 unconditionally, so kQStep
  ; collapses to the same near-transparent floor used at home (0.0025)
  ; regardless of how far up CLOCK goes. This leans into a physical truth
  ; about real sample-rate reduction rather than fighting it: downsampling
  ; aliasing (holding/repeating samples) is far more audible/harsh than the
  ; upsampling case (reading faster just point-samples ahead, which is a much
  ; milder artifact) — so a symmetric crush curve was fighting the actual
  ; acoustics, not honouring them. See the READ block below (THE ENGINE, v6)
  ; for the matching read-side asymmetry (down = literal ZOH hold, up =
  ; interpolated, no point-sampling grain either).
  kAbsSemi = abs(kSemi)
  kIsDown  = (kSemi < -0.01 ? 1 : 0)
  kGrit    = (kIsDown == 1 ? 0.18 + 0.82 * pow(limit:k(kAbsSemi / 24, 0, 1), 0.55) : 0)

  ; ---- v7 (2026-07-14): the quantizer step is now RELATIVE TO THE SIGNAL'S
  ; OWN RUNNING PEAK, not a fixed absolute amplitude. See "THE BOTTOM-RUNG
  ; COLLAPSE BUG" in the header for the full root cause. In one line: v6's
  ; kQStep was an ABSOLUTE step (up to 0.0525 at the -24 rung), which is a
  ; dead zone of +-0.026 — i.e. -31.6 dBFS. Any signal quieter than that (a
  ; decaying note's tail, a quiet passage, anything but a hot transient)
  ; rounded to EXACTLY ZERO across the board, while what did survive got
  ; magnified up to a full +-0.0525 step, so the output degenerated into a
  ; 3-level (-Q/0/+Q) pulse train: hard square-wave switching against total
  ; silence. That is precisely the reported "noise ... goes totally crackly
  ; silent vibe at the bottom".
  ;
  ; kQFrac is that same v6 curve re-expressed as a FRACTION of the current
  ; peak, referenced to giMD2_QRef — the hot signal level (0.22) the v6 curve
  ; was actually tuned and benched at. So at that level the character is
  ; UNCHANGED (byte-for-byte the same step sizes the user just told us sounded
  ; "much better"), and at every OTHER level the crush now scales with the
  ; signal instead of eating it. This is what a bit-depth reducer is supposed
  ; to be: a constant number of BITS, i.e. a constant signal-to-quantization-
  ; noise ratio — not a constant absolute floor that the music falls through.
  ; The grit curve itself (0.18 + 0.82*(|semi|/24)^0.55, down-only) is
  ; untouched: monotonic, bounded on [0.18, 1.0] over the down rungs, no
  ; discontinuity or singularity anywhere on the ladder.
  kQFrac   = (0.0025 + kGrit * 0.05) / giMD2_QRef

  ; ---- LENGTH: window size, smoothly ported (~120 ms) so a LENGTH sweep
  ; moves kRd by only a few samples per host sample (bounded, no zipper) —
  ; the bug-2 fix. kWT (the knob target) can drift continuously; kWTCur (the
  ; actual per-cycle window, jittered — see SLIP HUMANIZATION) is what's
  ; used inside the loop, redrawn only at each wrap. ----
  kWT = 480 + kLength * 47040     ; ~10 ms .. ~990 ms
  kWT limit kWT, 480, 47520

  aInL = gaMD2_sendL
  aInR = gaMD2_sendR
  iDip = 96                              ; ~2 ms edge de-click, host samples

  aOutL init 0
  aOutR init 0
  kWritten init 0    ; how many ring samples THIS instance has actually
                      ; written so far — a hard safety clamp so a fresh
                      ; engagement can never read ring content that hasn't
                      ; been written yet (cold-start silence, found by bench
                      ; scn4/idx0 during development)

  ; ---- v7: kQStep is computed ONCE per k-block, from the level follower's
  ; state as of the END of the previous block, and held constant for every
  ; sample in this block. A lattice that is fixed within a block (and drifting
  ; only on the follower's slow 60/600 ms envelope across blocks) is what
  ; makes this read as a bit-crusher rather than as noise modulation.
  ;
  ; The FLOOR (giMD2_QFloor) is the denormal / divide-by-near-zero guard:
  ; kQStep >= giMD2_QFloor * kQFrac, which is strictly positive for every rung
  ; on the ladder, so `kWetL0 / kQStep` can never blow up, go denormal, or
  ; produce an inf/NaN no matter how silent the input goes. The CEILING stops
  ; a single over-unity transient from widening the step beyond full scale.
  ; DE-RIPPLE (v7): the raw peak follower still ripples, because kBlkMax
  ; samples the wet waveform's own peak once per k-block and the read
  ; frequency beats against the k-block rate (at the -12 rung the wet is
  ; 110 Hz = 3.41 blocks/cycle, at -24 it is 55 Hz = 6.82 — neither divides
  ; the block rate evenly, so the sampled peak wanders on a slow beat). Left
  ; alone that beat drifts the quantizer lattice by several percent over a
  ; second, which is audible as a slow wobble in the grit and measurable as
  ; a locally-clean-but-globally-smeared level count (13 levels in any 50 ms
  ; window, but 220 across 0.8 s). A second, slower smoothing stage removes
  ; the beat while leaving the follower's actual envelope tracking (a note
  ; decaying over seconds) completely intact.
  kLevSm  portk kLev, 0.25
  kLevQ0  = limit:k(kLevSm, giMD2_QFloor, 1.0)

  ; ---- v8 (2026-07-14): STEPPED LATTICE — the "pure bitcrush" fix. ----
  ; v7 derived kQStep from a CONTINUOUSLY-SLIDING level follower. That fixed
  ; the dead-zone/silence bug, but it introduced a subtler one: a lattice that
  ; is always creeping means the same input sample lands on a slightly
  ; different rung every cycle, so the quantization error DECORRELATES from
  ; the waveform and turns into broadband hiss. That is the sound of "bit
  ; REDUCTION" — a noise floor sitting under the signal.
  ;
  ; A real bit CRUSHER has an absolutely FIXED lattice. The error is then a
  ; deterministic function of the waveform, so it folds back as HARMONIC,
  ; buzzy, aliased distortion locked to the note — the unmistakably digital
  ; crunch, not hiss.
  ;
  ; So: snap the follower to hysteretic 6 dB (factor-of-2) buckets. Inside a
  ; bucket the lattice does not move AT ALL — true fixed-grid crush, correlated
  ; error, classic character. The bucket only re-scales when the signal moves a
  ; full 6 dB, which is what keeps a decaying note from ever falling through
  ; the grid into silence (the v7 bug we are NOT reintroducing). The 2x/0.5x
  ; hysteresis means it cannot chatter at a boundary, and the 30 ms portk only
  ; glides during an actual bucket change, so the lattice is still the rest of
  ; the time.
  kBucket init 1.0
  if (kLevQ0 > kBucket * 2.0) then
    kBucket = kBucket * 2.0
  elseif (kLevQ0 < kBucket * 0.5) then
    kBucket = kBucket * 0.5
  endif
  kBucket   limit kBucket, giMD2_QFloor, 1.0
  kBucketSm portk kBucket, 0.03
  kQStep    = kQFrac * kBucketSm
  kBlkMax init 0
  kBlkMax = 0

  kS = 0
  while kS < ksmps do
    kXL = aInL[kS]
    kXR = aInR[kS]

    ; ---- write side: always clean, full host-sample rate (v5 — the write
    ; side no longer decimates; decimation now happens entirely on read, see
    ; THE ENGINE header). ----
    tabw(kXL, kRngWp, giMD2_RngL)
    tabw(kXR, kRngWp, giMD2_RngR)
    kRngWp = kRngWp + 1
    if (kRngWp >= giMD2_RingN) then
      kRngWp = 0
    endif
    if (kWritten < giMD2_RingN) then
      kWritten = kWritten + 1
    endif

    ; ---- SLIP window: read phase cycles 0..kWTCur at the full (up-or-down)
    ; CLOCK ratio; the read position is kSlipA (frozen at the last wrap,
    ; jittered — see SLIP HUMANIZATION) plus that phase, offset back by
    ; kWTCur. Between wraps, kSlipA and kWTCur are the only things kRd
    ; depends on besides kSlipPh, so kRd's rate of motion is exactly kRate.
    kSlipPh = kSlipPh + kRate
    if (kSlipPh >= kWTCur) then
      kSlipPh = kSlipPh - kWTCur
      kJit     random -1, 1
      kJitAmt  = (0.015 * kWTCur < 400 ? 0.015 * kWTCur : 400)
      kSlipA   = kRngWp + int(kJit * kJitAmt)
      kLenJit  random -1, 1
      kWTCur   = kWT + kLenJit * 0.03 * kWT
      kWTCur   limit kWTCur, 480, 47520
      gkMD2_DbgWT       = kWTCur
      gkMD2_DbgAnchorJit = int(kJit * kJitAmt)
    endif

    kDW  = min(kSlipPh, kWTCur - kSlipPh)
    kWEnv = (kDW < iDip ? kDW / iDip : 1)

    kRd = kSlipA - kWTCur + kSlipPh
    kRd = kRd - giMD2_RingN * int(kRd / giMD2_RingN)
    if (kRd < 0) then
      kRd = kRd + giMD2_RingN
    endif

    ; SAFETY CLAMP (the cold-start fix): kRd must never be more than
    ; kWritten samples "behind" the current write head — otherwise, in the
    ; first fraction of a second after a fresh engagement (or right after a
    ; LENGTH increase), "kWTCur samples behind now" can reach back past this
    ; instance's own start into ring memory that was never written this
    ; session (still zero from the table's init).
    kBehind = kRngWp - kRd
    if (kBehind < 0) then
      kBehind = kBehind + giMD2_RingN
    endif
    if (kBehind > kWritten) then
      kRd = kRngWp - kWritten
      if (kRd < 0) then
        kRd = kRd + giMD2_RingN
      endif
    endif

    ; ---- READ (v6, 2026-07-14): kRd's POSITION is driven by kRate — the
    ; exact same rate/velocity variable that also set the hold-length via
    ; kSlipPh's advance, above — for BOTH directions. There is exactly ONE
    ; rate mechanism here; what differs by direction below is only which
    ; SAMPLING KERNEL reads that single position, per the user's explicit
    ; request ("only start bitcrushing when it goes below the pitch"):
    ;   - DOWNWARD (kSemi < 0): literal zero-order-hold (nearest-neighbor,
    ;     int(kRd)). This IS the decimation/hold/grit mechanism itself (see
    ;     THE ENGINE header) — repeated int(kRd) values across output samples
    ;     are the audible "held" grit. Unchanged from v5.
    ;   - UPWARD (kSemi >= 0, includes home): linearly interpolated between
    ;     int(kRd) and int(kRd)+1. The pitch shift is still 100% a
    ;     consequence of kRd advancing faster than 1 host-sample per output
    ;     sample (real sample-rate-derived transposition, completely
    ;     unchanged) — only the point-sampling GRAIN the ZOH kernel would add
    ;     on top is removed, because real-world upsampling aliasing is a much
    ;     milder artifact than the downsampling case (see GRIT header). This
    ;     is a reconstruction-filter choice on the READ of an already-shifted
    ;     position, not a second/parallel pitch algorithm — kRd itself is
    ;     identical either way.
    ; v7: the window de-click envelope (kWEnv) is NO LONGER applied here.
    ; It used to multiply the ring sample BEFORE the quantizer, which meant
    ; the quantizer ate the de-click ramp itself: a ~2 ms fade rendered in
    ; steps of 0.0525 is not a fade, it is a staircase of hard 0.05 jumps —
    ; so at exactly the rungs where the step was biggest, the anti-click
    ; trapezoid was DESTROYED and every single window seam became a click
    ; source. That is the second half of the reported crackle, and it is why
    ; it got worse the further down the ladder you went. kWEnv (and kHitEnv)
    ; are now applied AFTER quantization, where they behave as the smooth
    ; gain ramps they are meant to be. See the header.
    kI0 = int(kRd)
    if (kSemi < -0.01) then
      kWetL0 = tab:k(kI0, giMD2_RngL)
      kWetR0 = tab:k(kI0, giMD2_RngR)
    else
      kI1 = kI0 + 1
      if (kI1 >= giMD2_RingN) then
        kI1 = 0
      endif
      kFr  = kRd - kI0
      kA0L = tab:k(kI0, giMD2_RngL)
      kA1L = tab:k(kI1, giMD2_RngL)
      kA0R = tab:k(kI0, giMD2_RngR)
      kA1R = tab:k(kI1, giMD2_RngR)
      kWetL0 = kA0L + (kA1L - kA0L) * kFr
      kWetR0 = kA0R + (kA1R - kA0R) * kFr
    endif

    ; v7 level follower, part 1 of 2: accumulate this block's peak over
    ; max(|L|,|R|) — ONE shared follower for both channels, so the stereo
    ; image can never pump or wander apart. The follower itself is updated
    ; once, after the loop (part 2), and kQStep is FROZEN for the whole block
    ; (computed above, from the previous block's kLev) — see the time-constant
    ; comment at iLevAtk/iLevRel for why a still lattice is mandatory.
    kMagL = abs(kWetL0)
    kMagR = abs(kWetR0)
    kMag  = (kMagL > kMagR ? kMagL : kMagR)
    if (kMag > kBlkMax) then
      kBlkMax = kMag
    endif

    ; ---- bit-crush quantizer on the SAME sample the ZOH just produced (the
    ; "quality" half of CLOCK, see GRIT header). Round-to-nearest, symmetric
    ; about zero (no DC offset introduced: the +-0.5 sign term makes int()'s
    ; truncate-toward-zero behave as a symmetric round). kQFrac collapses to
    ; its near-transparent floor for kSemi >= 0 (upward + home), so this is
    ; effectively a no-op crush at/above home — "no bitcrushing going up",
    ; per the user. ----
    kSgnL = (kWetL0 >= 0 ? 0.5 : -0.5)
    kSgnR = (kWetR0 >= 0 ? 0.5 : -0.5)
    kGritL = kQStep * int(kWetL0 / kQStep + kSgnL)
    kGritR = kQStep * int(kWetR0 / kQStep + kSgnR)

    ; de-click trapezoid + landing hit applied AFTER the quantizer (see the
    ; v7 note at the top of this block) — smooth gain ramps on a quantized
    ; signal, not a quantized ramp.
    aOutL[kS] = kGritL * kWEnv * kHitEnv
    aOutR[kS] = kGritR * kWEnv * kHitEnv
    kS = kS + 1
  od

  ; ---- v7 level follower, part 2 of 2: one update per k-block, from this
  ; block's observed peak. Peak-up is fast-ish (60 ms), peak-down is slow
  ; (600 ms) so a decaying note is tracked down by its own envelope rather
  ; than by its waveform. Feeds the NEXT block's kQStep.
  if (kBlkMax > kLev) then
    kLev = kBlkMax + (kLev - kBlkMax) * iLevAtk
  else
    kLev = kBlkMax + (kLev - kBlkMax) * iLevRel
  endif

  ; soft ceiling — the live input can run hot (v9: the 5 summed loop
  ; channels that used to justify this are gone, but the safety rail stays);
  ; a hard limit() would itself introduce the exact discontinuities bug 2
  ; was about, so this is a soft knee, linear (untouched) below 0.7.
  aOutL clip aOutL, 0, 0.99, 0.7
  aOutR clip aOutR, 0, 0.99, 0.7
  gaMD2_outL = aOutL
  gaMD2_outR = aOutR

  ; ---- OSC (change-triggered) ----
  ; /md2_state follows the rig-wide convention (1 = bypassed, 0 = on) so the
  ; UI's generic bypass handler (param == "state") picks it up with no
  ; MOOD-II-specific UI code, unlike the old transport readout.
  kc1 changed gkMD2_bypass
  if (kc1 == 1) then
    MD2_Osc3i "/md2_state", gkMD2_bypass
  endif
  kc2 changed gkMD2_Mix
  if (kc2 == 1) then
    MD2_Osc3f "/md2_mix", gkMD2_Mix
  endif
  kc3 changed gkMD2_Clock
  if (kc3 == 1) then
    MD2_Osc3f "/md2_clock", gkMD2_Clock
  endif
  kc4 changed gkMD2_Length
  if (kc4 == 1) then
    MD2_Osc3f "/md2_length", gkMD2_Length
  endif
  kc5 changed gkMD2_Gen
  if (kc5 == 1) then
    MD2_Osc3f "/md2_gen", gkMD2_Gen
  endif
endin

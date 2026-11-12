# Engine rewrite 2026-10-01: resampler pitch, LPC formant, velocity synth

Deployed to the Bela 2026-10-01 (binary built on-device from this tree; previous
binary kept at `/root/hyperGuitar6chPan.bak-2026-10-01`). Lock-in of the state
before this change: git `e4b7844`.

Owner's stated goals (memory: `shifter-sound-goals`): pitch UP only has to be
SOFT; pitch DOWN and FORMANT have to be CLEAR (no grain texture, no transient
morphing). The synth needs real dynamics, held sustain, fast response.

## Pitch stage (`shifter.cpp`)
- ONE drifting-delay resampler for both directions (`resRun`). Down: delay
  grows, jumps back one period at the ceiling (unchanged SOLAD). Up: delay
  falls at (rho-1)/sample, jumps back N whole periods at the floor,
  N = clamp(round(350/T),1,4) (latency budget ~8 ms), splice offset refined
  +-4 by normalised correlation (`corrRefine`), equal-power smoothstep
  crossfade one output period long (`SUP_XF`, cap `SUP_XF_MAX`).
- Why: the old PSOLA up leg overlap-added grains half a period apart; on
  inharmonic plain strings that is slow partial-level AM ("beading", 25-30%
  on strings 4-5 at +12 with an oracle period). A resampler is a true
  frequency scaler between splices. Measured on pitchbench (oracle, 25c
  flat), +12 beading s0..s5: 8.5 9.0 16.1 17.2 30.3 24.7 -> 1.8 2.5 1.3 2.9
  3.5 7.1 (dry reference 2.3 3.0 1.5 2.9 3.1 5.2).
- Up reads the anti-aliased ring `abuf` (one-pole at 0.45*sr/max(rho,phi)).
  A splice never jumps back past the onset stamp.
- Down attacks: `shifterOnset` re-anchors the read to delay 4 through a
  smoothstep overlay (0.2T, 32..80 samples) as a THIRD leg over any splice
  in flight; the chain stays live, so the pick lands already shifted. The
  first splice is ~2T away at an octave; `resRun` waits up to 1.5T past the
  ceiling for the tracker lock, then splices by correlation if still unlocked.
- Modes: RUN and TRANS only. TRANS (dry tap, fixed 4-sample read) is entered
  at an onset only for pitch UP or a live GRAIN formant leg. The
  GRAINS/DRIFT split, noLockHops leash and dwell floors are gone: the
  resampler IS the splicer. The formant gate (GRAIN mode only) survives.
- `mkIn` picker and the up `GrainLeg` removed. Tap/chain blend is smoothstep
  equal-power. Bit-exactness guard in the splice search removed (it took the
  2700-read slow path on every splice for a near-constant base).
- Clicks (disc.py d2 hits, before -> after): up +7 53->17, up +12 52->19,
  up12+formant 52->19, down -12 193->125, down -5 61->48, formant 45->13.
  At device level (0.1x, 3e-4 noise) every scene reads 0.
- CPU: up leg ~2.4 bufAtM per string per sample at +12 vs 6 grains; worst
  slot 924 vs 2725.
- Known: low-string up latency ~13 ms mean at +12 (N=1 saw over a whole
  period, by construction); down + GRAIN formant still passes the pick dry.

## Formant stage (`lpc.cpp`), LPC now the DEFAULT mode (`gFormantMode = 1`,
saved state overrides; GRAIN stays on the toggle)
- Bug fixed: analysis was on the input, inverse applied to the pitch-SHIFTED
  output, so with pitch on the whitening never matched (measured: no better
  than bypass). Now: envelope from the input, filter = E_in(w/phi) /
  E_in(w/rho) fitted as FIR+IIR lattices; formant 1.0 with pitch on = the
  original formants restored (formant-corrected pitch shift); phi == rho is
  a bit-exact bypass.
- Decimate-by-2 analysis (15-tap half-band), 1024 window, order 28 (s0-1) /
  24 (s2-5), analysis-only pre-emphasis 0.94, lag window max(60 Hz,
  0.25 f0) capped 250 Hz, -45 dB noise loading, trig tables everywhere
  (561 cosf/hop/string gone), onset snap: dry until 6/8 window of post-pick
  data (17 ms), then snapped lattices primed from history; slow in/out
  power trim. Envelope error 4.4 -> 2.6 dB RMS; steady gain error 0.00 dB.
- Cost: ~45-51k MAC per string per hop (hops are staggered across strings
  in render.cpp), lattice 116-135 MAC/sample/string. `-DLPC_NG=65` cuts
  hop cost 35%; `-DLPC_ORD_LO=24 -DLPC_ORD_HI=20` cheaper still.
- Harness: `hosttest/lpcbench/` (+ `LPC-FINDINGS.md`).
- `dsp_core.cpp`: auto-EQ tilt now runs AFTER lpcProcess.

## Synth (`synth.cpp`), poly and mono share one engine
- Note-on on tracker acceptance (attackBlank 0, conf >= 0.90, gate not 1/2,
  2 good hops): 5 ms after acceptance, was 40 ms (14 tonalHops).
- Velocity = peak env over 15 ms after the pick / slow per-string reference
  (60 s decay, floor 0.008), ^0.7. Amplitude 0.6 * velocity, HELD.
- Note-off: env under max(floor, -40 dB of the note's own peak) for 4 hops,
  or a damp/release event 2 hops, or gate 2 for 6 hops; then 90 ms release.
  Floor = clamp(gSynthGateThr*0.15, 0.0008, 0.006).
- Legato: >= 0.7 st with 3 agreeing hops retriggers pitch, same velocity, no
  attack; pitch frozen during releaseBlank / gate 1 / closed gate.
- Timbres static (no attack-then-quiet); velocity -> brightness 0.3 on the
  filter modes. Modes renumbered dark -> bright by measured centroid:
  0 DRIFT PAD, 1 GLASS, 2 RINGMOD BELL, 3 SUB GROWL, 4 SOFT SQUARE,
  5 WARM TRI, 6 WAVEFOLDER, 7 MOOG, 8 ACID, 9 FM GROWL, 10 FORMANT VOWEL,
  11 SUPERSAW, 12 CHIP, 13 SAW, 14 SYNC. presets.cpp slot and hg_lanes.cpp
  entries renumbered; a state saved before this boots a different timbre
  until re-saved. Mono lead picker uses LEAD_PICK_HOPS 3 (render.cpp).
- Found: the tracker's fret-release detector (releaseBlank, 35%/50% env
  drop tests) essentially cannot fire on real signals because v->env is a
  peak-hold with ~45 ms decay. synth.cpp has its own damp detector on envS.
- Harness: `hosttest/synthbench/` (+ `SYNTH-FINDINGS.md`).

## Tracker (not changed in this pass)
Diagnosis report (log mining + literature): `TRACKER-DIAGNOSIS.md` in the
session tmp dir (copied below as docs/tracker-diagnosis-2026-10-01.md).
Headlines: v2 only became live at the 2026-10-01 boot; v1 had 72-75% bleed
co-locks on s3/s4; ghost onsets 26-32% on s4/s5 with a 50 ms refractory
eating real picks in strums; v2's -17 dB bleed model has no per-string trim
and will hold back soft notes in chords. Next: calibrate the bleed model on
the rig, relative onset gate + 20 ms refractory, 2-hop relock, provisional
one-period estimate, LMS bleed canceller on the analysis path.

## Verification status
Host: `hosttest/run_tests` ALL PASS (10); bench, pitchbench, lpcbench,
synthbench numbers above. Device: built clean, 0 underruns in the first 90 s.
Listening verdict pending.

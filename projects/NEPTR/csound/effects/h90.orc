; H90 Pitch Shifter — asymmetric: distinct down-shift and up-shift algorithms.
; Signal chain position: 48 (before Pitchfork at 49, old PS at 50)
; Reads: gaH90SendL/R
; Writes: gaH90OutL/R
; Bypass: gkBypassH90
;
; ==========================================================================
; v11 (2026-05-15) — asymmetric branch redesign per the SOTA research pass.
;
; The fundamental problem (Laroche-Dolson 1997/1999, Roebel DAFx-03): a
; phase-vocoder pitch shift is intrinsically asymmetric. DOWN-shift
; compresses the spectrum, partials stay inside their analysis-bin
; support, peak-locking holds vertical phase coherence cleanly, and the
; spectral envelope drifts to a register the ear is forgiving of.
; UP-shift expands the spectrum: partials are demanded between output
; bins, energy bleeds across neighbours (phasiness/swirl), the envelope
; rides into the high-mid band where it sounds chipmunk-y or hollow, and
; high content above sr/(2·R) folds back below Nyquist as audible
; aliasing (the "sharp/fizzy" character).
;
; v10's solution was a single pipeline with branch-specific parameters
; (kkeepform 0 vs 2, anti-alias cutoff tracking ratio). The kkeepform=2
; true-envelope LPC choice fit one source-filter model to polyphonic
; chord content — averaging multiple unrelated harmonic series — and
; produced the flat, smeary, hollow timbre the user reported. The 2×
; butterlp cascade was -24 dB/oct, marginal above +7 semis; fold-back
; was leaking the "sharp" highs.
;
; v11 keeps the down branch (which the user said is perfect) and replaces
; the up branch with three coordinated changes:
;
;   1. NO formant preservation (kkeepform=0). Per the user's spec:
;      "don't match source timbre, just be full and natural in the
;      inherent pitch-shifted timbre". Let formants migrate naturally
;      with the shift; this is musically the right call for guitar
;      (formant migration is part of "real" voice/instrument transposition)
;      and dodges the LPC chord-smearing trap entirely.
;
;   2. Steep anti-alias chain. 4× cascaded butterlp ≈ -48 dB/oct,
;      cutoff tracks min(sr/2-500, sr/(2·R)-500). At +12 (R=2) cutoff
;      sits at 11.5 kHz with ~48 dB/oct rolloff above — the fold-back
;      band is killed.
;
;   3. Warmth chain after pvsynth: tanh soft-clip + pareq high-shelf cut
;      (-3 dB above 6 kHz). Removes the brittle "digital pitch shifter"
;      edge without coloring the body. Subtle drive, quiet content
;      passes nearly transparent.
;
;   4. Transient sidechain. The PV pipeline has ~16 ms inherent latency;
;      pick attacks get smeared across the synthesis grid and the wet
;      sounds disconnected from the playing. A fast envelope-derivative
;      detector on the dry input opens a low-passed dry-attack
;      passthrough for ~25 ms after each detected onset, scaled at ~35%
;      mix. The LP at 4 kHz keeps the passthrough from adding pick-click
;      harshness on top of the warm wet. The 25 ms envelope is timed to
;      hand off to the PV body just as it emerges from its latency
;      buffer — listener hears: dry-attack → smooth crossfade →
;      pitch-shifted body. This is the cheap Roebel-style transient
;      preservation trick that makes the Eventide H90 feel like real
;      playing instead of a shifted recording.
;
; A selector picks output based on kRatio. The switch happens at kRatio=1.0
; where the existing small-interval blend already pulls output toward 100%
; dry, so the branch transition is inaudible. Both branches USED to run in
; parallel every cycle (~2× FFT cost on instr 48, ~half of it discarded);
; since 2026-08-10 each is gated on kRatio with a ±1 semitone overlap band —
; see BRANCH GATING in instr 48.
;
; ==========================================================================
; Down branch latency:  16 ms inherent (FFT=1024, hop=256)
; Up branch latency:    16 ms inherent (PV path) — sidechain adds 0 ms
; ==========================================================================

instr 48
    aIn = (gaH90SendL + gaH90SendR) * 0.5
    aIn dcblock2 aIn

    kSemiC limit gkSemiH90, gkH90_min, gkH90_max
    kSemi  portk kSemiC, 0.02
    kRatio = pow(2, kSemi / 12)

    ; BRANCH GATING (2026-08-10, perf): only one branch is ever selected, so
    ; running both costs an entire extra PV pipeline (1024-pt FFT/IFFT pair)
    ; plus the 4x butterlp anti-alias chain, every cycle, for nothing. Each
    ; branch now runs only when it is selected or about to be, with a +/-1
    ; semitone hysteresis band around unity (ratio 0.9439..1.0595) in which
    ; BOTH run — a cold pvsanal needs a full 1024-sample frame (~21 ms) to
    ; produce valid output, so the incoming branch must be spun up before
    ; the selector crosses 1.0. The band is exactly the |semi|<1 region
    ; where the small-interval blend below already pulls output toward dry,
    ; so any residual warm-up is masked.
    ; kWarmBoth: the band alone only covers a ratio that CREEPS across unity.
    ; kSemi is portk'd here, but a full-range sweep crosses the 2-semitone
    ; band in well under the 21 ms a cold pvsanal needs, so any ratio
    ; movement also forces both branches to run for 25 ms. Sweeps therefore
    ; cost exactly what they did before; the steady state gets the saving.
    aShiftD init 0
    aShiftU init 0
    kRatPrev  init 1
    kWarmBoth init 0
    if (abs(kRatio - kRatPrev) > 0.0005) then
      kWarmBoth = 0.025 * kr
    elseif (kWarmBoth > 0) then
      kWarmBoth = kWarmBoth - 1
    endif
    kRatPrev = kRatio

    ; ---------- DOWN BRANCH (kRatio <= 1.0) — unchanged from v10 ----------
    if (kRatio < 1.0595 || kWarmBoth > 0) then
      fAnalD pvsanal aIn, 1024, 256, 1024, 1
      fLockedD pvslock fAnalD, 1
      fShiftedD pvscale fLockedD, kRatio, 0, 4
      aShiftD pvsynth fShiftedD
    endif

    ; ---------- UP BRANCH (kRatio > 1.0) — v11 redesign --------------------
    if (kRatio > 0.9439 || kWarmBoth > 0) then
      ; Anti-alias chain: cutoff tracks 1/kRatio so fold-back band is killed.
      kCutA = sr * 0.5 - 500
      kCutB = sr * 0.5 / kRatio - 500
      if (kCutB < kCutA) then
        kCutoffUp = kCutB
      else
        kCutoffUp = kCutA
      endif
      aPre1 butterlp aIn, kCutoffUp
      aPre2 butterlp aPre1, kCutoffUp
      aPre3 butterlp aPre2, kCutoffUp
      aPre4 butterlp aPre3, kCutoffUp

      ; v12: pvslock removed from UP branch only — DOWN branch keeps it.
      ; The Laroche-Dolson 1999 peak-locking algorithm is designed for
      ; monophonic harmonic signals. On polyphonic guitar (chord material)
      ; up-shifted, it locks the combined peak of multiple chord voices to
      ; a single phase trajectory, smearing the harmonics of each voice
      ; into an inharmonic composite — this is the "certain chords sound
      ; wrong like harmonics aren't shifted properly" the user described.
      ; Down-shift compresses the spectrum so per-voice peaks stay
      ; isolated and pvslock helps — kept there. Up-shift expands the
      ; spectrum so per-voice peaks collide; pvslock then hurts. Up gets
      ; pure pvscale with no peak protection — the natural PV phasiness
      ; is the cost of getting clean chord harmonics back.
      fAnalU pvsanal aPre4, 1024, 256, 1024, 1
      fShiftedU pvscale fAnalU, kRatio, 0, 4
      aShiftU pvsynth fShiftedU

      ; Warmth chain — soft-clip + gentle high-shelf cut.
      aShiftU = tanh(aShiftU)
      aShiftU pareq aShiftU, 6000, ampdb(-3), 0.7, 2

      ; Transient sidechain — detect pick attack on dry input, open LP-filtered
      ; dry-attack passthrough for ~25 ms. Hands off to PV body as it emerges
      ; from its latency buffer.
      aEnv follow2 aIn, 0.001, 0.015
      kEnv downsamp aEnv
      kEnvPrev init 0
      kDelta = kEnv - kEnvPrev
      kEnvPrev = kEnv
      kAtkEnv init 0
      ; ksmps-INDEPENDENT (2026-08-10). BOTH numbers in this detector were
      ; per-k-cycle constants tuned at ksmps=256 (kr=187.5 Hz); we now run
      ; ksmps=64, which broke them in opposite directions and between them
      ; had left this sidechain barely functional:
      ;   - kDelta is the envelope RISE OVER ONE K-PERIOD. At ksmps=64 that
      ;     period is 1.33 ms instead of 5.3 ms, so the same physical pick
      ;     produces a ~4x smaller delta and mostly failed to cross a fixed
      ;     0.05 — onsets were being missed outright.
      ;   - the decay was a hardcoded 0.77 per k-cycle = ~25 ms at kr=187.5
      ;     but only ~6 ms at kr=750, so the few transients that did fire
      ;     were cut off long before the PV body emerges from its 16 ms
      ;     latency — exactly the gap this sidechain exists to bridge.
      ; Threshold scales with the period; decay uses the habit.orc:128 form.
      if (kDelta > 0.05 * (ksmps / 256)) then
        kAtkEnv = 1.0
      else
        kAtkEnv = kAtkEnv * exp(-1 / (0.025 * kr))
      endif

      ; LP-filter the dry-attack tap so it adds presence not pick-click harshness.
      aDryAtk butterlp aIn, 4000
      aDryAtk butterlp aDryAtk, 4000

      aShiftU = aShiftU + aDryAtk * kAtkEnv * 0.35
    endif

    ; ---------- BRANCH SELECTOR --------------------------------------------
    if (kRatio > 1.0) then
      aShift = aShiftU
    else
      aShift = aShiftD
    endif

    ; ---------- SMALL-INTERVAL BLEND (unchanged) ---------------------------
    ; Transparent at |semi|<1 — output blends toward dry so micro-detunes
    ; don't expose phase-vocoder artifacts at near-unity ratios.
    kAbsSemi = abs(kSemi)
    kBlend init 0
    if (kAbsSemi < 1.0) then
      kBlend = 1.0 - kAbsSemi
    else
      kBlend = 0.0
    endif
    kBlend portk kBlend, 0.02
    kBlendInv = 1.0 - kBlend
    aOut = aShift * kBlendInv + aIn * kBlend

    gaH90OutL = aOut
    gaH90OutR = aOut
endin

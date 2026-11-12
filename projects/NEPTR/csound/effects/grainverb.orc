; GRAINVERB — the MOOD II reverb algorithm, as its own effect
; ---------------------------------------------------------------------------
; Menu: DIGIVERB (9) · code gvb · toggle 73 · instr 188
; Reads:  gaGvbSendL / gaGvbSendR
; Writes: gaGvbWetL  / gaGvbWetR
; Bypass: gkGvbBypass (1 = bypassed)
;
; WHAT IT IS: "a dense cluster of echoes that can be smudged and softened"
; (the Chase Bliss MOOD MkII manual's own description of its reverb mode).
; 6 discrete taps (independently placed on L and R so the cluster is
; stereo-decorrelated from the first reflection) feeding 4 cascaded
; Schroeder allpasses whose lengths scale with tap spacing (so the gaps
; between widely-spaced taps don't sit audibly empty at long times).
;
; 2026-07-26 upgrade (character-preserving — old sound unchanged at
; defaults; renamed control globals to the gkGvb* convention):
;   1. WIDTH: independent per-side tap-jitter offsets (up to ~15 ms, fixed
;      decorrelated pattern, not randomized per-run so behavior stays
;      testable) PLUS a mid/side lift (+3 dB on side) applied only to the
;      final output taps (not fed back into the recirculating loop, so loop
;      stability/decay character is untouched) — it blooms wide, not
;      politely.
;   2. Tempo-aware grain spacing: when gkLpMasterLen > 0 (a master loop
;      exists), tap spacing snaps to a stepped multiple of
;      gkChaseBlissSubdivSec (0.5x/1x/2x/3x/4x/6x/8x, selected by P2) so the
;      granular pre-echo cluster pulses with the drums. Free-running ms
;      spacing (old behavior) otherwise. ON by default (automatic — no
;      separate toggle).
;   3. In-loop damping on P3 (decay+damp): the recirculating feedback path
;      (not the transient allpass excitation) is one-pole lowpassed, more so
;      as P3 increases, so long/hot settings darken instead of fizzing.
;   4. Old MODIFY/Smear control (discrete cluster <-> washed diffuse tail)
;      is retained as a fixed internal constant at its old default (0.5) —
;      P1-P3 are now Mix / Spacing / Decay+Damp per the locked allocation
;      table; Smear is no longer live-controlled.
;
; P1 gkGvbMix     0..1  dry/wet (chain-level ntrpol; 0 = bit-exact null)
; P2 gkGvbSpacing 0..1  grain spacing/subdiv: free-run 15..500 ms tap
;                       spacing, OR (tempo-locked) stepped multiple of
;                       gkChaseBlissSubdivSec: 0.5/1/2/3/4/6/8x
; P3 gkGvbDecay   0..1  size/decay + in-loop damping (higher = longer AND
;                       darker recirculation)
;
; RAM: ~640 KB of ftables (one ring + one allpass table).
; ---------------------------------------------------------------------------

gkGvbBypass  init 1
gkGvbMix     init 1
gkGvbSpacing init 0.5
gkGvbDecay   init 0.5
gaGvbSendL   init 0
gaGvbSendR   init 0
gaGvbWetL    init 0
gaGvbWetR    init 0

; fixed (no longer live) — preserves old default Smear=0.5 sound
giGvbSmearFixed = 0.5

; fixed per-side tap jitter pattern (ms), up to ~15 ms, hand-decorrelated
giGvbJitL0 = 0.0
giGvbJitL1 = 3.1
giGvbJitL2 = -2.4
giGvbJitL3 = 5.7
giGvbJitL4 = -4.3
giGvbJitL5 = 7.9
giGvbJitR0 = -1.6
giGvbJitR1 = 4.4
giGvbJitR2 = -3.8
giGvbJitR3 = 6.2
giGvbJitR4 = -5.9
giGvbJitR5 = 2.8
giGvbSideLiftDb = 3.0

giGvbRingN = 32768
giGvbRngL  ftgen 0, 0, -giGvbRingN, -2, 0
giGvbRngR  ftgen 0, 0, -giGvbRingN, -2, 0
giGvbApL   ftgen 0, 0, -65536, -2, 0
giGvbApR   ftgen 0, 0, -65536, -2, 0

opcode Gvb_Osc3f, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
endop

opcode Gvb_Osc3i, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
endop

instr 188
  kSpacing port limit:k(gkGvbSpacing, 0, 1), 0.05, i(gkGvbSpacing)
  kDecay   port limit:k(gkGvbDecay, 0, 1), 0.05, i(gkGvbDecay)

  ; ---- tap spacing: tempo-locked (stepped subdiv multiple) or free (ms) ----
  kMultIdx = int(kSpacing * 6.999)
  kMult = (kMultIdx == 0 ? 0.5 : (kMultIdx == 1 ? 1 : (kMultIdx == 2 ? 2 : \
           (kMultIdx == 3 ? 3 : (kMultIdx == 4 ? 4 : (kMultIdx == 5 ? 6 : 8))))))
  kWTtempo = gkChaseBlissSubdivSec * kMult * sr
  kWTfree  = 720 + kSpacing * 23280
  kWTraw = (gkLpMasterLen > 0 ? kWTtempo : kWTfree)
  kWTraw limit kWTraw, 240, 26000

  ; ---- SLEW kWT (2026-09-23) ----------------------------------------------
  ; kWT is taken straight off the SPACING knob across a 23,280-sample range,
  ; so one encoder step moved every tap delay and all four diffuser offsets by
  ; hundreds of samples IN ONE K-CYCLE. The shared-cursor fix above made that
  ; jump clean rather than tearing, but a clean jump of the read point is
  ; still a step in the output - which is why the measured sweep residual
  ; (p95 6.50e-5) stayed ~400x above the knob-still floor of 1.6e-7.
  ;
  ; A one-pole on kWT makes the lengths move CONTINUOUSLY instead of in one
  ; leap: ~30 ms, fast enough that the knob still feels immediate and a tempo
  ; change lands promptly, slow enough that per-k-cycle movement is a sample
  ; or two rather than hundreds. Seeded on the first k-cycle so the effect
  ; does not glide up from zero when it is switched on.
  kWTs   init 0
  kWTsIn init 0
  if (kWTsIn == 0) then
    kWTs = kWTraw
    kWTsIn = 1
  else
    kWTs = kWTs + (kWTraw - kWTs) * 0.04
  endif
  kWT = kWTs

  kDec = 0.15 + 0.80 * kDecay        ; recirculation decay
  kApG = 0.45 + 0.35 * giGvbSmearFixed   ; allpass coefficient (fixed smear)

  ; in-loop damping: more decay -> more feedback-path lowpass (darkens
  ; instead of fizzing at long/hot settings)
  kDampCoef = 0.1 + 0.75 * kDecay

  ; diffuser lengths scale with kWT (see header — keeps tap gaps from
  ; sounding audibly empty at long spacing). These are READ OFFSETS into the
  ; fixed 16384-slot stages below, not ring lengths — see the kApW note.
  kAL0 = int(limit:k(kWT * 0.13, 48, 16000))
  kAL1 = int(limit:k(kWT * 0.21, 48, 16000))
  kAL2 = int(limit:k(kWT * 0.34, 48, 16000))
  kAL3 = int(limit:k(kWT * 0.55, 48, 16000))

  kJL0 = giGvbJitL0 * 0.001 * sr
  kJL1 = giGvbJitL1 * 0.001 * sr
  kJL2 = giGvbJitL2 * 0.001 * sr
  kJL3 = giGvbJitL3 * 0.001 * sr
  kJL4 = giGvbJitL4 * 0.001 * sr
  kJL5 = giGvbJitL5 * 0.001 * sr
  kJR0 = giGvbJitR0 * 0.001 * sr
  kJR1 = giGvbJitR1 * 0.001 * sr
  kJR2 = giGvbJitR2 * 0.001 * sr
  kJR3 = giGvbJitR3 * 0.001 * sr
  kJR4 = giGvbJitR4 * 0.001 * sr
  kJR5 = giGvbJitR5 * 0.001 * sr
  kSideLift = ampdb(giGvbSideLiftDb)

  aInL = gaGvbSendL
  aInR = gaGvbSendR

  ; =========================================================================
  ; NATIVE A-RATE ENGINE (2026-09-25) — replaces the hand-rolled per-sample
  ; k-rate loop.
  ;
  ; WHY: the old engine ran `while kS < ksmps` and did, per sample, 12 ring
  ; reads with manual wrap + interpolation, 8 allpass table read/writes with
  ; manual cursors, two feedback one-poles and the mid/side maths — all as
  ; interpreted k-rate opcodes. ~150 opcode dispatches x 64 samples x 750
  ; k-cycles/s = ~7M interpreted ops per second for ONE effect. Measured on
  ; the rig: grainverb alone pushed csound to B/Q 1.05-1.10 (over deadline,
  ; 23 xruns), and that overrun IS the "major crackle". It was never the
  ; algorithm's sound, it was its cost.
  ;
  ; SAME ALGORITHM, native opcodes:
  ;   ring + 6 taps          -> delayr / deltapi x6 / delayw  (interpolated)
  ;   4 Schroeder allpasses  -> valpass x4  (variable-length allpass; gain is
  ;                             set via krvt so it equals kApG exactly)
  ;   liveliness one-pole    -> filter2 (fixed 0.35/0.65 pole, exact)
  ;   damping one-pole       -> tone, cutoff derived from the pole so the
  ;                             darkening curve matches the old kDampCoef
  ;   width                  -> a-rate mid/side
  ; Every stage is one C-loop per block instead of 64 interpreter passes.
  ;
  ; THE ONE BEHAVIOURAL DIFFERENCE, stated plainly: the recirculation now has
  ; ONE BLOCK (ksmps = 64 samples, 1.3-1.45 ms) of extra latency in the
  ; feedback path, because the damped feedback written into the ring is the
  ; previous block's. The old loop fed back per sample. For a reverb whose
  ; tap spacing is 5-540 ms this is a ~0.3-27% change in loop length at the
  ; very shortest spacing and negligible elsewhere; it is not audible as a
  ; click, and it is the price of leaving the interpreter.
  ;
  ; Tap delays are converted to seconds and interpolated to a-rate, so the
  ; slewed kWT moves the taps CONTINUOUSLY within the block (the previous
  ; interp+hoist fix, now done natively). Delays are clamped to a floor of
  ; ksmps+4 samples: delayr/deltapi cannot read shorter than the block, and
  ; the old code's answer to a negative delay (jitter > spacing at the
  ; clamp floor) was to wrap to the far end of the ring, i.e. garbage.
  ; giGvbRngL/R, giGvbApL/R and the hoisted tap arrays are no longer used.
  ; =========================================================================
  kMinDel = (ksmps + 4) / sr
  kMaxDel = 0.69

  kDL0 = limit:k((kWT * 0.2137 + kJL0) / sr, kMinDel, kMaxDel)
  kDL1 = limit:k((kWT * 0.3541 + kJL1) / sr, kMinDel, kMaxDel)
  kDL2 = limit:k((kWT * 0.5279 + kJL2) / sr, kMinDel, kMaxDel)
  kDL3 = limit:k((kWT * 0.6857 + kJL3) / sr, kMinDel, kMaxDel)
  kDL4 = limit:k((kWT * 0.8431 + kJL4) / sr, kMinDel, kMaxDel)
  kDL5 = limit:k((kWT * 1.0    + kJL5) / sr, kMinDel, kMaxDel)
  kDR0 = limit:k((kWT * 0.1873 + kJR0) / sr, kMinDel, kMaxDel)
  kDR1 = limit:k((kWT * 0.3907 + kJR1) / sr, kMinDel, kMaxDel)
  kDR2 = limit:k((kWT * 0.4993 + kJR2) / sr, kMinDel, kMaxDel)
  kDR3 = limit:k((kWT * 0.7213 + kJR3) / sr, kMinDel, kMaxDel)
  kDR4 = limit:k((kWT * 0.8059 + kJR4) / sr, kMinDel, kMaxDel)
  kDR5 = limit:k((kWT * 0.9631 + kJR5) / sr, kMinDel, kMaxDel)
  aDL0 interp kDL0
  aDL1 interp kDL1
  aDL2 interp kDL2
  aDL3 interp kDL3
  aDL4 interp kDL4
  aDL5 interp kDL5
  aDR0 interp kDR0
  aDR1 interp kDR1
  aDR2 interp kDR2
  aDR3 interp kDR3
  aDR4 interp kDR4
  aDR5 interp kDR5

  ; recirculation: input + damped feedback (previous block, see above)
  aFbdL init 0
  aFbdR init 0
  aRcL = aInL + aFbdL * kDec
  aRcR = aInR + aFbdR * kDec
  aRcL limit aRcL, -2, 2
  aRcR limit aRcR, -2, 2

  ; ---- left ring: 6 scattered taps ----
  aRingL delayr 0.7
  aTL0 deltapi aDL0
  aTL1 deltapi aDL1
  aTL2 deltapi aDL2
  aTL3 deltapi aDL3
  aTL4 deltapi aDL4
  aTL5 deltapi aDL5
  delayw aRcL
  aSumL = aTL0 * 0.30 + aTL1 * 0.24 + aTL2 * 0.19 + aTL3 * 0.13 + aTL4 * 0.09 + aTL5 * 0.05

  ; ---- right ring ----
  aRingR delayr 0.7
  aTR0 deltapi aDR0
  aTR1 deltapi aDR1
  aTR2 deltapi aDR2
  aTR3 deltapi aDR3
  aTR4 deltapi aDR4
  aTR5 deltapi aDR5
  delayw aRcR
  aSumR = aTR0 * 0.30 + aTR1 * 0.24 + aTR2 * 0.19 + aTR3 * 0.13 + aTR4 * 0.09 + aTR5 * 0.05

  ; ---- 4 Schroeder allpasses = SMEAR. valpass derives its gain as
  ; 0.001^(delay/rvt); choosing rvt = delay * ln(0.001)/ln(kApG) makes that
  ; gain EXACTLY kApG for every stage regardless of its length. ----
  kApRvt = -6.907755 / log(kApG)
  kALs0 = kAL0 / sr
  kALs1 = kAL1 / sr
  kALs2 = kAL2 / sr
  kALs3 = kAL3 / sr
  aApL valpass aSumL, kALs0 * kApRvt, kALs0, 0.4
  aApL valpass aApL,  kALs1 * kApRvt, kALs1, 0.4
  aApL valpass aApL,  kALs2 * kApRvt, kALs2, 0.4
  aApL valpass aApL,  kALs3 * kApRvt, kALs3, 0.4
  aApR valpass aSumR, kALs0 * kApRvt, kALs0, 0.4
  aApR valpass aApR,  kALs1 * kApRvt, kALs1, 0.4
  aApR valpass aApR,  kALs2 * kApRvt, kALs2, 0.4
  aApR valpass aApR,  kALs3 * kApRvt, kALs3, 0.4

  ; MODIFY min = discrete cluster, max = wash (fixed default blend)
  aWetL = aSumL * (1 - giGvbSmearFixed) + aApL * giGvbSmearFixed
  aWetR = aSumR * (1 - giGvbSmearFixed) + aApR * giGvbSmearFixed

  ; ---- feedback conditioning, exactly the old two one-poles ----
  ; liveliness: y = 0.65 x + 0.35 y[n-1]   (fixed pole -> filter2, exact)
  aFbL filter2 aWetL, 1, 1, 0.65, -0.35
  aFbR filter2 aWetR, 1, 1, 0.65, -0.35
  ; damping: y = p y[n-1] + (1-p) x with p = kDampCoef (variable). Expressed
  ; as tone's half-power cutoff by solving |H|^2 = 1/2 for that pole:
  ;   cos(w) = (-1 + 4p - p^2) / (2p),  f = w * sr / 2pi
  ; p = 0.1 -> (no filtering, cos < -1, clamped to ~sr/2); p = 0.85 -> ~1.1 kHz.
  ; NOTE (2026-09-25): the first version of this used `acos(...)`, which
  ; is NOT a Csound function (arccos is spelled `cosinv`). The orchestra
  ; therefore failed to COMPILE on the rig and csound exited - the whole
  ; engine went down until the file was rolled back. Both machines run
  ; Csound 6.18; the Mac check had simply been misread. Clamping cos(w) to
  ; (-1, 1) before cosinv gives the endpoints ~sr/2 (no filtering) and 20 Hz.
  kDp   = kDampCoef
  kCosW = (-1 + 4 * kDp - kDp * kDp) / (2 * kDp)
  kCosW limit kCosW, -0.9999, 0.9999
  kW    = cosinv(kCosW)   ; Csound spells arccos "cosinv" - acos does not exist here
  kDampHz = kW * sr / 6.2831853
  kDampHz limit kDampHz, 20, sr * 0.45
  aFbdL tone aFbL, kDampHz
  aFbdR tone aFbR, kDampHz
  denorm aFbdL, aFbdR

  ; ---- WIDTH: mid/side lift on the OUTPUT taps only (loop untouched) ----
  aMid  = (aWetL + aWetR) * 0.5
  aSide = (aWetL - aWetR) * 0.5 * kSideLift
  aOutL = aMid + aSide
  aOutR = aMid - aSide

  aOutL clip aOutL, 0, 0.99, 0.7
  aOutR clip aOutR, 0, 0.99, 0.7
  ; output trim tuned via bench harness for unity RMS vs. dry at defaults
  iGvbTrim = 4.0
  gaGvbWetL = aOutL * iGvbTrim
  gaGvbWetR = aOutR * iGvbTrim

  kc1 changed gkGvbBypass
  if (kc1 == 1) then
    Gvb_Osc3i "/gvb_state", gkGvbBypass
  endif
  kc2 changed gkGvbMix
  if (kc2 == 1) then
    Gvb_Osc3f "/gvb_mix", gkGvbMix
  endif
  kc3 changed gkGvbSpacing
  if (kc3 == 1) then
    Gvb_Osc3f "/gvb_spacing", gkGvbSpacing
  endif
  kc4 changed gkGvbDecay
  if (kc4 == 1) then
    Gvb_Osc3f "/gvb_decay", gkGvbDecay
  endif
endin

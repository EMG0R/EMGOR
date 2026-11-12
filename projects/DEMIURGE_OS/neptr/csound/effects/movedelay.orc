; POLY DLY v5.1 — clean clock-locked pitch-cascade delay (2026-10-22)
; Signal chain position: 191 (chain 20.2, shape unchanged)
; Reads:  gaMvdInL/R
; Writes: gaMvdWetL/R   (dry at unity + wet; no mix control)
; Bypass: gkMvdBypass (1 = bypassed, house convention); bypass branch passes
;         the input straight through to gaMvdWetL/R as before.
; Spec:   docs/specs/2026-10-22-polydelay-v5.md. Replaces the v3/v4 16-mode
;         engine and movedelay_modes.orc (deleted).
;
; User's verbatim intent (EMGOR, 2026-10-22): "maintain the interpolated delay
; vibe (Doppler glide when time moves) but define exactly how many equal-volume
; delays as a number, not feedback; two numbers make an a/b delay; no mix —
; 100 % dry + 100 % wet, super clean, no filtering; every repeat pitch-shifts
; another interval, highest quality; attack/decay pitch envelope relative to
; delay time; time locked to clock subdivisions; two delays hard L and R with
; different subdivisions but shared attack/decay." Max tempo 300 BPM is fine.
;
; Two independent stacks: L stack (hard left) and R stack (hard right), up to
; 8 repeats each. Repeat k of a stack plays at k*T, T = Num/Den * 60/BPM
; (BPM = gkChaseBlissBPM limited 40..300, no fallback). Repeat k is shifted
; k * Interval * (1 - gkMvdExpr) * env semitones (cumulative). Every repeat is
; unity gain, no feedback.
;
; Design decisions:
;  - PITCH SHIFTER (v5.1, 2026-10-22 later the same night): Csound `mincer`
;    (phase-locked resampling phase vocoder reading a ring table), one per
;    tap, 16 total, unrolled via PDLY_TAP (opcode instances can't live in a
;    loop). It replaces v5's pvsanal/pvscale/pvsynth UDO PdlyShift, which was
;    measured NOT equal-volume (-2.3 dB at +14, -14.5 dB at +21, about -11 dB
;    at +28, frame-rate sidebands) because pvscale re-bins.
;    MEASURED (offline, Csound 6.18, steady 220 Hz sine unless noted):
;      level 0.00 dB at +7 +12 +14 +21 +24 +28 +36 -12 -24 (unity at every
;      ratio tested); pitch error < 0.01 %; spurs below -58 dBc on a pure
;      tone. -36 is below the FFT's resolution at 220 Hz (purity 5 dB), which
;      is why PDLY_MAXDN is -36 and not lower.
;  - GEOMETRY (Opcodes/pvlock.c sprocess1): hop = N/decim; a frame fires on
;    the first sample the opcode runs and then every hop samples THAT IT RUNS.
;    Read start spos = hop*trunc(atimpt*sr/hop) wrapped into the table; the
;    front window reads N samples spaced `pitch` apart forward from spos (span
;    N*pitch source samples), the back window starts hop*pitch earlier. The
;    output at pointer p is centred on source sample p + N*(pitch-1)/2, so
;    for the repeat to land on the grid ptr = W - LAT - N*(pitch-1)/2 (W the
;    ring write index) and the window end needs LAT >= N*(pitch+1)/2.
;  - RING + PRE-READ: each tap has its own ring table ($PDLY_RING samples,
;    giPdlyRing<S><K>), written EVERY cycle (so it is warm when the shifter
;    switches on) at the shared index aIdx = upsamp(kIdx0) + aRamp*ksmps. kIdx0 is
;    a k-rate counter advanced at the very end of the instrument body;
;    PDLY_RING MUST be a multiple of ksmps (and of the hop). The ring is fed
;    by a pre-read `deltap3` at K*T - (LAT + r)/sr; the exact (unshifted) tap
;    is a second deltap3 at K*T. The pre-read glides (portk 0.2 s, seeded at
;    its i-time value) = the Doppler glide. LAT = min(FFT*(Pmax+1)/2,
;    K*T*sr - MARGIN), Pmax = min(2^K, 8): 3072, 5120, 9216 at taps 1, 2, 3+.
;  - HOP ALIGNMENT AND r: mincer's trunc-to-hop makes content late by
;    r = ptr mod hop (0..hop-1). Frame instants sit on ring indices = 0 mod
;    hop, so for a static pitch r = (-LAT - N*(P-1)/2) mod hop, a constant,
;    absorbed in the pre-read delay (r is computed from the STATIC peak
;    pitch P at depth 1, so it does not wobble with the pedal or envelope).
;    Activation/run are latched only on cycles where kIdx0 % hop == 0, so
;    mincer only starts/pauses on frame boundaries (the opcode does not count
;    skipped cycles) and the compensation stays valid.
;  - ANTI-ALIAS: when the static up-shift > 1.001, the pre-read goes through
;    FOUR butterlp at 0.9*sr/(2*P) before the ring (measured necessary: a +19
;    shift of a sawtooth had off-harmonic energy only 17.6 dB below the
;    harmonics in 6-12 kHz; with the filter 49.7 dB).
;  - CAPS: kCapUp = 12*log2(2*LAT/N - 1) is the up-shift one tap can reach in
;    the time it has; the shift is limited to [PDLY_MAXDN -36, min(PDLY_MAXUP
;    +36, kCapUp)]. 1/8 and slower are uncapped to +12 per repeat at every
;    tempo <= 300; a 1/16 repeat 1 is uncapped to about 185 BPM, +11 at 200,
;    0 (unshifted) at the T floor; repeats 2+ are uncapped. T FLOOR: T >=
;    (FFT + MARGIN)/sr, MARGIN = 2*ksmps + hop = 56 ms at decim 4 (1/16 exact
;    to 268 BPM; above that T is clamped to the floor, never muted), 50.7 ms
;    at decim 8. The hop in MARGIN guarantees the pre-read never falls under
;    one k-cycle after the kR correction, so LAT >= N and timing is exact at
;    every T above the floor.
;  - DECIM (CPU vs hop): PDLY_DECIM 4 (hop 512) is the shipped default, chosen
;    for CPU headroom on the rig (the engine already sits at 60-88 % of the
;    audio period before POLY DLY and the HAT wedges on a late cycle). 16
;    shifters render 6 s of audio in 2.4 s on a Pi 4 at decim 4 vs 4.4 s at
;    decim 8 in the isolated prototype (v5: 6.1 s); purity on pure tones is
;    equal. Cost at decim 4: the
;    hop residual (see TIMING) is <= 10.7 ms, and the pitch envelope is
;    re-read once per hop (10.7 ms). PDLY_DECIM 8 (hop 256, residual 5.3 ms)
;    is the quality setting to flip to once the rig shows headroom.
;    PDLY_MAXREPS (default 8) clamps Repeats in the engine: the CPU knob for
;    a tight rig (taps above it never run).
;  - TIMING (measured): static timing is exact to the sample while the pitch
;    sits at its static peak; up to one hop (5.3 ms at decim 8, 10.7 ms at
;    decim 4) LATE while the envelope is between 0 and peak or the EXPR pedal
;    is partially pressed (inherent to the hop-quantised pointer; the exact
;    tap and the fully pressed pedal are exact). Phase-vocoder pre-echo on
;    transients grows with ratio: the 10 %-of-peak onset leads the grid by
;    about 9 ms at +7, 15 ms at +12, 43 ms at +24 while the energy centroid of
;    a 0.2 s burst stays within +-2.6 ms (inherent: the analysis window spans
;    N*(pitch+1)/2 either side).
;  - BENCH (neptr/scripts/bench, decim 4, INTV 7, 1 s burst, Reps k minus k-1):
;    plateau level 0.00 dB at +7 +14 +21 +28 both stacks; pitch error <= 0.02 %;
;    10 %-onset minus unshifted twin (0.2 s burst) -3/-1 ms (k1), -14/-11
;    (k2), -24/-26 (k3), -49/-47 (k4); energy centroid within +-3 ms. A 0.2 s
;    burst is shorter than the analysis smear at high ratios (plateau -0.5 dB
;    at k4), the 1 s burst is the level reference. The first ~N*(pitch+1)/2 of
;    a repeat is smeared (measured pitch there reads low).
;  - CPU (offline, Pi 4, 6 s of audio, 16 shifters): prototype decim 8 = 4.4 s,
;    decim 4 = 2.4 s (v5 pvs: 6.1 s). Full bench render of this file (REPS=8
;    INTV=7 BURST=1, includes clock, 16 deltap pairs, 16 anti-alias chains and
;    ring writes): decim 4 = 3.6 s, decim 8 = 5.4 s, no shifters (INTV=0) =
;    1.4 s.
;  - Shifter activity keys off the static peak shift (k*Interval*depth), not
;    the instantaneous env, so shifters stay warm while the env sweeps 0. Small
;    shifts blend to the exact tap (|semi| < 1).
;  - ENV PHASE SOURCE: gkLpMasterLen > 0 -> phase = (gkLpRdPos1/sr mod T)/T
;    (gkLpRdPos1 = ch1 loop read position in samples, the same global the
;    /lp1_pos echo is derived from); else a phasor at 1/T. One env per
;    stack (T differs L vs R), Attack/Decay shared. Attack ramps 0->1 over
;    A*T, decay 1->0 over D*T ending at window end, hold 1 between; A+D > 1
;    scales both by 1/(A+D) so ramps meet at a peak; A = D = 0 -> env = 1.
;  - Shifter activity keys off the static peak shift (k*Interval*depth), not
;    the instantaneous env, so shifters stay warm while the env sweeps 0.
;  - Tap time is portk'd (half-time 0.2 s) = Doppler glide when Num/Den move.
;    Taps with k*T > 15.9 s are muted (gain 0), never clamped. 16 s lines.
;  - Per-tap gain mask (k <= Repeats), portk'd 10 ms.
;  - UNITY GAIN FLAGGED: no per-repeat normalisation (user's choice); the
;    mono input is (L+R)/2 so a mono source is unity per repeat. Shift is
;    clamped (see CAPS above: -36..+36 per tap, further
;    limited by the per-tap time cap).
;  - Hidden filter (not in UI, not persisted): gkMvdFiltDepth (0 = skipped
;    entirely), gkMvdFiltQ, gkMvdFiltBase Hz; resonant 2-pole LP per stack,
;    cutoff = base * 2^(depth*6*env) riding the same pitch env.
; ---------------------------------------------------------------------------

#define PDLY_FFT    #2048#
#ifndef PDLY_DECIM
#define PDLY_DECIM  #4#       ; hop 512 (10.7 ms). 8 = hop 256, quality setting; 4 halves CPU, same pure-tone purity (--omacro:PDLY_DECIM=8 to bench)
#end
#define PDLY_RING   #32768#   ; per-tap ring: >= LATmax 9216 + FFT*8 + hop*8; MULTIPLE OF hop AND ksmps
#define PDLY_MARGIN #(128 + $PDLY_FFT/$PDLY_DECIM)#  ; lookahead headroom: 2*ksmps + one hop, so the hop-residual correction (kR < hop) never pushes the pre-read under one k-cycle. Sets the T floor: (FFT+MARGIN)/sr = 56 ms at decim 4 (1/16 exact to 268 BPM), 50.7 ms at decim 8
#define PDLY_MAXDN  #-36#     ; semitone clamp down (ratio 1/8)
#define PDLY_MAXUP  #36#      ; semitone clamp up (ratio 8) before the per-tap time cap
#ifndef PDLY_MAXREPS
#define PDLY_MAXREPS #8#      ; clamps gkMvdReps (1..8): CPU knob for a tight rig
#end

gkMvdBypass   init 1        ; 1 = bypassed
gkMvdNumL     init 1        ; 1..16
gkMvdDenL     init 2        ; 1..16
gkMvdNumR     init 1        ; 1..16
gkMvdDenR     init 3        ; 1..16
gkMvdAtt      init 0        ; 0..1 pitch-env attack (fraction of T)
gkMvdDec      init 0        ; 0..1 pitch-env decay  (fraction of T)
gkMvdReps     init 4        ; 1..8
gkMvdIntv     init 7        ; -12..+12 semitones per repeat
gkMvdExpr     init 0        ; expression: 0 = full interval, 1 = unison
gkMvdFiltDepth init 0       ; hidden, 0 = bypassed
gkMvdFiltQ     init 2
gkMvdFiltBase  init 400

gaMvdInL  init 0
gaMvdInR  init 0
gaMvdWetL init 0
gaMvdWetR init 0

; per-tap rings (16): giPdlyRingL1..L8, giPdlyRingR1..R8
#define PDLY_RINGT(S'K)# giPdlyRing$S.$K. ftgen 0, 0, $PDLY_RING, 2, 0 #
$PDLY_RINGT(L'1)
$PDLY_RINGT(L'2)
$PDLY_RINGT(L'3)
$PDLY_RINGT(L'4)
$PDLY_RINGT(L'5)
$PDLY_RINGT(L'6)
$PDLY_RINGT(L'7)
$PDLY_RINGT(L'8)
$PDLY_RINGT(R'1)
$PDLY_RINGT(R'2)
$PDLY_RINGT(R'3)
$PDLY_RINGT(R'4)
$PDLY_RINGT(R'5)
$PDLY_RINGT(R'6)
$PDLY_RINGT(R'7)
$PDLY_RINGT(R'8)

; One tap: K = 1..8, S = L|R. Needs kT$S (floored target T s), iT$S, kEnv$S,
; kReps, kIntv, kDepth, aIdx, kIdx0. Accumulates into aSum$S. Must sit inside
; the stack's delayr/delayw block.
#define PDLY_TAP(K'S)#
  ; --- i-time seeds (static peak shift at depth 1 from the current globals)
  iPmx$S.$K.  = min(2^$K, 8)
  iLatN$S.$K. = $PDLY_FFT * (iPmx$S.$K. + 1) / 2
  iTk$S.$K.   = $K * iT$S.
  iLat$S.$K.  = min(iLatN$S.$K., iTk$S.$K. * sr - $PDLY_MARGIN)
  iCap$S.$K.  = 12 * log(max(2 * iLat$S.$K. / $PDLY_FFT - 1, 1)) / log(2)
  iPs$S.$K.   = limit($K * limit(i(gkMvdIntv), -12, 12), $PDLY_MAXDN, min($PDLY_MAXUP, iCap$S.$K.))
  iX$S.$K.    = -iLat$S.$K. - $PDLY_FFT * (2^(iPs$S.$K./12) - 1) / 2
  iR$S.$K.    = iX$S.$K. - iHop * floor(iX$S.$K. / iHop)
  iPre$S.$K.  = limit(iTk$S.$K. - (iLat$S.$K. + iR$S.$K.) / sr, ksmps / sr, 15.9)
  ; --- k-rate geometry
  kTk$S.$K.   = $K * kT$S.
  kLat$S.$K.  = min(iLatN$S.$K., kTk$S.$K. * sr - $PDLY_MARGIN)
  kCap$S.$K.  = 12 * log(max(2 * kLat$S.$K. / $PDLY_FFT - 1, 1)) / log(2)
  kLim$S.$K.  = min($PDLY_MAXUP, kCap$S.$K.)
  kPk$S.$K.   = limit($K * kIntv * kDepth, $PDLY_MAXDN, kLim$S.$K.)
  kPkSt$S.$K. = limit($K * kIntv, $PDLY_MAXDN, kLim$S.$K.)
  kPSt$S.$K.  = 2 ^ (kPkSt$S.$K. / 12)
  kX$S.$K.    = -kLat$S.$K. - $PDLY_FFT * (kPSt$S.$K. - 1) / 2
  kR$S.$K.    = kX$S.$K. - iHop * floor(kX$S.$K. / iHop)
  kPreT$S.$K. = limit(kTk$S.$K. - (kLat$S.$K. + kR$S.$K.) / sr, ksmps / sr, 15.9)
  kPre$S.$K.  portk kPreT$S.$K., 0.2, iPre$S.$K.
  aPre$S.$K.  deltap3 kPre$S.$K.
  kExT$S.$K.  = limit(kTk$S.$K., ksmps / sr, 15.9)
  kEx$S.$K.   portk kExT$S.$K., 0.2, limit(iTk$S.$K., ksmps / sr, 15.9)
  aEx$S.$K.   deltap3 kEx$S.$K.
  kMk$S.$K.   = ($K <= kReps && $K * kT$S. <= 15.9 ? 1 : 0)
  kGn$S.$K.   portk kMk$S.$K., 0.01
  ; --- anti-alias, then write the ring every cycle
  aW$S.$K. = aPre$S.$K.
  if (kPSt$S.$K. > 1.001) then
    kCut$S.$K. = 0.9 * sr / (2 * kPSt$S.$K.)
    aA1$S.$K. butterlp aPre$S.$K., kCut$S.$K.
    aA2$S.$K. butterlp aA1$S.$K., kCut$S.$K.
    aA3$S.$K. butterlp aA2$S.$K., kCut$S.$K.
    aW$S.$K.  butterlp aA3$S.$K., kCut$S.$K.
  endif
  tablew aW$S.$K., aIdx, giPdlyRing$S.$K., 0, 0, 1
  ; --- hop-aligned activation
  kWant$S.$K. = ($K <= kReps && abs(kPk$S.$K.) > 0.001 ? 1 : 0)
  kAct$S.$K.  init 0
  kRun$S.$K.  init 0
  if (kIdx0 % iHop == 0) then
    kAct$S.$K. = kWant$S.$K.
  endif
  kOn$S.$K.   portk kAct$S.$K., 0.02
  if (kIdx0 % iHop == 0) then
    kRun$S.$K. = (kAct$S.$K. == 1 || kOn$S.$K. > 0.001 ? 1 : 0)
  endif
  kSemi$S.$K.  = kPk$S.$K. * kEnv$S.
  kSemiP$S.$K. portk kSemi$S.$K., 0.02, (i(gkMvdAtt) + i(gkMvdDec) == 0 ? iPs$S.$K. * (1 - limit(i(gkMvdExpr), 0, 1)) : 0)
  kPch$S.$K.   = 2 ^ (kSemiP$S.$K. / 12)
  aSh$S.$K. = 0
  if (kRun$S.$K. == 1) then
    aTim$S.$K. = (aIdx + $PDLY_RING - kLat$S.$K. - $PDLY_FFT * (kPch$S.$K. - 1) * 0.5) / sr
    aSh$S.$K.  mincer aTim$S.$K., 1, kPch$S.$K., giPdlyRing$S.$K., 1, $PDLY_FFT, $PDLY_DECIM
  endif
  kBl$S.$K.  = (abs(kSemiP$S.$K.) < 1 ? 1 - abs(kSemiP$S.$K.) : 0)
  kBlP$S.$K. portk kBl$S.$K., 0.02
  kWet$S.$K. = kOn$S.$K. * (1 - kBlP$S.$K.)
  aSum$S. = aSum$S. + (aSh$S.$K. * kWet$S.$K. + aEx$S.$K. * (1 - kWet$S.$K.)) * kGn$S.$K.
#

instr 191
  aInL = gaMvdInL
  aInR = gaMvdInR
  aMono = (aInL + aInR) * 0.5

  ; i-time T estimate: portk's default start is 0, which would make every
  ; tap glide in from 1 ms on instrument start; seed it at the resting value.
  iBeat = 60 / limit(i(gkChaseBlissBPM), 40, 300)
  iTL = max(limit(int(i(gkMvdNumL) + 0.5), 1, 16) / limit(int(i(gkMvdDenL) + 0.5), 1, 16) * iBeat, ($PDLY_FFT + $PDLY_MARGIN)/sr)
  iTR = max(limit(int(i(gkMvdNumR) + 0.5), 1, 16) / limit(int(i(gkMvdDenR) + 0.5), 1, 16) * iBeat, ($PDLY_FFT + $PDLY_MARGIN)/sr)
  kByp = (gkMvdBypass >= 0.5 ? 1 : 0)

  ; ---- clock ----
  kBPM  limit gkChaseBlissBPM, 40, 300
  kBeat = 60 / kBPM
  kNL = limit(int(gkMvdNumL + 0.5), 1, 16)
  kDL = limit(int(gkMvdDenL + 0.5), 1, 16)
  kNR = limit(int(gkMvdNumR + 0.5), 1, 16)
  kDR = limit(int(gkMvdDenR + 0.5), 1, 16)
  kTL = max(kNL / kDL * kBeat, ($PDLY_FFT + $PDLY_MARGIN)/sr)
  kTR = max(kNR / kDR * kBeat, ($PDLY_FFT + $PDLY_MARGIN)/sr)
  kReps  = limit(int(gkMvdReps + 0.5), 1, $PDLY_MAXREPS)
  kIntv  = limit(gkMvdIntv, -12, 12)
  kDepthT = 1 - limit(gkMvdExpr, 0, 1)
  kDepth portk kDepthT, 0.05

  ; ---- pitch envelope (per stack, phase locked) ----
  kA = limit(gkMvdAtt, 0, 1)
  kD = limit(gkMvdDec, 0, 1)
  if (kA + kD > 1) then
    kAn = kA / (kA + kD)
    kDn = kD / (kA + kD)
  else
    kAn = kA
    kDn = kD
  endif
  kPhFreeL phasor 1 / kTL
  kPhFreeR phasor 1 / kTR
  if (gkLpMasterLen > 0) then
    kPosS = gkLpRdPos1 / sr
    kPhL = (kPosS - kTL * int(kPosS / kTL)) / kTL
    kPhR = (kPosS - kTR * int(kPosS / kTR)) / kTR
  else
    kPhL = kPhFreeL
    kPhR = kPhFreeR
  endif
  kRiseL = (kAn > 0 ? min(kPhL / kAn, 1) : 1)
  kFallL = (kDn > 0 ? min((1 - kPhL) / kDn, 1) : 1)
  kRiseR = (kAn > 0 ? min(kPhR / kAn, 1) : 1)
  kFallR = (kDn > 0 ? min((1 - kPhR) / kDn, 1) : 1)
  kEnvL = min(kRiseL, kFallL)
  kEnvR = min(kRiseR, kFallR)

  iHop = $PDLY_FFT / $PDLY_DECIM
  kIdx0 init 0                  ; ring index of the first sample of this k-cycle
  aRamp phasor sr / ksmps       ; exact 0..1 ramp per k-cycle
  aIdx0 upsamp kIdx0            ; NOT a(): a(k) interpolates from the previous k value
  aIdx = aIdx0 + aRamp * ksmps

  aSumL = 0
  aSumR = 0
  aWrL = aMono * (1 - kByp)

  aBufL delayr 16
  if (kByp == 0) then
    $PDLY_TAP(1'L)
    $PDLY_TAP(2'L)
    $PDLY_TAP(3'L)
    $PDLY_TAP(4'L)
    $PDLY_TAP(5'L)
    $PDLY_TAP(6'L)
    $PDLY_TAP(7'L)
    $PDLY_TAP(8'L)
  endif
  delayw aWrL

  aBufR delayr 16
  if (kByp == 0) then
    $PDLY_TAP(1'R)
    $PDLY_TAP(2'R)
    $PDLY_TAP(3'R)
    $PDLY_TAP(4'R)
    $PDLY_TAP(5'R)
    $PDLY_TAP(6'R)
    $PDLY_TAP(7'R)
    $PDLY_TAP(8'R)
  endif
  delayw aWrL

  if (kByp == 1) then
    gaMvdWetL = aInL
    gaMvdWetR = aInR
  else
    if (gkMvdFiltDepth > 0) then
      kFq = max(gkMvdFiltQ, 0.5)
      kCfL = limit(gkMvdFiltBase * pow(2, gkMvdFiltDepth * 6 * kEnvL), 20, sr * 0.45)
      kCfR = limit(gkMvdFiltBase * pow(2, gkMvdFiltDepth * 6 * kEnvR), 20, sr * 0.45)
      aSumL zdf_2pole aSumL, kCfL, kFq
      aSumR zdf_2pole aSumR, kCfR, kFq
    endif
    gaMvdWetL = aInL + aSumL
    gaMvdWetR = aInR + aSumR
  endif

  kIdx0 = (kIdx0 + ksmps) % $PDLY_RING   ; after every tap used kIdx0/aIdx
endin

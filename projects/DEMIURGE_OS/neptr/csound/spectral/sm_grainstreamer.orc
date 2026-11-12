#define SM_HAVE_GRAINSTR ##
; SM_GrainStreamer - source: SoundMagic Spectral "Grain Streamer" (Michael Norris, AU GStm), TIME-DOMAIN.
; His description: "'Captures' and repeats grains". Signature:  aOutL, aOutR SM_GrainStreamer aInL, aInR, ifx
; Params used: Freeze probability, Grain repetitions, Repetitions variation, Grain length at start/end
;   (+ start/end var), Zero non-frozen grains, Fade amount, Gain.
; Process: input is always written to a 65536-sample circular buffer per channel. The stream is cut into
;   slots. At each slot end, with probability Freeze% the last grain (Lcap samples) is CAPTURED and played
;   back R times (R = Reps +- Variation%, 1..200); the repeat length glides from the start length to the end
;   length across the repeats (each +- its own var, rolled once per freeze). Non-frozen slots pass the live
;   input (or silence if Zero non-frozen). Each repeat has a sin^2 edge fade of Fade% of half the grain (min
;   1.5 ms, so no clicks); the live signal is crossfaded against the first/last repeat edge. Both channels
;   share one schedule (stereo-coherent). Setksmps 1 inside, so boundaries are sample-accurate.
; Approximations: manual is one line, so start/end length meaning (glide across repeats), var as +- ms,
;   and the live-slot length (start length +- var) are my reading. Grains play unpitched (no resampling);
;   lengths clamp 1..200 ms; Gain "lin. gain" read as dB (+-20), smoothed 20 ms. Params that set lengths/
;   probability are sampled at slot boundaries (bounded work: one grain active at a time, O(1) per sample).
opcode SM_GrainStreamer, aa, aai
  aInL, aInR, ifx xin
  setksmps 1
  iSz = 65536
  iMask = iSz - 1
  iTL ftgen 0, 0, iSz, 2, 0
  iTR ftgen 0, 0, iSz, 2, 0
  kWp init 0
  kMode init 0
  kC init 0
  kL init 480
  kRep init 0
  kRepN init 1
  kLs init 480
  kLe init 480
  kSp init 0
  kLG init 1
  kGn init 1
  iMinE = 0.0015 * sr
  iMaxL = 0.2 * sr
  kInL downsamp aInL
  kInR downsamp aInR
  tablew kInL, kWp, iTL
  tablew kInR, kWp, iTR
  if kC >= kL then
    kC = 0
    kPs = limit(SM_P(ifx, $SMP_GRAINSTR_FREEZE_PROBABILITY), 0, 100) / 100
    kLsb = SM_P(ifx, $SMP_GRAINSTR_GRAIN_LENGTH_AT_START)
    kLsv = SM_P(ifx, $SMP_GRAINSTR_GRAIN_LENGTH_START_VAR)
    kLeb = SM_P(ifx, $SMP_GRAINSTR_GRAIN_LENGTH_AT_END)
    kLev = SM_P(ifx, $SMP_GRAINSTR_GRAIN_LENGTH_END_VAR)
    if kMode == 1 && kRep + 1 < kRepN then
      kRep += 1
      kL = max(2, int(kLs + (kLe - kLs) * kRep / max(kRepN - 1, 1)))
    else
      kRn random -1, 1
      kRu random -1, 1
      kLsm = limit((kLsb + kRn * kLsv) * 0.001 * sr, 2, iMaxL)
      kLem = limit((kLeb + kRu * kLev) * 0.001 * sr, 2, iMaxL)
      kRd random 0, 1
      if kRd < kPs then
        kRp = limit(SM_P(ifx, $SMP_GRAINSTR_GRAIN_REPETITIONS), 1, 100)
        kRv = limit(SM_P(ifx, $SMP_GRAINSTR_REPETITIONS_VARIATION), 0, 100) / 100
        kRx random -1, 1
        kRepN = int(limit(kRp * (1 + kRv * kRx) + 0.5, 1, 200))
        kMode = 1
        kRep = 0
        kLs = kLsm
        kLe = kLem
        kL = max(2, int(kLs))
        kLcap = max(kLs, kLe)
        kSp = kWp - int(kLcap) + iSz
      else
        kMode = 0
        kL = max(2, int(kLsm))
      endif
    endif
  endif
  kZero = (SM_P(ifx, $SMP_GRAINSTR_ZERO_NON_FROZEN_GRAINS) >= 0.5) ? 0 : 1
  kLGs portk kZero, 0.005
  kFd = limit(SM_P(ifx, $SMP_GRAINSTR_FADE_AMOUNT), 0, 100) / 100
  kGt = limit(SM_P(ifx, $SMP_GRAINSTR_GAIN), -20, 20)
  kGs portk kGt, 0.02
  kG = ampdb(kGs)
  if kMode == 1 then
    kE = limit(max(kFd * kL * 0.5, iMinE), 1, kL * 0.5)
    kW = limit(min(kC, kL - kC) / kE, 0, 1)
    kW = sin(1.5707963 * kW) ^ 2
    kIdx = (kSp + kC) & iMask
    kFl tab kIdx, iTL
    kFr tab kIdx, iTR
    kLw = 0
    if kRep == 0 && kC < kL * 0.5 then
      kLw = 1 - kW
    elseif kRep == kRepN - 1 && kC >= kL * 0.5 then
      kLw = 1 - kW
    endif
    kLw = kLw * kLGs
    kOL = (kFl * kW + kInL * kLw) * kG
    kOR = (kFr * kW + kInR * kLw) * kG
  else
    kOL = kInL * kLGs * kG
    kOR = kInR * kLGs * kG
  endif
  kC += 1
  kWp = (kWp + 1) & iMask
  aOL upsamp kOL
  aOR upsamp kOR
  xout aOL, aOR
endop

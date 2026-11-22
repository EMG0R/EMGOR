#define SM_HAVE_IDEEFIX ##
; SM_IdeeFixer - source: SoundMagic Spectral "Idee Fixer" (Michael Norris, AU IFix). TIME-DOMAIN.
; Signature:  aOutL, aOutR SM_IdeeFixer aInL, aInR, ifx      (stereo in/out, params k-rate from giSMP)
; His description: a large granular/brassage effect: capture input into a buffer, then churn out
;   granular versions of it. All 26 catalog params are live.
; Algorithm: block-based (caller's ksmps). Stereo ring buffer in two ftables (12.5 s, freed on release).
;   Sampling state machine (Sampling status 0 Off/reset, 1 Mute, 2 Ready for start, 3 Ready for end
;   = capturing, 4 Sample loaded). The UDO writes the automatic transitions (2->3->4) back to the
;   status slot in giSMP, and honours external changes (re-arm = set 2). While armed/capturing
;   the ring runs continuously, so Trigger start preroll > 0 includes audio from before the trigger
;   (no copying: the sample is just a [start,len) window of the ring). Start: Trigger Threshold (10 ms
;   peak follower of max(|L|,|R|) > threshold dB) or Sample Start Delay (s after arming). Stop: Max
;   length always; in Trigger Threshold stop mode, level below threshold for Dur below thresh ms.
;   Granulation: voice pool of 32 grains, each = linear-interp read of the sample at a rate (cents ->
;   rate change, "quick and dirty" like his), raised-sine fade in/out (Grain fade % of the grain, 100 =
;   whole grain shaped), amplitude dB with random reduction (Grain amp var), spacing as % of grain
;   output length (100 = back to back) with +-var %. Grain start var modes: 1 Random +-var % per
;   grain; 2/3 loop fwd/back, 4 ping-pong: start offset sweeps at (var% * rate * 10) % of the
;   buffer per second; 5 random walk with that same speed as step scale per second. Granulation mode
;   0 / 2 use start..end (end var applies); 1 uses Grain duration ms (end ignored).
; Approximations / choices (his details unpublished): Threshold window size is not exposed (fixed 10 ms);
;   preroll: positive = include audio before trigger (start) / extend after stop (stop), negative =
;   skip after the start trigger / trim from the end; both capped at 1 s. Mute keeps the sample and
;   silences grains (5 ms ramp); Off clears the sample. Grains playing during a re-capture are
;   silenced. Grain output duration = span / rate. Overlapping grains are normalised by 1/sqrt(overlap)
;   when overlap > 1 (his has no AGC; this keeps dense settings bounded), voices capped at 32 (extra
;   triggers are skipped), minimum grain interval 8 samples, minimum fade 48 samples (no clicks at
;   fade 0). Params are re-read every k-cycle; grain start times are sample-accurate, capture start/stop is block-quantised, and a grain never reads past the sample end (start is pulled back / span clamped to the sample length, no wrap inside a grain); they affect NEW grains, gains are smoothed.
;   Grain amplitude -60 and Dry mix -60 and Gain -60 dB are treated as zero. Output soft-limited (tanh).
; Cost: ~14 vector ops + 2 interpolated table reads per ACTIVE grain per k-cycle; bounded by the 32-voice cap.


; LCG uniform 0..1 (state in/out)
opcode SM_IFr, kk, k
  kS xin
  kS = (kS * 1664525 + 1013904223) % 4294967296
  xout kS / 4294967296, kS
endop

opcode SM_IdeeFixer, aa, aai
  aInL, aInR, ifx xin
  iL = ceil(12.5 * sr)
  iNV = 32
  iKS = ksmps
  iTL ftgen 0, 0, -iL, 2, 0
  iTR ftgen 0, 0, -iL, 2, 0
  ftfree iTL, 1
  ftfree iTR, 1
  kvB[] init iNV     ; absolute ring position of grain source start
  kvInc[] init iNV   ; rate
  kvTot[] init iNV   ; output length (samples)
  kvE[] init iNV     ; elapsed (negative = start offset inside the block)
  kvG[] init iNV     ; gain
  kvF[] init iNV     ; 1/fade samples
  kSt init 2
  kLastW init -1
  kWp init 0
  kFill init 0
  kTimer init 0
  kCap init 0
  kPre init 0
  kSkip init 0
  kStartRing init 0
  kLen init 0
  kHas init 0
  kActN init 0
  kRS init 0
  kStop init 0
  kExtra init 0
  kTrim init 0
  kBelow init 0
  kEnv init 0
  kNext init 0
  kOff init 0
  kDir init 1
  kWalk init 0
  kSeed init 12345
  kGate init 0
  kNorm init 1
  kNormT init 1
  kDryS init 0
  kGainS init 1
  kSlot init ifx * 32 + ($SMP_IDEEFIX_SAMPLING_STATUS % 32)
  kWv init 0
  kPm_sm init 1
  kPm_thr init 0.25
  kPm_pre init 0
  kPm_dly init 0
  kPm_stm init 1
  kPm_spre init 0
  kPm_below init 0
  kPm_max init 96000
  kPm_gm init 1
  kPm_dur init 3000
  kPm_gs init 0
  kPm_svm init 2
  kPm_svv init 0
  kPm_svr init 0.1
  kPm_ge init 100
  kPm_gev init 0
  kPm_sp init 10
  kPm_spv init 2
  kPm_amp init 1
  kPm_ampv init 0
  kPm_fade init 100
  kPm_dry init 0
  kPm_tr init 0
  kPm_trv init 0
  kPm_gain init 1
  aRamp = phasor(sr / iKS) * iKS

  ; ---- param refresh (every k-cycle) ----
  if 1 == 1 then
    kPm_sm = int(limit(SM_P(ifx, $SMP_IDEEFIX_SAMPLE_START_MODE), 0, 1) + 0.5)
    kPm_thr = ampdb(limit(SM_P(ifx, $SMP_IDEEFIX_TRIGGER_THRESHOLD), -60, 0))
    kPm_pre = limit(SM_P(ifx, $SMP_IDEEFIX_TRIGGER_START_PREROLL), -1000, 1000) * 0.001 * sr
    kPm_dly = limit(SM_P(ifx, $SMP_IDEEFIX_SAMPLE_START_DELAY), 0, 10) * sr
    kPm_stm = int(limit(SM_P(ifx, $SMP_IDEEFIX_SAMPLE_STOP_MODE), 0, 1) + 0.5)
    kPm_spre = limit(SM_P(ifx, $SMP_IDEEFIX_TRIGGER_STOP_PREROLL), -1000, 1000) * 0.001 * sr
    kPm_below = limit(SM_P(ifx, $SMP_IDEEFIX_DUR_BELOW_THRESH), 0, 500) * 0.001 * sr
    kPm_max = max(limit(SM_P(ifx, $SMP_IDEEFIX_MAX_SAMPLE_LENGTH), 0, 10), 0.05) * sr
    kPm_gm = int(limit(SM_P(ifx, $SMP_IDEEFIX_GRANULATION_MODE), 0, 2) + 0.5)
    kPm_dur = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_DURATION), 1, 1000) * 0.001 * sr
    kPm_gs = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_START), 0, 100)
    kPm_svm = int(limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_START_VAR_MODE), 0, 5) + 0.5)
    kPm_svv = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_START_VAR), 0, 100)
    kPm_svr = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_START_VAR_RATE), 0, 2)
    kPm_ge = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_END), 0, 100)
    kPm_gev = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_END_VAR), 0, 100)
    kPm_sp = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_SPACING), 1, 1000)
    kPm_spv = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_SPACING_VAR), 0, 100)
    kA = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_AMPLITUDE), -60, 0)
    kPm_amp = (kA <= -59.9) ? 0 : ampdb(kA)
    kPm_ampv = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_AMPLITUDE_VAR), 0, 60)
    kPm_fade = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_FADE), 0, 100)
    kD = limit(SM_P(ifx, $SMP_IDEEFIX_DRY_MIX), -60, 0)
    kPm_dry = (kD <= -59.9) ? 0 : ampdb(kD)
    kPm_tr = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_TRANSPOSITION), -2400, 2400)
    kPm_trv = limit(SM_P(ifx, $SMP_IDEEFIX_GRAIN_TRANSPOSITION_VAR), 0, 2400)
    kG = limit(SM_P(ifx, $SMP_IDEEFIX_GAIN), -60, 10)
    kPm_gain = (kG <= -59.9) ? 0 : ampdb(kG)
    ; status: external change?
    kPar = int(limit(SM_P(ifx, $SMP_IDEEFIX_SAMPLING_STATUS), 0, 4) + 0.5)
    if kPar != kLastW then
      kLastW = kPar
      if kPar == 0 then
        kSt = 0
        kHas = 0
        kFill = 0
      elseif kPar == 1 then
        kSt = 1
      elseif kPar == 2 then
        kSt = 2
        kTimer = 0
        kFill = 0
      elseif kPar == 3 then
        if kSt != 3 then
          kSt = 3
          kPre = 0
          kSkip = 0
          kStartRing = kWp
          kCap = 0
          kStop = 0
          kBelow = 0
        endif
      else
        if kHas == 1 then
          kSt = 4
        else
          kLastW = kSt
          kWv = kSt
          tablew kWv, kSlot, giSMP
        endif
      endif
    endif
  endif


  ; ---- sampling state machine (block level) ----
  aAbs = max(abs(aInL), abs(aInR))
  kPk max_k aAbs, 1, 1
  kEnv = max(kPk, kEnv * 0.9)
  if kSt == 2 then
    kTimer += iKS
    kGo = (kPm_sm == 1) ? ((kTimer >= kPm_dly) ? 1 : 0) : ((kEnv > kPm_thr) ? 1 : 0)
    if kGo == 1 then
      kPre = limit(max(kPm_pre, 0), 0, 1 * sr)
      kPre = min(kPre, kFill)
      kSkip = max(-kPm_pre, 0)
      kStartRing = (kWp - kPre + iL) % iL
      kCap = kPre
      kStop = 0
      kBelow = 0
      kSt = 3
      kLastW = 3
      kWv = 3
      tablew kWv, kSlot, giSMP
    endif
  elseif kSt == 3 then
    kEndNow = 0
    if kStop == 1 then
      kExtra -= iKS
      if kExtra <= 0 then
        kEndNow = 1
      endif
    else
      kStopNow = 0
      if kCap >= kPm_max + kPre then
        kStopNow = 1
      endif
      if kPm_stm == 0 then
        if kEnv < kPm_thr then
          kBelow += iKS
        else
          kBelow = 0
        endif
        if kBelow > kPm_below && kCap > kPre + kSkip + 64 then
          kStopNow = 1
        endif
      endif
      if kStopNow == 1 then
        kStop = 1
        kTrim = max(-kPm_spre, 0)
        kExtra = limit(max(kPm_spre, 0), 0, 1 * sr)
        kExtra = min(kExtra, iL - kCap - 2 * iKS)
        if kExtra <= 0 then
          kEndNow = 1
        endif
      endif
    endif
    if kEndNow == 1 then
      kLen = min(kCap - kSkip - kTrim, iL - 2)
      if kLen < 0.02 * sr then
        kSt = 2
        kTimer = 0
        kLastW = 2
        kWv = 2
        tablew kWv, kSlot, giSMP
      else
        kRS = (kStartRing + kSkip) % iL
        kHas = 1
        kSt = 4
        kLastW = 4
        kWv = 4
        tablew kWv, kSlot, giSMP
        kNext = 0
        kOff = 0
        kWalk = 0
      endif
    endif
  endif
  if kSt == 2 || kSt == 3 then
    aWI = kWp + aRamp
    tablew aInL, aWI, iTL, 0, 0, 1
    tablew aInR, aWI, iTR, 0, 0, 1
    kWp = (kWp + iKS) % iL
    kFill = min(kFill + iKS, iL)
    if kSt == 3 then
      kCap += iKS
    endif
  endif

  ; ---- gate / smoothing ----
  kGate += (((kSt == 4) ? 1 : 0) - kGate) * 0.15
  kNorm += (kNormT - kNorm) * 0.05
  kDryS += (kPm_dry - kDryS) * 0.1
  kGainS += (kPm_gain - kGainS) * 0.1

  ; ---- grain scheduler (block level, sample-accurate offsets) ----
  if kSt == 4 && kHas == 1 then
    kSpeed = kPm_svv * kPm_svr * 10 * iKS / sr   ; % of buffer per block
    if kPm_svm == 2 then
      kOff += kSpeed
      if kOff >= 100 then
        kOff -= 100
      endif
    elseif kPm_svm == 3 then
      kOff -= kSpeed
      if kOff < 0 then
        kOff += 100
      endif
    elseif kPm_svm == 4 then
      kOff += kSpeed * kDir
      if kOff >= 100 then
        kOff = 100
        kDir = -1
      elseif kOff <= 0 then
        kOff = 0
        kDir = 1
      endif
    endif
    while kNext < iKS do
      kU, kSeed SM_IFr kSeed
      kU1 = kU * 2 - 1
      kU, kSeed SM_IFr kSeed
      kU2 = kU * 2 - 1
      kU, kSeed SM_IFr kSeed
      kU3 = kU * 2 - 1
      kU, kSeed SM_IFr kSeed
      kU4 = kU
      kU, kSeed SM_IFr kSeed
      kU5 = kU * 2 - 1
      kOffS = kOff
      if kPm_svm == 1 then
        kOffS = kU1 * kPm_svv
      elseif kPm_svm == 5 then
        kWalk += kU1 * kPm_svv * kPm_svr * 0.1
        kWalk = kWalk % 100
        kOffS = kWalk
      endif
      kSP = (kPm_gs + kOffS) % 100
      if kSP < 0 then
        kSP += 100
      endif
      kRate = pow(2, limit(kPm_tr + kU2 * kPm_trv, -4800, 4800) / 1200)
      if kPm_gm == 1 then
        kSpan = kPm_dur * kRate
      else
        kEP = kPm_ge + kU3 * kPm_gev
        kD2 = kEP - kSP
        if kD2 <= 0 then
          kD2 += 100
        endif
        kSpan = kD2 * 0.01 * kLen
      endif
      ; kSpan = source samples read; clamp so the grain stays inside the sample
      kSpan = limit(kSpan, 0.002 * sr * kRate, kLen - 2)
      kTot = max(kSpan / kRate, 48)
      kGs = min(kSP * 0.01 * kLen, kLen - 2 - kSpan)
      kInt = max(kTot * kPm_sp * 0.01 * (1 + kU5 * kPm_spv * 0.01), 8)
      kNormT = 1 / sqrt(max(1, kTot / kInt))
      if kActN < iNV && kPm_amp > 0 then
        kV = kActN
        kActN += 1
        kvB[kV] = kRS + kGs
        kvInc[kV] = kRate
        kvTot[kV] = kTot
        kvE[kV] = -int(kNext)
        kvG[kV] = kPm_amp * ampdb(-kU4 * kPm_ampv)
        kvF[kV] = 1 / limit(kPm_fade * 0.005 * kTot, 48, kTot * 0.5)
      endif
      kNext += kInt
    od
    kNext -= iKS
  endif

  ; ---- render voices ----
  aWL = a(0)
  aWR = a(0)
  if kLen > 0 && (kSt == 4 || kGate >= 0.0005) then
    kj = 0
    while kj < kActN do
      kE0 = kvE[kj]
      aE = kE0 + aRamp
      aEn = limit(min(aE, kvTot[kj] - aE) * kvF[kj], 0, 1)
      aEn = aEn * aEn * (3 - 2 * aEn)
      aIx = kvB[kj] + kvInc[kj] * limit(aE, 0, 1000000)
      aGn = aEn * kvG[kj]
      aTL tablei aIx, iTL, 0, 0, 1
      aTR tablei aIx, iTR, 0, 0, 1
      aWL = aWL + aGn * aTL
      aWR = aWR + aGn * aTR
      kE1 = kE0 + iKS
      if kE1 >= kvTot[kj] then
        kActN -= 1
        kL = kActN
        kvB[kj] = kvB[kL]
        kvInc[kj] = kvInc[kL]
        kvTot[kj] = kvTot[kL]
        kvE[kj] = kvE[kL]
        kvG[kj] = kvG[kL]
        kvF[kj] = kvF[kL]
      else
        kvE[kj] = kE1
        kj += 1
      endif
    od
  endif
  if kSt != 4 && kGate < 0.0005 then
    kActN = 0
  endif
  aMix = (aWL * interp(kNorm * kGate)) + aInL * interp(kDryS)
  aMixR = (aWR * interp(kNorm * kGate)) + aInR * interp(kDryS)
  aGnk = interp(kGainS)
  aOutL = tanh(aMix * aGnk)
  aOutR = tanh(aMixR * aGnk)
  xout aOutL, aOutR
endop

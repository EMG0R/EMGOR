#define SM_HAVE_SUPERFB ##
; SM_SuperFilterbank - source: SoundMagic Spectral "Super Filterbank" (Michael Norris, AU SuFB). TIME-DOMAIN.
; Signature: aOutL, aOutR SM_SuperFilterbank aInL, aInR, ifx   (params read k-rate via SM_P).
; His description: a bank of bandpass filters, selectable type (comb, biquad, butterworth...).
; Process: N filters (1..120) at f_i = F0 * 2^(oct + deg/12), degrees from the chosen Scale cycled upward
;   (Octaves, Fifths, triads, 7-note modes, whole-tone, octatonic, hexatonic, acoustic). Bandwidth in cents
;   -> Hz per filter (bw = f*(2^(c/1200)-1), floor f*2e-5 for stability). Types: 0 Comb (streson, feedback
;   g=exp(-pi*bw/f)), 1 2nd-order bandpass (reson), 2 Butterworth (butterbp), 3 Biquad (RBJ bandpass, 0 dB
;   peak), 4 Constant gain (reson RMS-normalised, peak gain capped at +9.5 dB), 5 Goertzel (leaky resonator, zero at DC, (1-r) scaled).
;   Interpolation time glides F0 (log domain, portk; ~97 % after the stated time). Wet/Dry linear, Gain dB.
; Approximations: Constant gain and Goertzel are my readings of the names (no manual text). Wet sum is
;   scaled 1/sqrt(N). Filters above 0.45*sr are muted; comb f clamped >= 20 Hz. Number of filters fades
;   per-filter (10 ms), per-filter freq/bw smoothed (10 ms), a Filter type change ducks the wet 25 ms
;   down, switches, 25 ms up. Cost ~ 2 filters per band per sample; bands above the (held) filter count are not evaluated at all.
giSFBsc ftgen 0, 0, 128, -2, \
  1,0,0,0,0,0,0,0,0,  2,0,7,0,0,0,0,0,0,  3,0,4,7,0,0,0,0,0,  3,0,3,7,0,0,0,0,0, \
  7,0,2,4,5,7,9,11,0, 7,0,2,3,5,7,9,10,0, 7,0,2,4,6,7,9,11,0, 7,0,2,4,5,7,9,10,0, \
  7,0,2,3,5,7,8,10,0, 6,0,2,4,6,8,10,0,0, 8,0,2,3,5,6,8,9,11, 6,0,2,4,7,9,11,0,0, \
  7,0,2,4,6,7,9,10,0

; one band (both channels) + recursion over the remaining bands; state is per recursion level
opcode SM_SFBBand, aa, aaikkkkkkk
  aL, aR, iIdx, kF0, kBwC, kSc, kN, kTyp, kMax, kSt xin
  iMax = 120
  kRow = int(limit(kSc, 0, 12)) * 9
  kCnt = tab:k(kRow, giSFBsc)
  kOct = int(iIdx / kCnt)
  kDeg = tab:k(kRow + 1 + (iIdx % kCnt), giSFBsc)
  kFr = limit(kF0 * pow(2, kOct + kDeg / 12), 0, 1e6)
  kOk = (iIdx < kN && kFr < 0.45 * sr) ? 1 : 0
  ; own half-time smoothers (same maths as portk): a band that was off (not evaluated, or faded
  ; out) jumps straight to its target, so a band entering the active range does not chirp in
  ; from a stale state
  iC2A = pow(0.5, (ksmps / sr) / 0.0015)
  iC2F = pow(0.5, (ksmps / sr) / 0.002)
  kA init 0
  kF init 200
  kBw init 10
  if kA <= 1e-5 && kOk == 0 then
    kF = limit(kFr, 10, 0.45 * sr)
    kBw = kBwC
    kA = 0
  else
    if kA <= 1e-5 then
      kF = limit(kFr, 10, 0.45 * sr)
      kBw = kBwC
    else
      kF = iC2F * kF + (1 - iC2F) * limit(kFr, 10, 0.45 * sr)
      kBw = iC2F * kBw + (1 - iC2F) * kBwC
    endif
    kA = iC2A * kA + (1 - iC2A) * kOk
  endif
  kFc = (kTyp == 0) ? max(kF, 20) : kF
  kBhz = max(kFc * (pow(2, kBw / 1200) - 1), kFc * 2e-5)
  aoL = 0
  aoR = 0
  if kA > 1e-5 || kOk == 1 then
    if kTyp == 0 then
      kG = limit(exp(-3.14159265 * kBhz / kFc), 0, 0.999)
      aoL = streson(aL, kFc, kG) * (1 - kG)
      if kSt == 1 then
        aoR = streson(aR, kFc, kG) * (1 - kG)
      endif
    elseif kTyp == 1 then
      aoL = reson(aL, kFc, kBhz, 1)
      if kSt == 1 then
        aoR = reson(aR, kFc, kBhz, 1)
      endif
    elseif kTyp == 2 then
      aoL = butterbp(aL, kFc, kBhz)
      if kSt == 1 then
        aoR = butterbp(aR, kFc, kBhz)
      endif
    elseif kTyp == 3 then
      kW = 6.28318531 * kFc / sr
      kAl = sin(kW) / (2 * (kFc / kBhz))
      aoL = biquad(aL, kAl, 0, -kAl, 1 + kAl, -2 * cos(kW), 1 - kAl)
      if kSt == 1 then
        aoR = biquad(aR, kAl, 0, -kAl, 1 + kAl, -2 * cos(kW), 1 - kAl)
      endif
    elseif kTyp == 4 then
      kS4 = min(1, 5.236 * sqrt(kBhz / sr))
      aoL = reson(aL, kFc, kBhz, 2) * kS4
      if kSt == 1 then
        aoR = reson(aR, kFc, kBhz, 2) * kS4
      endif
    else
      kW = 6.28318531 * kFc / sr
      kR = limit(exp(-3.14159265 * kBhz / sr), 0, 0.999999)
      kC = kR * cos(kW)
      aoL = biquad(aL, 1 - kR, -kC * (1 - kR), 0, 1, -2 * kC, kR * kR)
      if kSt == 1 then
        aoR = biquad(aR, 1 - kR, -kC * (1 - kR), 0, 1, -2 * kC, kR * kR)
      endif
    endif
  endif
  if kSt == 0 then
    aoR = aoL
  endif
  aoL = aoL * kA
  aoR = aoR * kA
  ; tail recursion only while a band above this one is still wanted or fading (kMax = bands to run)
  if iIdx + 1 < iMax then
    if kMax > iIdx + 1 then
      aRL, aRR SM_SFBBand aL, aR, iIdx + 1, kF0, kBwC, kSc, kN, kTyp, kMax, kSt
      aoL = aoL + aRL
      aoR = aoR + aRR
    endif
  endif
  xout aoL, aoR
endop

opcode SM_SuperFilterbank, aa, aai
  aInL, aInR, ifx xin
  kF0 = max(SM_P(ifx, $SMP_SUPERFB_FUNDAMENTAL_FREQUENCY), 10)
  kT = limit(SM_P(ifx, $SMP_SUPERFB_INTERPOLATION_TIME), 0, 60)
  kSc = limit(SM_P(ifx, $SMP_SUPERFB_SCALE), 0, 12)
  kN = int(limit(SM_P(ifx, $SMP_SUPERFB_NUMBER_OF_FILTERS) + 0.5, 1, 120))
  kBw = limit(SM_P(ifx, $SMP_SUPERFB_BANDWIDTH), 0.01, 100)
  kTypT = int(limit(SM_P(ifx, $SMP_SUPERFB_FILTER_TYPE) + 0.5, 0, 5))
  kWet = limit(SM_P(ifx, $SMP_SUPERFB_WET_DRY_MIX), 0, 100) / 100
  kGdb = limit(SM_P(ifx, $SMP_SUPERFB_GAIN), -30, 30)
  ; glide F0 in log domain
  kLg = portk(log2(kF0), max(kT / 5, 0.002), log2(200))
  kFg = pow(2, kLg)
  ; type change: duck, switch, restore
  kTyp init 1
  kDuckT init 1
  kHold init 0
  if kTypT != kTyp && kHold == 0 then
    kDuckT = 0
    kHold = 1
  endif
  kDuck = portk(kDuckT, 0.006, 1)
  if kHold == 1 && kDuck < 0.02 then
    kTyp = kTypT
    kDuckT = 1
    kHold = 0
  endif
  ; bands to evaluate: kN, held at the old (higher) count for 30 ms after a decrease so the
  ; per-band fade-out (1.5 ms tc) finishes; bands above kHi cost nothing
  ; (bands above 0.45*sr are muted anyway, so the count is also capped at the first such band;
  ;  band frequency rises monotonically with the band index)
  kRow0 = int(kSc) * 9
  kCnt0 = tab:k(kRow0, giSFBsc)
  kNy = 0
  while kNy < kN do
    if kFg * pow(2, int(kNy / kCnt0) + tab:k(kRow0 + 1 + (kNy % kCnt0), giSFBsc) / 12) >= 0.45 * sr then
      kgoto nyDone
    endif
    kNy += 1
  od
nyDone:
  kNeed = max(kNy, 1)
  kHi init 1
  kHtm init 0
  kNprv init 1
  if kNeed >= kHi then
    kHi = kNeed
    kHtm = 0
  else
    if kNeed != kNprv then
      kHtm = 0
    endif
    kHtm += ksmps / sr
    if kHtm > 0.03 then
      kHi = kNeed
    endif
  endif
  kNprv = kNeed
    ; bit-identical L/R (mono source on both channels) -> run the left bank only, copy to the right
  kSt = max_k(aInL - aInR, 1, 1) > 0 ? 1 : 0
  aBL, aBR SM_SFBBand aInL, aInR, 0, kFg, kBw, kSc, kN, kTyp, kHi, kSt
  kWg = kWet * kDuck * ampdb(kGdb) / sqrt(kN)
  iW0 = limit(tab_i(ifx * 32 + ($SMP_SUPERFB_WET_DRY_MIX % 32), giSMP), 0, 100) / 100
  kWs = portk(kWg, 0.005, 0)
  kDs = portk(1 - kWet, 0.005, 1 - iW0)
  aOL = limit(aInL * kDs + aBL * kWs, -4, 4)
  aOR = limit(aInR * kDs + aBR * kWs, -4, 4)
  xout aOL, aOR
endop

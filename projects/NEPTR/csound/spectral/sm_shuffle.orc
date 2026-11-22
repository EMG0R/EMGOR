#define SM_HAVE_SHUFFLE ##
; SM_Shuffle - source: SoundMagic Spectral "Spectral Shuffle" (Michael Norris, AU SSfl).
; His description: randomly shuffles blocks of bins around within the spectrum.
; Params used: Shuffle factor (% of groups moved per frame), Shuffle range (% of bins a block
;   may travel), Bins per group (bins kept together).
; Process: each full group is, with probability factor, swapped with a random partner group
;   within +-range. Swaps compose into a permutation, so total magnitude energy is preserved
;   exactly. Frequencies travel with their bins, offset by the distance moved (partials really move).
; Deviations: catalog lists "Bins per group" with a % unit; the manual says bins, so it is
;   treated as a bin count (1..50). Tail bins that do not fill a group are left in place.
opcode SM_Shuffle, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  if kNew == 1 then
    kP = limit(tab:k($SMP_SHUFFLE_SHUFFLE_FACTOR, giSMP), 0, 100) / 100
    kG = max(1, int(tab:k($SMP_SHUFFLE_BINS_PER_GROUP, giSMP)))
    kNg = int(iNb / kG)
    kR = max(1, round(tab:k($SMP_SHUFFLE_SHUFFLE_RANGE, giSMP) / 100 * iNb / kG))
    if kP > 0 && kNg > 1 then
      kg = 0
      while kg < kNg do
        if random:k(0, 1) < kP then
          kq = kg + int(floor(random:k(-kR, kR + 1)))
          kq = limit(kq, 0, kNg - 1)
          if kq != kg then
            kD = (kq - kg) * kG
            kj = 0
            while kj < kG do
              ka = kg * kG + kj
              kb = kq * kG + kj
              kTm = kMag[ka]
              kTf = kFrq[ka]
              kMag[ka] = kMag[kb]
              kFrq[ka] = kFrq[kb] - kD * iBinHz
              kMag[kb] = kTm
              kFrq[kb] = kTf + kD * iBinHz
              kj += 1
            od
          endif
        endif
        kg += 1
      od
    endif
  endif
  xout kMag, kFrq
endop

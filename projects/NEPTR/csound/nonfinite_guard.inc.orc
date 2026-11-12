; nonfinite_guard.inc.orc — NaN/inf/runaway safety net (2026-11-02)
; Symptom it exists for: guitar + loops go silent and STAY silent while the
; vocal strip (own bus, direct outs 5/6) keeps playing. A single NaN/inf in a
; recursive state or a loop buffer poisons the main mix permanently.
;
; NF_Chk  : 1 if the block is non-finite or absurdly large, else 0.
;   One pass: downsamp averages the block's squared samples (NaN/inf propagate
;   through the mean). Test is written (x < lim ? 0 : 1) so NaN -> 1.
;   Threshold mean-square 1e4 = rms 100 (+40 dBFS); legit audio is <= ~2.
; NF_Log  : one printks line per trip, at most once per 2 s per call site
;   (printks write()s blocking on the RT thread, so it must stay rare).
; LP_Heal : zero any non-finite / >1e3 sample in a window of a loop buffer
;   around the read head. Failure path only; heals the poisoned stretch
;   without wiping the rest of the recording.
opcode NF_Chk, k, aa
  aL, aR xin
  aSq = aL * aL + aR * aR
  kMs downsamp aSq, ksmps
  kBad = (kMs < 1e4 ? 0 : 1)
  xout kBad
endop

opcode NF_Chk1, k, a
  aL xin
  aSq = aL * aL
  kMs downsamp aSq, ksmps
  kBad = (kMs < 1e4 ? 0 : 1)
  xout kBad
endop

opcode NF_Log, 0, Sk
  Stag, kTrip xin
  kLast init -100
  kNow timeinsts
  if (kTrip == 1 && kNow - kLast > 2) then
    printks "NONFINITE guard tripped: %s\n", 0, Stag
    kLast = kNow
  endif
endop

opcode LP_Heal, 0, kkkii
  kPos, kSpan, kLen, iFnL, iFnR xin
  kI = -kSpan
  kN = 0
  while (kI <= kSpan && kN < 4096) do
    kX = int(kPos + kI)
    kX = kX - kLen * int(kX / kLen)
    if (kX < 0) then
      kX = kX + kLen
    endif
    kV1 tab kX, iFnL
    kV2 tab kX, iFnR
    kB1 = (abs(kV1) < 1e3 ? 0 : 1)
    kB2 = (abs(kV2) < 1e3 ? 0 : 1)
    if (kB1 == 1) then
      tabw 0, kX, iFnL
    endif
    if (kB2 == 1) then
      tabw 0, kX, iFnR
    endif
    kI += 1
    kN += 1
  od
endop

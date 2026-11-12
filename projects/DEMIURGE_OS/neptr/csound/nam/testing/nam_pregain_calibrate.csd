<CsoundSynthesizer>
<CsOptions>
</CsOptions>
<CsInstruments>

; ============================================================
; NAM pregain calibration — offline, autonomous.
;
; Feeds a deterministic guitar-band multi-tone (peak ~ -6.9 dBFS,
; the interface "green just before yellow" reference) through
; NAMProcess with a trial PREGAIN_DB, and measures exact RMS of
; input and output over a late window (the model loads on a
; wall-clock background thread while the render runs faster than
; realtime, so the window sits far from t=0 and a passthrough
; detector guards against measuring an unloaded model).
;
; Driven by testing/calibrate_pregains.sh via --omacro:
;   PREGAIN_DB  trial input gain in dB
;   PIDX        profile index (sorted order in SDIR)
; Prints:  RMS_IN_DB=  RMS_OUT_DB=  DIFF_DB=
; ============================================================

sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

#ifndef PREGAIN_DB
#define PREGAIN_DB #0.0#
#endif
#ifndef PIDX
#define PIDX #0#
#endif

giSine ftgen 0, 0, 65536, 10, 1

gkSumIn   init 0
gkSumOut  init 0
gkSumDiff init 0
gkN       init 0

gSDir = "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/NEPTR phase4/csound/nam/testing/measure_profiles"

instr 1
  iKsmps  = ksmps
  iPre    = ampdb($PREGAIN_DB)
  kIdx    init $PIDX
  ktime   timeinsts

  a1 oscili 0.28, 110, giSine
  a2 oscili 0.14, 220, giSine
  a3 oscili 0.03, 440, giSine
  aIn = a1 + a2 + a3

  aPreSig = aIn * iPre
  aOutL, aOutR NAMProcess aPreSig, aPreSig, kIdx, gSDir

  if (ktime >= 20.0 && ktime < 29.0) then
    kk = 0
  acc:
    kvi vaget kk, aIn
    kvo vaget kk, aOutL
    kvp vaget kk, aPreSig
    gkSumIn   = gkSumIn   + kvi*kvi
    gkSumOut  = gkSumOut  + kvo*kvo
    gkSumDiff = gkSumDiff + (kvo-kvp)*(kvo-kvp)
    kk = kk + 1
    if (kk < iKsmps) kgoto acc
    gkN = gkN + iKsmps
  endif
endin

instr 2
  iSumIn   = i(gkSumIn)
  iSumOut  = i(gkSumOut)
  iSumDiff = i(gkSumDiff)
  iN       = i(gkN)

  if (iN < 1 || iSumIn < 1e-12) igoto bad

  iInDb  = 20 * log10(sqrt(iSumIn  / iN) + 1e-12)
  iOutDb = 20 * log10(sqrt(iSumOut / iN) + 1e-12)
  ; diff energy relative to the pre-model signal: passthrough => very low
  iDiffDb = 10 * log10((iSumDiff + 1e-15) / (iSumIn + 1e-15))

  prints "RMS_IN_DB=%.2f\n",  iInDb
  prints "RMS_OUT_DB=%.2f\n", iOutDb
  prints "DIFF_DB=%.2f\n",    iDiffDb
  igoto done
bad:
  prints "RMS_IN_DB=NAN\n"
  prints "RMS_OUT_DB=NAN\n"
  prints "DIFF_DB=NAN\n"
done:
endin

</CsInstruments>
<CsScore>
i 1 0    30
i 2 29.5 0.1
e
</CsScore>
</CsoundSynthesizer>

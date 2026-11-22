<CsoundSynthesizer>
<CsOptions>
-n -m0 -d
</CsOptions>
<CsInstruments>
; Channel-isolation regression (the bug 5.2 / per-channel-instance guard).
;
; Renders the SAME left-channel signal twice through NAMProcess:
;   run RSIG=0 — right channel carries the same signal (correlated, mono-like)
;   run RSIG=1 — right channel carries a decorrelated multi-tone
; With one model instance per channel the left output must be identical in
; both runs (L depends only on L). The historical shared-instance build
; convolves each channel against the other channel's ring-buffer history,
; so the left output changes when R changes — measured at -4.7 dB relative
; crosstalk on the pre-fix build vs bit-exact zero on the fixed build.
;
; Macros (passed by test-nam.sh):
;   NAMLIBDIR  profile library dir     OUTDIR  where raw captures go
;   RSIG       0|1 as above
;
; Renders 60 s so the wall-clock background model load provably completes
; long before the analysis window (last 5 s) — offline render outruns the
; loader thread (same caveat as nam_signal_test.csd).
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

instr 1
  a1 poscil 0.20, 220
  a2 poscil 0.15, 330
  a3 poscil 0.10, 554
  aL = a1 + a2 + a3

  aB1 poscil 0.20, 277
  aB2 poscil 0.15, 415
  aB3 poscil 0.10, 622
  aRd = aB1 + aB2 + aB3

  irsig init $RSIG
  if (irsig == 0) then
    aR = aL
  else
    aR = aRd
  endif

  kidx init 0
  aoL, aoR NAMProcess aL, aR, kidx, "$NAMLIBDIR"

  ; raw float32, no header; macro expansion consumes the trailing dots
  fout "$OUTDIR./iso_dry_r$RSIG.", 0, aL
  fout "$OUTDIR./iso_wet_r$RSIG.", 0, aoL
endin

</CsInstruments>
<CsScore>
i1 0 60
e
</CsScore>
</CsoundSynthesizer>

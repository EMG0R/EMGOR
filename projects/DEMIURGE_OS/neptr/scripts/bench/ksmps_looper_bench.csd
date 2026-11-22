<CsoundSynthesizer>
<CsOptions>
-W -f -m0
</CsOptions>
<CsInstruments>
; LOOPER block-size-independence bench.
;   csound ksmps_looper_bench.csd -o out.wav --omacro:KSMPS=64
;   csound ksmps_looper_bench.csd -o out.wav --omacro:KSMPS=32
;
; Drives a real UNCLOCKED gesture pass on channel 1 so every code path that
; the block size could touch actually executes:
;   record -> click-off (state 6 trim scan over a silent tail) -> play
;   -> overdub (tablew into the buffer) -> play -> varispeed (tablei at 2x)
;
; All transitions are scheduled off a SAMPLE counter whose thresholds are
; multiples of 64, so ksmps 64 and ksmps 32 reach every transition on the
; SAME sample — any output difference is the looper's, not the driver's.
;
; This bench touches NO state-persist path: it includes looper.orc alone and
; never loads or writes ~/demiurge state.
#ifndef KSMPS
#define KSMPS #64#
#endif
sr = 48000
ksmps = $KSMPS
nchnls = 2
0dbfs = 1

; --- looper tables (same sizes/contents as globals.orc) ---
giLpBuf1L ftgen 0, 0, -2304000, -2, 0
giLpBuf1R ftgen 0, 0, -2304000, -2, 0
giLpBuf2L ftgen 0, 0, -2304000, -2, 0
giLpBuf2R ftgen 0, 0, -2304000, -2, 0
giLpBuf3L ftgen 0, 0, -2304000, -2, 0
giLpBuf3R ftgen 0, 0, -2304000, -2, 0
giLpBuf4L ftgen 0, 0, -2304000, -2, 0
giLpBuf4R ftgen 0, 0, -2304000, -2, 0
giLpBuf5L ftgen 0, 0, -2304000, -2, 0
giLpBuf5R ftgen 0, 0, -2304000, -2, 0
giLpSpeeds ftgen 0, 0, 8, -2, -2, -1, -0.5, -0.25, 0.25, 0.5, 1, 2

gaLpInL  init 0
gaLpInR  init 0
gaLpOutL init 0
gaLpOutR init 0
gaLpSampOff init 0

gkLpClock     init 0
gkLpMasterLen init 0
gkLpGridAvail init 0
gkLpBeatSamples init 0
gkGLITCH_bpm         init 120
gkGLITCH_extBpmSeen  init 0

gkLpState1 init 0
gkLpState2 init 0
gkLpState3 init 0
gkLpState4 init 0
gkLpState5 init 0
gkLpLen1 init 0
gkLpLen2 init 0
gkLpLen3 init 0
gkLpLen4 init 0
gkLpLen5 init 0
gkLpWrPos1 init 0
gkLpWrPos2 init 0
gkLpWrPos3 init 0
gkLpWrPos4 init 0
gkLpWrPos5 init 0
gkLpRdPos1 init 0
gkLpRdPos2 init 0
gkLpRdPos3 init 0
gkLpRdPos4 init 0
gkLpRdPos5 init 0
gkLpSpIdx1 init 6
gkLpSpIdx2 init 6
gkLpSpIdx3 init 6
gkLpSpIdx4 init 6
gkLpSpIdx5 init 6
gkLpAnchor1 init 0
gkLpAnchor2 init 0
gkLpAnchor3 init 0
gkLpAnchor4 init 0
gkLpAnchor5 init 0
gkLpTarget1 init 0
gkLpTarget2 init 0
gkLpTarget3 init 0
gkLpTarget4 init 0
gkLpTarget5 init 0
gkLpTrimPos1 init 0
gkLpTrimPos2 init 0
gkLpTrimPos3 init 0
gkLpTrimPos4 init 0
gkLpTrimPos5 init 0

#include "../../csound/effects/looper.orc"

; --------------------------------------------------------------
; Driver — sample-accurate gesture sequencer (thresholds are all
; multiples of 64 so ksmps 32 and 64 land on the same sample).
; --------------------------------------------------------------
gkTrimClosedAt init -1

instr 1
  kN init 0                       ; samples elapsed at the TOP of this block

  ; ---- source material ----
  ; take A: 317.37 Hz, 1.0 s of tone then 0.5 s of pure silence (the tail the
  ; trim scan has to find and remove).
  kToneOn  = (kN >= 24000 && kN < 72000 ? 1 : 0)
  aA       poscil 0.3, 317.37
  ; take B (overdub): 733.41 Hz
  aB       poscil 0.25, 733.41
  kOdOn    = (kN >= 288000 && kN < 336000 ? 1 : 0)

  aIn = aA * kToneOn + aB * kOdOn
  gaLpInL = aIn
  gaLpInR = aIn

  ; ---- gesture timeline (samples) ----
  ;      24000 (0.50 s)  press: idle -> RECORD
  ;      96000 (2.00 s)  click-off, UNCLOCKED -> state 6 (trim scan)
  ;     288000 (6.00 s)  play -> OVERDUB
  ;     336000 (7.00 s)  overdub -> play
  ;     384000 (8.00 s)  varispeed to 2.0x (speed index 7)
  if (kN == 24000) then
    gkLpState1  = 1
    gkLpWrPos1  = 0
  endif
  if (kN == 96000) then
    ; UNCLOCKED click-off, exactly as LP_QCLOSE does it
    gkLpTrimPos1 = gkLpWrPos1
    gkLpTarget1  = gkLpWrPos1
    gkLpState1   = 6
  endif
  if (kN == 288000 && gkLpState1 == 2) then
    gkLpState1 = 3
  endif
  if (kN == 336000 && gkLpState1 == 3) then
    gkLpState1 = 2
  endif
  if (kN == 384000) then
    gkLpSpIdx1 = 7
  endif

  ; log the exact sample at which the trim scan finished, and the length it
  ; decided — the numbers that must match across block sizes.
  if (gkTrimClosedAt < 0 && gkLpState1 == 2) then
    gkTrimClosedAt = kN
    printks "TRIM_CLOSED_AT_SAMPLE %d  LEN %d\n", 0, kN, gkLpLen1
  endif

  kN += ksmps
endin

; sums the looper bus to the file (instr number > 214 so it runs last)
instr 220
  outs gaLpOutL, gaLpOutR
  clear gaLpOutL, gaLpOutR
endin

</CsInstruments>
<CsScore>
i1   0 10
i209 0 10
i210 0 10
i211 0 10
i212 0 10
i213 0 10
i214 0 10
i220 0 10
</CsScore>
</CsoundSynthesizer>

<CsoundSynthesizer>
<CsOptions>
; Ignored under demiurge-run-csound (it passes -+ignore_csopts=1 and supplies
; -odac/-r/-b/-B/-M itself). Only here so the file also opens standalone.
-odac -d
</CsOptions>
<CsInstruments>
; hello nonlinear daylight -- minimal Csound example for Demiurge.
; Three 0..1 params through the pool hook (resources/params/csound/demiurge_params.udo):
;   /nld-csound/density  /nld-csound/tone  /nld-csound/length
; The pool writes `/p <path:s> <val:f>` to DEMIURGE_PARAM_PORT (manifest
; params/nld-csound.json, port 9000); the file answers with /pout on every change.
; The launcher passes --omacro:DEMIURGE_PARAM_PORT/..POOL_PORT and --env:INCDIR=$DEMIURGE_RESOURCES.

sr     = 48000        ; demiurge-run-csound forces -r to the graph rate anyway
ksmps  = 32           ; the hook handles one /p per k-cycle: keep <= 64
nchnls = 2
0dbfs  = 1

#include "params/csound/demiurge_params.udo"
giDParamPort init 9000   ; fallback only; set AFTER the include (the udo re-inits it to 0)

; Params (0..1), defaults used until the pool sends anything.
gkDensity init 0.35
gkTone    init 0.45
gkLength  init 0.50

giScale ftgen 0, 0, 8, -2, 0, 2, 4, 6, 7, 9, 11, 14  ; D Lydian degrees (semitones over root)
giRoot  = 50                                           ; D3

gaRevL init 0
gaRevR init 0

; --- param intake: pool -> k-rate values, and the echo back (/pout on change) ---
instr 1
  dparam_init "nld-csound", 0           ; 0 -> --omacro:DEMIURGE_PARAM_PORT, else giDParamPort
  gkDensity dparam "/nld-csound/density", 0.35
  gkTone    dparam "/nld-csound/tone",    0.45
  gkLength  dparam "/nld-csound/length",  0.50
  dpout "/nld-csound/density", gkDensity
  dpout "/nld-csound/tone",    gkTone
  dpout "/nld-csound/length",  gkLength
endin

; --- sparse generative scheduler ---
instr 2
  kDens port gkDensity, 0.2
  kLen  port gkLength, 0.2
  ; density 0..1 -> 0.05..1.2 notes/sec (exponential: sparse at the bottom)
  kRate = 0.05 * exp(kDens * 3.178)
  kTrig metro kRate
  kNote  init 0
  if (kTrig == 1) then
    kIdx  = int(random:k(0, 7.999))
    kOct  = 12 * int(random:k(0, 2.999))
    kNote table kIdx, giScale
    kNote = giRoot + kOct + kNote
    ; note length 0.4..9 s, exponential
    kDur  = 0.4 * exp(kLen * 3.11)
    kAmp  = random:k(0.10, 0.22)
    schedulek 3, 0, kDur, kNote, kAmp
  endif
endin

; --- the one voice: two detuned saws -> lowpass (tone) -> soft env ---
instr 3
  iCps  = cpsmidinn(p4)
  iAmp  = p5
  kEnv  linsegr 0, p3 * 0.35 + 0.02, 1, p3 * 0.65, 0, 0.3, 0
  kEnv  = kEnv * kEnv
  aA    vco2 1, iCps * 1.003, 0
  aB    vco2 1, iCps * 0.997, 0
  aMix  = (aA + aB) * 0.5
  kTone port gkTone, 0.1
  ; tone 0..1 -> cutoff 180 Hz..7 kHz, exponential, nudged by the note's pitch
  kCut  = limit(180 * exp(kTone * 3.66) + iCps * 0.5, 100, 9000)
  aF    moogladder aMix, kCut, 0.2
  aOut  = aF * kEnv * iAmp
  aL, aR pan2 aOut, 0.5
  outs aL, aR
  gaRevL += aL * 0.5
  gaRevR += aR * 0.5
endin

; --- shared reverb ---
instr 99
  aL, aR reverbsc gaRevL, gaRevR, 0.88, 9000
  outs aL * 0.6, aR * 0.6
  clear gaRevL, gaRevR
endin
</CsInstruments>
<CsScore>
i 1  0 86400
i 2  0 86400
i 99 0 86400
</CsScore>
</CsoundSynthesizer>

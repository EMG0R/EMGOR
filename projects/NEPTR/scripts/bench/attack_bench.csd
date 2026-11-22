<CsoundSynthesizer>
<CsOptions>
-o /tmp/attack_bench.wav -W
</CsOptions>
<CsInstruments>
; globals.orc references $PRESET_BANK (defined in neptrPhase4.csd's preamble in
; the real build). Mirror that default here so the bench compiles standalone.
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif

#include "../../csound/globals.orc"
#include "../../csound/effects/attack.orc"

; Scenario via --omacro:SCN=N. Renders the REAL instr 150 and prints
; "ATK t=... g=... c=..." telemetry every 5ms for analyze_attack.py.
;   0 = guitar run, 10 notes/sec, notes ring over each other (12 notes)
;   1 = tremolo re-pick, 15 notes/sec, same pitch, heavy ring (20 notes)
;   2 = low bass B0/E1 fingerstyle, 8 notes/sec, soft attacks (12 notes)
;   3 = legato: continuous tone, stepping pitch, NO amplitude change (10 steps)
;   4 = noise floor only (-70dB) — expect ZERO triggers
#ifndef SCN
#define SCN #0#
#endif

gaSrc init 0

; picked/plucked note: Karplus-Strong body + pick click burst
instr 20
  iAmp = p4
  iCps = p5
  iClk = p6
  aBody pluck iAmp, iCps, iCps, 0, 1
  aNz   rand  iAmp * iClk
  aEnvC transeg 1, 0.002, -4, 0
  aFade linsegr 1, p3 - 0.05, 1, 0.05, 0
  aSig = (aBody + aNz * aEnvC) * aFade
  gaSrc = gaSrc + aSig
endin

; legato tone: constant amplitude, pitch steps every 1/8s (flux-only test)
instr 21
  kT timeinsts
  kStep = int(kT * 8)
  kCps = 220 * pow(2, kStep * 3 / 12)
  aTone vco2 0.25, kCps
  aTone butterlp aTone, 3000
  aFade linsegr 0.001, 0.02, 1, p3 - 0.07, 1, 0.05, 0.001
  gaSrc = gaSrc + aTone * aFade
endin

; noise floor
instr 22
  aNz rand 0.0003
  gaSrc = gaSrc + aNz
endin

; driver: schedules the scenario, feeds instr 150, prints telemetry
instr 30
  iScn = $SCN
  ; controls via i-rate ternaries: k-rate assignments inside i-rate if-branches
  ; run on EVERY perf pass regardless of branch (csound goto semantics)
  iSens   = (iScn == 0 ? 0.5 : (iScn == 1 ? 0.5 : (iScn == 2 ? 0.6 : (iScn == 3 ? 0.7 : 1.0))))
  iRange  = (iScn == 0 ? 0.7 : (iScn == 2 ? 0.0 : 0.5))
  iRetrig = (iScn == 1 ? 40 : (iScn == 2 ? 50 : 45))
  gkATTK_bypass = 0
  gkATTK_mix = 1
  gkATTK_atkMs = 150
  gkATTK_shape = 0.5
  gkATTK_sens = iSens
  gkATTK_range = iRange
  gkATTK_retrigMs = iRetrig

  ; scheduling: schedule() is init-pass only, i-rate branching is safe here
  if (iScn == 0) then
    iN = 0
    while iN < 12 do
      ; E3 up a chromatic-ish run, 100ms apart, 1.2s ring
      schedule(20, 0.5 + iN * 0.1, 1.2, 0.35, 164.8 * pow(2, iN/12), 0.6)
      iN += 1
    od
  elseif (iScn == 1) then
    iN = 0
    while iN < 20 do
      ; A3 tremolo, 66.7ms apart, each rings 0.8s
      schedule(20, 0.5 + iN * 0.0667, 0.8, 0.35, 220, 0.6)
      iN += 1
    od
  elseif (iScn == 2) then
    iN = 0
    while iN < 12 do
      ; low B0 / E1 alternating, 125ms apart, soft finger attacks
      iCps = (iN % 2 == 0 ? 30.87 : 41.2)
      schedule(20, 0.5 + iN * 0.125, 1.0, 0.4, iCps, 0.15)
      iN += 1
    od
  elseif (iScn == 3) then
    schedule(21, 0.5, 1.45)
  else
    schedule(22, 0, p3)
  endif

  gaATTK_sendL = gaSrc
  gaATTK_sendR = gaSrc
  gaSrc = 0

  kT timeinsts
  printks "ATK t=%.3f g=%.3f c=%.0f\n", 0.005, kT, gkATTK_gain, gkATTK_trigCount
  outs gaATTK_outL, gaATTK_outR
endin
</CsInstruments>
<CsScore>
i 150 0 4
i 30  0 4
</CsScore>
</CsoundSynthesizer>

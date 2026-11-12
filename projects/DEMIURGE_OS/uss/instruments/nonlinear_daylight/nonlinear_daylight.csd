<CsoundSynthesizer>
<CsOptions>
; Ignored under demiurge-run-csound (-+ignore_csopts=1; it supplies -odac/-r/-b/-B/-M).
; Only here so the file also opens standalone.
-odac -d
</CsOptions>
<CsInstruments>
; ======================================================================
; USS instrument: nonlinear daylight  (Emory Smith's Csound piece)
; Derived from inbox/uss_sources/1_nonlinear_daylight_ORIGINAL.csd.
; The generative scheduler (instr 1), the FM voice with its phaser/flanger/
; chorus combs (instr 2) and the freeverb chain (instr 3) are the original,
; line for line. What was ADDED, all neutral at the defaults:
;   * the 7 USS performance params through the pool hook (see uss/README.md)
;   * a manual note trigger (instr 4): trig edge + pitch
;   * an output safety limiter (the original itself peaks at 1.08 and clips)
;   * ksmps 256 -> 32 (the hook needs ksmps <= 64), sr 44100 -> 48000
; Defaults (density 0.5, tone 0.5, length 0.5, space 0.75, intensity 0.5) are the
; ORIGINAL sound: every scale below is exactly 1.0 at its default.
; ======================================================================
sr     = 48000
ksmps  = 32
nchnls = 2
0dbfs  = 1

#include "params/csound/demiurge_params.udo"
giDParamPort init 9020     ; fallback only (the launcher passes the manifest port); set AFTER the include

#ifndef USS_SEED
#define USS_SEED #0#
#end

maxalloc 2, 40             ; CPU guard on the live Pi: never more than 40 voices (original peaks ~8)

gasigL init 0
gasigR init 0

gkRoomSize init 0.98
gkHFDamp   init 0.3
gkmix      init 1.0
gkLFO1freq init 0.03
gkLFO2freq init 0.15

; ---- USS performance params (0..1), live ----
gkDensity   init 0.5
gkTone      init 0.5
gkLength    init 0.5
gkSpace     init 0.75
gkIntensity init 0.5
gkPitch     init 0.5
gkTrig      init 0

; scale degrees of the piece (semitones, over 48), rhythm of instr 1's note table
giScale ftgen 0, 0, 8, -2, 48, 50, 52, 53, 55, 57, 59, 59

; ---- the original's weighted draws, factored so instr 1 and the trigger share them ----
opcode uss_ratio, k, 0
  kratiorand random 0, 36
  if (kratiorand < 12) then
    kratio = 0
  elseif (kratiorand < 18) then
    kratio = 1
  elseif (kratiorand < 24) then
    kratio = 2
  elseif (kratiorand < 28) then
    kratio = 3
  elseif (kratiorand < 31) then
    kratio = 4
  elseif (kratiorand < 33) then
    kratio = 5
  elseif (kratiorand < 34) then
    kratio = 6
  elseif (kratiorand < 35) then
    kratio = 7
  else
    kratio = 8
  endif
  xout kratio
endop

opcode uss_index, k, 0
  kindexrand random 0, 36
  if (kindexrand < 3) then
    kindex = 0
  elseif (kindexrand < 9) then
    kindex = 1
  elseif (kindexrand < 18) then
    kindex = 2
  elseif (kindexrand < 27) then
    kindex = 3
  else
    kindex = 4
  endif
  xout kindex
endop

; ---- param intake: pool -> k-rate values, and the echo back (/pout on change) ----
instr 1000
  dparam_init "uss-nonlinear_daylight", 0
  gkDensity   dparam "/uss-nonlinear_daylight/density",   0.5
  gkTone      dparam "/uss-nonlinear_daylight/tone",      0.5
  gkLength    dparam "/uss-nonlinear_daylight/length",    0.5
  gkSpace     dparam "/uss-nonlinear_daylight/space",     0.75
  gkIntensity dparam "/uss-nonlinear_daylight/intensity", 0.5
  gkPitch     dparam "/uss-nonlinear_daylight/pitch",     0.5
  gkTrig      dparam "/uss-nonlinear_daylight/trig",      0
  dpout "/uss-nonlinear_daylight/density",   gkDensity
  dpout "/uss-nonlinear_daylight/tone",      gkTone
  dpout "/uss-nonlinear_daylight/length",    gkLength
  dpout "/uss-nonlinear_daylight/space",     gkSpace
  dpout "/uss-nonlinear_daylight/intensity", gkIntensity
  dpout "/uss-nonlinear_daylight/pitch",     gkPitch
  dpout "/uss-nonlinear_daylight/trig",      gkTrig
endin

; scale helpers: 2^((x-0.5)*3), exactly 1.0 at x = 0.5
; (the original constants live in instr 1/2/3 below)

instr 100
    seed $USS_SEED                    ; 0 = time seed, as the original
    gkkeyoffset random -8, 2
    gktempo     random 40, 65
endin

; the generative scheduler (original), + density scaling the metro rate
instr 1
    kdscale = powoftwo((gkDensity - 0.5) * 3)
    kls     = powoftwo((gkLength - 0.5) * 3)
    ktick metro (gktempo * kdscale) / 60
    kchance random 0, 1
    knoterand random 0, 26
    if (ktick == 1 && kchance < 0.65) then
        if (knoterand < 4) then
            knote = 48
        elseif (knoterand < 6) then
            knote = 50
        elseif (knoterand < 12) then
            knote = 52
        elseif (knoterand < 14) then
            knote = 53
        elseif (knoterand < 18) then
            knote = 55
        elseif (knoterand < 23) then
            knote = 57
        else
            knote = 59
        endif
        knote = knote + gkkeyoffset
        kratio uss_ratio
        kindex uss_index
        schedkwhen 1, 0, 0, 2, 0, 10 * kls, knote, kratio, kindex, kls
    endif
endin

; the voice (original). p7 = length scale (stretches p3 and all envelopes)
instr 2
    imidi = p4
    iratio = p5
    iindex = p6
    ils = (p7 > 0 ? p7 : 1)
    ifreq = cpsmidinn(imidi)
    iamp = 0.3
    kvibfreq = 0.3
    kvibdepth = 11 / 1200
    kvib oscil kvibdepth, kvibfreq
    kmodfreq = ifreq * (1 + kvib)
    iatt = 1.5 * ils
    idec = 5 * ils
    isus = 0.0
    irel = 5 * ils
    kenv adsr iatt, idec, isus, irel
    iFMatt random 0, 2.5
    iFMdec random 0, 2.5
    iFMsus random 0, 0
    iFMrel random 0, 2.5
    kFMenv adsr iFMatt * ils, iFMdec * ils, iFMsus, iFMrel * ils
    ; tone = FM brightness: modulation index x 2^((tone-0.5)*3), live
    ktone = powoftwo((gkTone - 0.5) * 3)
    ; intensity = level: x 2^((i-0.5)*2), live
    klev = powoftwo((gkIntensity - 0.5) * 2)
    amod oscili kmodfreq * iindex * ktone * kFMenv, kmodfreq * iratio
    acar oscili iamp * klev * kenv, kmodfreq + amod
    kphasorLFO oscil 0.002, 0.05
    aphasorL comb acar, 0.2 + kphasorLFO, 0.005, 0.8
    aphasorR comb acar, 0.2 - kphasorLFO, 0.005, 0.8
    amixPhasorL = acar
    amixPhasorR = acar
    aflangerL comb amixPhasorL, 0.02, 0.005, 0.9
    aflangerR comb amixPhasorR, 0.02, 0.005, 0.9
    amixFlangerL = 0.8 * amixPhasorL + 0.2 * aflangerL
    amixFlangerR = 0.8 * amixPhasorR + 0.2 * aflangerR
    kchorusLFO oscil 0.002, 0.1
    achorusL vdelay amixFlangerL, 0.03 + kchorusLFO, 0.05
    achorusR vdelay amixFlangerR, 0.03 - kchorusLFO, 0.05
    amixChorusL = 0.8 * amixFlangerL + 0.2 * achorusL
    amixChorusR = 0.8 * amixFlangerR + 0.2 * achorusR
    gasigL = gasigL + amixChorusL
    gasigR = gasigR + amixChorusR
endin

; manual trigger: trig rising edge -> one note in the piece's key, pitch 0..1 picks
; the scale degree over three octaves; ratio/index drawn like every generated note
instr 4
    ktr trigger gkTrig, 0.5, 0
    if (ktr == 1) then
        kidx = int(limit(gkPitch, 0, 0.9999) * 21)
        kdeg table (kidx % 7), giScale
        knote = kdeg + 12 * (int(kidx / 7) - 1) + gkkeyoffset
        kratio uss_ratio
        kindex uss_index
        kls = powoftwo((gkLength - 0.5) * 3)
        schedkwhen 1, 0, 0, 2, 0, 10 * kls, knote, kratio, kindex, kls
    endif
endin

; reverb (original). space: wet 0..1 (0.75 = the original, fully wet),
; below 0.75 a dry copy of the voices fades in; above, bigger/darker room.
instr 3
    kLFO1 oscil 0.02, gkLFO1freq
    kLFO2 oscil 0.05, gkLFO2freq
    kover = limit((gkSpace - 0.75) * 4, 0, 1)                ; 0..1 above default
    kModRoomSize = limit(gkRoomSize + kover * 0.015, 0, 0.995) + kLFO1 * 0.05
    kModHFDamp = gkHFDamp + kover * 0.3 + kLFO2 * 0.1
    kwet = limit(gkSpace / 0.75, 0, 1)
    kdry = limit(1 - gkSpace / 0.75, 0, 1)
    kPreDelayL oscil 0.01, 0.15
    kPreDelayR oscil 0.01, 0.17
    adelayedL vdelay gasigL, 0.05 + kPreDelayL, 0.1
    adelayedR vdelay gasigR, 0.05 + kPreDelayR, 0.1
    acombL comb adelayedL, 0.2, 0.1, 0.7
    acombR comb adelayedR, 0.2, 0.1, 0.7
    arvbL, arvbR freeverb acombL, acombR, kModRoomSize, kModHFDamp
    aoutL = arvbL * 0.8 * kwet + gasigL * 0.8 * kdry
    aoutR = arvbR * 0.8 * kwet + gasigR * 0.8 * kdry
    ; safety limiter: identity below 0.7, soft knee above, ceiling 0.95 (the original clips at 1.08)
    aaL = abs(aoutL)
    aaR = abs(aoutR)
    aovL = limit(aaL - 0.7, 0, 10)
    aovR = limit(aaR - 0.7, 0, 10)
    aoutL = aoutL * (1 - (aovL - 0.25 * tanh(aovL / 0.25)) / (aaL + 0.000001))
    aoutR = aoutR * (1 - (aovR - 0.25 * tanh(aovR / 0.25)) / (aaR + 0.000001))
    outs aoutL, aoutR
    clear gasigL, gasigR
endin

</CsInstruments>
<CsScore>
i 1000 0 86400
i 100 0 1
i 1 0 86400
i 4 0 86400
i 3 0 86400
</CsScore>
</CsoundSynthesizer>

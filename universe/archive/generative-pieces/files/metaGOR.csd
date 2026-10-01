<CsOptions>
-odac   ; Real-time audio output
</CsOptions>
<CsInstruments>
sr      = 44100   ; Sample rate
ksmps   = 256     ; Control rate
nchnls  = 2       ; Stereo output
0dbfs   = 1       ; Full-scale amplitude

; Global variables for reverb
gasigL init 0
gasigR init 0

gkRoomSize init 0.98  ; Base room size (very large space)
gkHFDamp   init 0.3   ; Base high-frequency damping
gkmix      init 1.0   ; Fully wet mix

; Randomized reverb modulation variables
gkModRoomSize random 0.9, 1.0   ; Slightly modulated room size
gkModHFDamp   random 0.25, 0.35 ; Modulate HF damping subtly

; Global tempo – You can either keep this fixed or randomize it periodically 
gktempo init 50  ; Use a nominal tempo; if you want it random, update it periodically

; Note generation instrument
instr 1
    ; Metronome tick (calculated every control period)
    ktick metro gktempo / 60

    ; Chance to trigger a note per tick (65% chance here)
    kchance random 0, 1

    if (ktick == 1 && kchance < 0.65) then
        ; Compute a new random key offset for each note
        ikeyoffset random -8, 2

        ; Optimized weighted note selection:
        knoterand random 0, 26   ; Total weight is 26
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

        ; Apply random key offset per note
        knote = knote + ikeyoffset

        ; Weighted FM ratio selection
        kratiorand random 0, 36  ; Total weight is 36
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

        ; Weighted FM index selection (favoring higher values)
        kindexrand random 0, 36  ; Total weight is 36
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

        ; Schedule note with selected FM ratio and index – optimized scheduling call
        schedkwhen 1, 0, 0, 2, 0, 10, knote, kratio, kindex
    endif
endin

; FM oscillator note generator with various modulation effects
instr 2
    imidi = p4         ; MIDI note from instrument 1
    iratio = p5        ; FM ratio passed from instrument 1
    iindex = p6        ; FM index passed from instrument 1

    ifreq = cpsmidinn(imidi)
    iamp = 0.3

    ; Slow vibrato (±8 cents)
    kvibfreq = 0.3
    kvibdepth = 11 / 1200
    kvib oscil kvibdepth, kvibfreq
    kmodfreq = ifreq * (1 + kvib)

    ; Basic amplitude envelope
    iatt = 1.5
    idec = 5
    isus = 0.0
    irel = 5
    kenv adsr iatt, idec, isus, irel

    ; Randomized FM envelope
    iFMatt random 0, 2.5
    iFMdec random 0, 2.5
    iFMsus random 0, 0
    iFMrel random 0, 2.5
    kFMenv adsr iFMatt, iFMdec, iFMsus, iFMrel

    ; FM synthesis
    amod oscili kmodfreq * iindex * kFMenv, kmodfreq * iratio
    acar oscili iamp * kenv, kmodfreq + amod

    ; (Phasor-like, flanger, chorus effects, etc., are left unchanged for now)
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

; Reverb and modulation instrument remain unchanged
instr 3
    kLFO1 oscil 0.02, 0.03
    kLFO2 oscil 0.05, 0.15
    kModRoomSize = gkRoomSize + kLFO1 * 0.05
    kModHFDamp = gkHFDamp + kLFO2 * 0.1
    kPreDelayL oscil 0.01, 0.15
    kPreDelayR oscil 0.01, 0.17
    adelayedL vdelay gasigL, 0.05 + kPreDelayL, 0.1
    adelayedR vdelay gasigR, 0.05 + kPreDelayR, 0.1
    acombL comb adelayedL, 0.2, 0.1, 0.7
    acombR comb adelayedR, 0.2, 0.1, 0.7
    arvbL, arvbR freeverb acombL, acombR, kModRoomSize, kModHFDamp
    outs arvbL*0.8, arvbR*0.8
    clear gasigL, gasigR
endin

</CsInstruments>
<CsScore>
; Remove or comment out instrument 100 unless needed for other random globals
; i 100 0 1
i 1 0 z      ; Start note-generation instrument indefinitely
i 3 0 z      ; Start reverb instrument (always on)
</CsScore>
</CsoundSynthesizer>
<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="background">
  <r>240</r>
  <g>240</g>
  <b>240</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>

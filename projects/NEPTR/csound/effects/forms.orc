; FORMS — Dramatic Formant / Spectral Character Shifter
; Signal chain position: 119 (right before FORMANT at 120)
; Reads: gaFormsSendL/R
; Writes: gaFormsOutL/R
; Bypass: $FX_BYPASS via gkFormsBypass
; Controls (UNCHANGED — preserve mapping):
;   gkFormsShift  -12..+12 (secondary / toggle-28 encoder + expr pedal) — main timbral knob
;   gkFormsMix    0..1 dry/wet (handled in signal_chain.orc via ntrpol)
;
; --- WHAT CHANGED (more dramatic, TIMBRE-led) ---------------------------------
; The legacy version was a single clean pvswarp (spectral-envelope shift, pitch
; untouched) — transparent but tame. It now layers three timbral mechanisms on
; top of that same warp, all scaled by the SAME gkFormsShift knob so the control
; feel is preserved, just far more striking:
;   1) DEEPER SPECTRAL WARP + BLUR: pvswarp envelope shift is steepened and fed
;      through pvsblur (time-averaged spectrum) by an amount that grows with
;      |shift| — gives a vowel-like "holding"/throaty smear that turns a subtle
;      formant nudge into a vocal-tract morph.
;   2) FORMANT RESONATOR BANK: 3 parallel BROAD-Q bandpass resonators voiced at
;      vowel formants (~600/1200/2700 Hz) whose centre freqs ride kformscale.
;      This is the big timbral move — it imposes a strong, sung/throaty resonant
;      colour that the clean pvswarp alone never produced. Q is deliberately LOW
;      (wide bandwidth) so the formant reads as a broad, musical focus — you know
;      where you are — rather than a narrow, surgical ring.
;   3) LIVING MOTION: a slow LFO wobbles the resonator centres (depth scaled by
;      |shift|) so the timbre breathes instead of sitting static.
; STABILITY: resonator bank is RMS-balanced to the dry input (balance), the wet
; sum is dcblock'd, tanh soft-clipped, and hard-limited. No runaway feedback.
;
; kmeth=1 (liftered cepstrum, realtime-safe) on pvswarp. Do not switch to 2.

instr 119
aInL = gaFormsSendL
aInR = gaFormsSendR

iFFTsize = 1024
ioverlap = iFFTsize / 4
iwinsize = iFFTsize
iwintype = 1  ; Hanning

kshift port gkFormsShift, 0.03
kformscale = pow(2, kshift / 12)

; Magnitude of the move, 0..1 over the -12..+12 range — drives how DRAMATIC the
; extra colour is. At shift 0 the effect is near-clean; it intensifies as the
; knob departs from centre in either direction.
kmag = abs(kshift) / 12
kmag limit kmag, 0, 1

;----------------------------------------------------------------------
; 1) DEEPER SPECTRAL WARP + BLUR
; pvswarp args: fin, kscal, kshift, klowest, kmeth(?), kgain, kcoefs
; Steepen the envelope shift and lift gain with the move so the spectral
; tilt reads more strongly; then blur (average) the spectrum — blur grows
; with |shift| for a vocal "holding" smear (bounded to stay artifact-free).
;----------------------------------------------------------------------
fsigL pvsanal aInL, iFFTsize, ioverlap, iwinsize, iwintype
fsigR pvsanal aInR, iFFTsize, ioverlap, iwinsize, iwintype

; Envelope shift in Hz tracks the move (downshift on lower formants for body,
; upshift for brightness) — adds a directional spectral push on top of scale.
kenvshift = kshift * 12
kgain = 1 + kmag * 0.6

fWarpL pvswarp fsigL, kformscale, kenvshift, 0, 1, kgain, 60
fWarpR pvswarp fsigR, kformscale, kenvshift, 0, 1, kgain, 60

; Spectral blur: 0..~0.12 s of frame averaging as the knob opens up.
kblur = kmag * 0.10
fBlurL pvsblur fWarpL, kblur, 0.15
fBlurR pvsblur fWarpR, kblur, 0.15

aSpecL pvsynth fBlurL
aSpecR pvsynth fBlurR

; pvswarp/blur brighten when scaling envelope up; equal-power-ish comp.
kSpecGain = (kformscale > 1) ? 1/sqrt(kformscale) : 1
aSpecL = aSpecL * kSpecGain
aSpecR = aSpecR * kSpecGain

;----------------------------------------------------------------------
; 3) LIVING MOTION — slow LFO that wobbles the resonator centres.
; Depth scales with |shift| so it's quiet near centre, breathing when pushed.
;----------------------------------------------------------------------
klfo lfo 1, 0.7, 0          ; -1..1 triangle, ~0.7 Hz
kwob = 1 + klfo * kmag * 0.08   ; +/- up to ~8% centre-freq sway

;----------------------------------------------------------------------
; 2) FORMANT RESONATOR BANK — the dramatic timbral colour.
; Three vowel-ish formants, centres ride kformscale (so they track the same
; knob) and the LFO wobble. BROAD Q for a strong-but-gentle sung/throaty
; formant focus, bounded for safety. Applied to the spectrally-warped signal
; so the two stages reinforce.
; Centres are port-smoothed so the formant GLIDES (broad character) instead of
; snapping/zipping as the knob and LFO move — reads as wide, not surgical.
;----------------------------------------------------------------------
kf1 = 600  * kformscale * kwob
kf2 = 1200 * kformscale * kwob
kf3 = 2700 * kformscale * kwob
kf1 limit kf1, 80, 18000
kf2 limit kf2, 120, 19000
kf3 limit kf3, 200, 20000
kf1 port kf1, 0.05
kf2 port kf2, 0.05
kf3 port kf3, 0.05

; Bandwidths as a fraction of centre → BROAD, constant-ratio Q (Q ~ 1/0.20 = 5).
; Widened from 0.06 (narrow/peaky, Q~17) to 0.20 so each formant is a wide,
; musical hump rather than a pinched ring — strong presence, gentle edges.
ibwr = 0.20

aF1L butterbp aSpecL, kf1, kf1*ibwr
aF2L butterbp aSpecL, kf2, kf2*ibwr
aF3L butterbp aSpecL, kf3, kf3*ibwr
aResL = aF1L*1.0 + aF2L*0.8 + aF3L*0.6

aF1R butterbp aSpecR, kf1, kf1*ibwr
aF2R butterbp aSpecR, kf2, kf2*ibwr
aF3R butterbp aSpecR, kf3, kf3*ibwr
aResR = aF1R*1.0 + aF2R*0.8 + aF3R*0.6

; RMS-balance the resonant bank to the dry input so high Q can't blow up level.
aResL balance aResL, aInL
aResR balance aResR, aInR

;----------------------------------------------------------------------
; BLEND: spectral-warped body + resonant formant colour.
; Resonant share grows with |shift| so pushing the knob = more throat/character.
; (gkFormsMix dry/wet on top of this is applied in signal_chain.orc.)
;----------------------------------------------------------------------
; Slightly lower resonant share than the narrow version: a broad formant has
; far more energy/width than a thin spike, so less mix gives the same sense of
; "strong presence" without the bank dominating. 0.30 at centre → 0.68 pushed.
kresMix = 0.30 + kmag * 0.38
aOutL = aSpecL*(1 - kresMix) + aResL*kresMix
aOutR = aSpecR*(1 - kresMix) + aResR*kresMix

; Light harmonic edge that grows with the move — adds bite without fizz.
kdrive = 1 + kmag * 1.5
aOutL = tanh(aOutL * kdrive) / kdrive * (1 + kmag*0.5)
aOutR = tanh(aOutR * kdrive) / kdrive * (1 + kmag*0.5)

;----------------------------------------------------------------------
; STABILITY: DC block + hard limit. No feedback paths, so this is bounded.
;----------------------------------------------------------------------
aOutL dcblock2 aOutL
aOutR dcblock2 aOutR
aOutL limit aOutL, -0.99, 0.99
aOutR limit aOutR, -0.99, 0.99

gaFormsOutL = aOutL
gaFormsOutR = aOutR
endin

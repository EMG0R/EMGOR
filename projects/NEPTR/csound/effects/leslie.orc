; Leslie Speaker — TRUE DOPPLER ROTARY (v2, 2026-07-13)
; Signal chain position: 300
; Reads: gaLeslieSendL/R
; Writes: gaLeslieOutL/R
; Bypass: gkLeslieBypass (toggle 10), FX_BYPASS in signal_chain.orc — unchanged
;
; v1 ("DopplerSpin", saved at _retired/leslie_v1_dopplerspin.orc.bak) was
; independent tremolo + autopan LFOs with the Doppler depth hard-set to 0 —
; i.e. no Doppler at all. v2 runs the physical model: ONE rotation angle per
; rotor drives BOTH pitch (variable propagation delay, d/343 s) and loudness
; (1/d distance law x horn directivity), so AM and FM are phase-coupled in
; quadrature the way a real rotating speaker couples them.
;
; Geometry (meters): source on a circle (horn r=0.14, drum r=0.20), two
; virtual mics at azimuth +/-25deg, distance D from the cabinet center.
;   d(phi)    = sqrt(D^2 + r^2 - 2*D*r*cos(phi))        phi = rotor angle - mic az
;   delay     = 0.5ms + d/343                            -> physical Doppler
;   cosBeta   = (D*cos(phi) - r) / d                     horn axis vs mic bearing
;   gain      = (sqrt(D^2+r^2)/d) * directivity(cosBeta) same phi -> coupled AM
; Horn beams tighter (cardioid-weighted), drum is diffuse; drum counter-rotates
; at 0.90x horn speed below an 800 Hz crossover. Rotor INERTIA: speed target
; glides (portk half-times ~0.35 s horn / ~1.2 s drum => ~1 s / ~4 s settles),
; so speed flips swell like a real ramp, and pedal sweeps feel mechanical.
;
; Controls (channel names + ranges IDENTICAL to v1 — no midi_handler changes):
;   gkLeslieSpeed  0.1..10 Hz  target rotation (encoder exp / CC29 pedal)
;   gkLeslieDepth  0..1        virtual mic distance: 1=0.25m close (deep throb)
;                              0=3.0m far (subtle)   [was AM/pan depth in v1]
;
; INTEGRATION NOTES (for the integration agent):
;   * No shared-file changes REQUIRED: toggle 10, instr 300, OSC "les",
;     gkLeslieSpeed/gkLeslieDepth/gaLeslieSend*/gaLeslieOut* all unchanged.
;   * OPTIONAL: neptrPhase4_UI.py EFFECT_PARAMS "les" second param label
;     "Depth" reads better as "Mic Dist" (still 0..1, same OSC /les_depth).
;   * globals.orc DopplerSpin UDO is now UNUSED by any effect — retirement
;     candidate next time globals.orc is edited (do not remove here).
;   * CPU class: cheap inline (2 crossover pairs, 4 vdelayx taps, a-rate trig).

instr 300
  ; ---- targets + rotor inertia (the ramp IS the leslie gesture) ----
  kSpdTarg limit gkLeslieSpeed, 0.1, 10
  kHornHz  portk kSpdTarg,        0.35, i(gkLeslieSpeed)
  kDrumHz  portk kSpdTarg * 0.90, 1.20, i(gkLeslieSpeed) * 0.90

  ; mic distance from depth: close = deep effect, far = subtle
  kDepth limit gkLeslieDepth, 0, 1
  kD     = 0.30 + (1 - kDepth) * (1 - kDepth) * 2.70

  ; ---- input: mono sum, split at 800 Hz. Drum side is 48 dB/oct: a real
  ; drum driver + cabinet louvers kill highs steeply, and HF leaking into the
  ; counter-rotating drum path smears the horn's coupled AM/FM image.
  aMix = (gaLeslieSendL + gaLeslieSendR) * 0.5
  aLo  butlp aMix, 800
  aLo  butlp aLo,  800
  aLo  butlp aLo,  800
  aLo  butlp aLo,  800
  aHi  buthp aMix, 800
  aHi  buthp aHi,  800

  ; ---- rotation angles, a-rate (phase-continuous under speed changes) ----
  aPhH phasor kHornHz          ; horn, clockwise
  aPhD phasor kDrumHz          ; drum, counter-rotating (mirror via +micAz below)

  ihR   = 0.14                 ; horn mouth radius
  idR   = 0.20                 ; drum port radius
  iAz   = 0.4363               ; mic azimuth +/-25 deg
  i2pi  = 6.28318530717959
  kD2h  = kD*kD + ihR*ihR
  kD2d  = kD*kD + idR*idR
  kNrmH = sqrt(kD2h)           ; unity gain reference at quadrature
  kNrmD = sqrt(kD2d)

  ; ================= HORN (mic L: az -iAz, mic R: az +iAz) =================
  aCosHL = cos(i2pi*aPhH + iAz)
  aCosHR = cos(i2pi*aPhH - iAz)
  adHL   = sqrt(kD2h - 2*kD*ihR*aCosHL)
  adHR   = sqrt(kD2h - 2*kD*ihR*aCosHR)
  ; directivity: cardioid weighted toward the horn's outward axis
  aDirHL = 0.25 + 0.375 * (1 + (kD*aCosHL - ihR)/adHL)
  aDirHR = 0.25 + 0.375 * (1 + (kD*aCosHR - ihR)/adHR)
  aGnHL  limit (kNrmH/adHL) * aDirHL, 0, 4
  aGnHR  limit (kNrmH/adHR) * aDirHR, 0, 4
  aHLd   vdelayx aHi, 0.0005 + adHL/343, 0.05, 8
  aHRd   vdelayx aHi, 0.0005 + adHR/343, 0.05, 8

  ; ================= DRUM (counter-rotation: mirrored mic offsets) =========
  aCosDL = cos(i2pi*aPhD - iAz)
  aCosDR = cos(i2pi*aPhD + iAz)
  adDL   = sqrt(kD2d - 2*kD*idR*aCosDL)
  adDR   = sqrt(kD2d - 2*kD*idR*aCosDR)
  aDirDL = 0.60 + 0.20 * (1 + (kD*aCosDL - idR)/adDL)
  aDirDR = 0.60 + 0.20 * (1 + (kD*aCosDR - idR)/adDR)
  aGnDL  limit (kNrmD/adDL) * aDirDL, 0, 4
  aGnDR  limit (kNrmD/adDR) * aDirDR, 0, 4
  aDLd   vdelayx aLo, 0.0005 + adDL/343, 0.05, 8
  aDRd   vdelayx aLo, 0.0005 + adDR/343, 0.05, 8

  ; ---- sum, gentle makeup (mean directivity < 1), click-safe edges ----
  aEnv linsegr 0, 0.010, 1, 0.030, 0
  gaLeslieOutL = (aHLd*aGnHL + aDLd*aGnDL) * 1.25 * aEnv
  gaLeslieOutR = (aHRd*aGnHR + aDRd*aGnDR) * 1.25 * aEnv
endin

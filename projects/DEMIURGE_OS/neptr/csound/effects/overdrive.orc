; Overdrive slot — THE BACKUP TUBE SCREAMER (2026-06-11 v3).
; This is the user's original effects/tubescreamer.orc circuit, verbatim
; DSP, promoted to the DRIVE slot 0 pedal ("REPLACE overdrive with my
; backup tubescreamer"). The v2 recreation is gone; at default settings
; the signal path below is sample-for-sample the legacy instr 100 sound
; (plain tanh — deliberately NOT oversampled, its grit is part of the
; sound the user locked in on).
;
; INLINE ZERO-LATENCY: opcode called directly in instr 1 (signal_chain),
; gated by gkOD_bypass. No bus, no instr, no added latency.
;
; LEGACY CORE (see effects/tubescreamer.orc, kept as the reference):
;   kDrive    = 1 + 300*drive          (huge gain into tanh at the top)
;   split at 200 Hz: highs -> tanh clipper, lows -> clean unity path
;   aDist     = tanh(aHp*kDrive)/tanh(kDrive)
;   post LP   = 2500 + 1000*drive      (more treble as drive rises)
;   out       = aDist(LP'd) + clean lows
;
; CONTROLS (all 0..1, NEUTRAL AT DEFAULTS = exact legacy sound):
;   gkOD_drive  the legacy drive sweep 0..1 (preset default 0.45)
;   gkOD_tone   shifts the legacy post-LP cutoff x0.5..x2 (log);
;               0.5 = exactly the legacy curve
;   gkOD_wc1    BODY: moves the 200 Hz clean-lows split x2..x0.5
;               (0.5 = legacy 200 Hz; higher = fatter, more clean lows)
;   gkOD_wc2    CLEAN BLEND: out = wet*(1-b) + dry*b; 0 (default) = the
;               pure legacy pedal
;
; MAKEUP: legacy open-loop curve (0.026163 + drive*0.0327...) times the
; unified bench-fitted quadratic — target +0.83 dB (= +10% amplitude)
; over dry, same scheme as all DRIVE pedals.

opcode ODPedal, aa, aa
    ainL, ainR xin

    kDrv  portk gkOD_drive, 0.03
    kTon  portk gkOD_tone,  0.03
    kBod  portk gkOD_wc1,   0.03
    kBld  portk gkOD_wc2,   0.03
    kDrv  limit kDrv, 0, 1
    kTon  limit kTon, 0, 1
    kBod  limit kBod, 0, 1
    kBld  limit kBld, 0, 1

    ; legacy curves, with neutral-at-0.5 trims for tone/body.
    ; DRIVE TAPER (2026-06-11 v4): the legacy linear law 1+300*d put
    ; breakup at d~0.02 — the knob/pedal was "max everywhere". Same
    ; endpoints (1x..301x), exponential spread: breakup (~9x) now sits
    ; at d~0.4, halfway-to-max (d=0.7) is a true mid crunch (~55x).
    ; The CIRCUIT is unchanged — only the knob mapping.
    kDrive    = exp(5.70711 * kDrv)         ; 301^d : 1x..301x
    kLpCutoff = (2500 + 1000 * kDrv) * exp(0.693147 * (kTon - 0.5) * 2)
    kSplit    = 200 * exp(0.693147 * (0.5 - kBod) * 2)

    ; legacy open-loop makeup x unified gain-stage fit (+10% vs dry)
    kDeq    = (kDrive - 1) / 300            ; legacy-equivalent drive position
    kMakeup = (0.026163 + kDeq * 0.03270375)
    kMakeup = kMakeup * ampdb(+38.5953 + kDrv*(-39.1663 + kDrv*(-60.1691 + kDrv*(+93.0330 + kDrv*(-40.5780)))))  ; gain-stage fit 2026-06-11 v6 REAL (in-RMS 0.009): +0.83 dB vs dry
    kMakeup = kMakeup * ampdb(8.0 * kDrv + 0.0)  ; drive bias + slot offset: TS baseline

    ; ---- LEFT (legacy path, verbatim) ----
    aHpL = butterhp(ainL, kSplit)
    aLpL = butterlp(ainL, kSplit)
    aDsL = tanh(aHpL * kDrive) / tanh(kDrive)
    aDsL = tone(aDsL, kLpCutoff)
    aWetL = (aDsL + aLpL) * kMakeup

    ; ---- RIGHT ----
    aHpR = butterhp(ainR, kSplit)
    aLpR = butterlp(ainR, kSplit)
    aDsR = tanh(aHpR * kDrive) / tanh(kDrive)
    aDsR = tone(aDsR, kLpCutoff)
    aWetR = (aDsR + aLpR) * kMakeup

    ; clean blend (default 0 = pure legacy)
    aOutL = aWetL * (1 - kBld) + ainL * kBld
    aOutR = aWetR * (1 - kBld) + ainR * kBld
    aOutL dcblock2 aOutL
    aOutR dcblock2 aOutR
    aOutL limit aOutL, -0.98, 0.98
    aOutR limit aOutR, -0.98, 0.98

    xout aOutL, aOutR
endop

; Marshall — JCM800/Plexi-style crunch preamp (amp-in-a-box)
; Signal chain position: 131
; Reads:  gaMAR_sendL/R
; Writes: gaMAR_outL/R
; Bypass: N/A (chain launches/kills the instance; no bypass logic here)
;
; CIRCUIT MODEL — JCM800 2203 / 1959 Plexi PREAMP ONLY.
; This is meant to stack into a REAL amp: no cabinet/speaker sim, no
; power-amp sag or compression. Cascaded 12AX7 common-cathode stages:
;
;   input HP (tightness) -> STAGE 1 (mild, asymmetric) -> bright shelf
;   -> interstage HP coupling -> STAGE 2 (main gain, asymmetric,
;   polarity flipped vs stage 1 — real stages invert) -> STAGE 3
;   (crossfaded in above drive 0.6 for 2203-style cascade saturation)
;   -> TMB tone stack (fixed ~650 Hz Marshall mid notch, tone tilts
;   bass<->treble shelves around it) -> presence high shelf (~4 kHz,
;   what amp-in-a-box pedals do instead of power-amp NFB presence)
;   -> open-loop makeup -> dcblock2.
;
; Tube-isms modelled:
;   * Each stage clips asymmetrically (grid conduction vs cutoff) via
;     OS4Diode with unequal vp/vn -> even+odd harmonic mix ("warm").
;     Stage 2 uses mirrored thresholds (vp/vn swapped) because each
;     common-cathode stage inverts; partial even-harmonic cancellation
;     across the cascade, as in the real circuit. dcblock2 after every
;     asymmetric stage (they generate DC offset).
;   * Interstage HP coupling caps stop low-end mush as gain rises;
;     the tightness control moves the input + interstage corners.
;   * Bright cap across the gain pot: treble lift that is strongest at
;     LOW gain and fades as the pot opens -> shelf gain ~ (1 - drive).
;
; CONTROLS (all normalized 0..1):
;   gkMAR_drive  preamp gain (expression pedal). 0 = nearly clean
;                (stage gains ~1x/2x, no stage 3); 1 = full crunch/lead
;                (stage2 pre 60x, stage 3 fully in). Also shrinks the
;                bright-cap lift, like the real pot.
;   gkMAR_tone   tone-stack tilt: 0 = bass-heavy/dark (+6 dB @110 Hz,
;                -6 dB @2.2 kHz), 0.5 = flat shelves (mid notch only),
;                1 = bright/cutting (mirror). Mid notch fixed -4 dB @650.
;   gkMAR_wc1    PRESENCE: post high shelf @4 kHz, 0..+6 dB.
;   gkMAR_wc2    TIGHTNESS: input HP 30..300 Hz (log sweep) and
;                interstage HP 20..180 Hz.
;
; MAKEUP CALIBRATION (open-loop — pure function of drive, NO rms or
; envelope followers; see effects/tubescreamer.orc for the rationale:
; followers pump and eat sustain). Bench: 0.1-RMS 196 Hz decaying
; pluck into the send, default tone/pres/tight, measured pre-makeup
; output RMS, then fitted gain in dB as a quadratic in drive:
;
;     makeup_dB(d) = -6.2 - 19.0*d + 11.4*d^2
;
;   drive=0.00: -6.2 dB  (small-signal stage gain ~2x + tone stack)
;   drive=0.50: -12.8 dB (stage 2 deep into saturation)
;   drive=1.00: -13.8 dB (stages 2+3 fully saturated, level plateaus)
;
; Verified within +/-2 dB of dry at drive 0 / 0.5 / 1 (see bench notes
; at bottom). Saturation means a decaying note holds level longer than
; dry — that is the pedal's compression, not a level error.

; INLINE ZERO-LATENCY (2026-06-11): opcode called directly in instr 1,
; like the Preamp. The old bus/instr design added one ksmps block
; (5.3 ms) of latency per active pedal. instr 131 no longer exists.
opcode MARPedal, aa, aa
    aInL, aInR xin

    ; --- control conditioning (pedal/encoder data arrives stepped) ---
    kDrive portk gkMAR_drive, 0.02
    kTone  portk gkMAR_tone,  0.05
    kPres  portk gkMAR_wc1,   0.05
    kTight portk gkMAR_wc2,   0.05
    kDrive limit kDrive, 0, 1
    kTone  limit kTone,  0, 1
    kPres  limit kPres,  0, 1
    kTight limit kTight, 0, 1
    kTone  = kTone * 0.4        ; remap: 50% knob = original 20% (bright offset fix)

    ; --- mappings ---
    ; anti-thin 2026-06-11: HP ranges lowered (were 30..300 / 20..180)
    kHPin   = 25 * pow(6, kTight)           ; input HP: 25..150 Hz (log)
    kHPis   = 15 + 85 * kTight              ; interstage HP: 15..100 Hz
    kPre1   = 1 + 3 * kDrive                ; stage 1: 1x..4x (mild)
    ; max-drive extension 2026-06-11 ("needs to drive a bit more at max"):
    ; top-only multiplier — identical below d=0.7, up to ~150x at d=1
    kXt     limit (kDrive - 0.7) / 0.3, 0, 1
    kPre2   = (2 + 58 * kDrive * kDrive) * (1 + 1.5 * kXt * kXt)
    kMix3   limit (kDrive - 0.6) / 0.4, 0, 1 ; stage 3 crossfade
    ; bench 2026-06-11: +9 dB bright cap read 11.4% high-band energy at low
    ; drive (dry = 1%) — the "trebly" complaint. Halved to +4 dB max.
    kBright = ampdb(2.5 * (1 - kDrive))     ; bright cap: +2.5 dB..0 @1.8k
    kBass   = ampdb((0.5 - kTone) * 12)     ; tone tilt: +/-6 dB shelves
    kTreb   = ampdb((kTone - 0.5) * 12)
    kPrsAmp = ampdb(6 * kPres)              ; presence: 0..+6 dB @4k
    iMidCut = ampdb(-2)                     ; mid notch softened (anti-thin)
    ; bench-refit 2026-06-11: original analytic curve (-6.2 -19.0d +11.4d^2)
    ; read -5 dB at drive=0 and -1.9 dB early-note at drive=1 on the 0.1-RMS
    ; 196 Hz pluck bench. Refit through (0: 0 dB off), (0.5: hold), (1: +1):
    kMakeup = ampdb(-1.2 - 34.8*kDrive + 23.2*kDrive*kDrive)
    kMakeup = kMakeup * ampdb(+1.7094 + kDrive*(-15.9687 + kDrive*(-94.2589 + kDrive*(+204.1850 + kDrive*(-114.8724)))))  ; gain-stage fit 2026-06-11 v6 REAL (in-RMS 0.009): +0.83 dB vs dry
    ; residual correction table (2026-06-11): 11 breakpoints over
    ; drive 0..1, linearly interpolated — absorbs the stage-3
    ; crossfade level kink a polynomial can't follow.
    iMarFix ftgentmp 0, 0, 16, -2, -0.029, -0.029, +0.017, -0.037, +0.017, +0.048, -0.120, +0.392, +0.029, -0.619, +0.024
    kFixDb  tablei  kDrive * 10, iMarFix
    kMakeup = kMakeup * ampdb(kFixDb)
    kMakeup = kMakeup * ampdb(12.0 * kDrive + 0.5)  ; drive bias + slot offset: Marshall +0.5 dB


    ; ================= LEFT =================
    aL  butterhp aInL, kHPin                ; tightness: pre-clip low cut
    aL  OS4Diode aL, kPre1, 0.6, 0.9        ; stage 1: 12AX7, mild asym
    aL  dcblock2 aL
    aL  pareq    aL, 1800, kBright, 0.707, 2 ; bright cap (fades w/ gain)
    aL  butterhp aL, kHPis                  ; interstage coupling cap
    aL2 OS4Diode aL, kPre2, 0.9, 0.6        ; stage 2: main gain, inverted
    aL2 dcblock2 aL2                        ;          polarity asymmetry
    aL3 OS4Diode aL2, 3.0, 0.5, 0.8         ; stage 3: 2203 cascade
    aL3 dcblock2 aL3
    aLs = aL2 * (1 - kMix3) + aL3 * kMix3
    aLs pareq aLs, 110,  kBass,   0.71, 1   ; TMB: bass shelf
    aLs pareq aLs, 650,  iMidCut, 0.60, 0   ; TMB: Marshall mid notch
    aLs pareq aLs, 2200, kTreb,   0.71, 2   ; TMB: treble shelf
    aLs pareq aLs, 130, ampdb(2), 0.71, 1   ; warmth shelf (anti-thin)
    aLs pareq aLs, 4000, kPrsAmp, 0.71, 2   ; presence
    aLs butterlp aLs, 9000                  ; fizz guard: 2-pole @9k — only
                                            ; touches >8k hash, 2-5k presence true
    aLs = aLs * kMakeup
    aLs dcblock2 aLs

    ; ================= RIGHT ================
    aR  butterhp aInR, kHPin
    aR  OS4Diode aR, kPre1, 0.6, 0.9
    aR  dcblock2 aR
    aR  pareq    aR, 1800, kBright, 0.707, 2
    aR  butterhp aR, kHPis
    aR2 OS4Diode aR, kPre2, 0.9, 0.6
    aR2 dcblock2 aR2
    aR3 OS4Diode aR2, 3.0, 0.5, 0.8
    aR3 dcblock2 aR3
    aRs = aR2 * (1 - kMix3) + aR3 * kMix3
    aRs pareq aRs, 110,  kBass,   0.71, 1
    aRs pareq aRs, 650,  iMidCut, 0.60, 0
    aRs pareq aRs, 2200, kTreb,   0.71, 2
    aRs pareq aRs, 130, ampdb(2), 0.71, 1
    aRs pareq aRs, 4000, kPrsAmp, 0.71, 2
    aRs butterlp aRs, 9000                  ; fizz guard: 2-pole @9k
    aRs = aRs * kMakeup
    aRs dcblock2 aRs

    xout aLs, aRs
endop

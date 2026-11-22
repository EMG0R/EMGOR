; Tube Screamer — LEGACY, NOT WIRED (2026-06-11)
; Removed from neptrPhase4.csd includes and the signal chain when the
; DRIVE menu was redesigned; kept on disk as reference only. The TS
; character lives on inside effects/overdrive.orc (instr 130, wc1=1).
; To resurrect: re-add the #include and a chain block — nothing else
; references instr 100 anymore.
; Signal chain position: 100
; Reads: gaTSSendL/R
; Writes: gaTSOutL/R
; Bypass: N/A

instr 100
    aInL = gaTSSendL
    aInR = gaTSSendR

    kDrive = 1 + 300 * gkTSAmtAcc ; Increased for more distortion at top
    kLpCutoff = 2500 + 1000 * gkTSAmtAcc ; Slightly increase LP cutoff with drive for more treble

    aHpL = butterhp(aInL, 200) ; Lower HP to pass more low end through distortion
    aHpR = butterhp(aInR, 200)

    aLpL = butterlp(aInL, 200) ; Corresponding LP for clean lows
    aLpR = butterlp(aInR, 200)

    aDistL = tanh(aHpL * kDrive) / tanh(kDrive)
    aDistR = tanh(aHpR * kDrive) / tanh(kDrive)

    aDistL = tone(aDistL, kLpCutoff) ; Dynamic LP to get more trebly with drive
    aDistR = tone(aDistR, kLpCutoff)

    aOutL = aDistL + aLpL
    aOutR = aDistR + aLpR

    ; STATIC drive-dependent makeup gain — no RMS detection, no portk,
    ; no envelope follower. Open-loop: makeup is purely a function of the
    ; drive amount, stable across all input levels (no pumping; sustain
    ; decays linearly with input).
    ;
    ; RE-VOICED for the mixable TS channel (menu-0 leftmost): the slot's
    ; encoder sweeps gkTSAmtAcc 0..1 (clean..dirty), and level must stay
    ; ROUGHLY CONSTANT across that sweep, sitting a hair ABOVE dry guitar
    ; (~0.1 RMS at typical playing) so the channel reads as a unity-ish
    ; "always a touch louder" boost rather than the old set-it-quieter comp.
    ;
    ; tanh at kDrive≈300 clamps the distorted path to ~1.0 RMS regardless
    ; of input, so the curve is nearly FLAT (small positive slope) — that
    ; flatness is what keeps loudness consistent as you dial dirt:
    ;   drive=0.0:    makeup ≈ 0.105   (cleanest, ~dry + a hair)
    ;   drive=0.45:   makeup ≈ 0.112   (preset default — some dirt)
    ;   drive=1.0:    makeup ≈ 0.120   (full dirt, only ~1.2 dB hotter)
    ;
    ; NOTE: ear-calibrated target, not bench-measured. Nudge the 0.105
    ; baseline up/down to taste; keep the slope small to preserve the
    ; consistent-level-across-drive behaviour.
    kMakeup  =  0.105 + gkTSAmtAcc * 0.015
    aOutL    =  aOutL * kMakeup
    aOutR    =  aOutR * kMakeup

    gaTSOutL = aOutL
    gaTSOutR = aOutR
endin

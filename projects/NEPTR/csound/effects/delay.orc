; Delay
; Signal chain position: 62
; Reads: gaDelSendL/R
; Writes: gaDelOutL/R
; Bypass: gkBypassDelay
;
; MODES (gkDlyMode, self-declared below — 2026-07-13 reverse-delay feature):
;   0 = normal  — byte-identical to the pre-mode delay (mode-0 null verified
;                 against scripts/bench/delay_v1_ref.orc by
;                 scripts/bench/analyze_delay_rev.py)
;   1 = reverse — repeats play backwards. flooper2-style reversed-grain reads
;                 of a ring buffer written with the SAME aSum (input+feedback)
;                 the normal path delays. Grain length = the delay time; two
;                 read heads a half-grain apart, Hann amplitude-complementary
;                 crossfade (sum == 1, click-free wraps: each head only jumps
;                 while its window is at 0). The reversed signal then runs
;                 through the SAME pitch-shift + spring-tone feedback path, so
;                 all existing params behave identically (2nd repeat comes
;                 back re-reversed = forward — classic reverse-delay feedback).
;                 vdelay3 stays warm in mode 1 (runs unconditionally) so mode
;                 switches never replay a stale buffer; a 20 ms portk crossfade
;                 (exact 0 in mode 0 — preserves the null) blends the paths.
;
; INTEGRATION NOTES (for the integration agent — this feature branch touches
; ONLY this file; everything below is a spec, not a diff. Integration resolves
; collisions.):
;   globals.orc     : NOTHING to add — gkDlyMode + giDlyRev* are self-declared
;                     below (include order: globals -> effects). Do NOT
;                     redeclare them.
;   midi_handler.orc: delay = toggle 15, home s0/CC24. Existing map (verified
;                     read-only 2026-07-13): P1 CC24 gkDelMix, P2 CC20
;                     gkDelayMS, P3 CC23 gkFeedback, P4 fill CC22 gkSemiDel.
;                     gkDlyMode lands on the NEXT free fill = P5, in the CC25
;                     block's "P4+ fills" section (precedent: ATTACK/ENV/BLP
;                     P5 fills live there), stepped:
;                       elseif (gkLastToggle == 15) then
;                         ; Delay mode NORMAL/REVERSE (P5 fill, stepped)
;                         $ENC_ROT(gkDlyMode'1'0'1)
;                     OSC feedback, next to the existing /dly_* group:
;                       $OSC_I(gkDlyMode'/dly_mode'dlyMode)
;   neptrPhase4_UI.py: EFFECT_PARAMS["del"] += ("mode","Reverse",0,1);
;                     route_osc handles /dly_mode.
;   docs            : note P5 = mode on the delay row of encoder_mapping_v2.

gkDlyMode init 0    ; 0 = normal (default), 1 = reverse

; reverse-mode ring buffer: 131072 smp = 2.73 s @48k (>= 2x iMaxDel + margin)
giDlyRevL ftgen 0, 0, 131072, -2, 0
giDlyRevR ftgen 0, 0, 131072, -2, 0

instr 62
    aInL  = gaDelSendL
    aInR  = gaDelSendR

    kDelay portk gkDelayMS/1000, gkPortTime
    aDelay interp kDelay

    kFbRaw portk gkFeedback, gkPortTime
    kFb    = kFbRaw * 1.2
    kFb    limit kFb, 0, 1.2

    aMemL init 0
    aMemR init 0

    iMaxDel = 800
    aDelay  limit aDelay, 0, iMaxDel

    if (gkBypassDelay >= 0.5) then
        gaDelOutL = aInL
        gaDelOutR = aInR
    else
        aSumL = aInL + aMemL * kFb
        aSumR = aInR + aMemR * kFb

        ; --- reverse-mode ring buffer (see header) -------------------------
        ; Written EVERY block (both modes) so engaging reverse always has
        ; live history — the write is 2 tabw/sample, negligible. Reads only
        ; happen while the mode crossfade is open; in mode 0 the loop below
        ; is write-only and the audio path is bit-untouched.
        iM0   = (i(gkDlyMode) >= 0.5 ? 1 : 0)
        kMode = (gkDlyMode >= 0.5 ? 1 : 0)
        kXf   portk kMode, 0.02, iM0
        iN    = ftlen(giDlyRevL)
        ; grain = delay time, in samples (kDelay above is in ms — vdelay3
        ; units; gkDelayMS/1000 -> ms). Clamp: 10 ms floor, iMaxDel ceiling.
        kG    limit kDelay * 0.001 * sr, 480, iMaxDel * 0.001 * sr
        kInc  = 1 / kG
        k2G   = 2 * kG
        kWrRv init 0
        kPhRv init 0
        aRevL init 0
        aRevR init 0
        kS = 0
        while kS < ksmps do
            tabw(aSumL[kS], kWrRv, giDlyRevL)
            tabw(aSumR[kS], kWrRv, giDlyRevR)
            if (kXf > 0.0001) then
                kPhRv = kPhRv + kInc
                kPhRv = kPhRv - int(kPhRv)
                kP2   = kPhRv + 0.5
                kP2   = kP2 - int(kP2)
                ; Hann amplitude-complementary crossfade (sum == 1)
                kW1 = 0.5 - 0.5 * cos(6.283185307 * kPhRv)
                kW2 = 1 - kW1
                ; head 1: read runs backward at -1x through the last grain
                kRd = kWrRv - k2G * kPhRv
                kRd = (kRd < 0 ? kRd + iN : kRd)
                kI  = int(kRd)
                kF  = kRd - kI
                kI1 = (kI + 1 >= iN ? kI + 1 - iN : kI + 1)
                kV1L = tab:k(kI, giDlyRevL) * (1 - kF) + tab:k(kI1, giDlyRevL) * kF
                kV1R = tab:k(kI, giDlyRevR) * (1 - kF) + tab:k(kI1, giDlyRevR) * kF
                ; head 2: half a grain apart
                kRd = kWrRv - k2G * kP2
                kRd = (kRd < 0 ? kRd + iN : kRd)
                kI  = int(kRd)
                kF  = kRd - kI
                kI1 = (kI + 1 >= iN ? kI + 1 - iN : kI + 1)
                kV2L = tab:k(kI, giDlyRevL) * (1 - kF) + tab:k(kI1, giDlyRevL) * kF
                kV2R = tab:k(kI, giDlyRevR) * (1 - kF) + tab:k(kI1, giDlyRevR) * kF
                aRevL[kS] = kW1 * kV1L + kW2 * kV2L
                aRevR[kS] = kW1 * kV1R + kW2 * kV2R
            endif
            kWrRv = (kWrRv + 1 >= iN ? 0 : kWrRv + 1)
            kS += 1
        od
        ; -------------------------------------------------------------------

        aDelL vdelay3 aSumL, aDelay, iMaxDel
        aDelR vdelay3 aSumR, aDelay, iMaxDel

        ; mode blend: kXf is EXACTLY 0 in mode 0 (portk from 0 toward 0), so
        ; this branch never runs and mode 0 stays byte-identical to v1.
        if (kXf > 0.0001) then
            aDelL = aDelL + (aRevL - aDelL) * kXf
            aDelR = aDelR + (aRevR - aDelR) * kXf
        endif

        ; 2026-07-26: modern H90 pitch shifting replaces pitchshifter4 (the
        ; gritty 4-grain "echo shifter" the user vetoed). Mono-summed into the
        ; shared H90Shift UDO (globals.orc) like the H90 pedal itself; the
        ; small-interval blend below keeps |semi| < 1 pulled toward the
        ; untouched STEREO delay, so semi = 0 stays a plain transparent echo
        ; (no PV smear/latency on unshifted repeats).
        kRatio = pow(2, gkSemiDel/12)
        aDelM  = (aDelL + aDelR) * 0.5
        aShM   H90Shift aDelM, kRatio
        kAbsSD = abs(gkSemiDel)
        kBldD  = (kAbsSD < 1.0 ? 1.0 - kAbsSD : 0.0)
        kBldD  portk kBldD, 0.02
        aShiftL = aShM * (1 - kBldD) + aDelL * kBldD
        aShiftR = aShM * (1 - kBldD) + aDelR * kBldD

        ; Spring-delay tone shaping in the feedback path: each successive repeat
        ; gets band-limited like a spring reverb (no deep lows, no sparkle highs),
        ; so repeats darken naturally — classic Roland RE-201 / Echoplex feel.
        ; Cumulative on each pass through the feedback loop. The DRY signal is
        ; unaffected because filtering only touches aMem*, not the wet output.
        aMemL = butterhp(aShiftL, 180)
        aMemL = butterlp(aMemL,   4000)
        aMemR = butterhp(aShiftR, 180)
        aMemR = butterlp(aMemR,   4000)

        gaDelOutL = gkDelMix * aShiftL + (1 - gkDelMix) * aInL
        gaDelOutR = gkDelMix * aShiftR + (1 - gkDelMix) * aInR
    endif
endin

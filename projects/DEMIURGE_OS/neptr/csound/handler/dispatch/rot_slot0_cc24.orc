    ;==========================================================
    ; SLOT 0 ENCODER ROTATION — CC24 (Enc5 rot)
    ; Accumulator = amount for slot 0 effects
    ;==========================================================
    ; 2026-06-23 FULL-TAKEOVER: all slot rotations now dispatch on gkLastToggle
    ; (the most-recently-toggled effect owns every encoder). Menu 0 keeps its
    ; preset knobs (CC21/CC22); CC24/25/26 are inert in menu 0 as before.
    ; Menu 13 (LOOPER) intercepts all slot rotations for per-channel speed.
    elseif (kCC == 24) then
      $LP_CH1_SPEED
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 0 (PS GLIDE) — DELIBERATELY INERT.
        ; This lane is 100% wet pitch driven by the expression
        ; pedal; the user's call is that its encoder does nothing,
        ; so there is no list to scroll and no send here. The
        ; BUTTON still toggles the lane (toggle_slot0.orc).
        kHgNoop0 = 0
      elseif (gkCurrentMenu == 17) then
        ; NAM row 4 (CRUNCH) variant cycle: soft -> dirty, wraps
        if (kVal > 64) then
          gkNAMRowSel[4] = gkNAMRowSel[4] + 1
          if (gkNAMRowSel[4] >= giNAMRowCount[4]) then
            gkNAMRowSel[4] = 0
          endif
        elseif (kVal < 64) then
          gkNAMRowSel[4] = gkNAMRowSel[4] - 1
          if (gkNAMRowSel[4] < 0) then
            gkNAMRowSel[4] = giNAMRowCount[4] - 1
          endif
        endif
        if (gkNAMActiveRow == 4 && gkNAMBypass < 0.5) then
          gkNAMProfileIdx = giNAMRowBase[4] + gkNAMRowSel[4]
        endif
        printks "NAM row 4 -> variant %d\n", 0, gkNAMRowSel[4]
      elseif (gkCurrentMenu == 18) then
        ; IR row 0 (SPRING) variant cycle: short/tight -> long/washy, wraps
        if (kVal > 64) then
          gkIRSel[0] = gkIRSel[0] + 1
          if (gkIRSel[0] >= giIRRowCount[0]) then
            gkIRSel[0] = 0
          endif
        elseif (kVal < 64) then
          gkIRSel[0] = gkIRSel[0] - 1
          if (gkIRSel[0] < 0) then
            gkIRSel[0] = giIRRowCount[0] - 1
          endif
        endif
        printks "IR row 0 -> variant %d\n", 0, gkIRSel[0]
      elseif (gkCurrentMenu != 0) then
        ; --- P1 (effects whose home slot = 0) ---
        ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
        ; chain. Rotation 24 = hero param P1 of the focused effect.
        if (gkLastToggle >= 75 && gkLastToggle <= 98) then
          SM_RotStep gkLastToggle, 0, kVal
        elseif (gkLastToggle == 1) then
          $ENC_ROT(gkVOLpedal'0.02'0'1)
        elseif (gkLastToggle == 16) then
          $ENC_ROT(gkSpecDelMix'gkStepSpecMix'0'1)
        elseif (gkLastToggle == 26) then
          ; H90 mix — home moved to PITCH slot 0 (2026-07-13)
          $ENC_ROT(gkH90mix'0.05'0'1)
        elseif (gkLastToggle == 6) then
          $ENC_ROT(gkVIB_depth'0.0001'0'0.005)
        elseif (gkLastToggle == 31) then
          $ENC_ROT(gkOD_drive'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 15) then
          $ENC_ROT(gkDelMix'gkStepDelMix'0'1)
        elseif (gkLastToggle == 46) then
          ; LOSSY mix (P1, home FILTERING s0)
          $ENC_ROT(gkLSY_mix'0.02'0'1)
        elseif (gkLastToggle == 48) then
          ; IMPULSE mix (P1, home GLITCH s0)
          $ENC_ROT(gkIMP_mix'0.02'0'1)
        elseif (gkLastToggle == 40) then
          ; BASS PRE LIGHT drive (P1, BASS menu s0)
          $ENC_ROT(gkBPL_drive'0.02'0'1)
        elseif (gkLastToggle == 56) then
          ; BLOOPER (P1) — pedal-position parameter on the encoder (2026-09-01).
          ; This branch used to be an empty no-op (level knob removed
          ; 2026-07-14), which SHADOWED the generated P-EXPR route in
          ; gen/expr_rot_24.orc (included below — first match wins), leaving
          ; no encoder path to gkBLP_expr at all. Wire it here directly.
          $ENC_ROT(gkBLP_expr'0.02'0'1)
        elseif (gkLastToggle == 45) then
          ; ONWARD FOLLOW (P4 fill) — harmonic-follow strength, also CC29
          $ENC_ROT(gkONW_follow'0.02'0'1)
        elseif (gkLastToggle == 66) then
          ; MOOD II GENERATIVE (P4 fill) — 0 static clock .. 1 ~50% duty away
          ; from home on the tonal ladder, free-running (see mood2.orc header)
          $ENC_ROT(gkMD2_Gen'0.02'0'1)
        elseif (gkLastToggle == 61) then
          ; TRACK SYNTH blend (P1, home SYNTH s0)
          $ENC_ROT(gkTSY_mix'0.02'0'1)
        elseif (gkLastToggle == 63) then
          ; FEEDBACKER voice level (P1, home AIR s0)
          $ENC_ROT(gkFBK_level'0.02'0'1)
        ; (NAM OUT gain used to sit here on P1. Moved to P3/CC23 2026-08-11:
        ;  menu 17 gives every slot rotation to the NAM variant browse, so a
        ;  P1 home was dead in the amp sim's own menu.)
        ; --- P4+ fills ---
        elseif (gkLastToggle == 2) then
          ; ATTACK v2 swell shape: percussive .. violin (P4 fill)
          $ENC_ROT(gkATTK_shape'gkStepATTK_shape'0'1)
        elseif (gkLastToggle == 55) then
          ; ENV FILTER decay shape (P5 fill; home = BASS s4)
          $ENC_ROT(gkENV_dec'0.02'0'1)
        elseif (gkLastToggle == 57) then
          ; COMPRESSOR parallel blend (P4 fill; home = PREAMP s4)
          $ENC_ROT(gkCMP_blend'0.02'0'1)
        elseif (gkLastToggle == 58) then
          ; TONE EQ low shelf (P4 fill; home = SYNTH s2)
          $ENC_ROT(gkTEQ_low'0.02'0'1)
        elseif (gkLastToggle == 60) then
          ; CHAIN FB safety ceiling (P4 fill; home = AIR s4)
          $ENC_ROT(gkCFB_ceil'0.02'0.1'0.95)
        elseif (gkLastToggle == 62) then
          ; PLL FUZZ voice morph master->sub->PLL (P4 fill; home = SYNTH s1)
          $ENC_ROT(gkPLL_vmix'0.02'0'1)
        elseif (gkLastToggle == 64) then
          ; DIFFUSION duck (P4 fill; home = AIR s1)
          $ENC_ROT(gkDIF_duck'0.02'0'1)
        elseif (gkLastToggle == 65) then
        elseif (gkLastToggle == 67) then
          ; BINSCATTER max-delay window (P4 fill; home = SPECTRAL s2/CC26)
          $ENC_ROT(gkBscMaxDel'0.02'0.02'1.0)
        elseif (gkLastToggle == 68) then
          ; TAPE DELAY loop-filter CENTER (P4 fill; home = DELAY s4/CC22) —
          ; log taper 250-5000 Hz
          $ENC_ROT_EXP(gkTdlFiltCtr'1.06'250'5000)
        elseif (gkLastToggle == 69) then
          ; POLY DLY Num R (P4 fill) — integer 1..16
          $ENC_ROT(gkMvdNumR'1'1'16)
#include "handler/dispatch/gen/expr_rot_24.orc"
        endif
      endif


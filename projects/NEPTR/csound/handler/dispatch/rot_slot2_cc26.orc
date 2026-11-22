    ;==========================================================
    ; SLOT 2 ENCODER ROTATION — CC26 (Enc7 rot)
    ; Accumulator = amount for slot 2 effects
    ;==========================================================
    elseif (kCC == 26) then
      $LP_CH3_SPEED
      elseif (gkCurrentMenu == $MX_MENU) then
        ; MIXER page (mixer_page.orc): primary encoder = channel volume.
        $MX_VOL_ROT(2)
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 2 (FORMANT) — accumulate through the
        ; lane's preset list on the Bela. 64 is the encoder's no-movement
        ; value; sending on it would re-engage the lane on every idle report.
        if (kVal != 64) then
          $HG_ROTATE(2'(kVal > 64 ? 1 : -1))
        endif
      elseif (gkCurrentMenu == 17) then
        ; NAM row 2 (FENDER) variant cycle: soft -> dirty, wraps
        if (kVal > 64) then
          gkNAMRowSel[2] = gkNAMRowSel[2] + 1
          if (gkNAMRowSel[2] >= giNAMRowCount[2]) then
            gkNAMRowSel[2] = 0
          endif
        elseif (kVal < 64) then
          gkNAMRowSel[2] = gkNAMRowSel[2] - 1
          if (gkNAMRowSel[2] < 0) then
            gkNAMRowSel[2] = giNAMRowCount[2] - 1
          endif
        endif
        if (gkNAMActiveRow == 2 && gkNAMBypass < 0.5) then
          gkNAMProfileIdx = giNAMRowBase[2] + gkNAMRowSel[2]
        endif
        printks "NAM row 2 -> variant %d\n", 0, gkNAMRowSel[2]
      elseif (gkCurrentMenu == 18) then
        ; IR row 2 (HALL) variant cycle
        if (kVal > 64) then
          gkIRSel[2] = gkIRSel[2] + 1
          if (gkIRSel[2] >= giIRRowCount[2]) then
            gkIRSel[2] = 0
          endif
        elseif (kVal < 64) then
          gkIRSel[2] = gkIRSel[2] - 1
          if (gkIRSel[2] < 0) then
            gkIRSel[2] = giIRRowCount[2] - 1
          endif
        endif
        printks "IR row 2 -> variant %d\n", 0, gkIRSel[2]
      elseif (gkCurrentMenu != 0) then
        ; --- P1 (effects whose home slot = 2) ---
        ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
        ; chain. Rotation 26 = hero param P3 of the focused effect.
        if (gkLastToggle >= 75 && gkLastToggle <= 98) then
          SM_RotStep gkLastToggle, 2, kVal
        elseif (gkLastToggle == 10) then
          $ENC_ROT(gkLeslieDepth'gkStepLeslieDepth'0'1)
        elseif (gkLastToggle == 45) then
          ; ONWARD DENSITY (P5 fill) — call-and-response echo density
          $ENC_ROT(gkONW_density'0.02'0'1)
        elseif (gkLastToggle == 4) then
          $ENC_ROT(gkGPSmix'0.05'0'1)
        elseif (gkLastToggle == 38) then
          $ENC_ROT(gkBinPedal'0.02'0'1)
        elseif (gkLastToggle == 44) then
          $ENC_ROT(gkBcPedal'0.02'0'1)
        elseif (gkLastToggle == 8) then
          $ENC_ROT(gkPHA_depth'gkStepPHA_depth'0'1)
        elseif (gkLastToggle == 34) then
          $ENC_ROT(gkRMZ_drive'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 19) then
          $ENC_ROT(gkverbMix'gkStepverbMix'0'1)
        elseif (gkLastToggle == 22) then
          ; WAH pedal position — home moved to FILTERING slot 2 (2026-07-13)
          $ENC_ROT(gkWAH_freq'0.02'0'1)
        elseif (gkLastToggle == 23) then
          ; WGuide dry/wet — home moved to GLITCH slot 2 (2026-07-13)
          $ENC_ROT(gkWGuide_drywet'gkStepWGuide_drywet'0'1)
        elseif (gkLastToggle == 42) then
          ; BASS DIST drive (P1, BASS menu s2)
          $ENC_ROT(gkBDS_drive'0.02'0'1)
        elseif (gkLastToggle == 36) then
          ; HABIT mix (P1, home CHASE BLISS s2 — was CC25/TIME s1)
          $ENC_ROT(gkHabitMix'gkStepHabitMix'0'1)
        elseif (gkLastToggle == 58) then
          ; TONE EQ mid (P1, home SYNTH s2; 0.5 = flat)
          $ENC_ROT(gkTEQ_mid'0.02'0'1)
        elseif (gkLastToggle == 65) then
        elseif (gkLastToggle == 67) then
          ; BINSCATTER mix (P1, home SPECTRAL s2)
          $ENC_ROT(gkBscMix'0.02'0'1)
        elseif (gkLastToggle == 69) then
          ; POLY DLY Den R (P1 fill) — integer 1..16
          $ENC_ROT(gkMvdDenR'1'1'16)
        elseif (gkLastToggle == 72) then
          ; FREEZE frozen-bed level (P1, home DIGIVERB s2)
          $ENC_ROT(gkFrzBedLevel'0.02'0'1)
        ; --- P4+ fills ---
        elseif (gkLastToggle == 2) then
          ; ATTACK v2 retrigger refractory ms (P6 fill)
          $ENC_ROT(gkATTK_retrigMs'gkStepATTK_retrig'20'150)
        elseif (gkLastToggle == 56) then
          ; BLOOPER grain CHARACTER NEAR/THROW/FAR (P5 fill, stepped;
          ; home = CHASE BLISS s0 — moved off CC24 when that became its P1)
          $ENC_ROT(gkBLP_Char'1'0'2)
        elseif (gkLastToggle == 61) then
          ; TRACK SYNTH sub level (P5 fill; home = SYNTH s0)
          $ENC_ROT(gkTSY_sub'0.02'0'1)
        elseif (gkLastToggle == 68) then
          ; TAPE DELAY head mode (P6 fill; home = DELAY s4/CC22) — stepped
          ; 1-7 mask index (1,2,3,2+3,1+2,1+3,1+2+3)
          $ENC_ROT(gkTdlHeadMode'1'1'7)
#include "handler/dispatch/gen/expr_rot_26.orc"
        endif
      endif


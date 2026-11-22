    ;==========================================================
    ; SLOT 4 ENCODER ROTATION — CC22 (Enc3 rot)
    ; Accumulator = amount for slot 4 effects
    ;==========================================================
    elseif (kCC == 22) then
      $LP_CH5_SPEED
      elseif (gkCurrentMenu == $MX_MENU) then
        ; MIXER page (mixer_page.orc): primary encoder = channel volume.
        $MX_VOL_ROT(0)
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 4 (SYNTH) — accumulate through the
        ; lane's preset list on the Bela. 64 is the encoder's no-movement
        ; value; sending on it would re-engage the lane on every idle report.
        if (kVal != 64) then
          $HG_ROTATE(4'(kVal > 64 ? 1 : -1))
        endif
      elseif (gkCurrentMenu == 17) then
        ; NAM row 0 (AC30) variant cycle: soft -> dirty, wraps
        if (kVal > 64) then
          gkNAMRowSel[0] = gkNAMRowSel[0] + 1
          if (gkNAMRowSel[0] >= giNAMRowCount[0]) then
            gkNAMRowSel[0] = 0
          endif
        elseif (kVal < 64) then
          gkNAMRowSel[0] = gkNAMRowSel[0] - 1
          if (gkNAMRowSel[0] < 0) then
            gkNAMRowSel[0] = giNAMRowCount[0] - 1
          endif
        endif
        if (gkNAMActiveRow == 0 && gkNAMBypass < 0.5) then
          gkNAMProfileIdx = giNAMRowBase[0] + gkNAMRowSel[0]
        endif
        printks "NAM row 0 -> variant %d\n", 0, gkNAMRowSel[0]
      elseif (gkCurrentMenu == 0) then
        ; PRESETS leftmost slot = OVERDRIVE channel (unchanged).
        ; Sweep OD drive 0..1 = clean..dirty (makeup keeps level ~constant).
        $ENC_ROT(gkOD_drive'gkStepDriveTone'0'1)
      elseif (gkCurrentMenu == 18) then
        ; IR row 4 (WOOD RM) variant cycle
        if (kVal > 64) then
          gkIRSel[4] = gkIRSel[4] + 1
          if (gkIRSel[4] >= giIRRowCount[4]) then
            gkIRSel[4] = 0
          endif
        elseif (kVal < 64) then
          gkIRSel[4] = gkIRSel[4] - 1
          if (gkIRSel[4] < 0) then
            gkIRSel[4] = giIRRowCount[4] - 1
          endif
        endif
        printks "IR row 4 -> variant %d\n", 0, gkIRSel[4]
      else
        ; --- P1 (effects whose home slot = 4) ---
        ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
        ; chain. Rotation 22 = hero param P5 of the focused effect.
        if (gkLastToggle >= 75 && gkLastToggle <= 98) then
          SM_RotStep gkLastToggle, 4, kVal
        elseif (gkLastToggle == 21) then
          $ENC_ROT(gkTapeAmount'0.02'0'1)
        elseif (gkLastToggle == 45) then
          ; ONWARD OCTAVE voice (P7 fill) — -1 half-speed .. +1 double-speed
          $ENC_ROT(gkONW_oct'0.04'-1'1)
        elseif (gkLastToggle == 55) then
          ; ENV FILTER wet (P1, home BASS s4)
          $ENC_ROT(gkENV_wet'0.02'0'1)
        elseif (gkLastToggle == 25) then
          ; FreqShift dry/wet — home moved to GLITCH slot 4 (2026-07-13)
          $ENC_ROT(gkFShift_drywet'gkStepFShift_drywet'0'1)
        elseif (gkLastToggle == 28) then
          $ENC_ROT(gkFormsMix'0.05'0'1)
        elseif (gkLastToggle == 9) then
          $ENC_ROT(gkFLA_depth'0.0002'0'0.01)
        elseif (gkLastToggle == 35) then
          $ENC_ROT(gkMUF_drive'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 17) then
          $ENC_ROT(gkMOOD_Mix'gkStepMOOD_Mix'0'1)
        elseif (gkLastToggle == 57) then
          ; COMPRESSOR amount (P1, home PREAMP s4) — threshold+ratio macro
          $ENC_ROT(gkCMP_amt'0.02'0'1)
        elseif (gkLastToggle == 60) then
          ; CHAIN FB return level (P1, home AIR s4)
          $ENC_ROT(gkCFB_ret'0.02'0'1)
        elseif (gkLastToggle == 68) then
          ; TAPE DELAY mix (P1, home DELAY s4)
          $ENC_ROT(gkTdlMix'0.02'0'1)
        elseif (gkLastToggle == 70) then
          ; NEBULA mix (P1, home DIGIVERB s4)
          $ENC_ROT(gkNebMix'0.02'0'1)
        ; --- P4+ fills (CC22 = leftmost encoder = first aux slot) ---
        elseif (gkLastToggle == 69) then
          ; POLY DLY pitch-envelope Decay (fraction of T), 0..1
          $ENC_ROT(gkMvdDec'0.02'0'1)
        elseif (gkLastToggle == 4) then
          ; GPS delay (P4 fill)
          $ENC_ROT(gkGPSdly'0.005'0'0.1)
        elseif (gkLastToggle == 8) then
          ; Phaser feedback (P4 fill)
          $ENC_ROT(gkPHA_fback'0.02'0'0.9)
        elseif (gkLastToggle == 14) then
          ; AutoMoog level (P4 fill)
          $ENC_ROT(gkAUTOMOOG_level'0.04'0'2)
        elseif (gkLastToggle == 15) then
          ; Delay semitone shift (P4 fill)
          $ENC_ROT(gkSemiDel'gkStepSemiDel'-5'12)
        elseif (gkLastToggle == 18) then
          ; Granular pitch scatter (P4 fill)
          $ENC_ROT(gkGranPitch'1'0'24)
        elseif (gkLastToggle == 31) then
          ; OD tone (P4 fill)
          $ENC_ROT(gkOD_tone'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 32) then
          ; Marshall tone (P4 fill)
          $ENC_ROT(gkMAR_tone'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 33) then
          ; DHG tone (P4 fill)
          $ENC_ROT(gkDHG_tone'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 34) then
          ; RMZ tone (P4 fill)
          $ENC_ROT(gkRMZ_tone'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 36) then
          ; HABIT blend (fill; home = CHASE BLISS s2 since 2026-07-13). CC22 was
          ; never derived from HABIT's old home (s1) and does not collide with
          ; its new home (s2/CC26), so this binding is unchanged by the move.
          $ENC_ROT(gkHabitBlend'gkStepHabitBlend'0'1)
        elseif (gkLastToggle == 43) then
          ; Tremolo depth (P4 fill — duplicate of the CC21 P1 binding, per
          ; the tremolo v2 notes: harmless, keeps depth on the "next" encoder)
          $ENC_ROT(gkTremDepth'0.02'0'1)
        elseif (gkLastToggle == 46) then
          ; LOSSY packets (P4 fill; home = FILTERING s0)
          $ENC_ROT(gkLSY_pkt'0.02'0'1)
        elseif (gkLastToggle == 47) then
          ; RESONATOR octave voice -1..+1 (P4 fill; home = FILTERING s1)
          $ENC_ROT(gkRES_oct'0.04'-1'1)
#include "handler/dispatch/gen/expr_rot_22.orc"
        endif
      endif


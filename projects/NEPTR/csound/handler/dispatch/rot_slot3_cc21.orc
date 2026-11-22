    ;==========================================================
    ; SLOT 3 ENCODER ROTATION — CC21 (Enc2 rot)
    ; Accumulator = amount for slot 3 effects
    ;==========================================================
    elseif (kCC == 21) then
      $LP_CH4_SPEED
      elseif (gkCurrentMenu == $MX_MENU) then
        ; MIXER page (mixer_page.orc): primary encoder = channel volume.
        $MX_VOL_ROT(1)
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 3 (DYNAMICS) — accumulate through the
        ; lane's preset list on the Bela. 64 is the encoder's no-movement
        ; value; sending on it would re-engage the lane on every idle report.
        if (kVal != 64) then
          $HG_ROTATE(3'(kVal > 64 ? 1 : -1))
        endif
      elseif (gkCurrentMenu == 17) then
        ; NAM row 1 (DELUXE) variant cycle: soft -> dirty, wraps
        if (kVal > 64) then
          gkNAMRowSel[1] = gkNAMRowSel[1] + 1
          if (gkNAMRowSel[1] >= giNAMRowCount[1]) then
            gkNAMRowSel[1] = 0
          endif
        elseif (kVal < 64) then
          gkNAMRowSel[1] = gkNAMRowSel[1] - 1
          if (gkNAMRowSel[1] < 0) then
            gkNAMRowSel[1] = giNAMRowCount[1] - 1
          endif
        endif
        if (gkNAMActiveRow == 1 && gkNAMBypass < 0.5) then
          gkNAMProfileIdx = giNAMRowBase[1] + gkNAMRowSel[1]
        endif
        printks "NAM row 1 -> variant %d\n", 0, gkNAMRowSel[1]
      elseif (gkCurrentMenu == 0) then
        ; PRESETS slot 3 = SOLID DIST channel: sweep DHG drive (unchanged)
        $ENC_ROT(gkDHG_drive'gkStepDriveTone'0'1)
      elseif (gkCurrentMenu == 18) then
        ; IR row 3 (CHAMBER) variant cycle
        if (kVal > 64) then
          gkIRSel[3] = gkIRSel[3] + 1
          if (gkIRSel[3] >= giIRRowCount[3]) then
            gkIRSel[3] = 0
          endif
        elseif (kVal < 64) then
          gkIRSel[3] = gkIRSel[3] - 1
          if (gkIRSel[3] < 0) then
            gkIRSel[3] = giIRRowCount[3] - 1
          endif
        endif
        printks "IR row 3 -> variant %d\n", 0, gkIRSel[3]
      else
        ; --- P1 (effects whose home slot = 3) ---
        ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
        ; chain. Rotation 21 = hero param P4 of the focused effect.
        if (gkLastToggle >= 75 && gkLastToggle <= 98) then
          SM_RotStep gkLastToggle, 3, kVal
        elseif (gkLastToggle == 2) then
          ; ATTACK v2 mix — parallel blend (POG2 dry slider)
          $ENC_ROT(gkATTK_mix'0.02'0'1)
        elseif (gkLastToggle == 45) then
          ; ONWARD ERROR (P6 fill) — mangle intensity
          $ENC_ROT(gkONW_err'0.02'0'1)
        elseif (gkLastToggle == 66) then
          ; MOOD II MIX (P1 — home CHASE BLISS s3). Wet blend on top of the
          ; toggle-66 bypass; the chain ntrpol applies it, so 0 is a bit-exact
          ; null even while the effect is toggled on.
          $ENC_ROT(gkMD2_Mix'0.02'0'1)
        elseif (gkLastToggle == 27) then
          $ENC_ROT(gkPFmix'0.05'0'1)
        elseif (gkLastToggle == 5) then
          $ENC_ROT(gkFormantMix'0.05'0'1)
        elseif (gkLastToggle == 33) then
          $ENC_ROT(gkDHG_drive'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 18) then
          $ENC_ROT(gkGranMix'gkStepGranMix'0'1)
        elseif (gkLastToggle == 14) then
          ; AutoMoog sensitivity — home moved to FILTERING slot 3 (2026-07-13)
          $ENC_ROT(gkAUTOMOOG_sens'gkStepAUTOMOOG_sens'0'1)
        elseif (gkLastToggle == 24) then
          ; SqMod dry/wet — home moved to GLITCH slot 3 (2026-07-13)
          $ENC_ROT(gkSqMod_drywet'gkStepSqMod_drywet'0'1)
        elseif (gkLastToggle == 43) then
          ; Tremolo DEPTH — P1 drywet-analog ("how much"); rate moved to CC23
          ; per the 2026-07-13 tremolo v2 remap
          $ENC_ROT(gkTremDepth'0.02'0'1)
        elseif (gkLastToggle == 54) then
          ; OCTAVER sub level (P1, home BASS s3) — the "SUB" knob
          $ENC_ROT(gkOCT_sub'0.02'0'1)
        elseif (gkLastToggle == 71) then
          ; SHIMMER mix (P1, home DIGIVERB s3)
          $ENC_ROT(gkShmMix'0.02'0'1)
        ; --- P4+ fills (CC22 is leftmost; CC21 is next) ---
        elseif (gkLastToggle == 61) then
          ; TRACK SYNTH detune (P6 fill; home = SYNTH s0)
          $ENC_ROT(gkTSY_det'0.02'0'1)
        elseif (gkLastToggle == 17) then
          ; MOOD max delay (P4 fill) — home=CC22, so CC21 is first fill
          $ENC_ROT(gkMOOD_Dly'0.02'0'1)
        elseif (gkLastToggle == 35) then
          ; BigMuff tone (P4 fill) — home=CC22, so CC21 is first fill
          $ENC_ROT(gkMUF_tone'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 55) then
          ; ENV FILTER attack shape (P4 fill; home = BASS s4)
          $ENC_ROT(gkENV_att'0.02'0'1)
        elseif (gkLastToggle == 47) then
          ; RESONATOR freeze latch (P5 fill; home = FILTERING s1)
          $ENC_ROT(gkRES_frz'1'0'1)
        elseif (gkLastToggle == 69) then
          ; POLY DLY pitch-envelope Attack (fraction of T), 0..1
          $ENC_ROT(gkMvdAtt'0.02'0'1)
        endif
      endif


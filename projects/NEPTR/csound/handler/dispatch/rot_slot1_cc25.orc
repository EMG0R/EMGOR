    ;==========================================================
    ; SLOT 1 ENCODER ROTATION — CC25 (Enc6 rot)
    ; Accumulator = amount for slot 1 effects
    ;==========================================================
    elseif (kCC == 25) then
      $LP_CH2_SPEED
      elseif (gkCurrentMenu == $MX_MENU) then
        ; MIXER page (mixer_page.orc): primary encoder = channel volume.
        $MX_VOL_ROT(3)
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 1 (PS STEPS) — accumulate through the
        ; lane's preset list on the Bela. 64 is the encoder's no-movement
        ; value; sending on it would re-engage the lane on every idle report.
        if (kVal != 64) then
          $HG_ROTATE(1'(kVal > 64 ? 1 : -1))
        endif
      elseif (gkCurrentMenu == 17) then
        ; NAM row 3 (TWEED) variant cycle: soft -> dirty, wraps
        if (kVal > 64) then
          gkNAMRowSel[3] = gkNAMRowSel[3] + 1
          if (gkNAMRowSel[3] >= giNAMRowCount[3]) then
            gkNAMRowSel[3] = 0
          endif
        elseif (kVal < 64) then
          gkNAMRowSel[3] = gkNAMRowSel[3] - 1
          if (gkNAMRowSel[3] < 0) then
            gkNAMRowSel[3] = giNAMRowCount[3] - 1
          endif
        endif
        if (gkNAMActiveRow == 3 && gkNAMBypass < 0.5) then
          gkNAMProfileIdx = giNAMRowBase[3] + gkNAMRowSel[3]
        endif
        printks "NAM row 3 -> variant %d\n", 0, gkNAMRowSel[3]
      elseif (gkCurrentMenu == 18) then
        ; IR row 1 (PLATE) variant cycle
        if (kVal > 64) then
          gkIRSel[1] = gkIRSel[1] + 1
          if (gkIRSel[1] >= giIRRowCount[1]) then
            gkIRSel[1] = 0
          endif
        elseif (kVal < 64) then
          gkIRSel[1] = gkIRSel[1] - 1
          if (gkIRSel[1] < 0) then
            gkIRSel[1] = giIRRowCount[1] - 1
          endif
        endif
        printks "IR row 1 -> variant %d\n", 0, gkIRSel[1]
      elseif (gkCurrentMenu != 0) then
        ; --- P1 (effects whose home slot = 1) ---
        ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
        ; chain. Rotation 25 = hero param P2 of the focused effect.
        if (gkLastToggle >= 75 && gkLastToggle <= 98) then
          SM_RotStep gkLastToggle, 1, kVal
        elseif (gkLastToggle == 11) then
          $ENC_ROT(gkARModDepth'0.02'0'1)
        elseif (gkLastToggle == 3) then
          $ENC_ROT(gkDryPctPS'5'0'100)
        elseif (gkLastToggle == 20) then
          ; BBcut dry/wet — home moved to GLITCH slot 1 (2026-07-13)
          $ENC_ROT(gkBBcut_drywet'gkStepBBcut_drywet'0'1)
        elseif (gkLastToggle == 7) then
          $ENC_ROT(gkChorusMix'gkStepChorusMix'0'1)
        elseif (gkLastToggle == 32) then
          $ENC_ROT(gkMAR_drive'gkStepDriveTone'0'1)
        elseif (gkLastToggle == 45) then
          ; ONWARD sampled-layer level (P1, home CHASE BLISS s1)
          $ENC_ROT(gkONW_mix'0.02'0'1)
        elseif (gkLastToggle == 47) then
          ; RESONATOR mix (P1, home FILTERING s1)
          $ENC_ROT(gkRES_mix'0.02'0'1)
        elseif (gkLastToggle == 41) then
          ; BASS PRE HEAVY drive (P1, BASS menu s1)
          $ENC_ROT(gkBPH_drive'0.02'0'1)
        elseif (gkLastToggle == 62) then
          ; PLL FUZZ dry/wet (P1, home SYNTH s1)
          $ENC_ROT(gkPLL_mix'0.02'0'1)
        elseif (gkLastToggle == 64) then
          ; DIFFUSION dry/wet (P1, home AIR s1)
          $ENC_ROT(gkDIF_wet'0.02'0'1)
        elseif (gkLastToggle == 73) then
          ; GRAINVERB mix (P1, home DIGIVERB s1)
          $ENC_ROT(gkGvbMix'0.02'0'1)
        ; (NAM IN gain used to sit here. Moved to P2/CC20 2026-08-11 — same
        ;  reason as OUT gain above.)
        ; --- P4+ fills ---
        elseif (gkLastToggle == 15) then
          ; DELAY mode NORMAL/REVERSE (P5 fill, stepped; home = TIME s0)
          $ENC_ROT(gkDlyMode'1'0'1)
        elseif (gkLastToggle == 57) then
          ; COMPRESSOR gate (P5 fill; home = PREAMP s4)
          $ENC_ROT(gkCMP_gate'0.02'0'1)
        elseif (gkLastToggle == 58) then
          ; TONE EQ high shelf (P5 fill; home = SYNTH s2)
          $ENC_ROT(gkTEQ_high'0.02'0'1)
        elseif (gkLastToggle == 61) then
          ; TRACK SYNTH glide (P4 fill; home = SYNTH s0)
          $ENC_ROT(gkTSY_glide'0.02'0'1)
        elseif (gkLastToggle == 63) then
          ; FEEDBACKER vibrato depth (P4 fill; home = AIR s0)
          $ENC_ROT(gkFBK_vib'0.02'0'1)
        elseif (gkLastToggle == 2) then
          ; ATTACK v2 detector voicing: bass .. guitar (P5 fill)
          $ENC_ROT(gkATTK_range'gkStepATTK_range'0'1)
        elseif (gkLastToggle == 14) then
          ; AutoMoog dist (P5 fill; home moved to FILTERING s3)
          $ENC_ROT(gkAUTOMOOG_dist'0.02'0'1)
        elseif (gkLastToggle == 56) then
          ; BLOOPER grain SIZE x0.5..x2.0 (P4 fill)
          ; home = CHASE BLISS s0, so s1/CC25 is its first fill)
          $ENC_ROT(gkBLP_Size'0.03'0'1)
        elseif (gkLastToggle == 23) then
          ; REPEAT length (P4 fill) — stepped 0=1/2 tick 1=1 tick 2=2 ticks
          $ENC_ROT(gkRPT_len'1'0'2)
        elseif (gkLastToggle == 24) then
          ; CHOP gate shape (P4 fill) — 0 choppy/short .. 1 long/pumping
          $ENC_ROT(gkCHP_shape'0.02'0'1)
        elseif (gkLastToggle == 48) then
          ; IMPULSE tone (P4 fill) — filter cutoff base, moved from CC20/P2
          $ENC_ROT(gkIMP_tone'0.02'0'1)
        elseif (gkLastToggle == 68) then
          ; TAPE DELAY loop-filter WIDTH/Q (P5 fill; home = DELAY s4/CC22) —
          ; 0.5 wide/subtle .. 6 narrow/resonant
          $ENC_ROT(gkTdlFiltQ'0.1'0.5'6)
#include "handler/dispatch/gen/expr_rot_25.orc"
        endif
      endif


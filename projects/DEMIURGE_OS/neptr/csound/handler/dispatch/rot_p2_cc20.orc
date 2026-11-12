    ;==========================================================
    ; SECONDARY PARAM — CC20 (Enc1 rot)
    ; Dispatched by gkLastToggle
    ; Glitch group: shared BPM
    ;==========================================================
    elseif (kCC == 20) then

      ; PER-PRESET OUTPUT TRIM — menu 0 ONLY. CC20 nudges the ACTIVE preset's
      ; stored gain (giPresetTrim[gkCurrentPreset]) by 0.5 dB per detent,
      ; clamped to +/-6 dB. Applied at the master stage in signal_chain.orc.
      ; Dry (preset -1) has no trim. Session-only (table lives in RAM).
      ; 1.05925 = ampdb(0.5); 1.99526 = ampdb(6); 0.50119 = ampdb(-6).
      ; Menu 0 has no lastToggle-driven CC20 target, so this steals nothing.
      if (gkCurrentMenu == 0) then
        if (gkCurrentPreset >= 0) then
          kTrimIdx = gkCurrentPreset
          kTrimCur tab kTrimIdx, giPresetTrim
          if (kVal > 64) then
            kTrimCur = min(kTrimCur * 1.05925, 1.99526)
          elseif (kVal < 64) then
            kTrimCur = max(kTrimCur / 1.05925, 0.50119)
          endif
          tabw kTrimCur, kTrimIdx, giPresetTrim
          printks "PRESET TRIM: preset %d -> %5.3f\n", 0, gkCurrentPreset, kTrimCur
        endif

      ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, before the per-id
      ; chain. Rotation 20 = hero param P6 of the focused effect.
      elseif (gkLastToggle >= 75 && gkLastToggle <= 98) then
        SM_RotStep gkLastToggle, 5, kVal
      ; Glitch group — shared BPM (any glitch lastToggle)
      elseif (gkLastToggle == 66) then
        ; MOOD II CLOCK (P2) — tonal ladder position, 0.5 = home/root, 0/1 =
        ; 3 octaves down/up (see mood2.orc header for the ladder)
        $ENC_ROT(gkMD2_Clock'0.02'0'1)
      ; GLITCH v2 (2026-07-15): the tick now comes from the CHASE BLISS clock;
      ; the menu has no shared BPM of its own any more. P2 = per-effect
      ; PROBABILITY, the uniform convention for the whole menu (stutter.orc
      ; INTEGRATION NOTES item C).
      elseif (gkLastToggle == 20) then
        $ENC_ROT(gkBBcut_prob'0.05'0'1)
      elseif (gkLastToggle == 23) then
        $ENC_ROT(gkRPT_prob'0.05'0'1)
      elseif (gkLastToggle == 24) then
        $ENC_ROT(gkCHP_prob'0.05'0'1)
      elseif (gkLastToggle == 25) then
        $ENC_ROT(gkRVS_prob'0.05'0'1)
      elseif (gkLastToggle == 2) then
        ; ATTACK v2 swell time (the POG attack slider) — exponential feel
        $ENC_ROT_EXP(gkATTK_atkMs'1.12'10'2000)
      elseif (gkLastToggle == 3) then
        ; Old PS semitones (mix now on slot encoder CC25)
        $ENC_ROT(gkSemi'0.5'-12'12)
      elseif (gkLastToggle == 4) then
        ; GPS semitones (mix now on slot encoder CC26)
        $ENC_ROT(gkGPSsemis'0.5'-12'12)
      elseif (gkLastToggle == 5) then
        ; Formant shift (mix now on slot encoder CC24)
        $ENC_ROT(gkFormantShift'0.5'-12'12)
      elseif (gkLastToggle == 28) then
        ; FORMS — clean formant shifter
        $ENC_ROT(gkFormsShift'0.5'-12'12)
      elseif (gkLastToggle == 6) then
        ; 2026-06-23 Vibrato speed — exponential feel
        $ENC_ROT_EXP(gkVIB_freq'1.10'0.05'12)
      elseif (gkLastToggle == 7) then
        ; 2026-06-23 Chorus rate — exponential feel
        $ENC_ROT_EXP(gkChorusRate'1.10'0.05'8)
      elseif (gkLastToggle == 8) then
        ; 2026-06-23 Phaser rate — exponential feel
        $ENC_ROT_EXP(gkPHA_rate'1.12'0.05'12)
      elseif (gkLastToggle == 9) then
        ; 2026-06-23 Flanger rate — exponential feel (was crawling on a linear step)
        $ENC_ROT_EXP(gkFLA_rate'1.12'0.05'12)
      elseif (gkLastToggle == 11) then
        ; 2026-06-23 Ring Mod carrier frequency (P2) — exponential; depth is now P1
        if (kVal > 64) then
          gkARModFreq = min(gkARModFreq * 1.1, 250)
        elseif (kVal < 64) then
          gkARModFreq = max(gkARModFreq / 1.1, 0.05)
        endif
      elseif (gkLastToggle == 14) then
        $ENC_ROT(gkAUTOMOOG_res'0.02'0'0.95)
      elseif (gkLastToggle == 15) then
        ; Delay time
        $ENC_ROT(gkDelayMS'10000'0'700000)
      elseif (gkLastToggle == 16) then
        ; Spectral Delay speed (maxdel)
        $ENC_ROT(gkSpecDelMaxDel'gkStepSpecDelMaxDel'0'1)
      elseif (gkLastToggle == 17) then
        ; MOOD clock
        $ENC_ROT(gkMOOD_Clock'0.02'0'1)
      elseif (gkLastToggle == 19) then
        ; Reverb room/FB
        $ENC_ROT(gkReverbFB'gkStepReverbFB'0'0.999)
      elseif (gkLastToggle == 21) then
        ; TAPE fail (drop/crinkle density)
        $ENC_ROT(gkTapeFail'0.02'0'1)
      elseif (gkLastToggle == 22) then
        ; 2026-06-23 WAH resonance (P2)
        $ENC_ROT(gkWAH_res'0.01'0.5'0.97)
      elseif (gkLastToggle == 26) then
        ; H90 semitones (mix now on slot encoder CC21)
        $ENC_ROT(gkSemiH90'0.5'-12'12)
      elseif (gkLastToggle == 27) then
        ; PFORK steps through quantized intervals {-12,-9,-7,-5,+5,+7,+9,+12}.
        ; Step 2.5 covers all gap widths so one tick lands on (or past) the
        ; next allowed value; the at-rate quantizer in pitchfork.orc snaps
        ; to the nearest member of the set.
        $ENC_ROT(gkSemiPF'2.5'-12'12)
      elseif (gkLastToggle == 31) then
        ; OD wildcard 1: BODY (hump corner — higher = fatter)
        $ENC_ROT(gkOD_wc1'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 32) then
        ; MARSHALL wildcard 1: presence
        $ENC_ROT(gkMAR_wc1'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 33) then
        ; DHG wildcard 1: SLO <-> 5150 blend (top 5% = series stack)
        $ENC_ROT(gkDHG_wc1'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 34) then
        ; RMZ wildcard 1: mid-sweep frequency (200 Hz .. 5 kHz)
        $ENC_ROT(gkRMZ_wc1'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 35) then
        ; MUF wildcard 1: mid recovery (un-scoop)
        $ENC_ROT(gkMUF_wc1'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 36) then
        ; HABIT size (echo time, exp 50 ms .. 60 s inside the instr)
        $ENC_ROT(gkHabitSize'gkStepHabitSize'0'1)
      elseif (gkLastToggle == 10) then
        ; Leslie rotation speed — exponential feel (0.1..10 Hz)
        $ENC_ROT_EXP(gkLeslieSpeed'1.12'0.1'10)
      elseif (gkLastToggle == 43) then
        ; Tremolo MODE stepper (v2 remap 2026-07-13): per-detent with WRAP,
        ; NAM-variant convention. 0=SINE 1=OPTO 2=BIAS 3=HARM. (Depth moved
        ; to P1/P4; rate moved to CC23.)
        if (kVal > 64) then
          gkTremMode = gkTremMode + 1
          gkTremMode = (gkTremMode > 3 ? 0 : gkTremMode)
        elseif (kVal < 64) then
          gkTremMode = gkTremMode - 1
          gkTremMode = (gkTremMode < 0 ? 3 : gkTremMode)
        endif
      elseif (gkLastToggle == 18) then
        ; Granular density (P2) — grains/sec, coarse step
        $ENC_ROT(gkGranDens'10'10'400)
      elseif (gkLastToggle == 45) then
        ; ONWARD BLEND (P2) — 0 = Freeze pad .. 1 = Glitch repeats
        $ENC_ROT(gkONW_blend'0.02'0'1)
      elseif (gkLastToggle == 46) then
        ; LOSSY loss (P2) — spectral gate depth
        $ENC_ROT(gkLSY_loss'0.02'0'1)
      elseif (gkLastToggle == 47) then
        ; RESONATOR onset (P2) — excitation gate attack/character
        $ENC_ROT(gkRES_onset'0.02'0'1)
      elseif (gkLastToggle == 48) then
        ; IMPULSE prob (P2) — menu-wide P2=prob rule (impulse_synth.orc
        ; INTEGRATION NOTES); tone moved to CC25 P4 fill.
        $ENC_ROT(gkIMP_prob'0.05'0'1)
      elseif (gkLastToggle == 54) then
        ; OCTAVER tone (P2)
        $ENC_ROT(gkOCT_tone'0.02'0'1)
      elseif (gkLastToggle == 55) then
        ; ENV FILTER sensitivity (P2)
        $ENC_ROT(gkENV_sens'0.02'0'1)
      elseif (gkLastToggle == 56) then
        ; BLOOPER auto-motion depth (P2) — how much the scatter drifts on its
        ; own. Layers/undo are gone with the rearchitecture (BLOOPER owns no
        ; buffer any more), so this detent row is free for the motion knob.
        $ENC_ROT(gkBLP_Motion'0.03'0'1)
      elseif (gkLastToggle == 37) then
        ; NAM INPUT gain (P2, pre-NAMProcess drive trim) — dB, 0.5 dB/detent.
        ; Lives on P2/CC20 (a MENU-encoder rotation), NOT on a slot rotation:
        ; menu 17 hands every slot rotation to the NAM variant browse, so a
        ; slot-rotation home would be dead in the amp sim's own menu — which
        ; is exactly where the UI draws this slider. CC20/CC23 are the only
        ; two rotations no menu shadows. Unity = 0 dB.
        ; 2026-09-01: swapped In<->Out per user (physical encoders were
        ; reversed). P2/CC20 now drives OUTPUT gain.
        $ENC_ROT(gkNAMOutGain'0.5'-18'18)
      elseif (gkLastToggle == 49) then
        ; IR VERB row 0 dry/wet (P2) — see the NAM note above: menu 18 owns
        ; every slot rotation for the IR variant browse, so the dry/wet has
        ; to live on a menu-encoder rotation to be reachable from the IR menu.
        $ENC_ROT(gkIRRowMix[0]'0.02'0'1)
      elseif (gkLastToggle == 50) then
        ; IR VERB row 1 dry/wet (P2)
        $ENC_ROT(gkIRRowMix[1]'0.02'0'1)
      elseif (gkLastToggle == 51) then
        ; IR VERB row 2 dry/wet (P2)
        $ENC_ROT(gkIRRowMix[2]'0.02'0'1)
      elseif (gkLastToggle == 52) then
        ; IR VERB row 3 dry/wet (P2)
        $ENC_ROT(gkIRRowMix[3]'0.02'0'1)
      elseif (gkLastToggle == 53) then
        ; IR VERB row 4 dry/wet (P2)
        $ENC_ROT(gkIRRowMix[4]'0.02'0'1)
      elseif (gkLastToggle == 57) then
        ; COMPRESSOR speed (P2) — 0 FET fast .. 1 opto slow
        $ENC_ROT(gkCMP_speed'0.02'0'1)
      elseif (gkLastToggle == 58) then
        ; TONE EQ mid frequency (P2) — 200 Hz..3 kHz exp inside the instr
        $ENC_ROT(gkTEQ_freq'0.02'0'1)
      elseif (gkLastToggle == 60) then
        ; CHAIN FB loop gain (P2) — >0.67 = loop gain >1 = self-oscillation
        $ENC_ROT(gkCFB_gain'0.02'0'1)
      elseif (gkLastToggle == 61) then
        ; TRACK SYNTH cutoff (P2) — exp inside the DSP
        $ENC_ROT(gkTSY_cut'0.02'0'1)
      elseif (gkLastToggle == 62) then
        ; PLL FUZZ interval (P2) — stepped int 0..4 = /4 /2 x1 x2 x3
        $ENC_ROT(gkPLL_intv'1'0'4)
      elseif (gkLastToggle == 63) then
        ; FEEDBACKER harmonic (P2) — stepped x4 (.25/detent):
        ; 0 fund / .25 +oct / .5 +oct+5th / .75 +2oct
        $ENC_ROT(gkFBK_harm'0.25'0'0.75)
      elseif (gkLastToggle == 64) then
        ; DIFFUSION depth (P2) — scales times, gains AND live stage count
        $ENC_ROT(gkDIF_depth'0.02'0'1)
      elseif (gkLastToggle == 65) then
      ; 2026-07-26 delay/verb expansion — P2 (CC20, always)
      elseif (gkLastToggle == 67) then
        ; BINSCATTER scatter amount (P2) — 0=equal bins, 1=full random spread
        $ENC_ROT(gkBscScatter'0.02'0'1)
      elseif (gkLastToggle == 68) then
        ; TAPE DELAY time (P2) — head-1 target, ms
        $ENC_ROT(gkTdlTimeMs'15'60'1200)
      elseif (gkLastToggle == 69) then
        ; POLY DLY Num L (P2) — integer 1..16
        $ENC_ROT(gkMvdNumL'1'1'16)
      elseif (gkLastToggle == 70) then
        ; NEBULA density/mod (P2) — the "identity knob"
        $ENC_ROT(gkNebDensity'0.02'0'1)
      elseif (gkLastToggle == 71) then
        ; SHIMMER interval (P2) — stepped 4 positions (see kIdx in
        ; shimmerverb.orc: 0=+12st 1=+7st 2=+5st 3=-12st)
        $ENC_ROT(gkShmInterval'0.3334'0'1)
      elseif (gkLastToggle == 72) then
        ; FREEZE damp (P2)
        $ENC_ROT(gkFrzDamp'0.02'0'1)
      elseif (gkLastToggle == 73) then
        ; GRAINVERB spacing/subdiv (P2)
        $ENC_ROT(gkGvbSpacing'0.02'0'1)
      endif
      ; NAM (37) is toggle-only — profiles chosen by the 5 slot buttons


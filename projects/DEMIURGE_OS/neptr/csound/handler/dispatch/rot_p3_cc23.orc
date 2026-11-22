    ;==========================================================
    ; TERTIARY PARAM — CC23 (Enc4 rot)
    ; Dispatched by gkLastToggle
    ;==========================================================
    elseif (kCC == 23) then

      ; SOUNDMAGIC rack (toggle ids 75..98): ONE generic branch, BEFORE the per-id
      ; chain. Rotation 23 = hero param P7 of the focused effect.
      if (gkLastToggle >= 75 && gkLastToggle <= 98) then
        SM_RotStep gkLastToggle, 6, kVal
      elseif (gkLastToggle == 2) then
        ; ATTACK v2 detection sensitivity (P3)
        $ENC_ROT(gkATTK_sens'gkStepATTK_sens'0'1)
      elseif (gkLastToggle == 66) then
        ; MOOD II LENGTH (P3) — SLIP window size, ~10 ms .. ~1.0 s
        $ENC_ROT(gkMD2_Length'0.02'0'1)
      elseif (gkLastToggle == 4) then
        $ENC_ROT(gkGPSfback'0.02'0'0.99)
      elseif (gkLastToggle == 6) then
        ; Vibrato feedback
        $ENC_ROT(gkVIB_fback'gkStepVIB_fback'0'0.7)
      elseif (gkLastToggle == 7) then
        ; Chorus depth
        $ENC_ROT(gkChorusDepth'gkStepChorusDepth'0'1)
      elseif (gkLastToggle == 8) then
        ; Phaser stages
        $ENC_ROT(gkPHA_stages'1'1'24)
      elseif (gkLastToggle == 9) then
        ; Flanger feedback
        $ENC_ROT(gkFLA_fback'gkStepFLA_fback'-1'1)
      elseif (gkLastToggle == 14) then
        ; 2026-06-23 AutoMoog filter type (P3) — stepped 1..4
        $ENC_ROT(gkAUTOMOOG_type'1'1'4)
      elseif (gkLastToggle == 15) then
        $ENC_ROT(gkFeedback'gkStepFeedback'0'1.2)
      elseif (gkLastToggle == 16) then
        ; Spectral Delay amount/length (FB)
        $ENC_ROT(gkSpecDelFB'gkStepSpecDelFB'0'1)
      elseif (gkLastToggle == 17) then
        ; MOOD density
        $ENC_ROT(gkMOOD_Dens'gkStepMOOD_Dens'0'1400)
      elseif (gkLastToggle == 19) then
        ; 2026-06-23 Reverb damping/brightness (P3)
        $ENC_ROT(gkReverbDamp'0.02'0'1)
      elseif (gkLastToggle == 20) then
        ; 2026-06-23 BBcut resonant filter mix (P3)
        $ENC_ROT(gkBBcut_fltMix'0.02'0'1)
      elseif (gkLastToggle == 21) then
        ; TAPE model morph (reel -> cassette -> broken)
        $ENC_ROT(gkTapeModel'0.02'0'1)
      elseif (gkLastToggle == 23) then
        ; REPEAT character (P3) — 0 clean flat repeats .. 1 decaying/mangled
        $ENC_ROT(gkRPT_char'0.02'0'1)
      elseif (gkLastToggle == 24) then
        ; CHOP rate bias (P3) — 0 slow halves .. 1 fast buzz
        $ENC_ROT(gkCHP_rate'0.02'0'1)
      elseif (gkLastToggle == 25) then
        ; RVRS length (P3) — 0 tight 1-tick flips .. 1 = 2-tick + speed weird
        $ENC_ROT(gkRVS_len'0.02'0'1)
      elseif (gkLastToggle == 31) then
        ; OD wildcard 2: clean blend (Klon parallel dry path)
        $ENC_ROT(gkOD_wc2'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 32) then
        ; MARSHALL wildcard 2: tightness (pre-clip low-cut)
        $ENC_ROT(gkMAR_wc2'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 33) then
        ; DHG wildcard 2: tightness (global low-cut)
        $ENC_ROT(gkDHG_wc2'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 34) then
        ; RMZ wildcard 2: mid gain (cut <-> boost)
        $ENC_ROT(gkRMZ_wc2'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 35) then
        ; MUF wildcard 2: voicing morph Triangle / Ram's Head / Fallout
        $ENC_ROT(gkMUF_wc2'gkStepDriveWC'0'1)
      elseif (gkLastToggle == 36) then
        ; HABIT repeats (feedback; max = stable infinite build-up)
        $ENC_ROT(gkHabitRepeats'gkStepHabitRepeats'0'1)
      elseif (gkLastToggle == 18) then
        ; Granular grain size (P3) — max grain length in seconds
        $ENC_ROT(gkGranSize'0.05'0.05'1.2)
      elseif (gkLastToggle == 43) then
        ; Tremolo RATE (P3) — exponential feel (v2 remap: was the home-slot
        ; P1 rotation; depth is P1 now)
        $ENC_ROT_EXP(gkTremRate'1.10'0.5'20)
      elseif (gkLastToggle == 45) then
        ; ONWARD SIZE (P3) — sample length + master timing
        $ENC_ROT(gkONW_size'0.02'0'1)
      elseif (gkLastToggle == 46) then
        ; LOSSY bandwidth (P3)
        $ENC_ROT(gkLSY_bw'0.02'0'1)
      elseif (gkLastToggle == 47) then
        ; RESONATOR feedback (P3)
        $ENC_ROT(gkRES_fb'0.02'0'1)
      elseif (gkLastToggle == 48) then
        ; IMPULSE decay (P3)
        $ENC_ROT(gkIMP_dec'0.02'0'1)
      elseif (gkLastToggle == 54) then
        ; OCTAVER character (P3)
        $ENC_ROT(gkOCT_char'0.02'0'1)
      elseif (gkLastToggle == 55) then
        ; ENV FILTER mode morph (P3)
        $ENC_ROT(gkENV_mode'0.02'0'1)
      elseif (gkLastToggle == 56) then
        ; BLOOPER MOD AMOUNT — event density, the main knob (P3)
        $ENC_ROT(gkBLP_Scat'0.03'0'1)
      elseif (gkLastToggle == 37) then
        ; NAM INPUT gain (P3) — 2026-09-01: swapped In<->Out per user
        ; (physical encoders were reversed). P3/CC23 now drives INPUT gain.
        $ENC_ROT(gkNAMInGain'0.5'-12'18)
      elseif (gkLastToggle >= 49 && gkLastToggle <= 53) then
        ; IR VERB master wet tone (P3) — 0 dark .. 1 open. Moved off P2
        ; 2026-08-11 so the per-row dry/wet could take the always-reachable
        ; P2 rotation; the master wet LEVEL trim took damp's place on the
        ; slot rotation (P1).
        $ENC_ROT(gkIRDamp'0.02'0'1)
      elseif (gkLastToggle == 57) then
        ; COMPRESSOR makeup gain (P3) — 0..+12 dB inside the instr
        $ENC_ROT(gkCMP_makeup'0.02'0'1)
      elseif (gkLastToggle == 58) then
        ; TONE EQ tilt (P3) — 0.5 = flat
        $ENC_ROT(gkTEQ_tilt'0.02'0'1)
      elseif (gkLastToggle == 60) then
        ; CHAIN FB loop tone tilt (P3) — 0 dark LP .. 0.5 open .. 1 thin HP
        $ENC_ROT(gkCFB_tone'0.02'0'1)
      elseif (gkLastToggle == 61) then
        ; TRACK SYNTH resonance (P3)
        $ENC_ROT(gkTSY_res'0.02'0'1)
      elseif (gkLastToggle == 62) then
        ; PLL FUZZ lock speed (P3) — 0 chaos/hunting .. 1 tight
        $ENC_ROT(gkPLL_lock'0.02'0'1)
      elseif (gkLastToggle == 63) then
        ; FEEDBACKER bloom (P3) — 0.2..4 s onset inside the DSP
        $ENC_ROT(gkFBK_bloom'0.02'0'1)
      elseif (gkLastToggle == 64) then
        ; DIFFUSION mod rate (P3) — 0.08..2.6 Hz exp inside the DSP
        $ENC_ROT(gkDIF_rate'0.02'0'1)
      elseif (gkLastToggle == 65) then
      ; 2026-07-26 delay/verb expansion — P3 (CC23, always)
      elseif (gkLastToggle == 67) then
        ; BINSCATTER feedback (P3) — soft-clamped 0.5-1.1 inside the instr
        $ENC_ROT(gkBscFB'0.02'0.5'1.1)
      elseif (gkLastToggle == 68) then
        ; TAPE DELAY feedback (P3) — >1 = emergent self-osc, self-limited
        $ENC_ROT(gkTdlFeedback'0.02'0'1.15)
      elseif (gkLastToggle == 69) then
        ; POLY DLY Den L (P3) — integer 1..16
        $ENC_ROT(gkMvdDenL'1'1'16)
      elseif (gkLastToggle == 70) then
        ; NEBULA size/decay (P3)
        $ENC_ROT(gkNebSize'0.02'0'1)
      elseif (gkLastToggle == 71) then
        ; SHIMMER loop feedback / bloom amount (P3)
        $ENC_ROT(gkShmFbAmt'0.02'0'1)
      elseif (gkLastToggle == 72) then
        ; FREEZE listen-send gain (P3) — 0.3..1.5x actual, applied inside
        ; freezeverb.orc; the global itself is normalized 0..1
        $ENC_ROT(gkFrzListenGain'0.02'0'1)
      elseif (gkLastToggle == 73) then
        ; GRAINVERB decay+damp (P3)
        $ENC_ROT(gkGvbDecay'0.02'0'1)
      endif


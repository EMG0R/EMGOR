; EXPR DESTINATION TABLE — the SOURCE OF TRUTH for the expression system.
;
; This file is INPUT to tools/gen_expr_registry.py, which turns it into
; handler/expr_registry.orc (the UDO ExprSet) and handler/expr_targets.json
; (the machine-readable table the Python router maps sensors onto).
; It is NOT #included by the orchestra. Editing it and re-running the
; generator is the only supported way to change what an expression source does.
;
; Bodies are lifted VERBATIM, so write ordinary csound here and keep the
; incoming value named `kNorm`. Bodies may be compound — id 26 branches on
; gkPresetBank, id 10 writes two globals — and are preserved as written.
;
; `id` is a toggle id: the value gkLastToggle29 takes when an effect claims the
; expression source. Adding a destination = adding a branch here, setting
; gkLastToggle29 = <id> in that effect's toggle-ON branch, and regenerating.
;
; Extracted 2026-08-12 from handler/dispatch/expr_cc29.orc, where this chain
; lived inline and could therefore only ever be driven by the pedal.

      if (gkLastToggle29 == 30) then
        gkMasterVol = kNorm
      ; GLITCH v2 (2026-07-15): per-effect instant-trigger targets — expression
      ; pedal = instant trigger (stutter.orc INTEGRATION NOTES item F). No
      ; branch reads gkGLITCH_chance any more (globals.orc keeps the decl;
      ; the CHASE BLISS clock still writes gkGLITCH_bpm).
      elseif (gkLastToggle29 == 20) then
        gkBBcut_expr = kNorm
      elseif (gkLastToggle29 == 23) then
        gkRPT_expr = kNorm
      elseif (gkLastToggle29 == 24) then
        gkCHP_expr = kNorm
      elseif (gkLastToggle29 == 25) then
        gkRVS_expr = kNorm
      elseif (gkLastToggle29 == 48) then
        gkIMP_expr = kNorm
      elseif (gkLastToggle29 == 1) then
        gkVOLpedal = kNorm
      elseif (gkLastToggle29 == 21) then
        ; TAPE transport: heel = play, toe = stop (spin-up on release)
        gkTapeExpr = kNorm
      elseif (gkLastToggle29 == 3) then
        gkSemi = kNorm * 24 - 12
      elseif (gkLastToggle29 == 4) then
        gkGPSsemis = kNorm * 24 - 12
      elseif (gkLastToggle29 == 5) then
        gkFormantShift = kNorm * 24 - 12
      elseif (gkLastToggle29 == 28) then
        gkFormsShift = kNorm * 24 - 12
      elseif (gkLastToggle29 == 38) then
        ; BIN REDUCTION contrast: heel (0) = extreme crush, toe (1) = transparent/emphasis
        gkBinPedal = kNorm
      elseif (gkLastToggle29 == 44) then
        ; BINCRUSHER coarsening: heel (0) = transparent, toe (1) = max coarsening
        gkBcPedal = kNorm
      elseif (gkLastToggle29 == 26) then
        ; H90 expression sweep. Bass mode = up-only wammy (unison..+octave);
        ; surf/PITCH = full +/-12 (unchanged).
        if (gkPresetBank == 1) then
          gkSemiH90 = kNorm * 12
        else
          gkSemiH90 = kNorm * 24 - 12
        endif
      elseif (gkLastToggle29 == 27) then
        gkSemiPF = kNorm * 24 - 12
      elseif (gkLastToggle29 == 10) then
        ; Leslie speed
        gkLeslieSpeedAcc = kNorm
        gkLeslieSpeed = 0.1 + gkLeslieSpeedAcc * 9.9
      elseif (gkLastToggle29 == 11) then
        gkARModFreq = 0.05 * pow(200, kNorm) * 25
      elseif (gkLastToggle29 == 31) then
        ; OVERDRIVE drive (amount of distortion)
        gkOD_drive = kNorm
      elseif (gkLastToggle29 == 32) then
        ; MARSHALL drive
        gkMAR_drive = kNorm
      elseif (gkLastToggle29 == 33) then
        ; DUAL HIGH-GAIN drive
        gkDHG_drive = kNorm
      elseif (gkLastToggle29 == 34) then
        ; RAT × MZ drive
        gkRMZ_drive = kNorm
      elseif (gkLastToggle29 == 35) then
        ; BIG MUFF drive (sustain)
        gkMUF_drive = kNorm
      elseif (gkLastToggle29 == 17) then
        ; MOOD pitch/clock
        gkMOOD_Clock = 1 - kNorm
      elseif (gkLastToggle29 == 22) then
        ; WAH pedal position
        gkWAH_freq = kNorm
      elseif (gkLastToggle29 == 15) then
        ; Delay time (manual toggle, not preset)
        gkDelayMS = kNorm * 700000
      elseif (gkLastToggle29 == 36) then
        ; HABIT scan (auto-scan amount: how often + how deep into the memory)
        gkHabitScan = kNorm
      elseif (gkLastToggle29 == 45) then
        ; ONWARD FOLLOW — heel (0) = raw/unpitched replay, toe (1) = fully
        ; harmonic-locked to the live note. FOLLOW is the harmonic-rebuild's
        ; headline expression gesture; unlike MIX, the heel position can
        ; never mute the effect.
        gkONW_follow = kNorm
      elseif (gkLastToggle29 == 66) then
        ; MOOD II CLOCK scrub — the manual's headline move: play a note and
        ; sweep the clock while it decays. toe = 1.0 top of the tonal ladder
        ; (3 octaves up), heel = 0.0 bottom (3 octaves down), 0.5 = home/root
        ; (same toe/heel polarity as the rig's other CC29 claims).
        gkMD2_Clock = kNorm
      elseif (gkLastToggle29 == 61) then
        ; TRACK SYNTH filter cutoff
        gkTSY_cut = kNorm
      elseif (gkLastToggle29 == 63) then
        ; FEEDBACKER intensity push (more level + faster bloom)
        gkFBK_expr = kNorm
      elseif (gkLastToggle29 == 56) then
        ; BLOOPER: manual override of event probability + pitch, centred at
        ; rest (0.5 = pure generative, matching id 66's toe/heel polarity).
        ; Below rest scales density/pitch down from whatever the generative
        ; engine (gkBLP_Scat + motion) is producing; above rest scales it up,
        ; up to full every-tick coverage at 100%. See effects/blooper.orc
        ; PEDAL DESIGN (2026-08-23) for the full formula.
        gkBLP_expr = kNorm
      elseif (gkLastToggle29 == 69) then
        ; POLY DLY — expr pulls the per-repeat pitch shift toward unison
        ; (depth = 1 - expr; movedelay.orc reads gkMvdExpr directly, 0 = heel/neutral).
        gkMvdExpr = kNorm
      elseif (gkLastToggle29 == 70) then
        ; NEBULA — WARP (Supermassive-style): heel = neutral, toe stretches
        ; all FDN line lengths 1.0x -> 2.0x (tape-speed-style pitch smear).
        gkNebWarp = kNorm
      elseif (gkLastToggle29 == 72) then
        ; FREEZE — LISTEN: toe (>0.6) opens the send into the frozen loop
        ; (stacks new playing into the bed), heel (<0.4) closes it without
        ; clearing. Hysteresis + portk live inside freezeverb.orc.
        gkFrzExpr = kNorm
      elseif (gkLastToggle29 == 74) then
        ; HYPERGUITAR PITCH — the guitar's pitch shifter at 100% wet, with the
        ; pedal driving the interval directly. Heel/toe span the lane's range
        ; (default +/-12 semitones); the value is relayed to the Bela as
        ; /hg/pedal by instr 98, which is the ONLY consumer.
        ;
        ; This exists so the pedal CLAIMS on engage like every other effect.
        ; Before it, the engine forwarded the pedal to the guitar
        ; unconditionally, so the treadle drove the guitar AND whatever else
        ; owned it (menu-0 presets point it at target 30, gkMasterVol) at the
        ; same time -- reported as "the exp pedal is still on volume, it needs
        ; to come off volume like normal functionality".
        gkHgPedal = kNorm
      endif

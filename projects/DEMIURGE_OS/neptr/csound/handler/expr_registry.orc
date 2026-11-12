;==============================================================
; EXPR_REGISTRY.ORC — GENERATED. DO NOT HAND-EDIT.
;==============================================================
; Source:    handler/expr_targets.src.orc  (sha256 cab1cec05ee1fcfe)
; Generator: tools/gen_expr_registry.py
;
; ExprSet(kId, kNorm) writes the expression destination selected by kId.
; kId is a toggle id — the value gkLastToggle29 holds — so the pedal is
;     ExprSet gkLastToggle29, kNorm
; and an OSC /p write is the same call with an id chosen by the router.
; The pedal is no longer privileged: it is one source among many.
;
; Every branch body below is copied verbatim from the source block,
; comments included. Ranges are unchanged by construction. To alter a
; destination, edit the SOURCE block and re-run the generator.
;
; id 0 = no destination (dry, or a preset that declines the pedal).
; 36 destinations.
;==============================================================

opcode ExprSet, 0, kk
  kId, kNorm xin

  if (kId == 30) then
          gkMasterVol = kNorm
        ; GLITCH v2 (2026-07-15): per-effect instant-trigger targets — expression
        ; pedal = instant trigger (stutter.orc INTEGRATION NOTES item F). No
        ; branch reads gkGLITCH_chance any more (globals.orc keeps the decl;
        ; the CHASE BLISS clock still writes gkGLITCH_bpm).
  elseif (kId == 20) then
          gkBBcut_expr = kNorm
  elseif (kId == 23) then
          gkRPT_expr = kNorm
  elseif (kId == 24) then
          gkCHP_expr = kNorm
  elseif (kId == 25) then
          gkRVS_expr = kNorm
  elseif (kId == 48) then
          gkIMP_expr = kNorm
  elseif (kId == 1) then
          gkVOLpedal = kNorm
  elseif (kId == 21) then
          ; TAPE transport: heel = play, toe = stop (spin-up on release)
          gkTapeExpr = kNorm
  elseif (kId == 3) then
          gkSemi = kNorm * 24 - 12
  elseif (kId == 4) then
          gkGPSsemis = kNorm * 24 - 12
  elseif (kId == 5) then
          gkFormantShift = kNorm * 24 - 12
  elseif (kId == 28) then
          gkFormsShift = kNorm * 24 - 12
  elseif (kId == 38) then
          ; BIN REDUCTION contrast: heel (0) = extreme crush, toe (1) = transparent/emphasis
          gkBinPedal = kNorm
  elseif (kId == 44) then
          ; BINCRUSHER coarsening: heel (0) = transparent, toe (1) = max coarsening
          gkBcPedal = kNorm
  elseif (kId == 26) then
          ; H90 expression sweep. Bass mode = up-only wammy (unison..+octave);
          ; surf/PITCH = full +/-12 (unchanged).
          if (gkPresetBank == 1) then
            gkSemiH90 = kNorm * 12
          else
            gkSemiH90 = kNorm * 24 - 12
          endif
  elseif (kId == 27) then
          gkSemiPF = kNorm * 24 - 12
  elseif (kId == 10) then
          ; Leslie speed
          gkLeslieSpeedAcc = kNorm
          gkLeslieSpeed = 0.1 + gkLeslieSpeedAcc * 9.9
  elseif (kId == 11) then
          gkARModFreq = 0.05 * pow(200, kNorm) * 25
  elseif (kId == 31) then
          ; OVERDRIVE drive (amount of distortion)
          gkOD_drive = kNorm
  elseif (kId == 32) then
          ; MARSHALL drive
          gkMAR_drive = kNorm
  elseif (kId == 33) then
          ; DUAL HIGH-GAIN drive
          gkDHG_drive = kNorm
  elseif (kId == 34) then
          ; RAT × MZ drive
          gkRMZ_drive = kNorm
  elseif (kId == 35) then
          ; BIG MUFF drive (sustain)
          gkMUF_drive = kNorm
  elseif (kId == 17) then
          ; MOOD pitch/clock
          gkMOOD_Clock = 1 - kNorm
  elseif (kId == 22) then
          ; WAH pedal position
          gkWAH_freq = kNorm
  elseif (kId == 15) then
          ; Delay time (manual toggle, not preset)
          gkDelayMS = kNorm * 700000
  elseif (kId == 36) then
          ; HABIT scan (auto-scan amount: how often + how deep into the memory)
          gkHabitScan = kNorm
  elseif (kId == 45) then
          ; ONWARD FOLLOW — heel (0) = raw/unpitched replay, toe (1) = fully
          ; harmonic-locked to the live note. FOLLOW is the harmonic-rebuild's
          ; headline expression gesture; unlike MIX, the heel position can
          ; never mute the effect.
          gkONW_follow = kNorm
  elseif (kId == 66) then
          ; MOOD II CLOCK scrub — the manual's headline move: play a note and
          ; sweep the clock while it decays. toe = 1.0 top of the tonal ladder
          ; (3 octaves up), heel = 0.0 bottom (3 octaves down), 0.5 = home/root
          ; (same toe/heel polarity as the rig's other CC29 claims).
          gkMD2_Clock = kNorm
  elseif (kId == 61) then
          ; TRACK SYNTH filter cutoff
          gkTSY_cut = kNorm
  elseif (kId == 63) then
          ; FEEDBACKER intensity push (more level + faster bloom)
          gkFBK_expr = kNorm
  elseif (kId == 56) then
          ; BLOOPER: manual override of event probability + pitch, centred at
          ; rest (0.5 = pure generative, matching id 66's toe/heel polarity).
          ; Below rest scales density/pitch down from whatever the generative
          ; engine (gkBLP_Scat + motion) is producing; above rest scales it up,
          ; up to full every-tick coverage at 100%. See effects/blooper.orc
          ; PEDAL DESIGN (2026-08-23) for the full formula.
          gkBLP_expr = kNorm
  elseif (kId == 69) then
          ; POLY DLY — expr pulls the per-repeat pitch shift toward unison
          ; (depth = 1 - expr; movedelay.orc reads gkMvdExpr directly, 0 = heel/neutral).
          gkMvdExpr = kNorm
  elseif (kId == 70) then
          ; NEBULA — WARP (Supermassive-style): heel = neutral, toe stretches
          ; all FDN line lengths 1.0x -> 2.0x (tape-speed-style pitch smear).
          gkNebWarp = kNorm
  elseif (kId == 72) then
          ; FREEZE — LISTEN: toe (>0.6) opens the send into the frozen loop
          ; (stacks new playing into the bed), heel (<0.4) closes it without
          ; clearing. Hysteresis + portk live inside freezeverb.orc.
          gkFrzExpr = kNorm
  elseif (kId == 74) then
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

endop

    ;==========================================================
    ; EFFECT TOGGLES — CC74 (Enc5 button, slot 0, rightmost)
    ;==========================================================
    elseif (kCC == 74 && kVal == 127) then

      if (gkCurrentMenu == 0) then
        if (gkPresetBank == 0) then
          ; SURF: AMBIENT (rightmost)
          if (gkCurrentPreset == 3) then
            ; Already active → switch to TS preset (not dry)
            gkCurrentPreset = 1
            gkTremBypass    = 1
            gkPHA_bypass    = 1
            gkVIB_bypass  = 0
            gkVIB_freq    = giPresetVibFreq
            gkVIB_depth   = giPresetVibDepth
            gkVIB_fback   = giPresetVibFback
            gkBypassDelay = 0
            gkDelayMS     = giPresetDelMS
            gkFeedback    = giPresetDelFB
            gkDelMix      = giPresetDelMix
            gkOD_bypass   = 0
            gkOD_drive    = 0.75
            gkOD_tone     = 0.5
            gkOD_wc1      = 0.5
            gkOD_wc2      = 0
            gkDHG_bypass  = 1
            gkverbBypass  = 1
            gkVOLpedalBypass = 1
            gkMasterVol      = 1
            gkAmbBoost       = 1
            gkLastToggle29   = 30
            printks "PRESET: AMBIENT->OVERDRIVE\n", 0
          else
            gkCurrentPreset  = 3
            gkTremBypass     = 1
            gkPHA_bypass     = 1
            ; Vibrato + spring-delay base (unified preset values — see globals.orc)
            gkVIB_bypass  = 0
            gkVIB_freq    = giPresetVibFreq
            gkVIB_depth   = giPresetVibDepth
            gkVIB_fback   = giPresetVibFback
            gkBypassDelay = 0
            gkDelayMS     = giPresetDelMS
            gkFeedback    = giPresetDelFB
            gkDelMix      = giPresetDelMix
            ; Dirt channels: off
            gkOD_bypass   = 1
            gkDHG_bypass  = 1
            ; Reverb: spring-like
            gkverbBypass  = 0
            gkReverbFB    = 0.92
            gkverbMix     = 0.70
            ; Volume: master volume + ambient boost. CC29 sweeps gkMasterVol 0..1;
            ; gkAmbBoost (2.6x) stacks on top for gain parity with distortion presets.
            gkVOLpedalBypass = 1
            gkMasterVol      = 1
            gkAmbBoost       = 2.6
            gkLastToggle29   = 30
            printks "PRESET: AMBIENT\n", 0
          endif
        elseif (gkPresetBank == 2) then
          ; GORCORE onboard preset (bank slot 17) — user-recordable
          ; full state. Tap = RECALL; capture(5s)/delete(8s) arrive
          ; via the touchscreen (ch16). The old baked voicing is
          ; retired: gorcore onboard slots start EMPTY until captured,
          ; and an empty-slot recall is a no-op (state_persist.orc).
          gkPresetOp   = 2
          gkPresetSlot = 17
        else
          ; BASS: H90 WAMMY — up-only clamp [0,+12]; expr pedal sweeps pitch up
          if (gkBPL_bypass >= 0.5 && gkBPH_bypass >= 0.5) then
            gkBPL_bypass = 0              ; default to light preamp if none engaged
          endif
          gkBDS_bypass   = 1
          gkBypassChorus = 1
          gkBypassH90    = 0
          gkH90_min      = 0
          gkH90_max      = 12
          gkSemiH90      = 0
          gkLastToggle29 = 26
          printks "BASS PRESET: H90 WAMMY\n", 0
        endif

      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 0 (PS GLIDE) on/off. The mirror is not
        ; written here — it moves only when the Bela echoes /hg/state back, so
        ; a dead link shows as a lane that visibly does not respond.
        $HG_TOGGLE(0)
      elseif (gkCurrentMenu == 1) then
        ; Volume Pedal
        gkVOLpedalBypass = 1 - gkVOLpedalBypass
        if (gkVOLpedalBypass < 0.5) then
          gkLastToggle = 1
          gkLastToggle29 = 1
          printks "VOLUME PEDAL: ON\n", 0
        else
          printks "VOLUME PEDAL: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 3) then
        ; H90 PITCH (moved to slot 0 — ONWARD took slot 4, 2026-07-13)
        $FX_TOGGLE(gkBypassH90'26)
        if (gkBypassH90 < 0.5) then
          gkLastToggle29 = 26
          printks "H90: ON\n", 0
        else
          printks "H90: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 4) then
        ; LOSSY — codec degradation (FILTERING menu, slot 0)
        $FX_TOGGLE(gkLsyBypass'46)
        if (gkLsyBypass < 0.5) then
          ; 2026-09-01: knob re-defaults removed — settings persist across
          ; toggles (was: mix/loss/bw/pkt forced to factory on every engage)
          printks "LOSSY: ON\n", 0
        else
          printks "LOSSY: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 5) then
        ; Vibrato
        $FX_TOGGLE(gkVIB_bypass'6)
        if (gkVIB_bypass < 0.5) then
          printks "VIBRATO: ON\n", 0
        else
          printks "VIBRATO: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 6) then
        ; OVERDRIVE — pure Tube Screamer (inline ODPedal)
        $FX_TOGGLE(gkOD_bypass'31)
        if (gkOD_bypass < 0.5) then
          ; 2026-09-01: drive re-default removed (soft takeover in expr_cc29
          ; now prevents the stale-pedal slam this guarded against)
          gkLastToggle29 = 31
          printks "OVERDRIVE: ON\n", 0
        else
          printks "OVERDRIVE: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 9) then
        ; Reverb (DIGIVERB menu, slot 0 — LAST slot, user mandate: REVERB sits
        ; after NEBULA/SHIMMER/FREEZE/GRAINVERB's future slots 4-1). Moved
        ; here from CC76/slot2 (its old TIME-menu position) so the phase-2
        ; slot order reads NEBULA, SHIMMER, FREEZE, GRAINVERB, REVERB.
        $FX_TOGGLE(gkverbBypass'19)
        if (gkverbBypass < 0.5) then
          printks "REVERB: ON\n", 0
        else
          printks "REVERB: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 10) then
        ; DELAY (DELAY menu, slot 0 — UI list index4/slot0/CC74; the UI's
        ; list-index->slot mapping is REVERSED: index0=slot4/CC72 ...
        ; index4=slot0/CC74. Unchanged behavior/toggle id (15).
        $FX_TOGGLE(gkBypassDelay'15)
        if (gkBypassDelay < 0.5) then
          gkLastToggle29 = 15
          printks "DELAY: ON\n", 0
        else
          printks "DELAY: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 16) then
        ; IMPULSE SYNTH v2 (GLITCH menu, slot 0) — plain toggle, clocked
        ; contract: prob knob + CC29 instant-trigger claim (NEW in v2)
        $FX_TOGGLE(gkIMP_bypass'48)
        if (gkIMP_bypass < 0.5) then
          ; 2026-09-01: knob re-defaults removed — settings persist across
          ; toggles; gkIMP_expr stays reset (gesture var, 0 = neutral)
          gkIMP_expr = 0
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 48
          endif
          printks "IMPULSE: ON\n", 0
        else
          printks "IMPULSE: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 18) then
        ; IR VERB slot 0 → row 0 (SPRING) — NAM slot-handler pattern
        if (gkIRActiveRow == 0 && gkIRBypass < 0.5) then
          gkIRBypass = 1
        else
          gkIRActiveRow = 0
          gkIRBypass = 0
          gkLastToggle = 49
        endif
        printks "IR: row 0 variant %d\n", 0, gkIRSel[0]
      elseif (gkCurrentMenu == 7) then
        ; PREAMP menu, slot 4 — COMPRESSOR (toggle 57, instr 180). Dynamics
        ; belong with the front end. NO CC29 claim (pedal-into-threshold is a
        ; future option; gkLastToggle29 stays where it was).
        ; Lives in the CC74 chain: the UI's PREAMP row 4 maps to logical CC74,
        ; and the CC72 chain already spends its menu-7 branch on MAIN PREAMP,
        ; which is why the old CC72 copy of this branch was unreachable.
        $FX_TOGGLE(gkCMP_bypass'57)
        if (gkCMP_bypass < 0.5) then
          printks "COMPRESS: ON\n", 0
        else
          printks "COMPRESS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 11) then
        ; BASS menu slot 0 — BASS PREAMP LIGHT (toggle 40, radio vs HEAVY)
        $FX_TOGGLE(gkBPL_bypass'40)
        if (gkBPL_bypass < 0.5) then
          gkBPH_bypass = 1
          gkLastToggle29 = 40
          printks "BASS PRE L: ON\n", 0
        else
          printks "BASS PRE L: OFF\n", 0
        endif
      $LP_CH1_PRESS
      elseif (gkCurrentMenu == 17) then
        ; NAM slot 0 (rightmost) → row 4 (CRUNCH: JC-120 -> Plexi)
        if (gkNAMActiveRow == 4 && gkNAMBypass < 0.5) then
          gkNAMBypass = 1
        else
          gkNAMActiveRow = 4
          gkNAMBypass = 0
          gkNAMProfileIdx = giNAMRowBase[4] + gkNAMRowSel[4]
          ; Arm the encoders on the amp sim so P2/P3 (IN/OUT gain)
          ; reach it. Every row does this — before 2026-08-11 only
          ; row 4's slot did, so engaging any other amp left the
          ; gains pointing at whatever was toggled previously.
          gkLastToggle = 37
        endif
        printks "NAM: row 4 variant %d\n", 0, gkNAMRowSel[4]
      elseif (gkCurrentMenu == 2) then
        ; FORMS — clean formant shifter (SPECTRAL menu, slot 0)
        $FX_TOGGLE(gkFormsBypass'28)
        if (gkFormsBypass < 0.5) then
          gkLastToggle29 = 28
          printks "FORMS: ON\n", 0
        else
          printks "FORMS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 12) then
        ; TRACK SYNTH (SYNTH menu, slot 0) — toggle 61, instr 184.
        ; CC29 sticky claim -> cutoff (last-toggled-wins, see the CC29 header).
        $FX_TOGGLE(gkTSY_bypass'61)
        if (gkTSY_bypass < 0.5) then
          gkLastToggle29 = 61
          printks "TRACKSYN: ON\n", 0
        else
          printks "TRACKSYN: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 15) then
        ; CHASE BLISS s0 — BLOOPER (toggle 56, instr 208).
        ;
        ; 2026-07-13 REARCHITECTURE: BLOOPER is no longer a looper of its own.
        ; It is a GRANULAR MODIFIER of the 5-channel LOOPER (menu 12): when it
        ; is on, signal_chain swaps the looper's direct playback for BLOOPER's
        ; granular render of the same buffers. So it is a PLAIN TOGGLE now —
        ; the old raw press/release gesture machine (and its CC74 release
        ; branch) is gone. Record your loops in menu 12 as always, then flip
        ; this on to granulate them.
        $FX_TOGGLE(gkBLP_bypass'56)
        if (gkBLP_bypass < 0.5) then
          ; 2026-09-01: toggle-on NO LONGER re-defaults the knobs. It used to
          ; overwrite Scat/Size/Char/Motion/expr with factory values on every
          ; engage, so settings never survived an off/on cycle ("blooper
          ; always resets settings between uses"). Knob state now persists
          ; across toggles exactly like every other effect; boot defaults live
          ; in blooper.orc's init block and the settings ftab restores the
          ; user's own values.
          ;
          ; gkBLP_expr in particular MUST rest at 0.5, not 0: the pedal is
          ; centred (blooper.orc kPedalMul = 1 + 2*(expr-0.5)), so 0 means
          ; "multiply all event duty by ZERO" — writing 0 here silently forced
          ; BLOOPER into a null render until the pedal physically crossed 50%.
          gkBLP_expr   = 0.5     ; neutral pedal — pure generative until moved
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 56    ; sticky CC29 claim: pedal = manual chaos push
          endif
          printks "BLOOPER: ON (modulating %d loop(s))\n", 0, gkBLP_Active
        else
          printks "BLOOPER: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 13) then
        ; FEEDBACKER (AIR menu, slot 0) — toggle 63, instr 186.
        ; CC29 sticky claim -> intensity push (last-toggled-wins).
        $FX_TOGGLE(gkFBK_bypass'63)
        if (gkFBK_bypass < 0.5) then
          gkFBK_expr     = 0     ; fresh gesture — never inherit a stale pedal push
          gkLastToggle29 = 63
          printks "FEEDBACKER: ON\n", 0
        else
          printks "FEEDBACKER: OFF\n", 0
        endif
      endif


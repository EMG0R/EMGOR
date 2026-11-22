    ;==========================================================
    ; EFFECT TOGGLES — CC71 (Enc2 button, slot 3)
    ;==========================================================
    elseif (kCC == 71 && kVal == 127) then

      if (gkCurrentMenu == 0) then
        if (gkPresetBank == 0) then
          ; SURF: SOLID DIST — the heavy channel. Vib + spring-delay base +
          ; DUAL HIGH-GAIN at a solid setting. CC21 sweeps gkDHG_drive.
          if (gkCurrentPreset == 2) then
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
            printks "PRESET: SOLID->OVERDRIVE\n", 0
          else
            gkCurrentPreset = 2
            gkTremBypass    = 1
            gkPHA_bypass    = 1
            ; Vibrato + spring-delay base (unified preset values — see globals.orc)
            gkVIB_bypass  = 0
            gkVIB_freq    = giPresetVibFreq
            gkVIB_depth   = giPresetVibDepth
            gkVIB_fback   = giPresetVibFback
            gkBypassDelay = 0
            gkDelayMS     = giPresetDelMS
            gkFeedback    = giPresetDelFB
            gkDelMix      = giPresetDelMix
            ; OD: off (mutually exclusive dirt channels)
            gkOD_bypass   = 1
            ; DUAL HIGH-GAIN: on, solid setting — CC21 sweeps drive
            gkDHG_bypass  = 0
            gkDHG_drive   = 0.4    ; DS-1K solid setting
            gkDHG_wc1     = 0.5
            ; Reverb: off; expression pedal NOT routed here
            gkverbBypass     = 1
            gkVOLpedalBypass = 1
            gkMasterVol      = 1
            gkAmbBoost       = 1
            gkLastToggle29   = 0   ; exp pedal inactive for this preset
            printks "PRESET: SOLID DIST\n", 0
          endif
        elseif (gkPresetBank == 2) then
          ; GORCORE onboard preset (bank slot 14) — user-recordable
          ; full state. Tap = RECALL; capture(5s)/delete(8s) arrive
          ; via the touchscreen (ch16). The old baked voicing is
          ; retired: gorcore onboard slots start EMPTY until captured,
          ; and an empty-slot recall is a no-op (state_persist.orc).
          gkPresetOp   = 2
          gkPresetSlot = 14
        else
          ; BASS: PRE HEAVY — BPH (heavy) bass preamp, everything else clean
          gkBPL_bypass   = 1
          gkBPH_bypass   = 0
          gkBDS_bypass   = 1
          gkBypassChorus = 1
          gkBypassH90    = 1
          gkH90_min      = -12
          gkH90_max      = 12
          gkLastToggle29 = 0
          printks "BASS PRESET: PRE HEAVY\n", 0
        endif

      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 3 (DYNAMICS) on/off. The mirror is not
        ; written here — it moves only when the Bela echoes /hg/state back, so
        ; a dead link shows as a lane that visibly does not respond.
        $HG_TOGGLE(3)
      elseif (gkCurrentMenu == 10) then
        ; SPECTRAL DELAY (DELAY menu, slot 3 — UI list index1/slot3/CC71; the
        ; UI's list-index->slot mapping is REVERSED: index0=slot4/CC72 ...
        ; index4=slot0/CC74). 2026-07-26: no longer dual-homed with SPECTRAL
        ; menu 2 — BINSCATTER took over menu 2's old SPECDLY slot; SPECDLY
        ; now lives ONLY here.
        $FX_TOGGLE(gkSpecDelBypass'16)
        if (gkSpecDelBypass < 0.5) then
          printks "SPECTRAL DELAY: ON\n", 0
        else
          printks "SPECTRAL DELAY: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 9) then
        ; SHIMMER (DIGIVERB menu, slot 3 — UI list index1/slot3/CC71). No
        ; expr claim (spec: no expr for 71).
        $FX_TOGGLE(gkShmBypass'71)
        if (gkShmBypass < 0.5) then
          printks "SHIMMER: ON\n", 0
        else
          printks "SHIMMER: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 1) then
        ; Attack
        $FX_TOGGLE(gkATTK_bypass'2)
        if (gkATTK_bypass < 0.5) then
          ; v2: no expression-pedal claim — pedal keeps its previous target
          ; 2026-09-01: sweet-spot re-defaults removed - settings persist
          ; across toggles (boot defaults still live in the effect's inits)
          printks "ATTACK: ON\n", 0
        else
          printks "ATTACK: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 3) then
        ; PITCH FORK (slot 3 — second from left)
        $FX_TOGGLE(gkBypassPF'27)
        if (gkBypassPF < 0.5) then
          gkLastToggle29 = 27
          printks "PITCH FORK: ON\n", 0
        else
          printks "PITCH FORK: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 4) then
        ; AutoMoog (FILTERING menu, moved to slot 3 — RESONATR took s1)
        $FX_TOGGLE(gkAUTOMOOG_bypass'14)
        if (gkAUTOMOOG_bypass < 0.5) then
          printks "AUTOMOOG: ON\n", 0
        else
          printks "AUTOMOOG: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 5) then
        ; TREMOLO — now a normal MOD effect (toggle 43); also still a surf-preset
        ; modifier in menu 0. Defaults live in globals (gkTremRate/gkTremDepth).
        $FX_TOGGLE(gkTremBypass'43)
        if (gkTremBypass < 0.5) then
          ; 2026-09-01: mode re-default removed - the last-used trem mode persists
          gkLastToggle29 = 43
          printks "TREMOLO: ON\n", 0
        else
          printks "TREMOLO: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 6) then
        ; DS-1K — Keeley-modded DS-1 flagship (moved to slot 3, swap 2026-06-11)
        $FX_TOGGLE(gkDHG_bypass'33)
        if (gkDHG_bypass < 0.5) then
          ; 2026-09-01: drive re-default removed (soft takeover covers stale exp)
          gkLastToggle29 = 33
          printks "DS-1K: ON\n", 0
        else
          printks "DS-1K: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 7) then
        ; PREAMP menu, slot 1 — BASS PREAMP LIGHT (toggle 40), radio vs HEAVY
        $FX_TOGGLE(gkBPL_bypass'40)
        if (gkBPL_bypass < 0.5) then
          gkBPH_bypass = 1
          gkLastToggle29 = 40
          printks "BASS PRE L: ON\n", 0
        else
          printks "BASS PRE L: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 8) then
        ; Granular (GRANULAR menu, slot 3 — unchanged menu index, renamed from TIME)
        $FX_TOGGLE(gkGranBypass'18)
        if (gkGranBypass < 0.5) then
          printks "GRANULAR: ON\n", 0
        else
          printks "GRANULAR: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 16) then
        ; CHOP v2 (slot 3, toggle 24 — reuses SqMod bypass) — plain toggle
        $FX_TOGGLE(gkSqMod_bypass'24)
        if (gkSqMod_bypass < 0.5) then
          ; 2026-09-01: prob re-default removed; expr stays reset (gesture var)
          gkCHP_expr = 0
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 24
          endif
          printks "CHOP: ON\n", 0
        else
          printks "CHOP: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 18) then
        ; IR VERB slot 3 → row 3 (CHAMBER)
        if (gkIRActiveRow == 3 && gkIRBypass < 0.5) then
          gkIRBypass = 1
        else
          gkIRActiveRow = 3
          gkIRBypass = 0
          gkLastToggle = 52
        endif
        printks "IR: row 3 variant %d\n", 0, gkIRSel[3]
      elseif (gkCurrentMenu == 11) then
        ; BASS menu slot 3 — OCTAVER (toggle 54, instr 170)
        $FX_TOGGLE(gkOCT_bypass'54)
        if (gkOCT_bypass < 0.5) then
          printks "OCTAVER: ON\n", 0
        else
          printks "OCTAVER: OFF\n", 0
        endif
      $LP_CH4_PRESS
      elseif (gkCurrentMenu == 17) then
        ; NAM slot 3 → row 1 (DELUXE: clean -> light drive)
        if (gkNAMActiveRow == 1 && gkNAMBypass < 0.5) then
          gkNAMBypass = 1
        else
          gkNAMActiveRow = 1
          gkNAMBypass = 0
          gkNAMProfileIdx = giNAMRowBase[1] + gkNAMRowSel[1]
          ; Arm the encoders on the amp sim so P2/P3 (IN/OUT gain)
          ; reach it. Every row does this — before 2026-08-11 only
          ; row 4's slot did, so engaging any other amp left the
          ; gains pointing at whatever was toggled previously.
          gkLastToggle = 37
        endif
        printks "NAM: row 1 variant %d\n", 0, gkNAMRowSel[1]
      elseif (gkCurrentMenu == 15) then
        ; CHASE BLISS s3 — MOOD II (toggle 66, instr 189). REWORKED 2026-07-13:
        ; the old always-on micro-looper + gesture grammar (tap/hold, RECORDING/
        ; PLAYING/OVERDUBBING) is GONE — see mood2.orc header. MOOD II is now a
        ; plain $FX_TOGGLE effect like everything else: OFF really means off.
        ; NOTE: MOOD II (t66, instr 189) and the ORIGINAL MOOD (t17, instrs
        ; 401-405, TIME menu) are SEPARATE effects. Both stay.
        $FX_TOGGLE(gkMD2_bypass'66)
        if (gkMD2_bypass < 0.5) then
          ; claim moved INSIDE the ON branch (2026-09-01) — it used to sit
          ; before the if, so toggling MOOD II OFF also stole the pedal.
          gkLastToggle29 = 66   ; sticky CC29 claim -> CLOCK scrub
          printks "MOOD II: ON\n", 0
        else
          printks "MOOD II: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 2) then
        ; BIN REDUCTION — spectral contrast exponentiation (SPECTRAL menu, slot 3)
        $FX_TOGGLE(gkBinBypass'38)
        if (gkBinBypass < 0.5) then
          gkBinPedal = 1          ; toggle-on: toe/transparent, never stale exp
          gkLastToggle29 = 38
          printks "BIN REDU: ON\n", 0
        else
          printks "BIN REDU: OFF\n", 0
        endif
      endif


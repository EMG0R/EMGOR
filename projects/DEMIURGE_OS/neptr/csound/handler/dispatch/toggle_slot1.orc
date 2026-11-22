    ;==========================================================
    ; EFFECT TOGGLES — CC75 (Enc6 button, slot 1)
    ;==========================================================
    elseif (kCC == 75 && kVal == 127) then

      if (gkCurrentMenu == 0) then
        if (gkPresetBank == 0) then
          ; SURF: TREMOLO TOGGLE (cross-preset modifier)
          gkTremBypass = 1 - gkTremBypass
          if (gkTremBypass < 0.5) then
            gkTremMode = 0       ; gig-locked surf presets keep the v1 SINE sound
            printks "TREMOLO: ON\n", 0
          else
            printks "TREMOLO: OFF\n", 0
          endif
        elseif (gkPresetBank == 2) then
          ; GORCORE onboard preset (bank slot 16) — user-recordable
          ; full state. Tap = RECALL; capture(5s)/delete(8s) arrive
          ; via the touchscreen (ch16). The old baked voicing is
          ; retired: gorcore onboard slots start EMPTY until captured,
          ; and an empty-slot recall is a no-op (state_persist.orc).
          gkPresetOp   = 2
          gkPresetSlot = 16
        else
          ; BASS: BASS DIST — drive dialed, preamp unchanged
          if (gkBPL_bypass >= 0.5 && gkBPH_bypass >= 0.5) then
            gkBPL_bypass = 0
          endif
          gkBypassChorus = 1
          gkBypassH90    = 1
          gkH90_min      = -12
          gkH90_max      = 12
          gkBDS_bypass   = 0
          gkBDS_drive    = 0.6
          gkBDS_blend    = 0.35
          gkBDS_tone     = 0.5
          gkLastToggle29 = 42
          printks "BASS PRESET: BASS DIST\n", 0
        endif

      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 1 (PS STEPS) on/off. The mirror is not
        ; written here — it moves only when the Bela echoes /hg/state back, so
        ; a dead link shows as a lane that visibly does not respond.
        $HG_TOGGLE(1)
      elseif (gkCurrentMenu == 9) then
        ; GRAINVERB (DIGIVERB menu, slot 1 — UI list index3/slot1/CC75; the
        ; UI's list-index->slot mapping is REVERSED: index0=slot4/CC72 ...
        ; index4=slot0/CC74).
        $FX_TOGGLE(gkGvbBypass'73)
        if (gkGvbBypass < 0.5) then
          printks "GRAINVERB: ON\n", 0
        else
          printks "GRAINVERB: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 1) then
        ; Ring Mod
        $FX_TOGGLE(gkBypassRM'11)
        if (gkBypassRM < 0.5) then
          gkLastToggle29 = 11
          printks "RING MOD: ON\n", 0
        else
          printks "RING MOD: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 3) then
        ; OLD PS (was rightmost; now slot 1 — second from right)
        $FX_TOGGLE(gkBypassPS'3)
        if (gkBypassPS < 0.5) then
          gkLastToggle29 = 3
          printks "OLD PS: ON\n", 0
        else
          printks "OLD PS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 4) then
        ; RESONATOR — sympathetic pipe bank (FILTERING menu, slot 1)
        $FX_TOGGLE(gkRES_bypass'47)
        if (gkRES_bypass < 0.5) then
          ; 2026-09-01: knob re-defaults removed — settings persist across toggles
          printks "RESONATR: ON\n", 0
        else
          printks "RESONATR: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 5) then
        ; Chorus
        $FX_TOGGLE(gkBypassChorus'7)
        if (gkBypassChorus < 0.5) then
          printks "CHORUS: ON\n", 0
        else
          printks "CHORUS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 6) then
        ; MARSHALL crunch preamp (instr 131)
        $FX_TOGGLE(gkMAR_bypass'32)
        if (gkMAR_bypass < 0.5) then
          ; 2026-09-01: drive re-default removed (soft takeover covers stale exp)
          gkLastToggle29 = 32
          printks "MARSHALL: ON\n", 0
        else
          printks "MARSHALL: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 7) then
        ; PREAMP menu, slot 3 — BASS DISTORTION (toggle 42)
        $FX_TOGGLE(gkBDS_bypass'42)
        if (gkBDS_bypass < 0.5) then
          gkLastToggle29 = 42
          printks "BASS DIST: ON\n", 0
        else
          printks "BASS DIST: OFF\n", 0
        endif
      ; GRANULAR s1 (menu 8) is EMPTY — HABIT moved to CHASE BLISS s2
      ; (2026-07-13). The slot is deliberately left free; no CC75 branch here.
      ; DELAY menu (10) slot 1 (UI list index3) is EMPTY — deliberately left
      ; free ("+1 free" per spec); no CC75 branch here.
      elseif (gkCurrentMenu == 16) then
        ; BBCUT v2 (slot 1) — plain toggle (hold gesture retired 2026-07-15)
        $FX_TOGGLE(gkBBcut_bypass'20)
        if (gkBBcut_bypass < 0.5) then
          ; 2026-09-01: prob re-default removed; expr stays reset (gesture var)
          gkBBcut_expr = 0
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 20
          endif
          printks "BBCUT: ON\n", 0
        else
          printks "BBCUT: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 18) then
        ; IR VERB slot 1 → row 1 (PLATE)
        if (gkIRActiveRow == 1 && gkIRBypass < 0.5) then
          gkIRBypass = 1
        else
          gkIRActiveRow = 1
          gkIRBypass = 0
          gkLastToggle = 50
        endif
        printks "IR: row 1 variant %d\n", 0, gkIRSel[1]
      elseif (gkCurrentMenu == 11) then
        ; BASS menu slot 1 — BASS PREAMP HEAVY (toggle 41, radio vs LIGHT)
        $FX_TOGGLE(gkBPH_bypass'41)
        if (gkBPH_bypass < 0.5) then
          gkBPL_bypass = 1
          gkLastToggle29 = 41
          printks "BASS PRE H: ON\n", 0
        else
          printks "BASS PRE H: OFF\n", 0
        endif
      $LP_CH2_PRESS
      elseif (gkCurrentMenu == 17) then
        ; NAM slot 1 → row 3 (TWEED: edge -> breakup)
        if (gkNAMActiveRow == 3 && gkNAMBypass < 0.5) then
          gkNAMBypass = 1
        else
          gkNAMActiveRow = 3
          gkNAMBypass = 0
          gkNAMProfileIdx = giNAMRowBase[3] + gkNAMRowSel[3]
          ; Arm the encoders on the amp sim so P2/P3 (IN/OUT gain)
          ; reach it. Every row does this — before 2026-08-11 only
          ; row 4's slot did, so engaging any other amp left the
          ; gains pointing at whatever was toggled previously.
          gkLastToggle = 37
        endif
        printks "NAM: row 3 variant %d\n", 0, gkNAMRowSel[3]
      elseif (gkCurrentMenu == 2) then
        ; FORMANT (SPECTRAL menu, slot 1)
        $FX_TOGGLE(gkFormantBypass'5)
        if (gkFormantBypass < 0.5) then
          gkLastToggle29 = 5
          printks "FORMANT: ON\n", 0
        else
          printks "FORMANT: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 12) then
        ; PLL FUZZ (SYNTH menu, slot 1) — toggle 62, instr 185. NO CC29 claim.
        $FX_TOGGLE(gkPLL_bypass'62)
        if (gkPLL_bypass < 0.5) then
          printks "PLLFUZZ: ON\n", 0
        else
          printks "PLLFUZZ: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 15) then
        ; CHASE BLISS s1 — ONWARD (toggle 45, instr 155). MOVED here from
        ; PITCH s4 (2026-07-13); this is its ONLY home. Behaviour, params,
        ; toggle id and instr are UNCHANGED — only the menu slot moved, so its
        ; P1 rotation is now CC25 (was CC22).
        ; ONWARD v3 — onset-driven, harmonic-following DYNAMIC SAMPLER.
        ; gkONW_sust/gkONW_type are GONE (replaced by follow/density in the
        ; 2026-07-14 harmonic rebuild); the live params are now
        ; mix/blend/size/follow/density/err/oct.
        $FX_TOGGLE(gkONW_bypass'45)
        if (gkONW_bypass < 0.5) then
          gkONW_mix     = 0.6     ; P1 CC25 — sampled layer vs dry
          gkONW_blend   = 0.75    ; P2 CC20 — GLITCH-led (0 freeze pad .. 1 glitch)
          gkONW_size    = 0.3     ; P3 CC23 — ~0.16 s chunks: bloopy, not phrases
          gkONW_follow  = 0.6     ; P4 CC24 — harmonic-follow strength (also CC29)
          gkONW_density = 0.15    ; P5 CC26 — call-and-response echo density
          gkONW_err     = 0.35    ; P6 CC21 — mangle intensity
          gkONW_oct     = 0       ; P7 CC22 — half/double-speed octave voice
          gkLastToggle29 = 45     ; sticky CC29 claim -> FOLLOW
          printks "ONWARD: ON\n", 0
        else
          printks "ONWARD: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 13) then
        ; DIFFUSION (AIR menu, slot 1) — toggle 64, instr 187. NO CC29 claim.
        $FX_TOGGLE(gkDIF_bypass'64)
        if (gkDIF_bypass < 0.5) then
          printks "DIFFUSION: ON\n", 0
        else
          printks "DIFFUSION: OFF\n", 0
        endif
      endif


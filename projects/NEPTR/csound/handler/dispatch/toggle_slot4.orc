    ;==========================================================
    ; EFFECT TOGGLES — CC72 (Enc3 button, slot 4, leftmost)
    ;==========================================================
    elseif (kCC == 72 && kVal == 127) then

      if (gkCurrentMenu == 0) then
        ; PRESETS (menu 0): five USER preset slots. The press/release gesture
        ; (tap = recall, 5 s = capture, 8 s = delete) is decided on RELEASE in
        ; the USER PRESETS block at the top of handler_menus.orc, so the
        ; press itself does nothing here. (Onboard surf/bass/gorcore presets
        ; removed 2026-10-24 - see git history.)
        kPrNoop4 = 0

      elseif (gkCurrentMenu == $MX_MENU) then
        ; MIXER page (mixer_page.orc): click selects this channel; the two
        ; modulating encoders (CC20/CC23) now drive its A/B params.
        $MX_SELECT(0)
      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 4 (SYNTH) on/off. The mirror is not
        ; written here — it moves only when the Bela echoes /hg/state back, so
        ; a dead link shows as a lane that visibly does not respond.
        $HG_TOGGLE(4)
      elseif (gkCurrentMenu == 10) then
        ; TAPE DELAY (DELAY menu, slot 4 — UI list index0/slot4/CC72; the
        ; UI's list-index->slot mapping is REVERSED: index0=slot4/CC72 ...
        ; index4=slot0/CC74).
        $FX_TOGGLE(gkTdlBypass'68)
        if (gkTdlBypass < 0.5) then
          printks "TAPE DELAY: ON\n", 0
        else
          printks "TAPE DELAY: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 9) then
        ; NEBULA (DIGIVERB menu, slot 4 — UI list index0/slot4/CC72). Claims
        ; the expr pedal on engage (WARP, spec mandate).
        $FX_TOGGLE(gkNebBypass'70)
        if (gkNebBypass < 0.5) then
          gkLastToggle29 = 70
          printks "NEBULA: ON\n", 0
        else
          printks "NEBULA: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 1) then
        ; TAPE machine (Gen Loss-style; replaced DROPOUT)
        $FX_TOGGLE(gkTapeBypass'21)
        if (gkTapeBypass < 0.5) then
          ; 2026-09-01: knob re-defaults removed - settings persist across toggles
          gkLastToggle29 = 21
          printks "TAPE: ON\n", 0
        else
          printks "TAPE: OFF\n", 0
        endif
      ; PITCH s4 is EMPTY — ONWARD moved to CHASE BLISS s1 (2026-07-13).
      ; FILTERING s4 is EMPTY — BLOOPER moved to CHASE BLISS s0 (2026-07-13).
      ; Both slots are deliberately left free; no CC72 branch for menus 3 / 4.
      ; (LOSSY s0 and RESONATR s1 STAY in FILTERING — user decision.)
      elseif (gkCurrentMenu == 5) then
        ; Flanger
        $FX_TOGGLE(gkFLA_bypass'9)
        if (gkFLA_bypass < 0.5) then
          printks "FLANGER: ON\n", 0
        else
          printks "FLANGER: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 6) then
        ; BIG MUFF (Triangle/Fallout fuzz, instr 134)
        $FX_TOGGLE(gkMUF_bypass'35)
        if (gkMUF_bypass < 0.5) then
          ; 2026-09-01: drive re-default removed (soft takeover covers stale exp)
          gkLastToggle29 = 35
          printks "BIG MUFF: ON\n", 0
        else
          printks "BIG MUFF: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 7) then
        ; PREAMP menu, slot 0 — MAIN PREAMP (toggle 39)
        $FX_TOGGLE(gkPreamp_bypass'39)
        if (gkPreamp_bypass < 0.5) then
          gkLastToggle29 = 39
          printks "MAIN PREAMP: ON\n", 0
        else
          printks "MAIN PREAMP: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 8) then
        ; MOOD (GRANULAR menu, slot 4 — unchanged menu index, renamed from TIME)
        $FX_TOGGLE(gkMOOD_Bypass'17)
        if (gkMOOD_Bypass < 0.5) then
          gkLastToggle29 = 17
          printks "MOOD: ON\n", 0
        else
          printks "MOOD: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 16) then
        ; RVRS v2 (slot 4, toggle 25 — reuses FShift bypass) — plain toggle
        $FX_TOGGLE(gkFShift_bypass'25)
        if (gkFShift_bypass < 0.5) then
          ; 2026-09-01: prob re-default removed; expr stays reset (gesture var)
          gkRVS_expr = 0
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 25
          endif
          printks "RVRS: ON\n", 0
        else
          printks "RVRS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 18) then
        ; IR VERB slot 4 → row 4 (WOOD RM)
        if (gkIRActiveRow == 4 && gkIRBypass < 0.5) then
          gkIRBypass = 1
        else
          gkIRActiveRow = 4
          gkIRBypass = 0
          gkLastToggle = 53
        endif
        printks "IR: row 4 variant %d\n", 0, gkIRSel[4]
      elseif (gkCurrentMenu == 11) then
        ; BASS menu slot 4 — ENV FILTER (toggle 55, instr 171)
        $FX_TOGGLE(gkENV_bypass'55)
        if (gkENV_bypass < 0.5) then
          printks "ENV FILTER: ON\n", 0
        else
          printks "ENV FILTER: OFF\n", 0
        endif
      $LP_CH5_PRESS
      elseif (gkCurrentMenu == 17) then
        ; NAM slot 4 (leftmost) → row 0 (AC30: edge -> saturated)
        if (gkNAMActiveRow == 0 && gkNAMBypass < 0.5) then
          gkNAMBypass = 1
        else
          gkNAMActiveRow = 0
          gkNAMBypass = 0
          gkNAMProfileIdx = giNAMRowBase[0] + gkNAMRowSel[0]
          ; Arm the encoders on the amp sim so P2/P3 (IN/OUT gain)
          ; reach it. Every row does this — before 2026-08-11 only
          ; row 4's slot did, so engaging any other amp left the
          ; gains pointing at whatever was toggled previously.
          gkLastToggle = 37
        endif
        printks "NAM: row 0 variant %d\n", 0, gkNAMRowSel[0]
      elseif (gkCurrentMenu == 2) then
        ; BINCRUSHER — spectral bin coarsener (SPECTRAL menu, slot 4)
        $FX_TOGGLE(gkBcBypass'44)
        if (gkBcBypass < 0.5) then
          gkBcPedal = 0           ; toggle-on: heel (transparent, no coarsening)
          gkLastToggle29 = 44
          printks "BCRUSHER: ON\n", 0
        else
          printks "BCRUSHER: OFF\n", 0
        endif
      ; CHASE BLISS s4 is EMPTY — no CC72 branch for menu 13; renders as "—".
      ; (The COMPRESSOR branch that used to sit here was DEAD: this chain
      ;  already matches gkCurrentMenu == 7 further up, for MAIN PREAMP, so
      ;  the second menu-7 elseif could never be reached. Moved to the CC74
      ;  chain 2026-08-11, which is where the UI's PREAMP row 4 sends.)
      elseif (gkCurrentMenu == 13) then
        ; CHAIN FB (AIR menu, slot 4) — toggle 60, instr 183. Wraps the chain
        ; span (inject 11.4 / capture 22.9). NO CC29 claim. Toggle-on defaults
        ; keep the loop tame on engage.
        $FX_TOGGLE(gkCFB_bypass'60)
        if (gkCFB_bypass < 0.5) then
          gkCFB_ret  = 0.4
          gkCFB_gain = 0.5
          gkCFB_tone = 0.5
          gkCFB_ceil = 0.8
          printks "CHAIN FB: ON\n", 0
        else
          printks "CHAIN FB: OFF\n", 0
        endif
      endif

    ; (GLITCH v2 2026-07-15: the press/hold "chance group" gesture is RETIRED —
    ; BBCUT/REPEAT/CHOP/RVRS are plain toggles now; the expression pedal is the
    ; instant trigger. The four GLITCH RELEASE handlers are gone.)

    ; (MOOD II no longer has a raw press/release gesture, either — reworked
    ; 2026-07-13 into a plain $FX_TOGGLE. See handler CC71 menu 13 + mood2.orc.)
    ; (BLOOPER no longer has a raw press/release gesture — it is a plain
    ; toggle now that it is a granular modifier of the LOOPER, so the old
    ; CC74 kVal==0 branch is gone. See handler CC74 menu 13 + blooper.orc.)

    $LP_RELEASE_ALL


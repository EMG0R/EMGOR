    ;==========================================================
    ; EFFECT TOGGLES — CC76 (Enc7 button, slot 2)
    ;==========================================================
    elseif (kCC == 76 && kVal == 127) then

      if (gkCurrentMenu == 0) then
        if (gkPresetBank == 0) then
          ; SURF: PHASER TOGGLE (cross-preset modifier; analog MXR-style voicing).
          gkPHA_bypass = 1 - gkPHA_bypass
          if (gkPHA_bypass < 0.5) then
            ; Fixed MXR Phase 90-style ANALOG voicing, consistent regardless of
            ; last MOD-menu edits. The key fix: shape = 2 (SINE LFO) — the old
            ; shape 8 was randomi (a random sample/hold wobble), which is why it
            ; never swept like an analog phaser. Sweep also moved up into the
            ; classic ~250 Hz–2 kHz notch band (old freq 0.15 sat ~40–260 Hz),
            ; with real feedback for the resonant analog "throat".
            gkPHA_rate   = 0.71   ; slow analog sweep (2/3 of the previous 1.07 Hz)
            gkPHA_depth  = 0.4    ; ~3 octave sweep width (sine)
            gkPHA_freq   = 0.37   ; centre the sweep in the low-mids (~250–1900 Hz)
            gkPHA_fback  = 0.72   ; heavy regeneration → strong resonant/vocal feedback vibe
            gkPHA_stages = 4      ; classic 4-stage (2-notch) MXR Phase 90 topology
            gkPHA_mix    = 0.7
            gkPHA_shape  = 2      ; SINE LFO — smooth analog sweep (was 8 = random)
            printks "PHASER: ON\n", 0
          else
            printks "PHASER: OFF\n", 0
          endif
        elseif (gkPresetBank == 2) then
          ; GORCORE onboard preset (bank slot 15) — user-recordable
          ; full state. Tap = RECALL; capture(5s)/delete(8s) arrive
          ; via the touchscreen (ch16). The old baked voicing is
          ; retired: gorcore onboard slots start EMPTY until captured,
          ; and an empty-slot recall is a no-op (state_persist.orc).
          gkPresetOp   = 2
          gkPresetSlot = 15
        else
          ; BASS: TOTAL GLITCH v2 — BBCUT + CHOP + RVRS bundle (REPEAT off:
          ; three clocked glitchers at once is already dense). Pure additive:
          ; does NOT touch preamp, dist, H90, or any other state.
          if (gkTotalGlitch >= 0.5) then
            ; OFF → ON: engage three effects with hot probabilities
            gkBBcut_bypass  = 0
            gkSqMod_bypass  = 0
            gkFShift_bypass = 0
            gkBBcut_prob    = 0.7
            gkCHP_prob      = 0.7
            gkRVS_prob      = 0.5
            gkWGuide_bypass = 1   ; REPEAT off
            gkTotalGlitch   = 0
            printks "TOTAL GLITCH: ON\n", 0
          else
            ; ON → OFF: all four bypassed, probs back to default
            gkBBcut_bypass  = 1
            gkSqMod_bypass  = 1
            gkFShift_bypass = 1
            gkWGuide_bypass = 1
            gkBBcut_prob    = 0.5
            gkCHP_prob      = 0.5
            gkRVS_prob      = 0.5
            gkTotalGlitch   = 1
            printks "TOTAL GLITCH: OFF\n", 0
          endif
        endif

      elseif (gkCurrentMenu == $HG_MENU) then
        ; HYPERGUITAR lane 2 (FORMANT) on/off. The mirror is not
        ; written here — it moves only when the Bela echoes /hg/state back, so
        ; a dead link shows as a lane that visibly does not respond.
        $HG_TOGGLE(2)
      elseif (gkCurrentMenu == 10) then
        ; POLY DLY (DELAY menu, slot 2 — UI order: tdl/sdl/mvd/free/dly).
        ; Claims the expr pedal on engage (spec mandate — every mode maps it
        ; onto its own "something cool" target).
        $FX_TOGGLE(gkMvdBypass'69)
        if (gkMvdBypass < 0.5) then
          gkLastToggle29 = 69
          printks "MOVE DELAY: ON\n", 0
        else
          printks "MOVE DELAY: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 9) then
        ; FREEZE (DIGIVERB menu, slot 2 — UI list index2/slot2/CC76). Gesture
        ; contract (freezeverb.orc header, spec section 6), same press-fired
        ; tap-classify style as handler_looper.orc's LP_CH*_PRESS macros:
        ;   click while bypassed  -> engage (empty bed, listen closed until
        ;                            toe), claims the expr pedal
        ;   click while engaged   -> CLEAR (gkFrzClear pulse), stays engaged
        ;   double-tap (<0.5s) while engaged -> escape/bypass
        kFrzNow times
        if (gkFrzBypass >= 0.5) then
          gkFrzBypass = 0
          ; FREEZE NO LONGER CLAIMS THE EXPRESSION PEDAL (2026-09-15).
          ;
          ; This line used to assign FREEZE's toggle id to the pedal-owner
          ; global, handing the treadle to FREEZE's LISTEN send. Two properties
          ; of the expr system combine to make that awful in practice:
          ;
          ;   1. the claim is STICKY and is never released — expr_cc29.orc's
          ;      documented rule is that turning an effect OFF does not hand
          ;      the pedal back; and
          ;   2. nothing on screen says who owns the pedal.
          ;
          ; So one engage of FREEZE, at any point in the session, permanently
          ; re-points the treadle at gkFrzExpr. Every later push past 0.6 trips
          ; the Schmitt trigger in freezeverb.orc and opens the send, stacking
          ; whatever is being played into the frozen bed — with FREEZE long
          ; since switched off and forgotten. Reported 2026-09-15 as the pedal
          ; "randomly activating the freeze effect ... whenever i push it down",
          ; which is exactly right and is not random at all.
          ;
          ; The pedal is the DUODECIMUS treadle and it belongs to whatever the
          ; player last deliberately pointed it at (menu-0 presets declare
          ; target 30, the reverb-swell stage). FREEZE keeps its gesture
          ; engage, its encoders and its LISTEN knob; it just cannot silently
          ; take the pedal any more. gkFrzExpr stays a valid ExprSet target
          ; (id 72) for anything that asks for it BY NAME — a sensor, an OSC
          ; /p write, a future explicit assignment.
          printks "FREEZE: ON (engage)\n", 0
        elseif (kFrzNow - gkFrzLastTap < 0.5) then
          gkFrzBypass = 1
          printks "FREEZE: OFF (double-tap escape)\n", 0
        else
          gkFrzClear = 1
          printks "FREEZE: CLEAR\n", 0
        endif
        gkFrzLastTap = kFrzNow
      elseif (gkCurrentMenu == 1) then
        ; Leslie
        $FX_TOGGLE(gkLeslieBypass'10)
        if (gkLeslieBypass < 0.5) then
          gkLastToggle29 = 10
          printks "LESLIE: ON\n", 0
        else
          printks "LESLIE: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 3) then
        ; GRANULAR PS (slot 2, center)
        $FX_TOGGLE(gkBypassGPS'4)
        if (gkBypassGPS < 0.5) then
          gkLastToggle29 = 4
          printks "GPS: ON\n", 0
        else
          printks "GPS: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 4) then
        ; WAH (FILTERING menu, moved to slot 2 — LOSSY/RESONATR took s0/s1)
        $FX_TOGGLE(gkWAH_bypass'22)
        if (gkWAH_bypass < 0.5) then
          gkLastToggle29 = 22
          printks "WAH: ON\n", 0
        else
          printks "WAH: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 5) then
        ; Phaser
        $FX_TOGGLE(gkPHA_bypass'8)
        if (gkPHA_bypass < 0.5) then
          gkLastToggle29 = 8
          printks "PHASER: ON\n", 0
        else
          printks "PHASER: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 6) then
        ; RAT × MZ (moved to slot 2 — swapped with DS-1K 2026-06-11)
        $FX_TOGGLE(gkRMZ_bypass'34)
        if (gkRMZ_bypass < 0.5) then
          ; 2026-09-01: drive re-default removed (soft takeover covers stale exp)
          gkLastToggle29 = 34
          printks "RAT-MZ: ON\n", 0
        else
          printks "RAT-MZ: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 7) then
        ; PREAMP menu, slot 2 — BASS PREAMP HEAVY (toggle 41), radio vs LIGHT
        $FX_TOGGLE(gkBPH_bypass'41)
        if (gkBPH_bypass < 0.5) then
          gkBPL_bypass = 1
          gkLastToggle29 = 41
          printks "BASS PRE H: ON\n", 0
        else
          printks "BASS PRE H: OFF\n", 0
        endif
      ; DIGIVERB s2 (menu 9, CC76/slot2) is EMPTY for now — FREEZE lands here
      ; in phase 2 (slot order: NEBULA s4, SHIMMER s3, FREEZE s2, GRAINVERB s1,
      ; REVERB s0 — user mandate, REVERB moved to CC74/slot0, see below).
      elseif (gkCurrentMenu == 16) then
        ; REPEAT v2 (slot 2, toggle 23 — reuses WGuide bypass) — plain toggle
        $FX_TOGGLE(gkWGuide_bypass'23)
        if (gkWGuide_bypass < 0.5) then
          ; 2026-09-01: prob re-default removed; expr stays reset (gesture var)
          gkRPT_expr = 0
          ; 2026-09-25: generative effect - must NOT take the pedal (see giPedalClaimGenerative)
          if (giPedalClaimGenerative == 1) then
            gkLastToggle29 = 23
          endif
          printks "REPEAT: ON\n", 0
        else
          printks "REPEAT: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 18) then
        ; IR VERB slot 2 → row 2 (HALL)
        if (gkIRActiveRow == 2 && gkIRBypass < 0.5) then
          gkIRBypass = 1
        else
          gkIRActiveRow = 2
          gkIRBypass = 0
          gkLastToggle = 51
        endif
        printks "IR: row 2 variant %d\n", 0, gkIRSel[2]
      elseif (gkCurrentMenu == 11) then
        ; BASS menu slot 2 — BASS DISTORTION (toggle 42)
        $FX_TOGGLE(gkBDS_bypass'42)
        if (gkBDS_bypass < 0.5) then
          gkLastToggle29 = 42
          printks "BASS DIST: ON\n", 0
        else
          printks "BASS DIST: OFF\n", 0
        endif
      $LP_CH3_PRESS
      elseif (gkCurrentMenu == 17) then
        ; NAM slot 2 (centre) → row 2 (FENDER: Super Reverb -> Princeton edge)
        if (gkNAMActiveRow == 2 && gkNAMBypass < 0.5) then
          gkNAMBypass = 1
        else
          gkNAMActiveRow = 2
          gkNAMBypass = 0
          gkNAMProfileIdx = giNAMRowBase[2] + gkNAMRowSel[2]
          ; Arm the encoders on the amp sim so P2/P3 (IN/OUT gain)
          ; reach it. Every row does this — before 2026-08-11 only
          ; row 4's slot did, so engaging any other amp left the
          ; gains pointing at whatever was toggled previously.
          gkLastToggle = 37
        endif
        printks "NAM: row 2 variant %d\n", 0, gkNAMRowSel[2]
      elseif (gkCurrentMenu == 2) then
        ; BINSCATTER (SPECTRAL menu, slot 2) — 2026-07-26 replaces SPECDLY's
        ; old slot here (SPECDLY is now menu-10/DELAY only, not dual-homed).
        $FX_TOGGLE(gkBscBypass'67)
        if (gkBscBypass < 0.5) then
          printks "BINSCATTER: ON\n", 0
        else
          printks "BINSCATTER: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 12) then
        ; TONE EQ (SYNTH menu, slot 2) — toggle 58, instr 181. NO CC29 claim.
        ; Straight-through amp tone stack; 0.5 = flat on every band.
        $FX_TOGGLE(gkTEQ_bypass'58)
        if (gkTEQ_bypass < 0.5) then
          printks "TONE EQ: ON\n", 0
        else
          printks "TONE EQ: OFF\n", 0
        endif
      elseif (gkCurrentMenu == 15) then
        ; CHASE BLISS s2 — HABIT (toggle 36, instr 64). Its DSP and params are
        ; unchanged; 2026-07-13 adds a RECORD/FREEZE gesture on this button:
        ;
        ;   click (from off) : effect ON, ring buffer starts RECORDING at once
        ;   click (while on) : flip-flop the buffer — RECORDING <-> FROZEN.
        ;                      Frozen keeps playing/scanning the captured
        ;                      memory; the write head simply stops.
        ;   double-click     : effect OFF entirely (supersedes the single
        ;                      click's action, BLOOPER/glitch double-tap
        ;                      precedent).
        ;
        ; NOT a $FX_TOGGLE any more — the single click has to mean "flip the
        ; record gate", not "bypass". 0.32 s double-click window: long enough
        ; for a real human double-click, short enough that a deliberate
        ; single click is never swallowed.
        kHabNow times
        if (gkHab_lastTap > 0 && kHabNow - gkHab_lastTap < 0.32) then
          ; DOUBLE-CLICK -> off. Re-arm rec so the next power-on records.
          gkHabitBypass = 1
          gkHab_rec     = 1
          gkHab_lastTap = -10
          printks "HABIT: OFF (double-click)\n", 0
        else
          if (gkHabitBypass > 0.5) then
            gkHabitBypass  = 0
            gkHab_rec      = 1          ; ON = recording immediately
            gkLastToggle   = 36
            gkLastToggle29 = 36
            printks "HABIT: ON (REC)\n", 0
          else
            gkHab_rec = 1 - gkHab_rec   ; flip-flop rec <-> frozen
            if (gkHab_rec > 0.5) then
              printks "HABIT: REC\n", 0
            else
              printks "HABIT: FROZEN\n", 0
            endif
          endif
          gkHab_lastTap = kHabNow
        endif
      ; AIR s2 is EMPTY — SHIMMER (toggle 65, instr 188) was REMOVED
      ; 2026-07-13 at the user's call: a shimmer is achievable by routing the
      ; existing pitch shifter into reverb. No CC76 branch for menu 11 (AIR).
      endif


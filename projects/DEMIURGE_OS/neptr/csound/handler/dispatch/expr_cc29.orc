    ;==========================================================
    ; EXPRESSION PEDAL — CC29 (absolute 0-127)  +  EXPR-ROUTING SYSTEM
    ;==========================================================
    ; ONE pedal, MANY possible destinations. Where it goes is selected by a
    ; single sticky integer, gkLastToggle29 — the "expr target". kNorm is the
    ; pedal position 0..1.
    ;
    ; 2026-08-12: THE DESTINATION TABLE MOVED OUT OF THIS FILE. It used to be
    ; ~120 lines of `elseif (gkLastToggle29 == N) then <write a global>` right
    ; here. It is now the generated UDO ExprSet(kId, kNorm) in
    ; handler/expr_registry.orc, and this block is the one line below.
    ;
    ; NOTHING ABOUT THE PEDAL'S BEHAVIOUR CHANGED. The generator lifts every
    ; branch body verbatim, so all 35 destinations keep their exact ranges,
    ; including the two that are not simple assignments — id 26 (H90) branching
    ; on gkPresetBank, and id 10 (Leslie) writing two globals.
    ;
    ; WHY: a hardcoded chain can only ever be driven by the pedal. As a UDO the
    ; same destinations are reachable by anything that can name an id — a LiDAR,
    ; the gyro, a joystick axis, a web fader, an OSC /p write from the router.
    ; The pedal stops being privileged and becomes one source among many, and
    ; "sensor X does what the pedal does" needs no engine change at all.
    ;
    ; TWO things set the expr target, at the SAME level (both just write
    ; gkLastToggle29 — neither is special):
    ;   1. Toggling a CC29-using EFFECT sets it to that effect's id, so the
    ;      pedal "follows" the last effect you turned on (the normal behaviour).
    ;   2. A PRESET sets it when the preset activates, to DECLARE what the pedal
    ;      does for that preset. This is how a preset owns the expression pedal.
    ;
    ; Current presets (menu 0) all DECLARE target 30 = front-of-chain VOLUME
    ; (gkMasterVol, the reverb-swell stage in signal_chain.orc). Each preset
    ; also resets gkMasterVol = 1 on activation so you start at full.
    ;
    ; TO ADD A NEW EXPR TARGET: pick/use the effect's id, set
    ; gkLastToggle29 = <that id> in its toggle-ON branch, add the destination to
    ; the SOURCE block that feeds the generator — which, until a target needs to
    ; exist independently of the pedal, is expr_registry.orc's own source: the
    ; branch chain this file used to hold now lives in the generator's input.
    ; Then re-run `python3 tools/gen_expr_registry.py`.
    ;
    ; NOTE: id 0 = "pedal does nothing" (used by dry / presets that don't claim
    ; the pedal). Glitch group (20/23/24/25) shares one probability target.
    ;
    ; ARBITRATION: several effects claim the pedal on toggle-ON — 45 ONWARD
    ; (capture gate), 61 TRACKSYNTH (filter cutoff), 63 FEEDBACKER (intensity
    ; push), among others. There is NO priority table and none is needed: the
    ; standing LAST-TOGGLED-WINS sticky rule resolves them. Each claim is a
    ; plain `gkLastToggle29 = <id>` in its toggle-ON branch, so whichever effect
    ; you switched on most recently owns the pedal, and the others keep working
    ; from their encoders (their expr params just stop moving). Turning an
    ; effect OFF does NOT release the claim — the pedal keeps pointing at it
    ; until something else claims it; writing to a bypassed effect's param is
    ; harmless. FEEDBACKER additionally resets its gesture state (gkFBK_expr)
    ; on toggle-ON so a stale pedal position from a previous owner can never
    ; slam it on engage. COMPRESSOR (57), TONE EQ (58), CHAINFB (60),
    ; PLLFUZZ (62) and DIFFUSION (64) make NO claim on purpose — they leave the
    ; pedal where it was.
    ;==========================================================
    ; NOTE: no `endif` here. This is the last branch of the kCC chain that
    ; opens in nav.orc, and that chain is closed by midi_handler.orc
    ; ("end CC dispatch"). The `endif` this file used to end with belonged to
    ; the INNER gkLastToggle29 chain, which now lives inside the UDO.
    elseif (kCC == 29) then

      ; SOFT TAKEOVER (2026-09-01). When the pedal's TARGET changes (an effect
      ; toggle claims it, a preset re-declares it), the pedal's current
      ; physical resting position must NOT be written into the new target.
      ; Before this, engaging MOOD II with the pedal at heel instantly scrubbed
      ; gkMD2_Clock to 0 (bottom rung, -24) — the reported "self-oscillating
      ; mute sound when MOOD II turns on" — and every other claim had the same
      ; stale-slam exposure. Now: after any owner change the pedal is DISARMED;
      ; the first CC29 only records the position, and writes resume once the
      ; pedal actually MOVES (> ~4 CC steps from where it was when claimed).
      kExprOwnPrv init -1
      kExprArmed  init 1
      kExprLast   init -1
      if (gkLastToggle29 != kExprOwnPrv) then
        kExprArmed  = 0
        kExprOwnPrv = gkLastToggle29
        kExprLast   = -1
      endif
      if (kExprArmed == 0) then
        if (kExprLast < 0) then
          kExprLast = kNorm            ; first value after a claim: latch only
        elseif (abs(kNorm - kExprLast) > 0.03) then
          kExprArmed = 1               ; real movement — pedal takes over
        endif
      endif
      if (kExprArmed == 1) then
        ; SOUNDMAGIC rack (75..98): ONE range branch before the registry —
        ; pedal writes the focused effect's pedal param absolutely.
        if (gkLastToggle29 >= 75 && gkLastToggle29 <= 98) then
          SM_PedalSet gkLastToggle29, kNorm
        else
          ExprSet gkLastToggle29, kNorm
        endif
      endif

      ; HYPERGUITAR: nothing to do here any more.
      ;
      ; This used to forward the pedal to the guitar UNCONDITIONALLY whenever
      ; the guitar was present, which meant the treadle drove the guitar AND
      ; whatever else owned it at the same time — with the menu-0 presets
      ; parking it on target 30 (gkMasterVol), the player got a volume pedal
      ; that also bent the guitar, and no way to get it off volume.
      ;
      ; The guitar now has a proper ExprSet target (74, gkHgPedal) and claims
      ; the pedal on lane-0 engage like every other effect, so the ExprSet call
      ; above already routes the pedal correctly and instr 98 relays it.

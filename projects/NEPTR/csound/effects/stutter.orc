; GLITCH MENU v2 (2026-07-15) — TOTAL REBUILD ("it sounds like shit" -> clocked,
; tight, probabilistic, pedal-triggered). Replaces the 2026-06 stutter family.
; Old file archived: effects/_retired/stutter_v1_glitchmenu.orc
;
;   instr 89   GLT TICK   shared tick generator (always-on, new — verified free)
;   instr 90   BBCUT      breakbeat cutter, rebuilt (toggle 20, GLITCH s1)
;   instr 91   REPEAT     beat repeat / stutter rolls (toggle 23, GLITCH s2)
;                         REPLACES WGuide — same instr/toggle/buses
;   instr 92   CHOP       tick-synced trance gate / dropouts (toggle 24, s3)
;                         REPLACES SqMod — same instr/toggle/buses
;   instr 93   RVRS       reverse stutter (toggle 25, GLITCH s4)
;                         REPLACES FreqShift — same instr/toggle/buses
;   (GLITCH s0 = IMPULSE, toggle 48 / instr 156, lives in impulse_synth.orc —
;    rewired to the same contract, see that file.)
;
; KILL/KEEP VERDICTS (v1 -> v2):
;   BBCUT    REBUILT. bbcutm/bbcuts (the Csound BBCut opcode, see
;            /Library/Frameworks/CsoundLib64.framework/.../Manual/examples/
;            bbcuts.csd) is i-rate-only and free-runs from instrument start —
;            it can never be phase-anchored to the loop tick, which is why the
;            old one always felt smeared. v2 reimplements the BBCut IDEAS
;            (phrase-based cut sequences, "3+3R+2" style unit cuts with
;            repeats, one-cell stutter rolls on the phrase tail, exponential
;            grain envelopes — per the bbcutm manual's own description) as a
;            sample-accurate engine launched ON the shared tick.
;   WGUIDE   KILLED (random-pitch waveguide pluck: broken-windchime texture,
;            neither tight nor glitchy; its dice-gate was the only rhythm).
;   SQMOD    KILLED (4-14 Hz square AM = ring-modish flutter, unrelated to the
;            beat; thin and constantly mistaken for a broken cable).
;   FSHIFT   KILLED (random +/-2400-cent freqshift wash; v1 itself weighted it
;            to 20% chance because full-on it was mush. Detuned mush != glitch).
;   Replacements REPEAT/CHOP/RVRS reuse the freed instr numbers 91/92/93,
;   toggles 23/24/25 and the ga* bus names, so signal_chain.orc and the
;   existing bypass/mix dispatch keep working UNCHANGED until integration.
;
; ============================ THE GLITCH CONTRACT ===========================
; every effect in this menu:
;   1. CLOCKED: events launch ONLY on boundaries of the shared subdivision
;      tick gkChaseBlissSubdivSec (chase_bliss_clock.orc — seconds per tick,
;      musically anchored to the recorded loop; 120 BPM grid when no loop).
;      Arm/fire split exactly like blooper.orc: gap/probability logic ARMS,
;      the next tick boundary FIRES. Tick detection is loop-phase-anchored
;      when the master loop is playing (tick index = int(loopPos / tickSmp),
;      same grid blooper rides), wall-clock-gridded otherwise. One shared
;      generator (instr 89) so all five effects fire IN PHASE with each other.
;   2. PROBABILITY: per-effect gk*_prob knob, init 0.5. Each tick the effect
;      draws dice < prob -> fire. p=0 never, p=1 every tick.
;   3. EXPRESSION PEDAL = INSTANT TRIGGER: per-effect gk*_expr (CC29, sticky
;      last-toggled-wins claim). Heel (<0.10) adds nothing. Crossing 0.10
;      upward FORCE-FIRES on the very next tick (bypassing probability).
;      Held above threshold, the pedal FORCES a fraction of ticks that scales
;      with position; at toe (>=0.95) every tick fires, and per-effect
;      intensity (roll speed / roll count / chop rate / reverse weirdness)
;      also scales with pedal position. Wall-to-wall glitch at toe.
;      Pre-integration compatibility: the old hold gesture's gk*_held flags
;      are honoured as pedal-toe equivalents (hold = force every tick).
;   4. POST-LOOPER: integration moves this whole block (and IMPULSE) AFTER
;      the looper stage in signal_chain.orc so it mangles loops + live.
;
; ======================= INTEGRATION NOTES (shared files) ===================
; The integration agent owns all shared files. This block is the complete,
; per-effect wiring spec. NOTHING below requires edits to THIS file.
;
; A. signal_chain.orc — MOVE, don't rewrite: relocate the existing contiguous
;    glitch block (gaBBcut_* / gaWGuide_* / gaSqMod_* / gaFShift_* FX_BYPASS
;    stanzas, currently ~lines 551-576) AND the IMPULSE stanza (~lines
;    453-457, instr 156) to AFTER the looper stage, keeping their relative
;    order (IMPULSE first, then 90/91/92/93). The stanzas themselves are
;    unchanged — same buses, same instr numbers, same FX_BYPASS macros.
;
; B. handler_menus.orc, menu 9 buttons — the old press/hold "chance group"
;    gesture is RETIRED. Remove ALL of:
;      * $GLITCH_PRESS lines (CC75 menu9 / CC76 menu9 / CC71 menu9 / CC72
;        menu9 — toggles 20/23/24/25) and their "DOWN" printks;
;      * the four GLITCH RELEASE handlers (the whole `kCC==75 && kVal==0 &&
;        gkBBcut_held` ... `kCC==72 && kVal==0 && gkFShift_held` chain);
;      * (globals.orc may keep the GLITCH_PRESS/GLITCH_RELEASE macros and the
;        gk*_held/_pressT/_prevByp/giGLITCH_holdThresh decls — they become
;        dead but harmless; remove at leisure.)
;    Replace each with a plain toggle + CC29 claim + toggle-on defaults:
;      CC75 menu9 (s1): $FX_TOGGLE(gkBBcut_bypass'20)
;                       if on: gkBBcut_prob=0.5, gkBBcut_expr=0,
;                              gkLastToggle29=20, printks BBCUT ON/OFF
;      CC76 menu9 (s2): $FX_TOGGLE(gkWGuide_bypass'23)   ; REPEAT
;                       if on: gkRPT_prob=0.5, gkRPT_expr=0,
;                              gkLastToggle29=23, printks REPEAT ON/OFF
;      CC71 menu9 (s3): $FX_TOGGLE(gkSqMod_bypass'24)    ; CHOP
;                       if on: gkCHP_prob=0.5, gkCHP_expr=0,
;                              gkLastToggle29=24, printks CHOP ON/OFF
;      CC72 menu9 (s4): $FX_TOGGLE(gkFShift_bypass'25)   ; RVRS
;                       if on: gkRVS_prob=0.5, gkRVS_expr=0,
;                              gkLastToggle29=25, printks RVRS ON/OFF
;
; C. handler_menus.orc, CC20 (P2) — REMOVE the shared-BPM branch
;      `elseif (gkLastToggle == 20 || 23 || 24 || 25) then
;         $ENC_ROT(gkGLITCH_bpm'gkStepGLITCH_bpm'40'400)`
;    (the tick now comes from the CHASE BLISS clock; the glitch menu no longer
;    has a BPM of its own). Replace with per-effect PROBABILITY — the uniform
;    P2 = prob convention for the whole menu:
;      elseif (gkLastToggle == 20) then $ENC_ROT(gkBBcut_prob'0.05'0'1)
;      elseif (gkLastToggle == 23) then $ENC_ROT(gkRPT_prob'0.05'0'1)
;      elseif (gkLastToggle == 24) then $ENC_ROT(gkCHP_prob'0.05'0'1)
;      elseif (gkLastToggle == 25) then $ENC_ROT(gkRVS_prob'0.05'0'1)
;    (IMPULSE prob on CC20 too — see impulse_synth.orc notes.)
;
; D. handler_menus.orc, CC23 (P3) — keep toggle-20 -> gkBBcut_fltMix as-is.
;    REPLACE the dead v1 branches:
;      toggle 23: gkWGuide_fb        -> $ENC_ROT(gkRPT_char'0.02'0'1)
;      toggle 24: gkSqMod_freq       -> $ENC_ROT(gkCHP_rate'0.02'0'1)
;      toggle 25: gkFShift_amount    -> $ENC_ROT(gkRVS_len'0.02'0'1)
;
; E. handler_menus.orc, CC25 (slot-1 rotation) P4 fills — ADD (all unused
;    lastToggle values on CC25 today):
;      elseif (gkLastToggle == 23) then $ENC_ROT(gkRPT_len'1'0'2)   ; stepped
;      elseif (gkLastToggle == 24) then $ENC_ROT(gkCHP_shape'0.02'0'1)
;    P1 mixes need NO change: 20 -> CC25 gkBBcut_drywet, 23 -> CC26
;    gkWGuide_drywet, 24 -> CC21 gkSqMod_drywet, 25 -> CC22 gkFShift_drywet
;    all already dispatch, and v2 reuses those globals as its mix knobs.
;
; F. handler_menus.orc, CC29 — REMOVE the glitch-group chance branch
;      `elseif (gkLastToggle29 == 20 || 23 || 24 || 25) then
;         gkGLITCH_chance = kNorm`
;    and add per-effect instant-trigger targets:
;      elseif (gkLastToggle29 == 20) then gkBBcut_expr = kNorm
;      elseif (gkLastToggle29 == 23) then gkRPT_expr  = kNorm
;      elseif (gkLastToggle29 == 24) then gkCHP_expr  = kNorm
;      elseif (gkLastToggle29 == 25) then gkRVS_expr  = kNorm
;    (and gkLastToggle29 == 48 -> gkIMP_expr, see impulse_synth.orc.)
;    gkGLITCH_chance / gkGLITCH_bpm stay declared in globals.orc (the CHASE
;    BLISS clock still WRITES gkGLITCH_bpm) but nothing in this file reads
;    either any more.
;
; G. handler_menus.orc, TOTAL GLITCH (bass bank, CC76 menu 0) — the old body
;    (x5 gkGLITCH_bpm, gkGLITCH_chance=0.70, gkBBcut_held=1, WGuide off) is
;    obsolete. Replace with the v2 bundle:
;      ON:  gkBBcut_bypass=0  gkSqMod_bypass=0  gkFShift_bypass=0
;           gkBBcut_prob=0.7  gkCHP_prob=0.7    gkRVS_prob=0.5
;           gkWGuide_bypass=1 (REPEAT off — three at once is already dense)
;      OFF: all four bypassed, probs back to 0.5.
;    gkTotalGlitch bookkeeping/printks unchanged; gkTotalGlitch_bpmSav dead.
;
; H. handler_osc.orc — replace the /bbc_bpm, /wgd_bpm, /sqm_bpm, /fsh_bpm and
;    /glt_bpm + /glt_chance sends. Keep the *_state and *_mix sends (same
;    globals). New per-effect sends:
;      /bbc_prob gkBBcut_prob   /bbc_flt keeps gkBBcut_fltMix
;      /rpt_prob gkRPT_prob     /rpt_char gkRPT_char   /rpt_len gkRPT_len
;      /chp_prob gkCHP_prob     /chp_rate gkCHP_rate   /chp_shape gkCHP_shape
;      /rvs_prob gkRVS_prob     /rvs_len  gkRVS_len
;
; I. neptrPhase4_UI.py — menu 9 slot labels/params:
;      s1 ("bbc","BBCUT")  params [("mix",0,1),("prob",0,1),("filter",0,1)]
;      s2 ("rpt","REPEAT") params [("mix",0,1),("prob",0,1),("char",0,1),
;                                  ("len",0,2)]
;      s3 ("chp","CHOP")   params [("depth",0,1),("prob",0,1),("rate",0,1),
;                                  ("shape",0,1)]
;      s4 ("rvs","RVRS")   params [("mix",0,1),("prob",0,1),("len",0,1)]
;
; J. IDs: NO new toggles consumed (20/23/24/25/48 reused in place). ONE new
;    instr: 89 (tick generator, always-on) — verified unused across the rig
;    (next-free bookkeeping in midi_handler.orc: 59/65 toggles and 182/188
;    instrs REMAIN free; grainverb.orc claims 65/188 but is not #include'd).
; ============================================================================
;
; EXECUTION-ORDER NOTE (house convention: ascending instr number = k-cycle
; order): instr 89 < 90..93 < 100 (clock) < 156 (IMPULSE) < 209+ (looper).
; So 89 reads gkChaseBlissSubdivSec and the looper read-pointer as they stood
; at the END of the previous k-block — the same one-block (2.7 ms) staleness
; blooper.orc/mood2.orc already accept — and every glitch effect reads THIS
; block's freshly computed gkGLT_tick. All five fire on the identical grid.

;==============================================================
; self-declared globals (nothing added to globals.orc)
;==============================================================
; shared tick (instr 89 writes, 90-93 + 156 read)
gkGLT_tick    init 0      ; 1 for exactly one k-block per tick boundary
gkGLT_tickN   init 0      ; running tick counter (bench telemetry)
gkGLT_tickSec init 0.125  ; the tick length actually in use, seconds
gkGLT_src     init 0      ; 0 = wall-clock grid, 1 = loop-phase-anchored

; BBCUT v2 (toggle 20 — gkBBcut_bypass/drywet/fltMix/_held live in globals.orc)
gkBBcut_prob  init 0.5
gkBBcut_expr  init 0
gkBBcut_evN   init 0      ; bench: event counter
gkBBcut_evT   init 0      ; bench: tick index of last fire

; REPEAT (toggle 23 — reuses gkWGuide_bypass + gkWGuide_drywet as mix)
gkRPT_prob    init 0.5
gkRPT_expr    init 0
gkRPT_char    init 0.5    ; 0 clean flat repeats .. 1 decaying pitch-mangled
gkRPT_len     init 1      ; stepped 0 = 1/2 tick, 1 = 1 tick, 2 = 2 ticks
gkRPT_evN     init 0
gkRPT_evT     init 0

; CHOP (toggle 24 — reuses gkSqMod_bypass + gkSqMod_drywet as gate depth)
gkCHP_prob    init 0.5
gkCHP_expr    init 0
gkCHP_rate    init 0.5    ; chop-rate bias: 0 slow halves .. 1 fast buzz
gkCHP_shape   init 0.5    ; gate duty: 0 choppy/short .. 1 long/pumping
gkCHP_evN     init 0
gkCHP_evT     init 0

; RVRS (toggle 25 — reuses gkFShift_bypass + gkFShift_drywet as mix)
gkRVS_prob    init 0.5
gkRVS_expr    init 0
gkRVS_len     init 0.35   ; 0 = tight 1-tick flips .. 1 = 2-tick + speed weird
gkRVS_evN     init 0
gkRVS_evT     init 0

; circular capture buffers — 2^18 samples = 5.46 s @48k, plenty for 8 ticks
; at the slowest legal tick. One stereo pair per buffer-based effect.
giGLT_bufLen  init 262144
giBBC_bufL ftgen 0, 0, 262144, 2, 0
giBBC_bufR ftgen 0, 0, 262144, 2, 0
giRPT_bufL ftgen 0, 0, 262144, 2, 0
giRPT_bufR ftgen 0, 0, 262144, 2, 0
giRVS_bufL ftgen 0, 0, 262144, 2, 0
giRVS_bufR ftgen 0, 0, 262144, 2, 0

;==============================================================
; GLT_FireK — the contract's arm/fire decision, shared by all
;   kFire, kInt  GLT_FireK  kTick, kProb, kExpr, kHeld
; kFire: 1 = launch an event THIS tick. kInt 0..1: pedal intensity
; (0 when the pedal isn't the reason we fired). Rising-edge force:
; crossing 0.10 upward guarantees a fire on the next tick. Each call
; site is its own opcode instance -> independent dice + edge state.
;==============================================================
opcode GLT_FireK, kk, kkkk
  kTick, kProb, kExpr, kHeld xin
  kXP    = (kHeld > 0.5 ? 1 : limit:k(kExpr, 0, 1))
  kPed   = limit:k((kXP - 0.10) / 0.85, 0, 1)   ; 0 below threshold, 1 @ ~0.95
  kEdgeP init 0                                  ; pending rising-edge force
  kXPrev init 0
  if (kXP > 0.10 && kXPrev <= 0.10) then
    kEdgeP = 1
  endif
  kXPrev = kXP
  kFire = 0
  kInt  = 0
  if (kTick == 1) then
    kDice random 0, 1
    if (kEdgeP == 1) then                        ; instant trigger, no dice
      kFire  = 1
      kInt   = (kPed > 0.2 ? kPed : 0.2)
      kEdgeP = 0
    elseif (kPed >= 0.999) then                  ; toe: every tick, flat out
      kFire = 1
      kInt  = 1
    elseif (kPed > 0 && kDice < kPed) then       ; pedal density zone
      kFire = 1
      kInt  = kPed
    elseif (kDice < kProb) then                  ; the probability knob
      kFire = 1
    endif
  endif
  xout kFire, kInt
endop

;==============================================================
; instr 89 — GLT TICK (always-on)
; Loop playing -> tick boundaries at master-loop buffer positions
; 0, T, 2T ... (phase-anchored, follows varispeed/stop/clear for
; free — blooper.orc's grid). No loop -> wall-clock accumulator on
; gkChaseBlissSubdivSec (120 BPM default grid from the clock file).
;==============================================================
instr 89
  kSub   limit gkChaseBlissSubdivSec, 0.05, 2.0
  gkGLT_tickSec = kSub
  kSubSmp = kSub * sr

  kTick  = 0
  kPrv   init -1

  if (gkLpMasterLen > 0 && (gkLpState1 == 2 || gkLpState1 == 3) && gkLpLen1 > 1) then
    ; ---- loop-phase-anchored grid ----
    kPos = gkLpRdPos1
    if (kPos >= gkLpLen1) then
      kPos = kPos - gkLpLen1 * int(kPos / gkLpLen1)
    endif
    if (kPos < 0) then
      kPos = kPos + gkLpLen1
    endif
    kIdx = int(kPos / kSubSmp)
    if (kPrv >= 0 && kIdx != kPrv) then
      kTick = 1
    endif
    kPrv = kIdx
    gkGLT_src = 1
  else
    ; ---- wall-clock grid ----
    kAcc init 0
    kAcc = kAcc + ksmps / sr
    if (kAcc >= kSub) then
      kAcc  = kAcc - kSub
      kAcc  = (kAcc >= kSub ? 0 : kAcc)   ; guard vs a sudden shorter tick
      kTick = 1
    endif
    kPrv = -1                             ; re-prime loop tracking on return
    gkGLT_src = 0
  endif

  if (kTick == 1) then
    gkGLT_tickN = gkGLT_tickN + 1
  endif
  gkGLT_tick = kTick

#ifdef GLTDBG
  if (kTick == 1) then
    printks "GLTTICK t=%.5f n=%.0f sub=%.5f src=%.0f\n", 0, timeinsts:k(), gkGLT_tickN, kSub, gkGLT_src
  endif
#endif
endin
schedule 89, 0, -1

;==============================================================
; instr 90 — BBCUT v2, breakbeat cutter (toggle 20, GLITCH s1)
; Reads gaBBcut_sendL/R, writes gaBBcut_wetL/R (unchanged buses).
;
; A phrase is 8 ticks. On a fired tick the engine launches either:
;   CUT  — jump back a whole number of ticks (tick-aligned, that is
;          the bbcut feel) and replay 1-3 ticks from there, with a
;          probable immediate repeat of the same cut ("3R");
;   ROLL — phrase-tail (last 2 ticks) or high pedal intensity: a
;          one-cell stutter (bbcutm's istutterchance idea): the last
;          tick/kDiv slice retriggers for 1-2 ticks with a slight
;          per-cell decay — the classic snare-roll fill.
; Between events the live signal passes untouched. The v1 resonant
; randomh filter survives on the CUT LAYER only (gkBBcut_fltMix).
; P1 CC25 gkBBcut_drywet · P2 CC20 gkBBcut_prob · P3 CC23 fltMix
; CC29 gkBBcut_expr: force-fire; intensity raises roll chance/speed.
;==============================================================
instr 90
  iLen  = giGLT_bufLen
  iXf   = 0.004 * sr            ; 4 ms event-edge de-click ramps

  aInL = gaBBcut_sendL
  aInR = gaBBcut_sendR

  kFire, kInt GLT_FireK gkGLT_tick, gkBBcut_prob, gkBBcut_expr, gkBBcut_held

  ; ---- per-instance state ----
  kWr     init 0                ; write head
  kRem    init 0                ; samples left in current event
  kELen   init 1
  kRd     init 0                ; read head (cut playback)
  kSegSt  init 0                ; segment start (for repeats / roll cells)
  kSegLen init 1
  kCellRem init 0               ; roll: samples left in current cell
  kRepLeft init 0               ; cut: whole-segment repeats left
  kIsRoll init 0
  kG      init 1                ; roll decay gain
  kOffT   init 0                ; cut jump-back, whole ticks (telemetry)
  kTickCnt init 0
  kSubSmp = gkGLT_tickSec * sr
  kSubSmp = (kSubSmp < 480 ? 480 : kSubSmp)

  if (gkGLT_tick == 1) then
    kTickCnt = kTickCnt + 1
  endif

  ; ---------- event launch (tick boundary == block start) ----------
  if (kFire == 1 && kRem <= 0) then
    kPP = kTickCnt - 8 * int(kTickCnt / 8)      ; phrase position 0..7
    kU  random 0, 1
    kU2 random 0, 1
    kU3 random 0, 1
    kRollCh = 0.18 + 0.55 * kInt + (kPP >= 6 ? 0.35 : 0)
    if (kU < kRollCh) then
      ; ---- STUTTER ROLL ----
      kIsRoll = 1
      kDiv    = (kU2 < 0.5 ? 2 : 4)
      kDiv    = (kInt > 0.75 ? kDiv * 2 : kDiv) ; toe: up to 1/8-tick buzz
      kSegLen = int(kSubSmp / kDiv)
      kELen   = kSubSmp * (kU3 < 0.5 ? 1 : 2)   ; roll for 1 or 2 ticks
      kSegSt  = kWr - kSegLen                   ; the freshest cell
      kOffT   = 0
      kG      = 1
    else
      ; ---- CUT (bbcut unit cut with repeat) ----
      kIsRoll = 0
      ; length 1-3 ticks, weighted toward 3 then 2 (the "3+3R+2" flavour)
      kCL     = (kU2 < 0.40 ? 3 : (kU2 < 0.75 ? 2 : 1))
      kSegLen = kSubSmp * kCL
      ; jump back a whole number of ticks: own start, or 4/8 ticks ago
      kOffT   = (kU3 < 0.5 ? kCL : (kU3 < 0.8 ? 4 : 8))
      kSegSt  = kWr - kSubSmp * kOffT
      kRepLeft = (kU < kRollCh + (1 - kRollCh) * 0.55 ? 1 : 0) ; "3R" repeat
      kELen   = kSegLen * (1 + kRepLeft)
      kG      = 1
    endif
    kSegSt  = kSegSt - iLen * int(kSegSt / iLen)
    kSegSt  = (kSegSt < 0 ? kSegSt + iLen : kSegSt)
    kRd      = kSegSt
    kCellRem = kSegLen
    kRem     = kELen
    gkBBcut_evN = gkBBcut_evN + 1
    gkBBcut_evT = gkGLT_tickN
#ifdef GLTDBG
    printks "BBCFIRE t=%.5f n=%.0f roll=%.0f seg=%.0f len=%.0f off=%.0f int=%.2f\n", 0, timeinsts:k(), gkBBcut_evN, kIsRoll, kSegLen, kELen, kOffT, kInt
#endif
  endif

  ; ---------- sample loop: record + (maybe) replace with cut ----------
  aCutL init 0
  aCutR init 0
  aEnv  init 0
  kS = 0
  while kS < ksmps do
    kInL vaget kS, aInL
    kInR vaget kS, aInR
    tablew kInL, kWr, giBBC_bufL
    tablew kInR, kWr, giBBC_bufR
    kWr = kWr + 1
    kWr = (kWr >= iLen ? 0 : kWr)

    kEnvV = 0
    kCL_  = 0
    kCR_  = 0
    if (kRem > 0) then
      ; trapezoid: 4 ms in/out on the WHOLE event, plus 2 ms per roll cell
      kIn   = kELen - kRem
      kEnvV = kIn / iXf
      kEnvV = (kRem / iXf < kEnvV ? kRem / iXf : kEnvV)
      kEnvV = limit:k(kEnvV, 0, 1)
      if (kIsRoll == 1) then
        kCIn  = kSegLen - kCellRem
        kCE   = kCIn / (0.002 * sr)
        kCE2  = kCellRem / (0.002 * sr)
        kCE   = (kCE2 < kCE ? kCE2 : kCE)
        kEnvV = kEnvV * limit:k(kCE, 0, 1)
      endif

      kI0 = int(kRd)
      kFr = kRd - kI0
      kI1 = kI0 + 1
      kI1 = (kI1 >= iLen ? 0 : kI1)
      kB0 tab kI0, giBBC_bufL
      kB1 tab kI1, giBBC_bufL
      kCL_ = (kB0 + (kB1 - kB0) * kFr) * kG
      kB0 tab kI0, giBBC_bufR
      kB1 tab kI1, giBBC_bufR
      kCR_ = (kB0 + (kB1 - kB0) * kFr) * kG

      kRd = kRd + 1
      kRd = (kRd >= iLen ? kRd - iLen : kRd)
      kCellRem = kCellRem - 1
      if (kCellRem <= 0) then
        if (kIsRoll == 1) then
          kRd = kSegSt                        ; retrigger the roll cell
          kG  = kG * 0.88                     ; per-cell decay = real roll
        elseif (kRepLeft > 0) then
          kRd = kSegSt                        ; "3R": repeat the whole cut
          kRepLeft = kRepLeft - 1
          kG = kG * 0.96
        endif
        kCellRem = kSegLen
      endif
      kRem = kRem - 1
    endif

    aCutL[kS] = kCL_
    aCutR[kS] = kCR_
    aEnv[kS]  = kEnvV
    kS = kS + 1
  od

  ; v1's tick-hopping resonant-filter idea, kept on the cut layer only —
  ; but rebuilt on butterbp. v1's `resonz ..., 2` (RMS scaling) goes NaN on
  ; this cf sweep (REPRODUCED in the bench on all-zero input: the swept
  ; coefficients poison the chain because 0 * NaN = NaN, so even fltMix = 0
  ; railed the output). butterbp is unconditionally stable across the sweep.
  kTRate = 1 / gkGLT_tickSec
  kcfh  randomh 200, 8000, kTRate
  kcf   lineto kcfh, gkGLT_tickSec
  kcf   limit kcf, 200, 8000
  aFltL butterbp aCutL, kcf, kcf * 0.7
  aFltR butterbp aCutR, kcf, kcf * 0.7
  aCutL ntrpol aCutL, aFltL * 1.4, gkBBcut_fltMix
  aCutR ntrpol aCutR, aFltR * 1.4, gkBBcut_fltMix

  ; event REPLACES the stream (cut in, dry out), then master dry/wet
  aRepL = aInL + (aCutL - aInL) * aEnv
  aRepR = aInR + (aCutR - aInR) * aEnv
  kMix  portk limit:k(gkBBcut_drywet, 0, 1), 0.01
  aMix  interp kMix
  aOutL = aInL + (aRepL - aInL) * aMix
  aOutR = aInR + (aRepR - aInR) * aMix
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaBBcut_wetL = aOutL
  gaBBcut_wetR = aOutR
endin

;==============================================================
; instr 91 — REPEAT, beat repeat / stutter rolls (toggle 23, s2)
; REPLACES WGuide. Reads gaWGuide_sendL/R, writes gaWGuide_wetL/R,
; mix knob = gkWGuide_drywet (existing CC26 dispatch keeps working).
;
; On fire: capture the audio that just played (the last 1/2, 1 or 2
; ticks, tick-aligned) and roll it 3-16 times. Per-event character
; draw, biased by gkRPT_char:
;   FLAT   clean robot repeats
;   DECAY  each repeat quieter (fading echo-stutter)
;   PITCH  each repeat ~a semitone lower (tape-drop roll)
;   HALVE  each repeat half as long (accelerating buildup)
; P1 CC26 mix · P2 CC20 gkRPT_prob · P3 CC23 gkRPT_char ·
; P4 CC25 gkRPT_len (stepped 0/1/2 = half/one/two ticks)
; CC29 gkRPT_expr: force-fire; intensity = more rolls, wilder modes;
; at toe a new event may retrigger every tick (wall-to-wall).
;==============================================================
instr 91
  iLen = giGLT_bufLen
  iXf  = 0.003 * sr

  aInL = gaWGuide_sendL
  aInR = gaWGuide_sendR

  kFire, kInt GLT_FireK gkGLT_tick, gkRPT_prob, gkRPT_expr, gkWGuide_held

  kWr     init 0
  kRem    init 0                 ; rolls remaining (counts repeats)
  kSegSt  init 0
  kSegLen init 1
  kCellRem init 0
  kRd     init 0
  kSpd    init 1                 ; current read speed
  kSpdMul init 1                 ; per-repeat speed multiplier (PITCH mode)
  kLenMul init 1                 ; per-repeat length multiplier (HALVE mode)
  kGDec   init 1                 ; per-repeat gain multiplier (DECAY mode)
  kG      init 1
  kSubSmp = gkGLT_tickSec * sr
  kSubSmp = (kSubSmp < 480 ? 480 : kSubSmp)

  ; retrigger rule: idle -> any fire starts an event; busy -> only pedal-toe
  ; intensity may steal (guarantees wall-to-wall at toe, never self-chokes)
  kBusy = (kRem > 0 || kCellRem > 0 ? 1 : 0)
  if (kFire == 1 && (kBusy == 0 || kInt >= 0.999)) then
    kLsel   = int(limit:k(gkRPT_len, 0, 2))
    kSegLen = kSubSmp * (kLsel == 0 ? 0.5 : (kLsel == 1 ? 1 : 2))
    kSegLen = int(kSegLen)
    kSegSt  = kWr - kSegLen
    kSegSt  = kSegSt - iLen * int(kSegSt / iLen)
    kSegSt  = (kSegSt < 0 ? kSegSt + iLen : kSegSt)
    kU  random 0, 1
    kU2 random 0, 1
    ; rolls: 3..9 from dice, up to +7 from pedal intensity
    kRem = 3 + int(kU * 6.99) + int(kInt * 7)
    ; character morph: low char favours FLAT, high char favours mangle
    kCh = limit:k(gkRPT_char + kInt * 0.3, 0, 1)
    if (kU2 < 0.55 - 0.45 * kCh) then
      kSpdMul = 1        ; FLAT
      kLenMul = 1
      kGDec   = 1
    elseif (kU2 < 0.75 - 0.25 * kCh) then
      kSpdMul = 1        ; DECAY
      kLenMul = 1
      kGDec   = 0.72 + 0.16 * (1 - kCh)
    elseif (kU2 < 0.90) then
      kSpdMul = 0.9439   ; PITCH: -1 semitone per repeat
      kLenMul = 1
      kGDec   = 0.97
    else
      kSpdMul = 1        ; HALVE: accelerating buildup
      kLenMul = 0.5
      kGDec   = 1
    endif
    kSpd     = 1
    kG       = 1
    kRd      = kSegSt
    kCellRem = kSegLen
    gkRPT_evN = gkRPT_evN + 1
    gkRPT_evT = gkGLT_tickN
#ifdef GLTDBG
    printks "RPTFIRE t=%.5f n=%.0f seg=%.0f rolls=%.0f spdm=%.4f lenm=%.2f gdec=%.2f int=%.2f\n", 0, timeinsts:k(), gkRPT_evN, kSegLen, kRem, kSpdMul, kLenMul, kGDec, kInt
#endif
  endif

  aRptL init 0
  aRptR init 0
  aEnv  init 0
  kS = 0
  while kS < ksmps do
    kInL vaget kS, aInL
    kInR vaget kS, aInR
    tablew kInL, kWr, giRPT_bufL
    tablew kInR, kWr, giRPT_bufR
    kWr = kWr + 1
    kWr = (kWr >= iLen ? 0 : kWr)

    kEnvV = 0
    kRL_  = 0
    kRR_  = 0
    if (kCellRem > 0) then
      ; per-cell 3 ms edges (kSegLen tracks the CURRENT cell length)
      kIn   = kSegLen - kCellRem
      kE1   = kIn / iXf
      kE2   = kCellRem / iXf
      kEnvV = limit:k((kE2 < kE1 ? kE2 : kE1), 0, 1)

      kI0 = int(kRd)
      kFr = kRd - kI0
      kI1 = kI0 + 1
      kI1 = (kI1 >= iLen ? 0 : kI1)
      kB0 tab kI0, giRPT_bufL
      kB1 tab kI1, giRPT_bufL
      kRL_ = (kB0 + (kB1 - kB0) * kFr) * kG
      kB0 tab kI0, giRPT_bufR
      kB1 tab kI1, giRPT_bufR
      kRR_ = (kB0 + (kB1 - kB0) * kFr) * kG

      kRd = kRd + kSpd
      kRd = (kRd >= iLen ? kRd - iLen : kRd)
      kCellRem = kCellRem - 1
      if (kCellRem <= 0 && kRem > 0) then
        ; next repeat
        kRem  = kRem - 1
        kSpd  = kSpd * kSpdMul
        kG    = kG * kGDec
        kSegLen = int(kSegLen * kLenMul)
        kSegLen = (kSegLen < 0.02 * sr ? int(0.02 * sr) : kSegLen)
        kRd   = kSegSt
        kCellRem = (kRem > 0 ? kSegLen : 0)
      endif
    endif

    aRptL[kS] = kRL_
    aRptR[kS] = kRR_
    aEnv[kS]  = kEnvV
    kS = kS + 1
  od

  ; repeats REPLACE the stream while sounding, then master mix
  aRepL = aInL + (aRptL - aInL) * aEnv
  aRepR = aInR + (aRptR - aInR) * aEnv
  kMix  portk limit:k(gkWGuide_drywet, 0, 1), 0.01
  aMix  interp kMix
  aOutL = aInL + (aRepL - aInL) * aMix
  aOutR = aInR + (aRepR - aInR) * aMix
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaWGuide_wetL = aOutL
  gaWGuide_wetR = aOutR
endin

;==============================================================
; instr 92 — CHOP, tick-synced trance gate (toggle 24, GLITCH s3)
; REPLACES SqMod. Reads gaSqMod_sendL/R, writes gaSqMod_wetL/R,
; depth knob = gkSqMod_drywet (existing CC21 dispatch keeps working).
;
; A fired tick gets CHOPPED: gated on/off kDiv times within the tick
; (kDiv per-event from {2,4}, pedal pushes toward 8 = buzz chop), or
; occasionally fully MUTED (dropout hole, ~18% of fired events).
; Un-fired ticks pass clean. 2 ms slewed edges — hard but click-free.
; P1 CC21 depth · P2 CC20 gkCHP_prob · P3 CC23 gkCHP_rate ·
; P4 CC25 gkCHP_shape (duty) · CC29 gkCHP_expr instant trigger.
;==============================================================
instr 92
  iSlew = 1 / (0.002 * sr)       ; gate edge slew: full swing in 2 ms

  aInL = gaSqMod_sendL
  aInR = gaSqMod_sendR

  kFire, kInt GLT_FireK gkGLT_tick, gkCHP_prob, gkCHP_expr, gkSqMod_held

  kAct    init 0                 ; 1 = this tick is being chopped
  kIsMute init 0
  kCell   init 1                 ; samples per gate cell
  kDuty   init 0.5
  kPh     init 0                 ; sample phase within the tick
  kGateSm init 1
  kSubSmp = gkGLT_tickSec * sr
  kSubSmp = (kSubSmp < 480 ? 480 : kSubSmp)

  if (gkGLT_tick == 1) then
    kAct = 0                     ; each tick decides fresh
    kPh  = 0
    if (kFire == 1) then
      kAct = 1
      kU  random 0, 1
      kU2 random 0, 1
      kIsMute = (kU < 0.18 ? 1 : 0)
      ; rate bias: knob 0 -> mostly /2, knob 1 -> mostly /4; pedal -> /8
      kDiv = (kU2 < (1 - gkCHP_rate) ? 2 : 4)
      kDiv = (kInt > 0.7 ? 8 : kDiv)
      kCell = int(kSubSmp / kDiv)
      kDuty = 0.30 + 0.45 * limit:k(gkCHP_shape, 0, 1)
      gkCHP_evN = gkCHP_evN + 1
      gkCHP_evT = gkGLT_tickN
#ifdef GLTDBG
      printks "CHPFIRE t=%.5f n=%.0f mute=%.0f div=%.0f duty=%.2f int=%.2f\n", 0, timeinsts:k(), gkCHP_evN, kIsMute, kSubSmp/kCell, kDuty, kInt
#endif
    endif
  endif

  aGate init 0
  kS = 0
  while kS < ksmps do
    kTgt = 1
    if (kAct == 1) then
      if (kIsMute == 1) then
        kTgt = 0
      else
        kCPh = kPh - kCell * int(kPh / kCell)
        kTgt = (kCPh < kDuty * kCell ? 1 : 0)
      endif
    endif
    ; slew toward target (linear, 2 ms full swing)
    kD = kTgt - kGateSm
    kD = (kD > iSlew ? iSlew : (kD < -iSlew ? -iSlew : kD))
    kGateSm = kGateSm + kD
    aGate[kS] = kGateSm
    kPh = kPh + 1
    kS = kS + 1
  od

  ; depth: 0 = untouched, 1 = full-depth cuts
  kDep  portk limit:k(gkSqMod_drywet, 0, 1), 0.01
  aDep  interp kDep
  aG    = 1 - aDep + aDep * aGate
  aOutL = aInL * aG
  aOutR = aInR * aG
  gaSqMod_wetL = aOutL
  gaSqMod_wetR = aOutR
endin

;==============================================================
; instr 93 — RVRS, reverse stutter (toggle 25, GLITCH s4)
; REPLACES FreqShift. Reads gaFShift_sendL/R, writes gaFShift_wetL/R,
; mix knob = gkFShift_drywet (existing CC22 dispatch keeps working).
;
; On fire: the audio that just played (1 or 2 ticks, tick-aligned)
; plays BACKWARDS in its place, for exactly its own length — the
; grid never hiccups. Speed usually -1x; gkRVS_len's upper half (and
; pedal intensity) admit -2x (octave-up flick) and -0.5x (tape-slur,
; covers half the span). At toe every tick is a fresh flip =
; continuous mirrored audio.
; P1 CC22 mix · P2 CC20 gkRVS_prob · P3 CC23 gkRVS_len
; CC29 gkRVS_expr instant trigger.
;==============================================================
instr 93
  iLen = giGLT_bufLen
  iXf  = 0.004 * sr

  aInL = gaFShift_sendL
  aInR = gaFShift_sendR

  kFire, kInt GLT_FireK gkGLT_tick, gkRVS_prob, gkRVS_expr, gkFShift_held

  kWr   init 0
  kRem  init 0
  kELen init 1
  kRd   init 0
  kSpd  init 1
  kSubSmp = gkGLT_tickSec * sr
  kSubSmp = (kSubSmp < 480 ? 480 : kSubSmp)

  kBusy = (kRem > 0 ? 1 : 0)
  if (kFire == 1 && (kBusy == 0 || kInt >= 0.999)) then
    kU  random 0, 1
    kU2 random 0, 1
    kW  = limit:k(gkRVS_len + kInt * 0.35, 0, 1)
    ; event length: 1 tick, or 2 ticks when the knob's upper half says so
    kTk = (kU < kW * 0.6 ? 2 : 1)
    kELen = int(kSubSmp * kTk)
    ; speed: -1x normally; knob/pedal admit -2x and -0.5x oddballs
    kSpd = 1
    if (kU2 < kW * 0.5) then
      kSpd = (kU2 < kW * 0.25 ? 2 : 0.5)
    endif
    kRd  = kWr                     ; read backwards from "now"
    kRem = kELen
    gkRVS_evN = gkRVS_evN + 1
    gkRVS_evT = gkGLT_tickN
#ifdef GLTDBG
    printks "RVSFIRE t=%.5f n=%.0f len=%.0f spd=%.2f int=%.2f\n", 0, timeinsts:k(), gkRVS_evN, kELen, kSpd, kInt
#endif
  endif

  aRvL init 0
  aRvR init 0
  aEnv init 0
  kS = 0
  while kS < ksmps do
    kInL vaget kS, aInL
    kInR vaget kS, aInR
    tablew kInL, kWr, giRVS_bufL
    tablew kInR, kWr, giRVS_bufR
    kWr = kWr + 1
    kWr = (kWr >= iLen ? 0 : kWr)

    kEnvV = 0
    kL_   = 0
    kR_   = 0
    if (kRem > 0) then
      kIn   = kELen - kRem
      kEnvV = kIn / iXf
      kE2   = kRem / iXf
      kEnvV = limit:k((kE2 < kEnvV ? kE2 : kEnvV), 0, 1)

      kRd = kRd - kSpd                       ; BACKWARDS
      kRd = (kRd < 0 ? kRd + iLen : kRd)
      kI0 = int(kRd)
      kFr = kRd - kI0
      kI1 = kI0 + 1
      kI1 = (kI1 >= iLen ? 0 : kI1)
      kB0 tab kI0, giRVS_bufL
      kB1 tab kI1, giRVS_bufL
      kL_ = kB0 + (kB1 - kB0) * kFr
      kB0 tab kI0, giRVS_bufR
      kB1 tab kI1, giRVS_bufR
      kR_ = kB0 + (kB1 - kB0) * kFr

      kRem = kRem - 1
    endif

    aRvL[kS] = kL_
    aRvR[kS] = kR_
    aEnv[kS] = kEnvV
    kS = kS + 1
  od

  aRepL = aInL + (aRvL - aInL) * aEnv
  aRepR = aInR + (aRvR - aInR) * aEnv
  kMix  portk limit:k(gkFShift_drywet, 0, 1), 0.01
  aMix  interp kMix
  aOutL = aInL + (aRepL - aInL) * aMix
  aOutR = aInR + (aRepR - aInR) * aMix
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaFShift_wetL = aOutL
  gaFShift_wetR = aOutR
endin

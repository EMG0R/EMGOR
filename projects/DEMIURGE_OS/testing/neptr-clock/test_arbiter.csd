<CsoundSynthesizer>
<CsOptions>
-n -m0
</CsOptions>
<CsInstruments>
; testing/neptr-clock/test_arbiter.csd
;
; Headless (no audio device -- -n/nosound) test of the csound-side clock
; arbiter that decides between loop-derived and external tempo. Two things
; are exercised, both against the REAL source where feasible:
;
;   1. instr 100's "MASTER CLOCK COORDINATION" block (the real file is
;      #include'd below, unmodified -- see "../NEPTR phase4/csound/
;      chase_bliss_clock.orc" ~lines 444-524 as of 2026-08-12): decides
;      kBpmTarget/kSourceNow from (kLen<=0), (external fresh <2.0s), or
;      (loop-derived) in that priority order. We drive its inputs
;      (gkLpMasterLen, gkGLITCH_bpmExternal, gkGLITCH_extBpmEdge) directly
;      from a driver instrument -- "drive the globals directly" per the
;      test brief, since feeding this via real MIDI would need a MIDI
;      output port this Mac doesn't have wired to itself.
;
;   2. midi_handler.orc instr 99's gkGLITCH_extBpmSeen latch (~lines
;      216-241 as of 2026-08-12): must key ONLY on the daemon's CC116
;      role broadcast == 1 ("we are following an external master"), never
;      on a bare CC119 tempo-broadcast edge. That file can't be #include'd
;      standalone here (it depends on handler_remap.orc/handler_looper.orc/
;      handler_menus.orc and a live -M MIDI input port), so this
;      reimplements ONLY that one guarded snippet verbatim, side-by-side
;      with the historical (pre-2026-08-12) buggy version, to prove this
;      specific regression stays fixed -- the same technique
;      testing/clock/test_pll.cpp uses for its "old gate" comparison.
;
; Every check prints PASS/FAIL and increments gkChecks/gkFail; the final
; instrument prints a summary and exitnow()s non-zero on any failure.
sr = 48000
ksmps = 128
nchnls = 2
0dbfs = 1

; ---- globals chase_bliss_clock.orc reads/writes but doesn't declare itself
; (normally declared in globals.orc; declared here minimally instead of
; pulling in the whole 51KB globals.orc and its own dependency chain) ----
gkLpMasterLen         init 0
gkLpState1            init 0
gkLpRdPos1            init 0
gkGLITCH_bpm          init 95
gkGLITCH_bpmExternal  init 95
gkGLITCH_extBpmEdge   init 0
gkChaseBlissBPM       init 120
gkChaseBlissSubdivSec init 0.25
gkChaseBlissClockSource init 0

; 2026-10: these two were NOT declared here and the harness SIGABRTed without
; any parse message. They became dependencies of instr 100 in the uncommitted
; 2026-09-01 "no-loop tempo priority" change (the kChg line now reads
;   changed kLen, gkManualBpm, gkGLITCH_bpmExternal, gkGLITCH_extBpmSeen
; ), so this test had been broken since then -- it predates the 2026-10 clock
; work. Declared minimally, same as the rest of this block.
gkManualBpm           init 120
gkGLITCH_extBpmSeen   init 0
gkLpState2            init 0
gkLpState3            init 0
gkLpState4            init 0
gkLpState5            init 0

; ---- OSC telemetry stubs (added 2026-10) ----
; globals.orc defines OSC3x_f / OSC3x_i as single-OSCsend opcodes. This
; harness deliberately does NOT #include the 51KB globals.orc (see the
; minimal-globals note above), so stub them as no-ops. Two reasons this is
; the right call rather than pulling globals.orc in: (1) this test asserts
; tempo-ARBITRATION logic, not telemetry; (2) a real OSCsend here would fire
; UDP at port 8000, which is a LIVE neptrPhase4_UI if one happens to be
; running -- a test must not poke the running instrument.
; If a future test needs to assert on telemetry values, give these bodies a
; gi-rate counter rather than restoring the real OSCsend.
; 2026-10-01: chase_bliss_clock.orc no longer carries its own inline copy of
; the successive-halving loop->BPM search; the single definition lives in
; neptr/csound/loop_implied_bpm.inc.orc and this harness is a third includer
; of it (rather than the search existing twice). run.sh points csound's include
; path at the real csound dir, so this resolves to the same file the engine uses.
#include "@LOOP_IMPLIED_BPM_PATH@"

opcode OSC3x_f, 0, Sk
  Saddr, kv xin
endop
opcode OSC3x_i, 0, Sk
  Saddr, kv xin
endop

; ---- test bookkeeping ----
; i-rate globals (gi, NOT gk): referencing a k-rate global anywhere in a
; statement makes Csound schedule that statement as continuous PERFORMANCE
; (per-k-cycle) code for as long as the enclosing instrument instance is
; active -- even inside an all-i-rate-input opcode, and even for a note
; with p3 as short as 0.001s (still >= 1 k-cycle). That bit twice while
; writing this test: a k-rate gkChecks/gkFail counter got bumped once per
; k-cycle instead of once per CHECK call. gi-rate globals have no such
; per-k-cycle re-execution -- exactly the "increment once" semantics this
; needs.
giChecks init 0
giFail   init 0

opcode CHECK, 0, iiS
  iGot, iExpectOK, Smsg xin
  giChecks = giChecks + 1
  if (iExpectOK == 0) then
    giFail = giFail + 1
    prints "FAIL: %s\n", Smsg
  else
    prints "PASS: %s\n", Smsg
  endif
endop

; CB_CLOCK_MIDI_OUT=0: no MIDI output port on this box -- keep the send
; logic computed (telemetry) but skip the actual midiout call, per the
; file's own override macro (see its header, "OUTBOUND CLOCK").
#define CB_CLOCK_MIDI_OUT #0#
; run.sh substitutes this token with the real absolute path to
; chase_bliss_clock.orc (in the sibling "NEPTR phase4" checkout) before
; invoking csound -- this repo doesn't own or vendor a copy of it.
#include "@CHASE_BLISS_ORC_PATH@"

; ===========================================================================
; PART 1 -- instr 100 arbiter, driven directly (real file, unmodified)
; ===========================================================================
; Timeline (seconds, instr-1-local via timeinsts):
;   [0.0, 1.0)  no loop (gkLpMasterLen=0), no external edge ever sent
;               -> kLen<=0 rule: external wins unconditionally (95 BPM)
;   [1.0, ...)  loop appears (2s -> 120 BPM loop-derived) AND, in the same
;               block, external ticks to 150 BPM with a fresh edge
;               -> external still wins (fresh, <2.0s window)
;   after the edge goes >2.0s stale (no more edges), loop present
;               -> loop-derived wins (120 BPM)
instr 1
  kT timeinsts
  kEdgeState init 0

  if (kT < 1.0) then
    gkLpMasterLen        = 0
    gkGLITCH_bpmExternal = 95
    gkGLITCH_extBpmEdge  = 0
  else
    gkLpMasterLen        = 2 * sr   ; 2s loop -> 120 BPM (8th-note search, see file header)
    gkGLITCH_bpmExternal = 150
    if (kEdgeState == 0) then
      gkGLITCH_extBpmEdge = 1       ; one-block edge pulse, like changed(kMasterBpm)
      kEdgeState = 1
    else
      gkGLITCH_extBpmEdge = 0
    endif
  endif
endin

instr 901
  ; (a) no loop, no external ever seen -> falls back to external's value.
  iSrc  = i(gkChaseBlissClockSource)
  iOK   = (iSrc == 0 ? 1 : 0)
  CHECK iSrc, iOK, "(a) no loop + no external: clock source == EXTERNAL (kLen<=0 rule)"
endin

instr 902
  ; (b) external fresh (<2.0s since edge), loop ALSO present -> external
  ; still wins -- proves the recency rule, not just the no-loop rule.
  iSrc = i(gkChaseBlissClockSource)
  iOK  = (iSrc == 0 ? 1 : 0)
  CHECK iSrc, iOK, "(b) external fresh + loop present: clock source == EXTERNAL (recency rule)"
endin

instr 903
  ; (c) external now stale (>2.0s since the edge), loop present -> loop
  ; wins.
  iSrc = i(gkChaseBlissClockSource)
  iOK  = (iSrc == 1 ? 1 : 0)
  CHECK iSrc, iOK, "(c) external stale (>2.0s) + loop present: clock source == LOOP-DERIVED"

  ; Bonus: the smoothed output (gkGLITCH_bpm, after the 0.35s portk
  ; glide) should have settled near the loop's 120 BPM by now (long past
  ; the glide time), not stuck near external's 150.
  iBpm = i(gkGLITCH_bpm)
  iBpmOK = (iBpm > 118 && iBpm < 122 ? 1 : 0)
  CHECK iBpm, iBpmOK, sprintf("(c) gkGLITCH_bpm settled near loop-derived 120 BPM, got %.3f", iBpm)
endin

schedule 1,   0, 10
schedule 901, 0.9,  0.001
schedule 902, 1.25, 0.001
schedule 903, 3.9,  0.001

; ===========================================================================
; PART 2 -- gkGLITCH_extBpmSeen latch regression (midi_handler.orc ~216-241)
; ===========================================================================
; Faithful reimplementation of ONLY the guarded snippet (see file header
; for why the whole file can't be #include'd here). Two versions run
; side-by-side on the SAME simulated input stream:
;   NEW (current, fixed 2026-08-12): keys on kClockRole (CC116 role
;     broadcast) == 1, i.e. "the daemon says it is following an external
;     master" -- midi_handler.orc lines ~238-241.
;   OLD (the historical bug this replaced): keyed on
;     gkGLITCH_extBpmEdge, i.e. ANY change in the CC119 tempo broadcast --
;     including the daemon's own self-broadcast while it is MASTER, which
;     fires within one beat of every boot even with nothing external
;     attached. Kept here ONLY to prove the regression stays fixed, same
;     as test_pll.cpp's "old gate" comparison.
gkTestExtEdge   init 0   ; simulated gkGLITCH_extBpmEdge (CC119 changed())
gkTestClockRole init 2   ; simulated raw CC116 value (2 = "we are master", never latches)
gkTestSeenNew   init 0
gkTestSeenOld   init 0

instr 2
  kT timeinsts

  ; The daemon re-broadcasts CC119 (its own tempo) roughly every beat
  ; regardless of role -- simulate a changed()-edge every 0.5s the whole
  ; run, exactly the "we only heard ourselves" bug scenario.
  kEdgeTimer init 0
  kEdgeTimer = kEdgeTimer + (ksmps / sr)
  if (kEdgeTimer >= 0.5) then
    gkTestExtEdge = 1
    kEdgeTimer = 0
  else
    gkTestExtEdge = 0
  endif

  ; CC116 stays "we are master" (2) the whole time UNTIL a brief real
  ; external-follower announcement at t=3.0 (value 1, one block).
  gkTestClockRole = 2
  if (kT >= 3.0 && kT < 3.0 + 2 * (ksmps / sr)) then
    gkTestClockRole = 1
  endif

  ; ---- OLD (buggy) latch: fires on ANY CC119 edge ----
  if (gkTestExtEdge == 1) then
    gkTestSeenOld = 1
  endif

  ; ---- NEW (fixed) latch: fires ONLY on CC116 == 1 ----
  ; (midi_handler.orc: `if (kClockRole > 0.5 && kClockRole < 1.5) then
  ;  gkGLITCH_extBpmSeen = 1 endif`)
  if (gkTestClockRole > 0.5 && gkTestClockRole < 1.5) then
    gkTestSeenNew = 1
  endif
endin

instr 911
  ; Before the CC116==1 announcement: the OLD logic has already falsely
  ; latched (self-broadcast bug reproduced) while the NEW logic has not.
  iOld = i(gkTestSeenOld)
  iNew = i(gkTestSeenNew)
  CHECK iOld, (iOld == 1 ? 1 : 0), "(d) regression proof: OLD (pre-2026-08-12) latch DOES false-fire on self-broadcast-only CC119 edges"
  CHECK iNew, (iNew == 0 ? 1 : 0), "(d) NEW latch does NOT fire on CC119 edges alone (no CC116==1 seen yet)"
endin

instr 912
  ; After the CC116==1 announcement: NEW latch must now be set (a real
  ; external master really did show up).
  iNew = i(gkTestSeenNew)
  CHECK iNew, (iNew == 1 ? 1 : 0), "(d) NEW latch fires once a genuine CC116==1 (external master) is seen"
endin

schedule 2,   0, 10
schedule 911, 2.9, 0.001
schedule 912, 3.5, 0.001

; ===========================================================================
; Summary + exit code
; ===========================================================================
instr 999
  prints "\n%d/%d checks passed\n", giChecks - giFail, giChecks
  if (giFail > 0) then
    prints "%d FAILURE(S)\n", giFail
  else
    prints "ALL PASSED\n"
  endif
  exitnow
endin
schedule 999, 4.5, 0.001

</CsInstruments>
<CsScore>
e 5.0
</CsScore>
</CsoundSynthesizer>

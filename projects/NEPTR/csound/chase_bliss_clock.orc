;==============================================================
; CHASE BLISS SHARED MASTER CLOCK / SUBDIVISION — instr 100 (always-on)
;                + TEST METRONOME — instr 102 (always-on, gated by gkMetroOn)
;==============================================================
; Built 2026-07-14, reworked 2026-07-15 (8TH-NOTE derivation + metronome +
; debug telemetry + outbound clock ENABLED). Derives ONE shared
; tempo/subdivision from the LOOPER's master channel (menu 10, ch1,
; looper.orc/handler_looper.orc) and uses it to (a) quantize BLOOPER/ONWARD/
; MOOD II's generative timing and (b) become the rig's master tempo clock
; (gkGLITCH_bpm), taking over from the external DEMIURGE MIDI clock (CC119)
; whenever a loop is recorded, and (c) — NEW, now live — push that tempo back
; OUT to DEMIURGE as ch16 CC118 whenever the loop-derived tempo is the
; authoritative one.
;
; INTEGRATION NOTES (for the shared-files integration pass — nothing below
; blocks this file, these are follow-ups in files owned elsewhere):
;   1. globals.orc ~line 947: `gkChaseBlissSubdivSec init 0.125` — that
;      pre-instr-100 init (and its "1/16 @ 120bpm" comment) is STALE: the
;      algorithm now yields 0.25 s (one 8TH note @ 120 BPM) as the no-loop
;      default. Harmless at runtime (instr 100 overwrites it on its very
;      first k-block), but the init value + comment should be updated to
;      0.25 for consistency.
;   2. METRONOME TOGGLE: gkMetroOn (declared below, init 1 — ON by default
;      for this verification phase, deliberately) has no UI/footswitch
;      binding yet. /metro_state (i, 0/1) is already sent change-triggered to
;      127.0.0.1:8000 so the UI can show it; a later pass can wire a toggle
;      by writing gkMetroOn from midi_handler.orc / handler_menus.orc.
;   3. DEBUG TELEMETRY: /cb_bpm and /cb_subdiv (f) are now emitted to
;      127.0.0.1:8000 on every recompute (change-triggered) — the UI may
;      optionally display them. They exist so the rig can PROVE end-to-end
;      that gkLpMasterLen actually reaches this file on the real hardware.
;   4. mood2_bench.csd / onward_bench.csd (owned elsewhere) still default
;      their INJECTED tick to 0.125 s. The quantization property they prove
;      is tick-agnostic, but their SUBDIV macro defaults could be bumped to
;      0.25 to match the new no-loop default.
;
; WHY A DEDICATED FILE (not inline in globals.orc):
;   globals.orc is declarations only (init values, macros) — no instruments.
;   This needs a real always-on instrument (to poll gkLpMasterLen and run the
;   division search every k-block it changes), so it gets its own small file,
;   matching the pattern already used by looper.orc's instr 209 "transport"
;   and blooper.orc's instr 208 (both tiny always-on housekeeping instruments
;   with a bare `schedule ...,-1` at file scope).
;
;   The two OUTPUT globals (gkChaseBlissBPM, gkChaseBlissSubdivSec) are
;   declared in globals.orc, not here — globals.orc is #include'd first, so
;   every consumer (including ones that might theoretically be parsed before
;   this file) sees a valid default even before this instrument's first
;   k-block runs. (See INTEGRATION NOTES 1: that init is currently one
;   algorithm-revision stale; instr 100 corrects it on block 1.)
;
; INCLUDE ORDER / EXECUTION ORDER (these are TWO DIFFERENT things in Csound
; and it matters here — see the extensive comments already in blooper.orc/
; mood2.orc making the same point):
;   - TEXTUAL #include order (neptrPhase4.csd) only controls when UDOs/macros
;     become available to later files. This file is #include'd right after
;     effects/looper.orc and before onset_pick.orc/onward.orc/blooper.orc/
;     mood2.orc, purely for readability.
;   - RUNTIME k-block execution order is governed by INSTRUMENT NUMBER
;     (ascending), per this codebase's established convention (see blooper.
;     orc "WHY instr 208" and mood2.orc "WHY INSTR 189"). instr 100 is BELOW
;     onward (155), mood2 (189) and blooper (208), so all three read
;     gkChaseBlissSubdivSec/gkChaseBlissBPM freshly-updated THIS cycle. It is
;     also BELOW the looper's master-length-setting instr (210), so it reads
;     gkLpMasterLen exactly as it stood at the END of last cycle — a ONE
;     k-block (ksmps=128 @ 48k = 2.7 ms) staleness on the loop LENGTH input
;     only, and only in the rare block where a loop just finished recording.
;     Inaudible/immaterial for a tempo-defining length.
;   - The TEST METRONOME is instr 102: above 100 (so it reads THIS block's
;     freshly-derived tick) and below the looper's 209-214 (so it reads
;     gkLpRdPos1 exactly as the looper is about to use it this cycle — the
;     identical pointer-base convention blooper.orc instr 208 relies on).
;     Its click voice is instr 103 (event-scheduled, short one-shots).
;
; ===========================================================================
; ALGORITHM (v4, 2026-07-15) — loop length -> 8TH-NOTE tick, by successive
; halving. THE TICK *IS* AN 8TH NOTE. This replaces the earlier bar-count /
; 16th-vs-8th design entirely.
;
;   Take the master loop's whole length, loopSec = gkLpMasterLen / sr, and
;   divide it into N equal parts where N walks the powers of two:
;   N in {2, 4, 8, 16, 32, ... 4096}. Each candidate part duration
;   partSec = loopSec / N is interpreted as ONE 8TH NOTE. Since an 8th note
;   at tempo b lasts (60/b)/2 = 30/b seconds, the implied tempo of a
;   candidate is:
;
;         BPM = 30 / partSec = 30 * N / loopSec
;
;   Selection (checked for every power-of-two candidate):
;     - Prefer any candidate whose BPM lands inside the [80, 160] band over
;       any candidate that doesn't.
;     - Among candidates with the same in/out-of-band status, prefer the one
;       whose BPM is closest to 120. Exact ties (e.g. loopSec=6.0: N=16 ->
;       80 BPM and N=32 -> 160 BPM are both in-band at distance 40) go to
;       the FIRST-SEEN, i.e. the SMALLER N / slower BPM / coarser tick —
;       strict-less-than comparison, documented so the bench can pin it.
;     - If NO candidate lands in-band (possible only for very short loops:
;       the band [80,160] spans exactly a factor of 2 in N, so a power of
;       two always lands in it whenever 2 <= 80*loopSec/30 * 2 <= 4096, i.e.
;       any loop from ~0.375 s up to ~25 min), the same closest-to-120 rule
;       simply applies across all candidates — one rule, not two branches.
;
;   The shared tick is then EXACTLY that winning part:
;         gkChaseBlissSubdivSec = loopSec / N   ( == 30 / BPM, one 8th note )
;   so the grid divides the recorded loop EXACTLY — N ticks tile the loop
;   with zero remainder, by construction. In-band the tick ranges
;   30/160 = 0.1875 s .. 30/80 = 0.375 s.
;
;   Worked examples (all verified by scripts/bench/analyze_chase_bliss_clock.py):
;     loopSec=2.0  -> N=8    BPM=120  tick=0.250 s  (2 s = one 4/4 bar @120)
;     loopSec=8.0  -> N=32   BPM=120  tick=0.250 s
;     loopSec=6.0  -> N=16   BPM=80   tick=0.375 s  (tie vs N=32/160, first wins)
;     loopSec=1.0  -> N=4    BPM=120  tick=0.250 s
;     loopSec=0.5  -> N=2    BPM=120  tick=0.250 s
;     loopSec=0.25 -> no candidate in-band; closest overall N=2 BPM=240,
;                     tick=0.125 s (fallback path)
;
;   NO-LOOP DEFAULT (gkLpMasterLen == 0, i.e. nothing recorded yet, or the
;   master channel was just cleared): 120 BPM, one 8TH note = 30/120 =
;   0.25 s. 120 is the exact center of the target band and matches the
;   standing gkGLITCH_bpm init default used elsewhere in this codebase.
;
;   CONSUMERS: blooper.orc (grain-event launch grid), onward.orc, mood2.orc
;   all read gkChaseBlissSubdivSec directly, with NO further division or
;   multiplication — so the audible grid everywhere IS this 8th note. (This
;   is the user's "go one subdivision down all around": the old design's
;   default tick was a 16th; the tick itself is now the 8th, so every
;   consumer coarsened by exactly one level without touching consumer code.)
;
; ===========================================================================
; TEST METRONOME — instr 102 (+ click voice instr 103)   [v4, 2026-07-15]
;
;   PURPOSE: a VERIFICATION tool, on by default for this test phase (the
;   user explicitly wants to hear it): whenever the MASTER loop (ch1) is
;   playing back, a synthesized click marks every 8th-note tick of the grid,
;   with an ACCENTED click on each group of 8 (the "bar" downbeat, group
;   anchored at the loop start). If the clicks sit on the loop's own pulse,
;   the derivation is right; if they drift or sit at a wrong tempo, the bug
;   is instantly audible and localized.
;
;   SAMPLE-ANCHORED TO THE LOOP, NOT TO WALL-CLOCK — this is the whole
;   point. instr 102 does NOT run its own free oscillating phase. Every
;   k-block it walks the master loop's read position (gkLpRdPos1, the same
;   pointer-base blooper.orc uses, advanced per-sample at the speed measured
;   from the position's own block-to-block delta) and fires a click at the
;   exact sample where the position crosses a tick boundary (loop-buffer
;   positions 0, T, 2T, ... with T = gkChaseBlissSubdivSec * sr). Because
;   the detector rides the loop-buffer position itself, it automatically
;   follows varispeed, reverse, stop/clear — and directly PROVES whether the
;   grid matches the loop, which a wall-clock metronome cannot.
;
;   CLICK: instr 103, a 30 ms enveloped sine tick scheduled sample-offset
;   into the block (`event` with fractional start time). Regular tick:
;   1.2 kHz at 0.10 peak. Accent (tick index divisible by 8, so the loop
;   start itself is always accented): 2 kHz at 0.15 peak. Medium volume,
;   added DIRECTLY to the outputs via outs — deliberately outside
;   signal_chain.orc (test tool; a straight outs add keeps every owned-
;   elsewhere file untouched).
;
;   GATE: gkMetroOn (init 1 = ON for the test phase). Change-triggered OSC
;   /metro_state (i) so the UI can show it. See INTEGRATION NOTES 2 for the
;   future toggle wiring.
;
; ===========================================================================
; MASTER CLOCK INTEGRATION — gkGLITCH_bpm  (v2 — real DEMIURGE coordination)
;
;   gkGLITCH_bpm drives the GLITCH menu's BBCUT/WGuide/SqMod/FreqShift family
;   (stutter.orc) and historically was fed solely by DEMIURGE's external MIDI
;   clock (ch16 CC119, see midi_handler.orc instr 99 — DEMIURGE's
;   demiurge_clock daemon broadcasts BPM once per beat, continuously, whether
;   or not the tempo actually changed; see demiurge/docs/clock.md in
;   DEMIURGE_OS). v1 of this file made gkChaseBlissBPM unconditionally
;   REPLACE gkGLITCH_bpm every k-block whenever a loop existed — simple, but
;   it meant an explicit external retune (someone writes ch16 CC118, the
;   daemon commits it and the next CC119 broadcast announces it — see
;   "How to steer the clock" in clock.md) was PERMANENTLY invisible the
;   instant any loop was recorded. The user asked for real integration, not
;   a blind overwrite. v2 (UNCHANGED by the v4 derivation rework — only the
;   VALUE of gkChaseBlissBPM feeding it changed, now the 8th-note-derived
;   tempo):
;
;   1. THE EXTERNAL VALUE IS NEVER LOST. midi_handler.orc instr 99
;      unconditionally mirrors every CC119 broadcast into gkGLITCH_bpmExternal
;      (globals.orc) regardless of loop state, and flags gkGLITCH_extBpmEdge=1
;      for exactly the k-block a NEW value (an actual tempo change, not a
;      repeat of the same beat's broadcast) arrives. Instr 99 (midi_handler)
;      runs before instr 100 (this file) every k-block — ascending instrument
;      number is this codebase's established runtime-order convention (see
;      file header above) — so this instrument always sees this SAME block's
;      edge, not a block-stale one.
;
;   2. PRIORITY RULES for gkGLITCH_bpm (checked every k-block):
;        a) No loop recorded (gkLpMasterLen <= 0): gkGLITCH_bpm follows
;           gkGLITCH_bpmExternal exactly, instantly, no smoothing — IDENTICAL
;           to pre-existing/original instr-99-only behaviour. Busking without
;           the looper is completely unaffected by any of this.
;        b) Loop recorded, no external push in the last
;           giChaseBlissExtWindow seconds (2.0 s, see below): gkGLITCH_bpm
;           follows gkChaseBlissBPM (loop-derived) — the v1 behaviour, kept.
;        c) Loop recorded AND an external CC119 edge landed within the last
;           giChaseBlissExtWindow seconds: gkGLITCH_bpm follows
;           gkGLITCH_bpmExternal instead — an explicit external retune wins,
;           even with a loop active.
;      gkChaseBlissClockSource tracks which rule is currently authoritative:
;      0 = external, 1 = loop-derived. Declared in globals.orc; inspectable
;      and could be surfaced via OSC/UI later.
;
;      WHY A 2-SECOND WINDOW (not "immediate edge wins for one block, then
;      reverts") and not "external wins forever once it's pushed once"):
;      gkGLITCH_bpm only feeds stutter.orc's tempo-synced glitch family —
;      Blooper/Onward/Mood2 quantize off gkChaseBlissSubdivSec directly and
;      are untouched by this arbitration entirely. A one-block win (2.7 ms
;      @ ksmps=128/sr=48000) would be inaudible and pointless — the point is
;      that the retune must actually be heard for a moment. A permanent win
;      would mean the very first external CC119 edge after a loop starts
;      recording locks GLITCH to external forever, which is the same
;      "one source silently and permanently wins" bug in the other direction
;      — the user's whole ask was for the loop-derived tempo to be usable
;      too. A 2-second window means: right after someone explicitly retunes
;      DEMIURGE (writes CC118), GLITCH audibly follows for a couple of
;      seconds — long enough to register as "yes, the retune took" — then,
;      if no further retunes arrive (steady tempo => `changed` stops firing,
;      see instr 99 comment), authority quietly falls back to whatever the
;      loop implies. This mirrors how CC119 itself works: it's a continuous
;      broadcast, not a one-shot command, so "recent edge" is the correct
;      proxy for "external system is actively asserting a tempo change" and
;      naturally self-expires exactly when the external side goes quiet.
;
;   3. HANDOFF SMOOTHING (no instantaneous tempo jump into stutter.orc):
;      gkGLITCH_bpm is written through `portk` (portamento/glide). Outside
;      any authority handoff, the portk half-time is 0 — gkGLITCH_bpm tracks
;      its current authoritative source exactly and instantly, so rule (a)
;      above (no-loop external tracking) is byte-for-bit identical to the
;      pre-existing behaviour, and rule (b)/(c) track their own source's live
;      value with zero added lag too (e.g. a loop-length change while already
;      loop-authoritative is NOT glided — only the SOURCE FLIP moment is).
;      The instant gkChaseBlissClockSource changes value (external<->loop),
;      a giChaseBlissGlideTime=0.35 s half-time glide window opens: for the
;      next 0.35 s, gkGLITCH_bpm exponentially approaches the new
;      authoritative target instead of jumping to it. 0.35 s was chosen
;      because it's comfortably shorter than one beat across the whole
;      80-160 BPM loop-tempo band this file targets (0.375-0.75 s/beat), so
;      the glide always resolves well inside a single beat and never causes
;      BBCUT/stutter.orc's own tempo-synced grid to visibly lag the new
;      tempo for more than a fraction of a beat, while still being long
;      enough (audio-rate: a few hundred k-blocks) to fully remove the
;      zipper/click of an instant BPM jump landing mid-phrase.
;
; ===========================================================================
; OUTBOUND CLOCK — NEPTR AS THE CLOCK SOURCE  (v4: NOW ENABLED, 2026-07-15)
;
;   *** LIVE AS OF 2026-07-15: giChaseBlissMidiOut defaults to 1, and the
;   *** matching deploy-config change (`-Q 14:0` added to BOTH exec paths of
;   *** DEMIURGE_OS/src/wrappers/demiurge-run-csound, deployed to the Pi as
;   *** /opt/demiurge/bin/demiurge-run-csound) has been made. The -Q flag
;   *** takes effect at the next demiurge restart. First-run verification on
;   *** the Pi: `aseqdump -p "Midi Through" | grep "controller 118"` while a
;   *** loop is recorded.
;
;   THE GOAL (user): "make it based on the loop, send clock output in the
;   format demiurge expects, to clock demiurge when it is active." Everything
;   above this section is NEPTR *receiving* DEMIURGE's clock. This is the
;   reverse: when a loop has been recorded and the loop-derived tempo is the
;   authoritative one (gkChaseBlissClockSource == 1), NEPTR pushes that
;   tempo back OUT so DEMIURGE re-tunes itself to the loop.
;
;   THE PROTOCOL (verified read-only against DEMIURGE_OS, not assumed):
;     - demiurge/docs/clock.md, "Protocol summary" + "How to steer the clock
;       from any program": ch16 CC119 is the daemon's own BPM BROADCAST and is
;       explicitly READ-ONLY to everyone else ("Nothing else ever writes to
;       CC119"). ch16 CC118 is the BPM STEER — the WRITE side, "any program
;       can write CC118 with a new BPM value; the clock retunes itself and the
;       next beat's CC119 broadcast reflects the new tempo". The two are
;       deliberately split so the daemon never has to filter its own echo.
;       => the correct outbound CC is 118, NOT 119. Sending 119 would be
;       writing to the daemon's own broadcast channel, which it ignores.
;     - src/demiurge-clock.cpp input_thread(): confirms it directly. It only
;       looks at ALSA channel index 15 (== MIDI ch16), and on cc == 118 does
;       `new_bpm = 40.0 + (val / 127.0) * 360.0`. So the wire encoding is
;       0..127 -> 40..400 BPM, and the inverse (what we must send) is
;       val = round((bpm - 40) / 360 * 127).
;     - BONUS, and the reason this is worth doing at all: in that same
;       handler, when the daemon is built with -DDEMIURGE_LINK, a CC118 steer
;       is committed straight into Ableton Link
;       (`state.setTempo(new_bpm, ...); g_link->commitAppSessionState(state)`).
;       So NEPTR pushing CC118 does not merely retune the local daemon — it
;       re-tunes the whole Link session, and every other language on the bus
;       picks the new tempo up on the daemon's next CC119 broadcast. NEPTR
;       genuinely becomes the clock source, exactly as asked.
;     - The daemon already dead-bands its own retune (`if (fabs(new_bpm -
;       g_bpm) > 0.25)`), so a repeated identical value is harmless — but we
;       change-trigger anyway (below) rather than spraying a CC every k-block.
;
;   WHY IT NEEDED A DEPLOY-CONFIG CHANGE (now made): Csound can only emit
;   MIDI if a MIDI OUTPUT device was opened at invocation, with the `-Q`
;   flag — there is no way to open one from inside an .orc/.csd at runtime,
;   and the Pi's deployed invocation uses `-+ignore_csopts=1`, so our own
;   <CsOptions> is discarded. The wrapper that builds the command line
;   (DEMIURGE_OS/src/wrappers/demiurge-run-csound, both the ALSA/direct and
;   JACK exec paths) now passes `-Q 14:0` right after `-M 14:0` — the same
;   shared `Midi Through` ALSA-seq bus (14:0) the daemon subscribes its
;   in_port from (clock.md "Running it"), opened for OUTPUT.
;
;   NOTE: Csound's `midiout` is most commonly used from MIDI-activated
;   instruments; instr 100 is score-activated (`schedule 100, 0, -1`). This
;   is believed fine — `midiout` takes an explicit status/channel rather
;   than inheriting one from an activating MIDI event — but it could NOT be
;   verified end-to-end from the Mac (no MIDI output port to observe), so
;   the aseqdump check above is the first thing to do on the rig. The
;   value/edge logic itself IS verified (bench drives it and asserts the
;   exact CC values and firing edges); only the final `midiout` call is
;   unproven off-rig.
;
;   WHAT IS SENT: CC118, ch16, change-triggered, ONLY while the loop-derived
;   tempo is the authoritative one (gkChaseBlissClockSource == 1). We
;   deliberately do NOT send while external is authoritative — that would be
;   NEPTR echoing DEMIURGE's own tempo straight back at it, i.e. a feedback
;   loop between the two clocks. NEPTR speaks only when it has something to
;   say: a loop exists, and the loop is what's driving the tempo.
; ===========================================================================

; ---- outbound-clock switch + telemetry (self-declared here, not in
; globals.orc, to keep this feature entirely contained in its own file) ----
;
; CB_CLOCK_MIDI_OUT: 1 = LIVE (default as of 2026-07-15 — the matching
; `-Q 14:0` wrapper change has been deployed; see OUTBOUND CLOCK header).
; Overridable from a bench/host with --omacro:CB_CLOCK_MIDI_OUT=0 to force
; it inert (e.g. on a machine where csound is started without -Q).
#ifndef CB_CLOCK_MIDI_OUT
#define CB_CLOCK_MIDI_OUT #1#
#endif
giChaseBlissMidiOut = $CB_CLOCK_MIDI_OUT

; Telemetry — computed UNCONDITIONALLY (whether or not the midiout call is
; enabled), so the send LOGIC is fully benchable on a machine with no MIDI
; output device at all.
gkChaseBlissMidiOutVal  init -1   ; last CC118 value we would send (0..127); -1 = none yet
gkChaseBlissMidiOutFire init 0    ; 1 for exactly the k-block a send fires

; ---- test-metronome globals (self-declared here, same containment rule) ----
; Turned OFF by default 2026-07-16 — verification phase is over (the clock
; bench proved sample-accurate loop-lock, see analyze_chase_bliss_clock.py
; scenarios m1-m4). instr 102/103 are left in place, inert, in case a real
; UI-toggleable metronome is wanted later — just flip this back to 1.
gkMetroOn      init 0     ; 1 = metronome audible while the master loop plays
                          ; (ON by default for the test phase; INTEGRATION
                          ;  NOTES 2 covers the future UI toggle)
gkMetroFire    init 0     ; telemetry: 1 for a k-block in which >=1 click was scheduled
gkMetroPosAt   init -1    ; telemetry: loop-buffer pos (samples) of the last
                          ; tick boundary a click fired on (bench grid proof)
gkMetroAccent  init 0     ; telemetry: 1 if that click was the accented downbeat

; triple-send OSC helpers (same reliability pattern as blooper.orc BLP_Osc3*)
opcode CBC_Osc3i, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
endop

opcode CBC_Osc3f, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
endop

instr 100
  kLen  = gkLpMasterLen
  ; 2026-09-01: recompute also when the UTILITY-page manual BPM or the external
  ; clock changes, not only on a loop-length change — so the utility BPM control
  ; and an incoming MIDI clock take effect immediately while no loop is recorded.
  kChg  changed kLen, gkManualBpm, gkGLITCH_bpmExternal, gkClkExtLive
  kInit init 1

  if (kChg == 1 || kInit == 1) then
    kInit = 0

    if (kLen <= 0) then
      ; ---- NO LOOP: tempo priority is EXTERNAL clock > manual utility BPM ----
      ; (a recorded loop, handled below, always outranks both — user's rule:
      ; "always allow the looper or an external clock to determine the bpm").
      kNoLoopBpm = (gkClkExtLive == 1 ? gkGLITCH_bpmExternal : gkManualBpm)
      kNoLoopBpm limit kNoLoopBpm, 40, 300
      gkChaseBlissBPM       = kNoLoopBpm
      gkChaseBlissSubdivSec = 30 / kNoLoopBpm
    else
      kLoopSec = kLen / sr

      ; ---- SUCCESSIVE-HALVING SEARCH (see ALGORITHM header):
      ; N in {2,4,...,4096} equal parts; each part IS an 8th note, so
      ; BPM = 30 * N / loopSec. Prefer in-band [80,160]; break by distance
      ; to 120 (strict <, so exact ties keep the FIRST/coarser candidate).
      ;
      ; 2026-10-01: the search itself lives in ONE place now —
      ; loop_implied_bpm.inc.orc. It was briefly duplicated verbatim here and
      ; as a UDO in globals.orc, cross-commented "must stay identical", which
      ; is how two copies of an algorithm start to drift. The obstacle was
      ; real: testing/neptr-clock/test_arbiter.csd #includes THIS file
      ; standalone without globals.orc, so this file couldn't depend on
      ; anything globals.orc declared. The fix was a third includer, not a
      ; second copy: the harness includes the .inc.orc too. effects/looper.orc
      ; calls the same UDO synchronously at its master-claim site so
      ; gkCommittedBpm and gkLpRecBpm{N} are stamped in the SAME k-cycle --
      ; that zero block-lag is what makes the first varispeed ratio after a
      ; fresh master-loop close land at exactly 1.000.
      kBestBpm, kBestN  LoopImpliedBpm  kLen

      gkChaseBlissBPM       = kBestBpm
      gkChaseBlissSubdivSec = kLoopSec / kBestN   ; == 30/BPM: the 8th-note tick,
                                                  ; tiling the loop EXACTLY
    endif
  endif

  ; ---- DEBUG TELEMETRY (end-to-end inbound-chain proof; see INTEGRATION
  ; NOTES 3): change-triggered OSC of what this file actually computed, plus
  ; a once-per-change journal line. If gkLpMasterLen never arrives on the
  ; real rig, /cb_bpm will sit at 120 / /cb_subdiv at 0.25 forever after a
  ; loop is recorded — instantly visible in the UI or `journalctl`.
  kDbgBpmChg = changed(gkChaseBlissBPM)
  kDbgSubChg = changed(gkChaseBlissSubdivSec)
  kDbgCnt init 0
  if (kDbgBpmChg == 1 || kDbgSubChg == 1) then
    CBC_Osc3f "/cb_bpm", gkChaseBlissBPM
    CBC_Osc3f "/cb_subdiv", gkChaseBlissSubdivSec
    kDbgCnt = kDbgCnt + 1
  endif
  printf "CHASE BLISS clock: BPM=%.3f subdiv=%.4f s (8th note), masterLen=%.0f samples (%.3f s)\n", kDbgCnt, gkChaseBlissBPM, gkChaseBlissSubdivSec, gkLpMasterLen, gkLpMasterLen / sr

  ; ---- MASTER CLOCK COORDINATION (see header "MASTER CLOCK INTEGRATION v2") ----
  ; i-rate constants (instr 100 is scheduled once with -1 dur — see file
  ; header "WHY A DEDICATED FILE" — so plain i-rate init here happens exactly
  ; once, at instrument start, same as this instrument's other i-rate locals).
  iChaseBlissExtWindow  = 2.0    ; seconds — "external just retuned" recency window
  iChaseBlissGlideTime  = 0.35   ; seconds — portk half-time on an authority flip

  ; recency timer for external CC119 edges: reset to 0 on a fresh edge
  ; (set by midi_handler.orc instr 99 earlier THIS block), else counts up.
  kExtSinceEdge init 999
  if (gkGLITCH_extBpmEdge == 1) then
    kExtSinceEdge = 0
  else
    kExtSinceEdge = kExtSinceEdge + (ksmps / sr)
  endif
  ; 2026-10-26: only a LIVE external clock may claim authority (gkClkExtLive);
  ; a free-running demiurge-clock re-broadcasting its last tempo must not.
  kExtRecent = (kExtSinceEdge < iChaseBlissExtWindow && gkClkExtLive == 1 ? 1 : 0)

  ; priority rules (a)/(b)/(c)
  if (kLen <= 0) then
    ; no loop: live external clock, else the UTILITY manual tempo
    kBpmTarget    = (gkClkExtLive == 1 ? gkGLITCH_bpmExternal : gkChaseBlissBPM)
    kSourceNow    = 0
  elseif (kExtRecent == 1) then
    kBpmTarget    = gkGLITCH_bpmExternal
    kSourceNow    = 0
  else
    kBpmTarget    = gkChaseBlissBPM
    kSourceNow    = 1
  endif

  ; open a glide window exactly on an authority flip; otherwise track
  ; the authoritative source instantly (portk htim=0 == pass-through), so
  ; no-loop / steady-state behaviour is unchanged from before this feature.
  kSourceFlip = changed(kSourceNow)
  kGlideTimer init 0
  if (kSourceFlip == 1) then
    kGlideTimer = iChaseBlissGlideTime
  elseif (kGlideTimer > 0) then
    kGlideTimer = kGlideTimer - (ksmps / sr)
    if (kGlideTimer < 0) then
      kGlideTimer = 0
    endif
  endif
  kPortHtim = (kGlideTimer > 0 ? iChaseBlissGlideTime : 0)

  gkGLITCH_bpm            portk kBpmTarget, kPortHtim
  gkChaseBlissClockSource = kSourceNow

  ; ===========================================================================
  ; COMMITTED TEMPO (2026-10, VARISPEED-WARBLE FIX) — see globals.orc
  ; gkCommittedBpm's header comment for the full "why" (two tempos, two
  ; jobs). This block is the ONLY writer of gkCommittedBpm in the codebase.
  ; It updates on exactly two deliberate-decision paths; ordinary steady-
  ; clock PLL tracking (the gkGLITCH_bpm portk above) never touches it.
  ;
  ;   (1) LOOP CLOSE — immediate, no deadband/dwell: a master-loop-closed
  ;       edge (gkLpMasterLen's 0 -> >0 transition, detected locally here off
  ;       kLen, which this instrument already reads every block) IS the
  ;       deliberate decision — there's nothing to debounce, it's a single
  ;       discrete event. Commits gkChaseBlissBPM (the tempo that loop
  ;       implies), not gkGLITCH_bpm, since at the instant of close
  ;       kSourceNow may still read the PRE-close source for one block.
  ;
  ;   (2) GENUINE EXTERNAL RETUNE — deadband + dwell, and only while external
  ;       is the CURRENTLY authoritative source (kSourceNow == 0): a CC119
  ;       broadcast arriving while a loop holds authority must not silently
  ;       retune a loop that isn't even following it right now.
  ;         DEADBAND 0.5 BPM — see globals.orc for the full derivation: ~7x
  ;         the PLL's steady-state noise floor (1 cent @124 BPM ~= 0.07 BPM),
  ;         comfortably under the smallest tempo move a musician would
  ;         actually dial in on purpose (whole BPMs, not half-BPMs).
  ;         DWELL 0.5 s — ~one beat across this rig's 80-160 BPM loop-tempo
  ;         band (0.375-0.75 s/beat) and longer than the daemon's own
  ;         24-tick trimmed-mean window at any tempo in that band, so by the
  ;         time it elapses the external reading is itself already a
  ;         converged PLL estimate, not one hot sample — long enough to
  ;         reject a single outlier tick, short enough to lock in a real
  ;         retune well inside a bar.
  kCbLenPrev init 0
  if (kLen > 0 && kCbLenPrev <= 0) then
    gkCommittedBpm = gkChaseBlissBPM
  endif
  kCbLenPrev = kLen

  kCbExtDwell init 0
  if (kSourceNow == 0 && gkClkExtLive == 1) then
    if (abs(gkGLITCH_bpmExternal - gkCommittedBpm) > 0.5) then
      kCbExtDwell = kCbExtDwell + (ksmps / sr)
      if (kCbExtDwell >= 0.5) then
        gkCommittedBpm = gkGLITCH_bpmExternal
        kCbExtDwell    = 0
      endif
    else
      kCbExtDwell = 0
    endif
  else
    kCbExtDwell = 0
  endif

  ; Telemetry — change-triggered single-send (see globals.orc's OSC3x_f
  ; comment on why NOT the old triple-send: ~900 permanently-open UDP
  ; sockets from the triple-send pattern once blew the engine's fd limit).
  ; Address follows the existing /clock_bpm, /cb_bpm naming so the
  ; committed-vs-live difference is directly comparable at the bench.
  kCbChg changed gkCommittedBpm
  if (kCbChg == 1) then
    OSC3x_f "/cb_committed_bpm", gkCommittedBpm
  endif

  ; ---- OUTBOUND CLOCK: push the loop-derived tempo to DEMIURGE as ch16 CC118
  ; (see the "OUTBOUND CLOCK" header block for the protocol proof and the
  ; wrapper -Q change that carries it).
  ;
  ; Wire encoding is the exact inverse of what demiurge-clock.cpp's input
  ; thread decodes on cc==118:  bpm = 40 + (val/127)*360   =>
  ; val = round((bpm - 40) / 360 * 127), clamped to the 7-bit CC range.
  ; Note this 40..400 span is the DAEMON's real mapping (read from its source,
  ; not from clock.md, whose prose says 40..240 and is stale).
  kSendVal = round(limit:k((gkChaseBlissBPM - 40) / 360 * 127, 0, 127))

  ; Send ONLY while the loop is the authoritative tempo. While DEMIURGE is
  ; authoritative we stay silent — echoing its own tempo back at it would be
  ; a clock feedback loop, not a sync.
  kLoopAuth = (kSourceNow == 1 ? 1 : 0)

  ; Change-triggered: fire when the value we'd send changes, and once more on
  ; the moment authority flips TO the loop (so DEMIURGE is retuned immediately
  ; on handoff even if the loop's BPM happens to be unchanged since last time).
  kValChg  = changed(kSendVal)
  kAuthChg = changed(kLoopAuth)
  kFire    = (kLoopAuth == 1 && (kValChg == 1 || kAuthChg == 1) ? 1 : 0)

  gkChaseBlissMidiOutFire = kFire
  if (kFire == 1) then
    gkChaseBlissMidiOutVal = kSendVal
    ; The ONLY gated line. Needs a MIDI output port (-Q 14:0, now in the
    ; wrapper) AND CB_CLOCK_MIDI_OUT=1 (now the default). 176 = 0xB0 =
    ; control change; channel 16 (Csound's kchan is 1-indexed, so 16 == the
    ; ALSA channel index 15 the daemon filters on); CC 118 = the BPM steer.
    if (giChaseBlissMidiOut == 1) then
      midiout 176, 16, 118, kSendVal
    endif
  endif

  ; ---- LOOP-STATE ANNOUNCE (2026-10) — CC115, UNGATED by clock authority ----
  ; The DEMIURGE daemon needs "does NEPTR currently hold a loop" as a FACT,
  ; not an inference from CC118 recency — CC118 above is sent ONLY while
  ; gkChaseBlissClockSource == 1 (loop authoritative), so the daemon's
  ; CC118-recency proxy goes dark the moment PMOR claims master even though
  ; a loop may still be sitting there playing. CC115 is free on ch16
  ; (80-95 touch, 102 sync-request, 104 manual BPM, 116 role, 117 transport,
  ; 118 steer, 119 BPM broadcast are the other users) and is sent
  ; UNCONDITIONALLY — independent of kSourceNow/gkChaseBlissClockSource —
  ; which is the entire point: the daemon needs this even while NEPTR does
  ; NOT currently own the clock.
  ;
  ; "Holds audio" = any of the 5 channels is NOT idle (state 0). ALL of
  ; 1 (recording), 2 (playing), 3 (overdubbing), 4 (stopped-with-content),
  ; 5 (sync-closing — still physically recording) and 6 (trim-closing —
  ; still physically recording) count as "holding content"; only 0 is
  ; genuinely empty (see effects/looper.orc's state-machine header comment).
  kLoopHeld    = (gkLpState1 != 0 || gkLpState2 != 0 || gkLpState3 != 0 || gkLpState4 != 0 || gkLpState5 != 0) ? 1 : 0
  kLoopHeldVal = (kLoopHeld == 1 ? 127 : 0)

  ; Change-triggered (a real transition is heard immediately) PLUS a
  ; periodic ~2 s re-announce — matching the daemon's own role/BPM
  ; re-announce cadence — so a late-starting or restarted daemon picks this
  ; up without waiting for a state change.
  iLoopAnnounceIval = 2.0
  kLoopAnnounceTimer init iLoopAnnounceIval   ; fires once on the very first block too
  kLoopAnnounceTimer = kLoopAnnounceTimer - (ksmps / sr)
  kLoopHeldChg = changed(kLoopHeld)
  if (kLoopAnnounceTimer <= 0 || kLoopHeldChg == 1) then
    kLoopAnnounceTimer = iLoopAnnounceIval
    ; NO FEEDBACK HAZARD: kLoopHeldVal is derived purely from THIS NEPTR
    ; instance's own gkLpState1..5 (the local looper). It never reads
    ; gkGLITCH_bpm / gkGLITCH_bpmExternal / gkChaseBlissClockSource or
    ; anything the daemon echoes back to NEPTR, and — unlike the CC118
    ; steer just above — it is deliberately UNGATED by clock authority. So
    ; there is no path by which the daemon receiving this could change what
    ; NEPTR next sends here: this is a one-way fact announcement, not a
    ; two-way negotiation like the CC118/CC119 tempo pair (which is exactly
    ; why THAT one needs the recency-window arbitration above and this one
    ; does not).
    if (giChaseBlissMidiOut == 1) then
      midiout 176, 16, 115, kLoopHeldVal
    endif
  endif

  ; ---- TASK 2 (2026-10): PHASE-ACCURATE BEAT SIGNAL FOR THE UI ----
  ; The UI wants a top-left dot that blinks on the beat, "accurate timing
  ; to the true clock". We deliberately do NOT send a message per beat and
  ; have the UI blink on arrival — repaint is event-driven and capped, so
  ; that reads as sloppy/laggy. Instead: emit the AUTHORITATIVE tempo's
  ; beat PERIOD plus a beat-index/downbeat flag, change-triggered on each
  ; beat edge, and let the UI run its OWN local predictor (period +
  ; the UI's own receipt wall-clock time as the phase anchor) that
  ; re-anchors on every message — accurate even when a message is late or
  ; the engine hiccups, because the UI never free-runs for longer than one
  ; beat period before the next anchor arrives.
  ;
  ; Free-running QUARTER-NOTE phase accumulator (NOT the 8th-note
  ; gkChaseBlissSubdivSec tick above, and NOT gated by gkMetroOn — that
  ; gate is only for the audible instr 102/103 test click; this is pure
  ; timing telemetry and must keep running even with the metronome off,
  ; which is the default). Tracks gkGLITCH_bpm, the fully-arbitrated
  ; AUTHORITATIVE tempo computed just above (loop-derived OR external —
  ; i.e. correct whether NEPTR or PMOR is currently the clock master).
  ;
  ; Deliberately NOT loop-position-locked (unlike instr 102's audio-rate
  ; click, which rides gkLpRdPos1 for sample accuracy): this only has to be
  ; accurate to a UI repaint, and a wall-time phase keeps blinking even with
  ; no loop recorded at all (busking). gkLpRecBpm/varispeed above never
  ; touches gkGLITCH_bpm, so this is unaffected by any channel's varispeed.
  iBeatMinBpm = 1   ; defensive floor: never a near-zero/zero period
  kBeatBpm    = (gkGLITCH_bpm > iBeatMinBpm ? gkGLITCH_bpm : iBeatMinBpm)
  kBeatPeriod = 60 / kBeatBpm

  kBeatPhase init 0
  kBeatPhase = kBeatPhase + (ksmps / sr)
  kBeatIdx   init -1
  kBeatEdge  = 0
  if (kBeatPhase >= kBeatPeriod) then
    ; modulo wrap (not a hard reset to 0) so a period SHRINK (tempo sped
    ; up) doesn't stall the very next edge — any overshoot carries into the
    ; new, shorter period exactly like the looper's own position wrap.
    kBeatPhase = kBeatPhase % kBeatPeriod
    kBeatIdx   = kBeatIdx + 1
    kBeatEdge  = 1
  endif
  ; downbeat = every 4th beat (one bar), anchored at engine start — a free
  ; running "bar" reference since there's no external bar/transport concept
  ; in this rig; good enough for "distinct from the other 3 blinks".
  kBeatDown = (kBeatIdx - 4 * int(kBeatIdx / 4) == 0 ? 1 : 0)

  ; Change-triggered on the beat edge ONLY — NOT on every k-block the
  ; period itself ticks during a gkGLITCH_bpm portk glide (that would be an
  ; un-gated k-rate OSC flood, exactly the bug class this codebase's OSC
  ; rules warn against). The period is carried piggyback on each beat edge
  ; instead, which is already naturally rate-limited to ~once per beat
  ; (0.1875-0.75s across the loop-tempo band) and is exactly the data the
  ; UI's free-running predictor needs to stay locked between edges.
  if (kBeatEdge == 1) then
    OSC3x_i "/beat_idx",    kBeatIdx
    OSC3x_i "/beat_down",   kBeatDown
    OSC3x_f "/beat_period", kBeatPeriod
  endif
endin

;==============================================================
; instr 102 — TEST METRONOME (see "TEST METRONOME" header block)
;==============================================================
; Always-on detector; reads gkLpState1/gkLpRdPos1/gkLpMasterLen READ-ONLY
; plus this block's freshly-derived gkChaseBlissSubdivSec (102 > 100), and
; event-schedules instr 103 clicks sample-offset into the block.
instr 102
  ; change-triggered UI state (fires once at startup too, announcing ON)
  kMetChg changed gkMetroOn
  if (kMetChg == 1) then
    CBC_Osc3i "/metro_state", gkMetroOn
  endif

  kLen = gkLpMasterLen
  ; master loop "playing back" = ch1 in play(2)/overdub(3) with a real length
  ; (state codes per looper.orc; blooper.orc gates on the identical condition)
  kPlay = ((gkLpState1 == 2 || gkLpState1 == 3) && kLen > 1 ? 1 : 0)

  ; tick in loop-buffer samples (defensive clamp mirrors blooper.orc's)
  kT = gkChaseBlissSubdivSec * sr
  if (kT < 1) then
    kT = 1
  endif

  kPosPrev init -1     ; previous block's pointer base; -1 = not primed
  kTkPrv   init -1     ; tick index at the previous SAMPLE; -1 = not primed
  kFired   = 0

  if (kPlay == 0 || gkMetroOn == 0) then
    ; not playing (or muted): unprime, so a mid-tick restart can never fire
    ; one bogus off-grid click (same priming rule as blooper.orc's detector)
    kPosPrev = -1
    kTkPrv   = -1
  else
    kPos = gkLpRdPos1
    ; normalize into [0, kLen) exactly as blooper.orc does with the same base
    if (kPos >= kLen) then
      kPos = kPos - kLen * int(kPos / kLen)
    endif
    if (kPos < 0) then
      kPos = kPos + kLen
    endif

    if (kPosPrev < 0) then
      ; first playing block: prime the speed/tick tracking, no clicks yet
      ; (at 1x the playhead moves ksmps samples/block, so this delays the
      ; first possible click by one block = 2.7 ms, once, at loop start)
      kPosPrev = kPos
      kTkPrv   = int(kPos / kT)
    else
      ; measured per-sample speed from the pointer's own block delta
      ; (wrap-aware) — follows the looper's varispeed/reverse with no
      ; dependency on looper internals beyond the public read-pointer.
      kDelta = kPos - kPosPrev
      if (kDelta > kLen / 2) then
        kDelta = kDelta - kLen
      elseif (kDelta < -kLen / 2) then
        kDelta = kDelta + kLen
      endif
      kSpd = kDelta / ksmps
      kPosPrev = kPos

      ; walk THIS block sample-by-sample; a click fires at the exact sample
      ; the tick index changes (any change is a boundary crossing — works
      ; for reverse too, and the loop wrap is itself the grid point at 0).
      kPh = kPos
      kS  = 0
      while kS < ksmps do
        kIdx = int(kPh / kT)
        if (kIdx != kTkPrv) then
          ; accent every group of 8 ticks, group anchored at loop start
          ; (index 0 — the loop's own downbeat — is always accented)
          kAcc = (kIdx - 8 * int(kIdx / 8) == 0 ? 1 : 0)
          kFrq = (kAcc == 1 ? 2000 : 1200)
          kAmp = (kAcc == 1 ? 0.15 : 0.10)
          ; sample-anchored: schedule the click kS samples into this block
          event "i", 103, kS / sr, 0.030, kFrq, kAmp
          kFired        = 1
          gkMetroPosAt  = kPh
          gkMetroAccent = kAcc
        endif
        kTkPrv = kIdx

        kPh = kPh + kSpd
        if (kPh >= kLen) then
          kPh = kPh - kLen
        endif
        if (kPh < 0) then
          kPh = kPh + kLen
        endif
        kS = kS + 1
      od
    endif
  endif

  gkMetroFire = kFired
endin

; instr 103 — the click voice: a clean 30 ms enveloped sine tick.
; p4 = frequency (2000 accent / 1200 regular), p5 = peak amp (0.15 / 0.10).
; Direct outs by design — test tool, deliberately outside signal_chain.orc.
instr 103
  iFrq = p4
  iAmp = p5
  ; 0.5 ms attack (no DC thump), exponential decay to silence across p3
  aAtt linseg 0, 0.0005, 1, p3 - 0.0005, 1
  aDec expseg 1, p3, 0.004
  aClk poscil aAtt * aDec * iAmp, iFrq
  outs aClk, aClk
endin

; always-on housekeeping instruments, same pattern as looper.orc instr 209
; and blooper.orc instr 208 — never toggled, never bypassed.
schedule 100, 0, -1
schedule 102, 0, -1

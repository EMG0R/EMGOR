; BLOOPER — grain-EVENT modulator for the 5-channel LOOPER
; ---------------------------------------------------------------------------
; Instr 208 (always-on)   Toggle 56   Menu 11 (CHASE BLISS) slot 0, button CC74
;
; WHAT IT IS
;   BLOOPER is NOT a looper and NOT a granular synth. It owns no buffer, records
;   nothing, and has no transport. It MODULATES the loops the user already
;   recorded with the 5-channel LOOPER (menu 10, instrs 209-214, looper.orc).
;
;   THE LOOP PLAYS BACK NORMALLY. That is the baseline and the dominant state.
;   Every so often a GRAIN EVENT is thrown: a chunk of the loop, grabbed from
;   somewhere else in the timeline and played at a different speed (sometimes
;   backwards), momentarily displaces the normal playback. Then it drops right
;   back into the loop. Punctuation, not a wash.
;
;   THIS IS DELIBERATELY *NOT* CLASSICAL GRANULAR SYNTHESIS. The loop is never
;   reconstructed out of a dense stream of grains, and there is NO continuous
;   pitch/vibrato/detune modulation anywhere in this file. The only thing that
;   changes pitch is a grain event's own discrete playback speed — which is a
;   per-event choice, not a modulator riding on top of the sound.
;
;   BLOOPER OFF -> the looper behaves exactly as it always has.
;   BLOOPER ON  -> signal_chain crossfades the looper's direct playback out and
;                  BLOOPER's modulated render in (a REPLACE, never a sum, so the
;                  loops are never doubled). At MOD = 0 no events ever fire and
;                  the render is a sample-exact copy of normal playback, so
;                  turning BLOOPER on with the knob down is inaudible.
;
; DUTY CYCLE — the loop always wins
;   Event density is capped: at the knob's maximum, grain events cover at most
;   ~45% of the timeline (BLP_MAXDUTY), so even flat out, more than half the
;   time you are hearing the loop play normally. At MOD = 0, no events at all.
;   The knob scales the target duty between those two extremes. After an event
;   of length L the next gap is G = L * (1 - duty) / duty, randomised x0.7-1.6
;   so the rhythm never locks into a pattern.
;
; READ-ONLY DEPENDENCY ON looper.orc (that file is NOT modified):
;   giLpBuf{1..5}L/R  loop buffers      gkLpState{N}  2 = play, 3 = overdub
;   gkLpLen{N}        loop length       gkLpRdPos{N}  read-pointer base
;   gkLpSpIdx{N} + giLpSpeeds           varispeed (BLOOPER follows it)
;
; WHY instr 208: instruments run in ascending order and 208 < 209..214, so this
;   sees gkLpRdPos{N} exactly as the looper is ABOUT TO use it this cycle. The
;   normal-playback read therefore starts from the identical pointer base the
;   looper's own playback would have used — which is what makes MOD=0 an exact
;   null against gaLpOut.
;
; ROUTING (csound/signal_chain.orc — no looper.orc edit):
;   kBlpX  port  (1 - gkBLP_bypass), 0.04, 0
;   aPrevL += gaLpOutL * (1 - kBlpX) + gaBLP_LoopL * kBlpX
;
; EACH LOOP MODULATES INDEPENDENTLY
;   The per-channel engine is a UDO, and each of the 5 call sites is its own
;   opcode instance -> its own jspline density drift and its own random draws.
;   No shared seed, no shared LFO phase: 5 loops throw grains on genuinely
;   decorrelated event streams.
;
; GRID-LOCKED EVENT LAUNCH (v2 2026-07-15, replaces the 2026-07-14
;   gap-duration rounding): event lengths, displacements and the underlying
;   gap/duty MATH are still drawn from continuous distributions exactly as
;   before — SIZE, CHARACTER, speed, and the duty-cycle formula are all
;   untouched. What changed, and WHY v1 was inaudible:
;
;   v1 rounded the inter-event GAP DURATION to a whole number of ticks of
;   the shared CHASE BLISS subdivision. But the gap starts counting from the
;   END of the previous event, and event LENGTHS are continuous — so every
;   event's launch PHASE drifted freely relative to the beat grid. Gap
;   durations that are tick-multiples while the phase free-runs are NOT
;   audible as a grid; being ON the grid is what's audible. The user
;   correctly reported hearing no quantization.
;
;   v2 is an ARM/FIRE split: the duty/gap countdown runs exactly as before
;   (that keeps the event RATE driven by the MOD knob), but when the gap
;   expires the event is only ARMED — it actually FIRES on the next
;   subdivision-tick boundary of a tick grid PHASE-ANCHORED TO THE LOOP
;   START (tick boundaries live at loop-buffer positions 0, T, 2T, ... where
;   T = gkChaseBlissSubdivSec * sr; see csound/chase_bliss_clock.orc —
;   derived from the LOOPER's master loop length; as of the 2026-07-15 v4
;   derivation the tick IS one 8TH NOTE of the loop-derived tempo, used here
;   directly with no extra scaling — "one subdivision down" vs the earlier
;   16th-note default — so the grid is musically locked to the recorded
;   loop). Grain events therefore LAND ON the loop's
;   own beat grid — audibly quantized — while everything about the event
;   itself stays generative. Because the grid rides the loop-buffer position,
;   it automatically follows the looper's varispeed and stop/clear, same as
;   the playhead itself. Nothing else in this file references the
;   subdivision or the BPM clock.
;
; GRAIN EVENTS ARE GENERATIVE
;   length   log-uniform around a centre set by the SIZE knob (x1/3 .. x3), so
;            most events are moderate and big ones turn up now and then:
;              size 0 -> ~33-300 ms      size 0.5 -> ~80-730 ms (centre 245 ms)
;              size 1 -> ~200-1800 ms
;   speed    a DISCRETE per-event draw from {1x, 0.5x, 2x, -1x reverse},
;            multiplied by the looper's own varispeed. 1x is the most common.
;            (Yes, this changes pitch — that is the wanted, characterful part.
;            What is banned is a continuous pitch LFO, and there is none.)
;   position continuous random displacement around the playhead; CHARACTER sets
;            how far it can throw.
;
; CONTROLS (menu 11 slot 0; encoder takeover fires on gkLastToggle == 56)
;   gkBLP_bypass  toggle 56 (CC74 button)      1 = off
;   (P1 CC24 output level knob REMOVED 2026-07-14 — BLOOPER always plays at
;   unity output level, the same level normal loop playback would have. The
;   P1/CC24 slot-0 rotation is now a documented no-op; see handler_menus.orc.)
;   gkBLP_Motion  P2 CC20  0..1  how much the density drifts on its own  (0.45)
;   gkBLP_Scat    P3 CC23  0..1  MOD AMOUNT — event density      (default 0.45)
;   gkBLP_Size    P4 CC25  0..1  grain size centre               (default 0.5)
;   gkBLP_Char    P5 CC26  0/1/2 NEAR / THROW / FAR displacement (default 1)
;   gkBLP_expr    CC29 expression pedal, sticky claim gkLastToggle29 = 56.
;
; PEDAL DESIGN (rewritten 2026-08-23 — see MANUAL OVERRIDE below; this
;   REPLACES the old "CUE" behaviour, which blended the pedal additively
;   into the density and fired an immediate event on the way up):
;
;   THE GENERATIVE ENGINE IS PEDAL-INDEPENDENT. gkBLP_Scat (P3 CC23, "MOD
;   AMOUNT") together with gkBLP_Motion's drift is the sole master
;   probability/density knob — call it kModB below. It is computed with NO
;   reference to the pedal at all, so density/pitch keep doing their own
;   generative thing regardless of where the pedal sits.
;
;   MASTER-PROBABILITY EDGE CASE: at gkBLP_Scat == 1 (its absolute max),
;   kModB clamps to exactly 1 (see BLP_Ev), and the duty curve below is
;   built so kMod == 1 forces kDuty == 1 exactly (snapped, not just
;   asymptotic) -> zero inter-event gap -> a grain event is armed and
;   guaranteed to fire on every single CHASE BLISS clock tick, no
;   off-by-one / rounding gap possible. Below max, the same curve tracks
;   the historical BLP_MAXDUTY-scaled behaviour closely (same shape,
;   same feel) — only the literal top of the range changes.
;
;   PEDAL = MANUAL OVERRIDE, CENTRED AT REST (0.5), continuous, on BOTH
;   probability and pitch — matching the "0.5 = home/root" bidirectional-
;   scrub idiom already used elsewhere in this rig for CC29 (see
;   handler/expr_targets.src.orc id 66, MOOD II CLOCK: heel=0 bottom,
;   toe=1 top, 0.5=neutral). gkBLP_expr inits to 0.5 (not 0) for exactly
;   this reason: before the physical pedal is ever touched (or while some
;   other effect currently owns the CC29 claim), BLOOPER must read as
;   "neutral", not "pedal shoved to the floor".
;     kExprDev  = gkBLP_expr - 0.5              (-0.5 .. 0 .. +0.5)
;     kPedalMul = 1 + 2*kExprDev, clamped 0..2   (0 .. 1 .. 2)
;     kMod      = limit(kModB * kPedalMul, 0, 1)
;   At rest (0.5) kPedalMul = 1 -> kMod = kModB: pure generative, pedal has
;   zero effect. Pushing the pedal DOWN from rest (e.g. 30%) scales kMod
;   proportionally down from whatever the generative engine was producing
;   (down to 0 at pedal 0%: manually forces silence). Pushing it UP from
;   rest (e.g. 70%) scales kMod proportionally up (up to 2x at pedal 100%,
;   clamped at kMod=1 -> guaranteed every-tick firing, same edge case as
;   the master-probability knob at max). Moving the pedal back to 0.5 hands
;   control straight back to the generative engine, continuously — there is
;   no snap/toggle anywhere in this relationship.
;
;   PITCH: the only pitch parameter is the discrete per-event speed draw
;   {1x, 0.5x, 2x, -1x reverse} (see GRAIN EVENTS ARE GENERATIVE below —
;   this file still has NO continuous pitch modulator, deliberately). The
;   pedal cannot make that continuous; instead it continuously RE-WEIGHTS
;   the discrete draw: pushing the pedal up from rest shifts probability
;   mass from the 0.5x (down) slot to the 2x (up) slot (and vice versa
;   below rest), while 1x and reverse keep their original 40%/20% shares.
;   At rest the split is the original 20%/20%. This is "manual pitch
;   control" in the sense the pedal asks for, without adding a continuous
;   LFO the file's own design rules forbid.
;
; OSC:  /blp_state (i) bypass   /blp_scat /blp_size /blp_motion (f)
;       /blp_char (i)           /blp_active (i) loops being modulated
; ---------------------------------------------------------------------------

#define BLP_MAXDUTY #0.45#     ; hard ceiling on grain-event coverage

; ---- self-declared globals (nothing added to globals.orc) ----
gkBLP_bypass  init 1
gkBLP_Motion  init 0.45
gkBLP_Scat    init 0.45
gkBLP_Size    init 0.5
gkBLP_Char    init 1
gkBLP_expr    init 0.5      ; 0.5 = pedal rest/neutral (see PEDAL DESIGN above);
                             ; NOT 0, so an untouched/unclaimed pedal reads as
                             ; "pure generative", never "manually forced off"
gkBLP_Active  init 0        ; how many looper channels are being modulated
; --- bench / UI probes ---
gkBLP_Ev1     init 0        ; 1 while a grain event is sounding on that channel
gkBLP_Ev2     init 0
gkBLP_Ev3     init 0
gkBLP_Ev4     init 0
gkBLP_Ev5     init 0
gkBLP_EvId1   init 0        ; increments once per new event (ch1)
gkBLP_EvLen1  init 0        ; that event's length, samples
gkBLP_EvSpd1  init 0        ; that event's discrete speed multiplier
gkBLP_ModNow  init 0        ; live density after motion + pedal
gkBLP_DbgOn1  init 0        ; bench/debug telemetry only: ch1 loop-buffer
                             ; position (samples) at the instant the last
                             ; grain event LAUNCHED — must sit on a whole
                             ; multiple of the subdivision tick (grid proof)
gaBLP_LoopL   init 0
gaBLP_LoopR   init 0

opcode BLP_Osc3i, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "i", kval
endop

opcode BLP_Osc3f, 0, Sk
  Saddr, kval xin
  ks init 0
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
  ks = ks + 1
  OSCsend ks, "127.0.0.1", 8000, Saddr, "f", kval
endop

; ---------------------------------------------------------------------------
; BLP_Ev — ONE looper channel: normal playback, punctuated by grain events.
;
; Each call site is its own opcode instance, so the density drift and every
; random draw below are independent per channel.
;
;   aL,aR, kEv,kEvId,kEvLen,kEvSpd, kModNow, kOnDbg  BLP_Ev
;         kActive, kLen, kPhBase, kSpeed, kModKnob, kMotion, kExpr,
;         kSizeK, kDispFrac, iFnL, iFnR
;   kOnDbg (bench/debug telemetry only, not sent over OSC): the loop-buffer
;   position (samples) at the instant the last grain event LAUNCHED — sits on
;   a whole multiple of the subdivision tick (see GRID-LOCKED EVENT LAUNCH).
; ---------------------------------------------------------------------------
opcode BLP_Ev, aakkkkkk, kkkkkkkkkii
  kActive, kLen, kPhBase, kSpeed, kModKnob, kMotion, kExpr, kSizeK, kDispFrac, iFnL, iFnR xin
  kOnDbg init 0

  ; ---- per-instance state ----
  kPh    init 0        ; nominal (normal) playhead
  kGap   init 0        ; samples until the next grain event is ARMED
  kArm   init 0        ; 1 = gap expired, event fires on the next tick boundary
  kTkPrv init -1       ; previous sample's tick index (-1 = tracking not primed)
  kRem   init 0        ; samples left in the current event (0 = none)
  kELen  init 0        ; current event's total length
  kEPos  init 0        ; current event's read head
  kESpd  init 1        ; current event's discrete speed
  kEvId  init 0

  iXf = 0.008 * sr     ; 8 ms de-click ramps on every event edge

  ; ---- density: knob + motion drift is the GENERATIVE baseline, computed
  ; with NO reference to the pedal at all (kModB). The pedal is mixed in
  ; afterwards as a separate, centred manual-override stage — see PEDAL
  ; DESIGN in the file header. This is a DENSITY modulator; it is
  ; emphatically not a pitch modulator — there is none anywhere in this file.
  kMot jspline 1, 0.12, 0.8
  kModB = limit:k(kModKnob * (1 + kMotion * kMot * 0.7), 0, 1)

  ; ---- pedal: manual override, centred at rest (0.5) ----
  ; kExprDev  -0.5 (pedal floored)  .. 0 (rest, no effect)  .. +0.5 (pedal maxed)
  ; kPedalMul  0   (forces silence) .. 1 (pure generative)  .. 2   (doubles it)
  kExprDev  = kExpr - 0.5
  kPedalMul = limit:k(1 + 2 * kExprDev, 0, 2)
  kMod      = limit:k(kModB * kPedalMul, 0, 1)

  ; target coverage: 0 at kMod=0, BLP_MAXDUTY through most of the range
  ; (same shape/feel as the historical linear mapping), curving up so
  ; kMod=1 lands EXACTLY on full coverage (kDuty=1 -> zero inter-event gap
  ; -> guaranteed grain on every clock tick). Snapped rather than left to
  ; float precision, so the guarantee is exact, not asymptotic.
  if (kMod >= 0.9995) then
    kDuty = 1
  else
    kDuty = kMod * ($BLP_MAXDUTY + (1 - $BLP_MAXDUTY) * pow(kMod, 8))
  endif

  ; FULL-COVERAGE flag: at the very top of the range (both amount knobs maxed)
  ; the loop should be blooping essentially 100% of the time. Grid-locked firing
  ; only launches events on subdivision ticks, and a random event shorter than a
  ; tick leaves a stretch of clean normal playback between event-end and the next
  ; tick — so at max it was NOT covering the whole timeline. When kFull, events
  ; fire BACK-TO-BACK (the FIRE stage below ignores the tick grid), so a new grain
  ; starts the instant the previous one ends and normal playback never renders.
  kFull = (kDuty >= 0.9999 ? 1 : 0)

  aOutL init 0
  aOutR init 0

  if (kActive < 0.5 || kLen < 2) then
    aOutL = 0
    aOutR = 0
    kRem  = 0
    kGap  = 0
    kArm  = 0
    kTkPrv = -1        ; re-prime tick tracking on the next activation
  else

  ; re-sync from the looper every cycle: BLOOPER never accumulates its own
  ; transport, so it cannot drift and it follows stop/clear/varispeed for free.
  kPh = kPhBase
  kPh = (abs(kPh) < 1e12 ? kPh : 0)   ; NaN-safe read-head (2026-11-02)
  if (kPh >= kLen) then
    kPh = kPh - kLen * int(kPh / kLen)
  endif
  if (kPh < 0) then
    kPh = kPh + kLen
  endif

  ; grain-size centre: 100 ms at size 0 -> 600 ms at size 1
  kCtr = 0.10 * pow(6, kSizeK) * sr

  ; shared subdivision tick, in loop-buffer SAMPLES (grid anchored at buffer
  ; pos 0 = loop start; see GRID-LOCKED EVENT LAUNCH in the file header).
  ; The tick IS one 8TH NOTE of the loop-derived tempo (v4 derivation,
  ; chase_bliss_clock.orc: 0.1875-0.375 s whenever the implied BPM lands in
  ; the 80-160 band; 0.25 s no-loop default). It is used DIRECTLY — no
  ; further division/multiplication — so BLOOPER's grain-event grid sits
  ; exactly on the shared 8th-note grid. Clamp defensively anyway.
  kSubdivSmp = gkChaseBlissSubdivSec * sr
  kSubdivSmp = (kSubdivSmp >= 1 ? kSubdivSmp : 1)   ; NaN-safe (NaN fails >=)

  kS = 0
  while kS < ksmps do

    ; ---------------- event scheduling: ARM (gap countdown) ----------------
    ; The duty/gap countdown is unchanged from the free-running design — it
    ; still sets the event RATE from the MOD knob. Expiring only ARMS the
    ; event; the actual launch waits for the next grid tick below.
    if (kRem <= 0) then
      if (kDuty > 0.001) then
        if (kArm < 1) then
          kGap = kGap - 1
          if (kGap <= 0) then
            kArm = 1
          endif
        endif
      else
        kArm = 0           ; knob returned to zero while armed: cancel
      endif
    endif

    ; ---------------- event scheduling: FIRE (on-grid launch) --------------
    ; Tick-boundary detector, phase-anchored to the LOOP START: the tick
    ; index int(kPh/T) changes exactly when the playhead crosses a grid
    ; point (works for reverse/varispeed too — any index change is a
    ; crossing, and the loop wrap itself is the grid point at pos 0).
    ; kTkPrv = -1 means tracking isn't primed (first sample after
    ; activation): record the index without firing, so a mid-tick
    ; activation can never produce one off-grid event.
    kTickX  = 0
    kTickIdx = int(kPh / kSubdivSmp)
    if (kTkPrv >= 0 && kTickIdx != kTkPrv) then
      kTickX = 1
    endif
    kTkPrv = kTickIdx

    if (kArm == 1 && kRem <= 0 && (kTickX == 1 || kFull == 1)) then
        kArm = 0
        kOnDbg = kPh       ; telemetry: launch position, sits on the grid
        ; ---- fire a grain event (all draws continuous / free-running) ----
        kU  random 0, 1
        kU2 random 0, 1
        kU3 random 0, 1

        ; length: log-uniform x1/3..x3 around the centre -> mostly moderate,
        ; occasionally big. Generative, never a fixed size.
        kNL = kCtr * pow(3, kU * 2 - 1)
        if (kNL < 0.02 * sr) then
          kNL = 0.02 * sr
        endif
        if (kNL > kLen * 0.5) then
          kNL = kLen * 0.5
        endif

        ; speed: DISCRETE per-event choice. 1x common, then half / double /
        ; reverse. Multiplies the looper's own varispeed.
        ; PEDAL PITCH BIAS: continuously re-weights the 0.5x<->2x split
        ; around rest (still a discrete per-event draw — no continuous
        ; pitch LFO is added). 1x (40%) and reverse (20%) keep their share;
        ; only how the remaining 40% splits between "down" and "up" moves,
        ; sliding from mostly-down (pedal floored) through 20/20 (rest)
        ; to mostly-up (pedal maxed). See PEDAL DESIGN in the file header.
        kPitchBias = limit:k(kExprDev * 2, -1, 1)
        kPDown = 0.20 - 0.15 * kPitchBias
        if (kU2 < 0.40) then
          kESpd = 1
        elseif (kU2 < 0.40 + kPDown) then
          kESpd = 0.5
        elseif (kU2 < 0.80) then
          kESpd = 2
        else
          kESpd = -1
        endif

        ; position: continuous displacement around the playhead (never gridded)
        kD = kDispFrac * kLen * (kU3 * 2 - 1)
        kSt = kPh + kD
        kSt = kSt - kLen * int(kSt / kLen)
        if (kSt < 0) then
          kSt = kSt + kLen
        endif

        kEPos = kSt
        kELen = kNL
        kRem  = kNL
        kEvId = kEvId + 1
    endif

    ; ---------------- normal (contiguous) playback ----------------
    ; This is the baseline and, by design, the dominant state. At kDuty = 0 it
    ; is the ONLY thing that ever runs -> sample-exact normal loop playback.
    kI0 = int(kPh)
    kFr = kPh - kI0
    kI1 = kI0 + 1
    if (kI1 >= kLen) then
      kI1 = kI1 - kLen
    endif
    kA0 = tab:k(kI0, iFnL)
    kA1 = tab:k(kI1, iFnL)
    kNrmL = kA0 + (kA1 - kA0) * kFr
    kA0 = tab:k(kI0, iFnR)
    kA1 = tab:k(kI1, iFnR)
    kNrmR = kA0 + (kA1 - kA0) * kFr
    ; NaN/huge buffer sample must never enter the render (2026-11-02)
    kNrmL = (abs(kNrmL) < 1e3 ? kNrmL : 0)
    kNrmR = (abs(kNrmR) < 1e3 ? kNrmR : 0)

    kOutL = kNrmL
    kOutR = kNrmR

    ; ---------------- grain event (momentarily displaces the loop) ----------
    if (kRem > 0) then
      ; trapezoid envelope: 8 ms in, 8 ms out — de-clicks both edges
      kIn  = kELen - kRem
      kEnv = kIn / iXf
      kEnv = (kRem / iXf < kEnv ? kRem / iXf : kEnv)
      kEnv = limit:k(kEnv, 0, 1)

      kJ0 = int(kEPos)
      kJf = kEPos - kJ0
      kJ1 = kJ0 + 1
      if (kJ1 >= kLen) then
        kJ1 = kJ1 - kLen
      endif
      kB0 = tab:k(kJ0, iFnL)
      kB1 = tab:k(kJ1, iFnL)
      kGrL = kB0 + (kB1 - kB0) * kJf
      kB0 = tab:k(kJ0, iFnR)
      kB1 = tab:k(kJ1, iFnR)
      kGrR = kB0 + (kB1 - kB0) * kJf
      kGrL = (abs(kGrL) < 1e3 ? kGrL : 0)
      kGrR = (abs(kGrR) < 1e3 ? kGrR : 0)

      kOutL = kNrmL + (kGrL - kNrmL) * kEnv
      kOutR = kNrmR + (kGrR - kNrmR) * kEnv

      ; the event reads at its own discrete speed (this is the ONLY pitch change)
      kNp = kEPos + kSpeed * kESpd
      if (kNp >= kLen) then
        kNp = kNp - kLen
      endif
      if (kNp < 0) then
        kNp = kNp + kLen
      endif
      kEPos = kNp

      kRem = kRem - 1
      if (kRem <= 0) then
        ; schedule the next gap so the long-run coverage lands on kDuty,
        ; randomised so the punctuation never becomes a rhythm (length,
        ; position, speed and duty math ALL remain fully free-running/random
        ; — only WHEN the next event LAUNCHES is quantized, by the on-grid
        ; FIRE stage above; see GRID-LOCKED EVENT LAUNCH in the file header.
        ; NOTE (v2): the gap itself is deliberately NOT rounded any more —
        ; rounding gap DURATIONS while the launch phase free-ran was the v1
        ; design and was inaudible by construction. The continuous gap now
        ; only decides WHICH tick the next event arms in front of.
        kR random 0.7, 1.6
        if (kDuty > 0.001) then
          kGap = kELen * (1 - kDuty) / kDuty * kR
        else
          kGap = kLen
        endif
      endif
    endif

    aOutL[kS] = kOutL
    aOutR[kS] = kOutR

    ; advance the normal playhead at the looper's own rate
    kPh = kPh + kSpeed
    if (kPh >= kLen) then
      kPh = kPh - kLen
    endif
    if (kPh < 0) then
      kPh = kPh + kLen
    endif

    kS = kS + 1
  od

  endif

  kEvOn = (kRem > 0 ? 1 : 0)
  xout aOutL, aOutR, kEvOn, kEvId, kELen, kESpd, kMod, kOnDbg
endop

; ---------------------------------------------------------------------------
instr 208
  kByp = gkBLP_bypass

  kModK port gkBLP_Scat,   0.05, i(gkBLP_Scat)
  kMotn port gkBLP_Motion, 0.05, i(gkBLP_Motion)
  kSize port gkBLP_Size,   0.05, i(gkBLP_Size)
  kExpr port gkBLP_expr,   0.04, i(gkBLP_expr)
  kChar = int(limit:k(gkBLP_Char, 0, 2))

  ; CHARACTER = how far an event may throw the playhead
  if (kChar == 0) then
    kDispFrac = 0.10      ; NEAR  — stutters and near-repeats
  elseif (kChar == 1) then
    kDispFrac = 0.35      ; THROW — the default
  else
    kDispFrac = 0.50      ; FAR   — grabs from anywhere in the loop
  endif

  kAct1 = ((gkLpState1 == 2 || gkLpState1 == 3) && gkLpLen1 > 0 ? 1 : 0)
  kAct2 = ((gkLpState2 == 2 || gkLpState2 == 3) && gkLpLen2 > 0 ? 1 : 0)
  kAct3 = ((gkLpState3 == 2 || gkLpState3 == 3) && gkLpLen3 > 0 ? 1 : 0)
  kAct4 = ((gkLpState4 == 2 || gkLpState4 == 3) && gkLpLen4 > 0 ? 1 : 0)
  kAct5 = ((gkLpState5 == 2 || gkLpState5 == 3) && gkLpLen5 > 0 ? 1 : 0)

  kSp1 = (abs(gkLpEffSpd1) < 64 ? gkLpEffSpd1 : 1)   ; was giLpSpeeds[gkLpSpIdx1]: ignored the tempo trim -> 375 Hz sawtooth (2026-10-24)
  kSp2 = (abs(gkLpEffSpd2) < 64 ? gkLpEffSpd2 : 1)   ; was giLpSpeeds[gkLpSpIdx2]: ignored the tempo trim -> 375 Hz sawtooth (2026-10-24)
  kSp3 = (abs(gkLpEffSpd3) < 64 ? gkLpEffSpd3 : 1)   ; was giLpSpeeds[gkLpSpIdx3]: ignored the tempo trim -> 375 Hz sawtooth (2026-10-24)
  kSp4 = (abs(gkLpEffSpd4) < 64 ? gkLpEffSpd4 : 1)   ; was giLpSpeeds[gkLpSpIdx4]: ignored the tempo trim -> 375 Hz sawtooth (2026-10-24)
  kSp5 = (abs(gkLpEffSpd5) < 64 ? gkLpEffSpd5 : 1)   ; was giLpSpeeds[gkLpSpIdx5]: ignored the tempo trim -> 375 Hz sawtooth (2026-10-24)

  ; kBlpTrip = last block tripped the output guard: park every channel for one
  ; block (kActive=0 branch in BLP_Ev resets gap/arm/event/tick state).
  kBlpTrip init 0
  kRun = (kByp < 0.5 && kBlpTrip == 0 ? 1 : 0)

  aL1,aR1, kEv1,kId1,kEl1,kEs1, kMd1, kGp1 BLP_Ev kAct1*kRun, gkLpLen1, gkLpRdPos1, kSp1, kModK, kMotn, kExpr, kSize, kDispFrac, giLpBuf1L, giLpBuf1R
  aL2,aR2, kEv2,kId2,kEl2,kEs2, kMd2, kGp2 BLP_Ev kAct2*kRun, gkLpLen2, gkLpRdPos2, kSp2, kModK, kMotn, kExpr, kSize, kDispFrac, giLpBuf2L, giLpBuf2R
  aL3,aR3, kEv3,kId3,kEl3,kEs3, kMd3, kGp3 BLP_Ev kAct3*kRun, gkLpLen3, gkLpRdPos3, kSp3, kModK, kMotn, kExpr, kSize, kDispFrac, giLpBuf3L, giLpBuf3R
  aL4,aR4, kEv4,kId4,kEl4,kEs4, kMd4, kGp4 BLP_Ev kAct4*kRun, gkLpLen4, gkLpRdPos4, kSp4, kModK, kMotn, kExpr, kSize, kDispFrac, giLpBuf4L, giLpBuf4R
  aL5,aR5, kEv5,kId5,kEl5,kEs5, kMd5, kGp5 BLP_Ev kAct5*kRun, gkLpLen5, gkLpRdPos5, kSp5, kModK, kMotn, kExpr, kSize, kDispFrac, giLpBuf5L, giLpBuf5R

  ; unity output level always — no separate level/volume knob (removed
  ; 2026-07-14). BLOOPER's render is leveled the same as normal loop
  ; playback would be; only the scatter/duty knob (kModK) shapes the sound.
  aSumL = aL1 + aL2 + aL3 + aL4 + aL5
  aSumR = aR1 + aR2 + aR3 + aR4 + aR5

  ; output safety net: non-finite/huge block -> silence + state reset next block
  kBlpTrip NF_Chk aSumL, aSumR
  NF_Log "blooper output", kBlpTrip
  if (kBlpTrip == 1) then
    aSumL = 0
    aSumR = 0
  endif
  gaBLP_LoopL limit aSumL, -0.99, 0.99
  gaBLP_LoopR limit aSumR, -0.99, 0.99

  ; ---------------- probes + feedback ----------------
  gkBLP_Ev1 = kEv1
  gkBLP_Ev2 = kEv2
  gkBLP_Ev3 = kEv3
  gkBLP_Ev4 = kEv4
  gkBLP_Ev5 = kEv5
  gkBLP_EvId1  = kId1
  gkBLP_EvLen1 = kEl1
  gkBLP_EvSpd1 = kEs1
  gkBLP_ModNow = kMd1
  gkBLP_Active = kAct1 + kAct2 + kAct3 + kAct4 + kAct5
  gkBLP_DbgOn1 = kGp1

  kc1 changed gkBLP_bypass
  if (kc1 == 1) then
    BLP_Osc3i "/blp_state", gkBLP_bypass
  endif
  kc2 changed gkBLP_Active
  if (kc2 == 1) then
    BLP_Osc3i "/blp_active", gkBLP_Active
  endif
  kc4 changed gkBLP_Scat
  if (kc4 == 1) then
    BLP_Osc3f "/blp_scat", gkBLP_Scat
  endif
  kc5 changed gkBLP_Size
  if (kc5 == 1) then
    BLP_Osc3f "/blp_size", gkBLP_Size
  endif
  kc6 changed kChar
  if (kc6 == 1) then
    BLP_Osc3i "/blp_char", kChar
  endif
  kc7 changed gkBLP_Motion
  if (kc7 == 1) then
    BLP_Osc3f "/blp_motion", gkBLP_Motion
  endif
endin

; always-on: instr 208 runs BEFORE the looper's 209-214 every cycle (see header)
schedule 208, 0, -1

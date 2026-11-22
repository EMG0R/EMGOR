; ONWARD — dynamic sampler (Chase Bliss Onward, 2024) — REBUILD v3
; NOT CLOCKED. HARMONIC-FOLLOWING.
; ===========================================================================
; Signal chain position : 22.8 (after RESONATOR 22.7, before REVERB 23)
; Menu / slot           : CHASE BLISS menu (11), slot 1 — toggle 45, CC75,
;                         instr 155, OSC prefix "onw", home rotation CC25
; Reads                 : gaONW_sendL / gaONW_sendR
; Writes                : gaONW_outL  / gaONW_outR   (DRY UNITY + wet added
;                         inline; chain ntrpol keeps mix constant = 1)
; Bypass                : gkONW_bypass (standard FX_BYPASS — no trails)
;
; ---------------------------------------------------------------------------
; RESEARCH — what the REAL pedal does (read the sources, don't guess)
;   Official product/manual: Chase Bliss call Onward a "Dynamic Envelope
;   Sampler" (chasebliss.com/onward; manual mirrors: manualslib.com/manual/
;   3472953/Chase-Bliss-Audio-Onward.html, static1.squarespace.com/static/
;   622176a9b8d15d57ffbf5700/t/665deb49edd59863d39764fa/
;   Onward_Manual_Pedal_Chase-Bliss.pdf). "Whenever Onward detects sound at
;   the input, it samples that sound. If there was already a sample, it
;   replaces it" — capture is triggered by playing DYNAMICS (a SENSITIVITY/
;   SIZE hidden option sets "the ease with which you trigger the effected
;   signal"), not a clock. GLITCH "loops that captured chunk... mangles and
;   re-arranges your sample in randomized ways"; FREEZE sustains it as a pad.
;   ERROR (TIMING/CONDITION/PLAYBACK) randomises each replay. OCTAVE adds a
;   half/double-speed companion voice. Reviews agree and add nothing about
;   pitch: Guitar.com ("this pedal will jump in volume depending on the
;   dynamics of your pick attack... when you get loud, it gets loud" —
;   guitar.com/reviews/effects-pedal/hands-on-chase-bliss-onward-review/);
;   GuitarPedalX's writeup on Tom Majeski's design lineage (CooperFX Outward
;   -> Onward) again describes envelope/dynamics triggering and the Glitch/
;   Freeze/Octave/Error control set, never pitch (guitarpedalx.com/news/
;   gpx-blog/chase-blisss-latest-onward-dynamic-sampling-device...).
;
;   HONEST CONCLUSION: the real Onward does NOT pitch-track, does NOT know
;   what key you're in, and does NOT transpose the sample to match your note.
;   It is a dynamics-triggered capture/replace sampler, full stop — "Dynamic
;   Envelope Sampler" is its own name for itself. Nothing in the manual, the
;   reviews, or the GuitarPedalX design piece mentions harmony, key, or pitch
;   detection anywhere. HARMONIC-FOLLOWING in this build is a DELIBERATE
;   DESIGN CHOICE beyond the real pedal, built because the user explicitly
;   asked for a sampler that fits what is being played, harmonically — not a
;   faithful clone of the hardware's (harmony-blind) behaviour.
;
; ---------------------------------------------------------------------------
; WHY v2 FAILED (user, playing it live)
;   v2's Glitch engine re-decided its segment (new slice offset, new speed/
;   reverse/mute error draw) on a free-running block counter (kBlk >= kSegB)
;   — i.e. a metronome inside the effect, ticking at whatever rate SIZE set,
;   completely independent of whether you were playing or not. That is
;   "clocked": the mangle events fired on a schedule, not on your picking.
;   It also had zero knowledge of what note you were playing — OCTAVE was a
;   fixed, hand-set half/double-speed offset, not a pitch relationship to the
;   live note, so the loop could (and did) grind against you harmonically.
;
; WHAT THIS BUILD DOES INSTEAD
;   1. NOT CLOCKED. The Glitch engine's segment decisions (new slice, new
;      speed/reverse/mute error draw, new harmonic lock) fire ONLY on a
;      "new event": a genuine player onset (the shared OnsetPick UDO) or —
;      while you are actively playing, never in silence — a probabilistic
;      DENSITY-scaled "call-and-response" echo. Between events the segment
;      just LOOPS (exactly what the real pedal's Glitch does — "loops that
;      captured chunk"); nothing about it changes until you play again. No
;      new events ever fire while the input is silent (bench SCN1 proves
;      this: 8 irregular picks -> 8 events, then 2.5 s of noise-floor
;      silence -> zero more events).
;   2. HARMONIC-FOLLOWING. Live pitch is tracked continuously with `ptrack`
;      on the mono input. At capture time the live pitch is latched as that
;      buffer's "native" pitch (giONW_capCps{A,B}) — the pitch of the note
;      that triggered the recording. At every new-event decision, the ratio
;      (current live pitch / that buffer's native pitch) is computed, then
;      OCTAVE-REDUCED and SNAPPED to the nearest ratio in a fixed consonant
;      table (unison 1:1, octave 2:1, and — as FOLLOW is turned up — fifth
;      3:2, then major third 5:4 / fourth 4:3), via the OnwSnapRatio UDO.
;      FOLLOW (0..1) blends the playback-rate multiplier LINEARLY from 1.0
;      (raw, unshifted — FOLLOW=0 is bit-identical to "off") to the snapped
;      ratio (FOLLOW=1 = fully harmonically locked). Because the table is
;      FIXED and every candidate is a genuine consonant interval, the result
;      can never land on a sour ratio — there is no continuous/free pitch
;      shift that could drift onto a dissonant interval; it always snaps to
;      one of a short, hand-picked, always-consonant list. Turning FOLLOW up
;      both strengthens the pull toward the table AND widens the table
;      (unison+octave only below 0.34, +fifth 0.34-0.67, +third/fourth
;      above 0.67) — one control does double duty as "how hard" and "how
;      rich" the harmonic lock is.
;   3. Freeze (the pad/sustain channel) keeps granulating continuously while
;      held — that is what a sustain pad IS, not a clock — but the grains'
;      START position and the harmonic lock only move on a new-event too, so
;      the "what note is this pad remembering" question is answered the same
;      way as Glitch.
;
; STILL A DYNAMIC SAMPLER: it continuously records what you play into one of
; two ping-ponged buffers (replacing, never stacking, per the manual quote
; above), and throws that material back at you — now fitted, on every new
; event, to whatever you are playing right now.
;
; KEPT FROM v2 (measured, still true): reliable dynamics triggering (fires
; on real picks, not noise floor); the AUDIBILITY level law (giONW_makeup
; +6dB compensates FREEZE reading the sustain region, not the pick peak);
; zero-latency dry path; bit-exact dry at mix=0; block-quantized edges (no
; clicks); bounded output.
;
; ---------------------------------------------------------------------------
; INTEGRATION NOTES  (integration agent owns every shared file — apply these)
;
;   [1] csound/neptrPhase4.csd — UNCHANGED. Already has, in this order:
;         #include "onset_pick.orc"
;         #include "effects/onward.orc"
;
;   [2] csound/signal_chain.orc @ stage 22.8 — UNCHANGED, still correct:
;         gaONW_sendL = aPrevL
;         gaONW_sendR = aPrevR
;         $FX_BYPASS(gkONW_bypass'155'gaONW_sendL'gaONW_sendR'gaONW_outL'gaONW_outR'onw)
;         aPrevL ntrpol gaONW_sendL, gaONW_outL, 1
;         aPrevR ntrpol gaONW_sendR, gaONW_outR, 1
;
;   [3] PARAM MAP (globals declared in THIS file; may be moved to globals.orc
;       verbatim). BREAKING CHANGE from v2's ONW_TYPES stepped param — TYPE
;       is gone from the encoder surface (see below), so drop any v2
;       ONW_TYPES / STEP_LABELS[("onw","type")] UI wiring.
;         gkONW_mix     0..1   Mix       — sampled layer level against dry
;         gkONW_blend   0..1   Blend     — 0 Freeze pad .. 1 Glitch repeats
;         gkONW_size    0..1   Size      — sample length + nominal seg length
;         gkONW_follow  0..1   Follow    — NEW. harmonic-follow strength AND
;                                          interval-table width (see above)
;         gkONW_density 0..1   Density   — NEW. call-and-response echo rate
;                                          while playing; 0 = onsets only.
;                                          (2026-07-14: candidate echo timing
;                                          is now gated to the shared CHASE
;                                          BLISS subdivision tick, see instr
;                                          155 and csound/chase_bliss_clock.
;                                          orc — density is the per-tick
;                                          probability now, not a per-block
;                                          rate. Onset-triggered events are
;                                          unaffected.)
;         gkONW_err     0..1   Error     — mangle intensity (speed/reverse/
;                                          dropout/decimate), same family as
;                                          v2's ERROR (ALSO CC29 candidate,
;                                          but see CC29 note below)
;         gkONW_oct    -1..1   Octave    — companion half/double-speed voice
;                                          (character control, NOT part of
;                                          the harmonic-follow mechanism)
;       Non-encoder (sane defaults baked in): gkONW_sust .55 (SHAPE hold),
;       gkONW_sens .6, gkONW_fade .12, gkONW_tex -.35, gkONW_anim 0,
;       gkONW_duck .3, gkONW_hold 0, gkONW_errtype 3 (0 TIMING/1 CONDITION/
;       2 PLAYBACK/3 ALL — replaces v2's encoder-exposed TYPE).
;
;   [4] csound/midi_handler.orc — ONWARD is toggle 45, CC75, menu 11 slot 1,
;       home rotation CC25. Replace the lastToggle==45 rotation cases with
;       (each via $ENC_ROT(var'step'min'max), per encoder_mapping_v2):
;         P1  CC25  gkONW_mix     [0..1]   lin  .02  -> /onw_mix     "Mix"
;         P2  CC20  gkONW_blend   [0..1]   lin  .02  -> /onw_blend   "Blend"
;         P3  CC23  gkONW_size    [0..1]   lin  .02  -> /onw_size    "Size"
;         P4  CC24  gkONW_follow  [0..1]   lin  .02  -> /onw_follow  "Follow"
;         P5  CC26  gkONW_density [0..1]   lin  .02  -> /onw_density "Density"
;         P6  CC21  gkONW_err     [0..1]   lin  .02  -> /onw_err     "Error"
;         P7  CC22  gkONW_oct    [-1..1]   lin  .05  -> /onw_oct     "Octave"
;       FX_TOGGLE gkONW_bypass'45 on CC75.
;
;       *** TOGGLE-ON DEFAULTS — set these EXACTLY in the toggle-on branch ***
;         gkONW_mix     = 0.6
;         gkONW_blend   = 0.75   ; glitch-led but freeze audibly present
;         gkONW_size    = 0.3    ; ~0.16-0.3 s chunks
;         gkONW_follow  = 0.6    ; solidly harmonic-locked (fifth-table range)
;         gkONW_density = 0.15   ; light call-and-response, mostly onset-led
;         gkONW_err     = 0.35   ; audible mangle without dissolving into noise
;         gkONW_oct     = 0
;       Non-encoder globals live in this file with these defaults: sust .55,
;       sens .6, fade .12, tex -.35, anim 0, duck .3, hold 0, errtype 3.
;
;       CC29 (expression) case for lastToggle==45:
;         gkONW_follow = <pedal 0..1>   ; heel = raw/unshifted, toe = fully
;                                       ; harmonic-locked + widest interval set
;         keep the sticky claim: set gkLastToggle29 = 45 on toggle-ON
;         (last-toggled-wins vs TRACKSYNTH 61 / FEEDBACKER 63 / BLOOPER 56).
;       WHY FOLLOW on the pedal, not ERROR (v2's choice): FOLLOW is this
;       build's headline, non-real-pedal gesture — sweeping the expression
;       pedal live morphs the sampler from "plays back whatever it captured,
;       raw" to "everything it throws back is harmonically glued to the note
;       you're holding right now". It is the most audible, most musical, and
;       most on-concept expression target for THIS build, it never mutes the
;       effect, and it directly demonstrates the harmonic-following design
;       goal to a listener. (v2 pointed CC29 at ERROR; that control still
;       exists here as gkONW_err and remains a fine manual/preset target, but
;       FOLLOW is more central to what changed.)
;
;   [5] csound/midi_handler.orc OSC feedback — add $OSC_F sends for:
;       /onw_state /onw_mix /onw_blend /onw_size /onw_follow /onw_density
;       /onw_err /onw_oct
;       (REMOVE any old /onw_sust, /onw_type, /onw_pad, /onw_gate sends.)
;
;   [6] neptrPhase4_UI.py — MENUS m11 s1 stays ("onw","ONWARD"). Replace the
;       EFFECT_PARAMS row:
;         "onw": [("mix", "Mix", 0, 1), ("blend", "Blend", 0, 1),
;                 ("size", "Size", 0, 1), ("follow", "Follow", 0, 1),
;                 ("density", "Density", 0, 1), ("err", "Error", 0, 1),
;                 ("oct", "Octave", -1, 1)],
;       Remove any v2 ONW_TYPES / STEP_LABELS[("onw","type")] entry — TYPE is
;       no longer on the encoder/UI surface (it's the non-encoder
;       gkONW_errtype global, default 3 = ALL).
;
;   [7] Non-encoder globals with no UI row (baked-in defaults, OSC- or
;       preset-settable later): gkONW_sust .55, gkONW_sens .6, gkONW_fade
;       .12, gkONW_tex -.35, gkONW_anim 0, gkONW_duck .3, gkONW_hold 0,
;       gkONW_errtype 3. gkONW_hold is the natural target for a press-and-
;       hold gesture on CC75 if the handler ever grows one.
;
; CPU: ptrack (hop 256, sr 48000, ksmps 128) is the one meaningfully new
;   cost vs v2 — one continuous pitch-tracking analysis, always running
;   while the effect is on (not gated to onsets, since FOLLOW needs the
;   live pitch at arbitrary moments). Budget MEDIUM: comparable to a
;   pvsanal-based effect. MEASURED (scripts/bench, M-series, 48k, ksmps128):
;   see bench report for instr 155 %CPU. If it ever needs trimming: raise
;   ptrack's hopsize (256 -> 512, still >= ksmps) for roughly half the
;   ptrack cost, at the price of slower pitch-lock reaction.
;
; LATENCY: dry path adds ZERO samples. Wet path is a sampler by design — its
;   output is audio you played SIZE seconds ago, now harmonically fitted.
; ===========================================================================

; ---- sample memory: each engine PERMANENTLY owns one buffer (no swapping) --
giONW_bufA  ftgen 0, 0, 65536, 2, 0     ; 1.365 s @ 48k (max record = 1.0 s)
giONW_bufB  ftgen 0, 0, 65536, 2, 0
giONW_trash ftgen 0, 0, 1024,  2, 0     ; write sink while not recording
giONW_win   ftgen 0, 0, 8193,  20, 2, 1 ; Hanning grain window
giONW_sine  ftgen 0, 0, 4096,  10, 1    ; ANIMATE LFO

; ---- calibrated gains (see LEVEL LAW; verified by scripts/bench) ----------
giONW_frzG   = 1.0       ; freeze engine trim (grain sum is already COLA-flat)
giONW_gltG   = 1.0       ; glitch engine trim (loop replays the buffer 1:1)
giONW_makeup = 2.0       ; +6 dB — freeze reads the sustain region, not the peak

; ---- TOGGLE-ON DEFAULTS ----------------------------------------------------
gkONW_bypass  init 1
gkONW_mix     init 0.6     ; P1 CC25  — sampled layer level against dry
gkONW_blend   init 0.75    ; P2 CC20  — 0 Freeze pad .. 1 Glitch
gkONW_size    init 0.3     ; P3 CC23  — sample/segment length
gkONW_follow  init 0.6     ; P4 CC24  — harmonic-follow strength (ALSO CC29)
gkONW_density init 0.15    ; P5 CC26  — call-and-response echo density
gkONW_err     init 0.35    ; P6 CC21  — mangle intensity
gkONW_oct     init 0       ; P7 CC22  — -1 half-speed .. +1 double-speed
gkONW_sust    init 0.55    ; SHAPE hold (non-encoder)
gkONW_sens    init 0.6     ; onset sensitivity (non-encoder)
gkONW_fade    init 0.12    ; crossfade time (non-encoder)
gkONW_tex     init -0.35   ; -1 sample-rate crush .. 0 off .. +1 drive
gkONW_anim    init 0       ; -1 vibrato .. 0 off .. +1 chorus (non-encoder)
gkONW_duck    init 0.3     ; duck depth (non-encoder)
gkONW_hold    init 0       ; 1 = lock the current sample (non-encoder)
gkONW_errtype init 3       ; 0 TIMING/1 CONDITION/2 PLAYBACK/3 ALL (non-encoder)
gkONW_caps    init 0       ; telemetry: accepted capture count
gkONW_evts    init 0       ; telemetry: accepted new-event (retrigger) count
gkONW_dbgExtra init 0      ; bench telemetry only: 1 on the block a density-
                            ; driven "extra" echo fires (subdivision-tick-gated)
gaONW_sendL   init 0
gaONW_sendR   init 0
gaONW_outL    init 0
gaONW_outR    init 0

; ===========================================================================
; OnwSnapRatio — octave-reduce kRatioIn to [1,2), snap to the nearest ratio in
; a fixed, always-consonant table, then restore the octave. kSet widens the
; table: 0 = {1, 2} (unison/octave only) .. 1 = {1, 1.5, 2} (+fifth) ..
; 2 = {1, 1.25, 1.3333, 1.5, 2} (+ third/fourth). Every candidate is a real
; consonant interval, so the output can NEVER be a sour ratio.
; ===========================================================================
opcode OnwSnapRatio, k, kk
  kRatioIn, kSet xin

  kR = (kRatioIn <= 0.0001 ? 1 : kRatioIn)
  kOctMult = 1
  kIter = 0
  while (kR >= 2 && kIter < 12) do
    kR = kR * 0.5
    kOctMult = kOctMult * 2
    kIter = kIter + 1
  od
  kIter = 0
  while (kR < 1 && kIter < 12) do
    kR = kR * 2
    kOctMult = kOctMult * 0.5
    kIter = kIter + 1
  od

  kBest  = 1
  kBestD = abs(kR - 1)
  kD2 = abs(kR - 2)
  if (kD2 < kBestD) then
    kBestD = kD2
    kBest  = 2
  endif
  if (kSet >= 0.5) then
    kD15 = abs(kR - 1.5)
    if (kD15 < kBestD) then
      kBestD = kD15
      kBest  = 1.5
    endif
  endif
  if (kSet >= 1.5) then
    kD125  = abs(kR - 1.25)
    kD1333 = abs(kR - 1.33333333)
    if (kD125 < kBestD) then
      kBestD = kD125
      kBest  = 1.25
    endif
    if (kD1333 < kBestD) then
      kBestD = kD1333
      kBest  = 1.33333333
    endif
  endif

  xout kBest * kOctMult
endop

; ===========================================================================
; OnwErr — one error draw. Shared by both engines so TIMING/CONDITION/PLAYBACK
; mean the same thing on each side.
;   in : kDraw (1 = redraw now), kErr 0..1, kType 0..3
;   out: kRate (playback speed sign/mag, <0 = reverse), kLenSc (segment length
;        scale), kMute (1 = dropout), kDec (read-index decimation, 1 = off),
;        kOffFr (0..1 re-arrange: which slice of the sample to jump to)
; ===========================================================================
opcode OnwErr, kkkkk, kkk
  kDraw, kErr, kType xin

  kRate  init 1
  kLenSc init 1
  kMute  init 0
  kDec   init 1
  kOffFr init 0

  kR1 trandom kDraw, 0, 1     ; does an error happen at all
  kR2 trandom kDraw, 0, 1     ; which flavour within the type
  kR3 trandom kDraw, 0, 1     ; type pick when TYPE = ALL
  kR4 trandom kDraw, 0, 1     ; reverse coin
  kR5 trandom kDraw, 0, 1     ; re-arrange: where in the sample to jump to

  if (kDraw == 1) then
    kRate  = 1
    kLenSc = 1
    kMute  = 0
    kDec   = 1
    kOffFr = 0
    if (kR1 < kErr * 0.9) then
      kTy = (kType >= 3 ? int(kR3 * 2.999) : kType)

      if (kTy == 0) then
        ; TIMING — chop into a subdivision AND jump to a random slice.
        kSub   = (kR2 < 0.2 ? 0.125 : kR2 < 0.4 ? 0.25 : kR2 < 0.6 ? 0.3333 : kR2 < 0.8 ? 0.5 : 0.6667)
        kLenSc = 1 + (kSub - 1) * min(1, kErr * 1.6)
        kOffFr = kR5

      elseif (kTy == 1) then
        ; CONDITION — dropouts and momentary sample-rate shifts
        if (kR2 < 0.5) then
          kMute = 1
        else
          kDec = 1 + int(kErr * 28 * kR2)
        endif

      else
        ; PLAYBACK — random speed (2x/4x up or down) and direction
        kMag = (kR2 < 0.35 ? 0.5 : kR2 < 0.70 ? 2.0 : kR2 < 0.85 ? 0.25 : 4.0)
        if (kErr < 0.5) then
          kMag = (kMag == 4.0 ? 2.0 : kMag == 0.25 ? 0.5 : kMag)
        endif
        kRate = (kR4 < 0.45 ? -kMag : kMag)
      endif
    endif
  endif

  xout kRate, kLenSc, kMute, kDec, kOffFr
endop

; ===========================================================================
; OnwGlitch — chops, stutters, re-arranges, speed-flips, reverses and
; HARMONICALLY LOCKS the captured sample out of ONE fixed ftable.
;
; NOT CLOCKED: the segment (offset/rate/length/mute + harmonic ratio) is only
; re-decided on kAdopt (a fresh capture just landed) or on kNewEvt while this
; buffer kIsActive (a real player onset, or a density-gated call-and-response
; echo that ONLY ever fires while the player is actively sounding). Between
; those moments the segment just LOOPS at its last-decided length/rate/offset
; — exactly the real pedal's "loops that captured chunk" Glitch behaviour —
; with nothing about it changing on its own.
;
; kHarm is the ALREADY-SNAPPED, ALREADY-FOLLOW-BLENDED harmonic ratio (see
; instr 155): multiplying it into the playback rate at decision time is what
; makes the loop consonant with whatever note is being played right now.
; ===========================================================================
opcode OnwGlitch, aakkkkk, kkkkkkkkki
  kSizeS, kRecS, kHarm, kNewEvt, kIsActive, kErr, kType, kOct, kAdopt, ifn xin

  kSegB   init 8        ; current segment DURATION in k-blocks
  kSegSm  init 4800      ; source samples the segment spans
  kRateC  init 1        ; playback speed (<0 = reverse), harm-multiplied
  kOffC   init 0        ; slice offset into the recording, in samples
  kDecC   init 1
  kMuteC  init 0
  kLenScC init 1

  ; Segment-length floor, in SAMPLES first — block count derived from it.
  ; Two independent requirements, both of which must hold:
  ;   (a) >= 2 k-blocks, so the syncphasor below always gets a clean 0->1
  ;       reset edge (this is what the old bare `max(2, ...)` expressed);
  ;   (b) >= 128 source samples (2.67 ms @ 48 k), so the SHORTEST glitch cell
  ;       occupies the same WALL-CLOCK time whatever the block size is.
  ; 128 is exactly what 2 blocks came to at the patch's ksmps 64
  ; (globals.orc), so at ksmps 64 this is max(2, 2) = 2 — bit-identical to
  ; the previous constant. At ksmps 32 it is max(2, 4) = 4 blocks = the same
  ; 128 samples, instead of halving to 64. At ksmps 128 the 2-block rule
  ; dominates, as it must.
  ; The floor is only reachable at the extremes (tiny SIZE with a large
  ; |rate x harm|, i.e. err-driven 4x speed on an octave-up snap), but that
  ; is exactly the shortest-stutter texture where a halved cell is audible.
  iSegBMin = max(2, ceil(128 / ksmps))

  kAllowNew = (kNewEvt == 1 && kIsActive == 1 ? 1 : 0)
  kBound = (kAdopt == 1 || kAllowNew == 1 ? 1 : 0)
  kRateN, kLenScN, kMuteN, kDecN, kOffFrN OnwErr kBound, kErr, kType

  if (kAdopt == 1) then
    ; fresh sample: clean, forward, full length, from the top, harm-locked
    kLenScC = 1
    kRateC  = 1 * kHarm
    kMuteC  = 0
    kDecC   = 1
    kOffC   = 0
    kSegLs  = limit(kSizeS, 0.02, kRecS)
    kSegB   = max(iSegBMin, round(kSegLs / abs(kRateC) * kr))
    kSegSm  = min(kSegB * ksmps * abs(kRateC), kRecS * sr)
  elseif (kAllowNew == 1) then
    kLenScC = kLenScN
    kRateC  = kRateN * kHarm
    kMuteC  = kMuteN
    kDecC   = kDecN
    kSegLs  = limit(kSizeS * kLenScN, 0.02, kRecS)
    ; duration = span / |rate|, rounded to whole blocks (floored at iSegBMin
    ; for a clean 0->1 reset edge AND a block-size-independent minimum cell —
    ; see above); the SPAN is then DERIVED BACK from the rounded block count
    ; so read speed = span / (blocks*ksmps) = |rate| EXACTLY.
    kSegB   = max(iSegBMin, round(kSegLs / abs(kRateC) * kr))
    kSegSm  = min(kSegB * ksmps * abs(kRateC), kRecS * sr)
    kOffC   = kOffFrN * max(0, kRecS * sr - kSegSm)
  endif

  ; ---- read pointer: phase 0..1 across exactly kSegB blocks, reset only on
  ; a genuine new event (kBound). While no new event occurs the phasor just
  ; wraps and the SAME segment repeats — a loop, not a re-triggered event.
  kRst  = kBound
  aOne  = a(kRst)
  aPrv  delay1 aOne
  aImp  limit aOne - aPrv, 0, 1
  kFreq = kr / kSegB
  aPh, aSy syncphasor kFreq, aImp

  kRev  = (kRateC < 0 ? 1 : 0)
  aRamp = (kRev == 1 ? 1 - aPh : aPh)
  aPos  = aRamp * kSegSm

  ; CONDITION sample-rate crush
  aQ    = int(aPos / kDecC) * kDecC
  kUseQ = (kDecC > 1 ? 1 : 0)
  aPosC = aPos + (aQ - aPos) * kUseQ

  aIdx  wrap a(kOffC) + aPosC, 0, kRecS * sr
  aMain table3 aIdx, ifn

  ; OCTAVE: a second read at half or double speed through the same buffer
  kOctR = (kOct < 0 ? 0.5 : 2)
  aIdxO wrap a(kOffC) + aPosC * kOctR, 0, kRecS * sr
  aOctv table3 aIdxO, ifn

  ; ---- edge window: 1.5 ms, just enough to bridge the seam without a click.
  kSegDur = kSegB / kr
  kEdge   = min(0.0015, kSegDur * 0.08)
  aE      = limit(min(aPh, 1 - aPh) * kSegDur / kEdge, 0, 1)
  aWin    = 0.5 - 0.5 * cos(3.14159265 * aE)

  kMuteG = 1 - kMuteC
  aMuteG interp kMuteG

  kOctA = abs(kOct)
  aM = aMain * aWin * aMuteG
  aO = aOctv * kOctA * aWin * aMuteG

  xout aM + aO * 0.62, aM + aO * 1.28, kBound, kRateC, kLenScC, kMuteC, kDecC
endop

; ===========================================================================
; OnwGrain — one Hann grain of the FREEZE cloud. Reads the SUSTAIN region of
; the sample (28-62% in, past the pick attack) with per-grain start jitter.
; The grain LOOP itself is continuous (that is what a sustained pad IS — not
; a clock); only the START position, error draw and harmonic lock update on
; a genuine new event (kAdopt or kNewEvt while kIsActive).
; ===========================================================================
opcode OnwGrain, aakkkkk, kkkkkkkkkkii
  kGr, kRecS, kHarm, kNewEvt, kIsActive, kAdopt, kErr, kType, kOct, kPan, iPhs, ifn xin

  kRate  init 1
  kLenSc init 1
  kMute  init 0
  kDec   init 1
  kStart init 0

  kGrL  limit kGr * kLenSc, 0.02, kRecS
  kFreq = kRate / kGrL

  aZ = a(0)
  aPh, aSy syncphasor kFreq, aZ, iPhs
  kPh downsamp aPh

  kPhPrev init 0
  kWrap = (abs(kPh - kPhPrev) > 0.5 ? 1 : 0)
  kPhPrev = kPh

  kAllowNew = (kNewEvt == 1 && kIsActive == 1 ? 1 : 0)
  kDraw = (kAdopt == 1 || (kWrap == 1 && kAllowNew == 1) ? 1 : 0)
  kRateN, kLenScN, kMuteN, kDecN, kOffFrN OnwErr kDraw, kErr, kType
  kRJ trandom kDraw, 0, 1
  if (kDraw == 1) then
    kRate  = kRateN * kHarm
    kLenSc = kLenScN
    kMute  = kMuteN
    kDec   = kDecN
    ; new grain start: sustain region + jitter, always inside the recording
    kStart = limit(kRecS * (0.28 + 0.34 * kRJ), 0, max(0.001, kRecS - 0.004))
  endif

  aPos  = aPh * (kGrL * sr)
  aQ    = int(aPos / kDec) * kDec
  kUseQ = (kDec > 1 ? 1 : 0)
  aPosC = aPos + (aQ - aPos) * kUseQ

  aBase = a(kStart * sr)
  aIdx  wrap aBase + aPosC, 0, kRecS * sr
  aMain table3 aIdx, ifn

  kOctR = (kOct < 0 ? 0.5 : 2)
  aIdxO wrap aBase + aPosC * kOctR, 0, kRecS * sr
  aOctv table3 aIdxO, ifn

  aWinH tablei aPh, giONW_win, 1
  aWin  = sqrt(aWinH)

  kMuteG portk (kMute == 1 ? 0 : 1), 0.003
  aMuteG interp kMuteG

  kOctA = abs(kOct)
  aSum = (aMain + aOctv * kOctA) * aWin * aMuteG

  kTh = (kPan + 1) * 0.7853981634
  xout aSum * cos(kTh), aSum * sin(kTh), kDraw, kRate, kLenSc, kMute, kDec
endop

; ===========================================================================
; OnwFreeze — 3 sqrt-Hann grains at 1/3 hop (power-COLA; see sqrt-Hann note
; below). Grains panned L/C/R for width without a fixed comb.
; ===========================================================================
opcode OnwFreeze, aakkkkk, kkkkkkkkki
  kGr, kRecS, kHarm, kNewEvt, kIsActive, kAdopt, kErr, kType, kOct, ifn xin

  aL1,aR1,kB1,kRt1,kLs1,kMu1,kDc1 OnwGrain kGr,kRecS,kHarm,kNewEvt,kIsActive,kAdopt,kErr,kType,kOct,-0.55, 0.00000, ifn
  aL2,aR2,kB2,kRt2,kLs2,kMu2,kDc2 OnwGrain kGr,kRecS,kHarm,kNewEvt,kIsActive,kAdopt,kErr,kType,kOct, 0.00, 0.33333, ifn
  aL3,aR3,kB3,kRt3,kLs3,kMu3,kDc3 OnwGrain kGr,kRecS,kHarm,kNewEvt,kIsActive,kAdopt,kErr,kType,kOct, 0.55, 0.66667, ifn

  ; sqrt-Hann, NOT Hann: the grains read JITTERED (decorrelated) positions,
  ; so they add in POWER, not amplitude. sum(w^2)=1.5 at 1/3 hop is the
  ; correct COLA condition, so x1/sqrt(1.5) flattens the cloud.
  aL = (aL1 + aL2 + aL3) * 0.81649658
  aR = (aR1 + aR2 + aR3) * 0.81649658

  kB  = (kB1 + kB2 + kB3 > 0 ? 1 : 0)
  kRt = (kB1 == 1 ? kRt1 : kB2 == 1 ? kRt2 : kRt3)
  kLs = (kB1 == 1 ? kLs1 : kB2 == 1 ? kLs2 : kLs3)
  kMu = (kB1 == 1 ? kMu1 : kB2 == 1 ? kMu2 : kMu3)
  kDc = (kB1 == 1 ? kDc1 : kB2 == 1 ? kDc2 : kDc3)
  xout aL, aR, kB, kRt, kLs, kMu, kDc
endop

; ===========================================================================
instr 155
  aInL  = gaONW_sendL
  aInR  = gaONW_sendR
  aMono = (aInL + aInR) * 0.5
  kDT   = ksmps / sr

  ; ------------------------------------------------------------------ params
  kMix    limit gkONW_mix,     0, 1
  kBlend  limit gkONW_blend,   0, 1
  kSizeK  limit gkONW_size,    0, 1
  kFollow limit gkONW_follow,  0, 1
  kDens   limit gkONW_density, 0, 1
  kErr    limit gkONW_err,     0, 1
  kOct    limit gkONW_oct,    -1, 1
  kType   = limit(int(gkONW_errtype + 0.5), 0, 3)
  kSust   limit gkONW_sust,    0, 1
  kSens   limit gkONW_sens,    0, 1
  kFadeK  limit gkONW_fade,    0, 1
  kTex    limit gkONW_tex,    -1, 1
  kAnim   limit gkONW_anim,   -1, 1
  kDuckD  limit gkONW_duck,    0, 1
  kHold   = (gkONW_hold >= 0.5 ? 1 : 0)

  kSizeS = 0.08 + 0.92 * kSizeK * kSizeK              ; 0.08 .. 1.00 s (max rec)
  kFadeT = limit(kSizeS * (0.06 + 1.7 * kFadeK * kFadeK), 0.015, 0.9)
  kGrain = limit(0.05 + 0.30 * kSizeS, 0.05, 0.35)    ; freeze grain length
  kHoldT = kSizeS * (0.6 + 40 * kSust * kSust)        ; SUSTAIN, linked to SIZE
  kInf   = (kSust >= 0.97 ? 1 : 0)                    ; max SUSTAIN = infinite

  ; ------------------------------------------------- onset -> capture request
  kTrig, kEnv OnsetPick aMono, kSens, 0.06
  kEnvDb = dbamp(max(kEnv, 0.000001))

  kFloor init -60
  kFloor = (kEnvDb < kFloor ? kEnvDb : kFloor + 6 * kDT)
  kFloor limit kFloor, -90, -30
  kGateDb = max(-46 - 8 * kSens, kFloor + 12)

  kQuietT init 1
  kArmed  init 1
  kWake = (kArmed == 1 && kEnvDb > kGateDb ? 1 : 0)
  if (kEnvDb > kGateDb) then
    kArmed  = 0
    kQuietT = 0
  else
    kQuietT = kQuietT + kDT
    if (kQuietT > 0.20) then
      kArmed = 1
    endif
  endif
  kOnset = ((kTrig == 1 && kEnvDb > kGateDb) || kWake == 1 ? 1 : 0)

  ; -------------------------------------------------- live pitch tracking
  ; Continuous, not onset-gated (FOLLOW needs the live pitch at any moment).
  ; Sample-and-hold on unvoiced/out-of-range frames so the harmonic target
  ; doesn't chase noise between notes.
  kLiveCpsRaw, kLiveAmpRaw ptrack aMono, 256
  kLiveCpsHeld init 220
  kVoiced = (kEnvDb > kGateDb && kLiveCpsRaw > 55 && kLiveCpsRaw < 1300 ? 1 : 0)
  if (kVoiced == 1) then
    kLiveCpsHeld = kLiveCpsRaw
  endif

  ; --------------------------------------------- NOT-CLOCKED event gating
  ; A "new event" is either a real player onset, or — ONLY while the player
  ; is actively sounding — a density-scaled call-and-response echo. Silence
  ; (kEnvDb <= kGateDb) can NEVER produce a new event: this is the guarantee
  ; that bench SCN1 (picks then noise-floor silence) checks directly.
  kSinceEvt init 999
  kSinceEvt = kSinceEvt + kDT
  kPlayingGate = (kEnvDb > kGateDb ? 1 : 0)

  ; MASTER SUBDIVISION TICK (2026-07-14): the call-and-response echo's event
  ; SPACING now references the shared CHASE BLISS subdivision tick
  ; (gkChaseBlissSubdivSec, csound/chase_bliss_clock.orc — derived from the
  ; LOOPER's master loop length) instead of rolling dice every k-block. A new
  ; "extra" echo can only be CONSIDERED on a tick boundary; DENSITY (kDens)
  ; is now the per-tick probability directly, rather than a per-block rate.
  ; Onset-triggered events (kOnset) are completely unaffected — they still
  ; fire immediately on a real pick, exactly as before; only the density-
  ; driven "extra" echo is tick-gated.
  kOnwTickAcc init 0
  kOnwTickAcc = kOnwTickAcc + kDT
  kOnwTick = 0
  if (kOnwTickAcc >= gkChaseBlissSubdivSec) then
    kOnwTickAcc = kOnwTickAcc - gkChaseBlissSubdivSec
    kOnwTick = 1
  endif
  kDensDraw random 0, 1
  kExtra = (kPlayingGate == 1 && kOnwTick == 1 && kSinceEvt > 0.05 && kDens > 0.001 && kDensDraw < kDens ? 1 : 0)
  gkONW_dbgExtra = kExtra
  kNewEvt = 0
  if (kOnset == 1 || kExtra == 1) then
    kNewEvt = 1
    kSinceEvt = 0
    gkONW_evts = gkONW_evts + 1
  endif

  ; ---------------------------------------------------- two-buffer recorder
  kRecing init 0
  kLock   init 0
  kTgt    init 1
  kActive init 0
  kRecS   init 0.25
  kRecSA  init 0.25
  kRecSB  init 0.25
  kCapCpsA init 220
  kCapCpsB init 220

  if (kLock > 0) then
    kLock = max(kLock - kDT, 0)
  endif

  kStart = 0
  if (kOnset == 1 && kRecing == 0 && kLock == 0 && kHold == 0) then
    kStart  = 1
    kTgt    = 1 - kActive
    kRecS   = kSizeS
    kRecing = 1
    gkONW_caps = gkONW_caps + 1
    if (kTgt == 0) then
      kCapCpsA = kLiveCpsHeld
    else
      kCapCpsB = kLiveCpsHeld
    endif
  endif

  aOneR = a(kStart)
  aPrvR delay1 aOneR
  aImpR limit aOneR - aPrvR, 0, 1
  kRecFreq = 1 / max(0.02, kRecS)
  aRecPh, aRecSy syncphasor kRecFreq, aImpR
  kRecPh downsamp aRecPh

  kRecPhPrev init 0
  kRecWrap = (kRecing == 1 && kStart == 0 && kRecPh < kRecPhPrev - 0.0001 ? 1 : 0)
  kRecPhPrev = kRecPh

  kAdoptA = 0
  kAdoptB = 0
  kDone   = 0
  if (kRecWrap == 1) then
    kRecing = 0
    kDone   = 1
    if (kTgt == 0) then
      kRecSA  = kRecS
      kAdoptA = 1
    else
      kRecSB  = kRecS
      kAdoptB = 1
    endif
    kActive = kTgt
    kLock   = min(kFadeT * 0.85, 0.35)
  endif

  aRecSig delay    aMono, 0.015
  aRecSig butterlp aRecSig, 9000
  aWIdx = aRecPh * (kRecS * sr)
  kWfn  = (kRecing == 1 ? (kTgt == 0 ? giONW_bufA : giONW_bufB) : giONW_trash)
  tablewkt aRecSig, aWIdx, kWfn, 0, 0, 1

  ; ------------------------------------------------- harmonic-lock ratios
  ; Widen the consonant table as FOLLOW increases: < .34 unison+octave only,
  ; .34-.67 + fifth, > .67 + third/fourth. Blend LINEARLY from 1.0 (raw, no
  ; shift) to the snapped ratio, so FOLLOW=0 is bit-identical to "off".
  kIntervalSet = (kFollow < 0.34 ? 0 : (kFollow < 0.67 ? 1 : 2))
  kRatioA = kLiveCpsHeld / max(kCapCpsA, 20)
  kRatioB = kLiveCpsHeld / max(kCapCpsB, 20)
  kSnapA OnwSnapRatio kRatioA, kIntervalSet
  kSnapB OnwSnapRatio kRatioB, kIntervalSet
  kHarmA = 1 + (kSnapA - 1) * kFollow
  kHarmB = 1 + (kSnapB - 1) * kFollow

  kIsActiveA = (kActive == 0 ? 1 : 0)
  kIsActiveB = (kActive == 1 ? 1 : 0)

  ; -------------------------------------------------------------- engines
  aGAL,aGAR,kGAb,kGArt,kGAls,kGAmu,kGAdc OnwGlitch kSizeS,kRecSA,kHarmA,kNewEvt,kIsActiveA,kErr,kType,kOct,kAdoptA,giONW_bufA
  aGBL,aGBR,kGBb,kGBrt,kGBls,kGBmu,kGBdc OnwGlitch kSizeS,kRecSB,kHarmB,kNewEvt,kIsActiveB,kErr,kType,kOct,kAdoptB,giONW_bufB
  aFAL,aFAR,kFAb,kFArt,kFAls,kFAmu,kFAdc OnwFreeze kGrain,kRecSA,kHarmA,kNewEvt,kIsActiveA,kAdoptA,kErr,kType,kOct,giONW_bufA
  aFBL,aFBR,kFBb,kFBrt,kFBls,kFBmu,kFBdc OnwFreeze kGrain,kRecSB,kHarmB,kNewEvt,kIsActiveB,kAdoptB,kErr,kType,kOct,giONW_bufB

  ; equal-power crossfade old sample -> new sample over FADE.
  kXf init 0
  kXfStep = kDT / max(0.01, kFadeT)
  if (kXf < kActive) then
    kXf = min(kXf + kXfStep, kActive)
  elseif (kXf > kActive) then
    kXf = max(kXf - kXfStep, kActive)
  endif
  kThX = kXf * 1.5707963
  aXfA interp cos(kThX)
  aXfB interp sin(kThX)

  aGlL = aGAL * aXfA + aGBL * aXfB
  aGlR = aGAR * aXfA + aGBR * aXfB
  aFrL = aFAL * aXfA + aFBL * aXfB
  aFrR = aFAR * aXfA + aFBR * aXfB

  ; ------------------------------------------- SHAPE: attack / hold / release
  kEnvStage init 0
  kEnvV     init 0
  kHoldLeft init 0
  if (kDone == 1) then
    kEnvStage = 1
    kHoldLeft = kHoldT
  endif
  if (kEnvStage == 1) then
    kEnvV = kEnvV + kDT / max(0.005, kFadeT)
    if (kEnvV >= 1) then
      kEnvV = 1
      kEnvStage = 2
    endif
  elseif (kEnvStage == 2) then
    if (kInf == 0 && kHold == 0) then
      kHoldLeft = kHoldLeft - kDT
      if (kHoldLeft <= 0) then
        kEnvStage = 3
      endif
    endif
  elseif (kEnvStage == 3) then
    kEnvV = kEnvV - kDT / max(0.005, kFadeT)
    if (kEnvV <= 0) then
      kEnvV = 0
      kEnvStage = 0
    endif
  endif
  aShape interp 0.5 - 0.5 * cos(3.14159265 * limit(kEnvV, 0, 1))

  ; --------------------------------------- BLEND: Freeze <-> Glitch (eq power)
  kThB = kBlend * 1.5707963
  aWF interp cos(kThB)
  aWG interp sin(kThB)
  aWetL = (aFrL * giONW_frzG) * aWF + (aGlL * giONW_gltG) * aWG
  aWetR = (aFrR * giONW_frzG) * aWF + (aGlR * giONW_gltG) * aWG
  aWetL = aWetL * aShape
  aWetR = aWetR * aShape

  ; ------------------------------------------------------- TEXTURE (Effects)
  if (kTex > 0.001) then
    kDrv = 1 + 7 * kTex
    kCmp = 1 / sqrt(kDrv)
    aTL = tanh(aWetL * kDrv) * kCmp
    aTR = tanh(aWetR * kDrv) * kCmp
    aWetL = aWetL + (aTL - aWetL) * kTex
    aWetR = aWetR + (aTR - aWetR) * kTex
  elseif (kTex < -0.001) then
    kFld = 1 + 34 * (-kTex)
    kCrM = min(1, (-kTex) * 1.7)
    aSL fold aWetL, kFld
    aSR fold aWetR, kFld
    aWetL = aWetL + (aSL - aWetL) * kCrM
    aWetR = aWetR + (aSR - aWetR) * kCrM
  endif

  ; ------------------------------------------------------- ANIMATE (Effects)
  if (kAnim < -0.01) then
    kVR = limit(1 / (kSizeS * 2), 0.4, 12)
    aVL oscili 2.5 * (-kAnim), kVR, giONW_sine
    aWetL vdelay3 aWetL, a(4) + aVL, 16
    aWetR vdelay3 aWetR, a(4) + aVL, 16
  elseif (kAnim > 0.01) then
    aC1 oscili 4 * kAnim, 0.33, giONW_sine
    aC2 oscili 4 * kAnim, 0.27, giONW_sine, 0.25
    aDL vdelay3 aWetL, a(9) + aC1, 26
    aDR vdelay3 aWetR, a(9) + aC2, 26
    aWetL = aWetL * 0.62 + aDL * 0.62
    aWetR = aWetR * 0.62 + aDR * 0.62
  endif

  ; ------------------------------------------------------------------- DUCK
  aInE follow2 aMono, 0.005, 0.25
  kInE downsamp aInE
  kDkT = 1 - kDuckD * limit((dbamp(max(kInE, 0.000001)) + 45) / 30, 0, 1)
  kDkPrev init 1
  kDkG portk kDkT, (kDkT < kDkPrev ? 0.012 : 0.30)
  kDkPrev = kDkG
  aDk interp kDkG
  aWetL = aWetL * aDk
  aWetR = aWetR * aDk

  ; ----------------------------------------------------------------- output
  kMixS portk kMix, 0.02
  aMixA interp kMixS
  aWetL = aWetL * aMixA * giONW_makeup
  aWetR = aWetR * aMixA * giONW_makeup

  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL = tanh(aWetL * 1.1) * 0.90909091
  aWetR = tanh(aWetR * 1.1) * 0.90909091

  aOutL limit aInL + aWetL, -0.99, 0.99
  aOutR limit aInR + aWetR, -0.99, 0.99
  gaONW_outL = aOutL
  gaONW_outR = aOutR

#ifdef ONWDBG
  kGb  = (kActive == 0 ? kGAb  : kGBb)
  kHb  = (kActive == 0 ? kHarmA : kHarmB)
  kCap = (kActive == 0 ? kCapCpsA : kCapCpsB)
  printf "ONWEVT t=%.4f new=%d onset=%d extra=%d\n", kNewEvt, timeinsts:k(), kNewEvt, kOnset, kExtra
  printf "ONWSEG g t=%.4f bound=%d rate=%.4f harm=%.4f cap=%.2f live=%.2f\n", kGb, timeinsts:k(), kGb, kGArt, kHb, kCap, kLiveCpsHeld
  printf "ONWCAP t=%.4f n=%.0f recS=%.4f\n", kStart, timeinsts:k(), gkONW_caps, kSizeS
  kWrms rms aWetL
  printks "ONW t=%.3f wet=%.6f caps=%.0f evts=%.0f env=%.3f xf=%.3f rec=%.0f\n", 0.02, timeinsts:k(), kWrms, gkONW_caps, gkONW_evts, kEnvV, kXf, kRecing
#endif
endin

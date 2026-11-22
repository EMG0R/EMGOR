; FEEDBACKER — natural amp-feedback bloom (DigiTech FreqOut class)
; ---------------------------------------------------------------------------
; Signal chain position : 2.5 (proposed — EARLY, after H90 PITCH/2 and before
;                          PITCH FORK/3, i.e. PRE-DRIVES, so the drive menu
;                          saturates the bloom exactly like a real amp would)
; Menu / slot           : proposed AIR menu — toggle 63, instr 186, code fbk
; Reads                 : gaFBK_sendL / gaFBK_sendR
; Writes                : gaFBK_outL / gaFBK_outR  (DRY UNITY inside + voice
;                         mixed UNDER the dry; chain ntrpol mix constant 1 —
;                         same inline-blend convention as ONWARD/OCTAVER)
; Bypass                : gkFBK_bypass (FX_BYPASS)
;
; INTEGRATION NOTES (integration agent owns all shared files; every number
; below is PROPOSED — the integration pass resolves cross-batch collisions):
;   * instr 186 — verified unused by grep over csound/*.orc, csound/effects/*.orc,
;     signal_chain.orc, neptrPhase4.csd at build time (2026-07-13). PROPOSED.
;   * toggle 63 — verified unused by grep over effects/*.orc + midi_handler.orc
;     at build time. PROPOSED.
;   * neptrPhase4.csd: #include "effects/feedbacker.orc" (self-contained; the
;     gk/ga declarations below may MOVE to globals.orc verbatim, then delete
;     them here).
;   * signal_chain.orc @ stage 2.5 (after H90 PITCH, before PITCH FORK):
;       gaFBK_sendL = aPrevL
;       gaFBK_sendR = aPrevR
;       $FX_BYPASS(gkFBK_bypass'186'gaFBK_sendL'gaFBK_sendR'gaFBK_outL'gaFBK_outR'fbk)
;       aPrevL ntrpol gaFBK_sendL, gaFBK_outL, 1
;       aPrevR ntrpol gaFBK_sendR, gaFBK_outR, 1
;     (mix constant = 1: dry is unity inside the instr, the feedback voice
;     level is gkFBK_level internally.)
;   * midi_handler.orc: FX_TOGGLE gkFBK_bypass'63 on its menu slot.
;     Encoder takeover v2 dispatch on gkLastToggle==63:
;       P1 (home-slot rot)  -> gkFBK_level [0..1]   lin  .02 -> /fbk_level "Level"
;       P2 (CC20, always)   -> gkFBK_harm  [0..1]   STEPPED-INT x4 (.25/detent)
;                              -> /fbk_harm "Harmonic" (0 fund / 1 +oct /
;                              2 +oct+5th / 3 +2oct)
;       P3 (CC23, always)   -> gkFBK_bloom [0..1]   lin  .02 -> /fbk_bloom "Bloom"
;                              (maps to 0.2..4 s onset inside the DSP)
;       P4 (next slot rot)  -> gkFBK_vib   [0..1]   lin  .02 -> /fbk_vib "Vibrato"
;     CC29 expression -> gkFBK_expr (intensity push: more level + faster
;     bloom). STICKY CLAIM per spec: set gkLastToggle29 = 63 on toggle-ON.
;     gkLastToggle29 is contested across this batch — INTEGRATION ARBITRATES;
;     dropping the CC29 wire entirely is safe (gkFBK_expr defaults to 0).
;   * OSC feedback: /fbk_state, /fbk_level, /fbk_harm, /fbk_bloom, /fbk_vib.
;   * neptrPhase4_UI.py: MENUS entry ("fbk","FEEDBACKER");
;     EFFECT_PARAMS["fbk"] = [("level","Level",0,1),("harm","Harmonic",0,3),
;                             ("bloom","Bloom",0,1),("vib","Vibrato",0,1)]
;   * POSSIBLE LATER DEDUPE: pitch tracking here is a self-contained inline
;     ptrack block by design (a parallel builder owns the shared-tracker
;     territory). Once tracksynth's shared tracker lands, this block can be
;     swapped for it in a follow-up — the only contract is (kF0 Hz clamped
;     55..1200, held on loss) + a gate.
;
; WHAT IT DOES
;   Pitch-tracks the input (ptrack, 55–1200 Hz clamp, holds last good value
;   on tracking loss). While a note SUSTAINS (input-envelope gate with
;   hysteresis), a chosen overtone of the tracked fundamental — fundamental /
;   +octave / +octave+fifth / +2 octaves, stepped — swells in as a 2-partial
;   band-limited oscillator with a bloom envelope (0.2–4 s onset, kBl^2
;   curve: slow start then accelerating, like real feedback taking hold).
;   ±5-cent 5 Hz vibrato plus a slow random detune walk keep it organic.
;   The voice amplitude rides a latched note-level tracker so it swells UP
;   while the string decays (FreqOut behaviour) yet always sits UNDER the
;   dry at default level. Voice gates off fast (<300 ms) when the input
;   stops. Cheap: one ptrack + three oscillators.
; ---------------------------------------------------------------------------

gkFBK_bypass init 1
gkFBK_level  init 0.5    ; P1 — feedback voice level
gkFBK_harm   init 0      ; P2 (CC20) — stepped: 0 fund / .25 +oct / .5 +oct+5th / .75 +2oct
gkFBK_bloom  init 0.35   ; P3 (CC23) — bloom onset, 0..1 -> 0.2..4 s
gkFBK_vib    init 0.5    ; P4 — vibrato depth (0..1 -> 0..5 cents @ 5 Hz)
gkFBK_expr   init 0      ; CC29 — intensity push (level + bloom-speed)
gaFBK_sendL  init 0
gaFBK_sendR  init 0
gaFBK_outL   init 0
gaFBK_outR   init 0

instr 186  ; FEEDBACKER
  aInL = gaFBK_sendL
  aInR = gaFBK_sendR
  aMono = (aInL + aInR) * 0.5
  aTrk dcblock aMono
  kDT = ksmps/sr

  ; ---- inline pitch tracker (see dedupe note in header) ----
  ; hop 1024 -> 2048-sample window: solid down to low E, latency irrelevant
  ; for a slow bloom voice.
  kCps, kAmpDb ptrack aTrk, 1024

  ; ---- input envelope: fast gate detector + smoother amp tracker ----
  aEnvG follow2 aTrk, 0.004, 0.03     ; gate: quick release (300 ms budget)
  kEnvG downsamp aEnvG
  aEnvA follow2 aTrk, 0.010, 0.12     ; amp reference: smoother
  kEnvA downsamp aEnvA

  ; gate with hysteresis: on > 0.010, off < 0.0035
  kGate init 0
  kGate = (kEnvG > 0.010 ? 1 : (kEnvG < 0.0035 ? 0 : kGate))

  ; ---- clamp 55..1200, hold last good f0 on loss ----
  kF0 init 220
  if (kGate == 1 && kCps >= 55 && kCps <= 1200) then
    kF0 = kCps
  endif
  kF0s portk kF0, 0.04, 220

  ; ---- harmonic select (stepped): 0..1 -> x1 / x2 / x3 / x4 ----
  kSel  = int(limit:k(gkFBK_harm, 0, 0.999) * 4)
  kMult = kSel + 1

  ; ---- bloom envelope: integrate up while gated, fast decay when not ----
  kPush  limit gkFBK_expr, 0, 1
  kBloomT = (0.2 + 3.8 * limit:k(gkFBK_bloom, 0, 1)) * (1 - 0.5 * kPush)
  kBl init 0
  if (kGate == 1) then
    kBl = limit:k(kBl + kDT / kBloomT, 0, 1)
  else
    kBl = kBl * exp(-kDT / 0.05)      ; tau 50 ms -> gone well inside 300 ms
  endif
  kShape = kBl * kBl                  ; slow start, accelerating grab

  ; ---- latched note level: fast up, slow sag while gated, fast fall off ----
  kNote init 0
  if (kEnvA > kNote) then
    kNote = kNote + (kEnvA - kNote) * limit:k(kDT / 0.02, 0, 1)
  elseif (kGate == 1) then
    kNote = kNote * exp(-kDT / 2.5)   ; swells UP relative to decaying string
  else
    kNote = kNote * exp(-kDT / 0.05)
  endif

  ; ---- vibrato + slow random detune (cents) ----
  kVibC poscil 5 * limit:k(gkFBK_vib, 0, 1), 5
  kDetC jspline 3, 0.05, 0.25
  kRatio = exp((kVibC + kDetC) * 0.00057762265)   ; 2^(cents/1200)

  ; ---- voice: 2-partial band-limited osc, mixed UNDER the dry ----
  kFq   = kF0s * kMult * kRatio
  kFq   limit kFq, 40, 6000
  kLev  portk limit:k(gkFBK_level, 0, 1), 0.05
  kVAmp = kNote * kShape * kLev * (1.15 + 1.1 * kPush)
  kVAmp limit kVAmp, 0, 0.8
  aVAmp interp kVAmp
  aV1 poscil 0.82, kFq
  aV2 poscil 0.18, limit:k(kFq * 2, 80, 12000)
  aVoice = (aV1 + aV2) * aVAmp
  aVoice dcblock aVoice

#ifdef FBK_BENCH_DBG
  kTdbg timeinsts
  printks "FBK t=%.3f f0=%.1f mult=%d env=%.4f bl=%.3f gate=%d note=%.4f\n", 0.05, kTdbg, kF0s, kMult, kEnvG, kBl, kGate, kNote
#endif

  ; dry unity + voice under it (chain ntrpol mix constant = 1)
  aOutL = aInL + aVoice
  aOutR = aInR + aVoice
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaFBK_outL = aOutL
  gaFBK_outR = aOutR
endin

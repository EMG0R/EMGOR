; CHAINFB — chain feedback loop (Death By Audio TSA2-style meta-effect):
; a global feedback bus wrapping a SPAN of the signal chain
; ---------------------------------------------------------------------------
; Signal chain position : TWO tap points in instr 1 (integration inserts both):
;                         INJECT early (post-drives, ~stage 11.4, before
;                         BINCRUSH sends) and CAPTURE late (pre-reverb,
;                         ~stage 22.9, right after ONWARD). Everything in
;                         between is inside the loop.
; Reads                 : gaCFB_capL / gaCFB_capR   (written by chain CAPTURE)
; Writes                : gaCFB_retL / gaCFB_retR   (read by chain INJECT next
;                         k-cycle: the one-block loop delay is inherent and is
;                         the loop's base resonance, 48000/128 = 375 Hz comb)
; Bypass                : gkCFB_bypass (FX_BYPASS, instr 183) + a mandatory
;                         (1 - bypass) gate at the INJECT site (see notes:
;                         ga buses hold their last block after turnoff)
; Proposed IDs          : toggle 60, instr 183. NOTE: toggles 1-56 verified
;                         taken, 57-59 may be claimed by parallel builders
;                         (59 = TAPESTOP from this pack); instrs 123, 155-157,
;                         170, 171, 175, 180-182, 240 assumed taken —
;                         integration resolves collisions.
;
; INTEGRATION NOTES (integration agent owns all shared files)
;   * neptrPhase4.csd: #include "effects/chainfb.orc" (any order; fully
;     self-declared).
;   * globals.orc: the gk/ga declarations below may MOVE verbatim to
;     globals.orc (then delete them here). Safe where they are.
;   * signal_chain.orc — TWO blocks (this effect wraps the chain, it is not a
;     link in it; gaCFB_dumL/R exist only so the stock FX_BYPASS macro can be
;     used verbatim):
;     (a) INJECT, post-drives (after the OD/MUFF/BDS/preamp cluster, before
;         the first spectral send — suggested slot: right before BINCRUSH
;         stage 11.5):
;           ; 11.4 CHAINFB inject — feedback return into the loop span
;           kCFBon portk (gkCFB_bypass < 0.5 ? 1 : 0), 0.01
;           aCFBon interp kCFBon
;           aPrevL = aPrevL + gaCFB_retL * aCFBon
;           aPrevR = aPrevR + gaCFB_retR * aCFBon
;         (the gate is REQUIRED, not belt-and-braces style: ga buses hold
;         their last block after turnoff2, and the 10 ms port declicks
;         engage/disengage; the instr's own release ramp handles the rest)
;     (b) CAPTURE, pre-reverb (after ONWARD 22.8, before REVERB 23):
;           ; 22.9 CHAINFB capture — end of the wrapped span
;           gaCFB_capL = aPrevL
;           gaCFB_capR = aPrevR
;           $FX_BYPASS(gkCFB_bypass'183'gaCFB_capL'gaCFB_capR'gaCFB_dumL'gaCFB_dumR'cfb)
;         (aPrev is NOT modified at the capture point — instr 183 only
;         computes the return bus; the dummy outs are never read)
;   * midi_handler.orc: FX_TOGGLE gkCFB_bypass'60 on the chosen menu slot
;     (suggest same THEATER menu as TAPESTOP). Toggle-on defaults: ret 0.4,
;     gain 0.5, tone 0.5, ceil 0.8. No CC29 claim (pedal-free effect).
;   * Encoder map (lastToggle == 60 cases):
;       P1 / CC24-home  -> $ENC_ROT(gkCFB_ret'0.02'0'1)      ; return level
;       CC20 (P2)       -> $ENC_ROT(gkCFB_gain'0.02'0'1)     ; feedback gain
;                          (0..1 -> x0..x1.5 in-opcode; >0.67 = loop gain >1
;                          = controlled self-oscillation region)
;       CC23 (P3)       -> $ENC_ROT(gkCFB_tone'0.02'0'1)     ; loop tone tilt
;                          (0 = dark LP .. 0.5 = open .. 1 = thin HP)
;       P4 (first fill) -> $ENC_ROT(gkCFB_ceil'0.02'0.1'0.95) ; safety ceiling
;   * OSC feedback:
;       $OSC_I(gkCFB_bypass'/cfb_state'cfbByp)
;       $OSC_F(gkCFB_ret'/cfb_ret'cfbRet)
;       $OSC_F(gkCFB_gain'/cfb_gain'cfbGain)
;       $OSC_F(gkCFB_tone'/cfb_tone'cfbTone)
;       $OSC_F(gkCFB_ceil'/cfb_ceil'cfbCeil)
;   * neptrPhase4_UI.py: MENUS entry ("cfb", "CHAIN FB");
;       EFFECT_PARAMS["cfb"] = [("ret", "Return", 0, 1),
;                               ("gain", "Feedback", 0, 1),
;                               ("tone", "Tone", 0, 1),
;                               ("ceil", "Ceiling", 0.1, 0.95)]
;
; WHAT IT DOES
;   Captures the chain pre-reverb, runs it through gain (up to x1.5, past
;   unity = self-oscillation) -> tanh soft limiter -> LP/HP tone tilt ->
;   DC block -> hard safety ceiling -> return level, and re-injects the
;   result post-drives one k-block later. Every effect inside the span
;   becomes part of a playable feedback instrument. BOUNDEDNESS PROOF: the
;   last op on the return bus is limit +-kCeil with kCeil clamped to <= 0.95,
;   scaled by kRet <= 1 — the injected signal can NEVER exceed 0.95 peak, no
;   matter what the loop grows into; tanh (monotonic, |out| < 1) keeps the
;   loop interior finite, dcblock kills DC latch-up, and a PER-SAMPLE NaN/Inf
;   scrub at the loop input (NaN fails x == x) guarantees the loop filters
;   never ingest poison even if an upstream effect emits one. ret = 0 ->
;   injected signal identically 0 -> chain is bit-identical to today.
;   CPU: two butterworths + tanh per channel + a 128-step scrub loop.
; ---------------------------------------------------------------------------

; ---- self-declared globals (may move to globals.orc verbatim) ----
gkCFB_bypass init 1
gkCFB_ret    init 0.4    ; P1  return (injection) level 0..1
gkCFB_gain   init 0.5    ; CC20 feedback gain 0..1 -> x0..x1.5
gkCFB_tone   init 0.5    ; CC23 loop tone: 0 dark LP .. 0.5 open .. 1 thin HP
gkCFB_ceil   init 0.8    ; P4  hard safety ceiling (peak), 0.1..0.95
gaCFB_capL   init 0
gaCFB_capR   init 0
gaCFB_retL   init 0
gaCFB_retR   init 0
gaCFB_dumL   init 0      ; FX_BYPASS dummy outs — never read by the chain
gaCFB_dumR   init 0

instr 183   ; CHAINFB loop processor: capture -> gain -> tanh -> tone -> dcblock -> ceiling -> return
  aInL = gaCFB_capL
  aInR = gaCFB_capR

  ; ---- controls ----
  kRet  portk limit:k(gkCFB_ret, 0, 1),  0.02, i(gkCFB_ret)
  ; snap-to-zero: portk only asymptotes, and "ret = 0 -> injected signal is
  ; IDENTICALLY 0" is a promise this file makes (dry-only must be bit-exact)
  kRet  = (gkCFB_ret <= 0 && kRet < 1e-4 ? 0 : kRet)
  kGnN  portk limit:k(gkCFB_gain, 0, 1), 0.02, i(gkCFB_gain)
  kTone portk limit:k(gkCFB_tone, 0, 1), 0.05, i(gkCFB_tone)
  kCeil limit gkCFB_ceil, 0.1, 0.95
  kGain = kGnN * 1.5            ; unity loop gain crossed at knob ~0.67

  ; ---- NaN/Inf scrub, per sample: nothing non-finite (or absurd) may enter
  ; the loop filters — a butterworth fed a single NaN keeps NaN state forever.
  ; NaN fails x == x; |x| > 16 is clamped (tanh saturates it anyway). With
  ; bounded input the filters are BIBO-stable, so no NaN can arise inside. ----
  kS = 0
  while kS < ksmps do
    kV = aInL[kS]
    aInL[kS] = (kV != kV ? 0 : limit:k(kV, -16, 16))
    kV = aInR[kS]
    aInR[kS] = (kV != kV ? 0 : limit:k(kV, -16, 16))
    kS += 1
  od

  ; ---- gain into tanh soft limit (this is what makes >1 loop gain a
  ; sustained oscillation instead of a runaway) ----
  aFbL tanh aInL * kGain
  aFbR tanh aInR * kGain

  ; ---- loop tone: LP/HP tilt. tone 0 -> LP slams to 450 Hz (dark bloom);
  ; 0.5 -> wide open; 1 -> HP up to 1.5 kHz (glassy screech) ----
  kLPf = 450 * pow(40, limit:k(kTone * 2, 0, 1))        ; 450 Hz .. 18 kHz
  kHPf = 25 * pow(60, limit:k(kTone * 2 - 1, 0, 1))     ; 25 Hz .. 1.5 kHz
  aFbL butterlp aFbL, kLPf
  aFbR butterlp aFbR, kLPf
  aFbL butterhp aFbL, kHPf
  aFbR butterhp aFbR, kHPf

  ; ---- DC block (IIR dcblock, NOT dcblock2 — house rule: dcblock2 adds a
  ; real 254-sample FIR delay) ----
  aFbL dcblock aFbL
  aFbR dcblock aFbR

  ; ---- brickwall safety ceiling: THE boundedness guarantee. limit is a
  ; true hard clip; nothing runs after it except the <=1 return scaler and
  ; the release ramp ----
  aFbL limit aFbL, -kCeil, kCeil
  aFbR limit aFbR, -kCeil, kCeil

  ; ---- return level + 30 ms release ramp so the bus dies to ~0 before
  ; turnoff (ga buses hold their last block; inject-site gate finishes it) ----
  xtratim 0.05
  aRel linsegr 1, 0.03, 0
  aRet interp kRet
  gaCFB_retL = aFbL * aRet * aRel
  gaCFB_retR = aFbR * aRet * aRel

#ifdef CFBDBG
  kRms rms gaCFB_retL
  printks "CFB t=%.2f ret_rms=%.5f gain=%.2f lp=%.0f hp=%.0f ok=%d\n", 0.25, timeinsts:k(), kRms, kGain, kLPf, kHPf, kOK
#endif
endin

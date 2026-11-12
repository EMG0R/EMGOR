;==============================================================
; OCTAVER — Boss OC-2-style analog-voiced sub-octave (instr 170)
; Time-domain, monophonic-by-design, zero latency, no FFT: Schmitt-triggered
; flip-flop halves the tracked fundamental (÷2, plus a quiet ÷4 voice), the
; sub is low-passed to taste and its amplitude is slaved to the dry envelope
; so it dies exactly with the string. Dry always passes at unity — the slot
; rotation is SUB LEVEL, not a dry/wet.
; Tracks to low B (31 Hz); above ~400 Hz it glitches like the real pedal.
;
; INTEGRATION NOTES (integration agent: apply these, then delete this block's
; "pending" tag; numbers are proposed — resolve cross-batch collisions):
;   instr 170 (verified unused in existing csd/midi_handler; proposed, integration
;     agent resolves cross-batch collisions), toggle 54 (proposed).
;   Menu: new BASS menu (also guitar-useful). OSC prefix "oct".
;   globals used (self-declared below — move to globals.orc if house style
;     prefers): gkOCT_bypass, gkOCT_sub, gkOCT_tone, gkOCT_char,
;     gaOCTSendL/R, gaOCTOutL/R.
;   signal_chain.orc: early-ish, BEFORE drive section (sub into fuzz = classic
;     bass move), suggested stage 0.9 right after BASS DIST 162:
;     gaOCTSendL = aPrev_L ; gaOCTSendR = aPrev_R
;     $FX_BYPASS(gkOCT_bypass'170'gaOCTSendL'gaOCTSendR'gaOCTOutL'gaOCTOutR'oct)
;     aPrev_L = gaOCTOutL ; aPrev_R = gaOCTOutR
;     (no ntrpol mix stage: dry is unity inside; rotation maps to gkOCT_sub)
;   midi_handler.orc dispatch: slot rotation -> gkOCT_sub (step like a drywet,
;     UI label "SUB"); CC20 secondary -> gkOCT_tone; CC23 tertiary -> gkOCT_char;
;     no CC29 claim. OSC sends: /oct_state, /oct_sub, /oct_tone, /oct_char.
;   neptrPhase4_UI.py EFFECT_PARAMS "oct": [("sub","SUB",0,1),
;     ("tone","TONE",0,1),("char","CHAR",0,1)]  label "OCTAVER".
;==============================================================

gkOCT_bypass init 1
gkOCT_sub    init 0.6
gkOCT_tone   init 0.5
gkOCT_char   init 0.3
gaOCTSendL   init 0
gaOCTSendR   init 0
gaOCTOutL    init 0
gaOCTOutR    init 0

; Schmitt-trigger flip-flop divider, per-sample. Hysteresis threshold rides
; the envelope so string decay never chatters the comparator. Outputs ±1
; squares at f/2 and f/4.
opcode OC2Divider, aa, aa
  ain, aenv xin
  setksmps 1
  kIn   downsamp ain
  kEnv  downsamp aenv
  kSt   init 0
  kFF   init 1
  kFF2  init 1
  kCnt  init 0
  kThr  = 0.30 * kEnv + 0.0004
  if (kSt == 0) && (kIn > kThr) then
    kSt = 1
    kFF = -kFF
    kCnt += 1
    if (kCnt >= 2) then
      kCnt = 0
      kFF2 = -kFF2
    endif
  elseif (kSt == 1) && (kIn < -kThr) then
    kSt = 0
  endif
  aFF  upsamp kFF
  aFF2 upsamp kFF2
  xout aFF, aFF2
endop

instr 170  ; OCTAVER
  aInL = gaOCTSendL
  aInR = gaOCTSendR
  aMono = (aInL + aInR) * 0.5
  aMono dcblock aMono, 0.9995

  ; fundamental isolation for the comparator: 4-pole LP at 240 Hz keeps
  ; harmonic ripple from double-triggering the Schmitt stage; tracks bass to
  ; low B, glitches above ~250 Hz like an OC-2 (feature, not bug)
  aTrk butterlp aMono, 240
  aTrk butterlp aTrk, 240

  ; envelope: fast attack, string-length release; also the divider gate
  aEnv follow2 aMono, 0.004, 0.09
  ; comparator hysteresis rides the TRACKED signal's own envelope
  aEnvT follow2 aTrk, 0.005, 0.12

  aFF, aFF2 OC2Divider aTrk, aEnvT

  kSubLv portk gkOCT_sub,  0.02
  kTone  portk gkOCT_tone, 0.05
  kChar  portk gkOCT_char, 0.05

  ; sub voices: square * envelope (amplitude slaved to the string)
  aSq  = aFF  * aEnv
  aSq2 = aFF2 * aEnv

  ; tone: 4-pole LP = round OC-2 sine-ish; char blends toward a single-pole
  ; brighter, synthier square
  kCut = 90 + kTone * 610
  aClean butterlp aSq, kCut
  aClean butterlp aClean, kCut
  aGrit  butterlp aSq, limit:k(kCut * 3.2, 140, 2600)
  aSubV  ntrpol aClean, aGrit, kChar

  ; quiet -2 oct voice (free with the second flip-flop), always round
  aSub2 butterlp aSq2, 140
  aSub2 butterlp aSub2, 140

  aSubMix = (aSubV + aSub2 * 0.25) * kSubLv * 1.15
  aSubMix dcblock aSubMix, 0.9995

  ; dry at unity, always
  gaOCTOutL = aInL + aSubMix
  gaOCTOutR = aInR + aSubMix
endin

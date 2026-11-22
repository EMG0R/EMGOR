;==============================================================
; VOCAL CHAIN — always-on strip for the mic pair (inputs 5,6)
; Rebuilt 2026-10-22 from EMGOR's spec (PRIME_V5_SWAP 8.1, his window
; 2026-10-21). Rebuild from description: the pre-crash chain is gone.
;
;   mic in 5,6 -> EQ -> slow delay -> reverbsc -> VOC LEVEL -> out 5,6
;
; Deliberately NOT here: tube preamp, vocoder front-end, multiband VoxEq,
; AGC / any added input gain (EMGOR: "it made the vocoder far too loud").
; Unity in, unity level: nothing in this file adds gain except the user's
; own voc_level and the two wet sends.
;
; ALWAYS ON = no instrument to start, no preset recall: VocalChain is a UDO
; called inline from instr 1 (signal_chain.orc, MIXER builds only, i.e. the
; 8ch mode; the 2ch build has no inputs 5,6). It is added to the direct bus
; gaMxDir[4]/[5] (mixer.orc) so it lands on outputs 5,6 through MixOut and
; its output-stage gain/mute. voc_on only crossfades processed <-> dry mic;
; the filters/delay/reverb keep running so toggling is click-free.
;
; CONTROLS — chn_k control channels (read every k-cycle with chnget, set from
; any layer with chnset). Defaults are the spec values; ranges clamp inside.
;   voc_on        0/1       1      processed (1) vs dry mic (0)
;   voc_hpf_hz    40..300   80     high-pass, 12 dB/oct (butterhp, 2-pole)
;   voc_mid_db    -12..+12  -4     peaking cut/boost gain
;   voc_mid_hz    300..5000 1000   peaking centre
;   voc_mid_q     0.3..4    1.2    peaking Q
;   voc_dly_ms    50..1500  500    delay time (free-running, NOT tempo-synced)
;   voc_dly_fb    0..0.9    0.30   delay feedback
; voc_dly_wet   0..1      0.5    delay send level (dry stays at unity)
;   voc_rev_fb    0..0.94   0.85   reverbsc feedback (clamped < 0.95)
;   voc_rev_cut   1000..16000 10000 reverbsc lowpass Hz
; voc_rev_wet   0..1      0  (reverb OFF by default, 2026-11-01)   reverb return level (dry stays at unity)
; voc_pre_db    0..40     15     mic PREAMP gain in dB, first stage (before HPF, also in DRY); no hardware preamp on in 5,6 (2026-10-24)
;   voc_level     0..4      1.0    strip output level (1.0 = 7x / +17 dB; 4.0 = +29 dB, 2026-10-24)
; Ranges above are the CHANNEL limits; the VOCAL menu (menu 21) uses the
; narrower performance ranges in docs/vocal_chain.md. Persisted: state_persist
; slots 319..330 + 342 voc_pre_db (stored +100). See docs/vocal_chain.md.
;==============================================================

chn_k "voc_on",      3, 2, 1,    0,    1
chn_k "voc_hpf_hz",  3, 2, 80,   40,   300
chn_k "voc_mid_db",  3, 2, -4,   -12,  12
chn_k "voc_mid_hz",  3, 2, 1000, 300,  5000
chn_k "voc_mid_q",   3, 2, 1.2,  0.3,  4
chn_k "voc_dly_ms",  3, 2, 500,  50,   1500
chn_k "voc_dly_fb",  3, 2, 0.30, 0,    0.9
chn_k "voc_dly_wet", 3, 2, 0.5,  0,    1
chn_k "voc_rev_fb",  3, 2, 0.85, 0,    0.94
chn_k "voc_rev_cut", 3, 2, 10000, 1000, 16000
chn_k "voc_rev_wet", 3, 2, 0,    0,    1
chn_k "voc_level",   3, 2, 1,    0,    4
chn_k "voc_pre_db",  3, 2, 15,   0,    40
; VOCAL GATE (2026-11-02, EMGOR): the on/off for performance = a pre-gain x0 / x1 at
; the very front, before the preamp. Not voc_on (that is the FX vs DRY switch).
; DUODECIMUS 7 press, ch16 CC55, web VOCAL toggle and preset VOCAL steps drive it.
chn_k "voc_gate",    3, 2, 1,    0,    1
; chn_k's default is only a hint for hosts: the channel itself starts at 0.
; Set every default here, at load (state restore overrides it later).
chnset 1, "voc_on"
chnset 80, "voc_hpf_hz"
chnset -4, "voc_mid_db"
chnset 1000, "voc_mid_hz"
chnset 1.2, "voc_mid_q"
chnset 500, "voc_dly_ms"
chnset 0.30, "voc_dly_fb"
chnset 0.5, "voc_dly_wet"
chnset 0.85, "voc_rev_fb"
chnset 10000, "voc_rev_cut"
chnset 0, "voc_rev_wet"
chnset 1, "voc_level"
chnset 15, "voc_pre_db"
chnset 1, "voc_gate"

opcode VocalChain, aa, aa
  aInL, aInR xin

  kOn     chnget "voc_on"
  kHpf    chnget "voc_hpf_hz"
  kMidDb  chnget "voc_mid_db"
  kMidHz  chnget "voc_mid_hz"
  kMidQ   chnget "voc_mid_q"
  kDlyMs  chnget "voc_dly_ms"
  kDlyFb  chnget "voc_dly_fb"
  kDlyWet chnget "voc_dly_wet"
  kRevFb  chnget "voc_rev_fb"
  kRevCut chnget "voc_rev_cut"
  kRevWet chnget "voc_rev_wet"
  kLevel  chnget "voc_level"
  kPreDb  chnget "voc_pre_db"
  kGate   chnget "voc_gate"

  ; clamp (a bad write must never run the feedback networks away)
  kHpf    limit kHpf, 40, 300
  kMidDb  limit kMidDb, -12, 12
  kMidHz  limit kMidHz, 300, 5000
  kMidQ   limit kMidQ, 0.3, 4
  kDlyMs  limit kDlyMs, 50, 1500
  kDlyFb  limit kDlyFb, 0, 0.9
  kDlyWet limit kDlyWet, 0, 1
  kRevFb  limit kRevFb, 0, 0.94
  kRevCut limit kRevCut, 1000, 16000
  kRevWet limit kRevWet, 0, 1
  kLevel  limit kLevel, 0, 4
  kPreDb  limit kPreDb, 0, 40

  kHpfS   portk kHpf, 0.03
  kMidHzS portk kMidHz, 0.03
  kMidQS  portk kMidQ, 0.03
  kMidGn  portk ampdb(kMidDb), 0.03
  kDlyS   portk kDlyMs / 1000, 0.15       ; slow glide: time changes bend, never click
  kDlyFbS portk kDlyFb, 0.03
  kDlyWtS portk kDlyWet, 0.03
  kRevWtS portk kRevWet, 0.03
  ; 2026-10-24 (Emory): level 1.0 now = 7x the old unity (+17 dB makeup; 2.5x, 4x not enough).
  kLvS    portk kLevel * 7, 0.03
  kOnS    portk (kOn >= 0.5 ? 1 : 0), 0.03
  kPreS   portk ampdb(kPreDb) * (kGate >= 0.5 ? 1 : 0), 0.01   ; gate x0/x1, 10 ms ramp (no click)

  ; ---- mic PREAMP: very first stage, so the dry (voc_on = 0) path gets it too ----
  aInL = aInL * kPreS
  aInR = aInR * kPreS

  ; ---- EQ: HPF 12 dB/oct, then one peaking band ----
  aHL butterhp aInL, kHpfS
  aHR butterhp aInR, kHpfS
  aEqL rbjeq aHL, kMidHzS, kMidGn, kMidQS, 1, 8
  aEqR rbjeq aHR, kMidHzS, kMidGn, kMidQS, 1, 8

  ; ---- slow delay (dry at unity + wet send; own feedback loop per side) ----
  aDlyL delayr 2
  aTapL deltapi kDlyS
        delayw aEqL + aTapL * kDlyFbS
  aDlyR delayr 2
  aTapR deltapi kDlyS
        delayw aEqR + aTapR * kDlyFbS
  aDL = aEqL + aTapL * kDlyWtS
  aDR = aEqR + aTapR * kDlyWtS

  ; ---- reverb: reverbsc (Sean Costello), stereo in/out ----
  aRvL, aRvR reverbsc aDL, aDR, kRevFb, kRevCut
  aPL = aDL + aRvL * kRevWtS
  aPR = aDR + aRvR * kRevWtS

  ; on = processed, off = dry mic
  aOutL = (aInL + (aPL - aInL) * kOnS) * kLvS
  aOutR = (aInR + (aPR - aInR) * kOnS) * kLvS
  xout aOutL, aOutR
endop

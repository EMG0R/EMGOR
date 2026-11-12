;==============================================================
; TONE EQ — amp-style tone block (instr 181)
; DYNAMICS pack, 2026-07-13 batch. Three rbjeq biquads per channel:
;   low shelf  ±12 dB @ 100 Hz   (rbjeq imode 10)
;   mid peak   ±12 dB, sweepable 200 Hz .. 3 kHz, Q 0.8 (imode 8)
;   high shelf ±12 dB @ 3 kHz    (imode 12)
; plus a TILT axis: one knob tips the whole spectrum (low shelf down while
; high shelf up, and vice versa), summed into the two shelf gains and
; clamped ±15 dB. Post-drive placement = the "amp tone stack" spot.
; Zero latency, cheap inline: 6 biquads total. When every band computes to
; 0 dB the instr hard-bypasses (output = send, bit-transparent null) — the
; filters keep running so re-engage is click-free.
; rbjeq mode/gain law verified by impulse probe 2026-07-13: klvl is the
; linear FULL shelf / peak-center gain, shelves read half-gain (+6 of +12)
; at fc, peaking reads klvl exactly at center.
;
; INTEGRATION NOTES (integration agent: apply these; numbers are proposed —
; resolve cross-batch collisions):
;   instr 181 (verified unused in existing csd/midi_handler/effects; proposed,
;     integration agent resolves cross-batch collisions), toggle 58 (proposed).
;   Menu: integration agent picks the home (suggest DRIVE-adjacent or a
;     DYNAMICS/UTILITY slot). OSC prefix "teq".
;   csd: #include "effects/toneeq.orc" with the other effects (include order
;     globals -> effects -> chain makes the self-declared globals safe).
;   globals used (self-declared below): gkTEQ_bypass, gkTEQ_mid, gkTEQ_freq,
;     gkTEQ_tilt, gkTEQ_low, gkTEQ_high, gaTEQSendL/R, gaTEQOutL/R.
;   signal_chain.orc: stage 11.5 — after the DRIVE menu block (11/130-134),
;     before LOSSY (11.7/123) / FORMANT (13): post-drives, amp-like. No mix
;     stage (an EQ blend is a comb-filter foot-gun); wire straight through:
;     gaTEQSendL = aPrevL
;     gaTEQSendR = aPrevR
;     $FX_BYPASS(gkTEQ_bypass'181'gaTEQSendL'gaTEQSendR'gaTEQOutL'gaTEQOutR'teq)
;     aPrevL = gaTEQOutL
;     aPrevR = gaTEQOutR
;   midi_handler.orc dispatch (encoder takeover v2): toggle via $FX_TOGGLE
;     (gkTEQ_bypass'58). P1 (home-slot rotation) -> gkTEQ_mid [0..1] lin .02
;     (0.5 = flat); CC20 (P2) -> gkTEQ_freq [0..1] lin .02 (200 Hz..3 kHz
;     exp); CC23 (P3) -> gkTEQ_tilt [0..1] lin .02 (0.5 = flat); P4/P5
;     (remaining slot rotations, ascending) -> gkTEQ_low, gkTEQ_high [0..1]
;     lin .02 (0.5 = flat). NO CC29 claim.
;     OSC sends: /teq_state, /teq_mid, /teq_freq, /teq_tilt, /teq_low,
;     /teq_high.
;   neptrPhase4_UI.py EFFECT_PARAMS:
;     "teq": [("mid", "MID", 0, 1), ("freq", "FREQ", 0, 1),
;             ("tilt", "TILT", 0, 1), ("low", "LOW", 0, 1),
;             ("high", "HIGH", 0, 1)]   label "TONE EQ".
;==============================================================

gkTEQ_bypass init 1
gkTEQ_mid    init 0.5    ; 0..1 -> -12..+12 dB mid peak (0.5 = flat)
gkTEQ_freq   init 0.5    ; 0..1 -> 200 Hz..3 kHz exponential (0.5 ~ 775 Hz)
gkTEQ_tilt   init 0.5    ; 0..1 -> low+12/high-12 .. low-12/high+12
gkTEQ_low    init 0.5    ; 0..1 -> -12..+12 dB low shelf @ 100 Hz
gkTEQ_high   init 0.5    ; 0..1 -> -12..+12 dB high shelf @ 3 kHz
gaTEQSendL   init 0
gaTEQSendR   init 0
gaTEQOutL    init 0
gaTEQOutR    init 0

instr 181  ; TONE EQ
  aInL = gaTEQSendL
  aInR = gaTEQSendR

  kMid  portk gkTEQ_mid,  0.03, i(gkTEQ_mid)
  kFrq  portk gkTEQ_freq, 0.03, i(gkTEQ_freq)
  kTilt portk gkTEQ_tilt, 0.03, i(gkTEQ_tilt)
  kLow  portk gkTEQ_low,  0.03, i(gkTEQ_low)
  kHigh portk gkTEQ_high, 0.03, i(gkTEQ_high)

  kMidDb  = (kMid - 0.5) * 24
  kMidF   = 200 * exp(log(15) * kFrq)              ; 200 -> 3000 Hz
  kTiltDb = (kTilt - 0.5) * 24
  kLoDb   = limit:k((kLow  - 0.5) * 24 - kTiltDb, -15, 15)
  kHiDb   = limit:k((kHigh - 0.5) * 24 + kTiltDb, -15, 15)

  ; filters always run (click-free re-engage after a flat stretch)
  aL rbjeq aInL, 100,   ampdb(kLoDb),  0.7071, 1, 10   ; low shelf
  aL rbjeq aL,   kMidF, ampdb(kMidDb), 0.8,    1, 8    ; mid peak
  aL rbjeq aL,   3000,  ampdb(kHiDb),  0.7071, 1, 12   ; high shelf
  aR rbjeq aInR, 100,   ampdb(kLoDb),  0.7071, 1, 10
  aR rbjeq aR,   kMidF, ampdb(kMidDb), 0.8,    1, 8
  aR rbjeq aR,   3000,  ampdb(kHiDb),  0.7071, 1, 12

  ; flat = bit-transparent: all three bands at 0 dB -> pass the send through
  if (abs(kLoDb) < 0.005 && abs(kMidDb) < 0.005 && abs(kHiDb) < 0.005) then
    gaTEQOutL = aInL
    gaTEQOutR = aInR
  else
    gaTEQOutL = limit(aL, -0.99, 0.99)
    gaTEQOutR = limit(aR, -0.99, 0.99)
  endif
endin

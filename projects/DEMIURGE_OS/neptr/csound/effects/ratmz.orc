; RAT × MZ — ProCo RAT hard-clip core wearing a Boss MT-2 sweepable-mid EQ
; Signal chain position: 133
; Reads:  gaRMZ_sendL/R
; Writes: gaRMZ_outL/R
; Bypass: chain-managed — FX_BYPASS launches this instance on toggle-on
;         (event "i", 133, 0, -1) and kills it on toggle-off; no internal
;         bypass branch needed.
;
; CIRCUIT (ElectroSmash RAT analysis + Electric Druid MT-2 analysis):
;   in → 1-pole HP ~60 Hz (RAT input coupling)
;      → pre-gain mid/treble emphasis (RAT 47R/560R feedback-leg network:
;        response rises ~+6 dB into the clipper above ~500 Hz — modeled as
;        a 1-pole high shelf: x + atone(x, 500))
;      → variable-gain op-amp stage (LM308). The LM308's 0.3 V/us slew
;        rate IS the RAT's compression/feel: transients round off BEFORE
;        the diodes. Modeled as a 1-pole LP inside the gain path whose
;        corner drops as drive rises (8 kHz @ drive=0 → 2.5 kHz @ drive=1,
;        approximating the gain-bandwidth + slew interaction).
;      → SYMMETRIC HARD CLIP: two 1N914 silicon diodes to ground, ±0.6 V
;        (OS4Hard, 4x oversampled, true hard clip)
;      → RAT "Filter": passive 1-pole LP (the real knob works backwards —
;        clockwise = darker — ours maps normally: 0 = dark, 1 = bright)
;      → MT-2 layer: the Metal Zone's signature sweepable mid — peaking
;        EQ (pareq mode 0), Q ≈ 2.5, freq/gain on the two wildcards
;      → makeup → dcblock2
;   Pedal-voiced (stacks into a real amp): NO cab sim, NO power amp.
;
; CONTROLS (all normalized 0..1):
;   gkRMZ_drive  op-amp gain into the clipper, log 2x..200x
;                (also pulls the slew-limit corner down 8k → 2.5k)
;   gkRMZ_tone   RAT Filter LP cutoff, log 600 Hz..8 kHz (0 = darkest)
;   gkRMZ_wc1    MZ mid-sweep frequency, log 200 Hz..5 kHz
;   gkRMZ_wc2    MZ mid gain: 0 = -12 dB cut, 0.5 = flat, 1 = +12 dB boost
;
; MAKEUP (open-loop, pure function of the knobs — NO rms/env followers,
; stable across input levels, sustain decays naturally):
;   Hard clip at ±0.6 bounds the wet peak; as drive rises the signal
;   saturates toward a constant-level square wave, so makeup decays
;   exponentially from the linear-region value to the saturated floor:
;     kMakeup = 0.175 + 0.36 * exp(-4.6 * drive)
;       drive=0.0: 0.535  (gain 2x, barely clipping — near unity)
;       drive=0.5: 0.211  (gain 20x, default — squared up)
;       drive=1.0: 0.179  (gain 200x, full square)
;   Bench-calibrated: 0.1-RMS 196 Hz pluck → -0.5 dB vs dry at default
;   settings, +0.3..-0.9 dB across the full drive sweep.
;   Mid gain (wc2) compensation: a Q-2.5 peak only moves broadband RMS
;   by a FRACTION of its peak dB (and only when the sweep freq sits on
;   the signal's energy), so we fold in a quarter of the peak dB —
;   roughly half of the typical RMS effect. A +12 dB mid boost parked
;   on your note still nets several dB louder (musically it SHOULD);
;   a cut that misses the signal only drifts ~+1.5 dB.
;     kMakeup *= ampdb(-kMidDb * 0.25)

; INLINE ZERO-LATENCY (2026-06-11): opcode called directly in instr 1.
opcode RMZPedal, aa, aa
    aInL, aInR xin

    ; --- control mappings (clamp first: CC math upstream can overshoot) ---
    kDrv  limit gkRMZ_drive, 0, 1
    kTone limit gkRMZ_tone,  0, 1
    kWc1  limit gkRMZ_wc1,   0, 1
    kWc2  limit gkRMZ_wc2,   0, 1

    ; max-drive extension 2026-06-11: top-only, identical below d=0.7,
    ; doubles the ceiling (2x..400x) — "rat sounds so good", only MORE at max
    kXt     limit (kDrv - 0.7) / 0.3, 0, 1
    kGain   = 2 * exp(4.60517 * kDrv) * (1 + kXt * kXt)
    kSlewHz = 8000 * exp(-1.16315 * kDrv)        ; 8 kHz .. 2.5 kHz
    kToneHz = 600 * exp(2.59027 * kTone)         ; 600 Hz .. 8 kHz, log
    kMidHz  = 200 * exp(3.21888 * kWc1)          ; 200 Hz .. 5 kHz, log
    kMidDb  = (kWc2 - 0.5) * 24                  ; -12 .. +12 dB
    kMidAmp = ampdb(kMidDb)

    kMakeup = (0.175 + 0.36 * exp(-4.6 * kDrv)) * ampdb(-kMidDb * 0.25)
    kMakeup = kMakeup * ampdb(+0.8919 + kDrv*(-18.1270 + kDrv*(-17.2176 + kDrv*(+30.5694 + kDrv*(-12.4276)))))  ; gain-stage fit 2026-06-11 v6 REAL (in-RMS 0.009): +0.83 dB vs dry
    kMakeup = kMakeup * ampdb(12.0 * kDrv + 1.0)  ; drive bias + slot offset: RAT +1.0 dB

    ; --- LEFT ---
    aL = atone(aInL, 60)                  ; input HP ~60 Hz
    aL = aL + atone(aL, 500)              ; pre-emphasis: +6 dB shelf >500 Hz
    aL = tone(aL, kSlewHz)                ; LM308 slew/GBW limit (in gain path)
    aL OS4Hard aL, kGain, 0.6, 0.6        ; 1N914 pair to ground, hard ±0.6
    aL = tone(aL, kToneHz)                ; RAT Filter (passive 1-pole LP)
    aL pareq aL, kMidHz, kMidAmp, 2.5, 0  ; MT-2 sweepable mid, Q 2.5
    aL = aL * kMakeup
    aL dcblock2 aL

    ; --- RIGHT (identical, independent state) ---
    aR = atone(aInR, 60)
    aR = aR + atone(aR, 500)
    aR = tone(aR, kSlewHz)
    aR OS4Hard aR, kGain, 0.6, 0.6
    aR = tone(aR, kToneHz)
    aR pareq aR, kMidHz, kMidAmp, 2.5, 0
    aR = aR * kMakeup
    aR dcblock2 aR

    xout aL, aR
endop

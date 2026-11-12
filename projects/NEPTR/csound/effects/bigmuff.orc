; Big Muff — 4-stage Big Muff Pi fuzz, Triangle / Ram's Head / Fallout
; Signal chain position: 134
; Reads:  gaMUF_sendL/R
; Writes: gaMUF_outL/R
; Bypass: N/A (chain launches/kills the instance; no bypass logic here)
;
; CIRCUIT MODEL — EHX Big Muff Pi, PEDAL-VOICED (stacks into a real
; amp): no cabinet sim, no power amp. Per ElectroSmash / Coda Effects /
; bigmuffpage.com, four cascaded common-emitter transistor stages:
;
;   INPUT BOOSTER   input HP cap (1st-order, a key voicing difference:
;                   Triangle loose, Fallout tight) -> Sustain-pot gain
;                   that sets how hard the clippers are hit.
;   CLIP STAGE 1    series 1uF cap + R band-limits what gets clipped
;                   (1st-order HP ~100..250 Hz per version) -> ~25 dB
;                   stage gain into a silicon diode pair in FEEDBACK
;                   (soft clip clamping ~ +/-0.6 V) -> OS4Diode with
;                   vp/vn = 0.6/0.65 (slight asymmetry of real pairs).
;   CLIP STAGE 2    identical topology, slightly lower HP corner;
;                   thresholds mirrored (each CE stage inverts). Two
;                   consecutive soft clips -> near-square at high
;                   sustain: the wall-of-fuzz. dcblock2 after each.
;   PASSIVE TONE    parallel 1st-order LP path (~300 Hz corner) and
;                   1st-order HP path (~1.5..2.4 kHz corner) blended by
;                   the tone pot. The corner GAP plus the ~180 deg
;                   phase split between the paths carves the famous
;                   mid scoop (~1 kHz, deepest at center, ~ -13 dB).
;   OUTPUT BOOSTER  open-loop makeup recovering the tone-network loss.
;
;   + MID RECOVERY (the popular "mids" mod): post-tone peaking boost
;     @750 Hz, Q 1.5, 0..+8 dB, un-scoops toward mid-forward.
;
; CONTROLS (all normalized 0..1):
;   gkMUF_drive  SUSTAIN (expression pedal): input booster gain
;                1x..81x (1 + 80*d^2). A Muff fuzzes even at 0 — the
;                pot only sets how long the wall sustains.
;   gkMUF_tone   passive tone blend: 0 = dark (LP path), 1 = bright
;                (HP path), 0.5 = maximum mid scoop ~1 kHz.
;   gkMUF_wc1    MID RECOVERY: 0 = stock scoop, 1 = mid-forward
;                (+8 dB peaking @750 Hz).
;   gkMUF_wc2    VOICING morph, continuous component-level interp
;                across three anchors:
;
;     wc2=0.0  TRIANGLE   in-HP 55 Hz, clip HPs 110/85 Hz, gain x1.00,
;                         tone LP 300 / HP 2400 Hz (widest gap =
;                         deepest scoop, loosest bass), no presence.
;     wc2=0.5  RAM'S HEAD in-HP 75 Hz, clip HPs 150/120 Hz, gain x0.85
;                         (a touch less gain), tone LP 260 / HP 1500 Hz
;                         (narrower gap = smoother mids, darker top).
;     wc2=1.0  FALLOUT    (ThorpyFX Fallout Cloud — refined Triangle
;                         silicon Muff) in-HP 110 Hz, clip HPs
;                         240/180 Hz (tight, controlled lows), gain
;                         x1.12 (a hair hotter), tone LP 340 / HP
;                         2400 Hz (Triangle scoop kept) + 2.5 dB
;                         presence shelf @3.2 kHz.
;
; MAKEUP CALIBRATION (open-loop — pure function of drive, NO rms or
; envelope followers; see effects/tubescreamer.orc for the rationale:
; followers pump and eat sustain). Bench: 0.1-RMS 196 Hz decaying
; pluck into the send, defaults elsewhere, measured pre-makeup output
; RMS at drive 0/.25/.5/.75/1, fitted gain in dB quadratic in drive:
;
;     makeup_dB(d) = -3.6 + 1.7*d - 1.1*d^2
;
;   drive=0.00: -3.6 dB    drive=0.55: -3.0 dB    drive=1.00: -3.0 dB
;
; The 18x stage gains already saturate the clippers at unity boost, so
; 38 dB more pre-gain barely moves post-clip RMS — the bench curve is
; almost FLAT (and tips slightly DOWN with drive: the squarer wave
; shifts energy into harmonics the scoop network then removes). Bench
; result with this fit: dry-to-wet within +/-0.1 dB across the whole
; drive sweep at default tone/voicing. Sustain: a decaying note holds
; level far longer than dry — that IS the pedal, not a level error.
; (Tone=0 sits ~+6 dB — the dark LP path passes the fat fundamental,
; as on the real pedal; tone=1 ~-0.5 dB; voicing anchors within
; +/-1.4 dB of each other.)

; INLINE ZERO-LATENCY (2026-06-11): opcode called directly in instr 1.
opcode MUFPedal, aa, aa
    aInL, aInR xin

    ; --- control conditioning (pedal/encoder data arrives stepped) ---
    kDrive portk gkMUF_drive, 0.02
    kTone  portk gkMUF_tone,  0.05
    kMid   portk gkMUF_wc1,   0.05
    kVoice portk gkMUF_wc2,   0.05
    kDrive limit kDrive, 0, 1
    kTone  limit kTone,  0, 1
    kMid   limit kMid,   0, 1
    kVoice limit kVoice, 0, 1

    ; --- voicing morph: piecewise-linear through the three anchors ---
    ; kf1 sweeps Triangle->Ram's Head (wc2 0..0.5), kf2 Ram's->Fallout
    ; (wc2 0.5..1):  p = pT + (pR-pT)*kf1 + (pF-pR)*kf2
    kf1   limit kVoice * 2,     0, 1
    kf2   limit kVoice * 2 - 1, 0, 1
    kHPin =  55 + ( 75 -  55)*kf1 + (110 -  75)*kf2  ; input cap HP
    kHP1  = 110 + (150 - 110)*kf1 + (240 - 150)*kf2  ; pre-clip-1 HP
    kHP2  =  85 + (120 -  85)*kf1 + (180 - 120)*kf2  ; pre-clip-2 HP
    kVG   = 1.0 + (0.85 - 1.0)*kf1 + (1.12 - 0.85)*kf2 ; gain ratio
    kTLP  = 300 + (260 - 300)*kf1 + (340 - 260)*kf2  ; tone LP corner
    ; de-treble 2026-06-11 (user: way too much treble): HP-path anchors
    ; lowered from 2400/1500/2400
    kTHP  = 1600 + (1200 - 1600)*kf1 + (1600 - 1200)*kf2 ; tone HP corner
    kPrs  = ampdb(1.0 * kf2)                ; Fallout presence (trimmed)

    ; --- mappings ---
    ; hiss fix 2026-06-11: ceiling 80x -> 50x (4 dB less noise-floor gain;
    ; the clippers still saturate fully — sustain unchanged in practice)
    kBoost  = (1 + 50 * kDrive * kDrive) * kVG  ; Sustain: 1x..51x
    kMidAmp = ampdb(8 * kMid)                   ; mid recovery 0..+8 dB
    kMakeup = ampdb(-3.6 + 1.7*kDrive - 1.1*kDrive*kDrive)
    kMakeup = kMakeup * ampdb(-14.6004 + kDrive*(-26.0632 + kDrive*(+48.7874 + kDrive*(-41.8014 + kDrive*(+13.6657)))))  ; gain-stage fit 2026-06-11 v6 REAL (in-RMS 0.009): +0.83 dB vs dry
    kMakeup = kMakeup * ampdb(6.0 * kDrive + 2.0)  ; drive bias + slot offset: Muff +2.0 dB

    ; ================= LEFT =================
    aL   atone    aInL, kHPin               ; input cap (voicing)
    aL   = aL * kBoost                      ; Sustain booster
    aL   atone    aL, kHP1                  ; series cap into clipper 1
    aL   OS4Diode aL, 18, 0.6, 0.65         ; stage 1: ~25 dB, Si pair
    aL   dcblock2 aL
    aL   atone    aL, kHP2                  ; coupling cap into clipper 2
    aL   OS4Diode aL, 16, 0.65, 0.6         ; stage 2: inverted asymmetry
    aL   dcblock2 aL
    aLlp tone     aL, kTLP                  ; passive tone: LP path
    aLhp atone    aL, kTHP                  ;               HP path
    aLt  = (1 - kTone) * aLlp + kTone * aLhp
    aLt  pareq    aLt, 750,  kMidAmp, 1.5,  0 ; mid recovery mod
    aLt  pareq    aLt, 3200, kPrs,    0.71, 2 ; Fallout presence shelf
    aLt  butterlp aLt, 6500                 ; fizz guard lowered to 6.5k
                                            ; (de-treble 2026-06-11)
    aLt  = aLt * kMakeup                    ; output booster
    aLt  dcblock2 aLt

    ; ================= RIGHT ================
    aR   atone    aInR, kHPin
    aR   = aR * kBoost
    aR   atone    aR, kHP1
    aR   OS4Diode aR, 18, 0.6, 0.65
    aR   dcblock2 aR
    aR   atone    aR, kHP2
    aR   OS4Diode aR, 16, 0.65, 0.6
    aR   dcblock2 aR
    aRlp tone     aR, kTLP
    aRhp atone    aR, kTHP
    aRt  = (1 - kTone) * aRlp + kTone * aRhp
    aRt  pareq    aRt, 750,  kMidAmp, 1.5,  0
    aRt  pareq    aRt, 3200, kPrs,    0.71, 2
    aRt  butterlp aRt, 6500                 ; fizz guard @6.5k
    aRt  = aRt * kMakeup
    aRt  dcblock2 aRt

    ; NOISE GATE (2026-06-11, user: "super noisy"): downward expander
    ; keyed on the INPUT level, so the gate follows your playing, not the
    ; fuzz floor. Soft knee around -55 dBFS input; 4 ms open, 450 ms
    ; close — slow enough to ride the Muff's long sustain tails.
    kInEnv rms (aInL + aInR) * 0.5
    kGRaw  limit (kInEnv - 0.0012) / 0.0024, 0, 1
    kGate  SwitchPort kGRaw, 0.004, 0.45
    aGate  interp kGate
    aLt    = aLt * aGate
    aRt    = aRt * aGate

    xout aLt, aRt
endop

; DS-1K — the FLAGSHIP distortion: Boss DS-1 with the Keeley-style
; "Ultra" mod treatment (2026-06-11, replaces the SLO⊕5150 dual
; high-gain — user verdict: "rat sounds so good but that one sounds
; bad"; picked modded-DS-1 as the bread-and-butter replacement).
;
; Keeps the DHGPedal opcode name and gkDHG_* globals so no other file
; changed: same slot (DRIVE s2), same CCs, same OSC addresses.
;
; INLINE ZERO-LATENCY: opcode called directly in instr 1 (signal_chain),
; gated by gkDHG_bypass. No bus, no instr, no added latency.
;
; CIRCUIT (ElectroSmash Boss DS-1 analysis) + the mod choices that fix
; the stock pedal's famous thin/buzzy reputation:
;   STOCK:  input buffer -> transistor booster (~35 dB, asymmetric Q2
;           clipping = even harmonics) with two series HPs that thin the
;           lows -> op-amp variable gain -> HARD CLIP, Si pair to AC
;           ground -> Big-Muff-style LP/HP blend tone -> level.
;   MOD:    (a) FAT: the booster's HP corners dropped (stock ~100 Hz+
;           -> 40..120 Hz on the FAT knob) + low shelf, so lows actually
;           reach the clipper — the single biggest anti-thin change.
;           (b) CLIPPING: one Si diode swapped LED-style -> asymmetric
;           0.6/0.85 thresholds on the EDGE knob: opens the clip window,
;           louder, less compressed, less buzzy.
;           (c) MIDS: +2 dB peaking @800 Hz after the tone network so
;           the Muff-style scoop never hollows out the body.
;           (d) FIZZ: 2-pole guard at 7.5 kHz (stock DS-1 buzz lives
;           above this; mods kill it with cap changes).
;
; CONTROLS (all 0..1):
;   gkDHG_drive  op-amp gain into the hard clip, log 1x..150x.
;                Toggle-on resets to 0.75 (house rule).
;   gkDHG_tone   DS-1 tone: Muff-style crossfade of LP path (~500 Hz)
;                and HP path (~1.2 kHz); 0 = dark, 1 = cutting.
;   gkDHG_wc1    FAT: booster low corner 120 -> 40 Hz (log) plus
;                0..+3 dB low shelf @140 Hz. OSC /dhg_blend ("Fat").
;   gkDHG_wc2    EDGE: clip asymmetry morph — 0 = stock symmetric Si
;                (±0.6, tight/compressed), 1 = LED-mod asym (0.6/0.85,
;                open/aggressive) + up to +1.5 dB presence @3 kHz.
;                OSC /dhg_tight ("Edge").
;
; MAKEUP: open-loop (NO rms/env followers — they pump; see the legacy
; effects/tubescreamer.orc note). Hard clip bounds the wet peak like the
; RAT; closed-form base + the unified bench-fitted line (target +0.83 dB
; = +10% over dry at every drive — fitted by the calibration bench).

opcode DHGPedal, aa, aa
    ainL, ainR xin

    kDrv  portk gkDHG_drive, 0.02
    kTon  portk gkDHG_tone,  0.05
    kFat  portk gkDHG_wc1,   0.05
    kEdg  portk gkDHG_wc2,   0.05
    kDrv  limit kDrv, 0, 1
    kTon  limit kTon, 0, 1
    kFat  limit kFat, 0, 1
    kEdg  limit kEdg, 0, 1

    ; --- mappings ---
    kHPin   = 120 * exp(-1.09861 * kFat)    ; booster HP: 120 -> 40 Hz (log)
    kFatSh  = ampdb(3 * kFat)               ; fat low shelf 0..+3 dB @140
    kGain   = exp(5.01064 * kDrv)           ; 1x..150x (log)
    kVp     = 0.6 + 0.25 * kEdg             ; EDGE: Si 0.6 -> LED-ish 0.85
    kVn     = 0.6
    kPrsAmp = ampdb(1.5 * kEdg)             ; edge presence @3k
    kTLP    = 500                            ; DS-1 tone network corners
    kTHP    = 1200
    iMidAmp = ampdb(2)                       ; Keeley mid recovery @800

    ; open-loop base: hard clip saturates toward kV-bounded square
    iRef  = 0.05
    kV    = 0.5 * (kVp + kVn)
    kMk   = iRef / (kV * tanh(kGain * iRef / kV))
    kMk = kMk * ampdb(-5.9160 + kDrv*(-27.4240 + kDrv*(+149.1115 + kDrv*(-212.4935 + kDrv*(+93.4064)))))  ; gain-stage fit 2026-06-11 v6 REAL (in-RMS 0.009): +0.83 dB vs dry
    ; residual correction table (hot-level cal): 11 breakpoints over
    ; drive 0..1, linear interp — absorbs booster/hard-clip wiggle
    ; beyond quartic order (same mechanism as marshall.orc).
    iDhgFix ftgentmp 0, 0, 16, -2, -0.046, -0.046, +0.023, -0.035, -0.133, -0.038, +0.049, +0.006, +0.050, +0.225, +0.041
    kFixDb  tablei  kDrv * 10, iDhgFix
    kMk = kMk * ampdb(kFixDb)
    kMk = kMk * ampdb(14.0 * kDrv + 1.5)     ; drive bias + slot offset: DS-1K +1.5 dB


    ; ================= LEFT =================
    aL  butterhp ainL, kHPin                ; modded booster low corner
    aL  OS4Diode aL, 8, 0.70, 1.00          ; Q2 booster: mild asym (evens)
    aL  dcblock2 aL
    aL  pareq aL, 140, kFatSh, 0.71, 1      ; FAT shelf
    aL  OS4Hard aL, kGain, kVp, kVn         ; the DS-1 hard clip (modded)
    aL  dcblock2 aL
    aLlp tone  aL, kTLP                     ; DS-1 tone: LP path
    aLhp atone aL, kTHP                     ;            HP path
    aLt = (1 - kTon) * aLlp + kTon * aLhp
    aLt pareq aLt, 800, iMidAmp, 1.2, 0     ; mid recovery (anti-hollow)
    aLt pareq aLt, 3000, kPrsAmp, 0.71, 2   ; edge presence
    aLt butterlp aLt, 7500                  ; fizz guard (mod: kill the buzz)
    aLt = aLt * kMk
    aLt dcblock2 aLt

    ; ================= RIGHT ================
    aR  butterhp ainR, kHPin
    aR  OS4Diode aR, 8, 0.70, 1.00
    aR  dcblock2 aR
    aR  pareq aR, 140, kFatSh, 0.71, 1
    aR  OS4Hard aR, kGain, kVp, kVn
    aR  dcblock2 aR
    aRlp tone  aR, kTLP
    aRhp atone aR, kTHP
    aRt = (1 - kTon) * aRlp + kTon * aRhp
    aRt pareq aRt, 800, iMidAmp, 1.2, 0
    aRt pareq aRt, 3000, kPrsAmp, 0.71, 2
    aRt butterlp aRt, 7500
    aRt = aRt * kMk
    aRt dcblock2 aRt

    xout aLt, aRt
endop

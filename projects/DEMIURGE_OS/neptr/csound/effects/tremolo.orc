; Tremolo v2 — analog modes (2026-07-13)
; Signal chain position: stage 19.5 in instr 1 — surf-rock placement, BEFORE
;   the slap delay (62), reverb (70) and the NAM amp sims. The delay/reverb
;   tails bloom over the throb for the classic surf wash. (Briefly trialled
;   dead-last for max drama — too aggressive; baseline lives here.)
; Reads: gaTremSendL/R
; Writes: gaTremOutL/R
; Bypass: gkTremBypass (managed by preset system in midi_handler.orc)
;
; gkTremRate  = LFO frequency in Hz (default 7.38 for surf rock)
; gkTremDepth = modulation depth 0-1 (surf baseline default 0.7)
; gkTremMode  = 0 SINE / 1 OPTO / 2 BIAS / 3 HARM (stepped; toggle-on default
;               HARM per emgor 2026-07-13). All four shapes derive from ONE
;               shared LFO phase so rate feels identical across modes; mode
;               changes crossfade over 20 ms (no zipper). depth=0 is exact
;               unity in every mode.
;   0 SINE — the original v1 algorithm, null-tested byte-compatible
;   1 OPTO — photocell chop: lamp square (sine->square morph with depth)
;            through an asymmetric follower (fast ~3ms duck when the lamp
;            lights, slow ~45ms LDR recovery) — blackface helicopter
;   2 BIAS — tube-bias wobble: phase-skewed sine AM, and the SAME LFO phase
;            rides a soft tanh drive (max grit at the volume trough, i.e.
;            bias-shift headroom collapse) + gentle lowpass shading — volume,
;            grit and warmth breathe together
;   3 HARM — harmonic tremolo: 800 Hz band split, complementary see-saw duck
;            from the shared LFO (delta form: exact unity at depth 0)
;
; INTEGRATION NOTES (for the integration agent — shared files NOT touched here)
;   globals.orc     : nothing to add; gkTremMode is self-declared below
;                     (include order globals -> effects makes this safe).
;                     gkTremRate/gkTremDepth defaults unchanged in globals.
;   signal_chain.orc: NO CHANGE (same sends/outs/stage 19.5).
;   midi_handler.orc: CONTROL REMAP requested by user 2026-07-13:
;     * CC20 (secondary) -> MODE stepper, per-detent with WRAP, NAM-variant
;       convention (kVal > 64 -> +1, kVal < 64 -> -1, wrap 0..3), for
;       gkLastToggle == 43:
;         if (kVal > 64) then
;           gkTremMode = gkTremMode + 1
;           gkTremMode = (gkTremMode > 3 ? 0 : gkTremMode)
;         elseif (kVal < 64) then
;           gkTremMode = gkTremMode - 1
;           gkTremMode = (gkTremMode < 0 ? 3 : gkTremMode)
;         endif
;       (replaces the current CC20 depth binding at ~line 1835)
;     * CC23 (tertiary) -> RATE, exponential feel — move the existing
;       $ENC_ROT_EXP(gkTremRate'1.10'0.5'20) here (was $ENC_ROT_EXP 1.10
;       on the home-rotation P1 block at ~line 1644).
;     * P4 (next takeover encoder) -> DEPTH: $ENC_ROT(gkTremDepth'0.02'0'1).
;     * P1 (home slot rotation, currently rate at ~line 1644): freed by the
;       remap. RECOMMEND binding it to DEPTH as the drywet-analog per
;       encoder-takeover-v2 ("P1 = how much"); duplicate binding with P4 is
;       harmless. Integration agent decides; state the choice in the doc.
;     * Toggle-on default: when FX_TOGGLE turns tremolo ON manually, set
;       gkTremMode = 3 (HARM) — matches the init below. CAUTION: the menu-0
;       preset system also enables tremolo (preset-owned, e.g. surf preset
;       at 13.125 Hz); those preset blocks MUST explicitly set
;       gkTremMode = 0 so gig-locked surf presets keep the v1 sine sound.
;     * OSC feedback (tail block): $OSC_I(gkTremMode'/trm_mode'trmMode)
;   neptrPhase4_UI.py: EFFECT_PARAMS["trm"] -> [("rate","Rate Hz",0.5,20),
;       ("depth","Depth",0,1), ("mode","Style",0,3)]; mode is stepped —
;       render label from TRM_MODES = ["SINE","OPTO","BIAS","HARM"] if the
;       slider view supports text (LOOPER speed-index precedent), else the
;       0..3 fill is acceptable. route_osc: /trm_mode int.
;   docs            : tremolo row gains the mode param; CC20 detent action.
;   CPU             : all four shapes always computed while active (2 filters,
;                     1 tanh, 3 table reads) — cheap inline, ~unchanged; zero
;                     when bypassed via FX_BYPASS as before.

gkTremMode init 3        ; HARM default on manual toggle-on (user 2026-07-13)

instr 101
    aInL = gaTremSendL
    aInR = gaTremSendR

    kDepth portk gkTremDepth, 0.05
    kRate  portk gkTremRate,  0.05

    ; ---- shared LFO: one rate everywhere. Mode 0 keeps v1's oscili verbatim
    ; (byte-compatible null); modes 1-3 derive their shapes from a phasor at
    ; the SAME kRate (deterministic, phase-locked to each other; the constant
    ; sub-sample offset vs oscili is inaudible and bridged by the 20 ms
    ; mode crossfade).
    aLFO   oscili 1, kRate, gisine
    kLFO   downsamp aLFO
    aPhs   phasor kRate

    ; ================= mode 0: SINE (v1 algorithm, verbatim math) =========
    aAmp   interp (1 - kDepth * 0.5)
    aDep   interp (kDepth * 0.5)
    aMod0  = aAmp + aDep * aLFO
    aM0L   = aInL * aMod0
    aM0R   = aInR * aMod0

    ; ================= mode 1: OPTO (photocell) ============================
    ; lamp drive: sine->square morph with depth (squarer at high depth),
    ; 0..1 range, 1 = lamp lit = duck
    kSin01   = 0.5 + 0.5 * kLFO
    kSquare  = (kLFO > 0 ? 1 : 0)
    kLamp    = kSin01 + (kSquare - kSin01) * kDepth
    ; asymmetric one-pole: fast duck (~3 ms), slow LDR recovery (~45 ms)
    kAttC    init 0
    kRelC    init 0
    kAttC    = 1 - exp(-(ksmps/sr) / 0.003)
    kRelC    = 1 - exp(-(ksmps/sr) / 0.045)
    kFol     init 0
    kCoef    = (kLamp > kFol ? kAttC : kRelC)
    kFol     = kFol + kCoef * (kLamp - kFol)
    aFol     interp kFol
    aDepF    interp kDepth
    aMod1    = 1 - aDepF * aFol
    aM1L     = aInL * aMod1
    aM1R     = aInR * aMod1

    ; ================= mode 2: BIAS (tube-bias wobble) =====================
    ; skewed sine via phase distortion of the SAME phase
    aPhSkew  = aPhs + 0.12 * aLFO
    aWob     tablei aPhSkew, gisine, 1, 0, 1
    kWob     downsamp aWob
    aTrough  = 0.5 - 0.5 * aWob                   ; 1 at volume trough
    aMod2    = 1 - aDepF * aTrough
    aB2L     = aInL * aMod2
    aB2R     = aInR * aMod2
    ; grit rides the same phase: max drive at the trough (bias shift starves
    ; the tube — headroom collapses exactly when the volume dips, so THD
    ; peaks in the trough; 5x swing so saturation wins over the level drop)
    kG       = 1 + 5 * kDepth * (0.5 - 0.5 * kWob)
    aG       interp kG
    aSatL    = tanh(aB2L * aG) / aG
    aSatR    = tanh(aB2R * aG) / aG
    ; warmth breathes with it (subtle shading of the sat path only)
    kCut     = 9000 - 5500 * kDepth * (0.5 - 0.5 * kWob)
    aSatL    butterlp aSatL, kCut
    aSatR    butterlp aSatR, kCut
    ; sat blend scales with depth so depth=0 is EXACT unity
    kWSat    = 0.7 * (kDepth * 2 > 1 ? 1 : kDepth * 2)
    aWSat    interp kWSat
    aM2L     = aB2L + aWSat * (aSatL - aB2L)
    aM2R     = aB2R + aWSat * (aSatR - aB2R)

    ; ================= mode 3: HARM (harmonic see-saw) =====================
    ; delta form: out = in + (gLo-1)*lo + (gHi-1)*hi  -> exact unity at d=0
    aLoL     butlp aInL, 800
    aLoR     butlp aInR, 800
    aHiL     buthp aInL, 800
    aHiR     buthp aInR, 800
    aDLo     = -aDepF * (0.5 - 0.5 * aLFO)        ; lows duck on LFO-low half
    aDHi     = -aDepF * (0.5 + 0.5 * aLFO)        ; highs duck opposite
    aM3L     = aInL + aDLo * aLoL + aDHi * aHiL
    aM3R     = aInR + aDLo * aLoR + aDHi * aHiR

    ; ================= stepped mode select, 20 ms crossfade ================
    kMode    = round(limit(gkTremMode, 0, 3))
    iM0      = (round(i(gkTremMode)) == 0 ? 1 : 0)
    iM1      = (round(i(gkTremMode)) == 1 ? 1 : 0)
    iM2      = (round(i(gkTremMode)) == 2 ? 1 : 0)
    iM3      = 1 - iM0 - iM1 - iM2
    kW0      portk (kMode == 0 ? 1 : 0), 0.02, iM0
    kW1      portk (kMode == 1 ? 1 : 0), 0.02, iM1
    kW2      portk (kMode == 2 ? 1 : 0), 0.02, iM2
    kW3      portk (kMode == 3 ? 1 : 0), 0.02, iM3
    aW0      interp kW0
    aW1      interp kW1
    aW2      interp kW2
    aW3      interp kW3

    gaTremOutL = aM0L*aW0 + aM1L*aW1 + aM2L*aW2 + aM3L*aW3
    gaTremOutR = aM0R*aW0 + aM1R*aW1 + aM2R*aW2 + aM3R*aW3
endin

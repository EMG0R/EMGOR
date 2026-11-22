; Preamp — "BroadcastPre"
; Signal chain position: 0 (always-on, before any FX)
; Reads: aPrevL/R inline   Writes: aPrevL/R inline
; Bypass: none — inherently active
;
; Stage-by-stage model of the Hudson Broadcast: a transformer-coupled,
; discrete class-A germanium preamp based on 1960s broadcast consoles
; (silicon input transistor -> NOS germanium transistor -> Triad
; steel-core output transformer). The classic "always-on pedal squish
; in front of a real amp" — NOT an amp sim, NOT a compressor.
; Design + decisions: docs/specs/2026-06-11-broadcast-preamp-design.md
;
;   in -> HP 80 Hz -> [Si stage] -> [Ge stage] -> [transformer] -> LP+trim
;
;   Si stage    — small squared-term even-harmonic seasoning, DC-blocked,
;                 25 Hz coupling-cap highpass. Near-transparent floor.
;   Ge stage    — the dynamic part. A shared envelope follower (5 ms / 120 ms,
;                 mono sum — one box, one supply) moves the tanh bias point,
;                 the way a class-A germanium operating point sags under
;                 signal: dig in -> more asymmetry -> 2nd-harmonic bloom +
;                 compression "give", relaxing as the note decays.
;                   out = (tanh((in+b)·d) − tanh(b·d)) / d,  b = b0 + env·bdep
;                 The bias is interpolated to a-rate (interp) — k-rate bias
;                 steps into a waveshaper cause audible crackle. Keep a-rate.
;   Transformer — phase-coherent split at 250 Hz; the LOW band saturates
;                 harder (steel-core flux limits hit low freqs first ->
;                 low-end thickening, not flub); +1 dB bump at 80 Hz;
;                 one-pole HF rolloff at 12 kHz; DC block.
;   Output      — 9 kHz fizz rolloff, per-mode unity-loudness makeup,
;                 safety limit.
;
; MODES — giPreVoice selects a row of constants. All three are calibrated
; to unity loudness, so switching modes never jumps your level. To wire a
; UI later, replace the constant with a global and the rows still apply.
;   0 = SOFT    barely-there glue; transformer just kissed
;   1 = MEDIUM  (default) clearly audible color — warm thickening, squish
;               on hard picking, obvious when you A/B it off
;   2 = MAX     a ton through it — always saturated, heavy germanium
;               squish and transformer push

giPreVoice init 1    ; 0=SOFT  1=MEDIUM (default)  2=MAX

; Mono core: one channel through Si -> Ge -> transformer.
; kEnv is supplied by the stereo wrapper (shared between channels).
opcode BroadcastChan, a, akiiiiii
    ain, kEnv, iSiAmt, iInGain, iGeDrive, iBias0, iBiasDep, iXDrive xin

    ; --- Si input stage: even-harmonic seasoning + coupling cap ---
    aSi  =  ain + iSiAmt * ain * ain
    ; dcblock (one-pole, zero-latency) — NOT dcblock2, whose moving-average
    ; design delays ~254 samples (5.3 ms); two of those made the preamp lag.
    aSi  dcblock aSi
    aSi  =  butterhp(aSi, 25)

    ; --- Ge stage: envelope-driven bias shift, asymmetric soft clip ---
    ; iInGain drives the signal INTO the clipper (the pedal's "gain" knob) —
    ; real guitar levels sit well below full scale, so without this boost the
    ; nonlinearity barely engages. Normalized back out for unity small-signal.
    ; a-rate bias (interp) — k-rate steps here crackle. Do not downgrade.
    kBias  =  iBias0 + kEnv * iBiasDep
    aBias  interp kBias
    aGe    =  (tanh((aSi*iInGain + aBias) * iGeDrive) - tanh(aBias * iGeDrive)) / (iInGain * iGeDrive)

    ; --- Transformer: LF core saturation, LF bump, HF rolloff ---
    aLo   tone  aGe, 250
    aHi   =     aGe - aLo
    aLo   =     tanh(aLo * iXDrive) / iXDrive
    aX    =     aLo + aHi
    aX    pareq aX, 80, 1.122, 0.707, 0     ; +1 dB low resonance bump
    aX    tone  aX, 12000
    aX    dcblock aX     ; dcblock, not dcblock2 — see latency note above

    xout aX
endop

opcode Preamp, aa, aa
    aL, aR xin

    ; Mode rows:              SOFT                MEDIUM               MAX
    iV       =  giPreVoice
    iSiAmt   =  (iV == 0 ? 0.04 : (iV == 1 ? 0.10 : 0.20))   ; Si 2nd-harm amount
    iInGain  =  (iV == 0 ? 1.0  : (iV == 1 ? 3.2  : 7.0 ))   ; gain INTO the clipper
    iGeDrive =  (iV == 0 ? 1.3  : (iV == 1 ? 4.0  : 7.0 ))   ; Ge tanh drive
    iBias0   =  (iV == 0 ? 0.06 : (iV == 1 ? 0.18 : 0.30))   ; Ge static bias
    iBiasDep =  (iV == 0 ? 0.10 : (iV == 1 ? 0.35 : 0.60))   ; Ge dynamic bias depth
    iXDrive  =  (iV == 0 ? 1.15 : (iV == 1 ? 2.7  : 5.0 ))   ; transformer LF drive
    iMakeup  =  (iV == 0 ? 1.13 : (iV == 1 ? 1.92 : 4.22))   ; unity trim @ guitar level

    ; Subsonic guard (also keeps the transformer LF model sane)
    aL  =  butterhp(aL, 80)
    aR  =  butterhp(aR, 80)

    ; Shared envelope — mono sum, one box / one supply.
    ; portk smooths the k-rate follower before it reaches the bias interp.
    aEnv  follow2  (aL + aR) * 0.5, 0.005, 0.12
    kEnv  downsamp aEnv
    kEnv  portk    kEnv, 0.008
    kEnv  limit    kEnv, 0, 1

    aOutL  BroadcastChan aL, kEnv, iSiAmt, iInGain, iGeDrive, iBias0, iBiasDep, iXDrive
    aOutR  BroadcastChan aR, kEnv, iSiAmt, iInGain, iGeDrive, iBias0, iBiasDep, iXDrive

    ; Fizz rolloff + unity makeup + safety
    aOutL  =  tone(aOutL, 9000) * iMakeup
    aOutR  =  tone(aOutR, 9000) * iMakeup
    aOutL  limit aOutL, -0.99, 0.99
    aOutR  limit aOutR, -0.99, 0.99

    xout aOutL, aOutR
endop

; Pitch Shifter ("PS")
; Signal chain position: 50
; Reads: gaPSSendL/R
; Writes: gaPSOutL/R
; Bypass: N/A (wired via gkBypassPS/$FX_BYPASS in signal_chain.orc)
;
; 2026-08-23 SOTA upgrade (user: "the pitch shifting sounds like shit i
; need it using highest quality pitch shifting option for chill shit"):
; this instrument used to call pitchshifter4, a 4-grain time-domain
; delay-line "echo shifter" — the SAME algorithm h90.orc's header
; explicitly documents as "the gritty 4-grain echo shifter the user
; vetoed" when it was replaced on the plain DELAY effect's pitch stage.
; It was still live here, unreplaced, on NEPTR's own dedicated pitch
; shifter — this is almost certainly what the user is hearing.
;
; Replaced with H90Shift (globals.orc), the same asymmetric phase-vocoder
; pipeline already proven out on the H90 effect (h90.orc instr 48) and on
; the plain delay's pitch stage (delay.orc instr 62): down-shift uses
; pvslock (Laroche-Dolson peak-locked PV, clean on monophonic/harmonic
; content); up-shift uses a steep 4x cascaded anti-alias pre-filter +
; pure pvscale (no peak-locking, which the SOTA research pass found smears
; polyphonic/chord harmonics) + a warmth chain (tanh soft-clip + gentle
; top-end shelf) tuned to avoid the chipmunk/phasiness artifacts of naive
; PV up-shifting. This IS the highest-quality pitch-shift option already
; proven in this codebase — no reason to invent a third algorithm.
;
; Latency tradeoff: the PV pipeline has ~16-21 ms inherent analysis
; latency (1024-pt FFT, 256 hop). That's the right tradeoff for "chill
; shit" sustained pad/synth material — this is not a real-time
; performance path (unlike the separate Bela guitar project, which had to
; solve pitch-shift latency with a delay-line/H910-style trick because a
; guitarist is reacting to their own note in real time); quality wins
; here with no perceptible cost.
;
; Mono-summed before shifting (same convention H90 and the delay's pitch
; stage both use) rather than running two independent per-channel PV
; instances — keeps this at 1x the PV pipeline cost instead of 2x, matches
; the codebase's established Pi-CPU-budget convention for this algorithm.

instr 50
    aInL = gaPSSendL
    aInR = gaPSSendR
    kSemi  limit gkSemi, gkPS_min, gkPS_max
    kRatio = pow(2, kSemi/12)
    aInM   = (aInL + aInR) * 0.5
    aShM   H90Shift aInM, kRatio
    gaPSOutL = aShM
    gaPSOutR = aShM
endin

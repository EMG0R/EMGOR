<CsoundSynthesizer>
;
; DEMIURGE hello_patching — Csound slow RING-MOD + PLATE REVERB.
;
; Final stage of the chain. Two parts:
;   1. Slowly-evolving ring modulator: carrier target is re-chosen
;      every 4 beats (not every beat) and glides over half a beat.
;      Modulation depth is SUBTLE — mostly dry with a faint ring halo.
;   2. Plate reverb (reverbsc) with long tempo-locked decay so the
;      whole chain has a continuous shimmering tail.
;
; MIDI:  reads ch16 CC119 (global BPM) via alsaseq 14:0.
;
; CC map:
;   ch 1 CC 50 — ringmod depth nudge  (0..1)
;
; PARAMETER (docs/parameters.md): /csound/reverb  (0..1 -> reverb wet 0..0.9)
;   Driven from the Demiurge UI via the pool (`/p /csound/reverb <v>` on this
;   stage's manifest port) and echoed back with `/pout`. This REPLACES the old
;   CC51 nudge — one mechanism per job. Hook library: params/csound/
;   demiurge_params.udo under $DEMIURGE_RESOURCES (pass --env:INCDIR=...).
;   Manifest: params/csound.json (stage 6 -> port 9006; DEMIURGE_PARAM_PORT
;   overrides at launch).
;
<CsOptions>
-odac
-iadc
-d
--realtime
-B 1024
-b 128
-+rtmidi=alsaseq
-M14:0
-m0
</CsOptions>

<CsInstruments>

#include "params/csound/demiurge_params.udo"
; Fallback port (stage 6 -> 9006). Set AFTER the include (the udo re-inits it to 0)
; and used only when the launcher passes no --omacro:DEMIURGE_PARAM_PORT.
giDParamPort init 9006

sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

gk_bpm         init 110
gk_ring_depth  init 0.18
gk_wet_pool    init 0.25
gk_carrier_tgt init 220
gk_beat_count  init 0

; ---- CC router + pool parameter ----
instr 1
  dparam_init "csound", 0   ; 0 -> --omacro:DEMIURGE_PARAM_PORT, else giDParamPort
  gk_bpm_cc      ctrl7 16, 119, 0, 1
  gk_bpm          = 40 + gk_bpm_cc * 200
  gk_depth_cc    ctrl7 1, 50, 0, 1
  gk_ring_depth   = 0.10 + gk_depth_cc * 0.30
  gk_wet_pool    dparam "/csound/reverb", 0.25
  dpout "/csound/reverb", gk_wet_pool
endin

; ---- Ring-mod + plate reverb ----
instr 10
  aL, aR ins

  ; --- Slow carrier re-chosen every 4 beats ---
  ; A metro at (bpm/240) Hz fires once every four beats. trandom
  ; latches a fresh carrier target in [80, 420] Hz on each fire;
  ; port glides toward it over ~a beat for a smooth evolving tone.
  kchoose metro gk_bpm / 240
  gk_carrier_tgt trandom kchoose, 80, 420
  kcarrier port gk_carrier_tgt, 0.45
  kcarrier limit kcarrier, 40, 1200

  ; --- Pure sine carriers, slight stereo detune ---
  aCarL  oscili 1.0, kcarrier
  aCarR  oscili 1.0, kcarrier * 1.0025

  ; --- Ring mod: subtle, blended with dry ---
  arL  = aL * aCarL
  arR  = aR * aCarR
  aringL = aL * (1 - gk_ring_depth) + arL * gk_ring_depth
  aringR = aR * (1 - gk_ring_depth) + arR * gk_ring_depth

  ; --- Plate reverb (reverbsc) — long tail ---
  ; fco 12k, feedback 0.88 → spacious but not muddy.
  awetL, awetR reverbsc aringL, aringR, 0.74, 11000

  kmix = gk_wet_pool * 0.9
  kmix limit kmix, 0.0, 0.9

  aoutL = aringL * (1 - kmix) + awetL * kmix
  aoutR = aringR * (1 - kmix) + awetR * kmix

  aoutL clip aoutL, 2, 0.95
  aoutR clip aoutR, 2, 0.95

  outs aoutL, aoutR
endin

</CsInstruments>

<CsScore>
i 1  0 36000
i 10 0 36000
</CsScore>
</CsoundSynthesizer>

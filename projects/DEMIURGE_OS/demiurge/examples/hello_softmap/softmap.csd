<CsoundSynthesizer>
;
; DEMIURGE hello_softmap — a small self-playing Csound voice whose three
; parameters are all "map": "soft". See params/csound.json and README.md.
;
; A "soft" parameter is one a player is expected to re-map: the engine neither
; knows nor cares what drives it. It only listens for the pool's
; `/p /csound/<name> <0..1>` and reports its value back with `/pout`. A
; hardware source (a LiDAR, a knob, a gyro) reaches it only through the
; per-patch map file (hello_softmap.map.json). Engines never touch hardware.
;
; Parameters (all 0..1 on the wire, scaled to native units here, last inch):
;   /csound/cutoff   lowpass cutoff   200..8000 Hz (exp)
;   /csound/rate     note rate        0.5..6 notes/s (exp)
;   /csound/detune   saw detune       0..25 cents
;
; Hook library: params/csound/demiurge_params.udo under $DEMIURGE_RESOURCES
; (run csound with --env:INCDIR=$DEMIURGE_RESOURCES). Stage port 9000 here
; (single-stage chain); DEMIURGE_PARAM_PORT overrides at launch.
<CsOptions>
-odac
-d
--realtime
-B 1024
-b 128
-m0
</CsOptions>

<CsInstruments>

#include "params/csound/demiurge_params.udo"
; Fallback port (stage 0 -> 9000), set after the include; the launcher's
; --omacro:DEMIURGE_PARAM_PORT wins when present.
giDParamPort init 9000

sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

gk_cut init 0.5
gk_rate init 0.4
gk_det init 0.3

; ---- pool parameters: read, then echo on change ----
instr 1
  dparam_init "csound", 0
  gk_cut  dparam "/csound/cutoff", 0.5
  gk_rate dparam "/csound/rate",   0.4
  gk_det  dparam "/csound/detune", 0.3
  dpout "/csound/cutoff", gk_cut
  dpout "/csound/rate", gk_rate
  dpout "/csound/detune", gk_det
endin

; ---- note generator: random pentatonic pluck, rate set by /csound/rate ----
instr 2
  krate = 0.5 * pow(12, gk_rate)              ; 0.5 .. 6 notes/s
  ktrig metro krate
  if ktrig == 1 then
    kdeg  = int(random:k(0, 5))
    kmidi = 48 + 12 * int(random:k(0, 3)) + (kdeg == 0 ? 0 : kdeg == 1 ? 2 : kdeg == 2 ? 4 : kdeg == 3 ? 7 : 9)
    event "i", 3, 0, 1.6, cpsmidinn(kmidi)
  endif
endin

instr 3
  icps = p4
  kcut = 200 * pow(40, gk_cut)                ; 200 .. 8000 Hz
  kdet = gk_det * 25                          ; cents
  a1 vco2 0.18, icps * cent(kdet)
  a2 vco2 0.18, icps * cent(-kdet)
  aenv expseg 0.001, 0.01, 1, 1.5, 0.001
  aout moogladder (a1 + a2) * aenv, kcut, 0.35
  outs aout, aout
endin

</CsInstruments>

<CsScore>
i 1 0 36000
i 2 0 36000
</CsScore>
</CsoundSynthesizer>

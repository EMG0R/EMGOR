<CsoundSynthesizer>
;
; DEMIURGE latency — SWEEP INJECTOR (self-generating).
;
; Synthesises a looping exponential sine sweep (20 Hz -> 20 kHz over iSweep
; seconds) directly — no soundfile, no diskin2 (which segfaulted on the Pi).
; Emits to JACK ports inject:out1 / inject:out2. The analyzer uses the
; CAPTURED inject:out1 as its reference, so the exact sweep shape need not be
; known a-priori — any repeatable broadband signal measures latency by
; cross-correlation.
;
<CsOptions>
-+rtaudio=jack
-+jack_client=inject
-+jack_outportname=out
-odac
-d
-m0
--realtime
-B 4096
-b 1024
</CsOptions>

<CsInstruments>

sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

instr 1
  iSweep = 2.0                 ; sweep period (s) — loops continuously
  if0    = 20                  ; start frequency
  if1    = 20000               ; end frequency
  ilog   = log(if1 / if0)

  ; TRIANGLE sweep in log-frequency: up 20 Hz -> 20 kHz, then back down,
  ; continuously. The turnarounds at both ends are smooth (frequency is
  ; continuous, only its slope flips), so there is NO discontinuity to emit
  ; a click or require a fade — the glitch fingerprint stays clean.
  asaw   phasor 1 / (2 * iSweep)        ; 0 -> 1 over a full up+down cycle
  atri   = 1 - abs(2 * asaw - 1)        ; 0 -> 1 -> 0 (up then down)
  afreq  = if0 * exp(atri * ilog)
  ; integrate frequency -> continuous phase (phasor integrates its a-rate in).
  aphase phasor afreq
  asig   = 0.5 * sin(6.2831853 * aphase)
  outs   asig, asig
endin

</CsInstruments>

<CsScore>
i 1 0 z
</CsScore>

</CsoundSynthesizer>

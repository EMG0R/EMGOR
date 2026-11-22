<CsoundSynthesizer>
;
; DEMIURGE probe — TEST SIGNAL INJECTOR.
;
; Emits a deterministic multi-tone + impulse signal to JACK ports
; inject:out1 / inject:out2. probe.sh rewires this client into csound's
; input in place of the live mic capture.
;
; Signal composition (mono, duplicated to both channels):
;
;   1. Five pure sine tones, each at amplitude 0.044 (-27 dBFS), summed:
;        87 Hz, 240 Hz, 661 Hz, 1820 Hz, 5010 Hz
;      Logarithmically spaced (ratio ≈ 2.75 between adjacent tones, NOT
;      2 = octave, so no harmonic alignment between them). Spans six
;      octaves: low bass through high mid. Loads the FX broadband
;      but leaves the entire spectrum BETWEEN the tones essentially
;      silent — so any digital artifact (click, crackle, aliasing,
;      buffer-stall hash, IMD spurs that don't land on harmonic
;      relationships) appears in the gaps and stands out cleanly.
;      Total tone RMS ≈ 0.07 (-23 dBFS). Worst-case in-phase peak 0.22.
;
;   2. Sample-accurate impulse train, amplitude 0.5 (-6 dBFS peak),
;      every 0.25 s. Unambiguous time markers for latency / jitter.
;
; Total peak ≈ 0.72 (-2.8 dBFS). Well clear of clipping.
;
; Why multi-tone (not pink noise, not single sine):
;   - Single sine (1 kHz):   only tests one frequency. Misses everything
;                            off-fundamental. Doesn't load broadband.
;   - Pink/white noise:      loads broadband well, but the noise floor
;                            MASKS the very glitches we're trying to
;                            detect. Both algorithmic and ear-based
;                            detection lose to the noise.
;   - Multi-tone:            broadband load + narrow tonal footprint =
;                            max glitch SNR. Industry standard for
;                            audio-chain analysis (AES17 § 9.3).
;
; The five fundamentals' main IMD products (sum/difference of pairs):
;   240−87=153  661−240=421  1820−661=1159  5010−1820=3190
;   240+87=327  661+240=901  1820+661=2481  5010+1820=6830
;   Plus 2nd harmonics: 174, 480, 1322, 3640, 10020 Hz.
; These are FX coloration, not glitches. The analyzer can mask them
; from "anomaly" counting if needed.
;
<CsOptions>
-+rtaudio=jack
-+jack_client=inject
-+jack_outportname=out
-odac
-d
-m0
--realtime
; -b is the JACK bufsize. Per gotchas.md Q7a must be an integer MULTIPLE
; of clock.force-quantum, NEVER a divisor (divisor = silence).
; 1024 is a multiple of 64/128/256/512, safe for any quantum we sweep.
-B 4096
-b 1024
</CsOptions>

<CsInstruments>

sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

instr 1
  ; Five non-octave-related sine tones. Same amplitude each so the
  ; analyzer can compare them against an equal-energy template.
  a1   oscili 0.044,   87
  a2   oscili 0.044,  240
  a3   oscili 0.044,  661
  a4   oscili 0.044, 1820
  a5   oscili 0.044, 5010

  ; Sample-accurate impulse every 0.25 s.
  aclk mpulse 0.5, 0.25

  asum  =  a1 + a2 + a3 + a4 + a5 + aclk
  outs  asum, asum
endin

</CsInstruments>

<CsScore>
i 1 0 z
</CsScore>

</CsoundSynthesizer>

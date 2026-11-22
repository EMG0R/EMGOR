<CsoundSynthesizer>
;
; ~/demiurge/examples/hello_patching/csound_reverb.csd
;
; DEMIURGE hello_patching — stage 5 of 5: Csound REVERBSC (Sean Costello).
;
; Takes stereo in from stage 4 (Faust bitcrusher), runs it through
; reverbsc with generatively-modulated feedback and cutoff, outputs
; to Scarlett (demiurge-sink).
;
; Self-modulates: feedback and cutoff follow slow LFOs so the reverb
; space breathes without any user input.
;
; CC50 = feedback manual override (0..1)
; CC51 = cutoff manual override   (0..1 → 1kHz..16kHz)
;
; The last stage in the chain. This is where the sound lands.

<CsOptions>
-odac
-iadc
-d
--realtime
-B 1024
-b 128
-+rtmidi=alsaseq
-M0
-m0
</CsOptions>

<CsInstruments>

sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

gk_fb_cc   init 0.75
gk_cut_cc  init 0.5

; -------- CC router --------
instr 1
  gk_fb_cc  ctrl7 1, 50, 0, 1
  gk_cut_cc ctrl7 1, 51, 0, 1
endin

; -------- Reverb main --------
instr 10
  ; Stereo input from the upstream patch graph (in1 + in2)
  aL, aR ins

  ; --- Generative motion: slow LFOs drift the reverb parameters ---
  kfb_lfo    lfo    0.12, 0.07, 0  ; sine LFO, 0.07 Hz, ±0.12
  kcut_lfo   lfo    0.25, 0.11, 0  ; sine LFO, 0.11 Hz, ±0.25

  ; --- Blend CC-driven and generative motion ---
  ; feedback: 0.70 .. 0.94, biased by CC50
  kfb     = 0.82 + kfb_lfo + (gk_fb_cc - 0.5) * 0.2
  kfb     limit kfb, 0.55, 0.94

  ; cutoff: 2 kHz .. 14 kHz, biased by CC51
  kcut_n  = 0.5 + kcut_lfo + (gk_cut_cc - 0.5) * 0.5
  kcut_n  limit kcut_n, 0.05, 1.0
  kcut    = 2000 + kcut_n * 12000

  ; --- reverbsc: Sean Costello FDN reverb ---
  arvL, arvR reverbsc aL, aR, kfb, kcut

  ; --- Wet/dry mix; reverb gets most of the weight since this is the
  ;     final stage. Dry tail keeps some definition in the attacks. ---
  ; Master: big dry passthrough + generous reverb wash, then +6 dB makeup
  aoutL  = (aL * 0.7 + arvL * 0.75) * 1.2
  aoutR  = (aR * 0.7 + arvR * 0.75) * 1.2

  ; Soft limit
  aoutL  clip aoutL, 2, 0.95
  aoutR  clip aoutR, 2, 0.95

  outs aoutL, aoutR
endin

</CsInstruments>

<CsScore>
i 1  0 36000  ; CC router
i 10 0 36000  ; reverb
</CsScore>
</CsoundSynthesizer>

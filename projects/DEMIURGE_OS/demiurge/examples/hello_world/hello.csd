<CsoundSynthesizer>
;
; ~/demiurge/examples/hello_world/hello.csd
;
; DEMIURGE Csound hello_world — generative FM arp into a tempo-locked
; effects chain: vibrato -> chorus -> ping-pong delay -> Leslie-gated
; ringmod -> reverbsc. Every modulation rate and every delay time is
; derived from gk_beatsec, which tracks the master clock via ch16 CC119
; (demiurge-clock BPM broadcast). If the clock steers BPM, the whole
; patch — arp, delay, chorus, vibrato, reverb drift — glides with it.
;
; MIDI in:  Midi Through (PortMIDI device 0) — shared DEMIURGE bus.
; Master BPM: ch16 CC119 (emitted by demiurge-clock once per beat).
; Audio out: Scarlett 1+2 via -odac (pipewire-alsa compat).
;
; Local CCs (channel 1):
;   CC1  — FM morph (index/ratio/filter)
;   CC7  — master volume

<CsOptions>
-odac
--realtime
-b 128
-B 1024
-+rtmidi=alsaseq
-M 0
-m0
</CsOptions>

<CsInstruments>

sr      = 48000
ksmps   = 64
nchnls  = 2
0dbfs   = 1

; -------- Master clock state (from demiurge-clock ch16 CC119) --------
gk_bpm       init 110
gk_beatsec   init 60/110
gk_sixteenth init (60/110)/4

; -------- Shared patch state --------
gk_morph  init 0.5     ; CC1
gk_vol    init 0.85    ; CC7
gk_arpidx init 0

; -------- Effects send bus --------
ga_sendL init 0
ga_sendR init 0

; -------- MIDI CC router + master clock reader --------
instr 1
  ; Local voice controls
  gk_morph ctrl7 1, 1, 0, 1
  gk_vol   ctrl7 1, 7, 0, 1

  ; Master BPM from demiurge-clock (ch16 CC119, 40..240 BPM mapped 0..127)
  kbpm_cc      ctrl7 16, 119, 0, 1
  gk_bpm       = 40 + kbpm_cc * 200
  gk_beatsec   = 60 / gk_bpm
  gk_sixteenth = gk_beatsec / 4
endin

; -------- FM voice with built-in slow vibrato --------
instr 10
  inote   = p4
  ivel    = p5
  icps    = cpsmidinn(inote)

  kenv    linen ivel/127 * 0.32, 0.006, p3, 0.22

  ; Super slow chill vibrato: 1 cycle per 8 beats, ±10 cents
  kvib_rate = 1 / (8 * gk_beatsec)
  kvib      lfo 0.006, kvib_rate, 0
  kcps      = icps * (1 + kvib)

  ; Generative FM: ratio/index morph with CC1
  kratio    = 2 + gk_morph * 3
  kindex    = 3 + gk_morph * 6
  amod      oscili kindex * kcps, kcps * kratio
  acar      oscili kenv, kcps + amod

  kcf       = 1500 + 2500 * gk_morph
  afilt     moogladder acar, kcf, 0.3

  ; Feed effects bus (stereo split by note pitch for gentle spread)
  kpan      = 0.5 + ((inote - 60) / 48) * 0.35
  kpan      limit kpan, 0.15, 0.85
  ga_sendL  = ga_sendL + afilt * (1 - kpan)
  ga_sendR  = ga_sendR + afilt * kpan
endin

; -------- Self-generative pentatonic arp, locked to 16ths --------
instr 20
  ipenta[] fillarray 0, 3, 5, 7, 10

  ; 16th-note metro from master tempo
  ksix_hz  = 1 / gk_sixteenth
  ktrig    metro ksix_hz

  if ktrig == 1 then
    kdeg   = ipenta[gk_arpidx % 5]
    koct   = 60 + (gk_arpidx % 3) * 12
    knote  = koct + kdeg
    event "i", 10, 0, gk_sixteenth * 0.9, knote, 82
    gk_arpidx = gk_arpidx + 1
  endif
endin

; -------- External MIDI note handler --------
massign 0, 30
instr 30
  inote   notnum
  ivel    veloc
  event_i "i", 10, 0, 0.32, inote, ivel
  turnoff
endin

; -------- FX bus: chorus -> delay -> Leslie ringmod -> reverbsc --------
instr 80
  aL = ga_sendL
  aR = ga_sendR

  ; Smoothed beat time so delay / rates glide under BPM changes
  kbeatsec_s init 60/110
  kbeatsec_s port gk_beatsec, 0.5

  ; =========================================================
  ; CHORUS — super slow, chill, wide
  ; =========================================================
  ; Base 14 ms delay, ±4 ms modulation, ~1 cycle per 6 beats.
  ; L/R use slightly different rates (×1.13) for stereo decorrelation.
  kch_rate  = 1 / (6 * kbeatsec_s)
  kch_l     lfo 4, kch_rate, 0
  kch_r     lfo 4, kch_rate * 1.13, 0
  ach_L     vdelay aL, 14 + kch_l, 40
  ach_R     vdelay aR, 14 + kch_r, 40
  aL        = aL * 0.55 + ach_L * 0.65
  aR        = aR * 0.55 + ach_R * 0.65

  ; =========================================================
  ; PING-PONG DELAY — clearly audible, quarter on L, dotted-8 on R
  ; =========================================================
  imaxdel   = 2.5
  atime_L   = a(kbeatsec_s)               ; quarter note
  atime_R   = a(kbeatsec_s * 0.75)        ; dotted 8th

  afbL      init 0
  afbR      init 0

  abufL     delayr imaxdel
  atapL     deltap3 atime_L
            delayw aL + afbR * 0.45

  abufR     delayr imaxdel
  atapR     deltap3 atime_R
            delayw aR + afbL * 0.45

  afbL      = atapL
  afbR      = atapR

  adlyL     = aL * 0.75 + atapL * 0.65
  adlyR     = aR * 0.75 + atapR * 0.65

  ; =========================================================
  ; RINGMOD — Leslie vibe. Sub-audio carrier (~3..5 Hz), rate
  ; itself drifting on a 12-beat LFO. Gated on/off at 4-beat
  ; slot boundaries with ~1/4 on probability. Gate is
  ; port-smoothed over half a beat so the horn eases in/out.
  ; =========================================================
  krm_rate_lfo  lfo 1.0, 1 / (12 * kbeatsec_s), 0
  krm_rate      = 4 + krm_rate_lfo            ; 3..5 Hz
  arm_car       oscili 1, krm_rate

  ; Beat counter driven by beat metro
  kbeat         init 0
  kbeat_trig    metro 1 / kbeatsec_s
  if kbeat_trig == 1 then
    kbeat = kbeat + 1
  endif
  kslot         = int(kbeat / 4)              ; new slot every 4 beats
  khash         = (kslot * 2654435761 + 12345) % 4
  kgate_target  = (khash == 0 ? 1 : 0)        ; ~1/4 of slots are ON
  kgate         portk kgate_target, kbeatsec_s * 0.5, 0

  armL          = adlyL * arm_car
  armR          = adlyR * arm_car
  kwet          = kgate * 0.7
  amixL         = adlyL * (1 - kwet) + armL * kwet
  amixR         = adlyR * (1 - kwet) + armR * kwet

  ; =========================================================
  ; REVERBSC — Sean Costello FDN, breathing feedback
  ; =========================================================
  krv_fb_lfo    lfo 0.04, 1 / (16 * kbeatsec_s), 0
  kfb           = 0.86 + krv_fb_lfo
  kfb           limit kfb, 0.80, 0.92
  arvL, arvR    reverbsc amixL, amixR, kfb, 8500

  ; =========================================================
  ; Master mix + soft clip
  ; =========================================================
  aoutL = (amixL * 0.55 + arvL * 0.65) * gk_vol
  aoutR = (amixR * 0.55 + arvR * 0.65) * gk_vol
  aoutL clip aoutL, 2, 0.95
  aoutR clip aoutR, 2, 0.95
  outs aoutL, aoutR

  ga_sendL = 0
  ga_sendR = 0
endin

</CsInstruments>

<CsScore>
i 1  0 3600   ; CC router + master clock reader
i 20 0 3600   ; self-generative arp (16ths locked to master)
i 80 0 3600   ; FX bus — chorus / delay / ringmod / reverbsc
</CsScore>
</CsoundSynthesizer>

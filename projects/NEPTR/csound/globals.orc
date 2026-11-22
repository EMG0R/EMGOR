;==============================================================
; GLOBALS.ORC — Phase 4 Modular Guitar Effects Processor
; Deduplicated global declarations, function tables, macros, UDOs
;==============================================================

sr = 48000
; KSMPS macro override. Default 64 (2026-07-26): quantum lowered 128->64 in
; live.conf, and ksmps must match quantum or csound rounds -b UP to a ksmps
; multiple and silently re-raises the period. The VST already runs these orcs
; at ksmps 64 (proven), so hardware 64 is proven-safe. Override with
; --omacro:KSMPS=128 to go back to the old period.
#ifndef KSMPS
#define KSMPS #64#
#endif
ksmps = $KSMPS
              ; lockstep with live.conf quantum (csound rounds -b UP to a ksmps
              ; multiple, so ksmps > quantum silently re-raises the period).
              ; ksmps < quantum is safe (more k-cycles); keep <= every pvsanal
              ; hop (PFORK hop=256, SpecDel giHop=256) — hop < ksmps is the old
              ; PFORK crash mode. Gotchas Q7f.
; Channel count is a per-node MODE (~/.config/neptr/mode: stereo | 8ch), not an edit
; to this file: demiurge-run-csound reads the mode and passes these macros.
; Absent = 2 / 2, exactly as before. 8ch also turns on the mixer (mixer.orc).
#ifndef NEPTR_NCHNLS
#define NEPTR_NCHNLS #2#
#end
#ifndef NEPTR_NCHNLS_I
#define NEPTR_NCHNLS_I #2#
#end
nchnls = $NEPTR_NCHNLS
nchnls_i = $NEPTR_NCHNLS_I
0dbfs = 1
seed 0

;==============================================================
; MACROS
;==============================================================

; Effect bypass/launch/kill — standardized for signal chain
; All bypass checks use < 0.5 (active) / >= 0.5 (bypassed)
#define FX_BYPASS(kByp'iNum'gaSndL'gaSndR'gaOutL'gaOutR'uid)#
  kfx_$uid._byp = $kByp
  kfx_$uid._prv init -1
  if (kfx_$uid._prv == -1) then
    if (kfx_$uid._byp < 0.5) then
      event "i", $iNum, 0, -1
    else
      $gaOutL = $gaSndL
      $gaOutR = $gaSndR
    endif
  else
    if (kfx_$uid._prv >= 0.5 && kfx_$uid._byp < 0.5) then
      event "i", $iNum, 0, -1
    elseif (kfx_$uid._prv < 0.5 && kfx_$uid._byp >= 0.5) then
      turnoff2 $iNum, 0, 1
      $gaOutL = $gaSndL
      $gaOutR = $gaSndR
    endif
  endif
  kfx_$uid._prv = kfx_$uid._byp
  if (kfx_$uid._byp >= 0.5) then
    $gaOutL = $gaSndL
    $gaOutR = $gaSndR
  endif
#

; Tail-aware bypass for the ALWAYSON effects (tape delay 190, digiverb 193,
; shimmer 194). Those were started from <CsScore> with `z` and gated only by
; an internal kGate that multiplies input and output — the reverb/tape DSP
; still ran at FULL COST while "off". Measured 2026-08-10: csound sat at
; 73-100% of one core no matter what was enabled, and xruns were DSP-bound,
; not scheduling-bound. $FX_BYPASS could not be used on them directly: it
; turns the instrument off the instant you bypass, chopping the tail.
;
; This macro keeps the instrument running for iTail seconds after bypass so
; the tail rings out naturally (the instrument's own kGate has already muted
; its input), THEN turnoff2s it. While it is off, dry is parked in the wet
; bus every k-cycle — mandatory, because these stages assign the wet bus
; straight through (`aPrev = gaTdlWetL`), so a stopped instrument would
; otherwise leave a frozen a-vector looping as a buzz.
;
; Trade: re-enabling starts a FRESH instance, so tape-transport warmth and
; reverb tanks begin empty rather than resuming a spinning tape. That is the
; price of not burning a core on silence.
#define FX_BYPASS_TAIL(kByp'iNum'iTail'gaDryL'gaDryR'gaWetL'gaWetR'uid)#
  kgt_$uid._byp = $kByp
  kgt_$uid._prv init -1
  kgt_$uid._t   init 0
  if (kgt_$uid._prv == -1) then
    if (kgt_$uid._byp < 0.5) then
      event "i", $iNum, 0, -1
    endif
  elseif (kgt_$uid._prv >= 0.5 && kgt_$uid._byp < 0.5) then
    event "i", $iNum, 0, -1
    kgt_$uid._t = 0
  elseif (kgt_$uid._prv < 0.5 && kgt_$uid._byp >= 0.5) then
    kgt_$uid._t = $iTail * kr
  endif
  if (kgt_$uid._t > 0) then
    kgt_$uid._t = kgt_$uid._t - 1
    if (kgt_$uid._t <= 0) then
      turnoff2 $iNum, 0, 1
    endif
  endif
  kgt_$uid._prv = kgt_$uid._byp
  if (kgt_$uid._byp >= 0.5 && kgt_$uid._t <= 0) then
    $gaWetL = $gaDryL
    $gaWetR = $gaDryR
  endif
#

; Toggle effect bypass and set last-toggle tracker
#define FX_TOGGLE(kByp'iToggle)#
  $kByp = 1 - $kByp
  if ($kByp < 0.5) then
    gkLastToggle = $iToggle
  endif
#

; Glitch press (button down): capture pre-press bypass, force effect ON,
; mark held, stamp press time, set lastToggle trackers for BPM + chance dispatch.
#define GLITCH_PRESS(kByp'kHeld'kPressT'kPrev'iToggle)#
  $kPrev = $kByp
  $kByp = 0
  $kHeld = 1
  $kPressT times
  gkLastToggle = $iToggle
  gkLastToggle29 = $iToggle
#

; Glitch release (button up): classify hold vs click.
; - hold  (>= giGLITCH_holdThresh): effect goes OFF
; - click (<  giGLITCH_holdThresh): bypass flips its pre-press state
#define GLITCH_RELEASE(kByp'kHeld'kPressT'kPrev'iToggle)#
  kNow_$iToggle. times
  kDt_$iToggle. = kNow_$iToggle. - $kPressT
  $kHeld = 0
  if (kDt_$iToggle. >= giGLITCH_holdThresh) then
    $kByp = 1
  else
    $kByp = 1 - $kPrev
    if ($kByp < 0.5) then
      gkLastToggle = $iToggle
      gkLastToggle29 = $iToggle
    endif
  endif
#

; Encoder rotation: relative increment/decrement with clamp
; Assumes kVal is in scope (MIDI CC value, >64 = CW, <64 = CCW)
#define ENC_ROT(kVar'kStep'kMin'kMax)#
  if (kVal > 64) then
    $kVar = min($kVar + $kStep, $kMax)
  elseif (kVal < 64) then
    $kVar = max($kVar - $kStep, $kMin)
  endif
#

; Exponential (multiplicative) encoder rotation — for rate/frequency knobs.
; Fine + slow at the low end, fast at the top: nice feel across wide ranges.
; kFac per tick (e.g. 1.12 ~= full 0.05..30 sweep in ~55 ticks). kMin MUST be > 0.
; Self-heals from <=kMin (so a var that was 0 still climbs).
#define ENC_ROT_EXP(kVar'kFac'kMin'kMax)#
  if (kVal > 64) then
    $kVar = min(max($kVar, $kMin) * $kFac, $kMax)
  elseif (kVal < 64) then
    $kVar = max($kVar / $kFac, $kMin)
  endif
#

; UI late-join sync (2026-08-09, demiurge/docs/engine-ui.md): a one-k-cycle
; pulse that makes every $OSC_F/$OSC_I macro below fire unconditionally, so a
; UI that (re)starts mid-session receives the FULL state/param snapshot.
; Raised by instr 99 when the UI's sync request arrives (ch16 CC102 press),
; cleared by instr 99 at the top of the next cycle — instruments numbered
; above 99 see it the same cycle, those below see it the next; every macro
; fires exactly once per request.
gkOscRefresh init 0
; Staggered refresh sweep (2026-10-24). The pulse used to fire all ~360
; $OSC_F/$OSC_I macros (x3 sends each = 1000+ OSCsend) in ONE k-cycle: a
; guaranteed overrun, measured as HAT xruns 0-7 s after every UI resync. Now
; each macro gets a slot (init order mod OSC_REF_SLOTS) and resends only when
; gkOscRefSlot (stepped by instr 99, -1 = idle) reaches it: same full
; snapshot, spread over 32 k-cycles (~85 ms at ksmps 128).
#define OSC_REF_SLOTS #32#
gkOscRefSlot init -1
gkOscSweepReq init 0
giOscSlotN init 0
gkSpkT0 init 0          ; spike_probe.orc: wall time at the top of instr 1 (cycle start)

; PRESET SYSTEM (2026-09-08) — full-state capture/recall pulses. Set to 1 by a
; ch16 utility CC (105 = capture, 106 = recall) from the host (demiurge-preset /
; DUODECIMUS foot controller), consumed + cleared the same/next cycle by
; state_persist.orc (instr 97 capture, instr 96 recall). A "preset" is a saved
; copy of the giNeptrState table (.ftab) in ~/demiurge/presets/<context>/ — so
; recall restores EVERY effect param + toggle + routing exactly, and persists.
gkPresetCapture init 0
gkPresetRecall  init 0

; OSC feedback: send float on change (or in its slot of a refresh sweep)
#define OSC_F(kVar'Saddr'uid)#
  iOsl_$uid. = giOscSlotN % $OSC_REF_SLOTS
  giOscSlotN = giOscSlotN + 1
  kchg_$uid. changed $kVar
  if (kchg_$uid. == 1 || gkOscRefSlot == iOsl_$uid.) then
    OSC3x_f "$Saddr.", $kVar
  endif
#

; OSC feedback: send integer on change (or in its slot of a refresh sweep)
#define OSC_I(kVar'Saddr'uid)#
  iOsl_$uid. = giOscSlotN % $OSC_REF_SLOTS
  giOscSlotN = giOscSlotN + 1
  kchg_$uid. changed $kVar
  if (kchg_$uid. == 1 || gkOscRefSlot == iOsl_$uid.) then
    OSC3x_i "$Saddr.", $kVar
  endif
#

; MOOD bitcrusher — used by instrs 402 and 405
#define MOOD_BITCRUSH(aInL'aInR'aOutL'aOutR)#
  klen = ftlen(giSemi) - 1
  kIndex = round(gkMOOD_Clock * klen)
  ksemi table kIndex, giSemi
  kTrig changed2 ksemi
  kred_sr init sr
  kbits init 16
  kRandFactor init 1
  if (kTrig == 1 && ksemi < 0) then
    kRandFactor random 0.7, 1.3
  endif
  if (ksemi < 0) then
    kBaseRed = 1
    if (ksemi == -7) then
      kBaseRed = 0.5
    elseif (ksemi == -12) then
      kBaseRed = 0.333
    elseif (ksemi == -19) then
      kBaseRed = 0.25
    elseif (ksemi == -24) then
      kBaseRed = 0.125
    elseif (ksemi == -31) then
      kBaseRed = 0.0625
    endif
    kred_sr = sr * kBaseRed * kRandFactor
    kbits = (ksemi == -7 ? 12 : (ksemi == -12 ? 11 : (ksemi == -19 ? 10 : (ksemi == -24 ? 9 : 8))))
    klevels = pow(2, kbits - 1)
    a_quantL = round($aInL * klevels) / klevels
    a_quantR = round($aInR * klevels) / klevels
    $aOutL SrReducer a_quantL, kred_sr
    $aOutR SrReducer a_quantR, kred_sr
  else
    $aOutL = $aInL
    $aOutR = $aInR
  endif
#

;==============================================================
; FUNCTION TABLES
;==============================================================

; General tables
giHanning ftgen 0, 0, 1024, 20, 2
gisine ftgen 0, 0, 131072, 10, 1

; Granular / pitch-shift window functions
giwfn1 ftgen 0, 0, 131072, 9, 0.5, 1, 0
giwfn2 ftgen 0, 0, 131072, 7, 0, 3072, 1, 128000, 0
giwfn3 ftgen 0, 0, 131072, 16, 0, 3072, 0, 1, 128000, -2, 0
giwfn4 ftgen 0, 0, 131072, 7, 0, 3536, 1, 124000, 1, 3536, 0
giwfn5 ftgen 0, 0, 131072, 7, 0, 128000, 1, 3072, 0
giwfn6 ftgen 0, 0, 131072, 5, 0.001, 128000, 1, 3072, 0.001
giwfn7 ftgen 0, 0, 131072, 20, 2, 1
giwindows ftgen 0, 0, 8, -2, giwfn7, giwfn1, giwfn2, giwfn3, giwfn4

; Granular buffers (standalone granular effect)
giBufL ftgen 0, 0, 1048576, -2, 0
giBufR ftgen 0, 0, 1048576, -2, 0

; TAPE transport buffers — 2^18 samples ≈ 5.46 s @ 48 k. Deliberately bigger
; than the transport needs (~4 s worst-case stop gap): a future granular
; layer reads this same tape (spec §9, 2026-06-11-tape-machine-design.md).
giTapeBufL ftgen 0, 0, 262144, -2, 0
giTapeBufR ftgen 0, 0, 262144, -2, 0

; MOOD dedicated buffers (separate from standalone granular)
giMOOD_BufL ftgen 0, 0, 1048576, -2, 0
giMOOD_BufR ftgen 0, 0, 1048576, -2, 0

; GPS tables and buffers
giGPSbuffer ftgen 0, 0, 1048576, 2, 0
giGPSbufferR ftgen 0, 0, 1048576, 2, 0
giGPSwfn1 ftgen 0, 0, 131072, 9, 0.5, 1, 0
giGPSwfn2 ftgen 0, 0, 131072, 7, 0, 3072, 1, 128000, 0
giGPSwfn3 ftgen 0, 0, 131072, 5, 0.001, 3072, 1, 128000, 0.001
giGPSwfn4 ftgen 0, 0, 131072, 7, 0, 1536, 1, 128000, 1, 1536, 0
giGPSwfn5 ftgen 0, 0, 131072, 7, 0, 128000, 1, 3072, 0
giGPSwfn6 ftgen 0, 0, 131072, 5, 0.001, 128000, 1, 3072, 0.001
giGPSnnsamp = ftlen(giGPSbuffer) -1

; Spectral delay tables
giFFT = 1024  ; Spectral Delay FFT size
giHop = 256   ; LOCKED at 256 (was: ksmps). Decoupled from ksmps so SpecDel
              ; stays at 4× overlap regardless of ksmps. With ksmps=64,
              ; ksmps-tied hop would make pvsanal run 4× more often (16×
              ; overlap, ~4× CPU). Locking giHop keeps SpecDel CPU stable.
giWin = giFFT
giFormat = 1
giSpecDelAmpTab ftgen 300, 0, giFFT, -21, 1, 0.3
giSpecDelFrqTab ftgen 301, 0, giFFT, -21, 1, 0.3

; BINCRUSH spectral bin quantizer (FILTERING s2, toggle 38, instr 121).
; FFT=1024, hop=256 (4× overlap, ~21ms @48k). 46.9 Hz/bin resolves
; E2's harmonics (82–412 Hz) into separate bins — at FFT=256 (187.5 Hz/bin)
; all guitar harmonics collapsed into bins 0-2 making spectral contrast
; completely transparent. hop=256 = 2× ksmps (128) → still valid.
giBinFFT  = 1024
giBinHop  = 256
giBinWin  = 1024
giBinBins = giBinFFT / 2 + 1   ; 513 analysis bins

; HABIT ring buffers. Power-of-2 so phasor wrap is exact and table3 iwrap works.
giHabitMem  ftgen 0, 0, 8388608, 2, 0   ; 2^23 ~= 174.8 s mono memory @48k
giHabitEcho ftgen 0, 0, 4194304, 2, 0   ; 2^22 ~= 87.4 s echo line @48k

; Phaser shape tables
giLog ftgen 0, 0, 4096, 19, 0.5, 1, 0, 0
giExp ftgen 0, 0, 4096, 19, 0.5, 1, 180, 1

; MOOD tables
giSemi ftgen 0, 0, 10, -2, 24, 19, 12, 7, 0, -5, -12, -17, -24, -29
giHalfSine ftgen 0, 0, 131072, 9, 0.5, 1, 0
giPitchBufL ftgen 0, 0, 1048576, 2, 0
giPitchBufR ftgen 0, 0, 1048576, 2, 0

; LOOPER (menu 10, instrs 210-214) — 5-channel loop recorder/player
; 16 s per channel @ 48 kHz = 768000 samples
giLpBuf1L ftgen 0, 0, -2304000, -2, 0
giLpBuf1R ftgen 0, 0, -2304000, -2, 0
giLpBuf2L ftgen 0, 0, -2304000, -2, 0
giLpBuf2R ftgen 0, 0, -2304000, -2, 0
giLpBuf3L ftgen 0, 0, -2304000, -2, 0
giLpBuf3R ftgen 0, 0, -2304000, -2, 0
giLpBuf4L ftgen 0, 0, -2304000, -2, 0
giLpBuf4R ftgen 0, 0, -2304000, -2, 0
giLpBuf5L ftgen 0, 0, -2304000, -2, 0
giLpBuf5R ftgen 0, 0, -2304000, -2, 0
; Varispeed table: index 6 = 1.0x (default)
giLpSpeeds ftgen 0, 0, 8, -2, -2, -1, -0.5, -0.25, 0.25, 0.5, 1, 2

;==============================================================
; GLOBAL CONTROL VARIABLES
;==============================================================

gkPortTime init 0.03
gkPowerMax init 50
gkLastToggle init 0
gkLastToggle29 init 0

;==============================================================
; SOUNDMAGIC SPECTRAL RACK globals (spec: docs/specs/2026-10-04-soundmagic-
; spectral-rack-design.md, contract: csound/spectral/CONTRACT.md).
; giSMP (params, idx = fx*32+p) is filled from giSMPdef / the saved file by
; spectral/sm_persist.orc; giSMPmin/max/def/hero/pedal/fftp come from the
; generated spectral/sm_params.orc. Slot 767 of giSMP and slot 23 of giSMon
; carry the layout stamp. Slot 736 of giSMP (fx 23 p 0) = rack default FFT idx.
;==============================================================
giSMP      ftgen 0, 0, -768, -2, 0
giSMon     ftgen 0, 0, -24, -2, 0
giSMres    ftgen 0, 0, -24, -2, 2     ; resolved FFT index per fx (written by instr 249)
giSMshadow ftgen 0, 0, -768, -2, 1e30 ; OSC change-scan shadow of giSMP
giSMonSh   ftgen 0, 0, -24, -2, -1    ; OSC change-scan shadow of giSMon
giSMPBank  ftgen 0, 0, -13058, -2, 0  ; 17 preset slots x 768; [13056]=save generation, [13057]=file stamp
giSMonBank ftgen 0, 0, -408, -2, 0    ; 17 preset slots x 24 (slot idx 23 = occupied stamp)
giSMpsh    ftgen 0, 0, -768, -2, 0    ; persistence dirty-scan shadow of giSMP
giSMonPsh  ftgen 0, 0, -24, -2, 0     ; persistence dirty-scan shadow of giSMon
gkSMActive     init 0
gkSMDefaultFFT init 2
gkSMBankDirty  init 0                 ; set by preset capture/delete, consumed by instr 250
gaSMChainL init 0
gaSMChainR init 0
gkVOLpedal init 1
gkVOLpedalBypass init 1
; Expression-pedal / preset volume — applied at the FRONT of the chain
; (signal_chain.orc stage 0.7), before every effect, so volume swells bloom the
; reverb/delay tails instead of chopping them. Always applied (unity at 1.0).
; A menu-0 preset routes the expression pedal here by setting lastToggle29 = 30
; (the EXPR-ROUTING system — see the CC29 dispatch in midi_handler.orc); reset
; to 1 on every preset change so volume returns to full.
gkMasterVol  init 1
gkAmbBoost   init 1   ; ambient preset clean-gain boost (2.1x active, 1.0 all others)

; ---- UTILITY PAGE globals (2026-09-01) ----
; gkMasterOut: TRUE end-of-chain master volume, applied as the very LAST gain
; before `outs` (signal_chain.orc), AFTER the amp sim. This is distinct from
; gkMasterVol above (the FRONT/pedal swell volume, before the amps). Master out
; is NEVER on the expression pedal — it is set only from the UTILITY page. 0..1.5
; (unity 1.0), portk-smoothed at the output. Persisted.
gkMasterOut  init 1
; gkManualBpm: the UTILITY page's manual master tempo. Feeds the clock's manual
; fallback (chase_bliss_clock.orc) — the looper and an external MIDI clock still
; OVERRIDE it when present (priority: loop-derived > external CC119 > this). 40..300.
gkManualBpm  init 120
; gkRecState: mirror of the external master-output recorder (demiurge-record),
; for the UI's REC indicator only. 0 = idle, 1 = recording. The UI sets this
; locally when it toggles the recorder; the engine does not record audio itself
; (capture is an out-of-process PipeWire tap so the audio thread never does disk IO).
gkRecState   init 0

; Pitch shift controls (old PS, sig chain pos 50)
gkSemi init 0
gkDryPctPS init 100
gkBypassPS init 1
gkKnobRaw init 1

; H90 (sinusoidal model) controls (sig chain pos 48)
gkBypassH90 init 1
gkSemiH90 init 0
gkH90mix init 1   ; 0=dry, 1=wet (consistent with other effect mixes)

; Pitch Fork clone (mincer-based) controls (sig chain pos 49)
gkBypassPF init 1
; Default +5 (perfect 5th up); quantized to {-12,-9,-7,-5,+5,+7,+9,+12} in effects/pitchfork.orc
gkSemiPF init 5
; Default 50% — user wants a "dry/wet vibe" by default
gkPFmix init 0.5

; Distortion controls
gkBypassDist init 1
gkDistAmtAcc init 0.0
gkDistAmt init 1.0

; Bitcrusher controls
gkBitsBC init 16
gkBypassBC init 1
gkBitsAmtAcc init 0.0

; Delay controls
gkDelayMS init 0
gkFeedback init 0.5
gkDelMix init 0.5
gkBypassDelay init 1
gkSemiDel init -0.01
gkDryPctPSdel init 100
gkBypassPSdel init 0
gkKnobRawDel init 100

; Ring modulator controls
gkARModDepth init 1
gkARModFreq init 0.1
gkBypassRM init 1

; Reverb controls
gkverbBypass init 1
gkverbMix init 1
gkReverbFB init 0.92
gkReverbDamp init 0.9      ; 2026-06-23 damping/brightness on encoder (0=dark,1=bright; 0.9=legacy)

; Granular controls
gkGranBypass init 1
gkGranMix    init 0.5
gkGranDens   init 80    ; grain density grains/sec (10-400)
gkGranSize   init 0.55  ; max grain size seconds (0.05-1.2) — raised 0.4->0.55
                        ; 2026-07-26 with the bigger-grain "chill vibe" rework
gkGranPitch  init 6     ; max pitch scatter semitones (0-24)

; GPS controls
gkGPSamp init 1
gkGPSInGain init 1
gkGPSmix init 1
gkGPSbalance init 1
gkGPSmonostereo init 3
gkGPSfback init 0
gkGPSclip init 1
gkGPSClipLev init 0.5
gkGPSCutoff init 4000
gkGPSLPF_On init 0
gkGPSwsize init 2000
gkGPSrnd init 0
gkGPSsemis init 12
gkGPSolap init 200
gkGPSwfn init 1
gkGPSdly init 0.001
gkGPSbeta init 1
gkGPSfreeze init 0
gkGPSManPtrPort init 0.5
gkGPSManPtr init 0
gkBypassGPS init 1

; Time stretch controls
gkBypassTS init 1
gkTSAmtAcc init 0.0

; Chorus controls
gkBypassChorus init 1
gkChorusMixAcc init 1
gkChorusMix init 0.9
gkChorusRate init 0.7
gkChorusDereg init 0.5
gkChorusDepth init 0.4
gkChorusOffset init 0.02
gkChorusWidth init 1
gkChorusType init 1
gkChorusDepthAcc init 1

; Spectral delay controls
gkSpecDelBypass init 1
gkSpecDelMix init 0.5
gkSpecDelFB init 0.8
gkSpecDelMaxDel init 0.3
gkSpecDelPrev init 1

; BIN REDUCTION controls (spectral contrast exponentiation, toggle 38, instr 121)
gkBinBypass init 1
; EXPRESSION SOURCE EPOCH (2026-10-22): bumped by ch16 CC122 (sent by the
; demiurge-io pool whenever the pedal feeding expression changes) so
; expr_cc29.orc re-arms its soft-takeover instead of slamming the target to the
; other pedal's resting position.
gkExprSrcEpoch init 0
; PEDAL HANDOFF ON PRESET RECALL (2026-11-01). gkExprPos = last CC29 position seen
; (0..1, -1 = pedal not moved since boot), recorded even while soft-takeover has
; the pedal disarmed. gkExprHandoff = the owner id a DUODECIMUS preset recall just
; handed the pedal to (-1 = none); expr_cc29.orc consumes it so that owner change
; does NOT disarm the pedal (the target was set to the pedal's position on recall).
gkDuoNam      init 0    ; DUODECIMUS NAM preset (1/3/5) currently on, 0 = none
gkExprPos     init -1
gkDuoFx       init 0    ; DUODECIMUS: encoder N of the ACTIVE FX preset (0 = CLEAN/none)
gkExprHandoff init -1
gkBinPedal  init 1    ; expr pedal CC29: 1 = toe = transparent; 0 = heel = extreme crush

; BINCRUSHER controls (spectral bin coarsener, toggle 44, instr 122)
gkBcBypass init 1
gkBcPedal  init 0     ; expr pedal CC29: 0 = heel = no coarsening; 1 = toe = max coarsening

; HABIT (echo collector, toggle 36, instr 64) controls
gkHabitBypass  init 1
gkHabitMix     init 0.5
gkHabitSize    init 0.35     ; normalized; 0.05*1200^x s -> ~0.6 s
gkHabitRepeats init 0.45
gkHabitScan    init 0        ; expression pedal CC29
gkHabitBlend   init 1        ; 0=smooth long-grain delay, 1=full granular (default: current behavior)

; Glitch shared controls
gkGLITCH_bpm init 95
gkGLITCH_chance init 0.25

; --- Master clock coordination (external DEMIURGE CC119 vs loop-derived) ---
; gkGLITCH_bpmExternal: raw external broadcast, ALWAYS mirrors ch16 CC119
;   (midi_handler.orc instr 99), regardless of loop/authority state. This is
;   the "what is DEMIURGE's system tempo right now" source of truth and is
;   never silently dropped.
; gkGLITCH_extBpmEdge: 1 for exactly the k-block a genuinely NEW CC119 value
;   arrives (set via `changed` in instr 99), 0 otherwise. Consumed by instr
;   100 (chase_bliss_clock.orc) the same k-block, since 100 > 99 runs after.
; gkChaseBlissClockSource: which source currently drives gkGLITCH_bpm —
;   0 = external DEMIURGE clock, 1 = loop-derived (gkChaseBlissBPM). Set in
;   instr 100; inspectable/debuggable, could be surfaced via OSC/UI later.
gkGLITCH_bpmExternal     init 95
gkGLITCH_extBpmEdge      init 0
gkChaseBlissClockSource  init 0
; gkGLITCH_extBpmSeen: latched 1 the first time a real external CC119 tempo
;   broadcast lands (a genuine value CHANGE — see instr 99). Distinguishes
;   "a real external clock is attached" from "gkGLITCH_bpmExternal just sits
;   at its init default because nothing is plugged in". Consumed by
;   looper.orc instr 209 to gate CLOCKED vs UNCLOCKED capture (2026-07-29
;   Pocket OpGorator integration, spec §4). Once latched it stays latched for
;   the process lifetime — a clock that briefly disconnects mid-performance
;   should not suddenly start baking unquantized silence into new loops.
gkGLITCH_extBpmSeen      init 0

; gkClkExtLive (2026-10-26): 1 while an external clock is ACTUALLY driving us
;   right now = a /clk/bpm from pmor-clock-relay within the last
;   $CLK_EXT_FRESH s (it sends every 0.5 s, only while PMOR is plugged in,
;   clocking and tempo-locked). Replaces gkGLITCH_extBpmSeen as the gate for
;   CLOCKED loop capture and external tempo authority. Why: demiurge-clock in
;   the opgorator role keeps broadcasting clock + CC114=127 + its last tempo
;   after PMOR is unplugged, so the latch said "external clock" with nothing
;   there -> the first loop was quantized to a phantom grid and the close
;   waited for a beat nobody was playing (EMGOR 2026-10-26). Priority now:
;   live PMOR > recorded loop (subdivision search, LoopImpliedBpm) > UTILITY
;   manual BPM. Written once per block by midi_handler.orc instr 99.
gkClkExtLive             init 0

; gkCommittedBpm (2026-10, VARISPEED-WARBLE FIX): the DELIBERATE tempo — the
; tempo the instrument has decided to be at — as opposed to gkGLITCH_bpm,
; which is the CONTINUOUS arbitrated estimate (loop-derived or external,
; portk-glided for audible handoffs, but still carrying PLL jitter from a
; tracked external clock on every single k-block). gkGLITCH_bpm is correct
; for anything that wants "the current live tempo, smoothly" — the GLITCH/
; stutter.orc family, beat-grid quantization (gkLpBeatSamples). It is WRONG
; for anything that rate-scales already-recorded, sustained audio: scaling a
; playing loop's read-speed by a noisy ratio is a continuous PITCH modulation
; (roughly 0.4% rate error ~= 7 cents), audible as warble/flutter, and portk
; does not fix this — it just turns fast jitter into slow, arguably-worse
; warble. effects/looper.orc's varispeed ratio is the only consumer of
; gkCommittedBpm; everything else (beat-close quantization, GLITCH/stutter)
; keeps reading gkGLITCH_bpm, unchanged — see effects/looper.orc's "TWO
; TEMPOS, TWO JOBS" note and chase_bliss_clock.orc instr 100's "COMMITTED
; TEMPO" block for the full derivation.
;
; COMMIT RULE — gkCommittedBpm updates ONLY on a deliberate decision, never
; from ordinary PLL tracking of a steady clock, computed in instr 100
; (chase_bliss_clock.orc, the file that already owns tempo arbitration):
;
;   1. LOOP CLOSE (immediate, no deadband/dwell needed — this is already a
;      single discrete event, not a continuous estimate): the instant the
;      master loop (ch1) closes (gkLpMasterLen's 0 -> >0 edge), commit
;      gkChaseBlissBPM (the loop-implied tempo) straight in.
;   2. GENUINE EXTERNAL RETUNE (human tap-tempo / Ableton Link / any CC118
;      steer landing at the daemon — all of which surface here identically,
;      as a move in gkGLITCH_bpmExternal — vs. ordinary PLL estimation
;      noise on a steady clock): gated by a DEADBAND + DWELL, applied only
;      while external is the currently-authoritative source (so a broadcast
;      arriving while a loop holds authority can't silently retune a loop
;      that isn't even following it):
;        - DEADBAND 0.5 BPM: at 124 BPM, 1 cent of pitch error is only
;          ~0.07 BPM (per-beat MIDI-clock PLL jitter after the daemon's own
;          24-sample trimmed-mean, src/demiurge-clock-pll.h, lands well under
;          1 cent in steady state) — so 0.5 BPM sits roughly a factor of 7
;          above that noise floor, comfortably filtering it out, while still
;          being far smaller than the smallest tempo change a musician would
;          deliberately dial in (nobody taps a tempo knob to move it by half
;          a BPM on purpose; real retunes are whole BPMs or more).
;        - DWELL 0.5 s: roughly one beat across this rig's loop-tempo target
;          band (80-160 BPM => 0.375-0.75 s/beat, chase_bliss_clock.orc
;          ALGORITHM header) and comfortably longer than the daemon's own
;          24-tick/24-PPQN trimmed-mean window at any tempo in that band —
;          so by the time the dwell elapses the PLL's own averaging has
;          already had a full cycle to settle, meaning what we sample at
;          commit time is itself already a converged estimate, not a single
;          hot sample. Long enough to reject a one-off transient (a single
;          bad/outlier tick that slips the PLL's gate), short enough that a
;          real retune is locked in well inside one bar.
;   3. A plain gkManualBpm (UTILITY-page CC104) edit is deliberately NOT a
;      commit source here — it already has its own role (the no-loop
;      gkChaseBlissBPM fallback above) and folding it in here too would let
;      spinning that encoder for a LATER take silently retune a loop that is
;      currently playing at a different, unrelated tempo. Not one of the
;      three sources this task specified; left out rather than inventing a
;      fourth mechanism.
;
; Starts at 120 (matches gkChaseBlissBPM's/this rig's documented no-loop
; center default) until the first real commit above overwrites it.
gkCommittedBpm init 120

; Glitch hold/click state (shared mechanics for all 4 glitch toggles)
; _held    = 1 while button physically held down (forces 100% chance)
; _pressT  = perf-time when button went down (for hold-vs-click classification)
; _prevByp = bypass state captured at moment of press (restored on short click)
giGLITCH_holdThresh init 0.30   ; seconds — shorter than this = click

gkBBcut_held   init 0
gkBBcut_pressT init 0
gkBBcut_prevByp init 1

gkWGuide_held   init 0
gkWGuide_pressT init 0
gkWGuide_prevByp init 1

gkSqMod_held   init 0
gkSqMod_pressT init 0
gkSqMod_prevByp init 1

gkFShift_held   init 0
gkFShift_pressT init 0
gkFShift_prevByp init 1

; BBcut (beat-cutter) controls
gkBBcut_bypass init 1
gkBBcut_drywet init 1.0
gkBBcut_fltMix init 0.6

; WGuide (waveguide glitch) controls
gkWGuide_bypass init 1
gkWGuide_drywet init 1.0
gkWGuide_fb init 0.95      ; 2026-06-23 waveguide feedback on encoder (0.90..0.998)

; SqMod (square modulation) controls
gkSqMod_bypass init 1
gkSqMod_drywet init 1.0
gkSqMod_freq init 9        ; 2026-06-23 mod-rate centre on encoder (4..14 oct)

; FreqShift (frequency shifter) controls
gkFShift_bypass init 1
gkFShift_drywet init 1.0
gkFShift_amount init 0     ; 2026-06-23 shift centre on encoder (-2400..2400 cents)

; AutoMoog controls
gkAUTOMOOG_bypass init 1
gkAUTOMOOG_sensAcc init 0.8
gkAUTOMOOG_sens init 0.8
gkAUTOMOOG_att init 0.005
gkAUTOMOOG_rel init 0.15
gkAUTOMOOG_freq init 200
gkAUTOMOOG_type init 2
gkAUTOMOOG_res init 0.85
gkAUTOMOOG_dist init 0
gkAUTOMOOG_level init 1

; WAH controls
gkWAH_bypass init 1
gkWAH_freq init 0.5
gkWAH_res init 0.7

; Vibrato controls
gkVIB_bypass init 1
gkVIB_depthAcc init 0.0667
gkVIB_depth init 0.004
gkVIB_freq init 1.2
gkVIB_fback init 0

; Flanger controls
gkFLA_bypass init 1
gkFLA_depthAcc init 0.5
gkFLA_depth init 0.005
gkFLA_rate init 2
gkFLA_offset init 0.0001
gkFLA_fback init 0.2
gkFLA_lfoshape init 4
gkFLA_mix init 1

; Phaser controls
gkPHA_bypass init 1
gkPHA_depthAcc init 0.5
gkPHA_depth init 0.5
gkPHA_rate init 2
gkPHA_freq init 0.1
gkPHA_fback init 0.2
gkPHA_stages init 8
gkPHA_mix init 1
gkPHA_shape init 8

; Leslie controls
gkLeslieBypass init 1
gkLeslieDepthAcc init 0
gkLeslieDepth init 1
gkLeslieSpeedAcc init 0
gkLeslieSpeed init 7.5

; MOOD controls
gkMOOD_Bypass init 1
gkMOOD_MixAcc init 0.5
gkMOOD_Mix init 0.5
gkMOOD_Clock init 0
gkMOOD_Dens init 200
gkMOOD_Dly init 0.7

; Formant controls
gkFormantBypass init 1
gkFormantShift init 0
gkFormantMix init 1

; FORMS controls (clean formant shifter, pvswarp-based)
gkFormsBypass init 1
gkFormsShift init 0
gkFormsMix init 1

; Attack-shaper controls (v2 POG-style swell, 2026-07-03)
; defaults = picked-guitar sweet spot (bench SCN0-validated); toggle-on
; also resets to these in midi_handler so it always comes on sounding right
gkATTK_bypass init 1
gkATTK_mix init 1.0
gkATTK_atkMs init 150
gkATTK_sens init 0.5
gkATTK_shape init 0.5
gkATTK_range init 0.7
gkATTK_retrigMs init 45
gkATTK_trigCount init 0    ; onset counter (bench/debug telemetry)
gkATTK_gain init 1         ; live envelope gain (bench/debug telemetry)

; TAPE machine controls (Gen Loss-style; replaces DROPOUT at cutover, toggle 21)
; AMOUNT = saturation + generative spikes + drift macro. FAIL = drop/crinkle
; density. MODEL = voicing morph reel(0)->cassette(0.5)->broken(1).
; EXPR = CC29 transport: 0 = heel = full speed, 1 = toe = stopped.
gkTapeBypass init 1
gkTapeAmount init 0.4
gkTapeFail   init 0.25
gkTapeModel  init 0.35
gkTapeExpr   init 0

; Tremolo controls (managed by preset system)
gkTremBypass init 1
gkTremDepth  init 0.7        ; surf-rock baseline: noticeable throb, not too deep — swings 0.30 to 1.0
gkTremRate   init 7.38       ; Hz — classic surf trem (75% of the previous 9.84)

; Preset system
; gkCurrentPreset: -1=dry, 1=tubescreamer (leftmost), 3=ambient/reverb (rightmost)
; Controlled exclusively by menu 0 (PRESETS) in midi_handler.orc.
; (Old 0=clean / 2=dirty channels removed; CLEAN folded into the mixable TS channel.)
gkCurrentPreset init -1

; MAIN PREAMP (Hudson Broadcast) — now toggleable (was inherent). 0=ON, 1=OFF.
; Default OFF; surf presets re-enable it, the PREAMP menu toggles it.
gkPreamp_bypass init 1

; PRESET BANK: 0=surf, 1=bass. Selects which PRESETS-menu bank is live.
; Seeded from the PRESET_BANK macro (defined in neptrPhase4.csd; launcher overrides
; via -+omacro:PRESET_BANK=1).
gkPresetBank init $PRESET_BANK

; USER PRESETS (menu 0, 2026-10-24): five slots = bank slots 1..5, the same
; store the DUODECIMUS foot uses. Gesture state lives here so handler_menus.orc
; (writer), midi_handler.orc (/phold) and state_persist.orc (loaded/occupancy)
; all see it.
gkPrLoadedUi init 0   ; onboard index 1..5 of gkPrLoaded for /ploaded, 0 = foot slot
gkPgHeld     init 0   ; slot (1..5) whose button is down, 0 = none
gkPgT0       init 0   ; performance time (s) of that press
gkPrLoaded   init 0   ; bank slot last recalled/captured (0 = none)
gkPrOccMask  init 0   ; bit (N-1) set = bank slot N saved (slots 1..5)

; INPUT MODE: 1 = TRUE STEREO (in1 -> L, in2 -> R, preserved through the whole
; chain), 0 = legacy MONO SUM ((in1+in2)*0.5 to both sides). Seeded from the
; STEREO_IN macro; flip per set via csound_extra --omacro:STEREO_IN=0.
; NOTE: in stereo mode a single instrument plugged into one input leaves the
; other channel silent — use MONO SUM for one-source rigs.
#ifndef STEREO_IN
#define STEREO_IN #1#
#endif
gkStereoIn init $STEREO_IN

; Pitch-shifter clampable range — all default to full ±12 st so normal
; toggle behaviour is unchanged. Bass presets override per-shifter as needed
; (e.g. H90 wammy: gkH90_min=0, gkH90_max=12 = up-only).
gkH90_min init -12
gkH90_max init  12
gkPF_min  init -12
gkPF_max  init  12
gkGPS_min init -12
gkGPS_max init  12
gkPS_min  init -12
gkPS_max  init  12

; Bass preamps (radio pair) + bass distortion — PREAMP menu, toggles 40/41/42.
gkBPL_bypass init 1
gkBPL_drive  init 0.25
gkBPH_bypass init 1
gkBPH_drive  init 0.55
gkBDS_bypass init 1
gkBDS_drive  init 0.6
gkBDS_blend  init 0.35
gkBDS_tone   init 0.5
gkTotalGlitch        init 1    ; 0=glitch bundle ON, 1=off
gkTotalGlitch_bpmSav init 95   ; saved gkGLITCH_bpm before 3x boost
gaBPLSendL init 0
gaBPLSendR init 0
gaBPLOutL  init 0
gaBPLOutR  init 0
gaBPHSendL init 0
gaBPHSendR init 0
gaBPHOutL  init 0
gaBPHOutR  init 0
gaBDSSendL init 0
gaBDSSendR init 0
gaBDSOutL  init 0
gaBDSOutR  init 0

; IR VERB (menu 12, instr 240) audio buses — the gk/gi IR state lives in
; effects/ir_reverb.orc; only the chain buses are declared here (the effect
; file omits them and instr code cannot create a-rate globals).
gaIRSendL init 0
gaIRSendR init 0
gaIRWetL  init 0
gaIRWetR  init 0

;==============================================================
; PRESET BASE LAYER — vibrato + spring delay shared by ALL menu-0 presets.
;
; UNIFIED, single source of truth: every preset applies THESE values (not the
; per-effect init defaults above). This keeps preset voicing consistent and
; deliberately DISTINCT from the manual-toggle defaults — turning vibrato or
; delay on from their own menus uses gkVIB_depth=0.004 / gkDelMix=0.5 (the init
; values above); presets use the giPreset* values below.
;
;   - Vibrato preset depth = base 0.00115 + 10%  -> 0.001265
;   - Spring-delay preset wet = base 0.09  + 20%  -> 0.108
;==============================================================
giPresetVibFreq  init 0.5796     ; slow, shallow chorale vibrato
giPresetVibDepth init 0.001265   ; preset vib depth (base 0.00115 + 10%)
giPresetVibFback init 0
giPresetDelMS    init 60563       ; ~60.6 ms spring tap (just under 32nd @ 120 BPM)
giPresetDelFB    init 0.62
giPresetDelMix   init 0.108       ; preset delay wet (base 0.09 + 20%)

; PER-PRESET OUTPUT TRIM (2026-07-13) — one linear multiplier per preset id,
; indexed by gkCurrentPreset (0..15; surf 1/2/3, bass 10/11/12/13/14). Default
; 1.0 (unity). Adjusted ONLY in menu 0 via CC20 (±6 dB, 0.5 dB steps), applied
; at the master-vol stage in signal_chain.orc, mirrored on OSC /preset_trim.
; SESSION-ONLY: the table lives in RAM; cross-boot persistence is a future item.
giPresetTrim ftgen 0, 0, 16, -2, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
gkPresetTrim init 1

;==============================================================
; GLOBAL AUDIO BUSES
;==============================================================

; Pitch shift (old PS, instr 50)
gaPSSendL init 0
gaPSSendR init 0
gaPSOutL init 0
gaPSOutR init 0

; H90 (instr 48)
gaH90SendL init 0
gaH90SendR init 0
gaH90OutL init 0
gaH90OutR init 0

; Pitch Fork (instr 49)
gaPFSendL init 0
gaPFSendR init 0
gaPFOutL init 0
gaPFOutR init 0

; Distortion
gaDistSendL init 0
gaDistSendR init 0
gaDistOutL init 0
gaDistOutR init 0

; Time stretch
gaTSSendL init 0
gaTSSendR init 0
gaTSOutL init 0
gaTSOutR init 0

; Delay
gaDelSendL init 0
gaDelSendR init 0
gaDelOutL init 0
gaDelOutR init 0
gaDelWetL init 0
gaDelWetR init 0

; Ring modulator
gaRMSendL init 0
gaRMSendR init 0
gaRMOutL init 0
gaRMOutR init 0

; Reverb
gaVerbSendL init 0
gaVerbSendR init 0
gaVerbWetL init 0
gaVerbWetR init 0

; Granular
gaGranSendL init 0
gaGranSendR init 0
gaGranWetL init 0
gaGranWetR init 0

; GPS
gaGPSSendL init 0
gaGPSSendR init 0
gaGPSWetL init 0
gaGPSWetR init 0
gaGPSFBackSig init 0
gaGPSFBackSigR init 0
gaphsW init 0

; Chorus
gaChorusSendL init 0
gaChorusSendR init 0
gaChorusWetL init 0
gaChorusWetR init 0

; Spectral delay
gaSpecDelSendL init 0
gaSpecDelSendR init 0
gaSpecDelWetL init 0
gaSpecDelWetR init 0

gaHabitSendL init 0
gaHabitSendR init 0
gaHabitWetL  init 0
gaHabitWetR  init 0

; BIN REDUCTION
gaBinSendL init 0
gaBinSendR init 0
gaBinWetL  init 0
gaBinWetR  init 0

; BINCRUSHER
gaBcSendL init 0
gaBcSendR init 0
gaBcWetL  init 0
gaBcWetR  init 0

; AutoMoog
gaAUTOMOOG_sendL init 0
gaAUTOMOOG_sendR init 0
gaAUTOMOOG_outL init 0
gaAUTOMOOG_outR init 0

; Vibrato
gaVIB_sendL init 0
gaVIB_sendR init 0
gaVIB_outL init 0
gaVIB_outR init 0

; Flanger
gaFLA_sendL init 0
gaFLA_sendR init 0
gaFLA_outL init 0
gaFLA_outR init 0

; Phaser
gaPHA_sendL init 0
gaPHA_sendR init 0
gaPHA_outL init 0
gaPHA_outR init 0

; Leslie
gaLeslieSendL init 0
gaLeslieSendR init 0
gaLeslieOutL init 0
gaLeslieOutR init 0

; MOOD
gaMOOD_SendL init 0
gaMOOD_SendR init 0
gaMOOD_PitchL init 0
gaMOOD_PitchR init 0
gaMOOD_Bit1L init 0
gaMOOD_Bit1R init 0
gaMOOD_GranL init 0
gaMOOD_GranR init 0
gaMOOD_FinalL init 0
gaMOOD_FinalR init 0

; Formant
gaFormantSendL init 0
gaFormantSendR init 0
gaFormantOutL init 0
gaFormantOutR init 0

; FORMS (clean formant shifter)
gaFormsSendL init 0
gaFormsSendR init 0
gaFormsOutL init 0
gaFormsOutR init 0

; Attack-shaper
gaATTK_sendL init 0
gaATTK_sendR init 0
gaATTK_outL init 0
gaATTK_outR init 0


; Tremolo
gaTremSendL init 0
gaTremSendR init 0
gaTremOutL  init 0
gaTremOutR  init 0

; WAH
gaWAH_sendL init 0
gaWAH_sendR init 0
gaWAH_outL init 0
gaWAH_outR init 0

; BBcut
gaBBcut_sendL init 0
gaBBcut_sendR init 0
gaBBcut_wetL init 0
gaBBcut_wetR init 0

; WGuide
gaWGuide_sendL init 0
gaWGuide_sendR init 0
gaWGuide_wetL init 0
gaWGuide_wetR init 0

; SqMod
gaSqMod_sendL init 0
gaSqMod_sendR init 0
gaSqMod_wetL init 0
gaSqMod_wetR init 0

; FreqShift
gaFShift_sendL init 0
gaFShift_sendR init 0
gaFShift_wetL init 0
gaFShift_wetR init 0

;==============================================================
; STEP SIZE VARIABLES
;==============================================================

gkStepReverbFB init 0.02
gkStepGranMix init 0.02
gkStepGPSdly init 0.1
gkStepGPSfback init 0.1
gkStepGPSMix init 0.02
gkStepTSAmt init 0.01
gkStepChorusMix init 0.03
gkStepChorusRate init 0.05
gkStepChorusDepth init 0.05
gkStepFeedback init 0.02
gkStepSpecDelMaxDel init 0.02
gkStepGLITCH_chance init 0.01
gkStepSemiDel init 0.5
gkStepSpecDelFB init 0.02

gkStepHabitMix     init 0.02
gkStepHabitSize    init 0.01   ; ~7.3%/tick exp sweep, full range in 100 ticks
gkStepHabitRepeats init 0.02
gkStepHabitBlend   init 0.02
gkStepGLITCH_bpm init 2
gkStepBBcut_drywet init 0.02
gkStepWGuide_drywet init 0.02
gkStepSqMod_drywet init 0.02
gkStepFShift_drywet init 0.02
gkStepVIB_fback init 0.02
gkStepDelMix init 0.02
gkStepSpecMix init 0.02
gkPSdryPctStep init 0.5
gkKnobRawStep init 0.01
gkBitsStep init 1
gkStepverbMix init 0.02
gkStepDistAmt init 0.01
gkStepBitsAmt init 0.05
gkStepAUTOMOOG_sens init 0.02
gkStepVIB_depth init 0.02
gkStepFLA_depth init 0.02
gkStepFLA_rate init 0.1
gkStepFLA_fback init 0.04
gkStepPHA_depth init 0.02
gkStepPHA_rate init 0.1
gkStepPHA_fback init 0.01
gkStepLeslieDepth init 0.02
gkStepLeslieSpeed init 0.01
gkStepMOOD_Mix init 0.02
gkStepMOOD_Dens init 100
gkStepMOOD_Dly init 0.1
gkStepATTK_sens init 0.02
gkStepATTK_shape init 0.02
gkStepATTK_range init 0.02
gkStepATTK_retrig init 5

;==============================================================
; MENU SYSTEM VARIABLES
;==============================================================


; ---- FOOT MENU CYCLE ORDER (2026-09-25) ------------------------------------
; EVERY menu is still in the cycle - this only REORDERS it so the five that
; get used on stage come LAST, in the requested order:
;
;   ... all the others ... -> 15 CHASE BLISS -> 14 LOOPER -> 16 GLITCH
;                          -> 17 NAM -> 18 IR VERB -> wraps to 0 PRESETS
;
; So menu-PREV from PRESETS lands straight on IR VERB and walks back up the
; performance group, which is the fast way to reach them mid-song.
; HYPERGUITAR (21) is appended only while the guitar is present - nav.orc
; sizes the cycle as 21 + gkHgPresent, so it is simply not walked otherwise.
#include "handler/gen/menu_cycle.orc"

gkCurrentMenu init 0
gkMenuChanged init 0

;==============================================================
; NAM AMP SIM GLOBALS (Menu 8)
;==============================================================

gkNAMBypass      init 0        ; default ON at boot (DELUXE clean); slot button toggles as usual
gkNAMProfileIdx  init 5        ; boot profile: 21_deluxe_reverb_clean (row 1 base + variant 0)
; Amp sim is 100% wet, always — no dry/wet mix knob (removed 2026-07-27).
; Gain staging instead: IN trims drive into the model, OUT trims level after.
gkNAMInGain      init 0        ; input gain, dB (-12..+18), pre-NAMProcess, portk-smoothed
gkNAMOutGain     init 0        ; output gain, dB (-18..+18), post-NAMProcess, portk-smoothed
; NAM_DIR macro override (VST build points this at the repo's profile set;
; hardware never defines it -> Pi path unchanged).
; NOTE: the literal below is a static fallback only — a normal
; launch (neptr/scripts/launch_phase4.sh) always overrides this
; macro with --omacro:NAM_DIR=$HOME/... so it matches whatever Pi
; user is actually running. This default only fires on a manual,
; direct `csound` invocation that bypasses that script.
#ifndef NAM_DIR
#define NAM_DIR #/home/gorpi7/demiurge/profiles/nam#
#endif
gSNAMLibDir      = "$NAM_DIR"
; NAM quality (UTILITY page, 2026-10-23): chn "nam_size" 0 = FULL (default),
; 1 = MICRO (smallest SlimmableContainer submodel). Read in signal_chain.orc and
; passed to NAMProcess, whose loader thread reloads at the new size (never the
; audio thread). chn_k's default is only a host hint - the channel starts at 0,
; so the default is also set explicitly. Rig-level: excluded from presets.
chn_k "nam_size", 3, 2, 0, 0, 1
chnset 0, "nam_size"
giNAMProfileMax  init 24       ; 0-based max flat index; update if profiles added

; --- Amp ROWS (2026-07-03): each UI slot is an expandable row of variants,
; ordered soft -> dirty. Files are named <row+1><variant+1>_name.nam
; (11_, 12_, 21_, ...) so the plugin's sorted flat index is row-major.
; TO ADD AN AMP to row r: drop the file in with the next variant digit,
; bump giNAMRowCount[r] here, add its calibrated line to nam_pregain.conf
; (run csound/nam/testing/calibrate_pregains.sh), done.
; Rows: 0=AC30  1=DELUXE  2=FENDER  3=TWEED  4=CRUNCH
giNAMRowBase[]  fillarray 0, 5, 10, 15, 20 ; flat index of each row's first variant
giNAMRowCount[] fillarray 5, 5, 5, 5, 5    ; variants per row
gkNAMRowSel[]   init 5                     ; remembered variant per row (0 = softest)
gkNAMActiveRow  init 1                     ; which row is ON (-1 = none); boots on row 1 = DELUXE

; LOOPER state (menu 10, instrs 210-214)
; State: 0=idle, 1=recording, 2=playing, 3=overdubbing
gkLpState1 init 0
gkLpState2 init 0
gkLpState3 init 0
gkLpState4 init 0
gkLpState5 init 0
; Recorded lengths (in samples; 0 = nothing recorded yet)
gkLpLen1   init 0
gkLpLen2   init 0
gkLpLen3   init 0
gkLpLen4   init 0
gkLpLen5   init 0
; DRUM BROWSE GESTURE (2026-09-25): 0 = OFF. On the LOOPER page, rotating a
; channel's slot encoder while that channel is EMPTY used to browse/load a drum
; loop into it (drumloops.orc instr 216). The UI's sliders and DUODECIMUS send
; the same rotation CCs, so drum loops kept getting loaded by accident mid-set.
; With this 0 an empty channel's encoder does nothing; loops recorded normally
; are unaffected. Set to 1 to get the gesture back. The drum engine itself is
; untouched.
giDrumBrowse init 0

; PEDAL CLAIMS BY GENERATIVE EFFECTS (2026-09-25): 0 = OFF. BLOOPER (56) and the
; five GLITCH-menu effects (IMPULSE 48, BBCUT 20, REPEAT 23, CHOP 24, RVRS 25)
; used to claim the expression pedal on every toggle-ON, so pressing any of
; them mid-song silently took the pedal off volume/swell - and they are the
; effects you stab at most. With this 0 they leave gkLastToggle29 alone: the
; pedal stays on whatever last claimed it that actually uses it. The claim
; sites in handler/dispatch/toggle_slot*.orc are guarded, not deleted.
giPedalClaimGenerative init 0

; TAPE NOISE FLOOR (2026-09-25): 0 = no hiss, no motor hum. tape.orc still
; computes them (trivial cost) and multiplies by this; 1 restores the old
; noise floor. Crackle/dropouts are unaffected - they are the FAIL character.
giTapeNoise init 0
gkLpMasterLen init 0   ; ch1 length — grid-snap base for ch2-5

; CHASE BLISS SHARED MASTER CLOCK / SUBDIVISION (computed in instr 100,
; csound/chase_bliss_clock.orc — declared here, in globals.orc, so every
; consumer file — blooper.orc, onward.orc, mood2.orc, and midi_handler.orc's
; gkGLITCH_bpm — sees a valid value from k-cycle 1 regardless of #include
; order. See chase_bliss_clock.orc header for the full derivation/algorithm
; and the gkGLITCH_bpm master-clock-takeover rule.
gkChaseBlissBPM       init 120       ; implied tempo of gkLpMasterLen (or default)
gkChaseBlissSubdivSec init 0.25      ; shared subdivision tick, seconds (120 BPM 1/8 default;
                                      ; updated 2026-07-15 per chase_bliss_clock.orc INTEGRATION
                                      ; NOTES item 1 — instr 100 overwrites this on block 1 anyway)
; Write positions (sample index, used during recording)
gkLpWrPos1 init 0
gkLpWrPos2 init 0
gkLpWrPos3 init 0
gkLpWrPos4 init 0
gkLpWrPos5 init 0
; Read positions (sample index, used during play/overdub)
gkLpRdPos1 init 0
gkLpRdPos2 init 0
gkLpRdPos3 init 0
gkLpRdPos4 init 0
gkLpRdPos5 init 0
; Speed table indices (6 = 1.0x forward by default)
gkLpSpIdx1 init 6
; true per-channel playback rate (speed table x tempo trim), set by looper.orc,
; read by blooper.orc so its per-block re-sync never saws against the looper (2026-10-24)
gkLpEffSpd1 init 1
gkLpEffSpd2 init 1
gkLpEffSpd3 init 1
gkLpEffSpd4 init 1
gkLpEffSpd5 init 1
; loop re-lock to PMOR's beat (midi_handler.orc /clk/beat): switch, offset, live error
#define LP_RELOCK_MAX #0.125#
chn_k "lp_relock", 3, 2, 1, 0, 1
chnset 1, "lp_relock"
chn_k "lp_relock_ofs_ms", 3, 2, 0, -50, 50
gkLpRelockErr init 0
; BEAT LOCK (2026-11-05, looper.orc / midi_handler.orc instr 99):
;   gkLpB      PMOR beat counter in beats, integrated per block from the smoothed
;              /clk/bpm (gkLpBpmS) and pinned to PMOR's real beat by /clk/beat
;   gkLpBLock  1 while loops derive their read position from gkLpB
;   gkLpMasterB master loop length in beats (0 = unknown)
;   per channel: gkLpBOn (locked now), gkLpBHave (recB/lenB valid), gkLpRecB
;   (gkLpB at loop position 0, an exact integer beat of PMOR's ABSOLUTE count),
;   gkLpArmB (record-start armed until gkLpB reaches this beat; 0 = not armed), gkLpLenB (loop length in beats)
gkLpB init 0
#define LP_RECQ_LATE #0.15#   ; record press this far (beats) after a beat starts at once; later waits for the next beat
gkLpBpmS init 0
gkLpBLock init 0
gkLpMasterB init 0
gkLpBOn1 init 0
gkLpArmB1 init 0
gkLpBHave1 init 0
gkLpRecB1 init 0
gkLpLenB1 init 0
gkLpBOn2 init 0
gkLpArmB2 init 0
gkLpBHave2 init 0
gkLpRecB2 init 0
gkLpLenB2 init 0
gkLpBOn3 init 0
gkLpArmB3 init 0
gkLpBHave3 init 0
gkLpRecB3 init 0
gkLpLenB3 init 0
gkLpBOn4 init 0
gkLpArmB4 init 0
gkLpBHave4 init 0
gkLpRecB4 init 0
gkLpLenB4 init 0
gkLpBOn5 init 0
gkLpArmB5 init 0
gkLpBHave5 init 0
gkLpRecB5 init 0
gkLpLenB5 init 0
gkLpSpIdx2 init 6
gkLpSpIdx3 init 6
gkLpSpIdx4 init 6
gkLpSpIdx5 init 6
; Gesture timing (press timestamp, last tap timestamp, dbl-tap fired flag)
gkLpPressT1  init -1
gkLpPressT2  init -1
gkLpPressT3  init -1
gkLpPressT4  init -1
gkLpPressT5  init -1
gkLpLastTap1 init -10
gkLpLastTap2 init -10
gkLpLastTap3 init -10
gkLpLastTap4 init -10
gkLpLastTap5 init -10
gkLpDblFired1 init 0
gkLpDblFired2 init 0
gkLpDblFired3 init 0
gkLpDblFired4 init 0
gkLpDblFired5 init 0
gkLpConsumed1 init 0   ; press already performed the action; release is a no-op
gkLpConsumed2 init 0
gkLpConsumed3 init 0
gkLpConsumed4 init 0
gkLpConsumed5 init 0
gkLpTarget1 init 0      ; sync-close boundary (state 5 records until WrPos hits this)
gkLpTarget2 init 0
gkLpTarget3 init 0
gkLpTarget4 init 0
gkLpTarget5 init 0
gkLpClock   init 0      ; master transport: samples since engine start, never resets
gkLpAnchor1 init 0      ; clock value at each channel's record start
gkLpAnchor2 init 0
gkLpAnchor3 init 0
gkLpAnchor4 init 0
gkLpAnchor5 init 0
; Grid-true capture (2026-07-29 Pocket OpGorator integration, spec §4):
; gkLpGridAvail   — 1 when a tempo grid exists this k-block (see looper.orc
;                    instr 209); gates CLOCKED (whole-beat quantize) vs
;                    UNCLOCKED (trailing-silence trim) capture.
; gkLpBeatSamples — sr*60/gkGLITCH_bpm, only meaningful while grid available.
; gkLpTrimPos{N}  — backward-scan cursor for the UNCLOCKED trim (state 6).
gkLpGridAvail   init 0
gkLpBeatSamples init 0
gkLpTrimPos1 init 0
gkLpTrimPos2 init 0
gkLpTrimPos3 init 0
gkLpTrimPos4 init 0
gkLpTrimPos5 init 0
; VARISPEED-TO-TEMPO (2026-10, TASK 1; revised 2026-10 VARISPEED-WARBLE FIX):
; the COMMITTED tempo (gkCommittedBpm, see its own header comment above and
; chase_bliss_clock.orc instr 100's "COMMITTED TEMPO" block), captured at the
; instant each channel's loop length was finalized — i.e. whatever
; gkCommittedBpm read at that close (sync-close/trim-close/buffer-full/
; immediate-close; see effects/looper.orc + the LP_QCLOSE macro in
; handler/handler_looper.orc for every capture site). 0 = never captured (no
; loop closed yet), in which case looper.orc's varispeed ratio falls back to
; 1.0x (no-op).
;
; DELIBERATELY NOT gkGLITCH_bpm (the continuous arbitrated estimate) any
; more: that value carries live PLL jitter from a tracked external clock on
; every k-block, and rate-scaling sustained recorded audio by a noisy ratio
; is an audible pitch warble (portk glide does not fix this — it only turns
; fast jitter into slow warble). gkCommittedBpm only moves on a deliberate
; tempo decision (loop close / a genuine, deadbanded+dwelled external
; retune), so a loop played back with no deliberate tempo change since it
; was recorded gets gkCommittedBpm / gkLpRecBpm{N} == exactly 1.000 — both
; sides of the ratio are the SAME frozen number — not a near-1.0 value with
; residual drift. Loop CLOSE quantization (gkLpBeatSamples, instr 209) is a
; DIFFERENT consumer with a DIFFERENT noise requirement and deliberately
; still reads the live gkGLITCH_bpm — closing to the live grid is correct;
; only the already-recorded audio's playback RATE needs the noise-free
; committed value.
;
; This is a RECORDED-length-style value (set once, at capture, like
; gkLpMasterLen) — never written from playback, so it cannot feed back into
; chase_bliss_clock.orc's tempo derivation (which reads gkLpMasterLen only).
gkLpRecBpm1 init 0
gkLpRecBpm2 init 0
gkLpRecBpm3 init 0
gkLpRecBpm4 init 0
gkLpRecBpm5 init 0
; Audio buses
gaLpInL  init 0
gaLpInR  init 0
gaLpOutL init 0
gaLpOutR init 0

;==============================================================
; UDOs (User Defined Opcodes)
;==============================================================

opcode SrReducer, a, ak
  ain, ksrate xin
  kperiod = sr / ksrate
  aout = 0
  kcnt init 0
  kheld init 0
  if (kperiod <= 1) then
    aout = ain
  else
    ksmpl = 0
    while ksmpl < ksmps do
      if (kcnt >= kperiod) then
        kheld = ain[ksmpl]
        kcnt = kcnt - kperiod
      endif
      aout[ksmpl] = kheld
      kcnt = kcnt + 1
      ksmpl = ksmpl + 1
    od
  endif
  xout aout
endop

opcode SwitchPort, k, kii
  kin, iupport, idnport xin
  kold init 0
  kporttime = (kin < kold ? idnport : iupport)
  kout portk kin, kporttime
  kold = kout
  xout kout
endop

; LoopImpliedBpm lives in ONE place — see that file for the history.
#include "loop_implied_bpm.inc.orc"

; NaN/inf safety net UDOs (NF_Chk, NF_Log, LP_Heal) — used by looper, blooper, signal_chain
#include "nonfinite_guard.inc.orc"

opcode DopplerSpin, aa, akkkkki
  asig, kfreq, kAmpDepth, kAmpPhase, kPanDepth, kDopDep, ishape xin
  aAmp osciliktp kfreq, ishape, kAmpPhase
  aAmp = (aAmp * 0.5 * kAmpDepth) + 0.5
  aPan oscili (kPanDepth * 0.5), kfreq, ishape, 0.75
  aPan = aPan + 0.5
  aDelTim oscili kDopDep, kfreq, ishape, 0
  aDelTim = aDelTim + kDopDep
  iMaxDelay = 4
  aDelTap vdelay asig * aAmp, aDelTim, iMaxDelay
  aL, aR pan2 aDelTap, aPan, 1
  xout aL, aR
endop

opcode EnvelopeFollower, a, akkkkkkk
  ain, ksens, katt, krel, kfreq, ktype, kres, kdist xin
  setksmps 4
  aFollow follow2 ain, katt, krel
  kFollow downsamp aFollow
  kFollow expcurve kFollow/0dbfs, 0.3
  kFrq = kfreq + (kFollow * ksens * 10000)
  kFrq limit kFrq, 20, sr/2
  if ktype==1 then
    aout lpf18 ain, kFrq, kres, kdist
  elseif ktype==2 then
    aout moogladder ain, kFrq, kres
  elseif ktype==3 then
    aFrq interp kFrq
    aout butlp ain, aFrq
  elseif ktype==4 then
    aout tone ain, kFrq
  endif
  xout aout
endop

opcode LoFi, a, akk
  ain, kbits, kfold xin
  kval = pow(2, kbits)
  ain = clip(ain, -1, 1)
  aout = int(ain * kval) / kval
  aout fold aout, kfold
  xout aout
endop

; H90Shift — the modern H90 pitch-shift core as a shared mono UDO
; (extracted from effects/h90.orc v11/v12, minus the live-pick transient
; sidechain, which only makes sense on the live input). Used by DELAY's
; feedback shifter and GRANULAR's shifted-grain buffer (2026-07-26 —
; replaced the gritty pitchshifter4 "echo" sound there). MOOD instr 401
; already carries its own inline copy of this algorithm.
;   DOWN (ratio <= 1): peak-locked pvscale — clean, Laroche-Dolson locking.
;   UP   (ratio >  1): 4x butterlp anti-alias (tracks 1/ratio) -> plain
;        pvscale (no pvslock — chord-safe) -> tanh warmth + -3 dB shelf @6k.
; ~16 ms inherent PV latency (FFT 1024 hop 256; hop >= every ksmps we run).
; BRANCH GATING (2026-08-10, perf): only one of the two branches is ever
; selected, so computing both every cycle wastes a whole PV pipeline (two
; 1024-pt FFT/IFFT pairs plus 4x butterlp). Each branch is now gated on
; kRatio with a +/-1 semitone hysteresis band around unity (0.9439..1.0595)
; in which BOTH run: a cold pvsanal needs a full 1024-sample frame (~21 ms
; at 48k) before its output is valid, so the branch that is about to be
; selected must already be spun up before the selector crosses 1.0.
;
; The band alone is NOT sufficient here. Both callers drive kRatio from an
; UNSMOOTHED source (delay.orc kSemiDel, granular.orc gkGranPitch), so a
; preset recall can jump the ratio straight from 0.5 to 2.0 in one k-cycle,
; skipping the band entirely and landing on a stone-cold pvsanal. Before
; this change both branches ran forever and were therefore always warm, so
; that jump was free. kWarmBoth restores it: ANY ratio movement runs both
; branches for 25 ms (> one 1024-sample frame). Sweeps and jumps thus cost
; exactly what they used to; only the steady state — which is ~all of the
; time — gets the saving.
opcode H90Shift, a, ak
  aIn, kRatio xin
  aD init 0
  aU init 0
  kRatPrev  init 1
  kWarmBoth init 0
  if (abs(kRatio - kRatPrev) > 0.0005) then
    kWarmBoth = 0.025 * kr
  elseif (kWarmBoth > 0) then
    kWarmBoth = kWarmBoth - 1
  endif
  kRatPrev = kRatio
  ; ---------- DOWN BRANCH (needed at ratio <= 1.0595) ----------
  if (kRatio < 1.0595 || kWarmBoth > 0) then
    fAnalD pvsanal aIn, 1024, 256, 1024, 1
    fLockD pvslock fAnalD, 1
    fShD   pvscale fLockD, kRatio, 0, 4
    aD     pvsynth fShD
  endif
  ; ---------- UP BRANCH (needed at ratio >= 0.9439) ----------
  if (kRatio > 0.9439 || kWarmBoth > 0) then
    kCutA = sr * 0.5 - 500
    kCutB = sr * 0.5 / kRatio - 500
    kCut  = (kCutB < kCutA ? kCutB : kCutA)
    kCut  limit kCut, 2000, sr * 0.5 - 500
    aP    butterlp aIn, kCut
    aP    butterlp aP, kCut
    aP    butterlp aP, kCut
    aP    butterlp aP, kCut
    fAnalU pvsanal aP, 1024, 256, 1024, 1
    fShU   pvscale fAnalU, kRatio, 0, 4
    aU     pvsynth fShU
    aU = tanh(aU)
    aU pareq aU, 6000, ampdb(-3), 0.7, 2
  endif
  ; ---------- SELECT ----------
  if (kRatio > 1.0) then
    aOut = aU
  else
    aOut = aD
  endif
  xout aOut
endop

opcode pitchshifter4, aa, aakikkiip
  ainL, ainR, kRatio, iNIter, kDelay, kSmooth, iMaxDel, iWfn, iCnt xin
  setksmps 1
  kPt linseg 0, 0.001, 1
  if kSmooth>0 then
    kRatio portk kRatio, kPt*kSmooth
    kDelay portk kDelay, kPt*kSmooth
  endif
  aDel interp kDelay
  aRate = (kRatio-1)/kDelay
  a1 phasor -aRate
  a2 phasor -aRate, 0.5
  ag1 tablei a1, iWfn, 1, 0, 1
  ag2 tablei a2, iWfn, 1, 0, 1
  abuf1 delayr iMaxDel
  ad1 deltap3 a1*aDel
  aG1 = ad1*ag1
  delayw ainL
  abuf2 delayr iMaxDel
  ad2 deltap3 a2*aDel
  aG2 = ad2*ag2
  delayw ainL
  abuf3 delayr iMaxDel
  ad3 deltap3 a1*aDel
  aG3 = ad3*ag1
  delayw ainR
  abuf4 delayr iMaxDel
  ad4 deltap3 a2*aDel
  aG4 = ad4*ag2
  delayw ainR
  aL = 0.5*(aG1+aG2)
  aR = 0.5*(aG3+aG4)
  if iCnt<iNIter then
    aNL, aNR pitchshifter4 ainL, ainR, kRatio, iNIter, kDelay, kSmooth, iMaxDel, iWfn, iCnt+1
    aL = aL + aNL
    aR = aR + aNR
  endif
  xout aL, aR
endop

opcode STUTiteration, aa, aaiiiiiiiii
  aL, aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc, icount, ilayers xin
  aOutL bbcutm aL, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc
  aOutR bbcutm aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc
  aMixL = 0
  aMixR = 0
  if icount < ilayers then
    aMixL, aMixR STUTiteration aL, aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc, icount + 1, ilayers
  endif
  xout aOutL + aMixL, aOutR + aMixR
endop

opcode STUTfreqshifter, a, aki
  ain, kfshift, ifn xin
  areal, aimag hilbert ain
  asin oscili 1, kfshift, ifn, 0
  acos oscili 1, kfshift, ifn, 0.25
  amod1 = areal * acos
  amod2 = aimag * asin
  aFS = (amod1 - amod2)
  xout aFS
endop

opcode StChorus, aa, aakkakk
  ainL, ainR, krate, kdepth, aoffset, kwidth, kmix xin
  ilfoshape ftgentmp 0, 0, 131072, 19, 1, 0.5, 0, 0.5
  kporttime linseg 0, 0.001, 0.02
  kChoDepth portk kdepth*0.01, kporttime
  aChoDepth interp kChoDepth
  amodL osciliktp krate, ilfoshape, 0
  amodR osciliktp krate, ilfoshape, kwidth*0.5
  amodL = (amodL*aChoDepth) + aoffset
  amodR = (amodR*aChoDepth) + aoffset
  aChoL vdelay ainL, amodL*1000, 1.2*1000
  aChoR vdelay ainR, amodR*1000, 1.2*1000
  aoutL ntrpol ainL*0.6, aChoL*0.6, kmix
  aoutR ntrpol ainR*0.6, aChoR*0.6, kmix
  xout aoutL, aoutR
endop

opcode StChorusRspline, aa, aakkkkkk
  ainL, ainR, krate, kdereg, kdepth, koffset, kwidth, kmix xin
  kporttime linseg 0, 0.001, 0.02
  kChoDepth portk kdepth*0.01, kporttime
  kmod1 rspline koffset, kChoDepth, krate*4+0.01, ((krate*4*kdereg)+0.01)
  kmod2 rspline kChoDepth, koffset, krate*4+0.01, ((krate*4*kdereg)+0.01)
  kmod1 limit kmod1, 0.0001, 1.2
  kmod2 limit kmod2, 0.0001, 1.2
  amod1 interp kmod1
  amod2 interp kmod2
  aCho1 vdelay ainL, amod1*1000, 1.2*1000
  aCho2 vdelay ainR, amod2*1000, 1.2*1000
  kpan rspline 0, 1, krate, 2*krate*kdereg
  kpan limit kpan, 0, 1
  apan interp kpan
  aChoL = (aCho1*apan)+(aCho2*(1-apan))
  aChoR = (aCho2*apan)+(aCho1*(1-apan))
  aChoL ntrpol aChoL, aCho1, kwidth
  aChoR ntrpol aChoR, aCho2, kwidth
  aoutL ntrpol ainL*0.6, aChoL*0.6, kmix
  aoutR ntrpol ainR*0.6, aChoR*0.6, kmix
  xout aoutL, aoutR
endop

opcode Flanger, a, akkkkk
  ain, krate, kdepth, kdelay, kfback, klfoshape xin
  iparabola ftgentmp 0, 0, 131072, 19, 0.5, 1, 180, 1
  isine ftgentmp 0, 0, 131072, 19, 1, 0.5, 0, 0.5
  itriangle ftgentmp 0, 0, 131072, 7, 0, 131072/2, 1, 131072/2, 0
  adlt interp kdelay
  if klfoshape==1 then
    amod oscili kdepth, krate, iparabola
  elseif klfoshape==2 then
    amod oscili kdepth, krate, isine
  elseif klfoshape==3 then
    amod oscili kdepth, krate, itriangle
  elseif klfoshape==4 then
    amod randomi 0, kdepth, krate, 1
  else
    amod randomh 0, kdepth, krate, 1
  endif
  adlt sum adlt, amod
  adelsig flanger ain, adlt, kfback, 1.2
  adelsig dcblock adelsig
  aout sum ain*0.5, adelsig*0.5
  xout aout
endop

opcode PhaserSt, aa, aakkkkki
  ainL, ainR, krate, kdepth, kfreq, kstages, kfback, ishape xin
  if ishape=1 then
    klfo lfo kdepth*0.5, krate, 1
  elseif ishape=2 then
    klfo lfo kdepth*0.5, krate, 0
  elseif ishape=3 then
    klfo lfo kdepth*0.5, krate, 2
  elseif ishape=4 then
    klfo lfo kdepth, krate, 4
  elseif ishape=5 then
    klfo lfo kdepth, krate, 5
  elseif ishape=6 then
    klfo oscil kdepth, krate, giExp
  elseif ishape=7 then
    klfo oscil kdepth, krate, giLog
  elseif ishape=8 then
    klfo randomi -kdepth*0.5, kdepth*0.5, krate*8
    klfo portk klfo, 1/(krate*8)
  elseif ishape=9 then
    klfo randomh -kdepth*0.5, kdepth*0.5, krate
  endif
  aoutL phaser1 ainL, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback
  aoutR phaser1 ainR, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback
  xout aoutL, aoutR
endop

;==============================================================
; OSC TRIPLE-SEND UDOs
;==============================================================

;==============================================================
; SINGLE-SEND (2026-09-01). These UDOs used to fire THREE OSCsend opcodes per
; call "for UDP reliability". Each OSCsend opcode holds its OWN socket for the
; life of the instance, and handler_osc.orc calls these ~300 times (one per
; echoed parameter) from the always-on instr 99 — so the engine permanently
; held ~900 UDP sockets and sat right against its 1024 open-fd limit. With no
; spare descriptors the NEXT open() failed: switching an amp read its .nam as
; "empty input" and the model silently never changed (also blocked IR loads,
; sample loads — anything opening a file after warm-up). Triple-send bought
; nothing on 127.0.0.1 loopback (it does not drop packets like a real network),
; and the UI is resynced by OSC_I/OSC_F's change-trigger + gkOscRefresh. One
; send, one socket: ~900 -> ~300 sockets.
opcode OSC3x_f, 0, Sk
  Saddr, kvalue xin
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "f", kvalue
endop

opcode OSC3x_i, 0, Sk
  Saddr, kvalue xin
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "i", kvalue
endop

;==============================================================
; DRIVE MENU 2026 — 5-pedal redesign (spec: docs/specs/
; 2026-06-11-drive-menu-distortion-redesign.md)
;
;   slot0 OD  (130) Overdrive  Klon ⊕ Tube Screamer   toggle 31
;   slot1 MAR (131) Marshall crunch preamp            toggle 32
;   slot2 DHG (132) Dual high-gain preamp SLO ⊕ 5150  toggle 33
;   slot3 RMZ (133) RAT × Metal Zone hard clip        toggle 34
;   slot4 MUF (134) Big Muff Triangle/Fallout fuzz    toggle 35
;
; Uniform controls:
;   CC29 (expression, abs)  -> gk*_drive  (amount of distortion)
;   slot encoder rotation   -> gk*_tone
;   CC20 (prev-menu enc rot)-> gk*_wc1    (wildcard 1, per-pedal)
;   CC23 (next-menu enc rot)-> gk*_wc2    (wildcard 2, per-pedal)
;
; All params normalized 0..1; each pedal maps internally.
; Old Distortion (instr 60) retired/deleted. Old Tube Screamer
; (instr 100) kept on disk as legacy but NOT included/wired.
;==============================================================

; --- OD: Overdrive slot = the BACKUP TUBE SCREAMER (legacy circuit,
; promoted 2026-06-11 v3). Neutral defaults = exact legacy sound.
; wc1 = BODY (clean-lows split, 0.5 = legacy 200 Hz)
; wc2 = clean blend (0 = pure legacy pedal)
gkOD_bypass init 1
gkOD_drive  init 0.4   ; just-after-breakup (exp taper, 2026-06-11 v4)
gkOD_tone   init 0.5
gkOD_wc1    init 0.5
gkOD_wc2    init 0
gaOD_sendL  init 0
gaOD_sendR  init 0
gaOD_outL   init 0
gaOD_outR   init 0

; --- MAR: Marshall crunch preamp ---
; wc1 = presence (post high-shelf)   wc2 = tightness (pre-clip low-cut)
gkMAR_bypass init 1
gkMAR_drive  init 0.10  ; user-dialed sweet spot 2026-06-11 ("proper tone at 10%")
gkMAR_tone   init 0.5
gkMAR_wc1    init 0.4
gkMAR_wc2    init 0.3
gaMAR_sendL  init 0
gaMAR_sendR  init 0
gaMAR_outL   init 0
gaMAR_outR   init 0

; --- DHG: Dual high-gain preamp (SLO ⊕ 5150) ---
; wc1 = engine blend (0 = SLO, 1 = 5150; >0.95 = series stack A→B)
; wc2 = tightness (global low-cut)
gkDHG_bypass init 1
gkDHG_drive  init 0.5   ; DS-1K flagship (replaced dual-hi-gain 2026-06-11)
gkDHG_tone   init 0.5
gkDHG_wc1    init 0.5
gkDHG_wc2    init 0.25  ; bench 2026-06-11: 0.4 put the input HP at 100 Hz, above low E (82 Hz)
gaDHG_sendL  init 0
gaDHG_sendR  init 0
gaDHG_outL   init 0
gaDHG_outR   init 0

; --- RMZ: RAT × Metal Zone ---
; wc1 = mid-sweep freq (maps 200 Hz..5 kHz, log)   wc2 = mid gain (cut..boost)
gkRMZ_bypass init 1
gkRMZ_drive  init 0.5
gkRMZ_tone   init 0.55
gkRMZ_wc1    init 0.4
gkRMZ_wc2    init 0.5
gaRMZ_sendL  init 0
gaRMZ_sendR  init 0
gaRMZ_outL   init 0
gaRMZ_outR   init 0

; --- MUF: Big Muff (Triangle/Fallout) ---
; wc1 = mid recovery (0 = stock scoop, 1 = mid-forward mod)
; wc2 = voicing morph (0 = Triangle, 0.5 = Ram's Head, 1 = Fallout)
gkMUF_bypass init 1
gkMUF_drive  init 0.10  ; user-dialed sweet spot 2026-06-11
gkMUF_tone   init 0.5
gkMUF_wc1    init 0.15
gkMUF_wc2    init 0.85
gaMUF_sendL  init 0
gaMUF_sendR  init 0
gaMUF_outL   init 0
gaMUF_outR   init 0

; Encoder step sizes (50 detents = full sweep)
gkStepDriveTone init 0.02
gkStepDriveWC   init 0.02

;==============================================================
; 4x-OVERSAMPLED WAVESHAPER UDOs — anti-aliased nonlinearities
;
; All drive-pedal clipping goes through these. Each call:
;   1. 4-point cubic Hermite upsample (4x) of the input stream
;      (adds 2 samples latency — negligible, ~42 us at 48 kHz)
;   2. nonlinearity applied per subsample at 192 kHz
;   3. 4th-order Butterworth LP (2 cascaded RBJ biquads,
;      fc 20 kHz @ 192 kHz) on the oversampled stream
;   4. decimate back to sr
;
; State is per-instance: multiple calls in one instr each keep
; their own history/filters. Follows the SrReducer sample-loop
; pattern (a-var indexing inside a while loop).
;
;   OS4Tanh  ain, kpre            y = tanh(kpre * x)  (symmetric soft)
;   OS4Diode ain, kpre, kvp, kvn  asymmetric diode-pair soft clip:
;            x>0: vp*tanh(kpre*x/vp)   x<0: vn*tanh(kpre*x/vn)
;            (vp/vn = forward-voltage-style thresholds; vp==vn = symmetric;
;             Ge ~0.3, Si ~0.6 against unit-ish signals)
;   OS4Hard  ain, kpre, kvp, kvn  true hard clip at +vp / -vn
;
; NOTE: outputs are NOT level-compensated — caller applies makeup.
;==============================================================

; (Implementation note: the upsample/filter scaffold lives once in
; OS4Core; the shape is picked by an i-time selector (imode) so each
; wrapper instance compiles with a fixed mode. The selector IS tested
; inside the 4x subsample loop (Csound has no i-time code elision for
; if-blocks in a k-loop), but the test is two i-var comparisons per
; subsample — negligible next to the tanh/Hermite math, and OS4Diode
; already branched per subsample before this refactor. State is still
; per-instance: each wrapper call owns its own OS4Core instance.)

; imode: 0 = tanh(kpre*x)                              (OS4Tanh)
;        1 = asymmetric diode pair, vp/vn tanh knees   (OS4Diode)
;        2 = true hard clip at +vp / -vn               (OS4Hard)
; kvp/kvn are ignored in mode 0 (pass 1).
opcode OS4Core, a, akkki
  ain, kpre, kvp, kvn, imode xin
  iw0  = 6.28318530717959 * 20000 / (sr * 4)
  icw  = cos(iw0)
  ialp = sin(iw0) / (2 * 0.7071)
  ia0  = 1 + ialp
  inb0 = ((1 - icw) / 2) / ia0
  inb1 = (1 - icw) / ia0
  inb2 = inb0
  ina1 = (-2 * icw) / ia0
  ina2 = (1 - ialp) / ia0
  km3 init 0
  km2 init 0
  km1 init 0
  k0  init 0
  kz11 init 0
  kz12 init 0
  kz21 init 0
  kz22 init 0
  aout init 0
  ksmpl = 0
  while ksmpl < ksmps do
    km3 = km2
    km2 = km1
    km1 = k0
    k0  = ain[ksmpl]
    kc1 = 0.5 * (km1 - km3)
    kc2 = km3 - 2.5*km2 + 2*km1 - 0.5*k0
    kc3 = 0.5*(k0 - km3) + 1.5*(km2 - km1)
    kw2 = 0
    kj = 0
    while kj < 4 do
      kt = kj * 0.25
      kx = ((kc3*kt + kc2)*kt + kc1)*kt + km2
      kxx = kpre * kx
      if (imode == 0) then
        ky = tanh(kxx)
      elseif (imode == 1) then
        if (kxx >= 0) then
          ky = kvp * tanh(kxx / kvp)
        else
          ky = kvn * tanh(kxx / kvn)
        endif
      else
        ky = (kxx > kvp ? kvp : (kxx < -kvn ? -kvn : kxx))
      endif
      kw  = inb0*ky + kz11
      kz11 = inb1*ky - ina1*kw + kz12
      kz12 = inb2*ky - ina2*kw
      kw2 = inb0*kw + kz21
      kz21 = inb1*kw - ina1*kw2 + kz22
      kz22 = inb2*kw - ina2*kw2
      kj = kj + 1
    od
    aout[ksmpl] = kw2
    ksmpl = ksmpl + 1
  od
  xout aout
endop

opcode OS4Tanh, a, ak
  ain, kpre xin
  aout OS4Core ain, kpre, 1, 1, 0
  xout aout
endop

opcode OS4Diode, a, akkk
  ain, kpre, kvp, kvn xin
  aout OS4Core ain, kpre, kvp, kvn, 1
  xout aout
endop

opcode OS4Hard, a, akkk
  ain, kpre, kvp, kvn xin
  aout OS4Core ain, kpre, kvp, kvn, 2
  xout aout
endop

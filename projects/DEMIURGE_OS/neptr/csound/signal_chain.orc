;==============================================================
; MASTER SIGNAL CHAIN - instr 1
;==============================================================
; Mono-summed input from hardware inputs 1 and 2, fed into both
; sides of the stereo effect chain via FX_BYPASS macro and global
; audio buses. Each input is halved (*0.5) so two full-scale sources
; summed together cannot exceed 0 dBFS.
;
; Signal chain order:
;   1.  ATTACK          (150)    16. AUTOMOOG     (51)
;   2.  H90 PITCH       (48)     17. WAH          (52)
;   3.  PITCH FORK      (49)     18. VOLUME PEDAL (inline)
;   4.  OLD PS          (50)     19. SPECTRAL DLY (200)
;   5.  GPS             (55)     20. MOOD         (401-405)
;   6.  PHASER          (233)    21. GRANULAR     (80)
;   7.  FLANGER         (222)    22. DELAY        (62)
;   8.  CHORUS          (40)     22.5 HABIT       (64)
;   9.  VIBRATO         (211)    23. REVERB       (70)
;   10. LESLIE          (300)    24. BBCUT        (90)
;                                25. WGUIDE       (91)
;   11. DRIVE MENU 2026 — lighter dirt first so stacking is musical:
;       OVERDRIVE       (130)    26. SQMOD        (92)
;       MARSHALL        (131)    27. FREQSHIFT    (93)
;       DUAL HIGH-GAIN  (132)
;       RAT x MZ        (133)
;       BIG MUFF        (134)
;   13. FORMANT         (120)
;   14. TAPE            (inline)
;   14.5 RING MOD       (65)
;   0.7  EXPR/PRESET VOL (inline, FRONT — before all FX, for reverb swells)
;   19.5 TREMOLO        (101) — surf placement: right BEFORE the slap delay
;         (62), reverb (70) and amp sims, so their tails wash over the throb.
;
;   2026-07-13 batch additions (stage numbers = insertion labels):
;   0.9  OCTAVER        (170)  after BASS DIST, before drives
;   11.7 LOSSY          (123)  after BINCRUSHER, before TAPE
;   16.5 ENV FILTER     (171)  after WAH
;   19.2 IMPULSE SYNTH  (156)  after GRANULAR, before TREMOLO
;   22.7 RESONATOR      (157)  after HABIT (custom trails bypass)
;   22.8 ONWARD         (155)  after RESONATOR, before REVERB
;   21c  IR REVERB      (240)  right after REVERB
;   INPUT: true-stereo switch gkStereoIn (--omacro:STEREO_IN, default 1;
;   0 = legacy mono sum). OSC /stereo_in.
;
;   2026-07-13 effect batch (toggles 57-58, 60-65; instrs 180-181, 183-188):
;   0.7b PRESET TRIM (inline, giPresetTrim per-preset gain at the master stage)
;   0.8  COMPRESSOR   (180)  after BASS DIST, before OCTAVER
;   2.5  FEEDBACKER   (186)  after H90, before PITCH FORK
;   11.4 CHAINFB inject (183 return bus) — before BIN REDUCTION
;   11.5 TONE EQ      (181)  after the drive block, straight-through (no mix)
;   14.3 PLL FUZZ     (185)  after AUTOMOOG, before WAH
;   18.5 MOOD II      (189)  after MOOD, before DIFFUSION ($FX_BYPASS, toggle 66)
;   18.7 DIFFUSION    (187)  after MOOD, before GRANULAR
;   19.3 TRACK SYNTH  (184)  after IMPULSE, before TREMOLO
;   22.9 CHAINFB capture (183) after ONWARD, before REVERB — wraps the span
;
; (Old DISTORTION 60 deleted; old TUBE SCREAMER 100 unwired — legacy
;  file kept on disk, preset 1 now drives OVERDRIVE 130 instead.)
;==============================================================

; FREEZE (195, toggle 72) gesture-timing state — top-level global (like
; globals.orc's gkLpLastTap*/gkBBcut_pressT precedent) so handler_menus.orc's
; CC76/menu-9/slot-2 press handler can classify single-click-while-engaged
; (CLEAR) vs. double-tap-while-engaged (escape/bypass) across k-cycles. Lives
; here (not globals.orc) since this integration pass does not edit that file;
; declared top-level (outside instr 1) so it initializes once at orchestra
; start, same as any globals.orc global.
gkFrzLastTap init -10

instr 1

; INPUT — TRUE STEREO switch (gkStereoIn, seeded from --omacro:STEREO_IN,
; default 1). 1 = in1 -> L, in2 -> R preserved through the whole chain;
; 0 = legacy mono sum: (in1+in2)*0.5 written to BOTH sides (byte-identical
; to the pre-stereo code path).
#ifdef MIXER
ain1, ain2 MixIn                     ; mixer.orc: gain/mute/route per input channel
#else
ain1   inch 1
ain2   inch 2
#end
if (gkStereoIn >= 0.5) then
  aPrevL = ain1
  aPrevR = ain2
else
  aSum   = (ain1 + ain2) * 0.5
  aPrevL = aSum
  aPrevR = aSum
endif

; 0. PREAMP — always-on "BroadcastPre" (Hudson Broadcast model): Si stage ->
;     Ge stage with envelope-driven bias squish -> transformer LF saturation.
;     3 unity-loudness modes via giPreVoice in effects/preamp.orc (0 soft /
;     1 medium / 2 max). Spec: docs/specs/2026-06-11-broadcast-preamp-design.md
;     Now TOGGLEABLE via gkPreamp_bypass (0=ON,1=OFF); default OFF.
if (gkPreamp_bypass < 0.5) then
  aPrevL, aPrevR Preamp aPrevL, aPrevR
endif

; 0a. BASS PREAMPS (radio pair) — front of chain, right after the main preamp.
;     LIGHT (160) and HEAVY (161); mutual exclusion is enforced in midi_handler.
gaBPLSendL = aPrevL
gaBPLSendR = aPrevR
$FX_BYPASS(gkBPL_bypass'160'gaBPLSendL'gaBPLSendR'gaBPLOutL'gaBPLOutR'bpl)
aPrevL = gaBPLOutL
aPrevR = gaBPLOutR
gaBPHSendL = aPrevL
gaBPHSendR = aPrevR
$FX_BYPASS(gkBPH_bypass'161'gaBPHSendL'gaBPHSendR'gaBPHOutL'gaBPHOutR'bph)
aPrevL = gaBPHOutL
aPrevR = gaBPHOutR

; 0b. BASS DISTORTION (162) — PREAMP-family effect; sits with the bass preamps
;     at the front, NOT in the guitar drive region.
gaBDSSendL = aPrevL
gaBDSSendR = aPrevR
$FX_BYPASS(gkBDS_bypass'162'gaBDSSendL'gaBDSSendR'gaBDSOutL'gaBDSOutR'bds)
aPrevL = gaBDSOutL
aPrevR = gaBDSOutR

; 0.8. COMPRESSOR (180) — optical/FET comp + gate; post-preamp, pre-drives so
;      it evens the signal feeding the dirt section. Parallel blend inside via
;      the chain ntrpol (gkCMP_blend).
gaCMPSendL = aPrevL
gaCMPSendR = aPrevR
$FX_BYPASS(gkCMP_bypass'180'gaCMPSendL'gaCMPSendR'gaCMPOutL'gaCMPOutR'cmp)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkCMP_blend. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXCMP_blend interp gkCMP_blend
aPrevL = gaCMPSendL * (1 - aXCMP_blend) + gaCMPOutL * aXCMP_blend
aPrevR = gaCMPSendR * (1 - aXCMP_blend) + gaCMPOutR * aXCMP_blend

; 0.9. OCTAVER (170) — analog-voiced sub-octave, BEFORE the drive section
;      (sub into fuzz = classic bass move). Dry is unity inside the instr;
;      slot rotation maps to gkOCT_sub (no ntrpol mix stage).
gaOCTSendL = aPrevL
gaOCTSendR = aPrevR
$FX_BYPASS(gkOCT_bypass'170'gaOCTSendL'gaOCTSendR'gaOCTOutL'gaOCTOutR'oct)
aPrevL = gaOCTOutL
aPrevR = gaOCTOutR

; 0.7. EXPRESSION-PEDAL VOLUME (preset swell) — at the FRONT of the chain, on
;      purpose. Because it sits BEFORE every effect (reverb, delay, etc.),
;      swelling the pedal up feeds the reverb gradually and lets its tail
;      BLOOM — a true volume-swell-into-reverb. A volume stage placed AFTER
;      the reverb would just chop the tail as you back off, killing the swell.
;      Driven by gkMasterVol, which a menu-0 preset routes the expression
;      pedal to via the lastToggle29 system (see the CC29 dispatch in
;      midi_handler.orc). Unity (1.0) when no preset is routing volume, so
;      this stage is transparent the rest of the time.
kPresetVolSmooth portk gkMasterVol, gkPortTime
aPrevL = aPrevL * kPresetVolSmooth
aPrevR = aPrevR * kPresetVolSmooth
; ambient preset clean-gain boost (2.1x when ambient active, 1.0 otherwise)
kAmbSmooth portk gkAmbBoost, gkPortTime
aPrevL = aPrevL * kAmbSmooth
aPrevR = aPrevR * kAmbSmooth
; PER-PRESET OUTPUT TRIM — a stored gain per preset (giPresetTrim, indexed by
; gkCurrentPreset). Menu 0 / CC20 nudges the ACTIVE preset's entry ±6 dB. Dry
; (preset -1) is always unity. Multiplied in at the master stage so the trim
; rides the swell, session-only (see globals.orc / docs §4).
if (gkCurrentPreset >= 0) then
  kPTrimIdx = gkCurrentPreset
  gkPresetTrim tab kPTrimIdx, giPresetTrim
else
  gkPresetTrim = 1
endif
kPresetTrimSm portk gkPresetTrim, gkPortTime
aPrevL = aPrevL * kPresetTrimSm
aPrevR = aPrevR * kPresetTrimSm

; 1. ATTACK (150)
gaATTK_sendL = aPrevL
gaATTK_sendR = aPrevR
$FX_BYPASS(gkATTK_bypass'150'gaATTK_sendL'gaATTK_sendR'gaATTK_outL'gaATTK_outR'attk)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkATTK_mix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXATTK_mix interp gkATTK_mix
aPrevL = gaATTK_sendL * (1 - aXATTK_mix) + gaATTK_outL * aXATTK_mix
aPrevR = gaATTK_sendR * (1 - aXATTK_mix) + gaATTK_outR * aXATTK_mix

; 2. H90 PITCH (48) — sinusoidal model, "cleanest"
gaH90SendL = aPrevL
gaH90SendR = aPrevR
$FX_BYPASS(gkBypassH90'48'gaH90SendL'gaH90SendR'gaH90OutL'gaH90OutR'h90)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkH90mix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXH90mix interp gkH90mix
aPrevL = gaH90SendL * (1 - aXH90mix) + gaH90OutL * aXH90mix
aPrevR = gaH90SendR * (1 - aXH90mix) + gaH90OutR * aXH90mix

; 2.5. FEEDBACKER (186) — sustained-feedback overtone voice; dry unity inside,
;      voice level is gkFBK_level internally (chain mix constant 1).
gaFBK_sendL = aPrevL
gaFBK_sendR = aPrevR
$FX_BYPASS(gkFBK_bypass'186'gaFBK_sendL'gaFBK_sendR'gaFBK_outL'gaFBK_outR'fbk)
aPrevL ntrpol gaFBK_sendL, gaFBK_outL, 1
aPrevR ntrpol gaFBK_sendR, gaFBK_outR, 1

; 3. PITCH FORK (49) — pvscale FFT=1024 (raw, low-latency)
gaPFSendL = aPrevL
gaPFSendR = aPrevR
$FX_BYPASS(gkBypassPF'49'gaPFSendL'gaPFSendR'gaPFOutL'gaPFOutR'pf)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkPFmix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXPFmix interp gkPFmix
aPrevL = gaPFSendL * (1 - aXPFmix) + gaPFOutL * aXPFmix
aPrevR = gaPFSendR * (1 - aXPFmix) + gaPFOutR * aXPFmix

; 4. OLD PS (50) — original 4-grain delay-line shifter (kept for grit/legacy)
gaPSSendL = aPrevL
gaPSSendR = aPrevR
$FX_BYPASS(gkBypassPS'50'gaPSSendL'gaPSSendR'gaPSOutL'gaPSOutR'ps)
kPSmix = gkDryPctPS * 0.01
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of kPSmix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXkPSmix interp kPSmix
aPrevL = gaPSSendL * (1 - aXkPSmix) + gaPSOutL * aXkPSmix
aPrevR = gaPSSendR * (1 - aXkPSmix) + gaPSOutR * aXkPSmix

; 3. GPS (55)
gaGPSSendL = aPrevL
gaGPSSendR = aPrevR
$FX_BYPASS(gkBypassGPS'55'gaGPSSendL'gaGPSSendR'gaGPSWetL'gaGPSWetR'gps)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkGPSmix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXGPSmix interp gkGPSmix
aPrevL = gaGPSSendL * (1 - aXGPSmix) + gaGPSWetL * aXGPSmix
aPrevR = gaGPSSendR * (1 - aXGPSmix) + gaGPSWetR * aXGPSmix

; 4. PHASER (233) — mix handled internally
gaPHA_sendL = aPrevL
gaPHA_sendR = aPrevR
$FX_BYPASS(gkPHA_bypass'233'gaPHA_sendL'gaPHA_sendR'gaPHA_outL'gaPHA_outR'pha)
aPrevL = gaPHA_outL
aPrevR = gaPHA_outR

; 5. FLANGER (222) — mix handled internally
gaFLA_sendL = aPrevL
gaFLA_sendR = aPrevR
$FX_BYPASS(gkFLA_bypass'222'gaFLA_sendL'gaFLA_sendR'gaFLA_outL'gaFLA_outR'fla)
aPrevL = gaFLA_outL
aPrevR = gaFLA_outR

; 6. CHORUS (40)
gaChorusSendL = aPrevL
gaChorusSendR = aPrevR
$FX_BYPASS(gkBypassChorus'40'gaChorusSendL'gaChorusSendR'gaChorusWetL'gaChorusWetR'cho)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkChorusMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXChorusMix interp gkChorusMix
aPrevL = gaChorusSendL * (1 - aXChorusMix) + gaChorusWetL * aXChorusMix
aPrevR = gaChorusSendR * (1 - aXChorusMix) + gaChorusWetR * aXChorusMix

; 7. VIBRATO — inline (same pattern as DRIVE MENU 2026; eliminates 1-ksmps FX_BYPASS latency)
if (gkVIB_bypass < 0.5) then
  aPrevL, aPrevR VibratoPedal aPrevL, aPrevR
endif

; 8. LESLIE (300) — 100% wet when active
gaLeslieSendL = aPrevL
gaLeslieSendR = aPrevR
$FX_BYPASS(gkLeslieBypass'300'gaLeslieSendL'gaLeslieSendR'gaLeslieOutL'gaLeslieOutR'les)
aPrevL = gaLeslieOutL
aPrevR = gaLeslieOutR

; 11. DRIVE MENU 2026 — five pedals, serial, light→heavy.
; INLINE, ZERO ADDED LATENCY (2026-06-11 rework): called as opcodes right
; here, exactly like the always-on Preamp above. The previous per-pedal
; bus + FX_BYPASS instance design cost one ksmps block (256 smp = 5.3 ms)
; of latency PER ACTIVE PEDAL, stacking as drives were enabled — that was
; the audible "sometimes latency". The gk*_bypass flags (still flipped by
; the same FX_TOGGLEs in midi_handler) now gate the call sites directly;
; instrs 130-134 and the ga* drive buses are no longer used. When a flag
; is bypassed the branch is skipped: zero CPU, zero delay.

; DRIVELEV reporter: journal the level actually entering the drive
; section every 2 s — used to calibrate volume comp at the PLAYER'S real
; level (bench levels differ; saturation makes comp level-dependent).
; Behind #ifdef DRIVELEV since 2026-08-10: this was unconditional, so the
; rms ran every k-cycle and, worse, printks issued a BLOCKING write() to
; journald from the SCHED_FIFO audio thread every 2 s. A stalled or busy
; journal socket then shows up as an xrun. Re-enable for calibration with
; --omacro:DRIVELEV=1.
#ifdef DRIVELEV
kDrvLev rms aPrevL
printks "DRIVELEV rms=%8.5f\n", 2, kDrvLev
#endif

; 11a. OVERDRIVE (Tube Screamer) — inline
if (gkOD_bypass < 0.5) then
  aPrevL, aPrevR ODPedal aPrevL, aPrevR
endif

; 11b. MARSHALL — JCM800/Plexi crunch preamp, inline
if (gkMAR_bypass < 0.5) then
  aPrevL, aPrevR MARPedal aPrevL, aPrevR
endif

; 11c. DUAL HIGH-GAIN — SLO ⊕ 5150 preamp, inline
if (gkDHG_bypass < 0.5) then
  aPrevL, aPrevR DHGPedal aPrevL, aPrevR
endif

; 11d. RAT × MZ — hard clip + sweepable mid, inline
if (gkRMZ_bypass < 0.5) then
  aPrevL, aPrevR RMZPedal aPrevL, aPrevR
endif

; 11e. BIG MUFF — Triangle/Fallout fuzz, inline
if (gkMUF_bypass < 0.5) then
  aPrevL, aPrevR MUFPedal aPrevL, aPrevR
endif

; 11.5. TONE EQ (181) — amp-style tone block, post-drives. NO mix stage (an EQ
;       blend is a comb-filter foot-gun): wire straight through.
gaTEQSendL = aPrevL
gaTEQSendR = aPrevR
$FX_BYPASS(gkTEQ_bypass'181'gaTEQSendL'gaTEQSendR'gaTEQOutL'gaTEQOutR'teq)
aPrevL = gaTEQOutL
aPrevR = gaTEQOutR

; 10.9. FORMS (119) — clean formant shifter (pvswarp single-pass)
gaFormsSendL = aPrevL
gaFormsSendR = aPrevR
$FX_BYPASS(gkFormsBypass'119'gaFormsSendL'gaFormsSendR'gaFormsOutL'gaFormsOutR'fms)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkFormsMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXFormsMix interp gkFormsMix
aPrevL = gaFormsSendL * (1 - aXFormsMix) + gaFormsOutL * aXFormsMix
aPrevR = gaFormsSendR * (1 - aXFormsMix) + gaFormsOutR * aXFormsMix

; 11. FORMANT (120) — legacy double-pvscale formant shifter (kept as reference)
gaFormantSendL = aPrevL
gaFormantSendR = aPrevR
$FX_BYPASS(gkFormantBypass'120'gaFormantSendL'gaFormantSendR'gaFormantOutL'gaFormantOutR'fmt)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkFormantMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXFormantMix interp gkFormantMix
aPrevL = gaFormantSendL * (1 - aXFormantMix) + gaFormantOutL * aXFormantMix
aPrevR = gaFormantSendR * (1 - aXFormantMix) + gaFormantOutR * aXFormantMix

; 11.4. CHAINFB inject — feedback return re-injected into the wrapped loop span
;       (capture point is stage 22.9, pre-reverb). The 10 ms port gate is
;       REQUIRED (ga buses hold their last block after turnoff2), NOT belt-and-
;       braces. instr 183's return bus can never exceed 0.95 peak (see chainfb.orc).
kCFBon portk (gkCFB_bypass < 0.5 ? 1 : 0), 0.01
aCFBon interp kCFBon
aPrevL = aPrevL + gaCFB_retL * aCFBon
aPrevR = aPrevR + gaCFB_retR * aCFBon

; 11.5. BIN REDUCTION (121) — spectral contrast exponentiation, 100% wet when active
gaBinSendL = aPrevL
gaBinSendR = aPrevR
$FX_BYPASS(gkBinBypass'121'gaBinSendL'gaBinSendR'gaBinWetL'gaBinWetR'bin)
aPrevL = gaBinWetL
aPrevR = gaBinWetR

; 11.6. BINCRUSHER (122) — spectral bin coarsener, 100% wet when active
gaBcSendL = aPrevL
gaBcSendR = aPrevR
$FX_BYPASS(gkBcBypass'122'gaBcSendL'gaBcSendR'gaBcWetL'gaBcWetR'bc)
aPrevL = gaBcWetL
aPrevR = gaBcWetR

; 11.7. LOSSY (123) — codec degradation
gaLsySendL = aPrevL
gaLsySendR = aPrevR
$FX_BYPASS(gkLsyBypass'123'gaLsySendL'gaLsySendR'gaLsyWetL'gaLsyWetR'lsy)
aPrevL = gaLsyWetL
aPrevR = gaLsyWetR

; 11.8. SPECTRAL RACK (SoundMagic, instrs 241-249; spectral/sm_rack.orc)
; Bus hand-off: read the result the groups wrote LAST k-cycle, then publish this
; cycle's aPrev as their input. The groups (higher instr numbers) run after
; instr 1 within the same k-cycle, so the round trip costs exactly ONE ksmps
; (1.3 ms at 48k/64) of latency on top of each group's own pvs latency.
; Rack idle (no SoundMagic effect on) = bit-exact dry, bus cleared, zero cost.
if (gkSMActive > 0.5) then
  aSMoutL = gaSMChainL
  aSMoutR = gaSMChainR
  gaSMChainL = aPrevL
  gaSMChainR = aPrevR
  aPrevL = aSMoutL
  aPrevR = aSMoutR
else
  gaSMChainL = 0
  gaSMChainR = 0
endif

; 12. TAPE (inline) — Gen Loss-style tape machine, 100% wet when active
; (replaced DROPOUT 500; inline like the drive pedals: zero bus latency)
if (gkTapeBypass < 0.5) then
  aPrevL, aPrevR TapeMachine aPrevL, aPrevR
endif

; 13. RING MOD (65) — 100% wet when active
gaRMSendL = aPrevL
gaRMSendR = aPrevR
$FX_BYPASS(gkBypassRM'65'gaRMSendL'gaRMSendR'gaRMOutL'gaRMOutR'rm)
aPrevL = gaRMOutL
aPrevR = gaRMOutR

; 14. AUTOMOOG (51) — 100% wet when active
gaAUTOMOOG_sendL = aPrevL
gaAUTOMOOG_sendR = aPrevR
$FX_BYPASS(gkAUTOMOOG_bypass'51'gaAUTOMOOG_sendL'gaAUTOMOOG_sendR'gaAUTOMOOG_outL'gaAUTOMOOG_outR'auto)
aPrevL = gaAUTOMOOG_outL
aPrevR = gaAUTOMOOG_outR

; 14.3. PLL FUZZ (185) — comparator/divider/PLL fuzz; dry unity inside, P1
;       dry/wet happens in the instr (chain mix constant 1).
gaPLL_sendL = aPrevL
gaPLL_sendR = aPrevR
$FX_BYPASS(gkPLL_bypass'185'gaPLL_sendL'gaPLL_sendR'gaPLL_outL'gaPLL_outR'pll)
aPrevL ntrpol gaPLL_sendL, gaPLL_outL, 1
aPrevR ntrpol gaPLL_sendR, gaPLL_outR, 1

; 15. WAH (52) — 100% wet when active
gaWAH_sendL = aPrevL
gaWAH_sendR = aPrevR
$FX_BYPASS(gkWAH_bypass'52'gaWAH_sendL'gaWAH_sendR'gaWAH_outL'gaWAH_outR'wah)
aPrevL = gaWAH_outL
aPrevR = gaWAH_outR

; 16.5. ENV FILTER (171) — bass envelope filter, after WAH so env-into-drive
;       users re-order via presets, not chain surgery
gaENVSendL = aPrevL
gaENVSendR = aPrevR
$FX_BYPASS(gkENV_bypass'171'gaENVSendL'gaENVSendR'gaENVOutL'gaENVOutR'env)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkENV_wet. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXENV_wet interp gkENV_wet
aPrevL = gaENVSendL * (1 - aXENV_wet) + gaENVOutL * aXENV_wet
aPrevR = gaENVSendR * (1 - aXENV_wet) + gaENVOutR * aXENV_wet

; 16. VOLUME PEDAL (inline)
if (gkVOLpedalBypass < 0.5) then
  kVolSmooth portk gkVOLpedal, gkPortTime
  aPrevL = aPrevL * kVolSmooth
  aPrevR = aPrevR * kVolSmooth
endif

; 17. SPECTRAL DELAY (200)
gaSpecDelSendL = aPrevL
gaSpecDelSendR = aPrevR
$FX_BYPASS(gkSpecDelBypass'200'gaSpecDelSendL'gaSpecDelSendR'gaSpecDelWetL'gaSpecDelWetR'spec)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkSpecDelMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXSpecDelMix interp gkSpecDelMix
aPrevL = gaSpecDelSendL * (1 - aXSpecDelMix) + gaSpecDelWetL * aXSpecDelMix
aPrevR = gaSpecDelSendR * (1 - aXSpecDelMix) + gaSpecDelWetR * aXSpecDelMix

; 17.5. BINSCATTER (201) — split out of SPECDLY, shares the SAME send bus
; (gaSpecDelSendL/R, set immediately above) so both can run at once (their
; wet buses are independent). Does not self-gate on bypass (spectraldelay.orc
; header) — standard $FX_BYPASS + chain-level ntrpol, exactly like SPECDLY.
$FX_BYPASS(gkBscBypass'201'gaSpecDelSendL'gaSpecDelSendR'gaBscWetL'gaBscWetR'bsc)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkBscMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXBscMix interp gkBscMix
aPrevL = aPrevL * (1 - aXBscMix) + gaBscWetL * aXBscMix
aPrevR = aPrevR * (1 - aXBscMix) + gaBscWetR * aXBscMix

; 18. MOOD (401-405) — multi-instrument, handled inline
gaMOOD_SendL = aPrevL
gaMOOD_SendR = aPrevR
kfx_mood_byp = gkMOOD_Bypass
kfx_mood_prv init -1
if (kfx_mood_prv == -1) then
  if (kfx_mood_byp < 0.5) then
    event "i", 401, 0, -1
    event "i", 402, 0, -1
    event "i", 403, 0, -1
    event "i", 405, 0, -1
  else
    gaMOOD_FinalL = gaMOOD_SendL
    gaMOOD_FinalR = gaMOOD_SendR
  endif
else
  if (kfx_mood_prv >= 0.5 && kfx_mood_byp < 0.5) then
    event "i", 401, 0, -1
    event "i", 402, 0, -1
    event "i", 403, 0, -1
    event "i", 405, 0, -1
  elseif (kfx_mood_prv < 0.5 && kfx_mood_byp >= 0.5) then
    turnoff2 401, 0, 1
    turnoff2 402, 0, 1
    turnoff2 403, 0, 1
    turnoff2 404, 0, 1
    turnoff2 405, 0, 1
    gaMOOD_FinalL = gaMOOD_SendL
    gaMOOD_FinalR = gaMOOD_SendR
  endif
endif
kfx_mood_prv = kfx_mood_byp
if (kfx_mood_byp >= 0.5) then
  gaMOOD_FinalL = gaMOOD_SendL
  gaMOOD_FinalR = gaMOOD_SendR
endif
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkMOOD_Mix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXMOOD_Mix interp gkMOOD_Mix
aPrevL = gaMOOD_SendL * (1 - aXMOOD_Mix) + gaMOOD_FinalL * aXMOOD_Mix
aPrevR = gaMOOD_SendR * (1 - aXMOOD_Mix) + gaMOOD_FinalR * aXMOOD_Mix

; 18.5. MOOD II (189) — Chase Bliss MOOD MkII, SLIP-only (reworked 2026-07-13:
; the old always-on micro-looper is GONE, see mood2.orc header for why — real
; $FX_BYPASS now, same as every other simple effect, so OFF means instr 189 is
; actually stopped: true silence, no residual audio (was bug 1 + bug 3).
; Mix is still done HERE (ntrpol) so the dry path stays inline and
; gkMD2_Mix = 0 is a bit-exact null.
; Placed after MOOD (401-405) and before DIFFUSION so MOOD II can chew on the
; old MOOD's output. NOTE: MOOD (t17, TIME menu) and MOOD II (t66, CHASE BLISS)
; are SEPARATE effects and both stay.
gaMD2_sendL = aPrevL
gaMD2_sendR = aPrevR
$FX_BYPASS(gkMD2_bypass'189'gaMD2_sendL'gaMD2_sendR'gaMD2_outL'gaMD2_outR'md2)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkMD2_Mix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXMD2_Mix interp gkMD2_Mix
aPrevL = gaMD2_sendL * (1 - aXMD2_Mix) + gaMD2_outL * aXMD2_Mix
aPrevR = gaMD2_sendR * (1 - aXMD2_Mix) + gaMD2_outR * aXMD2_Mix

; 18.7. DIFFUSION (187) — front-half-of-a-reverb allpass smear; dry/wet by the
;       chain ntrpol (gkDIF_wet).
gaDIF_sendL = aPrevL
gaDIF_sendR = aPrevR
$FX_BYPASS(gkDIF_bypass'187'gaDIF_sendL'gaDIF_sendR'gaDIF_outL'gaDIF_outR'dif)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkDIF_wet. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXDIF_wet interp gkDIF_wet
aPrevL = gaDIF_sendL * (1 - aXDIF_wet) + gaDIF_outL * aXDIF_wet
aPrevR = gaDIF_sendR * (1 - aXDIF_wet) + gaDIF_outR * aXDIF_wet

; 19. GRANULAR (80)
gaGranSendL = aPrevL
gaGranSendR = aPrevR
$FX_BYPASS(gkGranBypass'80'gaGranSendL'gaGranSendR'gaGranWetL'gaGranWetR'gran)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkGranMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXGranMix interp gkGranMix
aPrevL = gaGranSendL * (1 - aXGranMix) + gaGranWetL * aXGranMix
aPrevR = gaGranSendR * (1 - aXGranMix) + gaGranWetR * aXGranMix

; 19.2. IMPULSE SYNTH (156) — MOVED POST-LOOPER 2026-07-15 (GLITCH v2 contract:
;       the whole glitch block now mangles loops + live; see stutter.orc
;       INTEGRATION NOTES item A). Stanza now lives after the looper stage.

; 19.3. TRACK SYNTH (184) — pitch-tracked 3-osc synth voice; dry unity inside,
;       P1 blend happens in the instr (chain mix constant 1). Uses OnsetPick
;       (onset_pick.orc, included before this effect).
gaTSY_sendL = aPrevL
gaTSY_sendR = aPrevR
$FX_BYPASS(gkTSY_bypass'184'gaTSY_sendL'gaTSY_sendR'gaTSY_outL'gaTSY_outR'tsy)
aPrevL ntrpol gaTSY_sendL, gaTSY_outL, 1
aPrevR ntrpol gaTSY_sendR, gaTSY_outR, 1

; 19.5 TREMOLO (101) — surf-rock placement: sits BEFORE the slap delay,
; reverb and amp sims on purpose. A modest depth here gives a noticeable
; throb without full chop; the delay/reverb tails then bloom over it for the
; classic surf wash. (Was briefly moved dead-last for max drama — too much;
; this is the baseline.) Depth = gkTremDepth (0.7 default, swings 0.30..1.0).
gaTremSendL = aPrevL
gaTremSendR = aPrevR
$FX_BYPASS(gkTremBypass'101'gaTremSendL'gaTremSendR'gaTremOutL'gaTremOutR'trem)
aPrevL = gaTremOutL
aPrevR = gaTremOutR

; 20. DELAY (62)
gaDelSendL = aPrevL
gaDelSendR = aPrevR
$FX_BYPASS(gkBypassDelay'62'gaDelSendL'gaDelSendR'gaDelOutL'gaDelOutR'del)
aPrevL = gaDelOutL
aPrevR = gaDelOutR

; 20.1. TAPE DELAY (190) — Space Echo/EP-3-accurate; ALWAYSON (started from
; score, see <CsScore>) and internally gated (kByp portk crossfade in
; tapedelay.orc) so the tape engine stays "warm" across bypass — NOT wrapped
; in $FX_BYPASS (that would turnoff2/kill the transport state). gaTdlWetL
; already contains the full equal-power dry/wet + bypass crossfade, so the
; chain just assigns it straight through (same convention as DELAY above).
gaTdlInL = aPrevL
gaTdlInR = aPrevR
$FX_BYPASS_TAIL(gkTdlBypass'190'6'gaTdlInL'gaTdlInR'gaTdlWetL'gaTdlWetR'tdl)
aPrevL = gaTdlWetL
aPrevR = gaTdlWetR

; 20.2. POLY DLY v5 (191) — clock-locked pitch-cascade delay; standard
; $FX_BYPASS (movedelay.orc has an internal bypass passthrough too).
; gaMvdWetL/R already carries dry (unity) + wet, no mix control, so — like
; DELAY (62) above — the chain assigns it straight through. Chain shape is
; unchanged from v4; only the engine behind 191 changed (docs/specs/2026-10-22-polydelay-v5.md).
gaMvdInL = aPrevL
gaMvdInR = aPrevR
$FX_BYPASS(gkMvdBypass'191'gaMvdInL'gaMvdInR'gaMvdWetL'gaMvdWetR'mvd)
aPrevL = gaMvdWetL
aPrevR = gaMvdWetR

; 20.5 HABIT (64) — echo collector (Chase Bliss Habit port)
gaHabitSendL = aPrevL
gaHabitSendR = aPrevR
$FX_BYPASS(gkHabitBypass'64'gaHabitSendL'gaHabitSendR'gaHabitWetL'gaHabitWetR'habit)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkHabitMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXHabitMix interp gkHabitMix
aPrevL = gaHabitSendL * (1 - aXHabitMix) + gaHabitWetL * aXHabitMix
aPrevR = gaHabitSendR * (1 - aXHabitMix) + gaHabitWetR * aXHabitMix

; 22.7 RESONATOR (157) — sympathetic pipe bank; custom bypass for trails
; (NOT $FX_BYPASS: turnoff2 release mode lets the 2 s trail keep flowing
; through the wet bus after toggle-off)
gaResSendL = aPrevL
gaResSendR = aPrevR
kfx_res_byp = gkRES_bypass
kfx_res_prv init -1
if ((kfx_res_prv == -1 || kfx_res_prv >= 0.5) && kfx_res_byp < 0.5) then
  event "i", 157, 0, -1
elseif (kfx_res_prv < 0.5 && kfx_res_byp >= 0.5) then
  turnoff2 157, 0, 1          ; release mode: instr rings out 2 s
endif
kfx_res_prv = kfx_res_byp
kfx_res_act active 157
if (kfx_res_act == 0) then
  gaResOutL = gaResSendL
  gaResOutR = gaResSendR
endif
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkRES_mix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXRES_mix interp gkRES_mix
aPrevL = gaResSendL * (1 - aXRES_mix) + gaResOutL * aXRES_mix
aPrevR = gaResSendR * (1 - aXRES_mix) + gaResOutR * aXRES_mix

; 22.8 ONWARD (155) — auto-recapturing spectral freeze pad (dry unity, pad
; level is gkONW_mix internally — inline-blend convention).
; ONWARD v2 = time-domain DYNAMIC SAMPLER (was a spectral freeze pad).
gaONW_sendL = aPrevL
gaONW_sendR = aPrevR
$FX_BYPASS(gkONW_bypass'155'gaONW_sendL'gaONW_sendR'gaONW_outL'gaONW_outR'onw)
aPrevL ntrpol gaONW_sendL, gaONW_outL, 1
aPrevR ntrpol gaONW_sendR, gaONW_outR, 1

; 22.9. CHAINFB capture — END of the wrapped loop span (inject point is stage
;       11.4). aPrev is NOT modified here; instr 183 only computes the return
;       bus (gaCFB_retL/R) that stage 11.4 re-injects one k-block later. The
;       FX_BYPASS dummy outs (gaCFB_dumL/R) are never read.
gaCFB_capL = aPrevL
gaCFB_capR = aPrevR
$FX_BYPASS(gkCFB_bypass'183'gaCFB_capL'gaCFB_capR'gaCFB_dumL'gaCFB_dumR'cfb)

; 20.6. NEBULA (193) — Supermassive-style long-line FDN galaxy verb.
; ALWAYSON (started from score) and internally gated (kGate portk crossfade
; in nebula.orc) so the tail decays naturally rather than being cut by
; turnoff2 — NOT wrapped in $FX_BYPASS. gaNebWetL/R is pure wet, so the
; chain-level mix MUST collapse to 0 while bypassed: ntrpol against a
; zeroed wet with mix=1 would output silence, not dry (unlike $FX_BYPASS
; stages, where the macro parks dry in the wet bus while off).
gaNebInL = aPrevL
gaNebInR = aPrevR
$FX_BYPASS_TAIL(gkNebBypass'193'5'gaNebInL'gaNebInR'gaNebWetL'gaNebWetR'neb)
kNebMixEff portk gkNebMix * (gkNebBypass < 0.5 ? 1 : 0), 0.03
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of kNebMixEff. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXkNebMixEff interp kNebMixEff
aPrevL = aPrevL * (1 - aXkNebMixEff) + gaNebWetL * aXkNebMixEff
aPrevR = aPrevR * (1 - aXkNebMixEff) + gaNebWetR * aXkNebMixEff

; 20.7. SHIMMER (194) — pitch-shifted feedback bloom (Valhalla Shimmer-
; style). Toggle 71 / instr 194 — a DIFFERENT effect/toggle from the old
; 2026-07-13 "SHIMMER" (toggle 65, retired to effects/_retired/shimmer.orc).
; ALWAYSON + internally gated (kGate), same convention as NEBULA above.
gaShmInL = aPrevL
gaShmInR = aPrevR
$FX_BYPASS_TAIL(gkShmBypass'194'5'gaShmInL'gaShmInR'gaShmWetL'gaShmWetR'shm)
kShmMixEff portk gkShmMix * (gkShmBypass < 0.5 ? 1 : 0), 0.03
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of kShmMixEff. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXkShmMixEff interp kShmMixEff
aPrevL = aPrevL * (1 - aXkShmMixEff) + gaShmWetL * aXkShmMixEff
aPrevR = aPrevR * (1 - aXkShmMixEff) + gaShmWetR * aXkShmMixEff

; 20.8. FREEZE (195) — infinite-sustain reverb capture; the bed NEVER
; auto-clears (stacking is the point), so it is summed IN PARALLEL with the
; dry signal rather than crossfaded — there is no gkFrzMix; P1 (bed level)
; and the LISTEN send (gkFrzListenGain/expr) are the only level controls,
; both already applied inside freezeverb.orc (gaFrzWetL = wet*kGate*
; kBedLevel, zeroed on bypass by kGate). ALWAYSON (started from score) so
; the frozen bed keeps looping across bypass toggles — NOT wrapped in
; $FX_BYPASS.
gaFrzInL = aPrevL
gaFrzInR = aPrevR
aPrevL += gaFrzWetL
aPrevR += gaFrzWetR

; 20.9. GRAINVERB (188) — dense granular echo cluster (MOOD II algorithm,
; standalone). Standard $FX_BYPASS (no internal bypass gating in
; grainverb.orc — the per-sample DSP loop only runs while the instrument is
; active, i.e. it is already fully skipped on bypass via $FX_BYPASS's own
; turnoff2; no per-sample gate needed inside the orc). gaGvbWetL/R is pure
; wet; chain-level ntrpol per the header's "P1 gkGvbMix (chain-level
; ntrpol)" contract.
gaGvbSendL = aPrevL
gaGvbSendR = aPrevR
$FX_BYPASS(gkGvbBypass'188'gaGvbSendL'gaGvbSendR'gaGvbWetL'gaGvbWetR'gvb)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkGvbMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXGvbMix interp gkGvbMix
aPrevL = aPrevL * (1 - aXGvbMix) + gaGvbWetL * aXGvbMix
aPrevR = aPrevR * (1 - aXGvbMix) + gaGvbWetR * aXGvbMix

; 21. REVERB (70) — DIGIVERB menu's LAST slot (user mandate: NEBULA, SHIMMER,
; FREEZE, GRAINVERB above, REVERB last).
gaVerbSendL = aPrevL
gaVerbSendR = aPrevR
$FX_BYPASS(gkverbBypass'70'gaVerbSendL'gaVerbSendR'gaVerbWetL'gaVerbWetR'verb)
; a-rate blend: ntrpol takes a K-RATE point, so the factor was
; constant for all ksmps samples and JUMPED at each block boundary -
; a step, i.e. a click, on every change of gkverbMix. interp() makes it
; continuous within the block; the crossfade itself is identical.
aXverbMix interp gkverbMix
aPrevL = gaVerbSendL * (1 - aXverbMix) + gaVerbWetL * aXverbMix
aPrevR = gaVerbSendR * (1 - aXverbMix) + gaVerbWetR * aXverbMix

; 21c. IR REVERB (240) — convolution reverb (Menu 12); instr writes the
; ALREADY-MIXED dry/wet blend to gaIRWetL/R (BBCUT-style assignment)
gaIRSendL = aPrevL
gaIRSendR = aPrevR
$FX_BYPASS(gkIRBypass'240'gaIRSendL'gaIRSendR'gaIRWetL'gaIRWetR'irvb)
aPrevL = gaIRWetL
aPrevR = gaIRWetR

; 22-25. GLITCH BLOCK (90-93) — MOVED POST-LOOPER 2026-07-15 (GLITCH v2
;        contract, stutter.orc INTEGRATION NOTES item A). Stanzas unchanged,
;        now after the looper stage below.

; (The expression-pedal/preset volume now lives at the FRONT of the chain —
;  stage 0.7 above — so reverb/delay tails swell and bloom properly. It used
;  to be a final stage here; moved on purpose.)

; NAM AMP SIM (Menu 8) — end of chain: all NEPTR effects → amp sim → output.
; Blend: kNAMBlend=0 → ntrpol passes aPrevL unchanged (100% dry); kNAMBlend=1
; → 100% amp-sim output — an amp sim has no dry/wet mix knob (removed
; 2026-07-27), it's always fully wet when on.
; IN/OUT gain (dB) trim drive into and level out of the model; both are
; portk-smoothed and applied only to the NAM path (bypass path untouched).
;
; RUN GATE (2026-08-10, perf): NAMProcess is by far the most expensive opcode
; in the orchestra — two stateful WaveNet inferences (one per channel), 8
; channels x 23 dilated layers, ~9k MAC/sample each. It used to run EVERY
; k-cycle regardless of bypass, with only the ntrpol blend zeroed, i.e. the
; amp sim burned a large fraction of a core while switched off. It is now
; skipped outright while bypassed. The previous comment here claimed a
; thread=3 kperf opcode "can't be gated inside a Csound if-block reliably";
; that is not true — branch-skipping simply doesn't call kperf, and the
; opcode's state is plain member data that survives untouched.
; kNAMHold keeps the model running 0.25 s past the bypass edge so the 20 ms
; blend fade-out lands on real amp-sim audio rather than on a frozen vector.
; kNAMWarm covers the other edge. Re-engaging after a full stop starts from a
; STALE receptive field (max dilation 239 samples), and portk opens the blend
; to ~6.5% within a single k-cycle — enough to leak a few samples of that
; stale state through as a faint tick on engage. Measured with a stub model:
; 3 samples at ~-41 dBFS relative, on the engage edge only; the disengage
; edge is clean. So the blend target is pinned at 0 for 6 ms after a cold
; start while the model runs and refills, and only then ramps. Engage
; therefore costs 6 ms + 20 ms fade — imperceptible on a footswitch, and it
; makes the transition click-free. A re-engage INSIDE the 0.25 s hold skips
; the warm-up entirely (the model never stopped), so fast toggling stays
; immediate.
; VST_NO_NAM guard: the Cabbage VST build (vst/) compiles without the
; NAMProcess plugin opcode; hardware never defines the macro → identical.
#ifndef VST_NO_NAM
kNAMInGainDb  portk gkNAMInGain,  0.03
kNAMOutGainDb portk gkNAMOutGain, 0.03
aNAMInGain    interp ampdb(kNAMInGainDb)
aNAMOutGain   interp ampdb(kNAMOutGainDb)
kNAMHold init 0
kNAMWarm init 0
if (gkNAMBypass < 0.5) then
  if (kNAMHold <= 0) then
    kNAMWarm = 0.006 * kr     ; cold start — see warm-up note above
  endif
  kNAMHold = 0.25 * kr
elseif (kNAMHold > 0) then
  kNAMHold = kNAMHold - 1
endif
if (kNAMWarm > 0) then
  kNAMWarm = kNAMWarm - 1
endif
kNAMBlendTgt = (gkNAMBypass < 0.5 && kNAMWarm <= 0 ? 1 : 0)
kNAMBlend portk kNAMBlendTgt, 0.02
aNAM_outL init 0
aNAM_outR init 0
if (gkNAMBypass < 0.5 || kNAMHold > 0) then
  aNAM_inL  = aPrevL * aNAMInGain
  aNAM_inR  = aPrevR * aNAMInGain
  aNAM_outL, aNAM_outR NAMProcess aNAM_inL, aNAM_inR, gkNAMProfileIdx, gSNAMLibDir
  aNAM_outL = aNAM_outL * aNAMOutGain
  aNAM_outR = aNAM_outR * aNAMOutGain
  ; a-rate blend: ntrpol takes a K-RATE point, so the factor was
  ; constant for all ksmps samples and JUMPED at each block boundary -
  ; a step, i.e. a click, on every change of kNAMBlend. interp() makes it
  ; continuous within the block; the crossfade itself is identical.
  aXkNAMBlend interp kNAMBlend
  aPrevL = aPrevL * (1 - aXkNAMBlend) + aNAM_outL * aXkNAMBlend
  aPrevR = aPrevR * (1 - aXkNAMBlend) + aNAM_outR * aXkNAMBlend
else
  aNAM_outL = 0
  aNAM_outR = 0
endif
#endif

; LOOPER — feed end-of-chain signal into looper input; mix looper output back.
;
; BLOOPER (instr 208, toggle 56, menu 11 s0) is a granular MODIFIER of these
; same loops — it owns no buffer of its own. When it is ON it REPLACES the
; looper's direct playback with its granular render of the very same buffers;
; it is never summed on top, so the user never hears their loops doubled.
;
; gaBLP_Loop* is produced by instr 208, which runs BEFORE 209-214 each cycle
; and therefore starts its grains from the identical gkLpRdPos base those
; instruments are about to use. Both buses are consumed here one k-block later,
; so they stay sample-aligned with each other: at SCATTER = 0 the granular
; render is a Hann/COLA reconstruction of contiguous playback and the crossfade
; is a null (no combing, no level jump — see blooper.orc header).
;
; Both are plain-assigned (=) in their producers, so neither needs a clear.
gaLpInL  = aPrevL
gaLpInR  = aPrevR
kBlpX port (1 - gkBLP_bypass), 0.04, 0
aPrevL  += gaLpOutL * (1 - kBlpX) + gaBLP_LoopL * kBlpX
aPrevR  += gaLpOutR * (1 - kBlpX) + gaBLP_LoopR * kBlpX

; ===== GLITCH BLOCK — POST-LOOPER (moved 2026-07-15, GLITCH v2 contract:
; stutter.orc INTEGRATION NOTES item A). IMPULSE first, then 90/91/92/93.
; Stanza text unchanged — same buses, same instr numbers, same macros. =====

; 19.2. IMPULSE SYNTH (156) — per-pick synth voice (parallel; dry unity inside)
gaIMP_sendL = aPrevL
gaIMP_sendR = aPrevR
$FX_BYPASS(gkIMP_bypass'156'gaIMP_sendL'gaIMP_sendR'gaIMP_outL'gaIMP_outR'imp)
aPrevL ntrpol gaIMP_sendL, gaIMP_outL, 1
aPrevR ntrpol gaIMP_sendR, gaIMP_outR, 1

; 22. BBCUT (90)
gaBBcut_sendL = aPrevL
gaBBcut_sendR = aPrevR
$FX_BYPASS(gkBBcut_bypass'90'gaBBcut_sendL'gaBBcut_sendR'gaBBcut_wetL'gaBBcut_wetR'bbcut)
aPrevL = gaBBcut_wetL
aPrevR = gaBBcut_wetR

; 23. REPEAT (91) — v2, reuses WGuide buses/toggle
gaWGuide_sendL = aPrevL
gaWGuide_sendR = aPrevR
$FX_BYPASS(gkWGuide_bypass'91'gaWGuide_sendL'gaWGuide_sendR'gaWGuide_wetL'gaWGuide_wetR'wgde)
aPrevL = gaWGuide_wetL
aPrevR = gaWGuide_wetR

; 24. CHOP (92) — v2, reuses SqMod buses/toggle
gaSqMod_sendL = aPrevL
gaSqMod_sendR = aPrevR
$FX_BYPASS(gkSqMod_bypass'92'gaSqMod_sendL'gaSqMod_sendR'gaSqMod_wetL'gaSqMod_wetR'sqmod)
aPrevL = gaSqMod_wetL
aPrevR = gaSqMod_wetR

; 25. RVRS (93) — v2, reuses FShift buses/toggle
gaFShift_sendL = aPrevL
gaFShift_sendR = aPrevR
$FX_BYPASS(gkFShift_bypass'93'gaFShift_sendL'gaFShift_sendR'gaFShift_wetL'gaFShift_wetR'fshft)
aPrevL = gaFShift_wetL
aPrevR = gaFShift_wetR

; ---- MASTER OUT (utility page) — the TRUE end-of-chain volume, AFTER the amp
; sim and every effect, the last thing before the converters. Distinct from the
; front pedal-swell volume (stage 0.7); never on the expression pedal. Set only
; from the UTILITY page. portk-smoothed so slider moves don't zipper. ----
kMasterOutSm portk gkMasterOut, gkPortTime
aPrevL = aPrevL * kMasterOutSm
aPrevR = aPrevR * kMasterOutSm

; ---- MASTER METER FEED (read-only) -----------------------------------------
; Never touches aPrevL/R - it only reads them - so it cannot colour, delay or
; interrupt the audio. PEAK (max_k itype=1), because a clipped square-top
; reads only ~-3 dB rms and rms would hide exactly what this exists to show.
kMtrTrig metro 25
kMtrL max_k aPrevL, kMtrTrig, 1
kMtrR max_k aPrevR, kMtrTrig, 1
OSCsend kMtrTrig, "127.0.0.1", 8000, "/meter", "ff", kMtrL, kMtrR

; OUTPUT
#ifdef MIXER
MixOut aPrevL, aPrevR                ; mixer.orc: per-output source/gain/mute -> outch
#else
outs aPrevL, aPrevR
#end

; Clear accumulation buses
clear gaHabitWetL, gaHabitWetR
clear gaGranWetL, gaGranWetR
clear gaMOOD_PitchL, gaMOOD_PitchR
clear gaMOOD_Bit1L, gaMOOD_Bit1R
clear gaMOOD_GranL, gaMOOD_GranR
clear gaMOOD_FinalL, gaMOOD_FinalR
; gaLpOut: += accumulated by looper instrs 210-214 (they run AFTER this
; instrument each cycle) — read above at the mix-in, cleared here so the
; loopers start from silence.
clear gaLpOutL, gaLpOutR
; gaLpIn must NOT be cleared here: loopers read it after instr 1 finishes;
; clearing it made them record silence. It is plain-assigned (=) above, so
; it never accumulates and needs no clear.
endin

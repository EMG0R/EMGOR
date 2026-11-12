# NEPTR Phase4 — Encoder Mapping v2 (full-takeover model)

**Model:** Encoder *buttons* only navigate menus + toggle effects on/off. When an effect is
toggled ON it becomes `gkLastToggle`, and **all 7 encoder rotations dispatch on `gkLastToggle`**
to control THAT effect's params. The expression pedal (CC29) mapping per effect is UNCHANGED.

## Encoder → CC (authoritative, from midi_handler remap lines 109–155)
| Role | Physical | CC | Priority |
|------|----------|----|----------|
| slot0 rot | E1 | CC24 | P1 if effect home=slot0 |
| slot1 rot | E4 | CC25 | P1 if home=slot1 |
| slot2 rot | E3 | CC26 | P1 if home=slot2 |
| slot3 rot | E2 | CC21 | P1 if home=slot3 |
| slot4 rot | E6 | CC22 | P1 if home=slot4 |
| PREV rot | E7 | CC20 | **P2** (always) |
| NEXT rot | E5 | CC23 | **P3** (always) |

**Priority rule per effect (home slot = S):** P1 = home-slot rotation (the encoder you toggled) =
most-important param (drywet/mix if it exists, else headline). P2 = CC20. P3 = CC23.
P4..P7 = the remaining slot rotations in ascending slot order (skipping S).

**Implementation:** convert CC24/25/26/21/22 handlers from menu-keyed to `gkLastToggle`-keyed
dispatch (CC20/CC23 already are). Each rotation uses `$ENC_ROT(var'step'min'max)` (relative,
clamped). Add `$OSC_F` feedback for every newly-exposed param, address `/<code>_<short>`.

## Scaling notes
- `$ENC_ROT` is incremental (±step/tick, clamped). Step chosen for ~25–50 detents full sweep.
- Freq/gain params: exponential where noted. Mix/depth: linear. Selectors: stepped-int.

---

## Master table (by menu / effect)

Legend: `toggle | home-slot/CC | P1 … Pn` — each param: `var [range] curve step → /osc "Label"`

### Menu 1 VOLUME
- **Volume Pedal** t1 s0/CC24 — P1 `gkVOLpedal [0..1] lin .02 → /vol_level "Volume"`. (1 param)
- **Ring Mod** t11 s1/CC25 — P1 `gkARModDepth [0..1] lin .02 → /rm_depth "Depth"`; P2 `gkARModFreq [0.05..250Hz] exp(×1.1) → /rm_freq "Freq"`.
- **Leslie** t10 s2/CC26 — P1 `gkLeslieDepth [0..1] lin .01 → /les_depth "Depth"`. (speed = CC29, unchanged)
- **Attack** t2 s3/CC21 — P1 `gkATTK_sens [0.1..1] lin .1 → /atk_sens "Sens"` (also CC29); P2 `gkATTK_holdMs [100..2000] lin 50 → /atk_hold "Hold"`.
- **Tape** t21 s4/CC22 — P1 `gkTapeAmount [0..1] lin .02 → /tape_amt "Amount"`; P2 `gkTapeFail [0..1] lin .02 → /tape_fail "Dropout"`; P3 `gkTapeModel [0..1] lin .02 → /tape_model "Model"`. (gkTapeExpr = CC29, unchanged)

### Menu 2 PITCH
- **SpecDel** t16 s0/CC24 — P1 `gkSpecDelMix [0..1] lin .01 → /spec_mix "Mix"`; P2 `gkSpecDelMaxDel [0..1] exp .002 → /spec_time "Time"`; P3 `gkSpecDelFB [0..1] lin .01 → /spec_fb "Feedback"`.
- **Old PS** t3 s1/CC25 — P1 `gkDryPctPS [0..100] int 5 → /ps_mix "Mix%"`; P2 `gkSemi [-12..12] lin .5 → /ps_pitch "Pitch"` (also CC29).
- **GPS** t4 s2/CC26 — P1 `gkGPSmix [0..1] lin .05 → /gps_mix "Mix"`; P2 `gkGPSsemis [-12..12] lin .5 → /gps_pitch "Pitch"` (also CC29); P3 `gkGPSfback [0..0.99] lin .01 → /gps_fb "Feedback"`.
- **Pitch Fork** t27 s3/CC21 — P1 `gkPFmix [0..1] lin .05 → /pf_mix "Mix"`; P2 `gkSemiPF [-12..12] quant 2.5 → /pf_pitch "Interval"` (also CC29).
- **H90** t26 s4/CC22 — P1 `gkH90mix [0..1] lin .05 → /h90_mix "Mix"`; P2 `gkSemiH90 [-12..12] lin .5 → /h90_pitch "Pitch"` (also CC29).

### Menu 3 FILTERING
- **WAH** t22 s0/CC24 — P1 `gkWAH_freq [0..1] concave .02 → /wah_freq "Freq"` (also CC29); P2 `gkWAH_res [0.5..0.97] lin .01 → /wah_res "Res"`.
- **AutoMoog** t14 s1/CC25 — P1 `gkAUTOMOOG_sens [0..1] lin .01 → /amoog_sens "Sens"`; P2 `gkAUTOMOOG_res [0..0.95] lin .01 → /amoog_res "Res"`; P3 `gkAUTOMOOG_type [1..4] int 1 → /amoog_type "Type"`; P4(s2/CC26) `gkAUTOMOOG_level [0..2] lin .02 → /amoog_level "Level"`; P5(s3/CC21) `gkAUTOMOOG_dist [0..1] lin .01 → /amoog_dist "Dist"`.
- **BinCrush** t38 s2/CC26 — P1 `gkBinPedal [0..1] exp .02 → /bin_bins "Bins"` (also CC29). (1 param)
- **Formant** t5 s3/CC21 — P1 `gkFormantMix [0..1] lin .05 → /fmt_mix "Mix"`; P2 `gkFormantShift [-12..12] lin .5 → /fmt_shift "Shift"` (also CC29).
- **Forms** t28 s4/CC22 — P1 `gkFormsMix [0..1] lin .05 → /forms_mix "Mix"`; P2 `gkFormsShift [-12..12] lin .5 → /forms_shift "Shift"` (also CC29).

### Menu 4 MOD
- **Vibrato** t6 s0/CC24 — P1 `gkVIB_depth (Acc) [0..0.005] lin → /vib_depth "Depth"`; P2 `gkVIB_freq [0.001..10Hz] lin .1 → /vib_rate "Rate"`; P3 `gkVIB_fback [0..0.7] lin .01 → /vib_fb "Feedback"`.
- **Chorus** t7 s1/CC25 — P1 `gkChorusMix [0..1] lin .1 → /cho_mix "Mix"`; P2 `gkChorusRate [0.001..7Hz] lin .05 → /cho_rate "Rate"`; P3 `gkChorusDepth [0..1] lin .05 → /cho_depth "Depth"`.
- **Phaser** t8 s2/CC26 — P1 `gkPHA_depth [0..1] lin .01 → /pha_depth "Depth"`; P2 `gkPHA_rate [0.001..40Hz] lin .1 → /pha_rate "Rate"`; P3 `gkPHA_stages [1..24] int 1 → /pha_stages "Stages"`; P4(s0/CC24) `gkPHA_fback [0..0.9] lin .02 → /pha_fb "Feedback"` (also CC29).
- **Granular** t18 s3/CC21 — P1 `gkGranMix [0..1] lin .01 → /gran_mix "Mix"`. (grain internals frozen; 1 param)
- **Flanger** t9 s4/CC22 — P1 `gkFLA_depth [0..0.01] lin → /fla_depth "Depth"`; P2 `gkFLA_rate [0.001..40Hz] lin .1 → /fla_rate "Rate"`; P3 `gkFLA_fback [-1..1] lin .01 → /fla_fb "Feedback"`.

### Menu 5 DRIVE (OSC short names MATCH existing UI EFFECT_PARAMS)
- **Overdrive** t31 s0/CC24 — P1 `gkOD_drive [0..1] exp .02 → /od_drive "Drive"` (also CC29); P2 `gkOD_wc1 [0..1] log .02 → /od_morph "Body"`; P3 `gkOD_wc2 [0..1] lin .02 → /od_blend "Clean"`; P4(s1/CC25) `gkOD_tone [0..1] log .02 → /od_tone "Tone"`.
- **Marshall** t32 s1/CC25 — P1 `gkMAR_drive [0..1] .02 → /mar_drive "Drive"` (CC29); P2 `gkMAR_wc1 → /mar_pres "Presence"`; P3 `gkMAR_wc2 → /mar_tight "Tight"`; P4(s0/CC24) `gkMAR_tone → /mar_tone "Tone"`.
- **RAT×MZ** t34 s2/CC26 — P1 `gkRMZ_drive [0..1] .02 → /rmz_drive "Drive"` (CC29); P2 `gkRMZ_wc1 → /rmz_midf "MidFreq"`; P3 `gkRMZ_wc2 → /rmz_midg "MidGain"`; P4(s0/CC24) `gkRMZ_tone → /rmz_tone "Filter"`.
- **DS-1K** t33 s3/CC21 — P1 `gkDHG_drive [0..1] .02 → /dhg_drive "Drive"` (CC29); P2 `gkDHG_wc1 → /dhg_blend "Fat"`; P3 `gkDHG_wc2 → /dhg_tight "Edge"`; P4(s0/CC24) `gkDHG_tone → /dhg_tone "Tone"`.
- **Big Muff** t35 s4/CC22 — P1 `gkMUF_drive [0..1] .02 → /muf_drive "Sustain"` (CC29); P2 `gkMUF_wc1 → /muf_mids "Mids"`; P3 `gkMUF_wc2 → /muf_voice "Voice"`; P4(s0/CC24) `gkMUF_tone → /muf_tone "Tone"`.

### Menu 6 TIME
- **Delay** t15 s0/CC24 — P1 `gkDelMix [0..1] lin .01 → /del_mix "Mix"`; P2 `gkDelayMS [0..700000] lin 10000 → /del_time "Time"` (also CC29); P3 `gkFeedback [0..1.2] lin .005 → /del_fb "Feedback"`; P4(s1/CC25) `gkSemiDel [-5..12] lin .1 → /del_pitch "PitchDly"`.
- **Habit** t36 s1/CC25 — P1 `gkHabitMix [0..1] lin .02 → /habit_mix "Mix"`; P2 `gkHabitSize [0..1] exp .01 → /habit_size "GrainSz"`; P3 `gkHabitRepeats [0..1] exp .01 → /habit_dens "Density"`; P4(s0/CC24) `gkHabitBlend [0..1] lin .02 → /habit_blend "Blend"`. (gkHabitScan = CC29, unchanged)
- **Reverb** t19 s2/CC26 — P1 `gkverbMix [0..1] lin .01 → /verb_mix "Mix"`; P2 `gkReverbFB [0..0.999] lin .01 → /verb_room "Room"`; P3 `gkReverbDamp [0..1] lin .01 → /verb_damp "Damp"` **(NEW global + DSP expose)**.
- **Granular** t18 s3/CC21 — P1 `gkGranMix [0..1] lin .01 → /gran_mix "Mix"`.
- **MOOD** t17 s4/CC22 — P1 `gkMOOD_Mix [0..1] lin .01 → /mood_mix "Mix"`; P2 `gkMOOD_Clock [0..1] lin .02 → /mood_pitch "Pitch"` (also CC29 inverted); P3 `gkMOOD_Dens [0..1400] lin 100 → /mood_dens "Density"`; P4(s0/CC24) `gkMOOD_Dly [0..1] lin .1 → /mood_dly "MaxDly"`. **DSP FIX:** mood.orc kDly2 should scale `gkMOOD_Dly*0.7`.

### Menu 7 GLITCH (press-hold button semantics UNCHANGED; gkGLITCH_chance = CC29 shared, unchanged)
- **BBcut** t20 s0/CC24 — P1 `gkBBcut_drywet [0..1] lin .01 → /bb_mix "DryWet"`; P2 `gkGLITCH_bpm [40..400] lin 2 → /glitch_bpm "BPM"` (shared); P3 `gkBBcut_fltMix [0..1] lin .01 → /bb_flt "FltMix"`.
- **WGuide** t23 s1/CC25 — P1 `gkWGuide_drywet → /wg_mix "DryWet"`; P2 `gkGLITCH_bpm → /glitch_bpm "BPM"`; P3 `gkWGuide_fb [0.90..0.998] lin .005 → /wg_fb "Feedback"` **(NEW global + DSP)**.
- **SqMod** t24 s2/CC26 — P1 `gkSqMod_drywet → /sq_mix "DryWet"`; P2 `gkGLITCH_bpm`; P3 `gkSqMod_freq [4..14] lin .5 → /sq_freq "Freq"` **(NEW global + DSP)**.
- **FreqShift** t25 s3/CC21 — P1 `gkFShift_drywet → /fs_mix "DryWet"`; P2 `gkGLITCH_bpm`; P3 `gkFShift_amount [-2400..2400] lin 50 → /fs_amt "ShiftAmt"` **(NEW global + DSP)**.

### Menu 8 NAM
- **NAM** t37 s0/CC24 — P1 `gkNAMWet [0..1] lin .02 → /nam_wet "Wet"`; P2 `gkNAMProfileIdx [0..4] int 1 → /nam_profile "Profile"`.

### Menu 0 PRESETS — LEFT AS-IS (preset selection + existing CC22/CC21 drive knobs). Not remapped (preserves preset recall + CC29 volume swell).

---

## New globals to add (globals.orc)
```
gkReverbDamp      init 0.5
gkWGuide_fb       init 0.95
gkSqMod_freq      init 9
gkFShift_amount   init 0
gkStepReverbDamp  init 0.01
gkStepWGuide_fb   init 0.005
gkStepSqMod_freq  init 0.5
gkStepFShift_amt  init 50
gkStepTapeModel   init 0.02   ; if not already present
```

## DSP edits
- `effects/reverb.orc` instr 70: replace hardcoded `iDampCtl = 0.9` with `kDamp = gkReverbDamp`, derive damping LPF.
- `effects/stutter.orc` instr 91 (WGuide): center feedback randomi on `gkWGuide_fb`.
- `effects/stutter.orc` instr 92 (SqMod): center mod-rate randomi on `gkSqMod_freq`.
- `effects/stutter.orc` instr 93 (FreqShift): center shift randomi on `gkFShift_amount`.
- `effects/mood.orc` instr 403: `kDly2 = gkMOOD_Dly * 0.7` (bug fix).

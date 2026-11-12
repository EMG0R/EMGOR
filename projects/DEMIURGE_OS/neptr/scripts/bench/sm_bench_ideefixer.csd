<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Idee Fixer bench (offline). csound --omacro:SCN=<n> -o out.wav ; L,R = processed. Input: tone bursts (see instr).
; SCN 7 mode2 end/walk/transpose, 8 pingpong+gain+dry, 6 muted baseline
; SCN 1 defaults (delay mode, 2 s capture) | 2 threshold start, preroll 100ms, threshold stop |
; 3 extreme (spacing 1%, dur 1000ms, var everything, +-2400c) | 4 live param sweeps | 5 mute/rearm/reset
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_ideefixer.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  p3 = 14
  if iScn == 6 then
    SM_Set $SMP_IDEEFIX_SAMPLING_STATUS, 1
  endif
  if iScn == 2 then
    SM_Set $SMP_IDEEFIX_SAMPLE_START_MODE, 0
    SM_Set $SMP_IDEEFIX_TRIGGER_START_PREROLL, 100
    SM_Set $SMP_IDEEFIX_SAMPLE_STOP_MODE, 0
    SM_Set $SMP_IDEEFIX_TRIGGER_STOP_PREROLL, 50
    SM_Set $SMP_IDEEFIX_TRIGGER_THRESHOLD, -30
  elseif iScn == 7 then
    SM_Set $SMP_IDEEFIX_GRANULATION_MODE, 2
    SM_Set $SMP_IDEEFIX_GRAIN_START, 10
    SM_Set $SMP_IDEEFIX_GRAIN_END, 60
    SM_Set $SMP_IDEEFIX_GRAIN_END_VAR, 10
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR_MODE, 5
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR, 20
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR_RATE, 1
    SM_Set $SMP_IDEEFIX_GRAIN_TRANSPOSITION, -1200
    SM_Set $SMP_IDEEFIX_GRAIN_TRANSPOSITION_VAR, 300
  elseif iScn == 8 then
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR_MODE, 4
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR, 30
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR_RATE, 2
    SM_Set $SMP_IDEEFIX_GAIN, -6
    SM_Set $SMP_IDEEFIX_GRAIN_AMPLITUDE_VAR, 12
    SM_Set $SMP_IDEEFIX_DRY_MIX, -12
  elseif iScn == 3 then
    SM_Set $SMP_IDEEFIX_MAX_SAMPLE_LENGTH, 10
    SM_Set $SMP_IDEEFIX_GRAIN_DURATION, 1000
    SM_Set $SMP_IDEEFIX_GRAIN_SPACING, 1
    SM_Set $SMP_IDEEFIX_GRAIN_SPACING_VAR, 100
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR_MODE, 1
    SM_Set $SMP_IDEEFIX_GRAIN_START_VAR, 100
    SM_Set $SMP_IDEEFIX_GRAIN_TRANSPOSITION, 2400
    SM_Set $SMP_IDEEFIX_GRAIN_TRANSPOSITION_VAR, 2400
    SM_Set $SMP_IDEEFIX_GRAIN_AMPLITUDE_VAR, 60
    SM_Set $SMP_IDEEFIX_GRAIN_FADE, 0
    SM_Set $SMP_IDEEFIX_DRY_MIX, 0
    SM_Set $SMP_IDEEFIX_GAIN, 10
  endif
  ; input: 0.3 s bursts (220/330 Hz + noise) every 1.1 s from t=0
  kt = timeinsts()
  kb = (kt % 1.1 < 0.3) ? 1 : 0
  aEnv = interp(port(kb, 0.005))
  aIn = aEnv * 0.4 * (oscili:a(1, 220) + 0.5 * oscili:a(1, 330) + 0.3 * rnd31:a(1, 0))
  if iScn == 4 then
    kx = lfo:k(0.5, 0.7) + 0.5
    kW2 = 20 + kx * 400
    tablew kW2, $SMP_IDEEFIX_GRAIN_DURATION, giSMP
  endif
  if iScn == 5 then
    if kt > 5 && kt < 5.05 then
      kW = 1
      tablew kW, $SMP_IDEEFIX_SAMPLING_STATUS, giSMP
    elseif kt > 7 && kt < 7.05 then
      kW = 4
      tablew kW, $SMP_IDEEFIX_SAMPLING_STATUS, giSMP
    elseif kt > 9 && kt < 9.05 then
      kW = 2
      tablew kW, $SMP_IDEEFIX_SAMPLING_STATUS, giSMP
    elseif kt > 12 && kt < 12.05 then
      kW = 0
      tablew kW, $SMP_IDEEFIX_SAMPLING_STATUS, giSMP
    endif
  endif
  aL, aR SM_IdeeFixer aIn, aIn, $SM_IDEEFIX
  if iScn == 4 then
    ; keep a record of status
  endif
  kS = tab:k($SMP_IDEEFIX_SAMPLING_STATUS, giSMP)
  printks "t=%.1f status=%d\n", 1, kt, kS
  outs limit(aL, -1, 1), limit(aR, -1, 1)
endin
</CsInstruments>
<CsScore>
i1 0 14
</CsScore>
</CsoundSynthesizer>

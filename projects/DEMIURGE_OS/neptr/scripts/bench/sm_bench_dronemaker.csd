<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; DroneMaker bench (offline). csound --omacro:SCN=<n> --omacro:FFT=<idx> -o out.wav ; L = processed, R = dry
; SCN 1 default, 2 short interp, 3 peak on, 4 comb strong, 5 flanger, 6 gate, 7 extreme (all on, res 99)
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_dronemaker.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = 512 * 2 ^ $FFT
  p3 = 8
  SM_Set $SMP_DRONE_INTERPOLATION_LENGTH, 1
  SM_Set $SMP_DRONE_INTERP_LENGTH_VARIANCE, 0.3
  if iScn == 2 then
    SM_Set $SMP_DRONE_INTERPOLATION_LENGTH, 0.1
    SM_Set $SMP_DRONE_INTERP_LENGTH_VARIANCE, 0
  elseif iScn == 3 then
    SM_Set $SMP_DRONE_USE_PEAK_AMPLITUDES, 1
  elseif iScn == 4 then
    SM_Set $SMP_DRONE_COMB_FILTERBANK_LEVEL, 100
    SM_Set $SMP_DRONE_COMB_FILTER_FUNDAMENTAL, 110
  elseif iScn == 5 then
    SM_Set $SMP_DRONE_FLANGER_AMOUNT, 100
    SM_Set $SMP_DRONE_FLANGER_DEPTH, 8
    SM_Set $SMP_DRONE_FLANGER_RATE, 1
  elseif iScn == 6 then
    SM_Set $SMP_DRONE_GATE_LEVEL, -20
  elseif iScn == 7 then
    SM_Set $SMP_DRONE_INTERPOLATION_LENGTH, 0.1
    SM_Set $SMP_DRONE_INTERP_LENGTH_VARIANCE, 10
    SM_Set $SMP_DRONE_USE_PEAK_AMPLITUDES, 1
    SM_Set $SMP_DRONE_COMB_FILTERBANK_LEVEL, 100
    SM_Set $SMP_DRONE_COMB_FILTER_FUNDAMENTAL, 20
    SM_Set $SMP_DRONE_SCALE_TYPE, 6
    SM_Set $SMP_DRONE_NUMBER_OF_OCTAVES, 10
    SM_Set $SMP_DRONE_FILTER_RESONANCE, 99
    SM_Set $SMP_DRONE_FLANGER_AMOUNT, 100
    SM_Set $SMP_DRONE_FLANGER_DEPTH, 15
    SM_Set $SMP_DRONE_FLANGER_RATE, 5
    SM_Set $SMP_DRONE_GATE_LEVEL, -40
    SM_Set $SMP_DRONE_GAIN, 40
  endif
  ; input: plucked-ish notes changing every 0.8 s plus a little noise
  kn = int(timeinsts() / 0.8) % 4
  kf = table:k(kn, 1)
  aIn = 0.3 * (oscili:a(1, port(kf, 0.001)) + 0.5 * oscili:a(1, 2 * port(kf, 0.001))) * (0.6 + 0.4 * (1 - (timeinsts() % 0.8)/0.8)) + 0.02 * rnd31:a(1, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_DroneMaker kM, kF, $SM_DRONE, kNew, iN
  kM, kF SM_Common kM, kF, $SM_DRONE, kNew, iN
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  outs limit(aOut, -0.99, 0.99), aIn
endin
</CsInstruments>
<CsScore>
f1 0 4 -2 220 277.18 329.63 392
i1 0 8
</CsScore>
</CsoundSynthesizer>

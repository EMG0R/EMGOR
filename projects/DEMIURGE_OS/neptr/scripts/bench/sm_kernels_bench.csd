<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SoundMagic kernels bench. Render:  csound --omacro:SCN=<n> -o <out.wav> sm_kernels_bench.csd
; (analyze_sm_kernels.py does this for every scenario). Output = stereo: L processed, R dry.
; (scn 14 writes the kernel-domain magnitude-energy ratio sqrt(E_out/E_in) to R instead of dry)
; SCN: 1..6 neutral (AVG BLUR FRZ SHUF BSHIFT PITCH), 11..16 characteristic (same order),
;      21 freeze factor 0 control, 99 all six kernels chained (CPU).
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1

#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_averaging.orc"
#include "../../csound/spectral/sm_blurring.orc"
#include "../../csound/spectral/sm_freezing.orc"
#include "../../csound/spectral/sm_shuffle.orc"
#include "../../csound/spectral/sm_binshift.orc"
#include "../../csound/spectral/sm_pitchshift.orc"


opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop

instr 1
  iScn = $SCN
  iN = 2048
  p3 = (iScn == 99) ? 10 : 6
  ; ---- neutral base: rand phases off on every effect used, gain 0
  SM_Set $SMP_AVERAGE_RANDOMIZE_PHASES, 0
  SM_Set $SMP_BLUR_RANDOMIZE_PHASES, 0
  SM_Set $SMP_FREEZE_RANDOMIZE_PHASES, 0
  SM_Set $SMP_SHUFFLE_RANDOMIZE_PHASES, 0
  SM_Set $SMP_BINSHIFT_RANDOMIZE_PHASES, 0
  iK = iScn % 10
  ; ---- scenario params
  if iScn == 1 then
    SM_Set $SMP_AVERAGE_NUMBER_OF_FRAMES, 2
  elseif iScn == 2 then
    SM_Set $SMP_BLUR_BLUR_AMOUNT, 0
  elseif iScn == 3 then
    SM_Set $SMP_FREEZE_FREEZE_FACTOR, 0
  elseif iScn == 4 then
    SM_Set $SMP_SHUFFLE_SHUFFLE_FACTOR, 0
  elseif iScn == 5 then
    SM_Set $SMP_BINSHIFT_BINS_TO_SHIFT, 0
    SM_Set $SMP_BINSHIFT_ORIGINAL_MULTIPLIER, 0
    SM_Set $SMP_BINSHIFT_SHIFTED_MULTIPLIER, 100
  elseif iScn == 6 then
    SM_Set $SMP_PITCH_PITCH_SHIFT, 0
    SM_Set $SMP_PITCH_FEEDBACK, 0
  elseif iScn == 11 then
    SM_Set $SMP_FREEZE_FREEZE_FACTOR, 100
  elseif iScn == 21 then
    SM_Set $SMP_FREEZE_FREEZE_FACTOR, 0
  elseif iScn == 12 then
    SM_Set $SMP_BLUR_BLUR_AMOUNT, 100
  elseif iScn == 13 then
    SM_Set $SMP_AVERAGE_NUMBER_OF_FRAMES, 50
  elseif iScn == 14 then
    SM_Set $SMP_SHUFFLE_SHUFFLE_FACTOR, 100
    SM_Set $SMP_SHUFFLE_SHUFFLE_RANGE, 20
    SM_Set $SMP_SHUFFLE_BINS_PER_GROUP, 1
  elseif iScn == 15 then
    SM_Set $SMP_BINSHIFT_BINS_TO_SHIFT, 10
    SM_Set $SMP_BINSHIFT_ORIGINAL_MULTIPLIER, 0
    SM_Set $SMP_BINSHIFT_SHIFTED_MULTIPLIER, 100
  elseif iScn == 16 then
    SM_Set $SMP_PITCH_PITCH_SHIFT, 1200
    SM_Set $SMP_PITCH_FEEDBACK, 0
  elseif iScn == 99 then
    SM_Set $SMP_AVERAGE_NUMBER_OF_FRAMES, 20
    SM_Set $SMP_BLUR_BLUR_AMOUNT, 40
    SM_Set $SMP_FREEZE_FREEZE_FACTOR, 60
    SM_Set $SMP_SHUFFLE_SHUFFLE_FACTOR, 30
    SM_Set $SMP_BINSHIFT_BINS_TO_SHIFT, 3
    SM_Set $SMP_PITCH_PITCH_SHIFT, 700
    SM_Set $SMP_PITCH_FEEDBACK, 0
    SM_Set $SMP_AVERAGE_RANDOMIZE_PHASES, 1
  endif
  ; ---- input
  kT = timeinsts()
  if iScn == 14 then
    aIn = 0.5 * butterbp(butterbp(rnd31:a(1, 0), 450, 300), 450, 300) * 4
  elseif iScn <= 6 then
    aIn = 0.15 * (oscili:a(1, 220) + oscili:a(1, 440) + oscili:a(1, 880) + oscili:a(1, 1760))
  elseif iScn == 11 || iScn == 21 then
    aIn = 0.5 * oscili:a(1, 440) * linseg:a(1, 2, 1, 0.01, 0, 10, 0)
  elseif iScn == 12 || iScn == 13 then
    aIn = 0.5 * oscili:a(1, 440) * linseg:a(0, 1, 0, 0.01, 1, 10, 1)
  elseif iScn == 99 then
    aIn = 0.2 * oscili:a(1, 330) + 0.2 * oscili:a(1, 990) + 0.05 * rnd31:a(1, 0)
  else
    aIn = 0.5 * oscili:a(1, 440)
  endif
  ; ---- analysis
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  if iScn == 1 || iScn == 13 || iScn == 99 then
    kM, kF SM_Averaging kM, kF, $SM_AVERAGE, kNew, iN
    kM, kF SM_Common kM, kF, $SM_AVERAGE, kNew, iN
  endif
  if iScn == 2 || iScn == 12 || iScn == 99 then
    kM, kF SM_Blurring kM, kF, $SM_BLUR, kNew, iN
    kM, kF SM_Common kM, kF, $SM_BLUR, kNew, iN
  endif
  if iScn == 3 || iScn == 11 || iScn == 21 || iScn == 99 then
    kM, kF SM_Freezing kM, kF, $SM_FREEZE, kNew, iN
    kM, kF SM_Common kM, kF, $SM_FREEZE, kNew, iN
  endif
  kEb init 1.0e-9
  kEa init 1.0e-9
  if iScn == 14 && kNew == 1 then
    kq = 0
    while kq < iNb do
      kEb += kM[kq] * kM[kq]
      kq += 1
    od
  endif
  if iScn == 4 || iScn == 14 || iScn == 99 then
    kM, kF SM_Shuffle kM, kF, $SM_SHUFFLE, kNew, iN
    kM, kF SM_Common kM, kF, $SM_SHUFFLE, kNew, iN
  endif
  if iScn == 5 || iScn == 15 || iScn == 99 then
    kM, kF SM_BinShift kM, kF, $SM_BINSHIFT, kNew, iN
    kM, kF SM_Common kM, kF, $SM_BINSHIFT, kNew, iN
  endif
  if iScn == 6 || iScn == 16 || iScn == 99 then
    kM, kF SM_PitchShift kM, kF, $SM_PITCH, kNew, iN
    kM, kF SM_Common kM, kF, $SM_PITCH, kNew, iN
  endif
  if iScn == 14 && kNew == 1 then
    kq = 0
    while kq < iNb do
      kEa += kM[kq] * kM[kq]
      kq += 1
    od
  endif
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  if iScn == 14 then
    outs limit(aOut, -0.99, 0.99), a(sqrt(kEa / kEb))
  else
    outs limit(aOut, -0.99, 0.99), aIn
  endif
endin
</CsInstruments>
<CsScore>
i1 0 6
</CsScore>
</CsoundSynthesizer>

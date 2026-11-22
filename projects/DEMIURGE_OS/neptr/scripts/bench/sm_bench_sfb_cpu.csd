<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Super Filterbank bench. csound --omacro:SCN=<n> -o out.wav sm_bench_superfilterbank.csd
; optional --omacro:TYP=-1 (sweep) or 0..5 fixes type in SCN 10
; TYP overrides Filter type (all SCN) ; NOTE SCN 4..8 = types 0..4
; SCN 16 worst-case CPU (F0 10, octatonic, N120 -> ~88 active bands)
; SCN 1 defaults noise 2 bw 0.01 cents N120 3 bw 100 N120 type1 4 type0 comb 5 type2 6 type3 7 type4 8 type5
; 9 extremes (F0 20000, bw .01, all types sweep) 10 param sweep click test 11 CPU (N120, noise) 12 dry (wet 0)
; 13 sweep input defaults  14 scale 4 type1 narrow N12  15 F0 10 comb
#ifndef DUR
#define DUR #20#
#end
#ifndef MONO
#define MONO #0#
#end
#ifndef TYP
#define TYP #-1#
#endif
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_superfilterbank.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  p3 = $DUR
  if iScn == 2 then
    SM_Set $SMP_SUPERFB_BANDWIDTH, 0.01
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
  elseif iScn == 3 then
    SM_Set $SMP_SUPERFB_BANDWIDTH, 100
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
  elseif iScn >= 4 && iScn <= 8 then
    SM_Set $SMP_SUPERFB_FILTER_TYPE, iScn - 4
    SM_Set $SMP_SUPERFB_SCALE, 0
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 8
    SM_Set $SMP_SUPERFB_BANDWIDTH, 20
  elseif iScn == 9 then
    SM_Set $SMP_SUPERFB_FUNDAMENTAL_FREQUENCY, 20000
    SM_Set $SMP_SUPERFB_BANDWIDTH, 0.01
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
    SM_Set $SMP_SUPERFB_GAIN, 30
  elseif iScn == 11 then
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
    SM_Set $SMP_SUPERFB_SCALE, 5
  elseif iScn == 20 then
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 1
  elseif iScn == 12 then
    SM_Set $SMP_SUPERFB_WET_DRY_MIX, 0
  elseif iScn == 14 then
    SM_Set $SMP_SUPERFB_SCALE, 4
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 12
    SM_Set $SMP_SUPERFB_BANDWIDTH, 3
  elseif iScn == 16 then
    SM_Set $SMP_SUPERFB_FUNDAMENTAL_FREQUENCY, 10
    SM_Set $SMP_SUPERFB_SCALE, 10
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
    SM_Set $SMP_SUPERFB_BANDWIDTH, 5
  elseif iScn == 15 then
    SM_Set $SMP_SUPERFB_FUNDAMENTAL_FREQUENCY, 10
    SM_Set $SMP_SUPERFB_FILTER_TYPE, 0
    SM_Set $SMP_SUPERFB_BANDWIDTH, 0.01
    SM_Set $SMP_SUPERFB_NUMBER_OF_FILTERS, 120
  endif
  if $TYP >= 0 && iScn != 10 then
    SM_Set $SMP_SUPERFB_FILTER_TYPE, $TYP
  endif
  if iScn == 13 then
    aIn = oscili(0.5, expseg(40, 4, 15000))
  else
    aIn = noise(0.3, 0)
  endif
  if iScn == 10 then
    kS = oscili:k(0.5, 0.7) + 0.5
    tablew 50 + kS * 4000, $SMP_SUPERFB_FUNDAMENTAL_FREQUENCY, giSMP
    tablew kS * 60, $SMP_SUPERFB_INTERPOLATION_TIME, giSMP
    tablew int(kS * 12.99), $SMP_SUPERFB_SCALE, giSMP
    tablew 1 + kS * 119, $SMP_SUPERFB_NUMBER_OF_FILTERS, giSMP
    tablew 0.01 + kS * 99, $SMP_SUPERFB_BANDWIDTH, giSMP
    tablew ($TYP < 0 ? int(kS * 5.99) : $TYP), $SMP_SUPERFB_FILTER_TYPE, giSMP
    tablew kS * 100, $SMP_SUPERFB_WET_DRY_MIX, giSMP
    tablew (kS - 0.5) * 12, $SMP_SUPERFB_GAIN, giSMP
  endif
  aL, aR SM_SuperFilterbank aIn, aIn * ($MONO == 1 ? 1 : 0.9), $SM_SUPERFB
  outs aL, aR
endin
</CsInstruments>
<CsScore>
i1 0 1
</CsScore>
</CsoundSynthesizer>

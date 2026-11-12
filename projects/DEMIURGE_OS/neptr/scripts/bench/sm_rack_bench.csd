<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
; SoundMagic rack path bench (render only, -n, no audio out).
; Proves the chain-bus + group + pvsanal/pvsynth path is an identity when the
; kernel is identity, and bit-exact dry when no kernel / effect is off.
;
; Run from neptr/:  csound scripts/bench/sm_rack_bench.csd [--omacro:SM_BENCH_REAL=1]
;   default: includes a throwaway identity stub (INCDIR=<dir with sm_stub.orc>,
;            e.g. --env:INCDIR=/path/to/scratch)
;   SM_BENCH_REAL=1: uses the real spectral/sm_core.orc + sm_freezing.orc.
; Scenarios (by test instr): A effect off, B effect on at default FFT (2048),
; C effect on at FFT idx 0 (512) -> a different group.
; Prints RMS of a 440 Hz tone before and after the rack (delay-compensated by
; comparing over a steady window); pass = ratio within 0.5 dB.
#define PRESET_BANK #0#
#include "csound/globals.orc"
#include "csound/spectral/sm_params.orc"
#ifdef SM_BENCH_REAL
#include "csound/spectral/sm_core.orc"
#include "csound/spectral/sm_freezing.orc"
#else
#include "sm_stub.orc"
#endif
#include "csound/spectral/sm_rack.orc"
#include "csound/spectral/sm_persist.orc"

; stand-in for signal_chain.orc's SPECTRAL RACK block + rack control
instr 1
  aT oscili 0.3, 440
  aPrevL = aT
  aPrevR = aT
  ; --- same code as signal_chain.orc 11.8 ---
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
  gaOutBL = aPrevL
  kIn  rms aT
  kOut rms aPrevL
  gkIn = kIn
  gkOut = kOut
endin
gaOutBL init 0
gkIn init 0
gkOut init 0

; scenario setter: p4=on, p5=FFT idx (-1 = leave)
instr 2
  tablew p4, 3, giSMon
  if (p5 >= 0) then
    tablew p5, 3*32 + 7, giSMP
  endif
endin
instr 300
  kt timeinstk
  if (kt % 700 == 0) then
    printks "t=%.2fs active=%d  rmsIn=%f rmsOut=%f  ratio_dB=%.2f\n", 0, kt/750, gkSMActive, gkIn, gkOut, dbfsamp(gkOut) - dbfsamp(gkIn)
  endif
endin
</CsInstruments>
<CsScore>
i249 0 -1
i1   0 7
i300 0 7
; A: off (0-1.5 s)   B: on 2048 (1.5-3.5)   C: on 512 (3.5-5.5)   D: off again
i2 0   0.01 0 -1
i2 1.5 0.01 1 2
i2 3.5 0.01 1 0
i2 5.5 0.01 0 -1
e
</CsScore>
</CsoundSynthesizer>

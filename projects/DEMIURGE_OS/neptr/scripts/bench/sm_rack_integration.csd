<CsoundSynthesizer>
<CsOptions>
-d -f -o /tmp/sm_rack_integration.wav
</CsOptions>
<CsInstruments>
; NORRIS spectral rack INTEGRATION bench (offline, no audio device).
; Real rack: same include order as csound/neptrPhase4.csd (params -> core ->
; 19 kernels -> rack), real instr 241..249, chain bus gaSMChainL/R, with the
; signal_chain.orc "SPECTRAL RACK" block copied as instr 1.
;
; Run from neptr/ (cwd matters: includes are "csound/..."):
;   taskset -c 0-2 nice -n 19 csound scripts/bench/sm_rack_integration.csd \
;       -o /tmp/x.wav --omacro:MASK=<bitmask of fx 0..18> --omacro:FFT=<0..7> \
;       [--omacro:MODE=0|1] [--omacro:GAIN=<input scale, hot-input test>] [--omacro:OFFAT=<sec>] [--omacro:DUR=10] [--smacro:DUR=.. --smacro:OFFAT=..]
;   (DUR must be given as BOTH --omacro and --smacro: orchestra and score macros are separate; OFFAT is orc-only)
;   MASK  bit i = effect i on (default 0 = all off = baseline)
;   FFT   uniform FFT index for every effect (also the rack default giSMP[736])
;   MODE  0 = uniform FFT; 1 = mixed: per-effect index from giPat (512/2048/8192)
;   OFFAT second at which every effect is switched OFF again (default never)
; Input: 10 s of 220 Hz sine (tremolo) + gated noise bursts + 880 Hz blips, mono->both.
; Prints one "RESULT" line: sticky NaN/Inf flag, peak, RMS windows A (3-6 s) and
; B (7.5-9.9 s) for input / output / (output-input), active group instrs at t=5, t=9.
; Wall/CPU: wrap the call in `/usr/bin/time -f %U`.
#define PRESET_BANK #0#
#ifndef MASK
#define MASK #0#
#end
#ifndef FFT
#define FFT #2#
#end
#ifndef MODE
#define MODE #0#
#end
#ifndef OFFAT
#define OFFAT #1000#
#end
#ifndef DUR
#define DUR #10#
#end
#ifndef GAIN
#define GAIN #1#
#end
#include "csound/globals.orc"
; ---- exactly neptrPhase4.csd order ----
#include "csound/spectral/sm_params.orc"
#include "csound/spectral/sm_core.orc"
#include "csound/spectral/sm_freezing.orc"
#include "csound/spectral/sm_blurring.orc"
#include "csound/spectral/sm_averaging.orc"
#include "csound/spectral/sm_binshift.orc"
#include "csound/spectral/sm_pitchshift.orc"
#include "csound/spectral/sm_shuffle.orc"
#include "csound/spectral/sm_dronemaker.orc"
#include "csound/spectral/sm_gateandhold.orc"
#include "csound/spectral/sm_weave.orc"
#include "csound/spectral/sm_shimmer.orc"
#include "csound/spectral/sm_pulsing.orc"
#include "csound/spectral/sm_harmonizer.orc"
#include "csound/spectral/sm_stretch.orc"
#include "csound/spectral/sm_tracing.orc"
#include "csound/spectral/sm_filterbank.orc"
#include "csound/spectral/sm_glidingfilters.orc"
#include "csound/spectral/sm_granulation.orc"
#include "csound/spectral/sm_emergence.orc"
#include "csound/spectral/sm_partialglide.orc"
#include "csound/spectral/sm_rack.orc"
#include "csound/spectral/sm_persist.orc"

; MODE 1 FFT index per effect (fx 8 PULSING and 11 PITCH have no FFT param -> rack default = 2)
giPat ftgen 0, 0, -19, -2, 0,0,2,0,2,2,2,0,2,2,0,2,4,4,4,4,4,4,4

gkPeakL init 0
gkPeakR init 0
gkBadL  init 0
gkBadR  init 0
gkAcc   init 0
giAct   ftgen 0, 0, -16, -2, 0   ; [0..7] active count per group at t=5, [8..15] at t=9
giWin   ftgen 0, 0, -16, -2, 0   ; sum-of-squares/count: A in,out,diff,n ; B in,out,diff,n

; ---- scenario setup: apply MASK/FFT/MODE at init ----
instr 2
  tablew $FFT, 736, giSMP                      ; rack default FFT index
  ifx = 0
  while (ifx < 19) do
    ibit = int($MASK / (2 ^ ifx)) % 2
    tablew ibit, ifx, giSMon
    ifp = table:i(ifx, giSMfftp)
    if (ifp >= 0) then
      iv = ($MODE == 1 ? table:i(ifx, giPat) : $FFT)
      tablew iv, ifp, giSMP
    endif
    ifx += 1
  od
  if ($MODE == 1) then
    tablew 2, 736, giSMP
  endif
  if ($OFFAT < $DUR) then
    schedule 3, $OFFAT, 0.01
  endif
endin
instr 3                                         ; switch everything off
  ifx = 0
  while (ifx < 19) do
    tablew 0, ifx, giSMon
    ifx += 1
  od
endin

; ---- test input + the SPECTRAL RACK block of signal_chain.orc ----
instr 1
  kEnv  = 0.7 + 0.3 * oscili:k(1, 0.5)
  aSin  oscili 0.20 * kEnv, 220
  kGate = (oscili:k(1, 2) > 0.6 ? 1 : 0)
  aNz   rnd31 0.08, 1
  aNz   = aNz * interp(kGate)
  kBlip = (phasor:k(0.7) < 0.1 ? 1 : 0)
  aBl   oscili 0.15, 880
  aBl   = aBl * interp(kBlip)
  aIn   = (aSin + aNz + aBl) * $GAIN
  aPrevL = aIn
  aPrevR = aIn
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
  outs aPrevL, aPrevR
  ; ---- metrics ----
  kPL max_k aPrevL, 1, 1
  kPR max_k aPrevR, 1, 1
  kEL rms aPrevL, 5
  kER rms aPrevR, 5
  if (kEL != kEL || abs(kEL) > 1e9) then
    gkBadL = 1
  endif
  if (kER != kER || abs(kER) > 1e9) then
    gkBadR = 1
  endif
  kI rms aIn, 5
  kO = kEL
  kD rms aPrevL - aIn, 5
  kt timeinsts
  if (kt > 1) then                     ; skip the first second (dry start-up before the rack engages)
    gkPeakL = max(gkPeakL, kPL)
    gkPeakR = max(gkPeakR, kPR)
  endif
  if (kt >= 3 && kt < 6) then
    kw = 0
  elseif (kt >= 7.5 && kt < 9.9) then
    kw = 4
  else
    kw = -1
  endif
  if (kw >= 0) then
    tablew table:k(kw, giWin) + kI*kI, kw, giWin
    tablew table:k(kw+1, giWin) + kO*kO, kw+1, giWin
    tablew table:k(kw+2, giWin) + kD*kD, kw+2, giWin
    tablew table:k(kw+3, giWin) + 1, kw+3, giWin
  endif
  ; group instr counts
  if (kt >= 5 && kt < 5 + 2/kr) then
    ks = 0
    while (ks < 8) do
      kn active 241 + ks
      tablew kn, ks, giAct
      ks += 1
    od
  endif
  if (kt >= 9 && kt < 9 + 2/kr) then
    ks = 0
    while (ks < 8) do
      kn active 241 + ks
      tablew kn, 8 + ks, giAct
      ks += 1
    od
  endif
endin

instr 301
  kc timeinstk
  if (kc == 1) then
    kAn = table:k(3, giWin)
    kBn = table:k(7, giWin)
    kAn = max(kAn, 1)
    kBn = max(kBn, 1)
    printks "RESULT bad=%d/%d peakL=%.4f peakR=%.4f | A in=%.4f out=%.4f diff=%.4f | B in=%.4f out=%.4f diff=%.4f | act5=%d%d%d%d%d%d%d%d act9=%d%d%d%d%d%d%d%d active=%d\n", 0, \
      gkBadL, gkBadR, gkPeakL, gkPeakR, \
      sqrt(table:k(0, giWin)/kAn), sqrt(table:k(1, giWin)/kAn), sqrt(table:k(2, giWin)/kAn), \
      sqrt(table:k(4, giWin)/kBn), sqrt(table:k(5, giWin)/kBn), sqrt(table:k(6, giWin)/kBn), \
      table:k(0, giAct), table:k(1, giAct), table:k(2, giAct), table:k(3, giAct), table:k(4, giAct), table:k(5, giAct), table:k(6, giAct), table:k(7, giAct), \
      table:k(8, giAct), table:k(9, giAct), table:k(10, giAct), table:k(11, giAct), table:k(12, giAct), table:k(13, giAct), table:k(14, giAct), table:k(15, giAct), \
      gkSMActive
  endif
endin
</CsInstruments>
<CsScore>
#ifndef DUR
#define DUR #10#
#end
#ifndef OFFAT
#define OFFAT #1000#
#end
i249 0 -1
i2   0 0.01
i1   0 $DUR
i301 [$DUR - 0.05] 0.05
e [$DUR + 0.1]
</CsScore>
</CsoundSynthesizer>

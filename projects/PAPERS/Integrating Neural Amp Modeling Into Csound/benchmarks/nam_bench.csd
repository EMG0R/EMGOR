<CsoundSynthesizer>
<CsOptions>
--opcode-lib=/Users/emgor/Library/CsoundPlugins/csound-nam.so -+rtaudio=null -o dac -d -m0
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

; macros supplied via --omacro:AUTOV=0|1 and --omacro:OUTF=path
#ifndef AUTOV
#define AUTOV #1#
#endif

giSeg  init 22        ; seconds per model segment
giMeas init 6         ; measure window at segment end (settled)
giN    init 25        ; number of models

instr 1
  ; --- source: repeating 110 Hz saw plucks, identical for every model ---
  ktrig  metro  2
  aenv   init   0
  if ktrig == 1 then
    reinit PLUCK
  endif
PLUCK:
  aenv   expseg  1, 0.4, 0.05, 0.09, 0.0001, 10, 0.0001
  rireturn
  asrc   vco2   0.28, 110
  a1     =      asrc * aenv
  a2     =      a1

  ; --- model stepping ---
  kt     timeinsts
  kseg   =      int(kt / giSeg)
  kidx   =      (kseg < giN ? kseg : giN - 1)

  aoL, aoR NAMProcess a1, a2, kidx, \
    "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/__PAPERS/Integrating Neural Amp Modeling Into Csound/cabbage-plugin/models", $AUTOV, 0

  ; --- peak 300 ms RMS in the settled window (archive methodology) ---
  krW    rms    aoL, 0.53          ; ~300 ms time constant
  krD    rms    a1,  0.53
  kinwin =      (kt - kseg*giSeg) > (giSeg - giMeas) ? 1 : 0
  kpkW   init   0
  kpkD   init   0
  if kinwin == 1 then
    kpkW = (krW > kpkW ? krW : kpkW)
    kpkD = (krD > kpkD ? krD : kpkD)
  endif

  ; --- at each segment boundary, write result for the finished segment ---
  klast  init   0
  if kseg != klast then
    SName NAMActiveName
    kerr  =  dbamp(kpkW + 1e-9) - dbamp(kpkD + 1e-9)
    fprintks "$OUTF", "%d\t%s\t%.2f\n", klast, SName, kerr
    kpkW = 0
    kpkD = 0
    klast = kseg
  endif

  outs aoL, aoR
endin

</CsInstruments>
<CsScore>
i1 0 573   ; 26 * 22 + 1 : one extra segment so model 24 gets written
e
</CsScore>
</CsoundSynthesizer>

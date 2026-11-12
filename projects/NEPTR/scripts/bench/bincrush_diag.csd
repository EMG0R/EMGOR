<CsoundSynthesizer>
<CsOptions>
-o /tmp/bincrush_diag.wav -W
</CsOptions>
<CsInstruments>
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif
#ifndef SCN
#define SCN #4#
#endif

#include "../../csound/globals.orc"
#include "../../csound/effects/bincrush.orc"

; Diagnostic: DRY on left, WET on right, so python can measure how much the
; effect actually changes a GUITAR-LIKE source (fundamental + decaying harmonics,
; slight inharmonicity + pick noise). SCN=pedal*4 (0..4). This exposes whether
; "keep loudest N bins" is audibly transparent on harmonic material.
instr 1
  gkBinBypass = 0
  gkBinPedal  = $SCN * 0.25

  ; pluck-ish E2 (82.4 Hz) with a handful of harmonics that decay at different
  ; rates (higher partials decay faster), plus a tiny bit of noise = guitar-ish
  kEnv   expon 1, 3, 0.02
  a1 poscil 0.40*kEnv,            82.41
  a2 poscil 0.26*kEnv*kEnv,       164.8
  a3 poscil 0.18*kEnv*kEnv,       247.1
  a4 poscil 0.10*kEnv*kEnv*kEnv,  329.6
  a5 poscil 0.06*kEnv*kEnv*kEnv,  412.0
  aN  noise 0.02*kEnv, 0
  aGtr = (a1+a2+a3+a4+a5+aN) * 0.5

  gaBinSendL = aGtr
  gaBinSendR = aGtr
  ; LEFT = dry (the send, 1 block old), RIGHT = wet
  outs aGtr, gaBinWetR
endin
</CsInstruments>
<CsScore>
i 121 0 3
i 1   0 3
</CsScore>
</CsoundSynthesizer>

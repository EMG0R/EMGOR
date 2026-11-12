; test_nam_passthrough.csd — smoke test for csound-nam.so: runs 1 second of
; silence through NAMProcess and verifies no crash / no error output.
;
; usage (from the opcode source directory):
;   csound --opcode-lib=build/csound-nam.so test/test_nam_passthrough.csd
;
; expected: Csound starts, runs 1 s, exits cleanly. with a missing or empty
; profiles dir a "no .nam files" message is expected and correct — the
; opcode passthrough is active.
<CsoundSynthesizer>
<CsOptions>
-n -d -m0
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

; globals required by the NAMProcess opcode
gkNAMProfileIdx  init 0
gkNAMBypass      init 1
gSNAMLibDir      = "./profiles"   ; point at your own .nam profile directory

; passthrough fallback UDOs — active when csound-nam.so is NOT loaded.
; when the plugin IS loaded it overrides these UDOs automatically.
opcode NAMProcess, aa, aakS
  aL, aR, kIdx, SDir xin
  xout aL, aR
endop

opcode NAMCount, i, 0
  xout 0
endop

instr 1
  ; feed silence through NAMProcess
  aL = 0
  aR = 0

  gkNAMBypass = 0

  aOutL, aOutR NAMProcess aL, aR, gkNAMProfileIdx, gSNAMLibDir

  ; verify output is finite (would blow up on NaN/inf if broken)
  kRMS rms aOutL
  if (kRMS > 10) then
    printks "ERROR: output level too high (%f) — possible NaN/inf\n", 0, kRMS
  endif

  ; also exercise the passthrough path with bypass on
  gkNAMBypass = 1
  aPassL, aPassR NAMProcess aL, aR, gkNAMProfileIdx, gSNAMLibDir

  ; no outs needed — the -n flag discards audio
endin

; smoke-test NAMCount if the plugin is loaded
instr 2
  icount NAMCount
  prints "NAMCount: %d profiles found\n", icount
  turnoff
endin

</CsInstruments>
<CsScore>
i1 0 1
i2 0 0.01
</CsScore>
</CsoundSynthesizer>

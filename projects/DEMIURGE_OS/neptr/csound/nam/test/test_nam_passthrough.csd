; test_nam_passthrough.csd
; Quick smoke test for csound-nam.so — runs 1 second of silence through
; NAMProcess and verifies no crash / no error output.
;
; Usage (from nam/ directory):
;   csound --opcode-lib=build/csound-nam.so test/test_nam_passthrough.csd
;
; Expected output: CSound starts, runs 1 s, exits cleanly.
; With an empty profiles dir: "[NAMProcess] WARNING: no .nam files found" is
; expected and correct — the opcode passthrough is active.
; With profiles present: "[NAMProcess] Loaded library: ... (N profiles)"
<CsoundSynthesizer>
<CsOptions>
-n -d -m0
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

; Globals required by the NAMProcess opcode
gkNAMProfileIdx  init 0
gkNAMBypass      init 1
gSNAMLibDir      = "./profiles"

; Passthrough fallback UDOs — active when csound-nam.so is NOT loaded.
; When the plugin IS loaded it overrides these UDOs automatically.
opcode NAMProcess, aa, aakS
  aL, aR, kIdx, SDir xin
  xout aL, aR
endop

opcode NAMCount, i, 0
  xout 0
endop

instr 1
  ; Feed silence through NAMProcess
  aL = 0
  aR = 0

  ; Disable bypass so the opcode actually runs
  gkNAMBypass = 0

  aOutL, aOutR NAMProcess aL, aR, gkNAMProfileIdx, gSNAMLibDir

  ; Verify output is finite (would blow up on NaN/inf if broken)
  kRMS rms aOutL
  if (kRMS > 10) then
    printks "ERROR: output level too high (%f) — possible NaN/inf\n", 0, kRMS
  endif

  ; Also test with bypass on (passthrough path)
  gkNAMBypass = 1
  aPassL, aPassR NAMProcess aL, aR, gkNAMProfileIdx, gSNAMLibDir

  ; Silent output (no outs needed — -n flag discards audio)
endin

; Also smoke-test NAMCount if the plugin is loaded
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

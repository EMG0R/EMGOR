; nam_signal_test.csd — DEMIURGE NAM functional test (offline, no hardware).
;
; WHY THIS EXISTS: the old csound/nam/test/test_nam_passthrough.csd fed SILENCE
; through NAMProcess and only checked for NaN/crash — so it passed even with the
; broken thread=5 build that muted audio (silence in -> silence out either way).
; It also defined a NAMProcess UDO, which in CSound 6.18 SHADOWS the plugin
; (gotcha #4), so it tested the stub, not the real opcode.
;
; THIS test fixes both: it feeds a real multi-tone signal, uses the REAL plugin
; (NO NAMProcess UDO defined here), and reports WET vs DRY RMS. A working NAM
; produces non-silent output that DIFFERS from the dry signal (amp saturation
; adds harmonics + changes level). A broken/silent build reports WET_RMS≈0 -> FAIL.
;
; Run (on the Pi, after building the plugin):
;   csound -n -m0 -d \
;     --opcode-lib=/usr/local/lib/csound/plugins64/csound-nam.so \
;     --omacro:NAMLIBDIR=/home/gorpi7/demiurge/profiles/nam \
;     testing/nam/nam_signal_test.csd
;
; NOTE (async loader vs offline render): the model loads on a background thread
; (~10-200 ms wall-clock for get_dsp). Offline render outruns wall-clock, so we
; run 8 s of audio and only sample RMS in the final second, by which point the
; model is loaded. If WET_RMS is ~equal to a fresh-boot passthrough, give it more
; seconds. test-nam.sh parses the WET_RMS/DRY_RMS/NAM_TEST lines below.
<CsoundSynthesizer>
<CsOptions>
-n -d -m0
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

; Globals the NAMProcess opcode reads. NO NAMProcess UDO here — we want the
; real plugin (loaded via --opcode-lib) to provide the opcode.
gkNAMProfileIdx  init 0
gkNAMBypass      init 1
gSNAMLibDir      = "$NAMLIBDIR"

gkWetRMS  init -1
gkDryRMS  init -1
giSine    ftgen 0, 0, 16384, 10, 1   ; sine table — functional oscili needs an explicit table on this build (else silent)

; deterministic test tone: three non-harmonic sines, guitar-ish band
opcode TestTone, a, 0
  aT = oscili(0.20, 220, giSine) + oscili(0.10, 587, giSine) + oscili(0.05, 1245, giSine)
  xout aT
endop

instr 1   ; WET = NAM(tone); DRY = the RAW tone (true reference). ONE instance —
          ; no dual-instance global race. NAMProcess processes once the model is
          ; loaded; compare its output vs the raw input tone.
  aTone TestTone
  gkNAMBypass     = 0
  gkNAMProfileIdx = 0
  aWetL, aWetR NAMProcess aTone, aTone, gkNAMProfileIdx, gSNAMLibDir
  gkWetRMS rms aWetL
  gkDryRMS rms aTone
endin

instr 99  ; verdict — sampled after the model has loaded.
          ; CSound gotcha: i-rate `prints` inside a k-rate if/then ALL fire at
          ; init regardless of branch. Snapshot the k-rate RMS into i-vars with
          ; i() and branch at i-rate so exactly ONE verdict prints.
  iWet = i(gkWetRMS)
  iDry = i(gkDryRMS)
  prints "WET_RMS=%f\n", iWet
  prints "DRY_RMS=%f\n", iDry
  ; PASS: wet is non-silent AND audibly different from dry (amp processing).
  if (iWet > 0.001 && abs(iWet - iDry) > 0.001) then
    prints "NAM_TEST=PASS\n"
  elseif (iWet <= 0.001) then
    prints "NAM_TEST=FAIL silent_output (thread/perf bug or model not loaded)\n"
  else
    prints "NAM_TEST=FAIL wet_equals_dry (opcode no-op / UDO shadow / null model)\n"
  endif
  turnoff
endin
</CsInstruments>
<CsScore>
; long render: NAM loads its model on a BACKGROUND thread (~10-200ms wall-clock).
; an offline render outruns wall-clock, so render long enough that the load
; completes in wall-clock before we sample (i99). 120 s render ≈ a few s wall.
i1 0 120
i99 115 0.1
</CsScore>
</CsoundSynthesizer>

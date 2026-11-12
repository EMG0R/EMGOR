<CsoundSynthesizer>
<CsOptions>
-o /tmp/tape_bench.wav -W -m0
</CsOptions>
<CsInstruments>
#include "../../csound/globals.orc"
#include "../../csound/effects/tape.orc"

; Scenarios (set with --omacro:SCN=N --omacro:SCN4AMT=x):
;  1 volume law: multitone, AMOUNT steps 0/.25/.5/.75/1 every 1.5 s
;  2 latency: single impulse at t=1, everything stochastic off
;  3 transport: 440 sine; pedal dive 2..3 s, hold to 5 s, release; check spin-up
;  4 generative stats: 440 sine, 30 s, AMOUNT = $SCN4AMT (0 or 1)
instr 1
  iScn = $SCN
  gkTapeBypass = 0
  ; bench input level: match live DRIVELEV journal value (memory: bench
  ; levels differ; saturation comp is level-dependent). Updated in Task 6
  ; from `journalctl -u demiurge | grep DRIVELEV | tail -1`.
  iAmp = 0.06 * 1.414

  if (iScn == 1) then
    kT = int(timeinsts() / 1.5)
    gkTapeAmount = limit:k(kT * 0.25, 0, 1)
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    a1 poscil iAmp*0.6, 196
    a2 poscil iAmp*0.5, 587
    a3 poscil iAmp*0.25, 2330
    aSig = a1 + a2 + a3
  elseif (iScn == 2) then
    gkTapeAmount = 0
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    ; impulse at t=3: clear of the t=0 metro/snag bench transient (kAmt
    ; ports from its 0.4 init default down to the scenario's 0 over ~50 ms,
    ; and metro fires at phase 0, so a snag can bump the gap early on)
    aSig mpulse 0.7, 999, 3
  elseif (iScn == 3) then
    gkTapeAmount = 0
    gkTapeFail = 0
    gkTapeModel = 0
    kPd = limit:k(timeinsts() - 2, 0, 1)            ; dive 2..3 s
    kPd = (timeinsts() > 5 ? 0 : kPd)               ; release at 5 s
    gkTapeExpr = kPd
    aSig poscil iAmp, 440
  else
    gkTapeAmount = $SCN4AMT
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    aSig poscil iAmp, 440
  endif

  aL, aR TapeMachine aSig, aSig
  outs aL, aR
endin
</CsInstruments>
<CsScore>
i 1 0 30
</CsScore>
</CsoundSynthesizer>

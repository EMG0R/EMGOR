; Ring Modulator (true bipolar ring mod)
; Signal chain position: 65
; Reads: gaRMSendL/R
; Writes: gaRMOutL/R
; Bypass: gkBypassRM
;
; True ring mod: output = input × sine carrier. Produces sum and difference
; frequencies (no original frequencies in output). gkARModDepth (0–1) scales
; the carrier amplitude — depth=1 is full ring mod.

instr 65
    aInL = gaRMSendL
    aInR = gaRMSendR
    aCarrier oscili gkARModDepth, gkARModFreq, gisine
    gaRMOutL = aInL * aCarrier
    gaRMOutR = aInR * aCarrier
endin

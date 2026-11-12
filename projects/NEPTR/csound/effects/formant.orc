; Formant Shifter
; Signal chain position: 120
; Reads: gaFormantSendL/R
; Writes: gaFormantOutL/R
; Bypass: N/A
;
; FFT size 4096 → 2048 halves inherent FFT latency (~42ms → ~21ms).
; Anti-alias prefilter cut from 4-pass butterlp to 2-pass (still 24dB/oct,
; enough headroom against aliasing when shifting down).

instr 120
aInL = gaFormantSendL
aInR = gaFormantSendR

iFFTsize = 2048
ioverlap = iFFTsize / 8
iwinsize = iFFTsize
iwintype = 1  ; Hanning window

kshift port gkFormantShift, 0.05
kformscale = pow(2, kshift / 12)

knyquist = sr * 0.5
kcutoff = (kformscale > 1) ? (knyquist / kformscale) : knyquist
kcutoff port kcutoff, 0.01

aFiltL = aInL
aFiltR = aInR
aFiltL butterlp aFiltL, kcutoff
aFiltL butterlp aFiltL, kcutoff
aFiltR butterlp aFiltR, kcutoff
aFiltR butterlp aFiltR, kcutoff

fsigL pvsanal aFiltL, iFFTsize, ioverlap, iwinsize, iwintype
fsigR pvsanal aFiltR, iFFTsize, ioverlap, iwinsize, iwintype

; Natural pitch/formant shift
f_natL pvscale fsigL, kformscale, 0, 1
f_natR pvscale fsigR, kformscale, 0, 1

; Formant-preserving compensation (shifts pitch back, preserves formants)
f_formL pvscale f_natL, 1/kformscale, 1, 1, 512
f_formR pvscale f_natR, 1/kformscale, 1, 1, 512

aOutL pvsynth f_formL
aOutR pvsynth f_formR

aOutL balance aOutL, aInL
aOutR balance aOutR, aInR

gaFormantOutL = aOutL
gaFormantOutR = aOutR
endin

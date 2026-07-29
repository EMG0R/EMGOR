<CsoundSynthesizer>
<CsOptions>
-odac
</CsOptions>

<CsInstruments>
sr = 44100
ksmps = 32
nchnls = 2
0dbfs = 1

giSine  ftgen 0, 0, 2^12, 10, 1
giNoise ftgen 1, 0, 2^12, 21, 1

gafxL init 0
gafxR init 0

instr 1 ; master rhythm driver
  ktime timeinsts
  kBPM = 50 + (100 * frac(ktime / 2))

  ksub = int(kBPM / 50)
  if (ksub < 1) then
    ksub = 1
  endif
  if (ksub > 4) then
    ksub = 4
  endif

  krate = ksub * kBPM / 60 * 2
  khatRate = kBPM / 60 * 4  ; 16ths

  ktrig = metro(krate)
  khtrig = metro(khatRate)

  krndKick = random:k(0, 1)
  krndSnare = random:k(0, 1)
  krndHat = random:k(0, 1)
  krndPan = random:k(0.2, 0.8)
  krndOpen = random:k(0, 1)

  kKickHz = random:k(40, 80)
  kSnrCar = random:k(80, 150)
  kSnrMod = random:k(200, 400)
  kSnrCut = random:k(400, 1000)

  kHatHz1 = random:k(300, 700)
  kHatHz2 = random:k(400, 800)
  kHatCut = random:k(2000, 4000)

  if (ktrig == 1) then
    if (krndKick < 0.5) then
      event "i", 2, 0, 0.15, kKickHz
    endif
  endif

  if (ktrig == 1 && krndSnare < 0.25) then
    event "i", 3, 0, 0.12, kSnrCar, kSnrMod, kSnrCut
  endif

  if (khtrig == 1 && krndHat < 0.6) then
    if (krndOpen < 0.3) then
      event "i", 4, 0, 0.08, kHatHz1, kHatHz2, krndPan, kHatCut, 1 ; open
    else
      event "i", 4, 0, 0.03, kHatHz1, kHatHz2, krndPan, kHatCut, 0 ; closed
    endif
  endif
endin

instr 2 ; kick (tonal, clean)
  iamp random 0.2, 0.8
  aenv line 1, p3, 0
  kcps expon p4, p3, 30
  asig oscil aenv * iamp, kcps, giSine
  gafxL += asig
  gafxR += asig
endin

instr 3 ; snare (distinct, low-pitched)
  aenv expseg 0.001, 0.01, 0.8, p3-0.02, 0.001
  anoise rand giNoise
  aMod oscil 1, p5, giSine
  aCar oscil aenv * 0.3, p4 + aMod * 50, giSine
  afilt butbp aCar + anoise * 0.2, p6, 500
  gafxL += afilt * 0.4
  gafxR += afilt * 0.4
endin

instr 4 ; hi-hat (tonal + noise, open/closed)
  iopen = p8
  if (iopen == 1) then
    aenv expseg 0.001, 0.01, 0.6, p3-0.01, 0.001 ; open hat envelope
  else
    aenv linseg 0, 0.005, 1, p3-0.01, 0.5, 0.005, 0 ; closed hat envelope
  endif

  a1 oscil aenv * 0.2, p4, giSine
  a2 oscil aenv * 0.2, p5, giSine
  anoise rand giNoise
  amix = (a1 + a2) * 0.5 + anoise * 0.2
  afilt buthp amix, p7
  aL = afilt * (1 - p6) * 0.12
  aR = afilt * p6 * 0.12
  gafxL += aL
  gafxR += aR
endin

instr 99 ; output
  outs gafxL, gafxR
  clear gafxL
  clear gafxR
endin

</CsInstruments>

<CsScore>
i 1 0 [3600*3600]
i 99 0 [3600*3600]
e
</CsScore>
</CsoundSynthesizer>
<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="background">
  <r>240</r>
  <g>240</g>
  <b>240</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>

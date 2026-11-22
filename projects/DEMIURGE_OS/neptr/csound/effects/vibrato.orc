; Vibrato — inline UDO for signal chain (zero bus overhead)
; Reads globals: gkVIB_freq, gkVIB_depth, gkVIB_fback, gisine

opcode VibratoPedal, aa, aa
  ainL, ainR xin
  kdepth  portk gkVIB_depth, 0.05
  kfback  portk gkVIB_fback, 0.02
  kfback  limit kfback, 0, 0.7

  aFbL    init 0
  aFbR    init 0

  a1 = ainL + aFbL * kfback
  a2 = ainR + aFbR * kfback

  aDelTim oscili kdepth, gkVIB_freq, gisine, 0
  aDelTim = aDelTim + kdepth
  iMaxDelay = 1
  aDelTap1  vdelayxw a1, aDelTim, iMaxDelay, 16
  aDelTap2  vdelayxw a2, aDelTim, iMaxDelay, 16

  aFbL = aDelTap1
  aFbR = aDelTap2

  xout aDelTap1, aDelTap2
endop

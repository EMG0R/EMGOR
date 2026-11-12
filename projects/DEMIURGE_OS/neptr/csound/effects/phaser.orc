; Phaser
; Signal chain position: 233
; Reads: gaPHA_sendL/R
; Writes: gaPHA_outL/R
; Bypass: gkPHA_mix

instr 233
kporttime       linseg       0,0.001,0.05
krate           portk        gkPHA_rate,kporttime
kdepth          portk        gkPHA_depth,kporttime
kfreq           portk        gkPHA_freq,kporttime
kfback          portk        gkPHA_fback,kporttime
ktrig           changed      gkPHA_shape,gkPHA_stages
if ktrig=1 then
 reinit RESTART_PHASER
endif
RESTART_PHASER:
a1              =            gaPHA_sendL
a2              =            gaPHA_sendR
aPhs1,aPhs2     PhaserSt     a1,a2,krate,kdepth*8,(kfreq*10)+4,i(gkPHA_stages),kfback*0.9,i(gkPHA_shape)
rireturn
aOut1           ntrpol       a1,aPhs1,gkPHA_mix
aOut2           ntrpol       a2,aPhs2,gkPHA_mix
; +15% makeup gain — the notches (and feedback) shave level off the wet
; signal; this restores roughly unity so toggling the phaser doesn't dip.
gaPHA_outL      =            aOut1 * 1.15
gaPHA_outR      =            aOut2 * 1.15
endin

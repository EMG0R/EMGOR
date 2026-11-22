; Flanger
; Signal chain position: 222
; Reads: gaFLA_sendL/R
; Writes: gaFLA_outL/R
; Bypass: gkFLA_mix

instr 222
krate           =            gkFLA_rate
kdepth          =            gkFLA_depth
kdelay          =            gkFLA_offset
kfback          =            gkFLA_fback
klfoshape       =            gkFLA_lfoshape
kmix            =            gkFLA_mix
a1              =            gaFLA_sendL
a2              =            gaFLA_sendR

afla1           Flanger      a1,krate,kdepth,kdelay,kfback,klfoshape
afla2           Flanger      a2,krate,kdepth,kdelay,kfback,klfoshape
gaFLA_outL      =            afla1
gaFLA_outR      =            afla2
endin

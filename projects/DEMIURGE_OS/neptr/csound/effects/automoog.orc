; Automoog (Envelope Follower)
; Signal chain position: 51
; Reads: gaAUTOMOOG_sendL/R
; Writes: gaAUTOMOOG_outL/R
; Bypass: N/A

instr 51
ksens           =            gkAUTOMOOG_sens
katt            =            gkAUTOMOOG_att
krel            =            gkAUTOMOOG_rel
kfreq           =            gkAUTOMOOG_freq
ktype           =            gkAUTOMOOG_type
kres            =            gkAUTOMOOG_res
kdist           =            gkAUTOMOOG_dist
klevel          =            gkAUTOMOOG_level
a1              =            gaAUTOMOOG_sendL
a2              =            gaAUTOMOOG_sendR

a1              EnvelopeFollower  a1, ksens, katt, krel, kfreq, ktype, kres * 0.95, kdist * 100
a2              EnvelopeFollower  a2, ksens, katt, krel, kfreq, ktype, kres * 0.95, kdist * 100
a1              =                 a1 * klevel * (1 - ((kdist * 0.3) ^ 0.02))                     ; scale amplitude according to distortion level (to compensate for gain increases it applies)
a2              =                 a2 * klevel * (1 - ((kdist * 0.3) ^ 0.02))
gaAUTOMOOG_outL =                 a1
gaAUTOMOOG_outR =                 a2
endin

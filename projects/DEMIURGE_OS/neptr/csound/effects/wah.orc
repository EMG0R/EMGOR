; WAH Pedal (Manual)
; Signal chain position: 52
; Reads: gaWAH_sendL/R
; Writes: gaWAH_outL/R
; Controls: gkWAH_freq (CC29 expression pedal 0-1), gkWAH_res (resonance)
;
; moogladder swept by expression pedal + post-filter tanh saturation for
; classic gritty wah character. Pedal scaling chosen so both heel and toe
; positions feel meaningful, with extra travel in the musical sweet spot.

instr 52
a1              =            gaWAH_sendL
a2              =            gaWAH_sendR

; Pedal 0–1 → 220–3200 Hz with a mild concave curve (more travel in the
; 300–1500 Hz vowel range; heel firmly bass, toe clearly bright).
kfreqRaw        portk        gkWAH_freq, 0.015
kfreqCurved     =            pow(kfreqRaw, 0.85)
kfreq           =            220 * pow(14.5, kfreqCurved)
kfreq           limit        kfreq, 220, 3200

; Resonance floor at 0.5 — keeps the wah always vocal even at low Q.
kres            =            gkWAH_res
kres            limit        kres, 0.5, 0.97

aOutL           moogladder   a1, kfreq, kres
aOutR           moogladder   a2, kfreq, kres

; Post-filter grit: gentle tanh saturation. Subtle at low res, gets more
; present as resonance peaks push into the saturator.
aOutL           =            tanh(aOutL * 1.5) * 0.75
aOutR           =            tanh(aOutR * 1.5) * 0.75

gaWAH_outL      =            aOutL
gaWAH_outR      =            aOutR
endin

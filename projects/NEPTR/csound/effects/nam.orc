;==============================================================
; NAM AMP SIM — Menu 8
;
; An amp sim is 100% wet — no dry/wet mix knob (removed 2026-07-27). The
; two controls are gkNAMInGain (dB, -12..+18, pre-NAMProcess drive trim)
; and gkNAMOutGain (dB, -18..+18, post-NAMProcess level trim), both
; portk-smoothed. Actual signal routing lives in signal_chain.orc's
; "NAM AMP SIM (Menu 8)" stage (this file is documentation-only — see
; the NAMProcess warning below).
;
; NAMProcess is provided by csound-nam.so (system plugin).
; Build:   csound/nam/build-on-pi.sh
; Install: /usr/local/lib/csound/plugins64/csound-nam.so
;
; WARNING: Do NOT define a UDO named NAMProcess here.
; In CSound 6.18 a UDO with the same name as a loaded plugin opcode wins,
; producing passthrough (silence from the amp sim) even when the .so is
; installed. The opcode below is intentionally commented out.
;
; opcode NAMProcess, aa, aakS
;   aL, aR, kIdx, SDir xin
;   xout aL, aR
; endop
;==============================================================

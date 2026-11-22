;==============================================================
; SPIKE PROBE — per-k-cycle engine time, reported over UDP (2026-10-23)
;
; Why: csound misses ~1 deadline per 3 min at quantum 128 (2.67 ms) while the
; mean busy is ~1.4 ms, so something makes single cycles 3-6x longer. Each
; miss can make PipeWire restart the HAT stream, which can trip the kernel
; DMA bug and silence the outputs. This names the cycle and what was going
; on, so the spike source is measured, not guessed.
;
; How: signal_chain.orc instr 1 (runs first each cycle) stamps gkSpkT0 with
; rtclock. instr 9999 is the highest instr number in the orchestra, so it
; runs last each cycle; its rtclock minus gkSpkT0 is
; the time spent in instruments this cycle (excludes csound's own overhead
; and instr 1's first line). Nothing touches disk on this thread: slow cycles
; go out as one OSC packet each to 127.0.0.1:9177, read by
; ~/PRIME_RECOVERY/spike/spike_logger.py, plus a once-per-second summary.
;
; Tags sent with each spike: menu, loop-1 speed idx, preset-recall flag, OSC
; refresh flag, IR row 0, active instance count.
;==============================================================

instr 9999   ; SpikeProbe — numbered, not named: named instrs fill numbering gaps (got 3), this must run LAST
  kDt     = (rtclock:k() - gkSpkT0) * 1000          ; ms
  kN      init 0
  kSpk    init 0
  kMax    init 0
  kSum    init 0
  kLast   init 0
  kN     += 1
  kSum   += kDt
  kMax    = max(kMax, kDt)
  if kDt > 1.8 then
    kSpk += 1
    OSCsend kSpk, "127.0.0.1", 9177, "/spike", "fffffff", kDt, gkCurrentMenu, gkLpSpIdx1, gkPresetRecall, gkOscRefresh, gkIRSel[0], active:k(0)
  endif
  kT timeinsts
  if kT - kLast >= 1 then
    kLast = kT
    OSCsend kT, "127.0.0.1", 9177, "/spkstat", "fff", kMax, kSum / kN, kN
    kMax = 0
    kSum = 0
    kN   = 0
  endif
endin
schedule 9999, 0, -1

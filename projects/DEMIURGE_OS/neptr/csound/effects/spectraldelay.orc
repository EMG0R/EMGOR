; Spectral Delay
; Signal chain position: 200
; Reads: gaSpecDelSendL/R
; Writes: gaSpecDelWetL/R
; Bypass: N/A
;
; BINSCATTER (split out of SPECDLY, see docs/specs/2026-07-26-delay-verb-
; expansion.md section 1) — same PV engine, but the per-bin delay-time table
; is filled with RANDOM values each reroll instead of SPECDLY's static
; table, giving each FFT bin its own delay ("scatter" smear).
; Signal chain position: 201
; Reads: gaSpecDelSendL/R (shared bus w/ SPECDLY, both can run at once)
; Writes: gaBscWetL/R
; Bypass: gkBscBypass (self-declared here; 1 = bypassed, house convention —
;   integration wires it into signal_chain.orc's $FX_BYPASS macro like the
;   other effects; this file does not gate on it directly)
;
; New globals declared below (integration reads these names):
;   gkBscBypass   init 1     ; 1 = bypassed
;   gkBscMix      init 0.5   ; P1 dry/wet (consumed externally like gkSpecDelMix)
;   gkBscScatter  init 0.7   ; P2 scatter amount 0..1 (0=equal bins, 1=full random)
;   gkBscFB       init 0.95  ; P3 feedback, soft-clamped 0.5..1.1 in-instr
;   gkBscMaxDel   init 0.15  ; P4 max delay window, seconds
;   gaBscWetL/R   init 0     ; wet outs

gkBscBypass   init 1
gkBscMix      init 0.5
gkBscScatter  init 0.7
gkBscFB       init 0.95
gkBscMaxDel   init 0.15
gaBscWetL     init 0
gaBscWetR     init 0

; per-bin random delay-time table for BINSCATTER (own table, independent of
; SPECDLY's giSpecDelFrqTab/giSpecDelAmpTab; filled/reroll'd at i-time
; below). Used for BOTH pvsbufread2 table args (bin-amplitude delay AND
; bin-frequency delay) so each bin gets ONE coherent random delay rather
; than an amplitude read and a frequency read from different times, which
; is the "each bin gets its own delay" smear the spec calls for.
giBscFrqTab ftgen 0, 0, giFFT, -2, 0
giBscSeed   init 0
giBscTrim   init 0.84  ; output trim, tuned via bench harness (see docs/specs) —
                        ; brings the wet-only RMS at default params (mix=1,
                        ; fb=0.95, scatter=0.7, maxdel=0.15) to within ~0 dB
                        ; of dry on a broadband sweep test signal; a static
                        ; narrowband sine can still read ~1-2 dB either side
                        ; since this is a frequency-domain effect (expected —
                        ; comparable to any comb/spectral filter's response
                        ; varying with input spectral content, not a flat
                        ; broadband gain error)

instr 200
  ; deterministic, static per-bin table (giSpecDelFrqTab) — no randomness
  ; here, kept byte-compatible in sound at defaults. Ranges cleaned per spec:
  ; musical maxdel 21 ms - 1 s, fb <= 0.9 (defaults 0.3 / 0.8 both already
  ; inside range, so this is a no-op at defaults).
  kMaxDelSm  portk gkSpecDelMaxDel, gkPortTime
  kMaxDelSm  limit kMaxDelSm, 0.021, 1.0
  kFdbkRaw   portk gkSpecDelFB,     gkPortTime
  kFdbk      limit kFdbkRaw, 0, 0.9

  kMaxDelMapped = kMaxDelSm

  kMaxDelQuant = int(kMaxDelMapped * 1000) * 0.001

  if changed(kMaxDelQuant) == 1 then
    reinit RESTART
  endif

RESTART:
  iFFT    = giFFT
  iHop    = giHop
  iWin    = giWin
  iFormat = giFormat

  iReqMax = i(kMaxDelQuant)

  iMin    = iFFT / sr
  iMaxDelay = (iReqMax < iMin ? iMin : iReqMax)

  aInL = gaSpecDelSendL
  aInR = gaSpecDelSendR

  fsigOutL pvsinit    iFFT, iHop, iWin, iFormat
  fsigInL  pvsanal    aInL,  iFFT, iHop, iWin, iFormat
  fsigFBL  pvsgain    fsigOutL, kFdbk
  fsigMixL pvsmix     fsigInL, fsigFBL
  iHandleL, kTimeL pvsbuffer fsigMixL, iMaxDelay
  fsigOutL pvsbufread2 kTimeL, iHandleL, giSpecDelAmpTab, giSpecDelFrqTab
  aWetL    pvsynth    fsigOutL

  fsigOutR pvsinit    iFFT, iHop, iWin, iFormat
  fsigInR  pvsanal    aInR,  iFFT, iHop, iWin, iFormat
  fsigFBR  pvsgain    fsigOutR, kFdbk
  fsigMixR pvsmix     fsigInR, fsigFBR
  iHandleR, kTimeR pvsbuffer fsigMixR, iMaxDelay
  fsigOutR pvsbufread2 kTimeR, iHandleR, giSpecDelAmpTab, giSpecDelFrqTab
  aWetR    pvsynth    fsigOutR

  gaSpecDelWetL = aWetL
  gaSpecDelWetR = aWetR
endin

; BINSCATTER — instr 201. Same PV engine/topology as SPECDLY (instr 200)
; above, but the per-bin delay-time table (giBscFrqTab) is refilled with
; random values on reroll (bypass-off transition, scatter-knob move, or
; max-delay-window change) instead of reading the static giSpecDelFrqTab.
; kScatter blends each bin between "equal" (0.5, i.e. all bins read the same
; mid-window delay = SPECDLY-like coherence) and "full random" (uniform
; 0..1 per bin = maximum smear). gkBscMix is NOT read here (external
; dry/wet crossfade, same convention as gkSpecDelMix / instr 200).
instr 201
  kMaxDelSm  portk gkBscMaxDel, gkPortTime
  kMaxDelSm  limit kMaxDelSm, 0.02, 1.0
  kFdbkRaw   portk gkBscFB,    gkPortTime
  kFdbk      limit kFdbkRaw, 0.5, 1.1
  kScatter   portk gkBscScatter, gkPortTime
  kScatter   limit kScatter, 0, 1

  ; soft feedback limiter: fb up to 1.1 is genuinely unstable in the
  ; spectral-magnitude domain (pvsgain is a linear per-bin multiply, so
  ; fb >= 1.0 diverges geometrically hop-over-hop with nothing to stop it —
  ; unlike the audio-domain feedback loops elsewhere in the house style,
  ; there is no tanh available here per the "no saturation outside the
  ; specified spots" mandate). Throttle kFdbk itself (a gain-automation/
  ; dynamics move, not waveshaping) from the previous block's output
  ; envelope (kBscEnv, updated at the end of this instr) so it backs off
  ; smoothly once the wet signal approaches a safety ceiling, and is
  ; transparent (kFdbkSafe == kFdbk) well under that ceiling.
  kBscEnv    init 0
  kFdbkSafe  = kFdbk / (1 + limit:k((kBscEnv - 1.2) / 0.8, 0, 20))

  kMaxDelQuant  = int(kMaxDelSm * 1000) * 0.001
  kScatterQuant = int(kScatter  * 100)  * 0.01

  kBypNow = gkBscBypass
  kBypPrevBsc init 1

  kReroll = 0
  if changed(kMaxDelQuant) == 1 then
    kReroll = 1
  endif
  if changed(kScatterQuant) == 1 then
    kReroll = 1
  endif
  if (kBypPrevBsc == 1 && kBypNow == 0) then
    kReroll = 1
  endif
  kBypPrevBsc = kBypNow

  if kReroll == 1 then
    reinit RESTART_BSC
  endif

RESTART_BSC:
  iFFT    = giFFT
  iHop    = giHop
  iWin    = giWin
  iFormat = giFormat

  iReqMax = i(kMaxDelQuant)
  iMin    = iFFT / sr
  iMaxDelay = (iReqMax < iMin ? iMin : iReqMax)

  ; reroll the per-bin random delay-time table. NOTE: pvsbufread2's ift2
  ; table holds ABSOLUTE per-bin delay in SECONDS (bin-indexed), not a 0..1
  ; fraction (matches SPECDLY's giSpecDelFrqTab convention, GEN21 range
  ; 0..0.3s). So values here must be scaled to iMaxDelay, the actual buffer
  ; length, or they get silently clamped/wrapped by the circular buffer and
  ; scatter has no audible effect. Blend equal(iMaxDelay/2 for every bin) ->
  ; full random(0..iMaxDelay per bin) by iScatter. Seeded off an
  ; incrementing counter so repeated rerolls at the same scatter amount
  ; still vary but stay reproducible run to run relative to each other
  ; ("repeatable-ish").
  iScatter = i(kScatterQuant)
  giBscSeed = giBscSeed + 1
  seed giBscSeed
  iBin = 0
  while iBin < iFFT do
    iRand random 0, 1
    iVal  = iMaxDelay * (0.5 + (iRand - 0.5) * iScatter)
    tableiw iVal, iBin, giBscFrqTab
    iBin += 1
  od

  aInL = gaSpecDelSendL
  aInR = gaSpecDelSendR

  fsigOutBscL pvsinit    iFFT, iHop, iWin, iFormat
  fsigInBscL  pvsanal    aInL,  iFFT, iHop, iWin, iFormat
  fsigFBBscL  pvsgain    fsigOutBscL, kFdbkSafe
  fsigMixBscL pvsmix     fsigInBscL, fsigFBBscL
  iHandleBscL, kTimeBscL pvsbuffer fsigMixBscL, iMaxDelay
  fsigOutBscL pvsbufread2 kTimeBscL, iHandleBscL, giBscFrqTab, giBscFrqTab
  aWetBscL    pvsynth    fsigOutBscL

  fsigOutBscR pvsinit    iFFT, iHop, iWin, iFormat
  fsigInBscR  pvsanal    aInR,  iFFT, iHop, iWin, iFormat
  fsigFBBscR  pvsgain    fsigOutBscR, kFdbkSafe
  fsigMixBscR pvsmix     fsigInBscR, fsigFBBscR
  iHandleBscR, kTimeBscR pvsbuffer fsigMixBscR, iMaxDelay
  fsigOutBscR pvsbufread2 kTimeBscR, iHandleBscR, giBscFrqTab, giBscFrqTab
  aWetBscR    pvsynth    fsigOutBscR

  ; update the safety envelope AFTER synthesis (1-block-lag feedback throttle,
  ; standard AGC technique — reads next control period, not this one, so it
  ; can never see its own contribution instantaneously; converges within a
  ; handful of ksmps blocks, well inside audio-rate reaction time).
  kBscEnv rms (aWetBscL + aWetBscR) * 0.5, 15

  ; output trim: high feedback (up to 1.1) + full-random per-bin scatter can
  ; build up broadband energy vs. SPECDLY's gentler defaults; trim tuned via
  ; bench harness so on/bypass RMS sits within the ±1 dB mandate at defaults.
  gaBscWetL = aWetBscL * giBscTrim
  gaBscWetR = aWetBscR * giBscTrim
endin

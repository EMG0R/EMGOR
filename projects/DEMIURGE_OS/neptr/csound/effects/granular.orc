; Granular Processor + Grain Generator
; Signal chain position: 80, 81
; Reads: gaGranSendL/R
; Writes: gaGranWetL/R (accumulated by grain instances in instr 81)
; Bypass: gkGranBypass
;
; Restored from pre-merge backup (2026-06-10). The DEMIURGE_OS sndwarp
; version that was merged in sounded wrong and crackled at short grain sizes.
; This is the original schedkwhen design. kDens reduced 1200 -> 80 for
; ~32 concurrent grains (dense wash, ~15x cheaper). Everything else is
; identical to the pre-merge version.
;
; 2026-07-26 "chill vibe" rework (user call — the varispeed up-shift grains
; sounded chipmunk-y):
;   * PITCH-SCATTER grains now read a SECOND ring buffer holding an
;     H90Shift-processed copy of the input (shared UDO, globals.orc) at
;     normal playback rate — modern PV pitch quality, granular texture.
;     gkGranPitch still sets the shift amount (P4, unchanged semantics).
;   * NEW: some grains (~15%) play a varispeed OCTAVE DOWN (0.5x) from the
;     normal buffer — the down direction is the tape-warm one, kept analog.
;   * More reverse: 80% -> 88% of grains play backwards.
;   * Bigger grains: floor 5 ms -> 40 ms, random draw biased toward the max
;     (rnd^0.6), and the boot default gkGranSize is raised in globals.orc.

; shifted-copy ring buffer (mono — grains pan anyway); same length as giBufL
giGranSh ftgen 0, 0, 1048576, -2, 0

;==============================================================
; GRANULAR PROCESSOR
;==============================================================
instr 80
    aInL = gaGranSendL
    aInR = gaGranSendR

    ilen = ftlen(giBufL)
    aptr phasor sr / ilen
    aptr *= ilen
    tablew aInL, aptr, giBufL
    tablew aInR, aptr, giBufR

    ; H90-shifted mono copy for the pitch-scatter grains (16 ms PV latency —
    ; inaudible under the 0-700 ms random grain delay)
    aMonoIn  = (aInL + aInR) * 0.5
    kShRatio = semitone(gkGranPitch)

    ; UNITY BYPASS (2026-08-12, perf). H90Shift is a full phase-vocoder
    ; pipeline and it ran every cycle to fill a buffer only the PITCHED grains
    ; read — kTrns1 = 0.15, so 15% of them. Worse, at the default pitch of 0
    ; semitones kShRatio is exactly 1.0 and the whole PV was computing an
    ; identity: analysis and resynthesis of a signal to get the signal back.
    ; At unity we now write the input straight through.
    ;
    ; The two behaviours are not bit-identical — H90Shift carries ~16 ms of PV
    ; latency that the direct write does not — but this file's own header
    ; already rules that inaudible here, since grains read the buffer at a
    ; random 0-700 ms delay. Shifting the read origin by 16 ms inside a 700 ms
    ; random window is not a perceptible difference.
    ;
    ; kWarmSh spins the PV up for 25 ms before it is needed: a cold pvsanal
    ; needs a full 1024-sample frame (~21 ms) to produce valid output, so
    ; moving the pitch encoder off zero must not hand grains an empty buffer.
    kShPrev init 1
    kWarmSh init 0
    if (abs(kShRatio - kShPrev) > 0.0005) then
      kWarmSh = 0.025 * kr
    elseif (kWarmSh > 0) then
      kWarmSh = kWarmSh - 1
    endif
    kShPrev = kShRatio

    aSh init 0
    if (abs(kShRatio - 1.0) > 0.001 || kWarmSh > 0) then
      aSh    H90Shift aMonoIn, kShRatio
    else
      aSh    = aMonoIn
    endif
    tablew aSh, aptr, giGranSh

    kptr downsamp aptr

    kGSize1   = 0.04
    kGSize2   = gkGranSize   ; max grain size — encoder P3
    kDens     = gkGranDens   ; grain density — encoder P2
    ; MINIMUM DELAY 10 ms (2026-08-12) — was 0, and 0 is the crackle.
    ;
    ; A grain scheduled kDly ahead reads exactly kDly BEHIND the write head, and
    ; that gap is constant for a forward grain and grows for a reversed one, so
    ; grains never overrun. The problem is the sample they interpolate TOWARD.
    ; `tablei` reads between index and index+1; at kDly = 0 that neighbour is
    ; one sample PAST the write head, which in this 1048576-frame ring has not
    ; been written since the previous lap — audio from 21.8 seconds ago. Every
    ; such grain splices the current signal against 21.8-second-old signal. That
    ; is a hard click, not a subtle one.
    ;
    ; And it is the COMMON case, not an edge case: kRndDly is passed through
    ; `expcurve ..., 100` below, which biases the distribution hard toward zero,
    ; so most grains were landing in exactly the broken region.
    ;
    ; 10 ms is far more than the one sample of interpolation plus k-rate
    ; scheduling granularity that has to be cleared, and far less than the
    ; 0-700 ms spread it sits inside, so the character is unchanged. An earlier
    ; attempt floored this at one full grain length (kGSize2) on a wrong model
    ; of the failure; at large grain sizes that collapsed the delay range to a
    ; narrow band and the engine stopped sounding random. Do not raise it.
    kDly1     = 0.01
    kDly2     = 0.7
    kTrns1    = 0.15         ; probability a grain is a PITCHED (H90) grain
    kTrns2    = gkGranPitch  ; (kept in the p-list for compat; instr 81 reads
                             ;  the shifted buffer instead of varispeeding)
    kPanSpread  = 1
    kAmpSpread  = 1
    kFiltSpread = 1
    kampdecay = 0.5
    kreverse  = 0.88         ; probability a grain plays backwards
    kwindow   = 1
    kDlyDst   = 1
    klevel    = 1

    kRnd random 0, 1
    kRnd pow kRnd, 0.6       ; bias toward bigger grains (chill)
    kGSize = kGSize1 + kRnd * (kGSize2 - kGSize1)

    kRndDly random 0, 1
    kRndDly expcurve kRndDly, 100
    kDly = kDly1 + kRndDly * (kDly2 - kDly1)

    ktrig metro kDens
    schedkwhen ktrig, 0, 0, 81, kDly + 0.0001, kGSize, kptr, kPanSpread, kreverse, \
               kDly, kDly1 + 1/sr, kDly2 + 0.0001, kampdecay, klevel, kwindow, \
               kAmpSpread, kFiltSpread, kTrns1, kTrns2
endin

;==============================================================
; GRANULAR GRAIN PLAYER
;==============================================================
instr 81
    iGStart  = p4
    ispread  = p5
    ireverse = (random(0, 1) > p6 ? 1 : -1)
    iwindow  table p12 - 1, giwindows
    idly     = p7
    iMinDly  = p8
    iMaxDly  = p9
    iampdecay = p10
    ilevel   = p11
    iAmpSpr  = p13
    iFiltSpr = p14
    ; 2026-07-26 grain flavors (see header):
    ;   PITCHED grain (prob p15): reads the H90-shifted buffer at 1x —
    ;     modern PV shift instead of the old varispeed chipmunk (p16 unused).
    ;   OCTAVE-DOWN grain (~15% of the rest): varispeed 0.5x from the normal
    ;     buffer — tape-warm down-octave, deliberately analog.
    ; OCTAVE-DOWN GRAINS REMOVED (2026-08-12, user call). ~15% of non-pitched
    ; grains used to be varispeed'd to 0.5x — an unrequested octave drop that
    ; fires at random and cannot be dialled out from the front panel, so the
    ; engine wandered below the pitch the player had set. Granular pitch is now
    ; ONLY what gkGranPitch asks for: a grain either reads the H90-shifted
    ; buffer (which carries that setting) or the plain buffer at unity. Nothing
    ; transposes itself.
    ;
    ; iRto is kept rather than folded away because `aline` below still scales
    ; its ramp by it, and a future intentional varispeed belongs at this exact
    ; spot — as a parameter, not a dice roll.
    iShift = (random(0, 1) < p15 ? 1 : 0)
    iRto   = 1

    iRtoAmp divz idly - iMinDly, iMaxDly - iMinDly, 0
    iamp = (1 - iRtoAmp) ^ 2
    iamp ntrpol 1, iamp, iampdecay
    iRndAmp random 1 - iAmpSpr, 1
    iamp = iamp * iRndAmp
    aenv oscili iamp, 1/p3, iwindow

    aline line iGStart, p3, iGStart + (p3 * iRto * sr * ireverse)
    if iShift == 1 then
        aL tablei aline, giGranSh, 0, 0, 1
        aR = aL
    else
        aL tablei aline, giBufL, 0, 0, 1
        aR tablei aline, giBufR, 0, 0, 1
    endif

    if iFiltSpr > 0 then
        iRndCfOct random 14 - (iFiltSpr * 10), 14
        iRndCf = cpsoct(iRndCfOct)
        aL butlp aL, iRndCf
        aR butlp aR, iRndCf
    endif

    ipan random 0.5 - (ispread * 0.5), 0.5 + (ispread * 0.5)
    gaGranWetL += aL * aenv * ipan * ilevel
    gaGranWetR += aR * aenv * (1 - ipan) * ilevel
endin

; TAPE DELAY — Space Echo/EP-3-accurate tape echo
; Signal chain position: instr 190 (DELAY menu, code tdl)
; Reads:  gaTdlInL/R        Writes: gaTdlWetL/R        Bypass: gkTdlBypass
; Toggle: 68                Spec:   docs/specs/2026-07-26-delay-verb-expansion.md
;                                   section 2 (TAPE DELAY) — LOCKED spec.
;
; Self-contained effect file — globals.orc / signal_chain.orc / handlers /
; csd / UI / VST are NOT touched here. All globals this file needs are
; self-declared immediately below (integration pass wires them into the
; midi handler, OSC, UI and signal_chain like every other effect here).
;
; Signal flow (per spec, hiss REMOVED 2026-08-23 — see note below):
;   in -> HP 40 Hz -> [+ feedback] -> pre-shelf +6 dB@2k -> subtle tanh
;     drive (DC-guarded, g=2.0 default) -> de-shelf -6 dB@2k -> delay line
;   reads: head1 @ motor-inertia-slewed time + wow + flutter + drift;
;          head2 = 2x head1, head3 = 3x head1 (physical multi-head spacing)
;   wet = headmask . taps -> LOOP FILTER (P4 center / P5 width) -> tone LP
;     (speed-dependent 2.5-5 kHz) -> HP 100 Hz -> head-bump pareq +3 dB @
;     90*speed_ratio Hz Q1 -> notch -1.5 dB @ 2x bump -> wear AM (~5.5 Hz
;     +/-0.7 dB)
;   feedback = wet * kFB (0..1.15 — self-oscillation is self-limited by the
;     tanh drive stage the feedback signal passes back through every pass)
;   out = equal-power dry/wet (sqrt crossfade, unity loudness at defaults)
;
; 2026-08-23 (user: "on the tape delay take out the bs white noise it is
; dramatic and bad"): the in-loop hiss bed (white noise -> LP 3.5k -> -60dB,
; summed into every repeat inside the feedback loop) is REMOVED entirely —
; not muted, not defaulted-off, the noise generators are gone. Every repeat
; still darkens/wobbles via the existing head-bump/notch/tone-LP chain and
; the wear-AM tremolo; there's just no added noise floor riding along with
; it. See the WEAR AM section below (hiss used to feed into it; now the
; head-bump/notch output feeds it directly).
;
; Motor inertia: SPEED (not time) is what slews. A fixed reference head
; distance divided by a portk-chased "motor speed" reproduces the real
; asymmetric pitch glide a Space Echo makes when you spin the speed knob —
; because delay-time = distance / speed is nonlinear in speed, glide-up and
; glide-down covering the same time-delta take different amounts of time.
;
; Encoder mapping (P1-P6; integration wires these into the takeover system):
;   P1  gkTdlMix       0..1     wet/dry (equal-power law)
;   P2  gkTdlTimeMs     60..1200 ms   head-1 target time; soft-snaps toward
;                                     gkChaseBlissSubdivSec grid multiples
;                                     when a master loop is running
;   P3  gkTdlFeedback  0..1.15  feedback amount (>1 = emergent self-osc)
;   P4  gkTdlFiltCtr   250..5000 Hz (log) loop-filter CENTER
;   P5  gkTdlFiltQ     0.5..6         loop-filter WIDTH/Q (0.5 = wide,
;                                     nearly transparent; 6 = narrow,
;                                     telephone-ify band-limited repeats)
;   P6  gkTdlHeadMode  1..7 stepped   head mask: 1,2,3,2+3,1+2,1+3,1+2+3
;   (gkTdlDrive is NOT on an encoder — fixed subtle character control,
;   default 2.0 per the global saturation mandate; expose later if wanted)
;
; Budget target (spec): delayr/delayw-equivalent (3x vdelay3 stereo) + ~5
; oscili + 2 randi-family + portk + 2 tone/butter LP/HP + 4 pareq + tanh —
; comfortably under the ~3% Pi-core target.

;==============================================================
; self-declared globals (nothing added to globals.orc)
;==============================================================
gkTdlBypass    init 1        ; 1 = bypassed (house convention: 1 = bypassed)
gkTdlMix       init 0.35     ; P1 — wet mix 0..1
gkTdlTimeMs    init 300      ; P2 — head-1 delay time target, ms (60..1200)
gkTdlFeedback  init 0.35     ; P3 — feedback 0..1.15
gkTdlFiltCtr   init 1200     ; P4 — loop-filter center Hz (250..5000, log)
gkTdlFiltQ     init 0.7      ; P5 — loop-filter Q (0.5..6)
gkTdlHeadMode  init 1        ; P6 — stepped 1..7 head mask index
gkTdlDrive     init 2.0      ; saturation drive g (subtle; mandate: g<=2.5)
gaTdlInL       init 0        ; input bus (fed by signal_chain.orc)
gaTdlInR       init 0
gaTdlWetL      init 0        ; wet output bus
gaTdlWetR      init 0

instr 190
    aInL = gaTdlInL
    aInR = gaTdlInR

    ; ---- click-free bypass crossfade (house pattern): kByp 0 = engaged,
    ; 1 = bypassed/dry. The whole engine keeps running underneath so the
    ; tape stays "warm" and re-enabling never replays a stale/frozen buffer.
    kBypTgt = (gkTdlBypass >= 0.5 ? 1 : 0)
    iBypInit = (i(gkTdlBypass) >= 0.5 ? 1 : 0)
    kByp portk kBypTgt, 0.02, iBypInit

    ;==========================================================
    ; P2 TIME — optional soft grid-snap, motor-inertia SPEED slew
    ;==========================================================
    kTimeMsRaw portk gkTdlTimeMs, 0.02
    kTimeMsRaw limit kTimeMsRaw, 60, 1200

    ; soft-snap toward nearest gkChaseBlissSubdivSec multiple; only pulls
    ; when a master loop exists (mandate 4), and only partially (soft, not
    ; a hard quantize) so the knob still feels continuous.
    kGridSec  limit gkChaseBlissSubdivSec, 0.03, 2.0
    kGridMs   = kGridSec * 1000
    kNearestN = round(kTimeMsRaw / kGridMs)
    kNearestN = (kNearestN < 1 ? 1 : kNearestN)
    kGridTgt  = kNearestN * kGridMs
    kGridTgt  limit kGridTgt, 60, 1200
    kSnapAmt  = (gkLpMasterLen > 0 ? 0.35 : 0)
    kTimeMsTgt = kTimeMsRaw + (kGridTgt - kTimeMsRaw) * kSnapAmt

    ; reference head-distance (seconds) — arbitrary constant that cancels
    ; out; chosen so a 300 ms knob setting = unity motor speed.
    iDistRef = 0.3
    kTimeSecTgt = kTimeMsTgt / 1000
    kSpeedTgt   = iDistRef / kTimeSecTgt

    ; SPEED slews (not time) with tau ~180 ms. portk's ihtim is a half-time;
    ; half-time ~= tau*ln(2) ~= 0.125 s approximates tau=180ms closely enough
    ; for a musical motor-inertia feel.
    iSpeedInit = iDistRef / (i(gkTdlTimeMs) / 1000)
    kSpeedAct  portk kSpeedTgt, 0.125, iSpeedInit
    kSpeedAct  limit kSpeedAct, iDistRef / 1.2, iDistRef / 0.06
    kTimeSecAct = iDistRef / kSpeedAct
    kTimeMsAct  = kTimeSecAct * 1000

    ; speed_ratio: how "fast" the transport is running relative to a
    ; 130 ms reference time; drives flutter rate, tone LP and head-bump freq
    kSpeedRatio = limit(130 / kTimeMsAct, 0.5, 1.3)

    ;==========================================================
    ; WOW: 0.6 Hz sine, depth 0.25 ms + randi-based drift (~10 Hz update,
    ; tone-smoothed, +/-0.05 ms)
    ;==========================================================
    aWowSine  oscili 0.25, 0.6, gisine
    aDriftRaw randi  1, 10, 1
    aDrift    tone   aDriftRaw, 10
    aDrift    = aDrift * 0.05
    aWowMs    = aWowSine + aDrift

    ;==========================================================
    ; FLUTTER: three cosines at f, 2f, 3f; amp ratio 1 : 0.35 : 0.43
    ; (base ~=0.08 ms), phases 0, 13pi/4, -pi/10; f = 11 Hz * speed_ratio.
    ; Implemented as phase-shifted sine (gisine + 0.25 cycle = cosine).
    ;==========================================================
    kFlutF = 11 * kSpeedRatio
    aFl1 oscili 0.08 * 1.00, kFlutF,     gisine, 0.25    ; phase 0
    aFl2 oscili 0.08 * 0.35, kFlutF * 2, gisine, 0.875   ; phase 13pi/4
    aFl3 oscili 0.08 * 0.43, kFlutF * 3, gisine, 0.20    ; phase -pi/10
    aFlutterMs = aFl1 + aFl2 + aFl3

    ; DC-guard offset (~2 ms) so modulation never drives the tap negative;
    ; min time floor 60 ms.
    aModMs     = aWowMs + aFlutterMs + 2.0
    aTimeMsAct interp kTimeMsAct
    aHead1Ms   = aTimeMsAct + aModMs
    aHead1Ms   limit aHead1Ms, 60, 1980
    aHead2Ms   = aHead1Ms * 2                 ; physically-correct: same
    aHead2Ms   limit aHead2Ms, 60, 1980       ; tape, larger head spacing ->
    aHead3Ms   = aHead1Ms * 3                 ; proportionally more wobble
    aHead3Ms   limit aHead3Ms, 60, 1980

    ;==========================================================
    ; FEEDBACK SUM -> HP 40 -> pre-shelf +6dB@2k -> subtle tanh -> de-shelf
    ;==========================================================
    kFbRaw portk gkTdlFeedback, 0.03
    kFb    limit kFbRaw, 0, 1.15

    aFbL init 0
    aFbR init 0

    aSumL = aInL + aFbL * kFb
    aSumR = aInR + aFbR * kFb

    aHpL butterhp aSumL, 40
    aHpR butterhp aSumR, 40

    aPreL pareq aHpL, 2000, ampdb(6), 0.707, 2
    aPreR pareq aHpR, 2000, ampdb(6), 0.707, 2

    kDrv   limit gkTdlDrive, 0.5, 2.5     ; subtle-saturation mandate: g<=2.5
    iBias  = 0.03
    kTanhB = tanh(kDrv * iBias)
    ; /kDrv normalizes the small-signal slope back to unity (tanh'(0)*g = g
    ; otherwise) so the drive stage doesn't act as a hidden ~2x gain stage —
    ; only the saturation curvature should color the sound, not raw level.
    aDrL   = (tanh(kDrv * (aPreL + iBias)) - kTanhB) / kDrv
    aDrR   = (tanh(kDrv * (aPreR + iBias)) - kTanhB) / kDrv

    aDeL pareq aDrL, 2000, ampdb(-6), 0.707, 2
    aDeR pareq aDrR, 2000, ampdb(-6), 0.707, 2

    aDelInL = aDeL
    aDelInR = aDeR

    ;==========================================================
    ; DELAY LINE — 2 s, three interpolated taps @ 1x/2x/3x head-1 time
    ;==========================================================
    iMaxDelMs = 2000
    aTap1L vdelay3 aDelInL, aHead1Ms, iMaxDelMs
    aTap1R vdelay3 aDelInR, aHead1Ms, iMaxDelMs
    aTap2L vdelay3 aDelInL, aHead2Ms, iMaxDelMs
    aTap2R vdelay3 aDelInR, aHead2Ms, iMaxDelMs
    aTap3L vdelay3 aDelInL, aHead3Ms, iMaxDelMs
    aTap3R vdelay3 aDelInR, aHead3Ms, iMaxDelMs

    ;==========================================================
    ; P6 HEAD MODE — 7-combo mask: 1,2,3,2+3,1+2,1+3,1+2+3
    ;==========================================================
    kMode = round(limit(gkTdlHeadMode, 1, 7))
    kM1 = (kMode==1 || kMode==5 || kMode==6 || kMode==7 ? 1 : 0)
    kM2 = (kMode==2 || kMode==4 || kMode==5 || kMode==7 ? 1 : 0)
    kM3 = (kMode==3 || kMode==4 || kMode==6 || kMode==7 ? 1 : 0)
    kM1s portk kM1, 0.02
    kM2s portk kM2, 0.02
    kM3s portk kM3, 0.02
    kActiveN = kM1s + kM2s + kM3s
    kActiveN = (kActiveN < 0.1 ? 0.1 : kActiveN)
    kHeadG   = 1 / sqrt(kActiveN)          ; keeps loudness stable across modes
    aM1 interp kM1s * kHeadG
    aM2 interp kM2s * kHeadG
    aM3 interp kM3s * kHeadG

    aTapsL = aTap1L * aM1 + aTap2L * aM2 + aTap3L * aM3
    aTapsR = aTap1R * aM1 + aTap2R * aM2 + aTap3R * aM3

    ;==========================================================
    ; LOOP FILTER — P4 center (250-5k log), P5 width/Q (0.5-6)
    ; Always-on gentle character tilt (subtle even at Q minimum), blended
    ; with a resonant bandpass whose weight rises with Q ("telephone-ify"
    ; the repeats as Q narrows toward 6). Nearly transparent at P5 minimum.
    ;==========================================================
    kFiltCtr portk gkTdlFiltCtr, 0.03
    kFiltCtr limit kFiltCtr, 250, 5000
    kFiltQ   portk gkTdlFiltQ, 0.03
    kFiltQ   limit kFiltQ, 0.5, 6

    aTiltL pareq aTapsL, kFiltCtr, ampdb(2), 0.6, 0
    aTiltR pareq aTapsR, kFiltCtr, ampdb(2), 0.6, 0

    kQNorm = limit((kFiltQ - 0.5) / 5.5, 0, 1)
    kBW    = kFiltCtr / kFiltQ
    aBpL butterbp aTapsL, kFiltCtr, kBW
    aBpR butterbp aTapsR, kFiltCtr, kBW
    kBpMk    = 1 + 2.5 * kQNorm            ; makeup so narrow-Q doesn't collapse in level
    kLoopMix = kQNorm ^ 1.5                ; nearly transparent at min, full character at max
    aLoopL = aTiltL + (aBpL * kBpMk - aTiltL) * kLoopMix
    aLoopR = aTiltR + (aBpR * kBpMk - aTiltR) * kLoopMix

    ;==========================================================
    ; TONE LP (speed-dependent 2.5-5 kHz) -> HP 100 -> head-bump pareq
    ; +3dB @ 90*speed_ratio Hz Q1 -> notch -1.5dB @ 2x bump
    ;==========================================================
    kToneLP = 2500 + 2500 * limit((kSpeedRatio - 0.5) / 0.8, 0, 1)
    aToneL butterlp aLoopL, kToneLP
    aToneR butterlp aLoopR, kToneLP

    aHp2L butterhp aToneL, 100
    aHp2R butterhp aToneR, 100

    kBumpF = 90 * kSpeedRatio
    aBumpL pareq aHp2L, kBumpF, ampdb(3), 1.0, 0
    aBumpR pareq aHp2R, kBumpF, ampdb(3), 1.0, 0

    kNotchF = kBumpF * 2
    aNotchL pareq aBumpL, kNotchF, ampdb(-1.5), 2.0, 0
    aNotchR pareq aBumpR, kNotchF, ampdb(-1.5), 2.0, 0

    ;==========================================================
    ; WEAR AM (~5.5 Hz, +/-0.7 dB) — in-loop hiss REMOVED 2026-08-23 per
    ; user request ("take out the bs white noise it is dramatic and bad").
    ; No noise generator runs here at all any more; every repeat just gets
    ; the head-bump/notch tone shaping above plus this amplitude wobble.
    ;==========================================================
    kWearDb randi 0.7, 5.5                 ; +/-0.7 dB wobble, ~5.5 Hz (spec: 3-8 Hz)
    kWearG  = ampdb(kWearDb)
    aWearL = aNotchL * kWearG
    aWearR = aNotchR * kWearG

    ; feedback tap — this IS what regenerates through the tanh-driven write
    ; stage above, so feedback > 1.0 self-oscillates but self-limits.
    aFbL = aWearL
    aFbR = aWearR

    ;==========================================================
    ; P1 MIX — equal-power dry/wet, then bypass crossfade, then safety limit
    ;==========================================================
    kMix  portk limit:k(gkTdlMix, 0, 1), 0.03
    kWetG = sqrt(kMix)
    kDryG = sqrt(1 - kMix)
    aOutL = aInL * kDryG + aWearL * kWetG
    aOutR = aInR * kDryG + aWearR * kWetG

    aFinalL = aOutL + (aInL - aOutL) * kByp
    aFinalR = aOutR + (aInR - aOutR) * kByp

    aFinalL limit aFinalL, -0.99, 0.99
    aFinalR limit aFinalR, -0.99, 0.99

    gaTdlWetL = aFinalL
    gaTdlWetR = aFinalR
endin

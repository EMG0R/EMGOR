; FREEZE — infinite-sustain reverb capture (stacking bed, never auto-clears)
; ---------------------------------------------------------------------------
; Menu: DIGIVERB (9) · code frz · toggle 72 · instr 195
; Reads:  gaFrzInL  / gaFrzInR
; Writes: gaFrzWetL / gaFrzWetR
;
; CONTROL CONTRACT (2026-07-26 redesign — bed never auto-clears):
;   gkFrzBypass init 1  — 1 = bypassed/off, 0 = engaged. Alwayson; reverbsc
;                         keeps running internally even while bypassed (only
;                         the wet-output crossfade, kGate, stops audibly) so
;                         that toggling back on can resume a still-live bed.
;                         A FRESH engage (bypass 1->0 edge) is the one thing
;                         that forces an empty bed: it fires the same clear
;                         mechanism as gkFrzClear and forces listen CLOSED
;                         regardless of the expression-pedal position, until
;                         the pedal is pushed past the toe threshold.
;   gkFrzExpr   init 0  — expression pedal, 0..1, portk 30-80 ms on the
;                         resulting send gain. Schmitt-trigger hysteresis:
;                         toe (>0.6) OPENS the input send into the frozen
;                         fb=1.0 loop (LISTEN/capture — new playing STACKS
;                         into the existing bed, does not replace it); heel
;                         (<0.4) CLOSES the send WITHOUT clearing anything
;                         (the bed keeps looping at whatever level it's at).
;   gkFrzClear  init 0  — one-shot pulse (set to 1 to fire). Clears the bed
;                         while STAYING engaged: feedback dips to ~0.2 for
;                         200 ms then relatches to 1.0. The effect resets
;                         this flag to 0 itself once handled — callers must
;                         not clear it themselves.
;
; GESTURE MODEL (documented for the integration pass — this file does not
; read buttons/taps, integration wires footswitch patterns to the globals
; above):
;   single click (while bypassed)  -> gkFrzBypass = 0  (engage; empty bed,
;                                      listen closed until toe)
;   single click (while engaged)   -> gkFrzClear  = 1  (clear the bed, stay
;                                      engaged — does NOT disengage)
;   double-tap (while engaged)     -> gkFrzBypass = 1  (escape/disengage)
;   expression pedal               -> gkFrzExpr (continuous listen control)
;
; DSP: a second reverbsc (independent from REVERB/menu 9's own), fixed
; 7 kHz post-tone stage (the frozen bed "slowly mellows"), gentle tanh
; limiter on the way out (the fb=1.0 latch is genuinely unstable without
; it — this is the documented bound on "infinite").
;
; P1 gkFrzBedLevel   — frozen-bed output level, 0..1 (default 0.8)
; P2 gkFrzDamp       — reverbsc internal damp, 0..1 -> ~400 Hz..16 kHz curve
; P3 gkFrzListenGain — listen-send gain, 0..1 -> 0.3..1.5x actual send gain
; ---------------------------------------------------------------------------

gkFrzBypass     init 1
gkFrzBedLevel   init 0.8
gkFrzDamp       init 0.5
gkFrzListenGain init 0.5
gkFrzExpr       init 0
gkFrzClear      init 0
gaFrzInL        init 0
gaFrzInR        init 0
gaFrzWetL       init 0
gaFrzWetR       init 0

instr 195
  kByp     = limit:k(gkFrzBypass, 0, 1)
  kEngaged = 1 - kByp
  kGate    portk kEngaged, 0.05              ; overall wet-output safety gate

  kBedLevel portk limit:k(gkFrzBedLevel, 0, 1), 0.05
  kDamp     portk limit:k(gkFrzDamp, 0, 1), 0.05
  kDamp     expcurve kDamp, 4
  kDampHz   scale kDamp, 16000, 400
  kListenKnob = 0.3 + 1.2 * limit:k(gkFrzListenGain, 0, 1)   ; P3: 0.3..1.5x

  ; ---- fresh-engage edge (bypass 1->0): forces empty bed + closed listen ----
  kPrevByp   init 1
  kBypEdge   changed kByp
  kEngageEdge = (kBypEdge == 1 && kByp < 0.5 && kPrevByp >= 0.5 ? 1 : 0)
  kPrevByp = kByp

  ; ---- clear pulse: fresh engage OR external gkFrzClear (self-resetting) ----
  kClearFire = 0
  if (kEngageEdge == 1) then
    kClearFire = 1
  endif
  if (gkFrzClear >= 0.5) then
    kClearFire = 1
    gkFrzClear = 0                            ; effect self-resets the flag
  endif
  kClearEdge   changed kClearFire
  kClearCount  init 0
  if (kClearEdge == 1 && kClearFire >= 0.5) then
    kClearCount = 0.2 * kr                    ; ~200 ms dip, in k-cycles
  endif
  kClearActive = (kClearCount > 0 ? 1 : 0)
  if (kClearActive == 1) then
    kClearCount = kClearCount - 1
  endif
  kFbTarget = (kClearActive == 1 ? 0.2 : 1.0) ; bed itself NEVER auto-clears —
  kFb portk kFbTarget, 0.012                  ; only an explicit clear dips it
                                               ; (fast portk so the 200 ms dip
                                               ; actually DWELLS near 0.2 long
                                               ; enough to drain the loop)

  ; ---- listen send: Schmitt-trigger hysteresis on the expr pedal ----
  kListenLatch init 0
  if (kEngageEdge == 1) then
    kListenLatch = 0                          ; force closed on fresh engage
  endif
  kExpr = limit:k(gkFrzExpr, 0, 1)
  if (kExpr > 0.6) then
    kListenLatch = 1
  elseif (kExpr < 0.4) then
    kListenLatch = 0
  endif
  kSendTarget = (kListenLatch == 1 ? kListenKnob : 0)
  kSend portk kSendTarget, 0.06                ; 30-80 ms send glide

  aInL = gaFrzInL * kSend * kGate
  aInR = gaFrzInR * kSend * kGate

  ; ---- run gate (2026-08-10, perf) ----------------------------------------
  ; 195 is always-on (see signal_chain: it is deliberately NOT wrapped in
  ; FX_BYPASS_TAIL) because the frozen bed must survive bypass. The reverbsc
  ; tank plus post chain still cost real CPU on every k-cycle while the
  ; effect is switched off, and that cost buys nothing: the header's claim
  ; that the tank must keep spinning to "resume a still-live bed" is
  ; contradicted by the code above — kClearFire fires on EVERY 1->0 engage
  ; edge, which drains the loop before the first sample is heard. The tank
  ; contents while bypassed are therefore provably unobservable, so we skip
  ; the whole network.
  ; Gate on the portk'd kGate, NOT on kByp: that lets the 50 ms output fade
  ; complete on live tank audio before the network stops running. 0.0005 is
  ; ~-66 dB, comfortably below audibility for the residual.
  aWetL init 0
  aWetR init 0
  if (kGate > 0.0005) then
    aWetL, aWetR reverbsc aInL, aInR, kFb, kDampHz, sr, 1

    ; frozen bed "slowly mellows" — fixed post damping + gentle limiter (this
    ; is also the practical ceiling on stacked-energy growth while LISTEN is
    ; held open for a long time — the loop is not literally lossless)
    aWetL tone aWetL, 7000
    aWetR tone aWetR, 7000
    aWetL = tanh(aWetL)
    aWetR = tanh(aWetR)

    aWetL dcblock aWetL
    aWetR dcblock aWetR
    aWetL limit aWetL, -0.99, 0.99
    aWetR limit aWetR, -0.99, 0.99

    gaFrzWetL = aWetL * kGate * kBedLevel
    gaFrzWetR = aWetR * kGate * kBedLevel
  else
    gaFrzWetL = 0
    gaFrzWetR = 0
  endif
endin

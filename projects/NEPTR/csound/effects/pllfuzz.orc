; PLLFUZZ — PLL fuzz-synth (EQD Data Corrupter class)
; ---------------------------------------------------------------------------
; Signal chain position : 14.3 (after TAPE 14 inline, before RING MOD 14.5)
; Menu / slot           : proposed NEW menu "SYNTH" slot 1 — toggle 62, instr 185, code pll
; Reads                 : gaPLL_sendL / gaPLL_sendR
; Writes                : gaPLL_outL / gaPLL_outR
; Bypass                : gkPLL_bypass (FX_BYPASS)
;
; INTEGRATION NOTES (integration agent owns all shared files — verified by
; grep 2026-07-13: toggle 62 unused (max in midi_handler is 56), instr 185
; unused; integration resolves collisions if another parallel builder claimed
; either id first)
;   * PRIME BASS-BANK MATERIAL: comparator/divider fuzz on bass is the
;     classic Data-Corrupter-on-bass move; sub divider voice reaches an
;     octave under the 55 Hz tracking floor. A bass preset with wet~0.7,
;     intv=x1, lock~0.85, vmix~0.4 makes a Synth-Bass-Fuzz slot. Flag for
;     the bass preset bank alongside TRACKSYNTH (tracksynth.orc).
;   * neptrPhase4.csd: #include "effects/pllfuzz.orc" (does NOT use
;     onset_pick.orc — no include-order dependency).
;   * globals.orc: gk/ga declarations below may MOVE there verbatim.
;   * signal_chain.orc @ stage 14.3:
;       gaPLL_sendL = aPrevL
;       gaPLL_sendR = aPrevR
;       $FX_BYPASS(gkPLL_bypass'185'gaPLL_sendL'gaPLL_sendR'gaPLL_outL'gaPLL_outR'pll)
;       aPrevL ntrpol gaPLL_sendL, gaPLL_outL, 1
;       aPrevR ntrpol gaPLL_sendR, gaPLL_outR, 1
;     (chain mix constant 1: P1 dry/wet happens INSIDE the instr.)
;   * midi_handler.orc: menu SYNTH slot 1 dispatch = plain FX_TOGGLE
;     gkPLL_bypass'62. Encoder map v2 (home slot 1 = CC25):
;       P1 (CC25, home rot) gkPLL_mix  [0..1] lin .02        "DryWet"
;       P2 (CC20)           gkPLL_intv [0..4] stepped-int 1  "Interval" (/4,/2,x1,x2,x3)
;       P3 (CC23)           gkPLL_lock [0..1] lin .02        "LockSpd" (0=chaos, 1=stable)
;       P4 (CC24, slot0)    gkPLL_vmix [0..1] lin .02        "Voice" (master->sub->PLL morph)
;     NO CC29 claim (near-zero latency comparator fuzz, no pedal target).
;   * OSC feedback: /pll_state, /pll_mix, /pll_intv, /pll_lock, /pll_vmix
;     ($OSC_F, house pattern).
;   * neptrPhase4_UI.py: proposed new MENUS row (with TRACKSYNTH):
;       {"name": "SYNTH", "effects": [("tsy","TRACKSYN"), ("pll","PLLFUZZ"),
;                                     ("—","—"), ("—","—"), ("—","—")]}
;     EFFECT_PARAMS["pll"] = [("mix","DryWet",0,1),("intv","Interval",0,4),
;         ("lock","LockSpd",0,1),("vmix","Voice",0,1)]
;
; WHAT IT DOES
;   A comparator hard-squares the input (master voice — near-zero latency
;   fuzz). A hysteresis comparator on a lowpassed copy measures the input
;   period sample-accurately (rising-edge spacing, 55-1200 Hz window with
;   hold-previous) and flip-flops a divide-by-2 square (sub voice at f/2).
;   A simulated PLL — phase-comparator error into a 2nd-order loop filter
;   (gain + momentum, i.e. port with inertia) — steers a vco2 square toward
;   f * interval (/4, /2, x1, x2, x3 stepped, P2). Lock speed (P3) sets loop
;   gain AND damping: fast = tight tracking; slow = underdamped hunting plus
;   injected loop noise -> the deliberately unstable, glitchy re-locks that
;   ARE the Data Corrupter sound. Rail slips re-lock an octave off. Three
;   voices morph master->sub->PLL (P4); output is gated by the input
;   envelope so silence stays silent. P1 dry/wet.
; ---------------------------------------------------------------------------

gkPLL_bypass init 1
gkPLL_mix    init 0.5
gkPLL_intv   init 2        ; index 0..4 -> /4 /2 x1 x2 x3 (default x1)
gkPLL_lock   init 0.7
gkPLL_vmix   init 0.5
gaPLL_sendL  init 0
gaPLL_sendR  init 0
gaPLL_outL   init 0
gaPLL_outR   init 0

instr 185
  aInL = gaPLL_sendL
  aInR = gaPLL_sendR
  aMono = (aInL + aInR) * 0.5

  ; ---- input envelope -> fuzz gate (silence stays silent) ----
  aEnvF follow2 aMono, 0.002, 0.06
  kEnvA downsamp aEnvF
  kGate = limit((dbamp(max(kEnvA, 0.00001)) + 60) / 12, 0, 1)
  kGate portk kGate, 0.01
  aGate interp kGate

  ; ---- comparator + sample-accurate period tracker + /2 flip-flop ----
  aTrk butterlp aMono, 700          ; tracking path: fundamental-weighted
  iMinPer = int(sr / 1200)
  iMaxPer = int(sr / 55)
  kPer   init 218                   ; ~220 Hz start
  kCnt   init 0
  kSubPh init 1
  kTrkSt init -1
  kHyst = max(kEnvA * 0.25, 0.002)  ; relative hysteresis, floor for noise
  aSq   init 0
  aSubA init 0
  kS = 0
  while kS < ksmps do
    kIn = aMono[kS]
    kT  = aTrk[kS]
    aSq[kS] = (kIn > 0 ? 1 : -1)    ; master voice: hard comparator
    if (kTrkSt < 0 && kT > kHyst) then
      kTrkSt = 1                    ; rising edge on tracking comparator
      if (kCnt >= iMinPer && kCnt <= iMaxPer) then
        kPer = kCnt                 ; in-window period -> accept
      endif                         ; else: hold previous (tracking loss)
      kCnt = 0
      kSubPh = -kSubPh              ; divide-by-2 flip-flop
    elseif (kTrkSt > 0 && kT < -kHyst) then
      kTrkSt = -1
    endif
    kCnt = (kCnt >= iMaxPer * 4 ? kCnt : kCnt + 1)
    aSubA[kS] = kSubPh
    kS += 1
  od
  kFin = sr / kPer

  ; ---- simulated PLL: error -> loop filter (gain+momentum) -> VCO ----
  kIdx = round(limit(gkPLL_intv, 0, 4))
  kMult = (kIdx == 0 ? 0.25 : (kIdx == 1 ? 0.5 : (kIdx == 2 ? 1 : (kIdx == 3 ? 2 : 3))))
  kTgt = limit(kFin, 55, 1200) * kMult
  kLock = limit(gkPLL_lock, 0, 1)
  ; ksmps-INDEPENDENT loop filter (2026-08-10). Both constants below are
  ; PER-K-CYCLE and were tuned at ksmps=256 (kr=187.5 Hz). At the ksmps=64 we
  ; now run, the loop iterated 4x more often per second: lock was ~4x too
  ; fast and the "slow lock = underdamped ring" character — the whole point
  ; of the LOCK knob — had largely collapsed. iPLLk rescales both to the
  ; reference rate: gain is an integrator step so it scales linearly, kDamp
  ; is a per-cycle decay factor so it scales as a power.
  iPLLk = ksmps / 256
  kG    = (0.004 + 0.45 * kLock * kLock) * iPLLk       ; loop gain per k-cycle
  kDamp = pow(0.985 - 0.75 * kLock, iPLLk)   ; slow lock = underdamped ring
  kNz   randh 1, 40                          ; loop noise: chaos at slow lock
  kVcoF init 220
  kVel  init 0
  kErr = kTgt - kVcoF + kNz * kTgt * 0.6 * (1 - kLock) * (1 - kLock)
  kVel = kVel * kDamp + kErr * kG
  kVcoF = kVcoF + kVel
  if (kVcoF < 20) then                       ; rail slip: glitchy octave re-lock
    kVcoF = kTgt * 2
    kVel = 0
  elseif (kVcoF > 5200) then
    kVcoF = kTgt * 0.5
    kVel = 0
  endif
  kVcoC = limit(kVcoF, 20, 5200)
  aPll vco2 1, kVcoC, 10

  ; ---- three-voice morph: master -> sub -> PLL ----
  kVMix = limit(gkPLL_vmix, 0, 1)
  kWM = limit(1 - 2 * kVMix, 0, 1)
  kWS = 1 - abs(2 * kVMix - 1)
  kWP = limit(2 * kVMix - 1, 0, 1)
  aVox = aSq * kWM + aSubA * kWS + aPll * kWP
  aWet = aVox * 0.30 * aGate

  ; ---- P1 dry/wet ----
  kMix portk limit(gkPLL_mix, 0, 1), 0.02
  aOutL ntrpol aInL, aWet, kMix
  aOutR ntrpol aInR, aWet, kMix
  aOutL dcblock aOutL
  aOutR dcblock aOutR
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaPLL_outL = aOutL
  gaPLL_outR = aOutR

#ifdef PLLDBG
  kWrms rms aWet
  printks "PLL t=%.3f fin=%.1f vco=%.1f tgt=%.1f gate=%.3f wrms=%.5f\n", 0.005, timeinsts:k(), kFin, kVcoC, kTgt, kGate, kWrms
#endif
endin

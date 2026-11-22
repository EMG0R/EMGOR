; HABIT — granular buffer scrubber
; Signal chain position: 20.5 (between DELAY 62 and REVERB 70)
; Reads:  gaHabitSendL/R
; Writes: gaHabitWetL/R (accumulated by grain instances; cleared in signal_chain)
; Bypass: gkHabitBypass (toggle ID 36)
; Controls: gkHabitMix    (slot enc CC25) — wet/dry
;           gkHabitSize   (CC20, 0-1) — max grain length: 50ms..3s (log-uniform draw)
;           gkHabitRepeats(CC23, 0-1) — grain density: 1..120 grains/sec
;           gkHabitScan   (CC29 expression pedal)
;             heel (0) = draw grains from entire recorded history (max depth/randomness)
;             toe  (1) = draw grains from narrow ~1 s window near current position
;
; RECORD GATE (2026-07-13) — gkHab_rec: 1 = writing into the ring, 0 = FROZEN.
;   Driven by the CC76 button gesture (handler_menus.orc, CHASE BLISS s2):
;     click from off = effect ON + recording immediately
;     click while on = flip-flop the buffer, RECORDING <-> FROZEN
;     double-click   = effect OFF entirely
;   When frozen the write head STOPS (the phasor's frequency is multiplied by
;   the gate, so its phase simply holds) and no samples are written — the
;   captured memory is held intact while the grain scheduler keeps playing and
;   scanning it, exactly as before.
;   Freeze/unfreeze are click-free: the recorded signal is faded out/in over
;   ~10 ms (kRecG) while the head is STILL MOVING, so the seam left behind in
;   the ring is a smooth fade rather than a hard cut; only once the fade has
;   reached zero does the head park.
;   NULL-SAFE: with gkHab_rec pinned to 1, the phasor frequency, the written
;   sample and kValid are all identical to the pre-gate version (kRecG is
;   initialised to 1, so there is no start-up fade-in either) — HABIT's sound
;   with recording enabled is unchanged.
;
; Architecture:
;   instr 64 — records mono input into giHabitMem ring; runs metro + schedkwhen
;   instr 68 — individual grain player (ephemeral, spawned per grain)
;              accumulates via += into gaHabitWetL / gaHabitWetR
;
; Buffer: giHabitMem = 2^23 = 8388608 samples = 174.8 s @ sr=48000
; Envelope: giHanning (1024-pt Hanning, defined in globals.orc)

; ---- self-declared globals for the record gate (globals.orc untouched) ----
gkHab_rec     init 1     ; 1 = ring is recording, 0 = frozen
gkHab_lastTap init -10   ; CC76 double-click timer (handler_menus.orc)

;==============================================================
; instr 64 — recorder + grain scheduler
;==============================================================
instr 64
  ;--- mono input
  aIn = (gaHabitSendL + gaHabitSendR) * 0.5

  ;--- smoothed controls (size + density slewed gently to avoid metro rate jumps)
  kScan    portk gkHabitScan,    0.05
  kSize    portk gkHabitSize,    0.04
  kDens    portk gkHabitRepeats, 0.04
  kBlend   portk gkHabitBlend,   0.15   ; slow blend slew so morph is gradual, not jarring

  ;--- derived parameters
  ; max grain duration: 0.5 s at knob=0, 4.0 s at knob=1
  kMaxGrainSec = 0.5 * pow(8, kSize)

  ; base density: 1.5 Hz at knob=0, 9 Hz at knob=1 — very sparse long-grain base
  kDensHz = 1.5 * pow(6, kDens)

  ;=====================================================================
  ; RECORD — writing into the ring buffer, GATED by gkHab_rec (see header).
  ; rec=1 -> identical to the original always-on recorder.
  ; rec=0 -> write head parks, buffer frozen, grains keep reading it.
  ;=====================================================================
  iMemLen = ftlen(giHabitMem)         ; 8388608

  kRec   = (gkHab_rec > 0.5 ? 1 : 0)
  kRecG  portk kRec, 0.005, 1         ; ~10 ms de-click fade; inits at 1 (null-safe)
  ; keep the head moving while the fade is still non-zero, so the seam written
  ; into the ring is a smooth fade-out rather than a hard cut; then park it.
  kMoving = (kRecG > 0.001 ? 1 : 0)

  aWPhs   phasor (sr / iMemLen) * kMoving  ; freq 0 -> phase holds = frozen head
  aWNdx   = aWPhs * iMemLen
  ; NB: multiply by the k-rate gain directly (zero-order hold across the block).
  ; `interp` would ramp up from 0 across the very FIRST k-block and so quietly
  ; change what the original recorder wrote — that alone broke the null test.
  ; A 2.7 ms ZOH staircase on a 10 ms fade is inaudible, and it is exact when
  ; the gain is pinned at 1.
  if (kMoving == 1) then
    tablew aIn * kRecG, aWNdx, giHabitMem
  endif

  ; kValid: samples recorded since (re)engage — gates grains against uninitialized reads
  kValid init 0
  if (kMoving == 1) then
    kValid = (kValid + ksmps > iMemLen ? iMemLen : kValid + ksmps)
  endif
  kWPos  downsamp aWNdx               ; write head in samples, at k-rate

  ;=====================================================================
  ; WINDOW — kScan controls how far back into history grains may reach
  ;
  ; kScan=0 (heel): window = entire valid history (max randomness / deep past)
  ; kScan=1 (toe) : window = last 1.0 s (tight present)
  ; Linear interpolation; clamped to [10 ms, kValid].
  ;=====================================================================
  kPresentWin = sr * 1.0              ; "tight" window size: 1.0 s
  kWinSamps   = kValid * (1 - kScan) + kPresentWin * kScan
  kWinSamps   = (kWinSamps < sr * 0.01 ? sr * 0.01 : kWinSamps)
  kWinSamps   = (kWinSamps > kValid   ? kValid      : kWinSamps)

  ;=====================================================================
  ; GRAIN SCHEDULER
  ; p-fields for instr 68:
  ;   p4 = kWPos        (write head position, samples)
  ;   p5 = kWinSamps    (window span, samples)
  ;   p6 = kMaxGrainSec (max grain duration, seconds)
  ;   p7 = iMemLen      (buffer length, samples — passed as i-rate)
  ;=====================================================================
  ;=====================================================================
  ; DENSITY BURST — Poisson-ish, mean ~15 s between events
  ; On fire: density multiplier spikes to 6-10x, decays exponentially ~8s
  ; Creates the generative "suddenly alive" moments the user wants
  ;=====================================================================
  kBurstCheck metro 2                            ; check twice per second
  kBurstRnd   trandom kBurstCheck, 0, 1
  ; P(fire per check) ≈ 0.5/15 = 0.033 → mean interval ≈ 15 s
  kBurstFire  = kBurstCheck * (kBurstRnd < 0.033 ? 1 : 0)
  kBurstEnv   init 0
  kBurstDecay init 0.99
  if (kBurstFire == 1) then
    kBurstPeak  random 5, 10                     ; random peak multiplier 5-10x
    kBurstEnv   = kBurstPeak
    kBurstDecay = exp(-1 / (8 * kr))             ; ~8 s exponential decay
  endif
  kBurstEnv   = kBurstEnv * kBurstDecay
  kDensFinal  = kDensHz * (1 + kBurstEnv)       ; base * (1 + burst)

  ; onset jitter 150-600ms — smears sparse long grains so they never pulse
  kJitter random 0, 0.15 + 0.45 * kDens
  ktrig metro kDensFinal
  schedkwhen ktrig, 0, 0, 68, kJitter, 0.001, kWPos, kWinSamps, kMaxGrainSec, iMemLen, kBlend

endin

;==============================================================
; instr 68 — individual grain player
;
; p3  = 0.001 (dummy; grain self-terminates via xtratim + timeinsts)
; p4  = write head position (samples) at scheduling moment
; p5  = window size (samples)
; p6  = max grain duration (seconds)
; p7  = buffer length (samples)
; p8  = blend: 0=smooth long-grain delay, 1=full granular (captured at spawn time)
;==============================================================
instr 68
  iWPos     = p4
  iWinSamps = p5
  iMaxSec   = p6
  iMemLen   = p7
  iBlend    = p8

  ;--- grain duration floor: morphs with blend
  ; delay end (0): min=1.5s — only big, inconspicuous overlapping grains
  ; granular end (1): min=0.30s — log-uniform gives many short + occasional long
  iMinSec  = 1.5 * (1 - iBlend) + 0.30 * iBlend
  if (iMinSec > iMaxSec * 0.85) then
    iMinSec = iMaxSec * 0.85         ; guard: min can't exceed max (low kSize setting)
  endif
  iLogMin  = log(iMinSec)
  iLogMax  = log(iMaxSec)
  iLogRnd  random iLogMin, iLogMax
  iGrainSec = exp(iLogRnd)

  ; Duration floor, expressed in SAMPLES first so it is the same WALL-CLOCK
  ; length at any block size. Two requirements, both must hold:
  ;   (a) >= 2 k-cycles, so the linseg envelope and the timeinsts
  ;       self-terminate below both get a real ramp rather than one block;
  ;   (b) >= 128 samples (2.67 ms @ 48 k) — which is exactly what 2*ksmps
  ;       came to at the patch's ksmps 64 (globals.orc), so at ksmps 64 this
  ;       is bit-identical to the old `2 * ksmps / sr`. At ksmps 32 it stays
  ;       2.67 ms instead of halving to 1.33 ms.
  ; (In practice unreachable — kMaxGrainSec bottoms out at 0.5 s and iMinSec
  ; at 0.30 s — but it is the guard that keeps a degenerate grain legal, and
  ; a guard that silently changes size with the block size is not a guard.)
  iMinDur  = max(128 / sr, 2 * ksmps / sr)
  iGrainSec = (iGrainSec < iMinDur ? iMinDur : iGrainSec)

  ;--- random read position within the window
  iOffset   random 0, iWinSamps
  iStartSmp = iWPos - iOffset

  ; wrap into table bounds (power-of-2 table, safe integer modulo)
  iStartSmp = iStartSmp - iMemLen * int(iStartSmp / iMemLen)
  iStartSmp = (iStartSmp < 0 ? iStartSmp + iMemLen : iStartSmp)

  ;--- occasional reverse: 0% at delay end, 10% at granular end
  iRevRnd   random 0, 1
  iDir      = (iRevRnd < 0.10 * iBlend ? -1 : 1)

  ;--- occasional octave up: 0% at delay end, 4% at granular end
  iPitchRnd random 0, 1
  iRto      = (iPitchRnd < 0.04 * iBlend ? 2.0 : 1.0)

  ;--- read line through the ring buffer
  iReadSpan = iGrainSec * iRto * sr * iDir
  aLine     line iStartSmp, iGrainSec, iStartSmp + iReadSpan
  aRNdx     wrap aLine, 0, iMemLen
  aGrain    tablei aRNdx, giHabitMem, 0, 0, 1

  ;--- Hanning envelope via linseg→tablei (duration = iGrainSec exactly)
  aEnvPhs linseg 0, iGrainSec, 1
  iEnvLen = ftlen(giHanning) - 1
  aEnvNdx = aEnvPhs * iEnvLen
  aEnv    tablei aEnvNdx, giHanning, 0

  aOut = aGrain * aEnv

  ;--- equal-power stereo pan (random per grain)
  iPan  random 0, 1
  gaHabitWetL += aOut * sqrt(1 - iPan)
  gaHabitWetR += aOut * sqrt(iPan)

  ;--- self-terminate after iGrainSec
  xtratim iGrainSec
  kElapsed timeinsts
  if (kElapsed >= iGrainSec) then
    turnoff
  endif

endin

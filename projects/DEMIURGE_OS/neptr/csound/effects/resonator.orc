; RESONATOR — Spectral Magnitude Resonator (v2, complete rewrite 2026-07-13)
; ---------------------------------------------------------------------------
; Signal chain position : 157 (stage 22.7 — after HABIT, before ONWARD/REVERB)
; Menu / slot           : FILTERING menu (4), slot 3 (toggle 47, CC75)
; Reads                 : gaResSendL / gaResSendR
; Writes                : gaResOutL  / gaResOutR   (wet-only; chain ntrpols dry)
; Latency               : DRY = zero (chain ntrpol taps the send directly).
;                         WET = one FFT frame, 4096/48000 = 85.3 ms (see CPU note).
;
; WHY v2 EXISTS (what was wrong with v1)
;   v1 was a pitch-TRACKING pipe bank: a pvsanal sidechain folded bins into
;   61 chromatic semitone bins, peak-picked, harmonic-subtracted, and then
;   allocated 8 streson "pipes" snapped to detected SEMITONES. Every failure
;   mode of that design is audible:
;     * It quantized to the 12-TET grid. Real guitar partials are inharmonic,
;       bent, vibratoed and detuned; snapping them to the nearest semitone
;       detunes the ring against the note you are actually playing → the
;       characteristic sour beating.
;     * Detection was a hard decision. One frame of ambiguity retunes a pipe
;       → chirps/warble. Voice STEALING retuned pipes mid-ring → glitches.
;     * 8 voices is not "the spectrum", it is 8 notes. Anything the detector
;       missed simply did not resonate; anything it hallucinated (leakage
;       skirts of a loud low note) rang loudly and wrongly.
;     * The streson bank fed a global regeneration loop that needed an
;       input-referenced envelope limiter to stay bounded — and that limiter
;       then had to be crossfaded OUT above fb 0.8 so drones could exist, i.e.
;       the thing was explicitly unbounded exactly where it mattered.
;   None of that survives. There is no pitch detection, no note quantization,
;   no voice allocation and no audio feedback loop anywhere in v2.
;
; WHAT v2 DOES — RECURSIVE SPECTRAL MAGNITUDE DECAY
;   Straight phase-vocoder domain. Each analysis frame, for every bin in the
;   guitar band, we run a leaky magnitude accumulator with a per-bin decay:
;
;       A[b] <- min( A[b]*g[b] + inj[b],  cap[b] )
;
;   where g[b] = exp(-hop / tau[b]) is the per-bin feedback gain and inj[b] is
;   the new energy admitted this frame. Bins that are loud get energy dumped
;   into them and ring on; bins that are quiet do not. The spectrum of the
;   tail IS the spectrum of what you played — no decision, no grid, no notes.
;   A chord blooms as a chord because its three partials are three loud bins;
;   a single note rings on its own actual partials; a pick scrape rings as a
;   band of noise turning tonal. This is the technique from Vickers, Wu,
;   Krishnan & Sadanandam, "Frequency Domain Artificial Reverberation using
;   Spectral Magnitude Decay" (AES 121, 2006) §4 "Recursive Spectral Magnitude
;   Decay" — a leaky accumulation of STFT magnitudes with a frequency-dependent
;   feedback gain, described there as the frequency-domain analogue of Moorer's
;   comb filter with frequency-dependent loop gain. Its two headline properties
;   are exactly what a guitar resonator wants: per-bin control of decay time,
;   and a CPU cost that is INDEPENDENT of how long the tail is.
;
;   Csound does the hard part for us. We resynthesize through tab2pvs/pvsynth
;   in AMP+FREQ format, and pvsynth integrates each bin's phase from its
;   frequency value. So a bin whose magnitude and frequency are held produces a
;   steady, coherent sinusoid — we never touch phase, and there is no phasiness
;   from re-randomization.
;
;   TWO ADDITIONS ON TOP OF THE PAPER, both load-bearing for guitar:
;
;   1. PEAK-GATED INJECTION ("FOCUS"). Only bins that are a local magnitude
;      maximum AND above a threshold relative to the frame peak are allowed to
;      inject. This is what stops the effect from being a smear: a partial's
;      leakage skirt is NOT a local max, so the skirt never rings, and the
;      partial collapses to one clean sinusoid on resynthesis. FOCUS sweeps the
;      threshold from -60 dB (dense, every peak in the room rings — a cloud)
;      to -9 dB (only the dominant partials ring — a clean singing voice).
;
;   2. MAGNITUDE-WEIGHTED FREQUENCY UPDATE. The paper's freeze holds the phase
;      increment rigid and it warns the result sounds "artificial and
;      mechanical" (§3.1), and turns noise into "tonal noise". We avoid that:
;      each accumulating bin's frequency crossfades toward the incoming
;      estimate weighted by how much of that bin's energy is NEW:
;         w = inj / (inj + A*g)        F[b] <- F[b] + w*(Fin[b] - F[b])
;      A fresh partial (A~0) snaps instantly to its true, unquantized frequency.
;      A ringing tail (inj~0) holds its frequency rock-steady. So the ring
;      inherits your bend/vibrato while the note is sounding, then locks — it
;      cannot warble, because the only thing that can move it is real energy.
;      F is additionally clamped to +/-1 bin around the bin centre, which makes
;      a mis-tuned partial structurally impossible (a peak bin's PV frequency
;      estimate is within +/-0.5 bin by construction; anything wilder is noise).
;
; BOUNDEDNESS (structural, not a limiter bolted on)
;   cap[b] = giResCap * H[b], where H[b] is a per-bin peak-hold of the INPUT
;   magnitude that releases at the bin's own decay rate:  H[b] <- max(Min[b],
;   H[b]*g[b]).  Therefore  A[b] <= giResCap * H[b] <= giResCap * (peak input
;   magnitude ever seen in that bin), ALWAYS, for any decay, any input, any
;   FOCUS. Because H decays at exactly the same g as A, the cap tracks the tail
;   instead of clamping it — it bounds GROWTH without shortening the ring.
;   With silence in, Min=0, so H decays and A decays; nothing can grow. At
;   FREEZE, g=1 and injection is off, so A is literally constant. There is no
;   audio-rate feedback path in this instrument at all, so "runaway" has no
;   mechanism to occur. dcblock + tanh give the final absolute ceiling.
;
; TILT (P4) — the per-bin decay time is tau[b] = tauBase / d[b], with
;   d[b] = (f_b / 200 Hz) ^ e,  e = 0.45 - 0.45*tilt.  This is the "damping"
;   control and it is what makes the thing musical rather than clinical:
;     tilt = -1 : e=0.9  → 5 kHz decays ~18x faster than 200 Hz. Dark, woody,
;                 like a damped soundboard. Chords bloom low and warm.
;     tilt =  0 : e=0.45 → 5 kHz decays ~4x faster. Natural string behaviour.
;     tilt = +1 : e=0    → every bin decays at the same rate. Highs hang as
;                 long as lows: glassy, shimmering, choir-like.
;
; PARAM CHOICE / JUSTIFICATION
;   P1 MIX    dry/wet (chain ntrpol).
;   P2 DECAY  (CC20) tau 0.08 s → 40 s, exponential. The single most important
;             knob: short shimmer → infinite drone. Required by spec.
;   P3 FOCUS  (CC23) selectivity. Chosen over "brightness" because brightness is
;             already covered better by TILT, and because FOCUS is the knob that
;             decides WHAT resonates — it is the identity of a spectral engine.
;             It is the difference between "a haze rings" and "the chord rings".
;   P4 TILT   damping/tone (see above). Bipolar, reuses the old octave encoder.
;   P5 FREEZE infinite hold of the current spectrum, input ignored. Play over a
;             frozen chord. In a spectral engine this is one line (g=1, inj=0)
;             and it is the single most-asked-for feature of the class.
;   Deliberately NOT included: a separate "regeneration/feedback" knob. In this
;   architecture regeneration IS decay — g[b] is the loop gain. A second
;   feedback control would be a duplicate knob that could only make it unstable.
;
; CPU
;   Class: LOW, and FLAT IN DECAY — the paper's headline property. A 40 s tail
;   costs exactly what a 0.1 s tail costs, because the decay is a per-bin
;   multiply, not a delay line. There is nothing here that grows with tail
;   length, so the DECAY knob is CPU-free.
;
;   MEASURED (macOS, Csound 6.18, 50 s render of scripts/bench/resonator_bench.csd,
;   differenced against the same render with instr 157 not instantiated):
;       0.806 s CPU per 50 s of audio = 1.61 % of one core.
;   Where it goes (measured by isolating each part):
;       ~35 %  the FFTs: 1x pvsanal(4096) + 2x pvsynth(4096), ~140 FFTs/s
;       ~60 %  the per-bin loop BODY — Csound's INTERPRETED ARRAY INDEXING, at
;              roughly 15 ns per array read/write. This, not the arithmetic and
;              not the FFTs, is the real cost driver, which is why the loop keeps
;              everything it can in scalars (rolling magnitude window, cached
;              gain table, guarded frequency reads). Those cuts took it from
;              0.937 s to 0.806 s.
;       ~5 %   loop overhead itself.
;   The per-bin pass is (N/2) bins at (sr / (N/4)) frames per second = 2*sr bin
;   updates per second REGARDLESS of N — doubling the FFT doubles the bins and
;   halves the frame rate. So giResFFT is a pure latency/resolution trade with no
;   CPU consequence, which is what made the 4096 fix free.
;
;   PI 5 ESTIMATE: ~5-10 % of one core. This is an EXTRAPOLATION (a Cortex-A76 at
;   2.4 GHz runs this kind of scalar/interpreted work roughly 3-6x slower than
;   this Mac), not a measurement — it is inside the 15 % budget with room, but it
;   SHOULD BE CONFIRMED ON THE PI before it is trusted.
;
;   The bin loop is restricted to 47 Hz .. 8 kHz (bins 4..683) — there is nothing
;   worth resonating below the low E or above the 8 kHz hiss — which is a 3x
;   saving over looping all 2049 bins.
;
; ===========================================================================
; INTEGRATION NOTES  (for the agent that owns the shared files)
; ===========================================================================
; *** GOOD NEWS: THE RIG WORKS WITH ZERO SHARED-FILE EDITS. ***
; Every global name, every encoder range, every encoder step, every OSC path,
; the toggle id, the instr number, the menu slot and the custom bypass block
; are all UNCHANGED from v1. I deliberately re-used the v1 global names so the
; parallel edits to signal_chain.orc / handler_menus.orc / handler_osc.orc /
; neptrPhase4_UI.py do not have to be touched or re-merged. What changed is the
; MEANING of three of them. All edits below are COSMETIC (labels only).
;
; 1. NO CHANGE REQUIRED in csound/neptrPhase4.csd
;      #include "effects/resonator.orc"        (line 68 — already there)
;
; 2. NO CHANGE REQUIRED in csound/signal_chain.orc
;      Stage 22.7's custom bypass block (lines ~478-497) is correct as-is and
;      MUST be kept as-is: it is NOT $FX_BYPASS, because the 2 s linsegr trail
;      release at the bottom of instr 157 needs gaResOut to keep flowing after
;      toggle-off. gaResSendL/R and gaResOutL/R are read/written exactly as
;      before, and the ntrpol dry blend is what gives the DRY path zero latency.
;
; 3. NO CHANGE REQUIRED in csound/handler/handler_menus.orc
;      Toggle 47 ($FX_TOGGLE(gkRES_bypass'47)) and all five $ENC_ROT lines keep
;      working with identical ranges and steps:
;        line ~1370  $ENC_ROT(gkRES_mix'0.02'0'1)      P1  MIX     0..1
;        line ~1837  $ENC_ROT(gkRES_onset'0.02'0'1)    P2  DECAY   0..1
;        line ~1972  $ENC_ROT(gkRES_fb'0.02'0'1)       P3  FOCUS   0..1
;        line ~1698  $ENC_ROT(gkRES_oct'0.04'-1'1)     P4  TILT   -1..1
;        line ~1584  $ENC_ROT(gkRES_frz'1'0'1)         P5  FREEZE  0/1
;      The toggle-on defaults at lines ~364-368 are ALSO still good values under
;      the new meanings, so they can be left alone:
;        gkRES_mix=0.4  gkRES_onset=0.5  gkRES_fb=0.6  gkRES_oct=0  gkRES_frz=0
;        (= 40% wet, tau ~1.8 s, focus -29 dB, natural tilt, unfrozen)
;
; 4. OLD -> NEW PARAM MAPPING (semantics only; identifiers/ranges unchanged)
;      ------------------------------------------------------------------
;      global        old meaning            NEW meaning              range
;      ------------------------------------------------------------------
;      gkRES_mix     dry/wet                dry/wet   (SAME)         0..1
;      gkRES_onset   onset follow speed  -> DECAY  (tau 0.08s..40s)  0..1
;      gkRES_fb      pipe/regen feedback -> FOCUS  (peak threshold)  0..1
;      gkRES_oct     octave companion    -> TILT   (damping slope)   -1..1
;      gkRES_frz     freeze pipe alloc   -> FREEZE (hold spectrum)   0..1
;      ------------------------------------------------------------------
;
; 5. COSMETIC-ONLY EDITS (safe to defer; nothing breaks without them)
;    a) neptrPhase4_UI.py line ~198 — relabel (keys/ranges unchanged, so the
;       OSC routing and encoder mapping keep working either way):
;         "res": [("mix", "Mix", 0, 1), ("onset", "Decay", 0, 1),
;                 ("fb", "Focus", 0, 1), ("oct", "Tilt", -1, 1),
;                 ("frz", "Freeze", 0, 1)],
;       (FILTERING menu slot 3 ("res","RESONATR") at line ~127 is correct.)
;    b) csound/handler/handler_osc.orc lines ~295-300 — the six $OSC_* lines are
;       fine as they stand. ONLY if you want the wire names to read true:
;         /res_onset -> /res_decay,  /res_fb -> /res_focus,  /res_oct -> /res_tilt
;       If you rename these you MUST rename them in neptrPhase4_UI.py's
;       route_osc at the same time. Renaming is optional and I recommend
;       deferring it until the rest of the batch has landed.
;    c) handler_menus.orc printks strings mentioning "sympathetic pipe bank"
;       are now wrong; the comment at line ~361 can say "spectral resonator".
;
; 6. globals.orc: NOTHING required. This file declares and inits every gkRES_*,
;    gaRes* and giRes* it uses. If midi_handler/globals re-init the gkRES_*
;    channels it is harmless (init, not =).
; ===========================================================================

; ---- channels & defaults (safe to re-init elsewhere) ----
gkRES_bypass init 1
gkRES_mix    init 0.4       ; P1  dry/wet
gkRES_onset  init 0.5       ; P2  DECAY   (name kept from v1 — see notes)
gkRES_fb     init 0.6       ; P3  FOCUS   (name kept from v1 — see notes)
gkRES_oct    init 0         ; P4  TILT    (name kept from v1 — see notes)
gkRES_frz    init 0         ; P5  FREEZE
gaResSendL   init 0
gaResSendR   init 0
gaResOutL    init 0
gaResOutR    init 0

; ---- analysis geometry ----
; 4096 / 11.72 Hz bins. This is a RESOLUTION requirement, not a preference, and
; it was forced by the bench:
;   At 2048 (23.44 Hz bins) the partials of an A major triad at A3 sit only
;   2.2-2.4 bins apart — closer together than a Hann main lobe is wide (4 bins).
;   The lobes overlap, their leakage adds constructively in the bins BETWEEN the
;   partials, and that overlap forms a shallow local maximum which is not a
;   partial at all. The accumulator cannot tell the difference, injects it, and
;   it rings forever: bench SCN 0 showed a ghost at 305.7 Hz (exactly bin 13,
;   between the C#4 and E4 partials) sustaining at -16.8 dB — a sour non-chord
;   tone in every single chord tail. No peak-picking rule can fix this: a
;   +/-2-bin local-max window would make the real chord partials, which are only
;   2.2 bins apart, cannibalise each other instead.
;   At 4096 those partials are 4.4-4.9 bins apart, the main lobes separate, and
;   the ghost has nowhere to form. Verified: the tail is now exactly 3 peaks.
;
; The cost of this is ZERO CPU and 85 ms of wet-path latency:
;   * CPU: the per-bin pass runs (N/2) bins at (sr / (N/4)) frames per second =
;     2*sr bin-updates per second REGARDLESS of N. Doubling the FFT doubles the
;     bins and halves the frame rate. It is a wash. (The FFTs themselves are
;     ~140/s of size 4096 — noise.)
;   * Latency: 4096/48000 = 85.3 ms, on the WET only. The dry path is untouched
;     and zero-latency (the chain's ntrpol taps gaResSend directly). 85 ms of
;     pre-delay on a resonant bloom is not a defect — it reads as the tail
;     swelling in behind the pick attack, which is what this effect is for.
giResFFT   = 4096
giResHop   = 1024                       ; 4:1 overlap -> 46.875 frames/s
giResWin   = 4096
giResBins  = giResFFT / 2 + 1           ; 2049
giResBinHz = sr / giResFFT              ; 11.71875 Hz
giResBinRcp = 1 / giResBinHz            ; bin-deviation scaler (sinusoidality)
giResLo    = 4                          ; ~47 Hz  (below the low E fundamental)
giResHi    = 683                        ; ~8 kHz  (above this it is only hiss)
giResHopT  = giResHop / sr              ; 21.33 ms per frame

; ---- engine constants ----
giResInj    = 0.35      ; injection gain per frame into an admitted peak bin
giResCap    = 2.0       ; hard per-bin ceiling, x the input peak-hold (BOUND)
giResMakeup = 1.0       ; output makeup (bench-calibrated: wet ~ input RMS)
giResFloor  = 0.00003   ; absolute magnitude floor — silence must not "ring"

instr 157  ; RESONATOR — spectral magnitude decay
  ; =========================================================================
  ; i-time: per-bin constant tables. Rebuilt on each toggle-on (2049 iterations,
  ; free). iLogF is log(f_b / 200 Hz) — the TILT exponent multiplies it.
  ; iPanL/R scatter bins across the stereo field on a golden-angle sequence:
  ; decorrelated and wide, energy-preserving (L^2+R^2 = 1), mono-sum safe.
  ; =========================================================================
  iLogF[] init giResBins
  iPanL[] init giResBins
  iPanR[] init giResBins
  iCtr[]  init giResBins
  ii = 0
  while ii < giResBins do
    iF = ii * giResBinHz
    iCtr[ii]  = iF
    iLogF[ii] = log((iF > 20 ? iF : 20) / 200)
    iP        = 0.5 + 0.30 * sin(ii * 2.39996323)   ; golden angle
    iPanL[ii] = sqrt(iP)
    iPanR[ii] = sqrt(1 - iP)
    ii += 1
  od

  ; ---- controls ----
  ; NB gkRES_mix is deliberately NOT read here: the chain's stage-22.7 ntrpol owns
  ; the dry/wet blend, and that is what keeps the DRY path at zero latency. This
  ; instrument is always 100% wet.
  kDecay portk limit:k(gkRES_onset, 0, 1), 0.05
  kFocus portk limit:k(gkRES_fb,    0, 1), 0.05
  kTilt  portk limit:k(gkRES_oct,  -1, 1), 0.05
  kFrz   = (gkRES_frz > 0.5 ? 1 : 0)

  ; DECAY -> base magnitude e-fold time, exponential 0.08 s .. 40 s.
  ; (T60 = 6.9 * tau, so 0.55 s .. ~4.6 min. At the top the tail outlives the
  ;  song, which is the "infinite drone" end of the spec.)
  kTau   = 0.08 * pow(500, kDecay)
  kLogG  = -giResHopT / kTau               ; g[b] = exp(kLogG * d[b])
  kTiltE = 0.45 - 0.45 * kTilt             ; damping slope exponent

  ; FOCUS -> injection threshold, dB below the frame's peak bin.
  ; 0 -> -60 dB (everything that is a peak rings: dense cloud)
  ; 1 -> -9  dB (only the dominant partials ring: clean singing voice)
  kThrDb  = -60 + 51 * kFocus
  kThrRel = pow(10, kThrDb / 20)

  ; ---- input conditioning (mono analysis; the chain owns the dry path) ----
  aMono = (gaResSendL + gaResSendR) * 0.5
  aCond dcblock aMono
  aCond buthp aCond, 55                    ; subsonics/handling rumble out

  ; ---- analysis ----
  fIn     pvsanal aCond, giResFFT, giResHop, giResWin, 1
  kMag[]  init giResBins
  kFrq[]  init giResBins
  kFlag   pvs2tab kMag, kFrq, fIn
  kPrevFl init -1

  ; ---- persistent spectral state ----
  kAcc[]  init giResBins                   ; A[b] : accumulated magnitude
  kFrqA[] init giResBins                   ; F[b] : held frequency
  kHold[] init giResBins                   ; H[b] : input peak-hold (the BOUND)
  kOutM[] init giResBins
  kOutL[] init giResBins
  kOutR[] init giResBins
  kOutF[] init giResBins

  kSeed init 0
  if (kSeed == 0) then
    kb = 0
    while kb < giResBins do
      kFrqA[kb] = iCtr[kb]                 ; unseeded bins sit at their centre
      kOutF[kb] = iCtr[kb]
      kb += 1
    od
    kSeed = 1
  endif

  ; =========================================================================
  ; ONE PASS PER ANALYSIS FRAME (46.875 Hz), bins giResLo..giResHi
  ; =========================================================================
  if (kFlag != kPrevFl) then

    ; ---- per-bin decay gain table: g[b] = exp(kLogG * (f_b/200)^e) ----
    ; Depends ONLY on the DECAY and TILT knobs, so it is rebuilt only when one of
    ; them actually moves — which, on a pedalboard, is almost never. Removes two
    ; exp() per bin per frame (64k exp/s) from the steady state.
    ; The rebuild lives INSIDE the frame branch on purpose: at the k-rate (375 Hz)
    ; a moving portk-smoothed knob would retrigger it on EVERY k-cycle and make it
    ; 8x more expensive than the thing it replaced. Worst case (knob being swept)
    ; is now exactly the old cost; the common case is free.
    ; Measured: worth ~3% of the instrument. The exp() calls were never the
    ; bottleneck — Csound's interpreted ARRAY INDEXING is (see the header's CPU
    ; note), which is why the main loop below hoards scalars so aggressively.
    kGArr[] init giResBins
    kCacheD init -999
    kCacheT init -999
    if (abs(kDecay - kCacheD) > 1e-6 || abs(kTilt - kCacheT) > 1e-6) then
      kb = giResLo
      while kb <= giResHi do
        kDmp = exp(kTiltE * iLogF[kb])
        kDmp = limit:k(kDmp, 0.7, 24)      ; keep the bass from ringing forever
        kGArr[kb] = exp(kLogG * kDmp)
        kb += 1
      od
      kCacheD = kDecay
      kCacheT = kTilt
    endif

    ; frame peak (for the FOCUS threshold) — content-relative, not absolute,
    ; so the effect behaves the same on a quiet fingerpick and a loud strum
    kPk = 0
    kb  = giResLo
    while kb <= giResHi do
      kPk = (kMag[kb] > kPk ? kMag[kb] : kPk)
      kb += 1
    od
    kThr = kPk * kThrRel
    kThr = (kThr > giResFloor ? kThr : giResFloor)

    ; Rolling 3-bin magnitude window. The peak test needs kMag[kb-1..kb+1], but
    ; re-indexing the array three times per bin is pure waste: each Csound array
    ; read is an interpreted opcode dispatch (~15 ns), and array indexing — not
    ; the arithmetic and not the FFTs — is this instrument's dominant cost. We
    ; slide the window through scalars instead: 1 array read per bin, not 3.
    kMlo = kMag[giResLo - 1]
    kMi  = kMag[giResLo]

    kb = giResLo
    while kb <= giResHi do
      kMhi = kMag[kb + 1]                  ; the only kMag read in the loop
      kG   = (kFrz == 1 ? 1 : kGArr[kb])   ; FREEZE: g = 1, perfect hold

      ; ---- peak-gated injection (FOCUS) ----
      ; (i) local magnitude maximum => a real partial, not a leakage skirt.
      ; This is what keeps the tail clean instead of smeared, and it is why
      ; there is no pitch detection anywhere: we never decide WHICH NOTE this
      ; is, only that this bin is where energy actually lives.
      kIsPk = ((kMi > kMlo && kMi >= kMhi && kMi > kThr) ? 1 : 0)

      ; (ii) SINUSOIDALITY. A genuine sinusoid's peak bin reports a PV frequency
      ; within +/-0.5 bin of that bin's own centre by construction. A bin whose
      ; energy is noise, or is dominated by a neighbour's leakage skirt, reports
      ; a frequency that does not belong to it. Rejecting those keeps noise bins
      ; from ringing on as mis-tuned sine tones.
      ;
      ; This must NOT be done with a magnitude prominence margin
      ; (kMi > kMag[kb-1]*1.3 etc). A partial that lands exactly BETWEEN two bins
      ; splits its energy evenly across them, so no margin test can pass, and the
      ; partial would silently drop out of the tail. The frequency test has no
      ; such blind spot: a straddling bin still reports ~0.5 bin deviation.
      ;
      ; NOTE (honest bookkeeping): this gate is NOT what killed the 305.7 Hz
      ; chord ghost — that ghost reported a deviation of only 0.04 bin and sailed
      ; straight through here. Constructive leakage between two partials points
      ; at the bin centre, not away from it. The ghost was a frequency-RESOLUTION
      ; artifact and was fixed by giResFFT (see the header). This gate earns its
      ; keep on noise/percussive input, not on chords.
      ;
      ; Guarded by kIsPk so it costs nothing on the ~95% of bins that already
      ; failed the magnitude test: only a handful of bins per frame are peaks, and
      ; this is two array reads we do not pay for on any of the others.
      if (kIsPk == 1 && kFrz == 0) then
        kDev  = abs(kFrq[kb] - iCtr[kb]) * giResBinRcp
        kIsPk = ((kDev < 0.7) ? 1 : 0)
      else
        kIsPk = 0                          ; FREEZE admits nothing
      endif
      kInj  = kIsPk * kMi * giResInj

      ; ---- magnitude accumulator + peak-hold ceiling ----
      kAg = kAcc[kb] * kG                  ; what is left of the tail
      kH  = kHold[kb] * kG
      kH  = (kFrz == 1 ? kHold[kb] : (kMi > kH ? kMi : kH))
      kHold[kb] = kH
      kA  = kAg + kInj
      kCp = kH * giResCap                  ; <== THE BOUND. A <= 2 * peak-hold.
      kA  = (kA < kCp ? kA : kCp)
      kAcc[kb] = kA

      ; ---- magnitude-weighted frequency update (no tracking, no snapping) ----
      ; w = share of this bin's energy that is NEW. Fresh partial -> w~1 ->
      ; snaps to the true frequency. Ringing tail -> w~0 -> holds, cannot drift.
      ; Only an INJECTING bin can move; a decaying one holds its frequency, so we
      ; also skip its kOutF write entirely (the array already holds the value).
      if (kInj > 0) then
        kW  = kInj / (kInj + kAg + 1e-12)
        kCf = iCtr[kb]
        kFn = kFrqA[kb]
        kFn = kFn + kW * (kFrq[kb] - kFn)
        ; +/-1 bin clamp: a genuine peak's PV estimate lives within +/-0.5 bin,
        ; so this cannot alter a good estimate and cannot let a bad one detune.
        kFn = limit:k(kFn, kCf - giResBinHz, kCf + giResBinHz)
        kFrqA[kb] = kFn
        kOutF[kb] = kFn
      endif

      ; ---- render this bin ----
      kAo = kA * giResMakeup
      kOutL[kb] = kAo * iPanL[kb]
      kOutR[kb] = kAo * iPanR[kb]

      kMlo = kMi                           ; slide the magnitude window
      kMi  = kMhi
      kb += 1
    od
  endif
  kPrevFl = kFlag

  ; ---- resynthesis: two IFFTs, one per channel (stereo scatter is in kOutL/R)
  fOutL  tab2pvs kOutL, kOutF, giResHop, giResWin, 1
  fOutR  tab2pvs kOutR, kOutF, giResHop, giResWin, 1
  aWetL  pvsynth fOutL
  aWetR  pvsynth fOutR

  ; ---- absolute ceiling (the spectral cap already bounds growth; this just
  ;      guarantees we can never leave [-1,1] into the amp) ----
  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL = tanh(aWetL)
  aWetR = tanh(aWetR)
  aWetL limit aWetL, -0.99, 0.99
  aWetR limit aWetR, -0.99, 0.99

  ; ---- 2 s trail release after toggle-off (turnoff2 mode 1, custom bypass) ----
  kRelEnv linsegr 1, 0.01, 1, 2, 0
  gaResOutL = aWetL * kRelEnv
  gaResOutR = aWetR * kRelEnv
endin

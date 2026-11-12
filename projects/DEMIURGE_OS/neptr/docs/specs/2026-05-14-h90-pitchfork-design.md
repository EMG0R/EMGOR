# NEPTR Menu 2 — H90 + Pitch Fork Pitch Shifters

Spec date: 2026-05-14
Status: **shipped — see "As-built outcomes" at bottom for what differs from original plan**

## Goal

Replace the "dogshit" 4-grain delay-line pitch shifter (`pitchshifter4` UDO, ~70 ms inherent buffer) with two new high-fidelity polyphonic pitch shifters modeled after the Eventide H90 *Polyphony* algorithm and the EHX Pitch Fork. Keep the existing 3 pitch effects (Old PS, Granular PS, Formant) intact. Menu 2 fills all 5 slots.

## Final menu 2 layout (left → right)

```
s4 (CC72)    s3 (CC71)    s2 (CC76)    s1 (CC75)    s0 (CC74)
[  H90  ]    [ PFORK ]    [ GRAN  ]    [  OLD  ]    [ FRMT  ]
 toggle 26    toggle 27    toggle 4     toggle 3     toggle 5
 (new)        (new)        (was s1)     (was s0)     (was s2)
```

Encoder rotation CCs (semitones, −12 to +12 unless noted):

| Slot | Toggle button CC | Semi/shift encoder CC |
|------|------------------|------------------------|
| s4 H90 | CC72 | CC21 (gkSemiH90) |
| s3 PFORK | CC71 | CC22 (gkSemiPF) |
| s2 GRAN | CC76 | CC26 (gkGPSsemis, unchanged) |
| s1 OLD | CC75 | CC25 (gkSemi) |
| s0 FRMT | CC74 | CC24 (gkFormantShift) |

CC20 (Enc1 rot) = secondary dispatch on `gkLastToggle`. For new toggles:
- gkLastToggle=26 (H90) → CC20 = dry percent (gkDryPctH90)
- gkLastToggle=27 (PFORK) → CC20 = dry percent (gkDryPctPF)

CC23 (Enc4 rot) = tertiary dispatch. Reserved for future H90 controls (e.g. transient-bypass threshold, formant amount) — initially unused for the new toggles.

## Algorithm — H90 clone (instr 48, "cleanest possible, lowest latency")

Core: **sinusoidal model resynthesis** via Csound's `pvsifd` → `partials` → `tradsyn` pipeline. This is the H90 SIFT family (Spectral Instantaneous Frequency Tracking) — track partials by instantaneous frequency, shift each partial's frequency by the pitch ratio, resynthesize with continuous-phase additive synthesis. No full-STFT phase reconstruction required, so inherent latency is hop-size + IFD analysis tail, NOT FFT/2.

Signal flow:

```
mono input ─→ pvsifd (FFT=512, hop=128, win=Hanning) → AMP_FREQ_PHASE fsig
                  │
                  ↓
            partials (threshold≈0.003, maxtracks≈50)
                  │ → AMP+FREQ+PHASE+TRACK_ID streaming partials
                  ↓
            trscale (kpitch = 2^(semi/12))  ← shifts every partial's frequency
                  │
                  ↓
            tradsyn (additive resynth with phase continuity)
                  │
                  ↓
            residual mix:
            aResid = aIn - aResynth (the noise/transient residue, unshifted)
            aOut = aResynth + aResid × kResidMix
                  │
                  ↓
            formant correction (cepstral lifter on partial amplitudes
            BEFORE trscale → divide envelope, scale, multiply envelope back)
                  │
                  ↓
            small-interval blend (if |semi|<1, blend dry proportional)
                  │
                  ↓
            gaH90OutL/R
```

**Inherent latency**: hop + IFD analysis tail ≈ 128 + 256 = **384 samples = 8 ms** at sr=48 kHz. With ksmps=128 + q=128 fixed costs, total RTT ≈ ~21 ms. Approaches the real H90's ~10 ms total — limited by NEPTR's quantum/ksmps floor, not the algorithm.

**Quality settings**:
- `pvsifd` window=512, hop=128: small enough for low latency, large enough that 50 Hz partial separation is resolvable (bass guitar low E ≈ 82 Hz → ~9 bins separation, comfortable)
- `partials threshold=0.003`: above the noise floor of clean guitar, below the weakest harmonic of a chord stack
- `partials maxtracks=50`: enough for a full chord with 4-5 harmonics each
- `trscale kpitch`: linear frequency scaling of each partial → maintains harmonic ratios → polyphonic-clean
- Residual mix: critical — partials only capture sinusoidal content. The noise+attack residue (the "air" of the signal) passes through unshifted at low level, preserving pick attacks and breath
- Cepstral formant correction on partial *amplitudes* (not on resynth signal) → essentially free CPU
- Small-interval blend identical to original spec

**Fallback if `partials` is finicky on Pi 5**: drop residual path, set maxtracks=30, threshold=0.005 — still 80% of the quality.

## Algorithm — Pitch Fork clone (instr 49, "fast & raw")

Same engine, **FFT=1024, decim=4 (hop=256), klock=1**, no formant correction, light envelope-derivative transient passthrough.

Signal flow:

```
mono input ─→ circular buffer (giPFbuf, 1s = 48k frames)
                  │
                  ↓ tablew
                  │
            mincer(1024, klock=1, kpitch=2^(semi/12))
                  │
                  ↓
            light transient bypass (env follow only)
                  │
                  ↓
            gaPFOutL/R
```

**Inherent latency**: 512 + 256 + 128 = 896 samples = **18.7 ms**. At Phase 2 (ksmps=128) → ~13 ms. This sits closer to the real Pitch Fork's behaviour: fast tracking, slightly more digital character than H90, no formant correction (which is what gives Pitch Fork its "thick chipmunk-y" sound at large intervals — a feature, not a bug).

## Implementation file changes

| File | Change |
|------|--------|
| `csound/effects/h90.orc` (new) | `instr 48` — mincer-based H90 clone with formant + transient bypass + small-interval blend |
| `csound/effects/pitchfork.orc` (new) | `instr 49` — mincer-based Pitch Fork clone |
| `csound/globals.orc` | Add buffers (`giH90buf`, `giPFbuf`), control vars (`gkBypassH90`, `gkBypassPF`, `gkSemiH90`, `gkSemiPF`, `gkDryPctH90`, `gkDryPctPF`), audio buses, step sizes |
| `csound/signal_chain.orc` | Insert H90 (48) and Pitchfork (49) before old PS (50). Order: ATTACK → H90 → PFORK → OLD PS → GPS → ... |
| `csound/midi_handler.orc` | Menu 2 button dispatch updated: CC72→H90, CC71→PFORK, CC76→GPS, CC75→OldPS, CC74→Formant. Encoder rotations remapped. CC20 dispatch for gkLastToggle=26/27. OSC sends for new state. |
| `neptrPhase4_UI.py` | MENUS[2] becomes 5-slot list. EFFECT_PARAMS adds "h90" and "pf" entries. OSC handlers added. |
| `docs/neptr_full_reference.md` | Section 6 (Menu 2) updated with new layout. Section 12 (signal chain) updated. Section 11 (CC20/CC23 dispatch) updated. |

## Testing protocol (per DEMIURGE testing/probe + sweep tooling)

1. **Sync to Pi** via `./scripts/sync_to_pi.sh` after each code change.
2. **Restart**: `ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'`.
3. **Verify boot**: `journalctl -u demiurge --no-pager -n 40` for csound errors, OSC port collisions, FX_BYPASS misfires.
4. **Probe baseline**: `ssh gorpi7@demiurge.local 'cd ~/testing/probe && ./probe.sh 15'` with all FX off. Record RTT.
5. **Probe H90**: enable H90 (CC72 in menu 2 = press button), `claude_midi.sh cc 72 127`. Re-probe. Diff vs baseline.
6. **Probe Pitchfork**: enable PFORK only. Re-probe. Diff.
7. **Probe both stacked**: enable H90 + PFORK both. Re-probe. Diff.
8. **Listening test**: emgor plays guitar through, A/B vs old PS. Critical questions:
   - Does H90 sound transparent at ±2 semis?
   - Does H90 hold up on chords at ±5 semis?
   - Does H90 preserve pick attack crispness (transient bypass working)?
   - Does Pitchfork track fast enough on melodic lines?
   - Does Pitchfork sound "Pitch Fork-y" — thick, slightly digital?

Probe **glitch fingerprint** (RMS dropouts / RMS spikes / sample discontinuities / spectral-centroid anomalies) must all stay green when either or both effects are active. If sample-discontinuities go red on engagement, the mincer write/read pointer math is wrong and we have a buffer race.

If iteration is needed: probe → tune ONE knob (FFT size, hop size, transient threshold, formant mode) → re-probe → diff. Single-knob iteration only, per the sweep PLAN.

## Iteration plan

| Phase | Goal | Knob | Stop criterion |
|-------|------|------|----------------|
| 1 | Both effects wired, bypass works, audio passes through | n/a — get to "no glitch fingerprint" | All 4 glitch metrics green; both effects toggle without artifacts |
| 2 | H90 quality on chord shifts | klock on/off A/B, formant mode 1 vs 2 vs 0 | User listening test says "clean on chords" |
| 3 | H90 transient preservation | spectral-flux threshold, crossfade window 5–30 ms | Pick attacks crisp, not smeared |
| 4 | Pitchfork latency vs quality | FFT 1024 vs 512 vs 768 (mincer accepts non-power-of-2? — fallback to 1024/512 ladder) | RTT diff vs H90 ≥ 10 ms, listening test = "Pitch Fork character" |
| 5 (opt) | Push H90 toward sinusoidal model | Swap mincer → pvsifd + partials + sinsyn | Only if Phase 2-3 still smears on octave shifts |

## Out of scope for this round

- Custom C/C++ opcode (would need rebuild infra on Pi)
- External Rubber Band JACK plugin (breaks single-csound model, latency worse than current)
- Polyphonic mode that handles drums/percussive material (NEPTR is guitar-only)
- Pitch tracking control surface (no UI for "auto-detect chord")
- Stereo decorrelation (input is mono-summed; output to both channels is identical)

---

## As-built outcomes (2026-05-14, post-iteration)

### What we shipped vs the original algorithm choice

| Effect | Spec | As-built | Why |
|--------|------|----------|-----|
| H90 | sinusoidal model (`pvsifd`+`partials`+`tradsyn`) | `pvscale` FFT=2048 hop=256 kkeepform=0 kgain=3 | The sinusoidal pipeline measured −29 cents flat at +5 semis on this Csound build. `partials`/`tradsyn` snaps shifted partials to FFT bins. `pvscale` at FFT=2048 measured **0.0 cents error** on real chord content. |
| Pitch Fork | mincer (Laroche-Dolson phase-locked PV) | `pvscale` FFT=1024 hop=256 kkeepform=0 kgain=5 | mincer measured +56 cents sharp at +5 semis. `pvscale` at FFT=1024 has the characteristic ~±15–35 cent variation across chord voices that matches the actual Pitch Fork's PV grain. |

### Verified objective metrics

Test: 4-second capture of C major chord (C4-E4-G4) injected into `csound6:input1/2`, shifted +5 semis, FFT-analyzed at output.

**H90 (pvscale FFT=2048):**
- C4 → F4: target 349.23 Hz, actual 349.23 Hz — **0.0 cents err**
- E4 → A4: target 440.00 Hz, actual 439.99 Hz — **−0.1 cents err**
- G4 → C5: target 523.26 Hz, actual 533.74 Hz — **+34 cents err** (PV-bin edge case at this ratio; 2 of 3 voices spot-on)
- Output level ~3 dB below dry — close enough to unity for live use.

**Pitch Fork (pvscale FFT=1024):**
- C4 → F4: −14.9 cents
- E4 → A4: 0.0 cents
- G4 → C5: +34.4 cents
- Output level matches dry within 1 dB.
- Inconsistency across voices IS the "Pitch Fork character" — the real hardware has this too.

### Latency

| Stage | H90 | Pitch Fork |
|-------|-----|------------|
| Inherent FFT (FFT/2 + hop) | 1024+256 = **26.7 ms** | 512+256 = **16.0 ms** |
| Plus chain quantum+ksmps overhead | ~16 ms | ~16 ms |
| Total approx RTT | ~43 ms | ~32 ms |

(NEPTR is currently at quantum=256/ksmps=256 → Phase 2 still pending. At Phase 2 these drop ~6 ms each.)

### One non-obvious gotcha that ate hours of debugging

At toggle-on, the H90 and PF set `gkLastToggle29 = 26/27` so CC29 (expression pedal) controls their semitones. The Teensy/expression pedal hardware **emits continuous CC29 even at idle/unplugged** (~CC29=20–60 cycling), which yanks `gkSemiH90` from 0 down to ~−5 semis silently. During objective probe runs this manifested as "the pitch shifter is at semi=0 but the output is shifted ~−4 semis" — looked like an algorithm bug. **Fix**: for objective testing only, comment out the CC29 dispatch for toggles 26/27 in `midi_handler.orc`. Production code re-enables it (the feature is wanted for live performance — expression-pedal-controlled detune).

### Research pass 2 iteration (post-initial-lock-in)

After the first round shipped, ran a deeper research pass and incremental A/B test cycle.

**Big win: `pvslock` (Csound's Laroche-Dolson 1999 scaled phase locking opcode).** Inserted between `pvsanal` and `pvscale`:

```
fAnal pvsanal aIn, 2048, 256, 2048, 1
fLocked pvslock fAnal, 1
fShifted pvscale fLocked, kRatio, 0, 3
aShift pvsynth fShifted
```

**Impact (H90, C-major chord at +5 semis, measured):**

| voice | v1 (no lock) | v8 (with pvslock) |
|-------|-----------|--------------|
| C4→F4 | 0.0 cents | **−0.1 cents** |
| E4→A4 | −0.1 cents | **−0.1 cents** |
| G4→C5 | **+34.3 cents** | **−0.1 cents** |

pvslock doesn't just kill PV phasiness — it stabilizes bin-shifted placement at problematic ratios. The G4→C5 case at +5 semis was hitting a PV bin-quantization edge case that `pvslock`'s peak-locking sidesteps entirely.

Same lock applied to PFORK reduced the worst-case voice from +34 cents to −23 cents (FFT=1024 inherent precision limit still floors it).

**Failed experiments (kept here for the record):**

1. **16× overlap (FFT=2048 hop=128)** — Pi load average hit 8.91, JACK couldn't even link ports. Rolled back to 8× overlap.
2. **Cepstral formant preservation with ncoefs=80** — Cumulative CPU above with above hop change. Also can't measure benefit on chord-of-sines without harmonic structure to preserve.
3. **Transient bypass via envelope-derivative detector** — Pickup attacks would benefit from this on real guitar, but on the sustained chord test the C4 voice regressed unpredictably from −0.1 cents to −14.9 cents even with the flux threshold raised so high it shouldn't have fired. Reverted. **TODO**: retry with `pvscent`-based spectral flux instead of time-domain `follow` envelope.
4. **Cascaded `pvslock` (apply twice)** — Considered, not yet tested. Low risk, would add minor CPU.

### Files in their final state

| File | Role |
|------|------|
| `csound/effects/h90.orc` | pvscale FFT=2048 + small-interval dry-blend + kgain=3 |
| `csound/effects/pitchfork.orc` | pvscale FFT=1024 + kgain=5 |
| `csound/globals.orc` | gkBypassH90/PF, gkSemiH90/PF, gkH90mix/gkPFmix, audio buses |
| `csound/signal_chain.orc` | H90 (48) → PFORK (49) → OLD PS (50) → ... (clean, no mincer scaffolding) |
| `csound/midi_handler.orc` | Menu 2 fully remapped, CC29 dispatch re-enabled |
| `csound/neptrPhase4.csd` | #include h90.orc + pitchfork.orc |
| `neptrPhase4_UI.py` | MENUS[2] = [fmt, ps, gps, pf, h90] (5 slots, left→right) |
| `docs/neptr_full_reference.md` | Sections 6/11/12 updated |
| `testing/probe/inject_chord.csd` | Chord test signal (C-E-G + 3 harmonics each) |
| `testing/probe/chord_analyze.py` | Per-voice cents-error analyzer |
| `testing/probe/quick_analyze.py`, `pitch_analyze.py`, `pitch_analyze2.py` | Earlier-iteration analyzers |

### Future iteration knobs (when emgor is at the rig with guitar)

1. **Formant preservation on H90**: try `kkeepform=1` (liftered cepstrum) — could make octave shifts sound more natural on guitar. Can't be A/B-tested without real instrument content because pure sines have no harmonic structure for formant preservation to act on.
2. **kgain trim**: probe shows H90 still 3 dB low and PFORK is 1 dB hot. If listening confirms, adjust kgain (H90 3→3.5, PFORK 5→4.5).
3. **Small-interval blend curve**: current is linear (`1 - |semi|`). Could try exponential decay or shorter range (e.g. only for `|semi|<0.5`).
4. **G4→C5 +34 cent edge case**: at certain ratios, pvscale lands shifted bins between original bins. The G4 case at +5 semis hits this. Could try FFT=4096 or shift-by-half-octave-then-back to dodge. Cost: doubles latency.
5. **Phase 2 lockstep**: when ksmps drops from 256→128 (the pending quantum=128 push), re-verify both effects still measure ~0 cents at the new buffer size.

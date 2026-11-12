# BroadcastPre — analog-modeled always-on preamp

Date: 2026-06-11
Status: approved (user), implementing

## Goal

Replace the static EQ+tanh `Preamp` UDO with a stage-by-stage model of the
**Hudson Broadcast** — a transformer-coupled, discrete class-A germanium
preamp pedal based on 1960s broadcast consoles. NEPTR feeds a real amp, so
this is the classic "super nice sounding pedal squish in front of the amp,"
not amp modeling. Default voicing is **clean glue** (24 V-headroom vibe);
hotter voicings are built in for a later UI hookup.

Hardware reference: silicon input transistor → NOS germanium transistor
(class A) → Triad steel-core output transformer. Gain saturates the
transformer + discrete circuitry → "gentle and dynamic compression coupled
with subtle thickening of the mid-range" (Hudson). Sources: Hudson
Electronics product page, Aion FX "Skywave" circuit trace, MusicRadar
review, ElectroSmash-style stage analysis practice.

## Decisions (from brainstorming)

- Model = Hudson Broadcast lineage, NOT a compressor, NOT a Klon, NOT amp sim.
- Always-on at chain position 0, same `Preamp aa → aa` opcode interface —
  `signal_chain.orc` is untouched.
- Fixed tuning (i-rate). `giPreVoice` constant selects voicing:
  `0 = CLEAN GLUE` (default), `1 = EDGE OF BREAKUP`, `2 = PUSHED`.
  Each voicing is a row of i-rate constants → later UI = swap constant
  for a global, no restructure.
- Full stage-by-stage model, **no oversampling** (clean-glue soft clipping
  generates only low-order harmonics; oversampling is cost without audible
  return at this drive level — can be added later if a hotter voicing fizzes).

## Signal flow (per channel; one shared envelope for both)

```
in ─▶ HP 80 Hz ─▶ [Si stage] ─▶ [Ge stage] ─▶ [transformer] ─▶ LP + trim ─▶ out
```

1. **HP 80 Hz** (kept from old preamp) — subsonic guard, also keeps the
   transformer low-band model sane.
2. **Si input stage** — small squared-term even-harmonic seasoning,
   DC-blocked, 25 Hz coupling-cap highpass. Near-transparent floor.
3. **Ge stage (the "intelligent" part)** — shared envelope follower
   (`follow2`, attack ≈ 5 ms, release ≈ 120 ms, mono sum, like one box /
   one supply) moves the clipping bias point:
   `out = (tanh((in + bias)·drive) − tanh(bias·drive)) / drive`,
   `bias = bias0 + env·biasDepth`. Hard picking → more asymmetry → 2nd-
   harmonic bloom + compression "give," relaxing as the note decays.
4. **Transformer** — phase-coherent band split at ~250 Hz (`tone` + subtract);
   low band saturates harder (steel-core flux limits hit LF first → low-end
   *thickening*); +1 dB peaking bump ~80 Hz; one-pole HF rolloff 12 kHz;
   `dcblock2`.
5. **Output** — fizz LP ~9 kHz; per-voicing makeup trim so the stage is
   **unity loudness** (always-on requirement); safety `limit ±0.99`.

The old static low/high shelves are deleted — the Ge stage + transformer are
the color now.

## Files touched

- `csound/effects/preamp.orc` — full rewrite (UDOs `BroadcastChan` mono core
  + `Preamp` stereo wrapper, name unchanged).
- Nothing else. No globals, no MIDI, no UI.

## Verification

1. Throwaway test `.csd` includes `effects/preamp.orc`, renders a synthetic
   pluck offline at sr 48000 / ksmps 128 on the Mac.
2. Checks: output RMS ≈ input RMS (unity, ±1 dB), no NaN, no DC offset,
   2nd harmonic present above noise floor, peaks bounded.
3. Syntax-check / compile the full `neptrPhase4.csd`.
4. `scripts/sync_to_pi.sh` → `sudo systemctl restart demiurge` → journal check.

## CPU budget

All one-pole filters, one `follow2`, point-wise `tanh` — ~4-5× the old
preamp, negligible vs the existing chain (spectral delay, granular, etc.).

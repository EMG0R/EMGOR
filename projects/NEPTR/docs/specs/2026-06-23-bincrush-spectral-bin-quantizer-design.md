# BINCRUSH — Spectral Bin Quantizer (design)

Date: 2026-06-23
Status: shipped to source tree (syntax-checked); on-Pi by-ear pass pending

## Summary

A new spectral effect that keeps only the **loudest N FFT bins** each frame and
fades the rest toward silence. The expression pedal sweeps N from "all bins"
(near-clean) down to "a handful" (sparse, glassy, frozen-sine texture). Always
100% wet, no mix knob. Tuned to sound **soft / lo-fi, not harsh**.

## User requirements (locked in brainstorming)

- "Bin quantization spectral" effect with a **soft, lo-fi vibe — not harsh**.
- **Expression pedal controls the number of bins.**
- **100% wet, no mix knob.**
- **Lowest possible latency.**
- Lives in the **FILTERING menu** ("the menu with the formant filters") — the
  empty slot 2 between AUTOMOOG and FORMANT.
- "Pretty simple" and "well documented."

## Decisions

| Choice | Value | Why |
|---|---|---|
| DSP | Keep loudest N bins | The requested "number of bins" semantic |
| Mix | 100% wet, hard-assigned | No mix knob requested |
| Pedal sweep | heel = all bins → toe = ~2 bins | "Push for effect" |
| FFT size | 256 (hop 128, win 256, 2× overlap) | Lowest-latency size that still resolves pitch; hop must be ≥ ksmps (128) |
| Menu / slot | FILTERING s2 | The "formant" menu's empty slot |
| Toggle id | 38 | Next genuinely free id (37 is NAM's lastToggle) |
| Toggle CC | 76 | FILTERING slot-2 toggle |
| Expr pedal | CC29 → `gkBinPedal`, dispatched by `gkLastToggle29 == 38` | House convention |
| Instr | 121 | Sequenced next to FORMANT (120) in the spectral cluster |
| Slot encoder (CC26) | unmapped | Kept simple |

## Architecture

New file `csound/effects/bincrush.orc` — one bus instr `121` plus a `BinKeep`
UDO. Reads `gaBinSendL/R`, writes `gaBinWetL/R`. Per channel:

```
pvsanal (256/128/256, Hann) → pvs2tab (amp,freq arrays)
  → BinKeep (adaptive-threshold keep-loudest-N, soft+smoothed)
  → tab2pvs → pvsmooth → pvsynth
  → balance vs dry send → dcblock → limit ±0.99
```

### Keeping loudest N without a per-frame sort
`BinKeep` tracks a magnitude **threshold** that drifts up/down (×1.10 / ×0.91
per frame) so that ~N bins survive. N comes from an exponential pedal map
(`N = 2 · (129/2)^(1−pedal)`), so most pedal travel lives in the sparse region.
State persists per call site, so L and R keep independent thresholds/masks.

### Soft / lo-fi (anti-"birdie") measures
The classic harshness of bin reduction is bins flickering on/off frame to frame.
Three defences, all baked in as tuned constants:
1. **Soft knee** around the threshold (bins fade 0→1 across `0.5·thr … thr`).
2. **Persistent per-bin mask** eased toward target by 0.30/frame.
3. **`pvsmooth` 0.3/0.3** on the resynth (gentle amp + freq smoothing).

### Latency notes
- FFT 256 ⇒ ~5.3 ms analysis window. hop = ksmps = 128 (hop < ksmps is the
  pvsanal crash mode). No reinit anywhere ⇒ no reinit clicks on pedal moves.
- **`dcblock`, not `dcblock2`** — dcblock2 is a linear-phase FIR with a real
  254-sample (~5.3 ms) delay; pointless in a latency-critical effect.
- `balance` vs the dry send keeps perceived level steady as bins drop out.

## Files touched (the standard 6-file effect path + spec)

1. `csound/effects/bincrush.orc` — NEW (instr 121 + BinKeep UDO)
2. `csound/globals.orc` — `giBin*` FFT consts, `gkBinBypass`/`gkBinPedal`, 4 buses
3. `csound/signal_chain.orc` — block 11.5: send copy, `$FX_BYPASS`, hard 100%-wet assign
4. `csound/midi_handler.orc` — CC76/menu-3 toggle (id 38), CC29 dispatch, OSC feedback
5. `csound/neptrPhase4.csd` — `#include "effects/bincrush.orc"`
6. `neptrPhase4_UI.py` — FILTERING s2 = `("bin","BINCRUSH")`, `EFFECT_PARAMS["bin"]`
   + `scripts/osc_smoketest.py` mirror

OSC: `/bin_state` (toggle), `/bin_pedal` (pedal 0..1). UI param label "Crush".

## Verification

- `csound --syntax-check-only neptrPhase4.csd` → "Syntax check completed. 0 errors."
- Python UI + smoketest parse; FILTERING s2 and `bin` params assert-checked.
- **Bench rendered + analyzed** (`scripts/bench/bincrush_bench.csd` +
  `analyze_bincrush.py`, 5 pedal scenarios via `--omacro:SCN=0..4`):

  | pedal | bins kept | RMS | flicker |
  |---|---|---|---|
  | 0.00 | 103 | 0.171 | 0.62 |
  | 0.25 | 57 | 0.171 | 0.69 |
  | 0.50 | 32 | 0.171 | 0.64 |
  | 0.75 | 12 | 0.170 | 0.55 |
  | 1.00 | 7  | 0.167 | 0.49 |

  Bin count drops monotonically 103→7; level held constant (balance working);
  flicker falls toward the crushed end (smoothing stable, no birdie runaway).
  *Caveat:* absolute flicker has a noise floor from overlap-2 analysis ripple on
  a steady test tone — final softness is an on-Pi by-ear call.
- **Bug found by the bench** (NOT by --syntax-check, which false-passed it):
  `kr` is a Csound reserved word (control rate); the knee-ratio var was renamed
  `kRat`. Lesson: render, don't just syntax-check.
- **Pending:** on-Pi deploy (`sync_to_pi.sh` + `redem`) and by-ear pass.

## Tunables (by ear / bench)
- Threshold step (1.10 / 0.91), mask ease (0.30), soft-knee width (0.5),
  pvsmooth (0.3/0.3), exp pedal map min bins (2).

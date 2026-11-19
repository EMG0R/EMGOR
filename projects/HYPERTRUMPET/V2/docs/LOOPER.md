# Looper (pads 13 and 14): behaviour and sources

Code: `HYPER_CORE/src/dsp/fx/fx_looper.h` (4 mono layers x 12 s in SDRAM, 9.2 MB, no cost while the effect is off), driven from `bringup/fw/fx_pads.h` (pads 13 and 14 are unchanged on the FX page in 1.12; on the harmony page the pads mean intervals and the looper keeps playing). The looper is not in the admission costs beyond 2.5 % peak and is never shed before the others (it is shed like any non-fader effect if the runtime guard fires). Host test: `bringup/fw/tests/looper_test.cpp` (13 checks, all pass).
Paths below are relative to `~/Documents/_EMGOR_SYNTH/______2026NEW/`. NEPTR = `DEMIURGE_OS/neptr/` (read only), v1 = `HYPERTRUMPET_V2/docs/`.

## Sources used
- **v1 HYPERTRUMPET** (the pad gestures): `v1/V1_SCRAPE.md` 221-227 (RNBO 12-layer looper, pad 13 and 14 gestures, inferred from the netlist), `v1/FX_MAP.md` 37 and 89 (pad table, "hold > ~0.55 s (counter > 10 at 50 ms)", marked UNCERTAIN, never run).
- **NEPTR looper** (the engine behaviour): spec `NEPTR/docs/superpowers/specs/2026-06-22-looper-pedal-design.md`; implementation `NEPTR/csound/effects/looper.orc`, `NEPTR/csound/handler/handler_looper.orc`; reference model `NEPTR/looper_ref/model.py` (only the power-of-two quantizer).
- **PMOR looper**: PMOR has none of its own (`Pocket-OpGorator/PMOR-HANDOFF.md` 374-380 is the NEPTR looper's grid-true fix pushed through the clock; the Daisy example `DaisyExamples/pod/Looper/README.md` 18 only says "Press to record/SOS. Hold to reset"). Nothing to copy beyond "hold = reset", which v1 already has.

## State table (one loop, pads 13 = record, 14 = play)
| State | Pad 13 tap | Pad 13 hold (500 ms) | Pad 14 tap | Pad 14 hold (500 ms) | Source |
|---|---|---|---|---|---|
| Empty | start recording layer 1 | nothing | nothing | nothing | v1 FX_MAP 37; NEPTR spec 51 |
| Recording (first pass) | close: scan back for the last sound, set the loop length there, play | cancel and wipe the take | - | - | NEPTR spec 54 and 62; `looper.orc` 24-38 (trim) |
| Playing | start an overdub (next layer) | delete all | stop | stop | v1 FX_MAP 37; NEPTR spec 55 |
| Overdubbing | punch out (the pass also ends by itself after exactly one loop) | cancel THIS pass only, loop keeps playing | stop | stop | v1 `fx_looper.h` header; NEPTR spec 56 and 65-66 (cancel session) |
| Stopped | nothing (press 14 first; NEPTR starts an overdub from stopped, not done here) | delete all | play (from the top) | stop | NEPTR spec 57 (differs) |

The only departures from the sources: the NEPTR double-tap clear is NOT used (v1 pad 13 uses hold; NEPTR's own overdub is "hold on the same pad", which would collide with v1's hold = delete all); the overdub pass is the v1 layer model (below), not NEPTR's in-place summing.

## Rules and where they come from
| Rule | Value | Source |
|---|---|---|
| First loop defines the loop length (the master) | first close sets it | NEPTR spec 104-108; `handler_looper.orc` 31 ("First loop closed anywhere = master transport") |
| Trailing-silence trim on the first close | scan back from the end to the last sample above -60 dB (0.001), cut there, keep the raw take if all silent; the scan is bounded to 16 x block samples per block (a 0.3 s tail takes about 20 ms) | `looper.orc` 14-16, 32-38 (NEPTR does 8 x block); `handler_looper.orc` 54-58 |
| Length quantization to the clock / grid | NOT applicable: v1 has no clock, the pads do not have a master Hz. NEPTR snaps to whole beats only when a MIDI clock exists, and channels 2..5 to power-of-two ratios of channel 1: there is one channel here | `looper.orc` 24-28; `handler_looper.orc` 34-45 and 66-100 |
| All layers share the loop length | an overdub pass lasts exactly one loop and punches out by itself (10 ms fades) | v1 `V1_SCRAPE.md` 222 (`@syncmode ms`, `@loop 1`); `fx_looper.h` header |
| Overdub feedback / decay | none: layers sum at unity, the old content is never attenuated (NEPTR: `tablew in + out` at write position, i.e. 100 % feedback) | `looper.orc` 286; v1 `V1_SCRAPE.md` 154 (loop sum x volume, unity) |
| Dry level while layers accumulate | `1 - layers/4` (v1: `1 - n/12`), switch DRYDUCK | v1 `V1_SCRAPE.md` 154, `FX_MAP.md` 37 |
| Playback fades | 10 ms in/out at punch points, first-layer end fade-out | v1 `V1_SCRAPE.md` 154 (`fade~` 10 ms) |
| Transpose / reverse | TRANSP -12..12 st, REVERSE; varispeed on the playback head only | v1 `V1_SCRAPE.md` 224; NEPTR spec 78-95 (pure varispeed) |
| Undo | v1 has `LOOPdeleteLAST` but it is unconnected; NEPTR spec designs "cancel this overdub session" (not in its Csound). Implemented here as: hold on pad 13 during an overdub pass cancels that pass | `V1_SCRAPE.md` 187; NEPTR spec 56 and 140-142 (snapshot buffers) |
| Clear | pad 13 hold when not overdubbing = delete all, with a fade (lazy chunked memory clear, never a long memset in the audio thread) | v1 `V1_SCRAPE.md` 226 |
| Hold time | 500 ms here (v1 `counter > 10` at 50 ms = 0.5 to 0.55 s; NEPTR hold 0.4 s) | `FX_MAP.md` 37; `handler_looper.orc` 537 |
| Layers x length | 4 x 12 s mono (v1: 12 x 80 s stereo, 368 MB, does not fit) | `V1_SCRAPE.md` 313 (budget) |
| Looper switches itself off (zero CPU) 2 s after it is empty; keeps its layers across off/on | pad glue | `fx_pads.h` |

## Not found / not decidable from the sources
- v1 gestures are inferred from the Max netlist and never run (V1_SCRAPE says "Verify in Max before relying on this"): whether a tap during overdub punches out or starts another layer, and the exact hold threshold, are unverified.
- No PMOR looper exists to compare with.
- NEPTR's in-place summing overdub (infinite layers in one buffer) is not reproduced: v1's layer model (separate buffers, 4 here) is kept because the pad gestures are v1's. If you want NEPTR's infinite overdub, it is a one-buffer mode switch in `fx_looper.h`.
- NEPTR's tempo-clock quantize (whole beats) needs a clock source the trumpet does not have.

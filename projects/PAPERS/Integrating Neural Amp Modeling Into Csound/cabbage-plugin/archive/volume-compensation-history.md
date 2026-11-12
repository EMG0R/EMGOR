# Volume compensation — approach history (archived 2026-07-09)

Benchmark: all 25 NEPTR phase 4 models, identical synthetic riff
(110 Hz saw plucks), peak 300 ms RMS of the settled output. Numbers are
population stdev / total spread across the folder.

| # | Approach | stdev | spread | verdict |
|---|----------|-------|--------|---------|
| 1 | 1 kHz sine probe @ -18 dBFS RMS, plain RMS meter, makeup to target (original NEPTR) | (unmeasured; audibly poor) | — | user-rejected: clean vs driven captures land at very different volumes |
| 2 | Guitar-chord multitone probe @ -18, ITU K-weighted meter | 2.60 | 9.9 dB | best static probe, still unsatisfying |
| 3 | Pluck-burst probe + BS.1770 relative gating | 3.29 | 10.7 dB | worse: bursts under-drive compressing amps |
| 4 | #2 + naive RMS live rider (deadband 2.5 dB) | 4.34 | 14.8 dB | unstable: rider chases decay tails, settles differently per amp |
| 5 | #2 + peak-tracking rider v1 | 2.17 | 8.3 dB | best rider variant, slow convergence |
| 6 | peak rider v2 (faster, tighter deadband) | 3.22 | 12.9 dB | oscillates |
| 7 | Official NAM reference DI probe at native -12 dBFS | 6.09 | 18.8 dB | probe level too hot vs program: linear-vs-compressing amps diverge |
| 8 | DI probe scaled to -18 dBFS RMS | 4.46 | 14.6 dB | better than #7, still level/material mismatch |
| 9 | **Dry-referenced live compensation in the opcode** (shipped) | **0.83** | **3.1 dB** | every model lands within ±2 dB of the DRY signal |

## What shipped (#9)

In `NAMProcess` itself (`csound_nam_opcode.cpp`, `NamCompState`):
- The RAW dry input (before drive/pad) is the loudness reference.
- Both dry and wet are metered as ~2.5 s K-weighted (BS.1770) EMAs of the
  mono mix; ratio -> target trim. Ratio-based, so program dynamics move
  both meters together and do NOT move the trim (not a compressor).
- Trim slews at most 2 dB/s, holds inside a 0.75 dB deadband, freezes on
  silence (< -55 dBFS), clamps at +/-18 dB.
- `kDriveDb` (new optional opcode arg) drives the model pre-input while the
  compensator holds the output at dry loudness: +18 dB drive measured
  +0.1 dB loudness change and -4.6 dB crest factor (more saturation).
- Converged trim is remembered per model path; swapping back is instantly
  correct (measured +0.6 dB at swap-back).
- The static load-time probe (#8's DI probe, scaled, K-weighted, gated)
  remains ONLY as the initial makeup seed so the first seconds of an
  unseen model start close; the live loop owns the result.

Verification (2026-07-09, CLI + real AU render host):
- folder: stdev 0.83 / spread 3.1 dB, all 25 within +/-2 dB of dry
- drive sweep +18 dB: loudness +0.1 dB, crest -4.6 dB
- dynamics: -12 dB input step -> -10.2 dB output step (residual is the
  amp's own compression; comp slew is too slow to be the cause)
- swap memory: +0.6 dB error immediately after swap-back

Old load-time probe implementations are recoverable from this file's table
descriptions and the NamLoader::measure_loudness_db history in the NEPTR
tree; the shipped probe is the DI-based one (#8) acting as seed.

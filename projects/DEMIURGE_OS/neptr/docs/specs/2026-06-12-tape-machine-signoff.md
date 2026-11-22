# TAPE — Implementation sign-off

Date: 2026-06-12
Status: **SHIPPED** — deployed to Pi, staged for first power-on with the USB
interface. Acceptance-by-ear pending (the only step that needs hardware).
Spec: `2026-06-11-tape-machine-design.md` (amended §5, adaptive headroom)
Plan: `2026-06-11-tape-machine-plan.md` (all 9 tasks executed)
Reference: `docs/reference/GenerationLoss_MKII_Manual_Chase_Bliss.pdf`

---

## What shipped

Gen Loss MKII-style tape machine replacing DROPOUT wholesale.

- `csound/effects/tape.orc` — inline `TapeMachine` UDO (drive-pedal pattern,
  zero bus latency) + `TapeTransport` / `TapeDropGain` / `TapeCrinkle`.
  Stage order per manual p.44: hysteresis saturate → single varispeed
  transport (wow + flutter + generative snags + expression tape-stop all on
  ONE read head) → morphable model voicing → failure (drops + crinkles,
  decorrelated L/R) → hiss/motor noise.
- Controls: MENU 1 (VOLUME) s4, toggle 21, CC72. Slot encoder = AMOUNT macro
  (saturation + spikes + drift), CC20 = FAIL, CC23 = MODEL morph
  (reel→cassette→broken), CC29 expression = transport (heel play / toe stop /
  release spin-up). 100% wet. Toggle-on defaults 0.4 / 0.25 / 0.35.
- DROPOUT removed everywhere: `dropout.orc` deleted (Mac + Pi), globals/
  buses/handler branches/OSC/UI relabeled to `/tape_*`, `("tape","TAPE")`.
- Future hook preserved: 2^18-sample tape buffers sized for a granular layer
  reading the same transport (spec §9).

## Verified (bench, `scripts/bench/tape_bench.csd` + `analyze_tape.py`)

| Check | Result |
|---|---|
| Volume law, AMOUNT 0→1 | within ±0.22 dB of unity (quartic gain-stage fit) |
| Rest latency, clean settings | 60 samples = **1.25 ms** total through effect |
| Adaptive headroom | 0.67 ms @ AMOUNT 0 → 3.2 ms @ AMOUNT 1, gap never near floor (min 147 @ A=1, 27 @ A=0, floor 8) |
| Tape-stop gesture | dive monotonic, click-free, spin-up settles ≤ ~1.3 s, resumes from NOW (hold not replayed) |
| Generative | 60+ pitch events/30 s @ AMOUNT 1; provably zero events + 0 cent drift @ AMOUNT 0 (3 random-seed runs) |
| Full system | `neptrPhase4.csd` syntax-clean incl. concurrent NAM tree; orch compiled clean on the Pi ("orch now loaded") |

## Process notes / discoveries

- **`dcblock2` is a linear-phase FIR with a real 254-sample (5.3 ms) delay**
  (measured). TAPE uses `dcblock` instead. Drive pedals + habit still use
  dcblock2 — unaudited, candidate latency fix later.
- Transport physics: pitch-down modulation requires read-behind headroom —
  the headroom IS the latency, so it now scales with AMOUNT (the only free
  latency win available without changing the sound).
- Snag "up-spikes" are implemented as speed dips + the catch-up recovery
  (physically how stuck tape behaves) so they can never eat past the write head.
- Bench gotchas for next time: `seed 0` makes renders nondeterministic
  (analyzer skips the first 1.5 s settle window); zsh doesn't word-split
  `$var` in for-loops (a silent stale-render trap); calibration was done at
  the 0.06 rms fallback — refit from a live DRIVELEV journal line if levels
  feel off.
- Cross-session coexistence with the NAM integration: targeted edits only,
  re-read shared files before each edit, toggle 37+ left for NAM. Zero
  collisions; both compile together.

## Outstanding (by-ear, first session with the interface)

1. Rest feel vs bypass · 2. pedal dive/spin-up character (overshoot cap
0.4) · 3. AMOUNT sweep taste (snag depth, drift amount) · 4. FAIL crinkle
loudness, hiss level · 5. volume law at real playing level (refit if needed).
All single-constant tweaks in `tape.orc`, re-verify with bench scn1+scn2.

Safety: pre-cutover tar at `../neptr_pre_tape_2344.tar.gz` — delete after
first approved play.

— implemented, benched, deployed & signed off by Claude (Fable 5), 2026-06-12

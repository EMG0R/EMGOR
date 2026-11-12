# DEMIURGE — Always-on master clock + multi-interface sync (Design B′)

Status: approved 2026-06-22. Implements the "sync layer always runs, clocked,
with or without an interface" goal + multi-interface aggregation at a common
latency floor without word-clock hardware.

## Goal

Make the DEMIURGE sync layer (`sync_layer = on`) **always clocked**, so audio
flows and passthrough/latency are testable **with or without** a physical
interface — and so multiple interfaces can run together on one synchronized
floor (best-effort software sync, no ADAT/word-clock required).

This only affects **sync mode**. In bypass (`sync_layer = off`, the SURF/CAR
default) PipeWire is stopped and the launcher owns ALSA directly — none of this
applies (dormant).

## The model (B′ — first interface is master, aloop is pure fallback)

Driver selection by interface count, by **priority** (mostly automatic) +
**sticky master pinning** (launcher):

| interfaces present | graph driver (clock master) | followers | latency |
|--------------------|------------------------------|-----------|---------|
| **0** | `snd-aloop` (virtual fallback clock) | — | n/a (no real out) |
| **1** | that interface, **native** | — | **zero added, no resampling** (HARD requirement, esp. inputs) |
| **2+** | the **first/primary** interface (sticky), native | the rest resample to it | aligned to a **common floor** (a few samples pad on the primary) |

Rules:
- **Sticky master:** once a real interface is the driver, it STAYS the master;
  plugging in a 2nd box later makes the new box a *follower*, it does not steal
  the clock. Primary = the aggregate "front" device (see `aggregate.rs` ordering:
  smallest channel count first; BT first when present).
- **Master removed:** graph reclocks to a remaining interface, or drops to the
  aloop if none left. A brief audible blip at that hot-swap moment is acceptable.
- **1-interface case is sacred:** native, no resampling, zero added latency.
  Never route a lone interface through the aloop.
- **2+ alignment tradeoff (decided):** equal-floor wins for the multichannel
  rig — pad the primary a few samples to time-align the boxes. (Single interface
  is exempt; it's never padded.)

Honest framing: this is **resampled, drift-corrected sync**, NOT bit-perfect
hardware sample-lock (that needs word clock). For music/performance it's
effectively perfect; it is not for phase-critical measurement.

## How the priority works (the mechanism)

- `snd-aloop` node: `priority.driver = 0`, `priority.session = 0` (LOWEST). So
  any real interface automatically outranks it as graph driver → the 0↔1
  transition is automatic (aloop only drives when it's the only candidate).
- Real interfaces: normal/high driver priority (PipeWire default already > a
  loopback). PipeWire picks ONE real device as driver; others adaptive-resample
  to it. For 2+, ensure the **front/primary** device is the chosen one and stays
  pinned (sticky) — this is the one bit that may need launcher logic; verify on
  the Pi whether PipeWire's default selection is already sticky-enough before
  adding code.

## Components

1. **Aloop fallback clock (foundation).** `config/aloop/`:
   - `90-demiurge-aloop.conf` — WirePlumber rule, **ACP profile** (NOT
     `use-acp=false` — that regressed clocking; ACP clocks the graph just by
     being present, verified live). Pin 48 kHz, `pause-on-idle = false`,
     `priority.driver/session = 0`, `node.dont-reconnect = true`.
   - `demiurge-aloop.modules` → `/etc/modules-load.d/demiurge-aloop.conf`
     (boot-load `snd-aloop`).
   - `demiurge-aloop.modprobe` → `/etc/modprobe.d/demiurge-aloop.conf`
     (`index=10 id=DemiurgeLoop pcm_substreams=2`).
2. **Sticky-master + follower alignment (phase 2).** Whatever minimal launcher /
   WirePlumber priority is needed so the primary interface is the stable master
   and additional interfaces resample to it at a common floor. **Verify the need
   on the Pi before writing speculative Rust.**
3. **Testing system integration.** With the graph always clocked, the testing
   system's `internal`/passthrough "just works" with no interface and no manual
   `modprobe`. The "is signal passing through NAM?" check = passthrough (RMS at
   output), the robust half. Update `testing/latency/` docs to drop the manual
   aloop-load steps and document the always-on assumption.

## Staged implementation (respect the unstable rig)

**Phase 1 (foundation, low risk):** the aloop fallback clock (component 1) +
priority config. Gets the 0-interface and 1-interface cases working (single
interface stays native; aloop catches the 0 case). Deploy + verify the graph
clocks with no interface and that a single interface still drives natively.

**Phase 2 (multi-interface, needs Pi experimentation):** sticky master +
follower resampling + common-floor alignment for 2+ interfaces. Design is above,
but the exact PipeWire priority/sticky mechanism must be **tested on the Pi**
(can't verify the driver-selection/resampling behavior blind). Implement after
Phase 1 is proven and a measurement confirms sync quality.

## Verification plan (uses the testing system — "testing IS the sync layer")

- Graph clocked with 0 interfaces: `pw-top` shows demiurge nodes `QUANT 256
  RATE 48000 Running` (not Dummy/QUANT 0). Proven once already this session.
- Passthrough with 0 interfaces: a sync chain with NAM → inject at entrance →
  RMS present at NAM output = signal passing. The user's stated goal.
- Single interface native: confirm no resampler on the lone-interface path and
  latency unchanged from today.
- 2+ interfaces: confirm followers resample to the primary and outputs are
  floor-aligned; measure sync tightness (the timer-clock jitter is the unknown).

## Safety constraints (from CONTINUATION.md §8 — do not violate)

- The Pi is a SINGLE shared resource, the connection is flaky, the rig reboots
  often. Pi work is **sequential, one change at a time, verify with `pw-top`,
  keep a revert ready.** Do not iterate live config rapidly.
- Never leave snd-aloop config half-applied (fully install or fully revert).
- Do NOT use `use-acp=false` on the aloop (it regressed clocking).
- Preserve the SURF default; bypass mode must stay untouched (this is all
  sync-mode-only).
- Never name a specific interface brand in code/docs (generic "interface").

## Open unknowns (resolve by measuring, not assuming)

- Is PipeWire's default driver selection sticky enough, or does the primary need
  explicit pinning? (Phase 2, test on Pi.)
- Aloop timer-clock jitter → how tight is the resampled multi-interface sync?
  (Measure with the testing system.)
- Exact follower latency-alignment mechanism (PipeWire node.latency padding vs a
  manual delay). (Phase 2, test.)

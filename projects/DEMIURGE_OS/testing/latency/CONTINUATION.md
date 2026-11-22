# DEMIURGE latency system — continuation handoff

Read this top-to-bottom before touching anything. It captures the mission, what
is built, the hard-won physics, where things stand, and the path forward.
Written 2026-06-16; updated 2026-06-22 (Option A is now a deployed design —
see `docs/superpowers/specs/2026-06-22-multi-interface-master-clock-design.md`,
the source of truth, and `config/aloop/`).

---

## 0. Mission (what the user actually wants)

A terminal tool that answers, from the terminal, with evidence, two questions
about the DEMIURGE sync layer **WITH OR WITHOUT an audio interface plugged in**:

1. **Is a signal passing through?** — passthrough OK/FAIL (RMS energy present at
   the stage/exit output). This is the user's primary, concrete goal: *"verify a
   signal is passing through a stage like NAM with no interface."* It is the
   robust half of the system (energy-present, not confidence-gated).
2. **What is the internal latency** — entrance to exit — and, when an interface
   is connected, plus that interface's latency on top.

Hard requirements from the user (do not violate):
- **It must be a real MEASUREMENT, not a hardcoded/computed estimate.** The user
  explicitly rejected the `computed = quantum × hops` fallback as the primary
  answer (keep it only as a last-resort fallback).
- **Works with no interface.** Demiurge is designed to run with no hardware; the
  testing tool must too. This is the central demand — and it is now SATISFIED for
  the passthrough check by the permanent aloop clock (see §1).
- **Never name a specific interface brand** in code/docs/UI. Always say "audio
  interface" / "interface" generically. (Watch for stray `scarlett` references in
  `cli.py INTERFACE_SPECS` and `README.md` — if you see one, rename to generic.)
- The user does NOT want the `shot` command (full mic-input chain). Focus is the
  self-generated **internal**/passthrough measurement.

Phase 2 (AFTER measurement quality is confirmed): surface the number +
passthrough indicator in the Mac `demiurge` companion TUI and the Pi Kivy
face-UI "MONITOR" menu. Both read `state.json`. NOT started.

---

## 1. Decision locked + now being deployed: Option A (self-clocking virtual interface)

The user chose **A**: make demiurge's sync layer **self-clock and run in
real-time even with no interface**, by giving it a permanent virtual-interface
fallback clock (`snd-aloop`). Quote: "the sync layer should be able to … run
audio without any real output … tricking everything into thinking it is an
interface." This is a genuine demiurge capability, not just a test hack.

**This is no longer just a plan — it is the approved design and Phase 1 is being
implemented as PERMANENT INFRASTRUCTURE**, not a manual experiment step. See the
spec (`docs/superpowers/specs/2026-06-22-multi-interface-master-clock-design.md`,
"Design B′") and the deployable config in `config/aloop/`:
- The aloop is a boot-loaded, always-present **fallback clock** pinned to the
  LOWEST driver/session priority, so any real interface automatically outranks
  it. It only drives the graph when it is the only candidate (the 0-interface
  case); a real device takes over the instant it appears (the 0↔1 transition is
  automatic).
- The deployed `90-demiurge-aloop.conf` uses the **default ACP profile**. It
  deliberately does **NOT** set `use-acp = false` — that was tried for an
  abandoned DMA-measurement path and regressed clocking (see §5). **NOTE:** the
  old §3/§5 description below of a `use-acp=false` rule is HISTORICAL — the live
  config is now ACP. The physics in §2 is unchanged.
- **CRITICAL (proven 2026-06-22): loading the module is NOT sufficient to clock
  the graph — the aloop must be LINKED into it.** With nothing linked, the
  idle/blank graph SUSPENDS; the launcher had been falling back to linking
  `demiurge-sink-output` to a suspended HDMI sink, leaving the graph dead. Once
  `demiurge-sink-output` is linked to the aloop playback, the blank graph goes
  fully clocked. The launcher's `aggregate.rs` does this link automatically when
  no real interface is present (being implemented).

Consequence for THIS testing system: with the graph always clocked (aloop loaded
**and** linked), the testing system's `internal`/passthrough check **"just works"
with no interface and no manual `modprobe snd-aloop`**. PROVEN 2026-06-22 with no
interface: `passthrough: OK` plus `internal 5.333 ms [measured]` — a REAL
measurement, not the computed fallback. What remains open is Phase 2
(multi-interface sync) and measuring sync quality (see §5).

---

## 2. THE PHYSICS YOU MUST UNDERSTAND (this is the whole ballgame)

Established by direct experiment on the Pi (`gorpi7@demiurge.local`):

1. **With no audio device, PipeWire SUSPENDS the entire sync-layer graph.**
   `pw-top` shows every demiurge node at `QUANT 0, RATE 0`, driven by an idle
   `Dummy-Driver`. Nothing runs in real-time → there is literally no latency to
   measure, because no audio is being clocked. This is why every early
   measurement attempt returned 0 ms or garbage (scattered 8–200 ms, conf ~0.02).

2. **The demiurge sync-layer nodes add ~ZERO real latency.** They are
   `libpipewire-module-loopback` passthroughs (see
   `config/pipewire/demiurge-virtual.conf`). Every path through them measures
   0 ms even when clocked; `pw-jack jack_lsp -l` reports their port latency as
   `[ 0 0 ]`. The `node.latency = 256/48000` in that conf is sync METADATA, not
   real delay. **The real latency lives ONLY in a device's DMA buffer (~2× the
   quantum).**

   => CONSEQUENCE: "internal sync-layer latency" ≈ 0 + whatever clocks it. The
   meaningful number is the **clock/interface buffer**. This reframes the whole
   feature and is **pending user confirmation** (see §6, open question #2).

3. **`snd-aloop` provides a real clock with no hardware — PROVEN, but only once
   LINKED.** Loading the ALSA loopback kernel module (ACP profile) is NOT enough
   on its own — with nothing linked, the demiurge nodes stay suspended (`QUANT
   0`). The graph only jumps to `R 256, RATE 48000, Running` once
   `demiurge-sink-output` is **linked** to the aloop playback. Proven live
   2026-06-22: relinking the sink output from a suspended fallback to the aloop
   clocked the whole graph and yielded a real measurement (`passthrough: OK`,
   `internal 5.333 ms [measured]`, no interface). This is the mechanism that makes
   Option A possible; `aggregate.rs` performs the link automatically when no real
   interface is present (being implemented).

4. **Independent free-running clock domains cannot be cross-correlated.** Before
   a shared clock exists, demiurge-source and demiurge-sink async-resample
   relative to each other and to the injector; a sweep AND white noise both
   decorrelate (conf ~0.02). A SHORT (≤1 s) windowed matched filter is required
   regardless (already implemented in `engine.measure_latency`), but it only
   helps once a single clock exists.

---

## 3. What is built and solid (don't rebuild)

Directory: `testing/latency/` (synced to Pi at `~/testing/latency/`).

| file | status | notes |
|------|--------|-------|
| `engine.py` | SOLID, 25 tests | sweep gen, **short windowed matched-filter** `measure_latency` (drift-immune), `analyze_shot` (passthrough/gain/freq-response/glitches), `measure_latency_series`, `aggregate_latency`. Passthrough = RMS-present (NOT confidence-gated). |
| `cli.py` | SOLID | `selftest`, `analyze`, `gensweep`, `calib` (offset resolve: calibrated→spec), `compute_internal_ms`/`choose_internal` (the COMPUTED fallback the user dislikes — keep as last resort but it must not be the primary answer). **Has `scarlett` in INTERFACE_SPECS — rename to generic.** |
| `inject_sweep.csd` | SOLID | self-generating **continuous triangle** exp-sweep (no discontinuity → clean glitch fingerprint; no `diskin2` which segfaulted on the Pi). |
| `latency.sh` | PARTIAL | `internal` (default), `shot`, `calibrate`, `table`, `selftest`. `internal` currently does entrance→exit routing + falls back to computed when not clocked. Needs the snd-aloop clock wired in (§5). |
| `test_engine.py`, `test_cli.py` | SOLID | ground-truth + drift + robustness tests. Run: `python3 -m unittest test_engine test_cli`. |
| `state.json` | working | atomic publish, the Phase-2 UI hand-off point. |
| `README.md` | OK but update | reflects computed model; update once measurement lands. Remove brand names. |

Config foundation for Option A (now the deployable Phase-1 artifact): `config/aloop/`
- `demiurge-aloop.modules`  → `/etc/modules-load.d/demiurge-aloop.conf` (boot-load snd-aloop)
- `demiurge-aloop.modprobe` → `/etc/modprobe.d/demiurge-aloop.conf` (`index=10 id=DemiurgeLoop pcm_substreams=2`)
- `90-demiurge-aloop.conf`  → wireplumber drop-in. **Now uses the DEFAULT ACP
  profile** and pins `priority.driver/session = 0` so a real interface always
  outranks it. It does NOT set `use-acp=false` — that earlier raw-PCM variant
  regressed clocking and is abandoned (see §5). NOTE: ACP + loading is still not
  sufficient — `demiurge-sink-output` must be LINKED to the aloop for the graph to
  clock (the `aggregate.rs` auto-link, being implemented).

The intent (per the 2026-06-22 spec) is that these become **boot-persistent
permanent infrastructure**, so the sync layer is always clocked — no manual
`modprobe`. Verify the actual install state on the Pi before assuming; older
notes below (§4/§5) describe the pre-deployment, manual-modprobe situation and
are kept as history.

---

## 4. Pi state (leave it healthy)

- Patch: `CAR_MODE` (sync_layer = on, blank chain, low power). This is the
  user's chosen test patch — "blank output but audio passes through the sync
  layer." Leave it here.
- `snd-aloop` (card 10, `DemiurgeLoop`), ACP profile (shows "Loopback Analog
  Stereo"). Per the 2026-06-22 design this is the **permanent fallback clock**
  pinned to lowest priority — harmless to a real interface, never in its path.
- The deployable wireplumber rule is `config/aloop/90-demiurge-aloop.conf` (ACP
  profile, lowest priority). The earlier `use-acp=false` variant was reverted as
  a clocking regression and is abandoned. Verify the current install/boot state
  on the Pi before assuming (the spec's intent is boot-persistent). Keep the
  system healthy: demiurge active, sync-layer sink (`DEMIURGE:monitor_FL`)
  present.
- Pi has NO USB/physical interface attached. Only HDMI (suspended). With the
  aloop fallback **loaded AND linked** (`demiurge-sink-output → aloop`; the
  `aggregate.rs` auto-link does this, or relink by hand until it ships) the graph
  clocks (R 256 RATE 48000 Running) and passthrough is testable — that is the
  whole point of Option A. Note: if the sink output is left on a suspended HDMI
  fallback the graph stays dead even with the module loaded.
- Pi deps installed: `python3-numpy/scipy/soundfile`, `csound`, `jack_rec`
  (no `jack_capture`). `sudo` is NOPASSWD.

---

## 5. WHERE THINGS STAND + the remaining open work

**The original §5 open problem (clock the graph with no interface) is SOLVED and
PROVEN:** the permanent aloop fallback clock (Phase 1, ACP profile), **loaded AND
linked** to `demiurge-sink-output`, keeps the sync-layer graph running with no
hardware. Proven 2026-06-22 with no interface: `passthrough: OK` plus a REAL
`internal 5.333 ms [measured]` (not computed). The one gotcha: loading the module
is not enough — the sink output must be linked to the aloop (the `aggregate.rs`
auto-link, being implemented, does this when no real interface is present; the
old fallback linked to a suspended HDMI sink and the graph stayed dead). So the
testing system's `internal`/passthrough check works with no interface and no
manual `modprobe` — *"is a signal passing through stage X?"* is answerable today
via the RMS-present passthrough indicator (`passthrough: OK/FAIL`). That is the
robust half and the user's stated goal.

### What is actually still open
1. **Phase 2 — multi-interface sync** (the new frontier, per the 2026-06-22
   spec): when 2+ interfaces are present, the first/primary is the sticky clock
   master and the rest resample to it at a common latency floor. The exact
   PipeWire priority/sticky-master mechanism must be **tested on the Pi** — it
   can't be verified blind. Implement after Phase 1 is proven.
2. **Measuring sync QUALITY** — the aloop is a timer-driven clock; how tight is
   the resampled multi-interface sync, and is the single-interface path still
   native/zero-added-latency? This testing system is the instrument ("testing IS
   the sync layer"): use the passthrough check + the short windowed matched
   filter to quantify it once Phase 2 exists.
3. **Absolute internal-latency cross-correlation** is the finicky half. Once the
   graph is in ONE clock domain (a real interface, or a full-duplex aloop loop),
   `engine.measure_latency` (short windowed matched filter) locks and reports
   `[measured]`; with independent free-running domains it cannot (see §2.4). This
   is a nice-to-have, NOT the user's primary goal — passthrough is.

### HISTORICAL — the DMA-loopback measurement path (abandoned, kept for context)
The pre-deployment attempt was to measure absolute latency through the aloop's
own DMA boundary. It is recorded here so nobody re-walks it; it is NOT the path
forward (the permanent ACP fallback clock replaced the need).

The conflict that blocked it:
- **ACP profile**: clocks the demiurge graph (good — and is what we now ship) BUT
  exposes the aloop as one stereo sink + source on device 0 — it HIDES the
  internal dev0↔dev1 loop, so you cannot capture the looped-back signal.
- **`use-acp=false` (raw PCMs)**: exposes both devices — `Loopback:playback_N`
  (dev0) loops to `Loopback (Loopback PCM):capture_N` (dev1) — BUT raw-PCM nodes
  suspend until actively driven, `pw-link` into them failed, AND it lost the
  auto-clocking ACP gave. Self-inflicted regression; reverted and abandoned.

Verified ground truth from that work (still true):
- ALSA-direct loopback WORKS: `speaker-test -D hw:10,0` + `arecord -D hw:10,1`
  captured real signal (rms 0.40). The kernel loop is fine.
- aloop raw-PCM node map (when use-acp=false): id94 `...playback.0.0` (dev0 pb,
  "Loopback"), id93 `...capture.0.0` (dev0 cap), id92 `...playback.1.0` (dev1
  pb, "Loopback (Loopback PCM)"), id91 `...capture.1.0` (dev1 cap). Loop rule:
  dev0 playback_N → dev1 capture_N.

If a future absolute-latency measurement is ever wanted, the cleanest route is
ALSA full-duplex on the aloop (open `hw:10,0` playback + `hw:10,1` capture in ONE
sample-aligned handle, play the reference sweep, cross-correlate the return with
`engine.measure_latency`). It sidesteps the PipeWire routing fight. But remember
§2.2: the sync-layer nodes add ~0, so what that yields is the device-buffer
latency, not a sync-layer delay.

### Product wiring status
- `latency.sh internal` injects at the entrance and captures the exit; with the
  graph clocked by the aloop fallback (loaded **and** linked to
  `demiurge-sink-output`) it confirms passthrough with no interface — proven
  2026-06-22 (`passthrough: OK`, `internal 5.333 ms [measured]`). No manual
  clocking dance required once the `aggregate.rs` auto-link ships.
- `choose_internal` already prefers measured over computed — once a real measured
  number exists (one clock domain), it reports `[measured]` automatically.

---

## 6. Pending questions for the user (get answers before building more)

(Largely overtaken by the 2026-06-22 design, which committed to the permanent
aloop fallback clock and reframed the goal around passthrough. Kept for context.)

1. The **ALSA full-duplex** aloop measurement is now an OPTIONAL nice-to-have,
   not the path (see §5). The primary deliverable is the passthrough check.
2. **CRITICAL framing** (now acknowledged in the design): the **sync layer itself
   adds ~0 ms**; any latency number is really the **interface/clock buffer**
   (~2× quantum, ≈10.7 ms @ quantum 256). The design's "honest framing" section
   accepts this. Still: don't quote an absolute internal-latency number as if the
   sync layer itself contributed it — the user has twice rejected answers built
   on the wrong assumption.

---

## 7. Repro / verification commands (run on the Pi)

```bash
# unit suite (Mac or Pi)
cd ~/testing/latency && python3 -m unittest test_engine test_cli

# is the graph clocked? (the single most useful diagnostic)
timeout 3 pw-top -b 2>/dev/null | grep -iE 'QUANT|demiurge|Loopback|Driver'

# load the virtual interface clock. NOTE: loading ALONE does NOT wake the graph —
# you must also LINK demiurge-sink-output to the aloop (the aggregate.rs auto-link
# does this when no real interface; relink by hand until it ships). With Option A
# deployed (config/aloop/ boot-installed) the module is loaded at boot, so keep
# this only for a manual/diagnostic poke or a box where the fallback isn't installed.
sudo modprobe snd-aloop index=10 id=DemiurgeLoop pcm_substreams=2

# confirm ALSA loop works
speaker-test -D hw:10,0 -c2 -t sine -f440 -l1 & arecord -D hw:10,1 -f S16_LE -c2 -r48000 -d2 /tmp/x.wav; \
  python3 -c "import soundfile as sf,numpy as np;d,_=sf.read('/tmp/x.wav');print((d**2).mean()**.5)"

# sync layer ports (CAR_MODE / sync_layer=on must be active)
pw-jack jack_lsp | grep -iE 'DEMIURGE'
```

---

## 8. Mistakes already made — do NOT repeat

- Don't measure with a long full-length cross-correlation; clock drift kills it.
  Use the short windowed matched filter (done).
- Don't trust a single `pw-top -n 1` snapshot (misses cycles). Sample for ~3 s.
- Don't assume a sweep is robust through async resampling — it is NOT (frequency
  is a function of time; any time-warp shifts it). Only valid once one clock.
- Don't iterate live config changes rapidly on the running rig — a `use-acp`
  change caused a clocking regression. Change one thing, verify with `pw-top`,
  keep a revert ready, and confirm the system stays healthy (demiurge active,
  sync-layer sink present, patch unchanged).
- Don't leave snd-aloop config half-applied. Either fully install (boot) or
  fully revert.
- Don't reintroduce a `shot`-centric framing or name a specific interface brand.

---

## 9. Related context

- Memory: `project_latency_test_system` (index in `MEMORY.md`).
- Sync-layer construction: `config/pipewire/demiurge-virtual.conf`.
- WirePlumber policy + format examples: `config/wireplumber/50-demiurge.conf`.
- Latency ladder reference (real device): `incidents/2026-05-14`,
  memory `project_lowlatency_mode` (bypass ≈ 2× quantum, synclayer ≈ 5×).
- Launcher (interface routing logic, Rust): `src/demiurge-launcher-rs/`
  (`aggregate.rs`, `graph.rs`) — where a permanent snd-aloop fallback clock
  would ideally be wired for the cleanest Option-A integration.

# RNBO ↔ Demiurge Integration — Design Spec

**Date:** 2026-06-05
**Status:** Approved approach (A, spike-first); pending user review of spec
**Owner:** emgor

## Goal

Run Cycling '74 RNBO patches as a first-class stage in the Demiurge patching system, driven by `live.conf`, while preserving the native Max **push-over-network** workflow (edit in Max → export to the Pi → load live). The RNBO web panel is replaced by Demiurge's `live.conf` as the user-facing control surface. Must fit Demiurge's PipeWire audio model and integrate with the sync layer.

## Key facts (verified 2026-06-05)

- **Target Pi:** `gorpi7@demiurge.local` — Debian 13 **Trixie**, **aarch64**, Pi 5 (kernel 6.18, bcm2712). (`INSTRUCTIONS.md` incorrectly says Bookworm — fixed in §8.)
- **Packaging:** C74 now ships a **64-bit Trixie** RNBO Pi image, so a **Trixie/arm64 apt repo with a prebuilt `rnbooscquery`** exists. **No from-source build required.** RNBO is at 1.4.4. Image manifest: `https://assets.cdn.cycling74.com/rnbo/pi-images/repo.json`.
- **Audio model:** Demiurge runs **no `jackd`** — PipeWire owns ALSA and provides JACK via the shim at `/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack/libjack.so.0`, forced via `LD_LIBRARY_PATH`. `jackd` (jackd2) IS installed on the Pi (pulled in for Faust) → the libjack collision (`INSTRUCTIONS.md` §5/§12 F1) is live and must be respected.
- **Discovery:** `avahi-daemon` is **active** → Max network discovery prerequisite already met.
- **Runner cooperation flags:** runner config supports `instance_auto_connect_audio = false` and `instance_auto_connect_midi = false`, and can connect to an existing JACK server rather than spawning its own — this is what lets it behave as a PipeWire-jack client that Demiurge wires by hand.

## Architecture (Approach A)

Install the prebuilt arm64 `rnbooscquery` from C74's Trixie repo, but **never enable its stock systemd service** and **never let it spawn `jackd`**. Demiurge launches and supervises the runner as a **persistent pipewire-jack client**:

- Launched through Demiurge's env shim (`LD_LIBRARY_PATH=…/pipewire-0.3/jack`, `PIPEWIRE_PROPS="node.autoconnect=false node.dont-reconnect=true"`).
- Runner config: `instance_auto_connect_audio=false`, `instance_auto_connect_midi=false` — it auto-wires nothing.
- A new `rnbo` stage type in the launcher registers the runner's JACK ports into the chain like any other stage, and translates `live.conf` directives into runner OSC (UDP :1234): patch-slot load + parameter values.
- Max discovers the Pi via Avahi and pushes/loads patches live — **unchanged**.

### Components and boundaries

1. **Runner daemon** (`rnbooscquery`, installed from apt). *Does:* hosts/compiles RNBO patches, exposes OSCQuery, presents JACK ports. *Depends on:* pipewire-jack (via shim), Avahi. *Interface:* OSC :1234, JACK ports.
2. **`demiurge-run-rnbo` wrapper** (`src/wrappers/`, new). *Does:* launches the runner with shim env + correct config path; idempotent. *Mirrors:* `demiurge-run-cpp`.
3. **`rnbo` launcher stage type** (`supervisor.rs` dispatch, new). *Does:* ensures the runner daemon is up; treats a named patch as a slot load via OSC; registers its JACK ports for chain wiring. *Does NOT:* compile-per-file (unlike `cpp`/`faust`). *Boundary:* the existing `cpp` single-file compile rule is untouched.
4. **`live.conf` schema additions** (§3). *Does:* user-facing control surface replacing the web panel.

## §3 `live.conf` schema (decision: locked defaults)

Chain entry referencing a runner patch slot:

```
chain =
  ~/demiurge/examples/hello_patching/chuck_synth.ck
  rnbo: my_patch          # load patch "my_patch" into a runner slot, wired in series
  ~/demiurge/examples/hello_patching/sc_stutter.scd
```

Optional parameter lines, emitted by the launcher as OSC to :1234:

```
rnbo.my_patch.cutoff = 0.4
rnbo.my_patch.feedback = 0.7
```

Final syntax to be reconciled with the launcher's existing chain/midi block parser during implementation; the `rnbo:` prefix form is the default unless the parser favors another shape.

## §4 Audio routing — both setups

- **PipeWire sync setup (primary):** runner is a PipeWire node; launcher wires its ports into the graph like any stage. Natural fit.
- **NEPTR low-latency bypass (csound direct-ALSA, no PipeWire graph):** decision — the `rnbo` stage **requires PipeWire present**; if a chain in bypass mode references an `rnbo` stage, the launcher emits a clear error rather than silently failing. (Rationale: the runner needs a JACK/PipeWire graph to join; reimplementing direct-ALSA hosting is out of scope.)

## §5 Parameter / patch control replaces the web UI

Launcher → runner OSC on UDP :1234:
- Patch swap: `/rnbo/inst/0/control/load <slotname>` (exact address verified against the installed runner version during the spike).
- Parameter set: per-parameter OSC address derived from the patch's named parameters.

`rnbo-runner-panel` (web UI) is **not installed**. `live.conf` is the control surface.

## §6 Phase plan

### Phase 0 — Spike (GATE; no Demiurge changes)
1. Add C74 Trixie/arm64 apt repo (pubkey + sources list); `apt install rnbooscquery`; `apt-mark hold`.
2. **Do not** enable `rnbooscquery.service`; mask it. Write a runner config with auto-connect off.
3. Launch the runner by hand under the shim env; confirm it appears as a PipeWire node (`pw-link -o`/`-i`) and spawns no `jackd`.
4. Hand-wire it into a simple graph; confirm audio passes.
5. From Max on the Mac: confirm the device appears (Avahi), push a patch, confirm live load + audio.

**Exit gate:** all of 1–5 green. If 1–3 fail (package won't install/run as client), fall back to from-source build; if Max push (5) can't be made to work, fall back to the C++-export + rsync path (lean, loses live push) — a separate spec.

### Phase 1 — Launcher integration
- Add `demiurge-run-rnbo` wrapper.
- Add `rnbo` stage type to `supervisor.rs` (daemon ensure + OSC slot load + JACK port registration).
- Parse `rnbo:` chain entries + `rnbo.<patch>.<param>` lines in `live.conf`.

### Phase 2 — Routing + control
- Wire runner ports into the chain in the PipeWire setup.
- Implement OSC param/patch control from `live.conf`.
- Bypass-mode guard (§4).

### Phase 3 — Docs + setup script (only after Phases 0–2 verified)
- Document the `rnbo` stage + runner setup in `demiurge/docs/`.
- Add an idempotent RNBO block to `setup-script/setup-demiurge.sh` (repo add, install, mask service, config).
- Fix the Bookworm→Trixie error in `INSTRUCTIONS.md` (line ~36 and related).

## §7 Reversibility catalogue (change ↔ undo)

| Change | Undo |
|---|---|
| Add C74 apt repo + pubkey | Delete `/etc/apt/sources.list.d/cycling74.list` + keyring file |
| `apt install rnbooscquery` (held) | `apt-mark unhold && apt remove rnbooscquery` |
| Mask `rnbooscquery.service` (never enabled) | `systemctl unmask` (it was never on) |
| Runner config file (new path) | Delete file |
| `demiurge-run-rnbo` wrapper (new) | Delete file |
| `rnbo` stage type in `supervisor.rs` (additive dispatch) | Revert the additive branch; existing `cpp`/`faust`/etc. paths untouched |
| `live.conf` `rnbo:` lines | Delete the lines (user text) |
| `setup-demiurge.sh` RNBO block | Remove the block (idempotent, guarded) |
| `INSTRUCTIONS.md` OS fix | Doc-only; revert text |

No existing Demiurge file's runtime behavior changes except the **additive** launcher dispatch. `avahi-daemon` is already active (no change). The system already has `jackd`/`libjack-jackd2` (no new collision introduced).

## §8 Risks / open items

- **R1 (low, was high):** prebuilt arm64 Trixie package installs cleanly on self-installed Pi OS Lite Trixie. Validated in Phase 0 step 1.
- **R2 (medium):** runner can be coerced to pure pipewire-jack client with no `jackd` spawn. Validated Phase 0 step 3.
- **R3 (medium):** Max on-Pi compile vs cloud compile works against the installed runner over the network. Validated Phase 0 step 5.
- **R4 (low):** exact OSC addresses for slot-load/params differ by runner version — read live from OSCQuery during the spike.
- **R5 (design):** bypass-mode behavior — resolved as "require PipeWire / error out" (§4).

## Out of scope

- C++-export + rsync path (fallback only; separate spec if needed).
- Containerized 32-bit runner (Approach B, rejected).
- Multi-slot / multi-patch orchestration beyond a single chain stage (future).

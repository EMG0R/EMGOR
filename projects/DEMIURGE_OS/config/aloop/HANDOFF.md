# HANDOFF — Always-on master clock + NAM passthrough (no interface)

Read this cold and you can continue the work. Written 2026-06-22.
Full design: `docs/superpowers/specs/2026-06-22-multi-interface-master-clock-design.md`.

## The goal (what the user wants)

The DEMIURGE sync layer should **run in real-time with NO audio interface
plugged in**, so the testing system can verify **a signal passes through a stage
like NAM** with nothing attached. Mechanism: a permanent virtual master clock
(`snd-aloop`) that clocks the graph when no real device is present — but only
once it is **loaded AND linked** (`demiurge-sink-output → aloop`); a real
interface always takes over when plugged in.

## What is DONE + VERIFIED (Phase 1)

The virtual master clock **infrastructure is installed** and boot-persistent on
the Pi (`gorpi7@demiurge.local`):

- `/etc/modules-load.d/demiurge-aloop.conf` → boot-loads `snd-aloop`.
- `/etc/modprobe.d/demiurge-aloop.conf` → `index=10 id=DemiurgeLoop pcm_substreams=2`.
- `/etc/wireplumber/wireplumber.conf.d/90-demiurge-aloop.conf` → the rule.
  Repo source of all three: `config/aloop/`.

CRITICAL — loading is NOT enough; the aloop must be LINKED:
- Loading `snd-aloop` alone does **NOT** clock the sync-layer graph. With nothing
  linked, an idle/blank graph SUSPENDS. The aloop only drives once
  `demiurge-sink-output` is **linked** to the aloop playback. (The launcher had
  been falling back to linking the sink output to HDMI — a suspended sink — which
  left the graph dead.)
- The auto-link is being implemented in the launcher's `aggregate.rs`: when there
  is no real USB/BT sink, link `demiurge-sink-output → aloop`. This is the
  implemented/critical step that makes the always-on clock actually work.

Key design points (do not regress):
- Uses the loopback's **DEFAULT (ACP) profile**. Do NOT set `api.alsa.use-acp=false`
  — that regressed clocking before (raw PCMs suspend until driven). ACP is
  necessary but still not sufficient: the link above is required regardless.
- aloop pinned `priority.driver=0 priority.session=0` (LOWEST) + `node.dont-reconnect=true`
  + `node.pause-on-idle=false`, 48 kHz. So any real interface (priority ~1009)
  outranks it and becomes driver/default; the aloop only drives when alone.
- This is SYNC-MODE-ONLY. In bypass (`sync_layer=off`, the SURF default) PipeWire
  is stopped and the module is inert — bypass is untouched.

VERIFIED + PROVEN live 2026-06-22 (NO interface, `sync_layer=on`): with
`snd-aloop` loaded but `demiurge-sink-output` left on a suspended fallback the
blank graph stayed suspended; once `demiurge-sink-output` was manually relinked
to the aloop, `pw-top` showed `demiurge-sink/source/sink-output/source-input` all
`R 256 48000 Running`, and the testing system measured **`passthrough: OK`** plus
**`internal 5.333 ms [measured]`** — a REAL measurement, not the computed
fallback, with no interface attached. aloop `priority.driver=0` after wireplumber
loads the rule; survived a wireplumber restart; demiurge active.

## What is NOT done / NEXT ACTIONS

0. **Land the aloop auto-link in `aggregate.rs` (the critical enabler — lead is
   on it).** When no real USB/BT sink is present, the launcher must link
   `demiurge-sink-output → aloop` (instead of falling back to a suspended HDMI
   sink). Without this link the graph stays suspended even with `snd-aloop`
   loaded. This is what made the proven test pass (`passthrough: OK` +
   `internal 5.333 ms [measured]`, no interface). Until this ships, the link must
   be made by hand to clock the graph.

1. **Run the NAM passthrough end-to-end (the actual goal — do this first).**
   Put NAM in a `sync_layer=on` chain and confirm signal passes through it with
   no interface. Recipe:
   - Ensure aloop loaded AND `demiurge-sink-output` LINKED to the aloop (the
     `aggregate.rs` auto-link does this when no real interface; until it ships,
     relink by hand). Loading alone does NOT clock the graph. Confirm:
     `ssh ... 'XDG_RUNTIME_DIR=/run/user/1000 timeout 4 pw-top -b | grep demiurge'`
     → demiurge nodes should be `R 256 48000` (not QUANT 0 / suspended).
   - Use a sync chain containing a `.nam` stage (see `demiurge-run-nam`,
     `testing/nam/`, `examples/nam_stage/`). Either a NAM patch or edit a sandbox
     live.conf (TESTING_GROUND is the auto-resetting sandbox).
   - `cd ~/testing/latency && bash latency.sh internal` → it injects at the
     sync-layer entrance, captures the exit, and prints `passthrough: OK/FAIL`.
     OK = signal is getting through. (Passthrough is RMS-present, robust — it does
     NOT need the latency cross-correlation to lock.) For NAM specifically, wire
     `inject:out1 → NAM:input` and capture `NAM:output` to isolate the stage.
   - The testing-system docs assume this always-on clock now: `testing/latency/README.md`
     + `testing/latency/CONTINUATION.md` (updated 2026-06-22).

2. **Confirm drop-in merge safety (needs a USB interface).** Could not test:
   verify that `90-demiurge-aloop.conf`'s `monitor.alsa.rules` MERGES with
   `config/wireplumber/50-demiurge.conf` (USB device tuning) rather than replacing
   it. Plug a USB interface in sync mode, confirm 50's USB rules still fire
   (period-num, the `__Mic__source` split-parent 48k pin). If they were clobbered,
   fold the aloop rule into 50's array instead of a separate file.

3. **Verify "real interface wins" (needs hardware).** Plug a real interface in
   sync mode; confirm the graph reclocks to it (it becomes driver, aloop idles).

4. **Phase 2 — multi-interface sync (designed, not built).** First/primary
   interface = sticky clock master (native, lowest latency); additional interfaces
   resample to it at a common floor. Single interface must stay native (zero added
   latency — hard requirement). Needs live multi-interface experimentation; see
   the spec. Measure sync tightness with the testing system (aloop timer-clock
   jitter is the unknown).

## Pi access gotchas (this period — important)

- Host `gorpi7@demiurge.local`. mDNS is FLAKY and the rig REBOOTS often. Use plain
  `ssh -o ConnectTimeout=20 gorpi7@demiurge.local '...'`. `sudo` is NOPASSWD.
- Use SHORT atomic commands. Long commands (restart + sleep + verify in one) DROP
  mid-run. Split into: deploy file, then restart, then verify — separate calls.
- Do NOT use retry-loops or ControlMaster (the user rejected those). If a call
  fails with "could not resolve hostname", the rig is mid-reboot — wait for the
  user to say it's back.
- For PipeWire/wireplumber user commands set `export XDG_RUNTIME_DIR=/run/user/1000`.

## Safety constraints

- Pi is a SINGLE shared resource; do Pi work sequentially, verify with `pw-top`,
  keep a revert ready. Never leave snd-aloop config half-applied.
- Preserve the SURF/bypass default unless told otherwise. All this is sync-mode-only.
- NEVER name a specific audio-interface brand in code/docs (generic "interface").
  See memory `feedback_no_interface_brand_names`.
- Current Pi patch state (2026-06-22): `CAR_MODE`, `sync_layer=on` (came up this way
  after a reboot). The user's earlier default was SURF (bypass) — confirm which
  they want before changing.

## Verify-it-works one-liner (no interface, sync mode)

Requires the aloop loaded AND `demiurge-sink-output` linked to it (the
`aggregate.rs` auto-link, or a manual relink until that ships). A FAIL here most
likely means the link is missing, not that the module failed to load.

```
ssh -o ConnectTimeout=20 gorpi7@demiurge.local \
  'XDG_RUNTIME_DIR=/run/user/1000 timeout 4 pw-top -b | grep -E "demiurge-(sink|source)"'
# PASS = lines show "R 256 48000 Running". FAIL = "QUANT 0" / Dummy-Driver (clock not running → check the sink-output→aloop link).
```

# Pocket OpGorator × Demiurge/NEPTR Integration — Design

**Date:** 2026-07-29 · **Status:** approved (user), implementation started same day

Integrates the Pocket OpGorator (custom Daisy-Seed groovebox, project at
`../Pocket-OpGorator/`) into the Demiurge system and NEPTR: bidirectional
clock sync over USB MIDI with negotiable master role, PO-style SY0–SY5 sync
on the OpGorator's 3.5mm jacks (firmware-side), OpGorator audio into NEPTR
over USB as its own parallel chain, and a grid-true looper capture fix across
all NEPTR deployment targets.

## Scope & repos touched

| Repo/dir | What changes |
|---|---|
| `DEMIURGE_OS` (this repo) | clock daemon follower mode + role CC, `demiurge-sync` CLI, reverse MIDI subscription, `opgorator` chain token, WirePlumber driver pin, web UI sync page, Mac TUI sync page |
| `../NEPTR phase4` | `csound/effects/looper.orc` grid-quantized capture (shared by Pi + Cabbage VST/AU/app), tests, VST csd regeneration |
| `../Pocket-OpGorator` | firmware: MIDI clock master/follower + role CC in existing `.ino`; PO sync engine (SY0–SY5); new TinyUSB composite USB stack (separate scaffold) |

Out of scope (explicitly, per user): PO 2-PPQN sync jacks on the NEPTR/Pi
side (future maybe — the OpGorator is the bridge to real POs/Volcas); audio
from NEPTR down to the Daisy (capture-only by design).

## 1. Clock architecture

One tempo authority: the `demiurge-clock` daemon (`src/demiurge-clock.cpp`)
remains the hub — 24 PPQN `0xF8` + ch16 CC119 BPM broadcast on ALSA-seq bus
`14:0`; accepts ch16 CC118 BPM steer and CC117 transport; optional Ableton
Link (`DEMIURGE_LINK`).

**New: follower mode.** The daemon gains a clock-in branch (its input thread
currently hard-drops everything but CONTROLLER events at
`demiurge-clock.cpp:199`):

- Role `follower`: incoming `SND_SEQ_EVENT_CLOCK` (0xF8) ticks are counted
  and PLL-averaged (sliding window ≥ 24 ticks; never per-tick — USB MIDI
  jitter is ±1ms ≈ 5–10% of a tick at 120 BPM). The daemon locks its own
  tick emission to the recovered tempo + phase and keeps re-broadcasting
  0xF8/CC119, so every consumer (looper grid, chase_bliss, csound) follows
  with zero changes. Incoming START/STOP/CONTINUE map to the existing
  transport handling. If ticks stop for > 2 s, freewheel at last tempo.
- Role `master`: current behavior (free-run at `--bpm`, or Link, or CC118
  steer from the looper).

**Role negotiation — ch16 CC116** (next to the existing 117/118/119):

- value `1` = OpGorator is master (daemon → follower)
- value `2` = Demiurge is master (daemon → master; the OpGorator firmware
  switches itself to follow bus 0xF8)
- The side whose menu flips the role sends CC116 on the bus; both sides
  obey. The daemon re-announces current role (CC116) once per ~2 s so a
  late-plugged device converges.
- Default: when the OpGorator is attached, OpGorator master (value 1).

**Persistence:** `~/demiurge/sync.conf` (`role = opgorator|demiurge`),
outside live.conf so `demiurge-set` patch switches don't clobber it
(interface.conf pattern). The daemon reads it at startup; runtime changes
arrive as CC116. New CLI **`demiurge-sync`** (`src/wrappers/demiurge-sync`;
name avoids colliding with the `demiurge-clock` binary):

- bare → human status (role, bpm, tick source alive?)
- `--list` → machine lines for UIs (`role|bpm|source_alive`)
- `opgorator|demiurge` → write sync.conf + inject CC116 live (via
  `aconnect`-visible ALSA-seq, same injection style as the steer path). No
  service restart needed.

**Reverse MIDI subscription:** `demiurge-midi-connect.sh` (written by
`setup-script/setup-demiurge.sh:132-141`) today merges devices → bus
one-way. Add the mirror `aconnect 14:0 "$client":0` so the Daisy hears
0xF8/CC116/CC119. Idempotent (aconnect duplicate = harmless error).

**CC118 mapping note:** firmware must use the daemon's actual 40..400
mapping (`40 + v/127*360`, `demiurge-clock.cpp:213`), not the stale 40..240
in docs/clock.md — fix the doc while in there.

**PO sync (3.5mm) is OpGorator-firmware-only.** SY0–SY5 per the table in
`Pocket-OpGorator/pocket-OpGorator.md` §"PO / Volca Sync". Electrical
contract (researched, sourced): output = unipolar positive ~1V, 2.5–5 ms
click at 2 PPQN (one per 8th note), rendered on the codec's LEFT channel in
master modes; input = rising-edge threshold detect (~0.3 FS) with ≥10 ms
refractory window, tolerant of Volca 5V/15ms pulses. PO sync carries no
transport and no position — pulse interval IS the tempo; slaves step 2
sixteenths per pulse.

## 2. OpGorator firmware

**Phase-now (existing DaisyDuino `.ino`):**

- Clock master: emit USB-MIDI 0xF8 at 24 PPQN from the internal sequencer
  tempo, plus Start/Stop; emit CC116=1 on boot/role-claim.
- Clock follower: on CC116=2, follow incoming bus 0xF8 (PLL-averaged, same
  smoothing rule) and drive the internal sequencer + PO sync out from it.
- PO sync engine in the audio callback: pulse render (master modes) and
  edge-detect (slave modes) per SY mode; SY mode + master-role items added
  to the OLED menu. Any mode other than SY0/SY5 forces mono audio (per doc).

**Phase-C (new TinyUSB composite stack, separate scaffold dir
`Pocket-OpGorator/firmware-tinyusb/`):**

- TinyUSB composite on the micro-USB: **UAC2 capture-only** (Daisy → host)
  + MIDI + CDC. Audio 48 kHz fixed; **24-bit attempted first, 16-bit is the
  tested fallback** (both fit USB full-speed: 288 vs 192 bytes per 1 ms
  frame; 24-bit is where community builds get fragile). Audio block size 48
  samples to match the 1 ms USB frame; drift handled by 47/49-sample flow
  control (community-proven), host side absorbed by PipeWire resampling.
- Capture-only is a hard requirement: a playback interface would make the
  Daisy a second USB sink and silently knock Demiurge out of direct-output
  mode (+1 quantum on the MAIN path — `aggregate.rs:208-213`).
- USB descriptor product string: `Pocket OpGorator` (Demiurge matches on it).
- Un-brickable: STM32 DFU bootloader is mask ROM; BOOT+RESET always
  recovers a bad flash. Worst case = reflash, never a dead board.

## 3. Demiurge side — audio + chain

- **Auto-bridge already works**: `aggregate.rs:144-157` links any
  `alsa_input.usb-*:capture_FL/FR` into `demiurge-source` on hot-plug.
- **New chain token `opgorator`**: a bare `opgorator` token in a `chain =`
  block (extension table `live.rs:299-314` + special-case, web
  `CHAIN_EXT`/parser in lockstep). No process is spawned — like
  rnbo/strudel stages, client-name resolution finds an existing graph node
  (node name containing `OpGorator`, fallback `Daisy`, capture ports).
  Declared as its own parallel chain, default empty:

  ```
  chain = opgorator
  ```

  terminates at `out`; multiple edges into `demiurge-sink` mix for free
  (`live.rs:251-253`), so it adds zero latency to any other chain. Stages
  (including a dedicated `.csd` for FX) can be inserted after the token
  later: `chain = opgorator -> opgorator_fx.csd` — that csound instance
  processes ONLY the OpGorator path.
- **Driver pin**: new WirePlumber rule (modeled on
  `config/aloop/90-demiurge-aloop.conf`) matching the OpGorator/Daisy node,
  `priority.driver = 0`, `priority.session = 0` — it must never be elected
  graph clock master. Rate is 48 kHz fixed in firmware, satisfying
  `enforce_usb_rates`.
- **UIs**: `demiurge-sync` CLI first, then the interface-selection playbook
  (`docs/superpowers/specs/2026-07-26-interface-selection-design.md`)
  across: Mac TUI page (`src/companion/demiurge` — new `sync_page()` +
  keybind, live BPM display can pipe `aseqdump`), web UI
  (`Probes`/`parse_sync`/`POST /api/sync`/tile + SSE + mock fixture +
  parser test), NEPTR pygame UI (on-Pi tab, same env-overridable command
  pattern — Mac copy in `NEPTR phase4/`). No UI edits config directly; all
  shell the CLI.

## 4. Looper — grid-true capture (all deployment targets)

Single source of truth: `NEPTR phase4/csound/effects/looper.orc`, included
by both the Pi deploy and the Cabbage VST/AU/standalone
(`vst/generate_vst.py` include list). One fix, re-export.

Bug: SET_LENGTH captures `gkLOOP_wpos` at the button press, so human press
timing bakes silence into the loop tail (and pre-roll at the head).

Behavioral contract:

- **Clocked** (any master: OpGorator / Ableton / looper-steer): finalized
  loop lengths are exact whole-beat multiples of the master grid.
  - Click-off **early** → recording continues until the next grid line,
    then finalizes: the last audio is never cut.
  - Click-off **late** → length rounds back to the nearest grid line.
  - Loop start is grid-referenced the same way, so playback is phase-locked:
    audio replays at the same clock phase at which it was performed.
  - Channel 1's master-grid role (`gkLOOP_masterLen`) is preserved; when an
    external clock master is present the grid derives from its BPM instead.
- **Unclocked**: current free capture, plus trailing-silence trim —
  threshold scan backwards from the write head; the head/start is left
  untouched ("prioritize audio on the way out over the way in").
- Deferred-finalize introduces a short RECORDING→pending-length state;
  overdub/stop semantics and undo snapshots unchanged.
- Mirror the fixed `looper.orc` to the `NEPTR-phase4-looper` working copy
  if it diverges; regenerate `vst/NEPTR_VST.csd` via `generate_vst.py`
  (Cabbage re-export itself is a manual app step — documented, not
  automated).

## 5. Latency budget (for the record)

- Main NEPTR path: **unchanged by construction** (capture-only device,
  parallel chain, driver-pinned).
- OpGorator audio over USB: USB isochronous framing ~1–2 ms + PipeWire
  quantum (128 @ 48 kHz ≈ 2.7 ms) + resampler ≈ **4–6 ms** to the sink.
  Physical 3.5mm into a spare interface input would be ≈ one quantum less;
  USB wins on cabling/convenience, not latency. Acceptable per user.
- Clock over USB MIDI: ±1 ms per-tick jitter, smoothed by PLL on whichever
  side follows; tempo-stable well under audibility. PO sync out of the
  OpGorator is sample-accurate (rendered in its audio callback).

## 6. Testing

- Clock: synthetic-tick PLL unit test (jittered 0xF8 → recovered BPM within
  ±0.1); CC116 role flip round-trip; freewheel on tick loss.
- Demiurge: direct-output regression (OpGorator attached ⇒ still direct);
  `opgorator` token parse tests; web parser fixture tests (existing mock
  harness pattern).
- Looper: unit tests for quantize math (early/late/exact, multi-beat,
  master-grid ratio) in the existing test style; by-ear grid check on Pi.
- Firmware: enumeration checks on the Pi (`arecord -l`, `aconnect -l`),
  MIDI clock jitter measurement via `aseqdump` timestamps, 30-min USB audio
  soak (count drops/xruns).

## 7. Build order

- **A** — Demiurge: clock follower + CC116 + `demiurge-sync` + reverse
  aconnect + UIs (works against today's shipped libDaisy MIDI firmware).
- **B** — Looper grid-true capture + VST regeneration.
- **C** — Firmware: `.ino` clock/roles/PO-sync now; TinyUSB composite audio
  scaffold in parallel; `opgorator` chain token lands with it.

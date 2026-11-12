# Architecture

## The core constraint that shapes everything

**Only one process can read `/dev/ttyACM0`.** The Teensy streams `NEPTR2 ...`
frames at 100 Hz, and today `serial_bridge.py` owns that port and turns frames
into MIDI CCs for csound. But the secret handshake has to be watchable *at all
times* — including while the synth is running — which means the same process that
detects the handshake must be the one reading serial.

So the arcade supervisor **becomes the single serial owner** and is a behavioural
**superset** of `serial_bridge.py`:

- **Synth mode** → it re-emits the *exact* same CC traffic (same change-only
  discipline, same `NEPTR Serial Bridge` ALSA-seq port). csound needs zero
  changes. We vendored the bridge's reader/parser/emitter into `lib/teensy.py`
  to guarantee parity.
- **Console mode** → it stops emitting MIDI and instead drives a uinput virtual
  gamepad that RetroArch reads as Player 1.

The supervisor *replaces* `serial_bridge.py` at runtime (stop the old one, run
this). It is not a code change to the bridge — it's a swap of who owns the port.

## Data flow

```
Teensy ──serial──> arcade_supervisor (sole owner)
                      │
                      ├── always: ComboDetector(b1 & jb, 1s) ── fires ──> toggle
                      │
              ┌───────┴────────┐
        SYNTH mode         CONSOLE mode
              │                  │
   MidiEmitter (mido)     VirtualGamepad (uinput "Demiurge Arcade P1")
              │                  │
   ALSA-seq "NEPTR          RetroArch reads it as Player 1
    Serial Bridge"
              │                  │
          csound            RetroArch audio ──> PipeWire (sync layer) ──> Scarlett
```

## The handshake (`lib/combo.py`)

`b1` (arcade button) **and** `jb` (joystick click) held ≥1s. Chosen because
gameplay only ever uses the joystick *axes* + the arcade button — the joystick
*click* is otherwise idle, so the gesture can't happen by accident mid-game.

`ComboDetector` is a one-shot edge detector: it returns `True` exactly once when
the hold threshold is crossed, then stays quiet until both buttons release and
re-press. That makes a single sustained press a clean toggle, not a flip-flop.
Pure logic, fully unit-tested (`tests/test_combo.py`).

The detector runs on the raw frame in **both** modes, so the same gesture opens
the console (from synth) and closes it (from console).

## Freeze / thaw — "pause every language exactly as it is"

On console open the supervisor runs `bin/demiurge-freeze`, which **SIGSTOPs only
the chain's DSP programs** (csound/chuck/pd/sclang/faust/node/…) found in the
`demiurge.service` cgroup — pausing every language on the same sample, no
per-language code. Close → `demiurge-thaw` sends `SIGCONT`; audio resumes exactly
where it stopped (generative state intact — a kill/restart would reset it).

**It deliberately does NOT freeze the launcher or the audio infra.** The
`demiurge-launcher` (the unit's MainPID) owns/manages the PipeWire graph —
the `demiurge-sink`/`-source` loopbacks and the scrub/reconcile. Freezing the
*whole* cgroup (the old `systemctl kill --signal=SIGSTOP demiurge.service`
approach) froze the launcher too, which **hung RetroArch's audio init → black
screen** (verified 2026-06-15). So freeze skips the MainPID and any
`pipewire`/`wireplumber`/`pw-*` procs, leaving the bus alive for the game.

`demiurge.service` runs as the **same user** (`gorpi7`), so `kill -STOP`/`-CONT`
on its child PIDs needs **no sudo** — the freeze helpers are unprivileged. (Only
the UI stop/start needs root; see below.) In TESTING_GROUND (empty chain) there
are no stage procs, so freeze is a clean no-op and the game gets a healthy graph.

## Display handoff — the screen (`bin/ui-release` / `ui-restore`)

The face UI **cannot be frozen off the screen.** Kivy is SDL2-KMSDRM and grabs
**DRM master via a direct `/dev/dri/card1` open with no VT/seat** (logind shows
`VTNr=0`, `SEAT=-`). A `SIGSTOP`'d kivy *keeps* that master, so RetroArch's KMS
modeset fails — verified: `[KMS]: Error when switching mode` → `Cannot open video
driver`. A VT switch doesn't help either, because the master isn't VT-bound.

So the UI is **stopped** (not frozen) on console-open and **restarted** on
console-close: `systemctl stop neptr-ui.service` closes its DRM fd → frees the
CRTC → RetroArch becomes master → modeset succeeds (verified in TESTING_GROUND:
`Found GL context "kms"`, `New FB 1280x720`, RetroArch stays up). On exit,
`ui-restore` restarts it and it re-takes the panel.

Trade-off: the UI's exact in-memory frame (the current face) is lost across the
restart — but it's an OSC-driven *read-only mirror*, so menu/effect state is
rebuilt from the engine's OSC. The **audio still truly freezes** (SIGSTOP) and
resumes on the exact sample; only the *display* is stop/restart.

> Upgrade path to a true UI pause: have the Kivy UI cooperatively `drmDropMaster`
> on a signal (e.g. SIGUSR1) and re-acquire on SIGUSR2. Then it could be
> SIGSTOP-paused like the audio and resume on its exact face. Needs UI code.

## Audio: the console as a Demiurge node

The freeze does **not** stop PipeWire — that's a separate service. So with
`sync_layer = on` in `live.conf`, RetroArch (`audio_driver = pipewire`,
`PIPEWIRE_NODE=demiurge-sink`) outputs into the sync layer just like any Demiurge
stage while the synth chain is frozen. **Verified in TESTING_GROUND**: RetroArch
logs `[PipeWire]: Found Sink Node: demiurge-sink` and registers a `RetroArch` node
in the graph. Routing through the sync layer (not grabbing the ALSA device) keeps
the game an *integrated node*, DSP-able later.

> Requires `sync_layer = on`. In `sync_layer = off` (exclusive-ALSA bypass) the
> frozen csound still holds `hw:` open and the game gets no audio — bypass is
> fundamentally incompatible. The **TESTING_GROUND** patch (empty chain,
> `sync_layer = on`) is the clean test bed: PipeWire up, no csound, no Scarlett
> dependency, resets clean on every switch.

## Input mapping (`lib/gamepad.py`)

> Canonical physical button roles for the WHOLE system (b1..b9 → CC60..68 → d-pad
> / face clusters / arcade) live in `docs/BUTTONS.md`. The console wiring below is
> a *consumer* of those buttons and intentionally differs (it uses the analog
> joystick for the d-pad, not b2–b5). Read BUTTONS.md for the source of truth.

- Joystick `jx`/`jy` (12-bit) → digital 4-way via `ABS_HAT0X/Y` (arcade games are
  4-way; thresholded with `joy_deadzone` about center 2048).
- `b1` (ARCADE) → fire / OK, `jb` (joystick click) → reserved for the handshake.
- The current `gamepad.py` emits a virtual KEYBOARD: `b1`/`b9` → A (fire),
  `b1`/`b8` → B, `b4` → Start + Coin (free-play / continue-on-death). (Earlier
  notes describing `BTN_SOUTH`/`BTN_EAST`/`BTN_START` gamepad codes were the older
  uinput-gamepad approach — see `gamepad.py` for the authoritative current wiring.)

## Privilege map

| Need | Why | How |
|---|---|---|
| read `/dev/ttyACM0` | serial frames | `dialout` group |
| write `/dev/uinput` | virtual gamepad | `input` group + udev rule |
| `SIGSTOP`/`SIGCONT` demiurge.service | audio freeze/thaw | NOPASSWD sudoers |
| stop/start neptr-ui.service | release/retake the screen | NOPASSWD sudoers |
| run RetroArch | game + PipeWire session | as the login user (PipeWire is per-user) |

Supervisor runs as the **login user** (so RetroArch shares the user's PipeWire
session); freeze/thaw escalate via the narrow sudoers entries.

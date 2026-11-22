# NEPTR Phase 4 — UI & Companion Mode (master reference)

This is the master reference for the on-device pygame UI
(`neptrPhase4_UI.py`). It supersedes the older, Kivy-era `docs/ui.md`.

> **Naming note.** This "companion" is the on-device dashboard/menu *inside
> the face UI*, reached by holding the joystick. It is **not** the demiurge
> CLI companion (`/opt/homebrew/bin/demiurge`, documented in the DEMIURGE_OS
> tree at `demiurge/docs/companion.md`). Different thing, same word.

---

## 1. What the UI is

A hand-drawn pygame surface (no widget toolkit) rendered portrait on the
1280×720 HDMI panel. It has two faces:

- **Normal UI** — a *read-only* mirror of the Csound engine, driven entirely
  by OSC on `127.0.0.1:8000`: menu views, effect-parameter sliders, and the
  protoFACES sleep screen. Idle CPU is ~zero (the loop blocks on the OSC
  queue and only repaints when visible state changes).
- **Companion mode** — an *interactive* dashboard / preset / power menu the
  player drives with the joystick and arcade button. Reached by holding the
  joystick; leaves on a second hold or after 15 s idle.

The normal UI is unchanged by this feature; companion mode is an overlay
state that takes over rendering while active.

---

## 2. Companion lifecycle

```
        hold joystick 3 s
NORMAL ───────────────────────────────►  COMPANION
  ▲                                          │
  │   hold joystick 3 s again                │
  └──────────────────────────────────────────┘
  ▲                                          │
  │   15 s with no nav / click               │ (drops straight
  └─────────── SLEEP FACES ◄─────────────────┘  into sleep)
```

- **Enter:** hold the **arcade button** (`CC60`) for **1.2 s**, or the
  joystick button (`CC77`) for **3 s**. If the screen was asleep, it wakes.
- **Exit (manual):** hold either button again (arcade 1.2 s / joystick 3 s) →
  back to the normal UI, sleep countdown restarts.
- **Exit (idle):** **15 s** with no joystick move or arcade tap → leave
  companion mode **and** drop into the sleep faces.
- **Inside the menu:** a quick **arcade tap** activates the current selection.
  A held arcade press (≥1.2 s) is a toggle, not a tap — the hold is consumed,
  so it never doubles as a click.

Thresholds live in `companion.CompanionController` (`HOLD_SECS = 3.0`,
`IDLE_SECS = 15.0`).

---

## 3. Architecture & data flow

The UI is a read-only OSC mirror *and* a read-only MIDI listener. **It never
opens the Teensy serial port** — Csound owns that.

```
                 ┌────────────── Teensy 4.1 (USB composite) ──────────────┐
                 │   serial /dev/ttyACM0          USB-MIDI endpoint        │
                 └───────┬────────────────────────────────┬───────────────┘
                         │ (serial frames)                 │ (CC mirror)
                  serial_bridge.py                          │
                         │ scales 12-bit → 7-bit CC         │
                         ▼                                  │
                   ALSA seq 14:0 ◄─────────────────────────┘  (whichever path
                         │   (Csound reads here: audio control)   is wired)
          ┌──────────────┴───────────────┐
          ▼                              ▼
   Csound engine                  neptrPhase4_UI.py
   (owns the audio path)          companion MIDI input (READ-ONLY tap)
          │                              ▲
          └────── OSC 127.0.0.1:8000 ────┘  (normal UI state)
```

Key points:

- **Audio-engine-independent.** The companion reads the **Teensy's own
  hardware USB-MIDI port** (`choose_midi_port()` prefers `Teensy`), not
  `Midi Through 14:0`. The Teensy emits CCs whenever it's plugged in, so sensor
  streaming (and the telemetry the terminal mirrors) works with Csound / the
  demiurge audio engine **on or off**. `Midi Through 14:0` only carries Teensy
  data because the audio launch wires it there — reading it would make
  streaming die when audio is off, so it's only a fallback.
- **The stream is SHARED, not captured.** ALSA-seq fans one source out to every
  subscriber, so Csound and the UI (and a future game-launcher) all read the
  same Teensy events at once. Opening the input is non-exclusive — it never
  steals the device. Verify with `aconnect -l`.
- **Csound keeps full control of the joystick** the whole time. The companion
  only *reads* the bus; it sends nothing back and suspends nothing. So while
  you navigate the menu, the joystick still drives whatever Csound maps it to
  (audio has priority — "Csound needs the controls most"). Minor side effect:
  your tone can shift while you scroll. A future opt-in "freeze sound while
  menu open" would require a signal back to Csound; not built.
- If MIDI is unavailable (no `mido`, no port), the companion is simply
  unreachable and the normal UI runs exactly as before.

---

## 4. Input mapping (Teensy CC → companion)

From `TEENSYY/TEENSYY.ino` (mirrored by `serial_bridge.py`):

| Control            | CC      | Companion use                                  |
|--------------------|---------|------------------------------------------------|
| Joystick button    | `CC77`  | 3 s hold = enter / exit                        |
| Joystick X         | `CC27`  | switch screen tab (left / right)               |
| Joystick Y         | `CC28`  | move selection within a tab (up / down)        |
| Arcade button (b1) | `CC60`  | activate / toggle the current selection        |

**Joystick = D-pad (dominant axis + hysteresis).** Axes are 0..127, centre
≈ 64. A push **≥ `NAV_COMMIT (26)`** from centre registers one step; **only
the axis furthest from centre acts**, so a diagonal wobble never cross-talks
between tab-switch (X) and selection (Y). After a step the stick must come back
**within `NAV_RELEASE (14)`** of centre before it can step again — that
hysteresis gap (commit 26 vs release 14) kills threshold chatter and any
runaway scroll. One deliberate push = one move. Buttons register pressed at
value ≥ 64.

If "up" reads as "down" (panel rotation) flip it with no code change:
`NEPTR_NAV_INVERT_X=1` / `NEPTR_NAV_INVERT_Y=1`.

Dashboard telemetry additionally reads `CC30/36` (LiDAR), `CC31/32/33`
(accel), `CC34/35` (FSR), `CC29` (expression), `CC78/79` (mics).

---

## 5. Screens

Three screens act as tabs; **joystick left/right** switches between them.

1. **DASHBOARD** (home) — dense text readout, terminal-visualizer style, no
   sliders. Top: system stats (`PWR` LOW/MED/HIGH from `scaling_max_freq`,
   `TEMP`, `FREQ`, `LOAD`, `MAXF`, `THR`). Below: every live MIDI channel as
   `label:value` cells (`jstX:70`, `ldr2:42`, `btn1:127`, `enc1`, `eb3`…),
   active values bright, idle dim. Fills the whole screen. Not navigable.
2. **PRESETS** — one row per demiurge preset (the `*.conf` files in the
   patches dir). **Joystick up/down** moves the highlight; **arcade tap**
   activates the selected preset.
3. **POWER** — a portrait list of buttons: **LOW / MEDIUM / HIGH / RESTART**.
   Up/down to move the cursor, arcade tap to choose. The currently-active
   power mode is marked with a ● dot. LOW/MED/HIGH set the mode; RESTART
   bounces the stack.

Selection is shown by inversion (white fill, dark text), matching the normal
menu's ON style. A transient message (e.g. `→ SURF_NEPTR`) confirms an action.

---

## 6. Configuration (env)

The companion never edits `live.conf` or `demiurge-power.service` — it only
*triggers* commands you own, so power-mode ownership stays where it belongs.
Everything is overridable without touching code:

| Env var                   | Default                          | Meaning                                   |
|---------------------------|----------------------------------|-------------------------------------------|
| `NEPTR_MIDI_PORT`         | auto (Through/Bridge/Teensy)     | which MIDI input to read (substring match)|
| `NEPTR_PATCHES_DIR`       | `~/demiurge/patches`             | where preset `*.conf` files are listed    |
| `NEPTR_PRESET_CMD`        | `demiurge patch {name}`          | command run to switch preset (`{name}` sub)|
| `NEPTR_POWER_SET_CMD`     | `demiurge-power {level}`         | set power mode (`{level}` = low/medium/high)|
| `NEPTR_RESTART_CMD`       | `sudo systemctl restart demiurge`| RESTART button (redem)                    |
| `NEPTR_NAV_INVERT_X`      | `0`                              | `1` flips left/right tab nav              |
| `NEPTR_NAV_INVERT_Y`      | `0`                              | `1` flips up/down selection nav           |

The POWER label is derived from `scaling_max_freq` (≤1600 LOW, ≤2400 MED,
else HIGH), matching the demiurge CLI — no status command needed.

If a configured helper does not exist, the action no-ops gracefully and the
dashboard shows `n/a` — the UI never crashes on a missing command.

> **Power ownership.** Per project rule, the system throttle is owned outside
> the UI. The companion's power toggle must call a helper you provide
> (`NEPTR_POWER_TOGGLE_CMD`); it must not be pointed at anything that edits
> `live.conf` / `demiurge-power.service` directly.

---

## 7. Files & modules

| File                     | Responsibility                                              |
|--------------------------|-------------------------------------------------------------|
| `neptrPhase4_UI.py`      | main loop, rendering (normal + companion), wiring           |
| `companion.py`           | `CompanionController` — pure hold/nav/idle state machine    |
| `companion_input.py`     | `InputState` (CC→snapshot) + `read_loop` (mido reader thread)|
| `pi_status.py`           | system metrics + `vcgencmd get_throttled` decoding          |
| `actions.py`             | preset listing, `Activator`, `PowerControl` (injected runner)|
| `tests/`                 | pytest suite (see below)                                    |

The four companion modules are import-light: `mido` is lazy-imported inside
`companion_input.read_loop`, so importing the UI never hard-requires MIDI.

---

## 8. Testing

Pure logic + headless rendering, no hardware required:

```sh
.venv/bin/python -m pytest tests/ -q
```

Covers: the hold/nav/idle state machine, CC→snapshot mapping, throttle
decoding, preset/power dispatch (injected runners — no subprocess), and a
headless end-to-end pass through `NeptrUI` (SDL dummy driver) that renders all
three screens and exercises enter → navigate → activate → idle-sleep.

Render the screens to PNG for eyeballing:

```sh
SDL_VIDEODRIVER=dummy .venv/bin/python - <<'PY'
import os, pygame, neptrPhase4_UI as UI
pygame.display.init(); pygame.font.init(); pygame.display.set_mode((1,1))
ui = UI.NeptrUI(); ui.companion.active = True
ui.presets = ["CAR_MODE","SURF_NEPTR"]; ui.power_label = "high"
for s in (0,1,2):
    ui.companion.screen = s; ui._render_companion(ui.canvas)
    pygame.image.save(ui.canvas, f"/tmp/companion_{s}.png")
PY
```

---

## 9. How to actually test it (on the device)

1. **Confirm the MIDI path.** Csound must be getting Teensy CCs on `14:0`
   (via `serial_bridge.py`). Check the bus is alive:
   ```sh
   aseqdump -p 14:0     # move the joystick → CC27/CC28; press it → CC77
   ```
2. **Point the UI at that bus.** The launcher already runs the UI under
   `kivy-venv` (built `--system-site-packages`, so it sees apt `python3-mido`
   / `python3-rtmidi`). If auto-detect picks the wrong port, set
   `NEPTR_MIDI_PORT` in the unit environment and restart the UI.
3. **Wire the helpers.** Set `NEPTR_PATCHES_DIR`, `NEPTR_PRESET_CMD`,
   `NEPTR_POWER_TOGGLE_CMD`, `NEPTR_POWER_STATUS_CMD` to your real demiurge
   commands (or accept the defaults if they match).
4. **Drive it:**
   - Hold the joystick ~3 s → the dashboard appears.
   - Push the joystick left/right → DASH ↔ PRESET ↔ POWER tabs.
   - On PRESET, push up/down to highlight, press the arcade button to load.
   - On POWER, press the arcade button to toggle.
   - Hold the joystick ~3 s again (or wait 15 s) → back to the faces.
5. **Watch the log:** `journalctl -u neptr-ui -f` — startup prints
   `companion: MIDI input on <port>` (or a warning if none was found).

---

## 10. Coordination with demiurge autoload

The UI is launched by `neptr-ui.service` → `/usr/local/bin/demiurge-ui-launch.sh`,
which reads `live.conf` (`ui` / `ui_cmd` / `ui_also`). That config-driven
autoload is owned by the demiurge side (DEMIURGE_OS tree); this document only
covers what the launched program *does*. The companion needs no service
changes — it runs inside the same `ui_cmd` process.

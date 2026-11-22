# NEPTR Phase 4 — Kivy UI

> **Outdated.** The UI is now hand-drawn pygame (not Kivy) and has an
> interactive companion mode. Master reference: **[companion.md](companion.md)**.

Portrait-mode Kivy display that autostarts on boot and reflects the live
state of the Csound engine via OSC. Zero interactive controls — state
comes in over UDP, the UI just renders it.

## What it shows

- **Menu view** (`/menu <0..5>`): the full vertical height is given to
  a single column of effect names in the selected menu. Names brighten
  when their effect is ON and dim when bypassed.
- **Effect view** (`/<code>_state 0`): triggered when an effect is
  turned on. The top label flips to the effect name and the body shows
  one horizontal slider per editable parameter — each with name, filled
  slider bar, and numeric value at the far right. Bypassed = dimmed.
- **Sleep screen**: after 10 s with no OSC, the UI replaces itself with
  a neutral purple background and an animated face that occasionally
  blinks or cycles expressions. Any OSC message wakes it.

## OSC protocol

Server: `127.0.0.1:8000` (UDP). Addresses:

| Address | Type | Meaning |
|---|---|---|
| `/menu` | int 0–5 | Switch menu (see `MENUS` in `neptrPhase4_UI.py`). |
| `/<code>_state` | int 0/1 | 0 = effect ON → switch to effect view; 1 = bypass → dim. |
| `/<code>_<param>` | float | Update one parameter value. Ranges in `EFFECT_PARAMS`. |

Coverage: **6 menus, 25 effects, 55 parameters**. Every one is
OSC-addressable — encoders, expression pedals, or any other controller
routed through the Csound engine can reach any of them. Effect codes and
parameter tables are the single source of truth in `neptrPhase4_UI.py`;
`csound/globals.orc` emits the same address space via `OSC3x_f` /
`OSC3x_i`, and the UI is a read-only mirror.

## Autostart on boot

Unit file: `scripts/neptr-ui.service` (installed to
`/etc/systemd/system/neptr-ui.service`).

- Runs as `gorpi7`, in the `video` + `render` + `input` supplementary
  groups so SDL2 can open `/dev/dri/card0` for KMSDRM.
- Pulls in after `multi-user.target` (no X/Wayland — it renders
  straight to the framebuffer).
- `Restart=on-failure` with 3 s backoff.
- Stdout/stderr go to the journal: `journalctl -u neptr-ui -f`.

First-time install on the Pi:

```sh
sudo apt install -y python3-kivy
python3 -m venv --system-site-packages ~/kivy-venv
~/kivy-venv/bin/pip install python-osc
sudo install -m 0644 ~/NEPTR_phase4/scripts/neptr-ui.service \
  /etc/systemd/system/neptr-ui.service
sudo systemctl daemon-reload
sudo systemctl enable --now neptr-ui.service
```

Subsequent syncs (`scripts/sync_to_pi.sh`) refresh the unit file and
restart the service automatically if it's already running.

## Portrait rotation

The physical panel is a 1280×720 HDMI display mounted portrait (USB-C
down). Kivy's `Window.size = (720, 1280)` + `Window.rotation = 270`
handle the rotation in software — the kernel `video=…,rotate=…` cmdline
param is **not** honored by Pi 5's `vc4-kms-v3d` driver.

Override without editing code:

```sh
sudo systemctl edit --runtime neptr-ui.service
# [Service]
# Environment=NEPTR_ROT=90   # or 0, 180, 270
sudo systemctl restart neptr-ui.service
```

## Smoke test

With the service running, fire a traversal at the live UI:

```sh
~/kivy-venv/bin/python ~/NEPTR_phase4/scripts/osc_smoketest.py
```

Cycles every menu, turns one effect per menu on, sweeps its params,
then bypasses. Watch the screen — any missed transition points at a
protocol mismatch between engine and UI.

## Troubleshooting

- **Black screen on boot** — check `journalctl -u neptr-ui -n 50` and
  the latest `~/.kivy/logs/kivy_*.txt`. Most common: `/dev/dri/card0`
  missing — service bails via `ConditionPathExists`.
- **UI stuck** — `sudo systemctl restart neptr-ui.service`. Reloads in
  under 2 s.
- **Wrong orientation** — flip `NEPTR_ROT` (see above), no reboot.
- **OSC not landing** — `sudo ss -ulpn | grep 8000` should show the
  UI's python3 PID bound on that port.

## NORRIS page (menu 20) — SoundMagic rack

Menu 20 NORRIS is a custom touch-only page (`sm_page.py`, delegated from `neptrPhase4_UI.py`; `NORRIS_MENU`).
Menu 2 SPECTRAL is a plain five-slot menu again (FORMS, FORMANT, BINSCATR, BIN REDU, BCRUSHER); the engine has
no slot branches for menu 20. The page: credit line, a 5x4 tile grid (fx 0..18 in registry order + a
RACK tile), the focused-effect strip (hero P1..P3 as vertical faders), and the expression strip.
Everything is generated from `csound/spectral/sm_params.json` (no hand lists).

- Tile: code, on-state (engine-authoritative via `/sm_state`), mini bar showing hero P1.
  Phase-2 tiles are dimmed with "soon" but fully usable. Tap = send `/st fx 0|1` and focus
  (no local on-state flip). Long-press >= 450 ms, or tapping the focused tile's upper half or
  the strip title, opens the deep sheet. Tapping the focused tile's lower half toggles it.
- Strip faders: absolute `/sp`, rate-limited to 30 Hz, final value on release; the thumb shows
  the finger value, then snaps to the `/sm_p` echo.
- Deep sheet (full-screen modal): one row per param (float fader, bool toggle, enum stepper,
  FFT Size as 8 chips), ON/OFF chip, BACK chip, swipe down (at scroll top) closes, fixed
  footer "SoundMagic Spectral · Michael Norris". A fader row decides axis after 14 px:
  horizontal = fader, vertical = scroll.
- RACK sheet: default FFT size chips (sends `/sp 23 0 <idx>`; the engine maps fx 23 p 0 to
  `gkSMDefaultFFT`) and a latency readout = sum over distinct active FFT sizes N of
  2*(N/4)/48000 s (effects without an FFT Size param use the rack default).
- Idle cost: no animation; repaints only on events (long-press uses a deadline).

### OSC
| Dir | Address | Args | Meaning |
|---|---|---|---|
| UI -> engine :7770 | `/st` | i fx, i on | toggle effect |
| UI -> engine :7770 | `/sp` | i fx, i p, f val | absolute param write (bool/enum rounded) |
| engine -> UI :8000 | `/sm_state` | i fx, i on | on-state |
| engine -> UI :8000 | `/sm_p` | i fx, i p, f val | param value / echo |

Port override: `NEPTR_SM_OSC_PORT`. Tests: `tests/test_sm_page.py`; shots via `scripts/ui_render_test.py`.

### Web (demiurge-web `#/neptr/spectral`)
Phone page for the whole SoundMagic rack: all 23 effects grouped spectral / time-domain with a live / phase 2 / phase 3
badge, an ON switch each (`/st`), and every parameter as a control (range slider with log taper when max/min > 100,
switch, select, FFT chips; each change -> `POST /api/neptr/sm/param` -> `/sp fx p val` on 7770). "Reset to his defaults"
sends every default. **Hero pins:** each param row has a hero-slot select (P1..P7, unique per effect) and a PEDAL pick;
changes autosave via `PUT /api/neptr/sm/heroes` -> `csound/spectral/sm_heroes.json` -> `gen_sm_params.py` regenerates
`sm_params.json` / `sm_params.orc` -> live push `/sh fx k idx` and `/shp fx idx`. Live values need the engine's mirror to
127.0.0.1:8005 (see `csound/spectral/CONTRACT.md` "Web mirror"); without it the page marks values "unconfirmed".
Open `http://demiurge.local:8080/#/neptr/spectral` (also linked from NEPTR index and the SPECTRAL menu page).
The generated web model (`web/neptr_model.py`) is rebuilt with `python3 web/gen_neptr_model.py neptr > web/neptr_model.py`
(now 20 menus); the web test suite fails if it drifts from `neptrPhase4_UI.py`.

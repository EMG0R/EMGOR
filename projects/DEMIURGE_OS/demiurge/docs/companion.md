# DEMIURGE companion — the MASTER UI

The **`demiurge` terminal companion** is the master control surface for the Pi,
run from your laptop. One screen: live status up top, single-key control of
**power**, **audio**, and **patches** (config-file variants applied via
`demiurge-set`). It drives the Pi (`gorpi7@demiurge.local`) over SSH — nothing
is installed on the Pi for the UI itself.

The **on-device NEPTR pygame UI** (joystick-hold companion, PATCH tab) uses the
exact same directory and switch mechanism, so both UIs show the same list and
stay in sync automatically — there is only one "patches" concept in DEMIURGE,
not two.

The PATCHES list and the SENSORS line are never hardcoded in the script —
every refresh it lists whatever's actually in `~/demiurge/sets/*.conf` on the
Pi, and mirrors whatever `demiurge-monitor` is currently publishing. Drop a
file on the Pi and it shows up on the next 2 s refresh, in both UIs; nothing to
edit on either side.

```bash
demiurge          # launch the UI  (alias in ~/.zshrc, like `gorpi`)
demiurge --once   # print one frame and exit (scripting / testing)
```

---

## The screen

A full-window dashboard with **bar graphs** and the complete thermal decode. It
**sizes itself to the terminal** — bars and rules scale to the width, and it pads
to fill the height. Make the window ~1/4 of your screen and it fills it.

```
  ▞▚▞ DEMIURGE · gorpi7@demiurge.local · up 36m
  ──────────────────────────────────────────────────────────────
  TEMP  43.3°C ████████████████████████░░░░░░░░░░░░░░░░░░░ warn80 crit85
  FREQ  2800   ████████████████████████████████████████░░ 1500-2800
  LOAD  0.42   ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 4 cores
  MEM   596M   ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ /16214M
  ──────────────────────────────────────────────────────────────
  THROTTLE 0x50000
    now  UNDERVOLT ARM-CAP THROTTLE TEMP-LIM      (red when set)
    past UNDERVOLT ARM-CAP THROTTLE TEMP-LIM      (yellow when occurred)
  VOLT 0.87V   CORES 0-3 (4)   GOV performance
  AUDIO ● active   UI ✓ on   PWR HIGH
  ──────────────────────────────────────────────────────────────
  PATCH neptr_gorcore
  SIGNAL IN ─▶ neptrPhase4 ─▶ OUT
  ──────────────────────────────────────────────────────────────
  PATCHES (~/demiurge/sets)
   1   car_mode         low
   2   neptr_bass       high
   3 ▸ neptr_gorcore    high
   4   neptr_surf       high
   5   testing_ground   high
  ──────────────────────────────────────────────────────────────
  1-9 patch    l low m med h high    s start x stop    t telemetry    i interface    r refresh q quit
```

The screen redraws after every action and **auto-refreshes every 2 seconds**
(1 s while disconnected, so reconnect is caught quickly).

---

## Keys

| Key | Action |
|---|---|
| `1`–`9` | Switch to that **patch**, in the order listed in the PATCHES box (runs `demiurge-set <name>` on the Pi) |
| `l` `m` `h` | **Power** → low / medium / high |
| `s` | **Start** audio (`systemctl start demiurge`) |
| `x` | **Stop** audio (`systemctl stop demiurge`) |
| `t` | **Telemetry** — live control-surface monitor (Teensy MIDI). Ctrl-C returns |
| `i` | **Interface** — full-screen audio-interface page: attached interfaces by product name, `▸` marks where DEMIURGE runs; digit picks one (runs `demiurge-interface <name>` on the Pi — writes the global choice and restarts audio). Any other key returns |
| `r` | Refresh now |
| `q` | Quit |

Single keypress, no Enter. Any other key (and the 2 s timeout) just refreshes.
Patch switching **auto-applies power** from each patch's `# power:` tag —
power first, then the switch, so the governor is already at the target level
before the audio engine restarts on it. `l`/`m`/`h` still let you override
power independently at any time.

---

## Status header

| Field | Source | Notes |
|---|---|---|
| `TEMP` | `vcgencmd measure_temp` | **bar** to 85°C; green <70, yellow ≥70, red ≥80 (`warn80 crit85`, from the thermal watchdog) |
| `FREQ` | `scaling_cur_freq` | **bar** across `cpuinfo_min`–`scaling_max` |
| `LOAD` | `/proc/loadavg` (1 min) | **bar** vs core count; green/yellow/red |
| `MEM` | `/proc/meminfo` | **bar** used / total |
| `THROTTLE` | `vcgencmd get_throttled` | raw hex + full decode of all 8 bits: `now` (red when set) and `past`/occurred (yellow) for UNDERVOLT / ARM-CAP / THROTTLE / TEMP-LIM — same bits as `demiurge-thermal-watchdog.sh` |
| `VOLT` | `vcgencmd measure_volts` | core voltage |
| `CORES` | `/sys/.../cpu/online` + `nproc` | e.g. `0-3 (4)`. On a weak supply collapses to `0 (1)` — shed cores |
| `GOV` | `scaling_governor` | the companion pins `performance` |
| `PWR` | derived from `scaling_max_freq` | `LOW` ≤1.6 GHz · `MED` ≤2.4 GHz · `HIGH` otherwise |
| `AUDIO` | `systemctl is-active demiurge` | green `● active` / dim `○ inactive` |
| `UI` | `systemctl is-active neptr-ui` | green `✓ on` / dim `✗ off` |
| `up` | `/proc/uptime` | header uptime |
| `PATCH` | `# set:` tag in `live.conf` (falls back to `# patch:` for compatibility) | the active patch name |

---

## Patches

A **patch** is a live.conf variant on the Pi (`~/demiurge/sets/<NAME>.conf`),
switched with `demiurge-set <name>` (cp over `live.conf` + restart) — the same
command the on-device NEPTR UI's own PATCH screen (joystick hold → PATCH tab)
already used before the Mac side caught up to it. There's one directory, one
switch mechanism, one list, shared by both UIs. Header tags:

```ini
# set: neptr_gorcore
# power: high
```

`# power:` is optional — missing it defaults to `high` (see `actions.patch_power()`
on-device / the `pw=${pw:-high}` fallback in the Mac script). Both UIs apply it
automatically on switch, before restarting the audio engine.

(The on-disk tag stays `# set:` for compatibility with the existing
`demiurge-set` CLI and `~/demiurge/sets/` directory name — only the **UI
labels** changed to "patch"/"PATCHES", not the underlying file convention.)

The companion lists every `*.conf` in `~/demiurge/sets/` on each refresh —
**nothing is hardcoded on the Mac side**. A patch named `nam` is hidden (it has
its own flow) — matching the on-device UI. Number keys (`1`–`9`) are assigned
in the order patches are listed (alphabetical by filename); beyond 9 patches
the rest are shown but not bound to a key — they can still be reached from the
on-device UI's joystick nav.

Currently on the Pi:

| Patch | Power |
|---|---|
| `car_mode` | **low** — brownout-safe (MacBook USB-C / in-car) |
| `neptr_bass` | high |
| `neptr_gorcore` | high |
| `neptr_surf` | high |
| `testing_ground` | high |

**Switching is always confirmed (2026-08-10).** All three UIs now take two
deliberate actions to load a patch, because one action rewrites `live.conf`,
moves the power governor and restarts the audio engine:

| UI | Confirm |
|---|---|
| web | confirm sheet (pre-existing) |
| terminal companion | digit key → `[y/N]` prompt |
| on-device NEPTR | first tap arms ("tap again → NAME"), second tap inside 4 s commits |

And all three mark the patch that is actually loaded — `▸` in the terminal and
web UIs, `●` on the on-device PATCHES screen, all resolved the same way (the
`# set:` tag in `live.conf`). The on-device screen previously had **no** active
marker and parked its cursor on row 0, so `car_mode` — alphabetically first,
and the one patch that drops the Pi to LOW power — permanently looked like the
live patch, and a single stray contact on the resistive panel loaded it. Its
cursor now opens on the live patch, and re-loading the already-active patch is
refused ("already loaded").

All variants of the same NEPTR Phase 4 chain, switching mostly
`csound_extra = --omacro:PRESET_BANK=N` and sync-layer/thread-count. (The old
`neptr` entry — a generic, untagged template with no specific rig purpose — was
removed 2026-07-03; the five above cover every real use case.)

**Add a patch:** drop `~/demiurge/sets/<NAME>.conf` on the Pi (with `# set:` /
optional `# power:` headers), or `cp ~/demiurge/live.conf ~/demiurge/sets/<NAME>.conf`
to save the current live config as a new one. It appears in the companion's
PATCHES box on the next refresh, and in the on-device UI's PATCH screen
immediately too — both UIs read the same directory.

> **Historical note:** an earlier iteration of this companion also read a
> *separate* `~/demiurge/patches/*.conf` directory (whole-engine swaps like
> `SURF_NEPTR`/`CAR_MODE`/`RNBO_NEPTR`/`helloPatching`/`TESTING_GROUND`,
> applied via raw `cp` + a power side-effect) as a second, differently-labeled
> "PRESETS" box. That directory still exists on the Pi (nothing was deleted)
> but neither UI reads it anymore — it was a redundant, confusing second
> system. If you want any of those five configs back in the UI, copy the file
> into `~/demiurge/sets/` (retagging its header `# patch:` line to `# set:`).

---

## Power modes

Applied to the Pi's cpufreq at runtime — governor `performance`,
`scaling_max_freq` cap, and core offlining (the isolated **audio core is always
kept online**).

| Mode | Freq | Cores | Use |
|---|---|---|---|
| **low** (`l`) | flat 2000 MHz | cpu0 + audio core + cpu1 (3) | MacBook USB-C — brownout-safe |
| **medium** (`m`) | 2400 MHz | all | own PSU, no overclock |
| **high** (`h`) | full `config.txt` ceiling (2800) | all | official 5V/5A PSU only |

**2026-10-03: the knob is now real.** `demiurge-power <level>` (Pi-side; also
`demlow`/`demmed`/`demhigh`) writes live.conf `power =` and restarts the root
oneshot `demiurge-power.service`, which also applies it at boot. low moved from
1.5 GHz to 2.0 GHz flat: at 1.5 GHz the NAM-loaded csound ran ~90-100% of a core
and xruns. See `power` in config-reference.md.

Also available as standalone Mac commands: `lowdemiurge`, `mediumdemiurge`,
`highdemiurge` (all backed by `demiurge-power-remote`).

> **Brownout reality (verified live):** on a MacBook USB-C port the Pi 5
> undervolts (`get_throttled=0x50000`), **sheds cpu1/2/3**, and can't re-boot
> them (`psci failed to boot CPU3`) — it survives on 1 core / 1.5 GHz. `high`/
> `medium` cannot online the dead cores, and audio (pinned to cpu3) won't run
> until you're on the **PSU + reboot**. The brownout happens at *boot* while
> `config.txt` is the high overclock block, so runtime `low` can't *prevent* it
> — only a laptop-safe `config.txt` (boot-only) does. Full physics in
> [`../../config/pi5-performance/README.md`](../../config/pi5-performance/README.md).

---

## Audio control

- In the UI: `s` start, `x` stop.
- Shell aliases (`~/.zshrc`): `redem` (restart), `stodem` (stop).

---

## SIGNAL — signal-flow visualization

A primitive, terminal-only view of the active patch's signal path, rendered as a
`SIGNAL` row under `PATCH`.

```
   PATCH neptr_gorcore
   SIGNAL IN ─▶ neptrPhase4 ─▶ OUT
```

- **Source:** the remote `awk` emits one `signal=tok1|tok2|…` line per `chain =`
  block from the active `live.conf`; `fmt_signal()` formats it on the Mac. Reuses
  the status SSH round-trip — no extra query.
- **Ends:** `IN ─▶` prefix only when the first stage takes hardware input
  (`sync_layer=off`, or an `in1/in2 ->` head); `─▶ OUT` always. A `*.rnbo`
  token renders as `rnbo:<name>`; basenames have their extension stripped.
- **Parallel chains:** one `SIGNAL` line per `chain =` block (first row carries
  the `SIGNAL` label, the rest are indented).
- **Always on**, no keypress.
- **Out of scope (stays primitive):** no live PipeWire/JACK graph query, no
  branch/fan-out boxes, no interactivity.

---

## Telemetry — live control surface (`t`)

A live monitor of the NEPTR control surface (encoders, buttons, footswitches,
joystick, pedal, FSRs, mic, LiDAR, accelerometer). Press `t`; Ctrl-C returns.

**It reads MIDI, not the raw serial — on purpose.** The Teensy's `/dev/ttyACM0`
serial is a **single-reader** port that csound needs (via `serial_bridge.py` →
MIDI); a second reader would garble both. But the firmware *also* emits the same
controls as **USB-MIDI CC** (ch1), and ALSA's sequencer fans those out to any
number of subscribers. So the companion runs `aseqdump -p <Teensy MIDI>:0` — a
read-only extra subscriber that **never touches the serial port** and can't
disturb csound or the bridge.

State is mapped from the firmware CC map (source of truth:
`NEPTR phase4/TEENSYY/TEENSYY.ino`): encoder rotations CC20–26 (⟳/⟲), encoder
buttons CC70–76 (●/○), footswitches CC60–68, joystick 27/28/btn 77, pedal 29,
FSR 34/35, mic 78/79, LiDAR 30/36, accel 31/32/33. It's event-driven — idle =
no events; move a control and it lights up.

## Architecture

- **Lives on the Mac:** `/opt/homebrew/bin/demiurge` (a Bash TUI). Aliased in
  `~/.zshrc`.
- **Drives the Pi over SSH**, multiplexed with `ControlMaster` /
  `ControlPersist=300` so repeated calls reuse one connection (snappy refresh).
  Requires key auth + passwordless `systemctl`/`sudo` for `gorpi7@demiurge.local`.
- **Status** is one SSH round-trip returning `key=value` lines, parsed on the Mac
  — including a `pfile=` line per patch file found on the Pi, so the PATCHES
  box rebuilds from live data every refresh, never from a script-side list.
- **Power** reuses `demiurge-power-remote`; **audio** uses `systemctl`; **patches**
  `demiurge-set <name>` (same command the on-device UI uses).
- Bash 3.2-compatible (macOS default shell) — no associative arrays (indexed
  arrays are fine and are how the dynamic patch list is held).
- **Parity with the on-device UI:** both UIs read `~/demiurge/sets/*.conf`,
  both exclude `nam`, both switch via `demiurge-set`. See `NEPTR_phase4/actions.py`
  (`list_patches`, `patches_dir`) and `neptrPhase4_UI.py` (`self.patches`,
  `_render_patches`, COMPANION_TABS `["DASH", "PATCH", "POWER"]`) for the
  on-device side — renamed from "preset" to "patch" terminology in the same
  pass as this script, so both UIs now use the same word for the same thing.

---

## Gotchas

- **`PATCH <none>`** appears if the active `live.conf` has neither a `# set:`
  nor a legacy `# patch:` tag. Resolves the moment you pick a patch.
- On laptop power, `high`/`medium` and audio won't fully work (dead cores) — see
  the brownout note. This is hardware, not a UI bug.
- The UI is **Mac-side + SSH**: it controls the Pi at runtime and installs
  nothing on it. The Pi just needs its existing services (`demiurge.service`,
  cpufreq, passwordless `systemctl`) — all already in place.
- `~/demiurge/patches/*.conf` still exists on the Pi but is no longer read by
  either UI (see the historical note above) — not a bug, an intentional
  consolidation down to one system.

---

## File map

| Thing | Location |
|---|---|
| Companion (the UI) | Mac `/opt/homebrew/bin/demiurge` + `~/.zshrc` alias |
| Power helper + commands | Mac `/opt/homebrew/bin/` — `demiurge-power-remote`, `lowdemiurge`, `mediumdemiurge`, `highdemiurge`, `stopdemiurge`, `startdemiurge` |
| Patches | Pi `~/demiurge/sets/*.conf` (shared with the on-device NEPTR UI's PATCH screen) |
| Active config | Pi `~/demiurge/live.conf` |
| Unused legacy dir | Pi `~/demiurge/patches/*.conf` (kept on disk, not read by either UI) |
| Boot-layer foundation | `/config/pi5-performance/` (governor, IRQ/RT, thermal watchdog, `config.txt*`) |

# DEMIURGE Config Reference

Full syntax for `~/demiurge/live.conf` and `/boot/firmware/demiurge.conf`. These are the only two files DEMIURGE ever asks you to touch.

---

## `~/demiurge/live.conf` — the only user-facing config

This is the file you edit day-to-day. The launcher polls its modification time once a second and hot-reloads on every save. Stages that didn't change keep running; stages you added start; stages you removed stop; the audio graph is re-applied.

There is no other user-facing format. Internally the launcher compiles `live.conf` into a Rust session graph (programs + edges), but you never see that graph and you never edit Rust unless you want to extend the engine itself. The simple `chain =` interface is the whole interface.

### Shape

```ini
# DEMIURGE-DEVICES-BEGIN
# devices detected by DEMIURGE (read-only, auto-refreshed when idle)
# audio:
#   - Scarlett Solo 4th Gen Headphones / Line 1-2  (out only)
#   - Scarlett Solo 4th Gen Input 1 Inst/Line  (in only)
# midi:
#   - Launchkey Mini MK3
# DEMIURGE-DEVICES-END

link = off
bpm  = 110

chain =
  in1 ->
  ~/demiurge/examples/hello_patching/faust_delay.dsp
  ~/demiurge/examples/hello_patching/cpp_crush.cpp
  ~/demiurge/examples/hello_patching/csound_ringmod.csd

midi =
  ~/patterns/driver.strudel
```

### Keys

| Key       | Type           | Meaning                                                                                             |
|-----------|----------------|-----------------------------------------------------------------------------------------------------|
| `link`    | `on` / `off`   | Ableton Link network sync. On save, `demiurge-clock` is respawned with `--link=on` or `--link=off`. |
| `bpm`     | number         | Initial tempo when Link is off. Integer or float.                                                   |
| `rate`    | integer Hz     | Graph sample rate. Default `48000`. Pinned via `pw-metadata clock.force-rate` at service start and injected into every child wrapper as `DEMIURGE_RATE`. Change this and the whole chain renegotiates — no manual PipeWire reconfiguration needed. |
| `quantum` | integer frames | Graph period (JACK bufsize). Default `128` (= 2.67 ms at 48 kHz). Pinned via `pw-metadata clock.force-quantum` and injected as `DEMIURGE_QUANTUM` into every wrapper so csound gets `-b $Q`, chuck gets `--bufsize:$Q`, etc. Must be a power of two ≥ 32 and ≥ the largest `ksmps` in any Csound orchestra you run (csound's rtjack backend refuses `-b` that isn't a multiple of `ksmps`). **In bypass mode (`sync_layer = off`) this does NOT control latency** — csound's ALSA backend pins a 1024-frame ring (~20.4 ms) whatever you set, so tune `quantum` for CPU headroom instead. See gotchas Q7c. |
| `bits`    | `16` \| `32` \| `float` | Sample format handed to the device. Default `32` (32-bit int). Platform-independent: it names the FORMAT and each wrapper maps it to its engine's flag (csound `-s`/`-l`/`-f`), so the same key means the same thing on an I2S HAT, a USB interface or HDMI. Exported to wrappers as `DEMIURGE_BITS`. **Default matters:** csound's ALSA backend negotiates S16_LE if nothing asks otherwise and `plughw` accepts it silently — a 24-bit converter then runs with its bottom 8 bits discarded (measured 2026-08-10). Feed a 24-bit DAC with `32`, not `24`: csound's realtime ALSA rejects `-3` outright. No latency cost either way. |
| `csound_host` | `standalone` \| `embedded` | Which Csound backend `demiurge-run-csound` uses in sync-layer mode. Default **`standalone`** — the `csound` binary with `-+rtaudio=jack`, which computes on its own thread and hands blocks to the JACK callback across the `-B` ring. `embedded` runs `demiurge-csound-host` instead: an ordinary JACK client that calls `csoundPerformKsmps()` **inside** the process callback, so there is no engine thread, no ring and no `-B`. Same JACK identity (`csound6`, ports `input1/2` + `output1/2`), same MIDI (`-+rtmidi=alsaseq -M 14:0 -Q 14:0`), same plugin opcodes, same `--omacro:` macros. **Requires `ksmps` to divide `quantum`** — the host refuses to start otherwise, printing the arithmetic, rather than glitching; the standalone backend instead rounds `-b` UP to the next `ksmps` multiple and quietly runs a different period than you asked for. **`embedded` was marked NOT production-ready** on 2026-08-10 — the JACK plumbing measured at zero added latency, but with Csound actually computing in the callback the round trip was unstable (sd 30–80 frames, run-to-run mean 235–380). **A cause for that instability was found and fixed on 2026-08-11** (gotchas Q7h): libcsound's default message handler was writing to the stage log on the SD card from inside the RT process, so every orchestra message stalled the audio core. The host now routes Csound messages through a lock-free ring drained off the audio thread. Measured after the fix, quantum 64, real patch under load: **p99 ≤ 56% of the period budget, 2 late callbacks in 105,000 (0.002%, both at startup), and the per-node xrun rate fell from ~2.9/s to ~0.07/s.** The latency **spread has not been re-measured** since — do that before calling it production-ready, and see `latency-measurements.md`. Run with `DEMIURGE_HOST_STATS=1` to get the deadline histogram. |
| `rt_core` | integer \| `off` | CPU core the ENGINE is pinned to (`taskset`), alongside its existing `chrt --fifo 74`. Default: the last core in the kernel's `isolcpus=` token, so a tuned Pi needs no config. `off` disables pinning. **Only the engine belongs on the isolated core** — pinning the whole service there (systemd `AllowedCPUs=`) measured worse than no pinning, because the launcher and its helpers then preempt the engine. Exported as `DEMIURGE_RT_PIN`. See gotchas Q7c. |
| `power`   | `low` \| `medium` \| `high` | CPU power level, default `high`. Applied by the root oneshot `demiurge-power.service` at boot (before `demiurge.service`) and by `demiurge-power <level>` (also `demlow` / `demmed` / `demhigh`), which writes this key then restarts that unit. **low** = cpu0 + the isolated core + cpu1, governor `performance`, flat 2000 MHz (flat avoids di/dt sag on weak supplies; 2.0 GHz is the floor the NAM-loaded engine needs — 1.5 GHz measured ~90-100% of a core and xruns). **medium** = all cores, 1500..2400 MHz (stock). **high** = all cores, 1500..`cpuinfo_max_freq` (2800 with the Phase 8 overclock block in `config.txt.snippet`). Optional overrides: `cpu_max_mhz = <MHz>`, `cpu_cores = <N>` (cpu0 and the isolated core are never offlined). A set file's `# power: <level>` header is documentation, and the fallback when the key is absent; the key wins. |
| `chain`   | indented list  | Ordered audio stages. Piped top→bottom, left→right. See below.                                      |
| `midi`    | indented list  | Programs that should run and join the MIDI bus but **not** be wired into the audio graph.          |
| `inputs`  | indented list  | Stage basenames (matching entries in `chain =`) that should receive `in1` + `in2` as extra parallel edges on top of whatever the chain already wires into them. Lets a mid-chain csound stage take hardware capture directly without forcing an upstream ChucK stage to pass adc through. |
| `ui`      | `on` / `off`   | Master toggle for the on-device UI (the NEPTR face display) + any `ui_also` aux commands. Default `on`. Read by `neptr-ui.service` → `demiurge-ui-launch.sh`. See *The UI section* below. |
| `ui_cmd`  | command        | The **full command** (interpreter + path/args) for the main UI, e.g. `/home/gorpi7/kivy-venv/bin/python /home/gorpi7/NEPTR_phase4/neptrPhase4_UI.py`. |
| `ui_also` | indented list  | Optional extra full commands launched alongside the UI, one per indented line. All relaunch on `systemctl restart demiurge`. `#` comments a line out. |
| `touch`   | `on` / `off`   | Touchscreen → MIDI CC. Default `off`. Read by `demiurge-touch.service` → `demiurge-touch --daemon` (exits cleanly when off, same self-gate as `ui`). Toggle live with `demiurge-touch on|off`. See *The touch section* below. |
| `touch_cc_base` | integer  | First CC of the touch block. Default `80` (block CC80–95 — collision-free vs the Teensy map on ch1 and the clock CCs 116–119 on ch16). |
| `touch_channel` | integer  | MIDI channel for touch CCs, 1-indexed. Default `16` — the clock/utility channel all clock-domain CCs already use. |
| `touch_slots` | integer 1–4 | How many multi-touch slots emit MIDI. Default `1` (the resistive XPT2046 panel is single-touch hardware; two fingers produce a phantom midpoint). Set `4` on a future capacitive screen. |
| `touch_poll` | `on` / `off` | The **polling touchscreen driver** (`demiurge-touch-poll.service`). Default `on`. This panel's controller has a dead PENIRQ line, so the kernel `ads7846` driver never fires; this daemon polls the chip over SPI and republishes it through uinput. Off only for debugging — turning it off leaves the panel completely dead. See *The polled touchscreen* below. |
| `touch_poll_hz` | integer 20–250 | Poll rate while a finger is down. Default `100` (the UI consumes at 60 Hz). |
| `touch_poll_idle_hz` | integer 5–120 | Poll rate while idle. Default `60`. Idle polls are a single 3-byte SPI read, so this is cheap. |
| `touch_poll_speed` | integer | SPI clock in Hz. Default `1000000`. The old overlay's `50000` made a full read burst cost 3.5 ms; at 1 MHz it is ~180 µs. |
| `touch_poll_zmin` | integer | Z1 pressure floor for "touched". Default `40`. **Raising this chops swipes into taps** — pressure dips mid-stroke; the coordinate gate is what rejects noise. |
| `touch_poll_rmax` | integer | R_touch ceiling in ohms for "touched". Default `5000`. |
| `touch_poll_margin` | integer | Raw counts from either rail treated as bogus. Default `100`. An untouched X plate floats to a rail (0 *or* 4095 — both screen corners), so this is the gate that actually rejects phantom touches. |
| `touch_poll_hold` | integer 1–30 | Consecutive below-threshold samples before releasing. Default `4` (40 ms at 100 Hz). Resistive panels lose contact mid-stroke; releasing too eagerly fragments one swipe into many taps. |
| `hdmi_default` | `on` / `off` | Whether HDMI audio (`vc4hdmi*` cards / `*hdmi*` PipeWire nodes) may be **auto-adopted** by the interface resolver's "first attached" fallback (rule 4) — HDMI then joins the ordering *after* every non-HDMI card. Default `off`: HDMI is still listed and explicitly selectable (`demiurge-interface`, `interface =` in a set), but never picked automatically. Internal knob — hand-edit in set files / live.conf; not surfaced in any UI. |
| `wavetable_osc_host` | host/IP | Where the NEPTR UI's **WAVE screen** sends the finger-drawn wavetable over OSC on edit-commit. Default `127.0.0.1`. Read once at UI start (restart `neptr-ui` after changing). See `wavetable-osc.md` for the message protocol + a ready-to-paste Csound receiver. |
| `wavetable_osc_port` | integer | UDP port for the wavetable OSC messages. Default `8001` (the UI itself *listens* on 8000; keep the two distinct). Env overrides for dev: `NEPTR_WT_OSC_HOST` / `NEPTR_WT_OSC_PORT`. |

### Changing these from a UI — `demiurge-audio`

`rate`, `quantum`, `bits` and `sync_layer` are hand-editable like everything else, but they also have a CLI, and **that CLI is the only write path any UI may use**:

```
demiurge-audio                   # human status of all four
demiurge-audio --list            # key|value|default|options|effect   (for UIs)
demiurge-audio --status-machine  # mode|rate|quantum|bits|latency_ms
demiurge-audio bits 16|32|float
demiurge-audio quantum <power of two 32..2048>
demiurge-audio rate 44100|48000|88200|96000
demiurge-audio mode sync|bypass  # writes sync_layer = on|off
```

It validates, writes the key in place (comments and the rest of the file are preserved byte-for-byte; a missing key is inserted *before* the first block key, never appended into a `chain =` list) and restarts `demiurge`. `--no-restart` writes without bouncing the engine.

Two things it exists to stop UIs getting wrong:

- **`bits 24` is refused**, with the reason. csound's realtime ALSA rejects `-3` ("Unknown sample format") — 24-bit is a file-only depth there. `32` is how you feed a 24-bit converter.
- **The `effect` field is mode-aware.** For `quantum` it reads as a real latency figure under `mode = sync` and as *"CPU headroom only — latency is pinned at 1024 frames by the audio backend"* under `mode = bypass`. UIs render that sentence verbatim instead of computing a latency claim of their own, so the bypass-mode buffer-size trap is impossible to misrepresent in any of the three front-ends at once.

**Rate and quantum are one knob (2026-10-02).** `rate` and `quantum` in `live.conf` are the only place to set them. `demiurge-quantum-limits lockstep` (run as `ExecStartPre=-` of `demiurge.service`, and by `demiurge-audio quantum|rate`) generates the device period, the device rate and the PipeWire clock fragments in `~/.config/{wireplumber,pipewire}/`, then pins `force-rate` / `force-quantum`. Device period = quantum (clamped: at least 64, period-num at least 4, ring at least 4 x quantum). A hand-edit of `quantum =` takes effect at the next service start. In bypass mode it writes files and does not touch PipeWire. Dry run: `demiurge-quantum-limits lockstep --dry-run --live <file>`.

Two read/inspect tools:

```
demiurge-audit-audio             # one-shot read-only report: rate, quantum, period, ring, csound threads/RT policy,
                                 # STATE_DIR on tmpfs?, librtjack hash, cards, mixer, xrun deltas, units, files
demiurge-mixer show              # the codec's capture/playback controls, discovered from the card
demiurge-mixer gain <dB>         # ADC input gain (PGA)
demiurge-mixer input <sel>       # ADC input selection
demiurge-mixer store             # alsactl store AND ~/demiurge/asound.state (what backup-rig.sh pulls)
demiurge-mixer restore           # reload it; runs as ExecStartPre=- of demiurge.service when the file exists
```

`demiurge-audit-audio` never plays audio and never restarts anything. `demiurge-mixer` takes control names from the card, never assumes a brand, and exits 0 if a control is absent.

Because all four keys live in `live.conf`, `demiurge-set` replaces them on a patch switch: they belong to the patch. That is deliberate, and the CLI's human output says so.

### Wi-Fi — `demiurge-wifi`

Network selection is **not** a `live.conf` key — it lives in NetworkManager. The CLI is the single write path, and the UIs call it:

```
demiurge-wifi                    # human: current SSID, IP, visible networks
demiurge-wifi --list             # ssid|signal|security|flags   (flags: active,saved)
demiurge-wifi --status-machine   # state|ssid|signal|security|ip|iface
demiurge-wifi --scan             # force an AP rescan, then --list
demiurge-wifi ip                 # just the current IPv4 address
demiurge-wifi join <ssid>                     # saved or open network
demiurge-wifi join <ssid> --password-stdin    # password read from STDIN
demiurge-wifi forget <ssid>
```

- **Passwords are never arguments.** `--password` is refused outright. The secret is read from stdin and written directly into a mode-600 root-owned keyfile in `/etc/NetworkManager/system-connections/`; `nmcli` is only ever told to *load* that file. Nothing lands in `ps`, in shell history, or in the web UI's action log. The staging temp file is created under a 077 umask and removed on every exit path.
- **Listing never blocks.** `--list` and `--status-machine` pass `--rescan no`, so they read NetworkManager's cache and stay fast enough for a 2 s UI poll. An explicit refresh is `--scan`.
- **Priority is preserved.** New networks are created at `autoconnect-priority 20` with powersave disabled, matching the existing profiles; the home network's `30` is never touched, so it still wins whenever it is in range. `forget` refuses to delete the network you are currently on.

> **`forget` on the home network deletes the box's only route home.** Refusing to delete the network you are *currently on* is not the same guard: you can be on ethernet and forget the wifi you will need the moment the cable moves. This has already happened once — the home profile was deleted, NetworkManager autoconnected to a neighbour's AP on a different subnet, and the Pi became unreachable while running perfectly. See `config/network/README.md`.

Underneath the chooser sits the **connectivity stability pack** (`config/network/`), which is not user-facing and needs no configuration beyond `HOME_SSID` in `/etc/demiurge/network.conf`. It keeps **one link up at a time** — ethernet primary, wifi parked while the cable is working — because both sit on the same subnet here, and with both up `demiurge.local` was resolving to the *wifi* address while the cable sat idle. Pull the cable and wifi is carrying traffic in about 6 seconds. If you ever find wifi disconnected while ethernet is plugged in, that is the guard doing its job, not a fault.

Unknown keys are ignored. `#` starts a comment to end of line. Blank lines are allowed inside `chain =` and `midi =` blocks — they don't terminate the list. A block ends at the next non-indented `key = value` line.

### The chain

Each line under `chain =` is one absolute path (or `~/…`). Language is inferred from the file extension:

| Extension                 | Language                                                 |
|---------------------------|----------------------------------------------------------|
| `.csd`                    | Csound                                                   |
| `.ck`                     | ChucK                                                    |
| `.pd`                     | Pure Data                                                |
| `.scd`                    | SuperCollider                                            |
| `.dsp`                    | Faust (compiled on demand to a sibling binary)           |
| `.cpp`                    | C++ (compiled on demand with `g++ … -ljack -lasound …`)  |
| `.strudel`, `.mjs`        | Strudel (headless Node runner)                           |
| `.py`                     | Python                                                   |

### The `opgorator` token

A bare `opgorator` line (no path, no extension) wires in the Pocket
OpGorator's USB audio — a Daisy-Seed groovebox that shows up as a
capture-only USB audio class device (`alsa_input.usb-...`). No process is
spawned for it: the launcher resolves the already-present PipeWire node
(matched by `OpGorator` in its name, falling back to `Daisy`) and links its
capture ports directly, same as it does for RNBO/Strudel stages whose
client is owned by an external process.

As its own parallel chain it terminates straight at `out`, mixing for free
with everything else:

```ini
chain =
  opgorator
```

Stages can be inserted after it to process only the OpGorator's audio, e.g.
a dedicated FX patch:

```ini
chain =
  opgorator -> opgorator_fx.csd
```

If the OpGorator isn't plugged in, the token resolves to nothing and is
silently skipped (logged once) — it's picked up automatically the next
time the device is hot-plugged. See `docs/superpowers/specs/2026-07-29-pocket-opgorator-integration-design.md`
§3 for the full integration design (clock sync, WirePlumber driver pin,
etc).

### Hardware inputs (mono, numbered)

Hardware capture is always **mono** and addressed by **number**. There is no stereo input pair and no `in ->` shortcut for "both at once" — if you want both physical inputs into a stage, declare both:

```ini
chain =
  in1 ->
  in2 ->
  ~/fx/dual_input.csd
```

Endpoints:

| Token   | What it routes                                                                                            |
|---------|-----------------------------------------------------------------------------------------------------------|
| `in1 ->` | Physical input 1 (Scarlett Solo Input 1, or any USB interface's first `__Mic1__source:capture_MONO` node). Goes into the first stage's first input port. |
| `in2 ->` | Physical input 2 (`__Mic2__source:capture_MONO`). Goes into the first stage's second input port.        |
| `in ->`  | Legacy alias for `in1 ->`. Accepted but the explicit form is preferred.                                  |

Both `in1 ->` and `in2 ->` only work as the *first* line(s) of `chain =`. They route into stage 0; subsequent stages are piped from the previous stage as usual.

`-> out` at the end is optional — the launcher always terminates the chain at `demiurge-sink`, so you don't need to write it.

The stage id (used internally for the patch graph and by the logs) is the file's basename with the extension stripped, uniquified on collision.

### Implicit programs

You never list these. The launcher always supervises them:

- **`demiurge-clock`** — ALSA seq clock daemon. Restarted on save with the current `link =` flag.
- **`pd_clock_bridge.py`** — spawned only when any stage has `.pd` extension. Pipes the clock into Pd (Pd vanilla's raw ALSA MIDI can't see the shared seq bus).

### MIDI is implicit for every stage

Every program in `chain =` or `midi =` is subscribed bidirectionally to `Midi Through` (ALSA seq `14:0`) after launch — but only for languages that actually register an ALSA seq client. ChucK and Pd-vanilla use raw ALSA MIDI and don't have a seq client at all; the launcher detects this via `aconnect -l` and silently skips them rather than logging spurious "invalid destination address" errors. Csound, SuperCollider, and Faust binaries built with `-midi` all show up in seq and get wired automatically.

### The DEMIURGE-DEVICES block

The block between `# DEMIURGE-DEVICES-BEGIN` and `# DEMIURGE-DEVICES-END` is auto-managed. The launcher rewrites it whenever USB audio or MIDI devices change — and only when `live.conf` has been idle for several seconds, so your in-progress edits don't get clobbered. Treat it as read-only; anything you write inside the markers is overwritten on the next hot-plug.

### Hot-reload semantics

The launcher diffs the new live config against what's currently running and reconciles by `(id, file)`:

- **Unchanged stages** keep their PID. No restart, no xrun, no graph churn.
- **New stages** are launched and wired into the graph at their position.
- **Removed stages** are terminated, their ports are unlinked, the graph is re-applied.
- **Changed `link =`** triggers a clock subprocess respawn with the new flag.
- **Changed `bpm =`** is sent to `demiurge-clock` as a `CC118` steer message (no restart).

Every reconcile decision is logged to `journalctl -u demiurge`.

### The UI section (face display + aux commands)

`live.conf` is also central control for the on-device UI — the NEPTR face display — and any number of extra programs that should run beside it:

```ini
ui      = on
ui_cmd  = /home/gorpi7/kivy-venv/bin/python /home/gorpi7/NEPTR_phase4/neptrPhase4_UI.py
ui_also =
  # /home/gorpi7/NEPTR_phase4/scripts/hello-autoload-script.sh
  # any number of extra full commands, one per indented line
```

- `ui = off` keeps the UI (and all `ui_also` commands) down this boot; the service goes inactive.
- `ui_cmd` is a **full command** — interpreter and arguments, not just a path — so you can point it at anything.
- `ui_also` holds infinitely many more commands, one per indented line; `#` comments a line out.

The UI service (`neptr-ui.service`) runs `/usr/local/bin/demiurge-ui-launch.sh`, which reads these keys. On `ui = on` it starts every `ui_also` command in the background and `exec`s `ui_cmd` as the service's main process; on `ui = off` it exits cleanly. Because the unit is `PartOf=demiurge.service`, **`sudo systemctl restart demiurge` relaunches the UI and every aux command** along with the audio stack. It is independent of `audio =` — the UI runs whenever `ui = on` even if audio is gated off. Re-read on its own with `sudo systemctl restart neptr-ui`. The companion's status header shows `UI ✓ on` / `✗ off` from `systemctl is-active neptr-ui`. (`demiurge-ui-launch.sh` is installed by `setup-demiurge.sh` Phase 7; `neptr-ui.service` ships with `NEPTR_phase4` via `sync_to_pi.sh`.)

### The polled touchscreen (`touch_poll`)

**The panel's touch controller has a dead interrupt line, and everything here exists because of it.** A screen came loose while the Pi was powered and the ADS7846's PENIRQ output is now stuck permanently LOW. Proven both ways on the same pin: GPIO25 reads `lo` with the panel attached and `hi` with it removed, so the Pi side is undamaged — the failure is at the panel end.

The kernel's `ads7846` driver is purely interrupt-driven and the IRQ is **edge**-triggered, so a permanently-low line never produces an edge and the driver is never woken. `/proc/interrupts` showed **0** for `ads7846` on every CPU since boot. That is why no reboot and no reseat ever helped — nothing was wrong with the software, and nothing a restart could fix.

**But the chip itself is fine.** Bound to `spidev` and read directly, it returns real pressure and coordinates that track a finger across the whole panel. Only the interrupt output died — and that signal is merely an optimisation ("a finger arrived, go measure"). Ask the chip on a timer instead of waiting to be told, and the panel works normally. That is `demiurge-touch-poll`.

```ini
touch_poll = on         # default on; off leaves the panel dead
```

The daemon owns `/dev/spidev0.1` and republishes the panel through **uinput** as an ordinary absolute-XY touch device. Every consumer — the NEPTR UI's `touch_evdev.py`, the `demiurge-touch` MIDI daemon, the `demiurge-touch-test` calibrator, SDL — reads `/dev/input/eventN` exactly as before and needs **no code change**. None of them are aware polling is involved.

The uinput device deliberately keeps the **exact name** of the kernel device it replaces, `ADS7846 Touchscreen`, because two persistence layers are keyed on that string and would silently stop applying if it changed: the calibrator-staged `/etc/udev/hwdb.d/61-demiurge-touch-cal.hwdb` (`evdev:name:ADS7846 Touchscreen:*`) and `touch-cal.conf`'s `device =` line. Keeping the name means an existing 5-point calibration survives untouched. The advertised axis ranges, axis set, `BTN_TOUCH`-only key set and `PROP=0` all mirror the old device. The giveaway for anyone debugging is `Phys`:

```
N: Name="ADS7846 Touchscreen"
P: Phys=demiurge-touch-poll/spi0.1     # kernel driver was: spi0.1/input0
```

**Owning the SPI device.** `config.txt` comments out the `dtoverlay=ads7846,…` line, which re-enables the `spidev@1` node that overlay was disabling, so `/dev/spidev0.1` exists from boot and the useless kernel driver never loads. The daemon *also* self-heals if it finds `ads7846` squatting on `spi0.1` (a reverted or reflashed boot partition): it unbinds, sets `driver_override=spidev`, and binds. The override is **required** — a bare bind is rejected with `ENODEV` because the devicetree node's compatible is `ti,ads7846`, not `spidev`.

**Cost.** ~0.4 % of one core idle. The unit sets `CPUAffinity=0-2` to keep the daemon off the audio engine's isolated CPU 3, at normal priority — never realtime.

**Tuning.** `demiurge-touch-poll` prints status; `--probe` shows live raw readings with the touch verdict per sample, which is the tool for re-tuning `zmin`/`rmax`/`margin` on a different panel. Before changing thresholds, read the two counter-intuitive notes in the table above: a *higher* `touch_poll_zmin` makes swiping worse, and it is `touch_poll_margin` (not pressure) that rejects phantom corner touches.

### The touch section (touchscreen → MIDI CC)

When a touch panel is attached, `demiurge-touch.service` can turn it into a MIDI controller on the shared "Midi Through" pool (client 14:0) — the same bus every stage's `-M 14:0` already reads:

```ini
touch         = on      # default off
touch_cc_base = 80
touch_channel = 16      # the clock/utility channel (CC116-119 live here too)
touch_slots   = 1       # resistive panel = single-touch; 4 for capacitive
```

The CC map, per slot *i* (0–3), with `base = touch_cc_base`:

| CC           | Meaning                                             |
|--------------|-----------------------------------------------------|
| `base + i*3`     | X position, 0–127 scaled from the panel's axis range |
| `base + i*3 + 1` | Y position, 0–127                                |
| `base + i*3 + 2` | pressure, 0–127 (`0` if the panel has no pressure axis) |
| `base + 12 + i`  | touch state: `127` on touch-down, `0` on release |

With the default base 80 that is CC80–91 (X/Y/P × 4 slots) + CC92–95 (state), all on channel 16 — verified collision-free against the Teensy map (ch1: CC20–36, 60–79) and the clock block (ch16: CC116–119). In csound: `kx ctrl7 16, 80, 0, 1`.

The daemon (`demiurge-touch --daemon`, `/opt/demiurge/bin`) reads the evdev device directly (auto-detected the same way as `demiurge-stream-touch`), speaks full multi-touch protocol B *and* plain single-touch (`ABS_X/Y` + `BTN_TOUCH` → slot 0), and emits change-only CCs through a ±1-CC deadband so resistive wobble doesn't spam the channel. Output is a virtual ALSA-seq port ("DEMIURGE Touch") that the daemon `aconnect`s into 14:0 itself. `touch = off` (the default) makes the daemon exit cleanly — the same self-gate pattern as `ui = off`. Toggle from anywhere with **`demiurge-touch on` / `off`** (writes the key, bounces the service); `demiurge-touch` alone prints status, `--status-machine` a parseable line for UIs (the web UI's TOUCH tile appears only when a panel is detected). Verify the stream with `aseqdump -p "Midi Through"` while touching. Panel test/calibration visual: `demiurge-touch-test` (see its header for how to borrow the screen from `neptr-ui`).

#### Touch calibration (`~/demiurge/touch-cal.conf`)

Resistive panels are never quite aligned with the LCD. `demiurge-touch-test` doubles as a smartboard-style **calibrator**: it auto-runs calibration when no calibration file exists, with `--calibrate`, or from the mirror's top-right **CAL** corner (hold 1.5 s). Five targets (4 corners inset 40 px + center), tap-and-hold each; RAW evdev coordinates are averaged per target (any existing calibration is bypassed while calibrating), unsteady presses are re-asked, and a wild outlier point is re-asked after the affine fit. A verify screen then tracks your finger **with the calibration applied** — tap **✓ to save, ✗ to redo**. Cancel any time (ESC, or press-and-hold anywhere ≥4 s): the file is written **only on a completed run confirmed with ✓** — aborting, crashing or stopping the service never touches an existing calibration.

The file lives next to `live.conf` (`~/demiurge/touch-cal.conf`), survives reboots, and is plain `key=value`:

```ini
version   = 1
swap_xy   = 0         # 1 = panel X drives screen Y (rotated mount)
raw_x_min = 213.0     # RAW evdev value at the LEFT screen edge
raw_x_max = 3891.0    # ... at the RIGHT edge (min > max = axis inverted)
raw_y_min = 287.0     # RAW value at the TOP edge
raw_y_max = 3856.0    # ... at the BOTTOM edge
width     = 800       # screen the run was made on (informational)
height    = 480
```

Mapping: optional axis swap, then per-axis linear `norm = (raw − raw_min) / (raw_max − raw_min)`, clamped 0–1 — offsets, scales, swap and inversion in four numbers per axis, so it survives future rotation changes. Delete the file to fall back to the raw kernel range.

**Who applies what (the layering):**

| Layer | Consumer | Mechanism |
|-------|----------|-----------|
| `touch-cal.conf` | `demiurge-touch` daemon (MIDI CCs) | Loads it at start and **hot-reloads** on file-mtime change (checked ~2 s) or `SIGHUP` — a fresh calibration takes effect without a service restart. |
| `touch-cal.conf` | `demiurge-touch-test` mirror mode | Applied live, so "crosshair under finger" is directly verifiable. |
| udev hwdb `EVDEV_ABS` | **SDL apps — the NEPTR UI** | SDL's kmsdrm/evdev backend scales finger coords by the *kernel-advertised* axis range and knows nothing about our file. On ✓ the calibrator stages `/etc/udev/hwdb.d/61-demiurge-touch-cal.hwdb` (via `sudo -n` + `systemd-hwdb update` + `udevadm trigger`), which rewrites the kernel-advertised min/max — effective for every device open after that (e.g. the next `neptr-ui` start) and persistent across reboots. Not expressible for swap/inversion (skipped with a note in that case). |
| ads7846 overlay `xmin=`/`xmax=`/… | *(nothing)* | **Ignored by the current kernel** — verified: `config.txt` carries `xmin=200,xmax=3900,…` yet the kernel advertises 0–4095. The equivalent overlay line is only printed and written to `~/demiurge/touch-cal-overlay.txt` for reference; `config.txt` is deliberately not edited. |

The NEPTR UI itself is never modified (byte-parity with the Mac copy) — it simply benefits from the corrected kernel range via the hwdb layer.

#### UI position (`~/demiurge/ui-state.conf`)

Where the on-device NEPTR UI was when it last stopped, so a `systemctl restart neptr-ui` or a reboot comes back to the same screen instead of the default one. Same `key = value` shape as everything else, written by the UI itself (atomic tmp+rename, debounced 2 s, flushed on SIGTERM), and **deliberately not in `live.conf`** — that file is overwritten wholesale by `demiurge-set` and has lost non-audio keys before.

```ini
view             = menu6   # 'menuN' or an effect code
menu             = 6       # menu an effect view sits in
companion        = 0       # companion menu open?
companion_screen = 1       # DASHBOARD/PATCHES/POWER/AUDIO/WAVE
companion_sel    = 3
scroll_patches   = -1      # -1 = window follows the cursor
scroll_audio     = -1
```

It holds **UI position only**. Effect on/off, parameter values, preset bank, looper transport and NAM/IR selections are the *engine's* state: the engine persists them itself (`state_persist.orc`) and re-broadcasts them over OSC when the UI asks (ch16 CC102 — see `engine-ui.md`). Delete the file to start from defaults; a corrupt one is ignored key-by-key and then wholesale, never fatal.


---

## `/boot/firmware/demiurge.conf`

The boot-partition file. Mount the SD card on any computer and edit it — useful when you've broken `live.conf` and can't SSH in.

### `launch`

```ini
launch = /home/gorpi7/demiurge/live.conf
```

`launch` resolves in this order:

1. **An absolute path** to a live.conf — the launcher loads it and starts the file watcher.
2. **Empty / unset** → fall back to `~/demiurge/live.conf`.

That's it. `live.conf` is the single user-facing surface; if you want an alternate show-file, point `launch =` at a different `live.conf` (show directories typically ship their own `live.conf` next to their source files, e.g. `~/NEPTR_phase4/live.conf`).

Wired audio devices — USB audio class and platform/I2S HATs (e.g. HiFiBerry) alike — are aggregated macOS-style automatically: smaller-channel-count interfaces go first (FL/FR), additional interfaces fill RL/RR on `demiurge-sink`. HDMI is listed but never auto-adopted (explicit selection or `hdmi_default = on` only; WirePlumber policy demotes HDMI nodes to zero priority with autoconnect off — enabled so explicit selection works, never picked by WirePlumber itself; see `/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf`), Bluetooth input is ignored, and the snd-aloop card is reserved as the fallback clock. In bypass mode, a selected card with no capture PCM (HDMI, playback-only USB DACs) runs output-only: the launcher exports `DEMIURGE_AUDIO_CAPTURE=none`, the csound wrapper substitutes `-iadc:null` (ALSA null plugin), and input opcodes (`ins`/`inch`) read silence.

---

## Example sessions

See `~/demiurge/examples/` on the Pi:

- `hello_world/` — one standalone generative file per language. Drop any of them into `live.conf`'s `chain =` list to hear it.
- `hello_patching/` — building blocks for the reference chain. The default `live.conf` wires several of these in series. Swap any of them for a different file in the same folder (`faust_crush.dsp`, `csound_reverb.csd`, `sc_grain.scd`, …) and save to hear the change.

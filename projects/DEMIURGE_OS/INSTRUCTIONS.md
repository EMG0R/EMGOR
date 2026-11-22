# DEMIURGE — Installation & Recreation Guide

Everything you need to rebuild DEMIURGE on a fresh Raspberry Pi 5. Written concept‑first: each section explains *what* the piece is and *why* it exists, then gives the commands to install it. If you had nothing but this file plus the `config/`, `src/`, and `demiurge/` trees, you could reconstruct the whole system.

Per‑language peculiarities (ChucK flags, Faust quirks, etc.) live in `demiurge/docs/language-setup.md` — they ship on the Pi next to the code they describe. The clock protocol lives in `demiurge/docs/clock.md`. This file is the build side.

---

## Contents

0. Hardware & base OS
1. Foundation (Pi OS Lite + system packages)
2. PipeWire replaces PulseAudio & JACK
3. The virtual audio layer — `demiurge-sink`
4. The shared MIDI bus — `Midi Through`
5. Audio languages + the `libjack` collision
6. The launcher
7. The global clock
8. The Pd clock bridge
9. The Strudel runner
10. Systemd service + boot config
11. M8 — the Pi 5 performance pack
12. Failure modes & how to escape them
13. Verification checklist
14. Reference: paths, ports, client names

---

## 0. Hardware & base OS

- **Raspberry Pi 5.** The M8 performance pack is Pi 5‑specific (clock, voltage, core‑isolation). Pi 4 can run the base system but none of the overclock numbers apply.
- **Active cooling.** Official Pi 5 Active Cooler or equivalent. Mandatory if you enable the overclock. No heatsink = no M8.
- **Official 5V/5A USB‑C PSU.** A 3A phone charger browns out under sustained load and the Pi silently under‑volts.
- **An audio interface.** Any of: a class‑compliant USB interface (any brand), an I2S HAT (any brand), HDMI, or an on‑SoC codec. Nothing in DEMIURGE is specific to a vendor or a bus — the launcher discovers devices at runtime, the interface resolver selects by name, and the performance layer derives the audio‑pacing IRQ from whatever is actually attached. With no interface at all the system still runs against the always‑on loopback card.
- **USB MIDI controller (optional).** Any class-compliant USB MIDI device.
- **SD card.** Pi OS Lite, Bookworm (Debian Trixie), 64‑bit. Flash with Raspberry Pi Imager; enable SSH and set your username and hostname in the advanced options.

> **Your names:**
> - Username: `gorpi7` in the examples below — set in Raspberry Pi Imager. You do NOT need to find-and-replace the repo to use a different one: `setup-script/setup-demiurge.sh` reads it from one `PI_USER` variable (defaults to whoever is logged in and runs the script) and templates every systemd unit's `User=`/`Group=`/`HOME=` from it at install time.
> - Hostname: `demiurge` / `demiurge.local` in the examples below — set in Raspberry Pi Imager.
>
> Mac-side scripts that reach the Pi over SSH (`neptr/scripts/sync_to_pi.sh`, `pi-monitor/`, `setup-script/deploy-*.sh`) read their default user@host from `pi-rig.conf` at the repo root — edit that one file once, or set `PI_USER`/`PI_HOST` env vars, to point every one of them at a renamed rig.

First contact:

```bash
ssh <your-pi-user>@<your-pi-host>          # e.g. ssh gorpi7v2@demiurge.local
ssh-copy-id <your-pi-user>@<your-pi-host>   # stop typing passwords
```

---

## 1. Foundation

Everything else builds on top of these packages, so bring them in first. The critical bit is putting the user into the `audio` group — JACK/PipeWire RT threads (§11) refuse to negotiate priority for non‑audio users.

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y \
    git build-essential cmake pkg-config \
    alsa-utils nano htop \
    sox ffmpeg libsox-fmt-all \
    liblo-dev liblo-tools
sudo usermod -aG audio "$USER"
```

Optional extras: add the user to `video gpio spi i2c input` if you plan to touch peripherals.

---

## 2. PipeWire replaces PulseAudio & JACK

DEMIURGE is built on a single realisation: **PipeWire is the entire audio OS**. It owns ALSA hardware exclusively, speaks JACK natively through `pipewire-jack`, and drives PulseAudio clients through `pipewire-pulse`. Every audio language — Csound, Pd, SC, ChucK, Faust, C++ — thinks it's talking to JACK, but there is no `jackd` anywhere on the system. The shim at `/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack/libjack.so.0` is what they actually load.

This gives DEMIURGE three things for free: hot‑plug resilience (PipeWire survives unplugged devices without dropping clients), unified routing (`pw-link` is the single source of truth), and one system to tune for latency instead of two.

```bash
sudo apt install -y pipewire pipewire-alsa pipewire-jack wireplumber pipewire-pulse
sudo apt remove -y pulseaudio pulseaudio-module-bluetooth 2>/dev/null || true
systemctl --user --now enable pipewire pipewire-pulse wireplumber
```

Verify it's actually running and seeing the hardware:

```bash
systemctl --user status pipewire wireplumber    # both active
pw-cli ls Node                                    # lists hardware nodes
speaker-test -D pipewire -c 2 -r 48000            # should play through default sink
```

---

## 3. The virtual audio layer — `demiurge-sink`

**The core innovation of DEMIURGE.** Every audio language points at a virtual node called `demiurge-sink`. That node always exists. Hardware can come and go — unplug the interface mid‑performance, replug it, swap it for a different one — and the running programs never see a thing. PipeWire's `libpipewire-module-loopback` loads a pair of nodes: the sink itself (apps write here) and a passive stream‑output that WirePlumber links to whatever physical device is currently the default. When the hardware disappears the link dies but both nodes persist with `node.linger=true`. The source side mirrors this for capture.

This is the level of abstraction that makes DEMIURGE feel like an instrument rather than a config puzzle: the programs only know about `demiurge-sink`, and the hardware is the OS's problem.

Install the loopback module config:

```bash
sudo install -d /etc/pipewire/pipewire.conf.d
sudo install -m 0644 config/pipewire/demiurge-virtual.conf \
    /etc/pipewire/pipewire.conf.d/demiurge-virtual.conf
systemctl --user restart pipewire wireplumber
pw-cli ls Node | grep demiurge        # demiurge-sink + demiurge-source present
```

(The full module config is in `config/pipewire/demiurge-virtual.conf` — two `libpipewire-module-loopback` blocks, one for sink, one for source, each declaring the pair of nodes with `FL/FR` position and `node.linger=true`.)

**Three subtleties worth understanding now, not when you're debugging at 2am:**

1. **`demiurge-sink` vs `demiurge-sink-output`.** `demiurge-sink` (`Audio/Sink`) is what apps write to — target port `demiurge-sink:playback_FL/FR`. `demiurge-sink-output` (`Stream/Output/Audio`) is the other side of the loopback and is what feeds the physical device — source port `demiurge-sink-output:output_FL/FR`. Never confuse them when reading `pw-link -l`.

2. **Do NOT make `demiurge-sink` the default sink.** The loopback's stream‑output follows the current default sink. If the default *is* `demiurge-sink`, its stream‑output targets its own input, creates a cycle, and PipeWire silently drops the path — all audio disappears. Keep the physical interface as default. Route to `demiurge-sink` explicitly through the launcher.

3. **`wpctl status` hides it.** Because it's a loopback filter node, `demiurge-sink` appears under **Filters**, not **Sinks**. It is still a perfectly valid sink — `PIPEWIRE_NODE=demiurge-sink somecmd` works — but `wpctl status | grep Sinks` won't show it. Use `pw-link -i | grep demiurge` to inspect.

A cleaner long‑term fix for (2) is a WirePlumber rule pinning `demiurge-sink-output.target.object` explicitly to the selected interface. Not yet implemented; the launcher's explicit `pw-link` calls work around it.

The unused `config/pipewire/demiurge-midi.conf` is a PipeWire MIDI loopback from an earlier design — DEMIURGE does not use it. MIDI goes through kernel ALSA seq instead (§4). The file is kept in‑tree as a reference for anyone who wants to explore the PipeWire MIDI graph path.

---

## 4. The shared MIDI bus — `Midi Through`

Every DEMIURGE program reads *and* writes one well‑known MIDI port: `Midi Through`, ALSA seq client `14:0`. This is the kernel's `snd-seq-dummy` virtual merge port — it exists on stock Bookworm without any DEMIURGE code. The only thing DEMIURGE adds is a small helper that auto‑merges every physical MIDI source into it on hot‑plug, so a controller plugged in after boot still joins the pool.

Why kernel ALSA seq instead of a PipeWire MIDI loopback? Because every audio language on Linux already speaks ALSA seq. Csound, ChucK, SC, Pd (via the sidecar in §8), Python — they all open ALSA seq ports natively. PipeWire's own MIDI graph would work, but would mean writing a new reader per language. Piggybacking on kernel seq is zero new code per language, and the kernel port never dies.

The merge helper and its udev rule:

```bash
# /usr/local/bin/demiurge-midi-connect.sh
#   sleep 1
#   aconnect -l | grep -oP 'client \K\d+(?=.*\[type=kernel,card)' | while read c; do
#       aconnect "$c":0 14:0 2>/dev/null || true
#   done
# /etc/udev/rules.d/99-demiurge-midi.rules
#   SUBSYSTEM=="sound", ACTION=="add", KERNEL=="midiC*D*", RUN+="/usr/local/bin/demiurge-midi-connect.sh"
```

(Both files are inlined into `setup-script/setup-demiurge.sh` Phase 4 — that is the canonical source for them.)

```bash
sudo udevadm control --reload-rules
/usr/local/bin/demiurge-midi-connect.sh   # run once for already-plugged devices
aconnect -l                               # verify Midi Through at client 14
```

`snd-virmidi` is **not** used — PipeWire owns raw MIDI devices exclusively and `snd-virmidi` loads silently without effect.

---

## 5. Audio languages + the `libjack` collision

DEMIURGE pre‑installs every major live‑coding audio language. Compilers and runtimes come from apt; per‑language port names, MIDI conventions, and footguns are documented in `demiurge/docs/language-setup.md`.

```bash
sudo apt install -y \
    csound \
    puredata puredata-utils \
    supercollider supercollider-language supercollider-server supercollider-supernova \
    chuck \
    faust \
    python3-venv python3-pip \
    libasound2-dev libjack-jackd2-dev
```

`libasound2-dev` is for the ALSA seq MIDI side of C++ clients. `libjack-jackd2-dev` is required so `faust2jackconsole` can build Faust stages — and installing it is the single biggest footgun in the whole system.

**The `libjack` collision.** `libjack-jackd2-dev` pulls in `libjack-jackd2-0`, which places jackd2's real `libjack.so.0` in `/usr/lib/aarch64-linux-gnu/`. From that moment on, *any* JACK‑linked program the dynamic loader touches will prefer jackd2's libjack over PipeWire's shim — and sit forever waiting for a `jackd` server that doesn't exist. The failure is silent: the process runs, registers no JACK client, produces no audio, prints nothing useful.

**The fix is `LD_LIBRARY_PATH`.** DEMIURGE's launcher prepends `LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack` to every audio‑language invocation. That forces PipeWire's shim to win the symbol lookup. `update-alternatives` does not manage `libjack.so.0` on Trixie and `pw-jack` wraps things unreliably on this hardware — `LD_LIBRARY_PATH` is the only path that always works.

**C++ stages** are compiled on the Pi (the launcher builds them on‑demand from a `.cpp` sitting next to the missing binary):

```bash
g++ -O2 -o cpp_crush cpp_crush.cpp -ljack -lasound -lm -lpthread
```

**Strudel (headless)** is Node‑based and needs a one‑time install in `~/demiurge/strudel/`:

```bash
cd ~/demiurge/strudel && npm install
```

`package.json` pins `@strudel/*` to **1.2.0** exactly. Do not bump to 1.2.6+ — it pulls `@kabelsalat/web@0.4.1` which dropped the `SalatRepl` export and breaks the whole import chain. Strudel's MIDI I/O goes through `easymidi` (which wraps `@julusian/midi`, an RtMidi ALSA seq backend); `jzz` does not work here because it only speaks raw MIDI and can't see seq ports.

Details of how each language integrates with the bus, which ports it registers, and which flags to use live in `demiurge/docs/language-setup.md`.

---

## 6. The launcher

`src/demiurge-launcher-rs/` is a zero‑dependency Rust crate (stdlib only) that compiles to a ~460 KB stripped binary installed at `/opt/demiurge/bin/demiurge-launcher`. It runs as user `gorpi7` under `demiurge.service`, reads one config file, and does everything the system needs to bring a patched audio graph up — event‑driven, idempotent, and resilient to USB audio hot‑plug. This is the *only* launcher — there is no shell fallback and no alternate binary. Every launch path on the Pi runs through this one process, and USB aggregation is handled inside it by `aggregate::apply` rather than any sidecar.

**Model.** The launcher's primary input is `~/demiurge/live.conf` — a user‑facing file that's polled once a second and hot‑reloaded on save. The boot‑partition file `/boot/firmware/demiurge.conf` contains exactly one interesting line:

```ini
launch = /home/gorpi7/demiurge/live.conf
```

Resolution at boot is exactly two options:

1. `launch = <path>` in `/boot/firmware/demiurge.conf` if set (must point at a `live.conf`).
2. Otherwise `~/demiurge/live.conf`.

There is no third fallback, no classic session format, and no raw‑audio‑file launch path. `live.conf` is the only user‑facing config format — full stop. If a feature can't be expressed in `chain =` / `midi =`, extend `src/live.rs` so the simple syntax keeps working.

**`live.conf`** (`src/demiurge-launcher-rs/src/live.rs`) is the day‑to‑day interface:

```ini
link = off
bpm  = 110

chain =
  ~/demiurge/examples/hello_world/hello.strudel
  ~/demiurge/examples/hello_patching/faust_delay.dsp
  ~/demiurge/examples/hello_patching/cpp_crush.cpp
  ~/demiurge/examples/hello_patching/csound_ringmod.csd

midi =
  ~/patterns/driver.strudel
```

- `link = on/off` toggles Ableton Link; on save, `demiurge-clock` is respawned with the new flag.
- `chain =` is an ordered list of audio stages, piped top‑to‑bottom. Language is inferred from the file extension. `.dsp` and `.cpp` are compiled on demand. The chain always terminates at `out` (`demiurge-sink:playback_FL/FR`) automatically; `in1 ->` / `in2 ->` as leading lines pipe hardware mono inputs into the first stage's matching input channel. Hardware inputs are always addressed by number — stereo input pairs don't exist on DEMIURGE.
- Multiple `chain =` blocks run in parallel. Each block terminates at `out`, and `demiurge-sink` sums them for free (multiple `pw-link` edges into the same playback port are mixed). Use this for a plucky lead path alongside a reverb‑drenched pad path without routing one through the other.
- `midi =` is an optional list of sidecars that join the MIDI bus but are not wired into the audio graph (pattern drivers, Python controllers, …).
- Implicit programs the user never lists: `demiurge-clock` (always) and `pd_clock_bridge.py` (when any stage is `.pd`).
- A fenced `# DEMIURGE-DEVICES-BEGIN … END` block near the top of the file is rewritten by the launcher with the current audio/MIDI device inventory. It's rewritten only when `live.conf` has been idle for several seconds so in‑progress edits aren't clobbered. The writer uses an mtime check‑and‑abort pattern: read current mtime, write a tempfile, verify mtime is still unchanged, atomic rename; otherwise abort and retry later.

**Hot reload.** The launcher diffs the new live config against the running supervisor state and reconciles by `(id, file)`:

- Unchanged stages keep their PID — no restart, no xruns on neighbours.
- New stages are launched and wired into the graph.
- Removed stages are terminated and unlinked.
- A changed `link =` triggers a clock subprocess respawn.
- `bpm =` changes are sent to `demiurge-clock` as `CC118` steer messages (no restart).

Each reconcile pass reruns `graph::apply` and `midi::wire_bus`, so the scrub passes (see below) still enforce the declared graph as authoritative.

**Internal session graph.** `src/live.rs::to_session` translates the parsed `live.conf` into a typed `Session { programs: Vec<Program>, edges: Vec<Edge> }` in `src/config.rs`. These types exist only so the rest of the launcher (`graph::apply`, `supervisor::reconcile`, `midi::wire_bus`) has something uniform to operate on — users never see them. The reserved edge endpoints are `in1` / `in2` (mono hardware inputs, `__Mic{N}__source:capture_MONO`) and `out` (`demiurge-sink:playback_FL/FR`). Implicit `clock` and `pdclock` programs are injected here. If you add a live.conf feature, extend `live.rs`; do not add a second user‑facing file format.

Full live syntax is in `demiurge/docs/config-reference.md`.

**Language dispatch.** The launcher knows how to start every language. Every invocation is prefixed with `LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack` (see §5) and `PIPEWIRE_PROPS="node.autoconnect=false node.dont-reconnect=true"` — that second env var prevents WirePlumber from auto‑linking each new node to the default sink in parallel with the declared patch graph, which would otherwise defeat the whole model.

| `lang` | command |
|---|---|
| `csound` | `csound -d -+rtaudio=jack -odac -iadc --realtime <file>` |
| `chuck` | `chuck --driver:jack --out:2 --in:2 --srate:48000 --bufsize:128 <file>` |
| `pd` | `pd -nogui -jack -alsamidi -r 48000 -blocksize 64 -audiobuf 5 <file>` |
| `sc` / `sclang` | `QT_QPA_PLATFORM=offscreen sclang <file>` |
| `faust` | runs the precompiled binary directly (never the `.dsp` source) |
| `cpp` | runs the precompiled binary; builds from `<file>.cpp` on demand if missing |
| `clock` | runs `/opt/demiurge/bin/demiurge-clock`; skips JACK port resolution entirely |
| `strudel` | `node ~/demiurge/strudel/strudel-runner.mjs <file>` with `NODE_PATH=~/demiurge/strudel/node_modules` |
| `python` | `/opt/demiurge/venv/bin/python <file>` if the venv exists, else `/usr/bin/python3` |
| `auto` | extension → language, then falls through to one of the above |

`csound` does NOT get `--sched` — on Pi 5 without `CAP_SYS_NICE` it exits with "cannot set scheduling policy to SCHED_RR". `--realtime` alone is enough for low‑latency operation.

**Client resolution.** After each `launch_prog` returns, the launcher polls `pw-link -o` up to 20 × 0.5 s for the expected JACK client name to appear:

| lang | candidate client names |
|---|---|
| csound | `csound6`, `csound`, `Csound` |
| chuck | `ChucK`, `chuck`, `Chuck` |
| pd | `pure_data` |
| sc | `SuperCollider` |
| faust | basename of the binary |
| cpp | basename of the binary, or `cpp_<basename>` |
| strudel | `cpal_client_out` (audio mode) — or empty in `--midi` mode |
| clock | ALSA seq only: records `demiurge_clock` as the client name and skips JACK polling |
| python | `python` |

If the process dies before registering (`-alsa` silent failure, a compile error, a Python exception) the launcher logs `resolve_client: FAILED` and moves on. The rest of the graph still comes up.

**Port pair matching.** JACK clients use different stereo output conventions. The launcher tries pairs in this order for outputs: `output_FL|output_FR`, `output_1|output_2`, `out_0|out_1`, `out_1|out_2`, `out_l|out_r`, `outL|outR`, `outport 0|outport 1`. Inputs: `input_FL|input_FR`, `input_1|input_2`, `in_0|in_1`, `in_1|in_2`, `in_l|in_r`, `inL|inR`, `playback_FL|playback_FR`, `inport 0|inport 1`. **Both halves of a pair must exist** — an earlier version accepted the left half alone and latched ChucK's `out_0|out_1` as `out_1|out_2`, which didn't exist. Pairs are encoded as `"left|right"` strings because JACK port names can contain spaces — ChucK emits `"outport 0"` and `"outport 1"` literally.

**Patch graph application.** `apply_patch_graph` walks every declared edge and calls `pw-link` for both L and R channels. If the destination is dead it walks forward through the graph until it finds a live node or reaches `out` — this is the **graceful‑failure passthrough**: a chain `A → B → C` with B dead becomes `A → C`, and when B comes back the original graph is restored on the next state‑change tick.

**Three scrub passes** then enforce the declared graph as authoritative:

1. **Undeclared‑input scrub.** For every live program, compute the allowed upstream clients from the parsed patch edges. Walk the program's input ports in `pw-link -l` and disconnect any incoming audio link whose source isn't in the allowed set. Kills JACK/PipeWire heuristic cross‑links (e.g. Csound latching onto ChucK's outports as "nearest audio source").
2. **Hardware‑playback scrub.** Walk the physical playback ports (`alsa_output.*:playback_FL/FR`) and disconnect anything that isn't `demiurge-sink-output:*`. Kills `faust2jackconsole`'s habit of auto‑connecting its outputs straight to system playback, which otherwise leaks middle‑of‑chain audio directly to the speakers and bypasses every downstream stage.
3. **Sink‑feedback scrub.** Walk every live session client's input ports and disconnect any incoming link whose source is `demiurge-sink-output:*`. Only `demiurge-sink-output → alsa_output.*` is legitimate; anything else closes a feedback loop (sink → sink‑output → chain program → sink → …) that drives csound / ringmod stages into NaN. WirePlumber occasionally re‑auto‑links these on startup; the scrub catches them.

**Shared MIDI wiring.** `wire_midi_bus` first runs the physical‑MIDI merge helper (§4), then iterates every live program: matches its ALSA seq client by JACK‑client‑name (falling back to literal name), then `aconnect "Midi Through":0 <client>:0` and the reverse. Known limitation: ALSA seq names don't always match JACK names — ChucK's seq client is `"RtMidi Input Client"`, Csound's is `"Csound"` — so the launcher's wire step is largely cosmetic, and programs that need MIDI tend to subscribe themselves anyway.

**Faust MIDI bridge.** Faust stages built with `faust2jackconsole -midi` register a JACK MIDI input port but **cannot see ALSA seq directly**. PipeWire exposes `Midi Through` as `Midi-Bridge:Midi Through Port-0 (capture)` on the JACK graph, and `wire_faust_midi` links that source into every Faust client's MIDI input.

**Graph clock pinning.** Before the first aggregation pass and again on every `UsbDeviceChange`, `util::pin_clock_metadata()` writes `clock.force-rate=48000` and `clock.force-quantum = 128` into the PipeWire `settings` metadata object via `pw-metadata`. This is one of four lockstep defenses against a hot‑plugged USB device dragging the graph rate to 88.2/96/176.4/192 kHz — the others are `default.clock.allowed-rates = [48000]` in the PipeWire low‑latency conf, the WirePlumber USB rule that pins `audio.rate=48000` at the alsa-monitor level, and `audio.rate=48000` hardcoded inside both `demiurge-sink` / `demiurge-source` loopback definitions. Without this layered pin, a class‑compliant USB interface that was last used at 96 kHz on another machine returns at 96 kHz on the next plug, every loopback in the chain starts SRC'ing, and audio sounds glitchy/pitchy with no error log. See `gotchas.md` Q7c for the full story and the six places to update if you ever change either constant.

**Event loop.** After the initial bring‑up the launcher waits on a single `mpsc::Receiver<Event>` fed by three threads:

1. A `pw-mon` tail thread debounces USB node add/remove events into a single `UsbDeviceChange` per 750 ms quiet window (plus a 2 s startup drain so existing devices don't re‑trigger aggregation on boot).
2. A child‑watcher thread polls `/proc/<pid>` every 500 ms and posts `ChildExited(pid)` on death.
3. A 2 s heartbeat catches any drift the other two channels miss.

A `UsbDeviceChange` re‑runs aggregation (`aggregate::apply` — idempotent, grow‑only, diffs against current `demiurge-sink-output`/`demiurge-source-input` links before touching anything). A `ChildExited` flips the program's `dead` flag so the next graph tick walks around it. Either event triggers `graph::apply` + `midi::wire_bus` again. On SIGTERM the launcher kills every child cleanly and exits.

**USB audio aggregation.** `aggregate::apply` enumerates every `alsa_output.usb-*` / `alsa_input.usb-*` node, sorts smaller‑channel‑count interfaces first (macOS style), and lays successive devices into `demiurge-sink-output:output_FL/FR` then `output_RL/RR` (symmetrically for `demiurge-source-input`). HDMI, Bluetooth, and built‑in audio are ignored. The pass is idempotent — it computes `desired: HashSet<(src, dst)>`, diffs against current links, and applies only what changed. That's what lets hot‑plug events re‑run it safely without flapping the graph.

**Logs.** Three places, in priority order when something goes wrong:

- `journalctl -u demiurge -f` — the launcher's own decisions: parsing, launch, resolve_client, link/unlink/scrub, reconcile, child-exit. This is the first place to look when a stage doesn't show up in `pw-link -l` or the chain shape is wrong.
- `~/.demiurge/logs/<id>.log` — each program's stdout+stderr, truncated on every spawn so you always see the most recent run. This is where a dying stage's *real* error message lives — the launcher itself only logs `program 'foo' marked dead`. If a `.csd` / `.ck` / `.dsp` stage exits silently, read this file before doing anything else.
- `~/.demiurge/logs/<id>.compile.log` — Faust `faust2jackconsole` and C++ `g++` output for any stage that needed building. Only written for `faust` and `cpp` languages, only on rebuild. When a `.dsp` stage refuses to come up, this is where the compiler diagnostic lives.

Install:

```bash
sudo install -d /opt/demiurge/bin

# Rust launcher (zero deps, stdlib only, ~460 KB stripped)
cd src/demiurge-launcher-rs
cargo build --release
sudo install -m 0755 target/release/demiurge-launcher /opt/demiurge/bin/demiurge-launcher
cd ../..

# Per-language wrappers that bake in LD_LIBRARY_PATH + PIPEWIRE_PROPS
sudo install -m 0755 src/wrappers/demiurge-env.sh /opt/demiurge/bin/demiurge-env.sh
for w in src/wrappers/demiurge-run-*; do
    sudo install -m 0755 "$w" "/opt/demiurge/bin/$(basename $w)"
done
```

---

## 7. The global clock

`demiurge_clock` is a tiny C++ ALSA seq daemon. It opens a client named `demiurge_clock`, creates one input and one output port, and subscribes both ways to `Midi Through` at startup. Then it does two things:

1. **Tick.** Emits MIDI realtime `0xF8` at 24 PPQN using `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` with absolute deadlines (`start + n * tick_period`), so scheduler jitter never accumulates. Emits `0xFA` / `0xFC` / `0xFB` on transport changes.
2. **Broadcast BPM.** Once per beat it sends a CC on `ch16 CC119` with the current BPM mapped linearly into 0..127 (40..240 BPM). Every program on the bus reads this to stay in tempo. Nothing else ever writes to `CC119`.

The protocol uses the top of the CC space on channel 16 so it never collides with musical CCs:

| Event | Direction | Meaning |
|---|---|---|
| `0xF8` tick | clock → bus | 24 PPQN metronome |
| `0xFA` / `0xFC` / `0xFB` | clock → bus | start / stop / continue |
| `ch16 CC119` | clock → bus (1/beat) | BPM broadcast — read‑only for peers |
| `ch16 CC118` | any → clock | request new BPM (0..127 → 40..240) |
| `ch16 CC117` | any → clock | transport (≥64 start, <64 stop) |

Why `CC119` and `CC118` are separate: splitting read‑broadcast from write‑steer removes echo‑feedback entirely — the daemon never has to filter its own output. Any program can retune the clock by writing `CC118`, and the whole chain follows on the next beat.

The IN‑side subscription (`14:0 → in_port`) is the easy thing to forget. Without it, `CC118` steer messages travel happily on Midi Through but the clock never sees them, and tempo silently refuses to change. The binary handles both subscriptions itself in `main()`.

Per‑language reader code (ChucK, Pd, SC, Faust, Csound, C++, Strudel) and steer examples live in `demiurge/docs/clock.md`.

**Ableton Link (optional).** With `link = on` in `/boot/firmware/demiurge.conf` and the daemon built with `-DDEMIURGE_LINK`, `demiurge-clock` joins any Link‑speaking peer on the local network (Ableton Live, Max/MSP, iOS/Android music apps, many hardware boxes). Link becomes the outer authority: its session tempo and transport state are captured on every tick (`captureAppSessionState`) and mirrored into `g_bpm` / `g_playing`, and every `CC118` (steer) / `CC117` (transport) message commits back into Link (`setTempo`/`setIsPlaying` + `commitAppSessionState`). The usual `0xF8` / `ch16 CC119` / `0xFA` / `0xFC` broadcasts still fan out to every language on the MIDI bus — Link just wraps the whole session in a networked outer loop. Off by default.

Build + install:

```bash
# Standalone (no network sync)
g++ -O2 -o demiurge-clock src/demiurge-clock.cpp -lasound -lpthread

# With Ableton Link (requires ableton-link-dev from apt)
sudo apt install -y ableton-link-dev
g++ -std=c++17 -O2 -DLINK_PLATFORM_LINUX=1 -DDEMIURGE_LINK \
    -o demiurge-clock src/demiurge-clock.cpp -lasound -lpthread

sudo install -m 0755 demiurge-clock /opt/demiurge/bin/demiurge-clock
```

`setup-script/setup-demiurge.sh` picks the Link variant automatically when `ableton-link-dev` is installed.

---

## 8. The Pd clock bridge

Pd vanilla's `-alsamidi` backend uses raw ALSA MIDI and cannot see the shared ALSA seq bus. `demiurge/bridges/pd_clock_bridge.py` is a small Python sidecar that joins the shared bus via `mido` (rtmidi backend), watches `ch16 CC119` (BPM) and `ch16 CC117` (transport), and forwards them to Pd as FUDI over UDP 9997:

```
[netreceive 9997 1] → [route bpm]       → float  (BPM in 40..240)
                    → [route transport] → float  (0 | 1)
```

The launcher injects it automatically whenever any `chain =` stage is a `.pd` file — users never list it. Same supervisor lifecycle as every other program; exits cleanly on SIGTERM.

Dependencies: `mido` + `python-rtmidi`. Either install via apt (`python3-mido python3-rtmidi`) or in the DEMIURGE venv at `/opt/demiurge/venv/`.

---

## 9. The Strudel runner

Strudel has no official Node CLI, so DEMIURGE ships one: `demiurge/strudel/strudel-runner.mjs`. It wires `@strudel/core` + `/mini` + `/transpiler` + `/tonal` into a minimal repl and exposes two output modes:

- **Audio (default).** `node-web-audio-api` drives an `AudioContext` backed by `cpal` → PipeWire‑JACK. Each hap becomes a saw + envelope voice on the audio graph. The cpal client shows up as `cpal_client_out:out_0/out_1` and is wired into the patch graph by the launcher like any other audio source. This is how Strudel acts as an audio *head* in a `live.conf` chain — a pattern‑driven synth, not just a MIDI generator.
- **MIDI (`--midi`).** Emits notes on the shared `Midi Through` bus via `easymidi`, for sessions where Strudel is a pure pattern driver and the audio comes from downstream languages.

**Both** modes always listen on `Midi Through` for `ch16 CC119` and call `scheduler.setCps(bpm/60/2)` — one cycle = two beats — so Strudel tempo‑locks to `demiurge_clock` automatically. `0xFA` / `0xFC` follow transport.

Pinning is non‑negotiable: `package.json` locks `@strudel/*` to `1.2.0`. One‑time install on the Pi:

```bash
cd ~/demiurge/strudel && npm install
```

---

## 10. Systemd service + boot config

`demiurge.service` runs the launcher as user `gorpi7` at boot, with RT limits (`LimitRTPRIO=99`, `LimitMEMLOCK=infinity`, `LimitNICE=-20`) set at the unit level so the launcher — and every child process it spawns — inherits a full real‑time budget. `After=sound.target user@1000.service` delays startup until the user session bus is up; the launcher needs `XDG_RUNTIME_DIR=/run/user/1000` to reach the user PipeWire.

`KillMode=mixed` + `KillSignal=SIGTERM` + `TimeoutStopSec=5` are set on the unit so `systemctl restart demiurge` cleanly terminates the entire cgroup — the launcher plus every child program it spawned. Without this, child audio programs survive launcher restarts and race with the new instance for JACK client names.

Two boot‑time files decide what runs:

- **`/boot/firmware/demiurge.conf`** — the boot‑partition pointer. One `launch = <abs-path>` line. Default points at `/home/gorpi7/demiurge/live.conf`. Leave it blank and the launcher falls back to `live.conf` automatically. Mount the SD card on any computer to edit — the boot partition is FAT32 and every OS can read it.
- **`~/demiurge/live.conf`** — the hot‑reloadable day‑to‑day interface. Edit it by SSH on the running Pi and save; the launcher's file‑watcher thread picks up the mtime change within a second, parses the new config, and reconciles the running supervisor state. See §6 for the full hot‑reload semantics, `demiurge/docs/config-reference.md` for the full syntax.

Drop-ins in `config/systemd/demiurge.service.d/` are installed by glob. Two matter for audio integrity:

- `10-clock-lockstep.conf` — `ExecStartPre=-demiurge-quantum-limits lockstep` (rate and quantum from `live.conf` to the device fragments, before any audio exists) and `ExecStartPre=-demiurge-mixer restore` (codec gains from `~/demiurge/asound.state`). Both are `-` prefixed: they can never block the service.
- `20-state-ram.conf` — `demiurge-state-ram` seeds the tmpfs state dir at start. csound uses `/dev/shm/neptr-state` when it exists, else `~/demiurge/state`. Without it the settings table is written to the SD card from the audio thread every 2 s (measured 2.3 to 8 ms per write against a 1.33 ms period).

`demiurge-capture-guard` is installed but **disabled**: its stall theory was disproved (memory: adc-capture-dma-stall) and a false positive stops the engine and rebinds the codec.

Install the service, the boot config template, and the staging tree:

```bash
sudo install -m 0644 config/demiurge.service /etc/systemd/system/demiurge.service
sudo systemctl daemon-reload
sudo systemctl enable demiurge.service

sudo install -m 0644 config/demiurge.conf.default /boot/firmware/demiurge.conf

mkdir -p ~/demiurge
rsync -a --delete --exclude '.DS_Store' demiurge/ ~/demiurge/
```

---

## 11. M8 — the Pi 5 performance pack

M8 ("Milestone 8 — Max Performance") is the set of tunings that take DEMIURGE from "runs fine at default latency" to "zero xruns at 128/48k under load." It layers five concerns on top of the base system:

1. **RT limits for `@audio`.** Every member of the audio group gets `rtprio 99`, `memlock unlimited`, `nice -20`. These apply at next login. PipeWire's realtime threads refuse to elevate priority without them.
2. **PipeWire quantum.** `default.clock.quantum = 128` at `48000` Hz (≈2.67 ms per hop) — **and** `clock.force-quantum = 128` pinned via `pw-metadata` in the `demiurge.service` `ExecStartPre` drop-in so it's still 128 after WirePlumber restart. Min‑quantum 32, max 128. The JACK bufsize ≡ quantum lockstep rule means every JACK client's internal block size must match this exactly — since 2026-10-02 you do not update six places: set `rate` / `quantum` in `live.conf` and `demiurge-quantum-limits lockstep` generates the device period, rate and clock fragments (see `gotchas.md` Q7c).
3. **Backup kit.** The SD card is not safe storage. `setup-script/backup-rig.sh` (Mac, pull-only, copy-only) mirrors `~/demiurge/*`, the user audio fragments, the flushed RAM state and the system files into `rig-backup/latest/<host>/`. The installer seeds a card with no `live.conf` from there; `setup-script/restore-rig.sh` pushes user files back and prints the sudo lines for system files. See `rig-backup/README.md`.
3b. **Patched `librtjack.so`.** `config/csound/librtjack.so` fixes a stack smash in csound 6.18's JACK backend on port names longer than 63 characters, which pro-audio port names exceed. The installer swaps it in (original kept as `.orig`) via `setup-script/deploy-rtjack.sh`.
4. **CPU governor pin.** All four cores pinned to `performance` at boot by a oneshot service firing *before* `demiurge.service` — otherwise `ondemand` would ramp down between audio bursts and add tens of microseconds of jitter.
4. **Thermal watchdog.** A oneshot + timer pair fires every 10 s. It logs `vcgencmd measure_temp` and the decoded throttle flags (`undervolt_now`, `arm_capped_now`, etc.) to the journal with `INFO` / `WARN` / `CRITICAL` levels (80°C / 85°C thresholds). One place to check (`journalctl -u demiurge-thermal-watchdog`) when the overclock misbehaves.
5. **Overclock + kernel cmdline.** Append `arm_freq=2800`, `over_voltage_delta=50000`, `force_turbo=0`, `gpu_freq=500`, `arm_boost=1` to `/boot/firmware/config.txt`. Append `threadirqs usbcore.autosuspend=-1 isolcpus=3` to the (single‑line!) `/boot/firmware/cmdline.txt` (`nohz_full=3 rcu_nocbs=3` are conventionally appended too, but the stock kernel rejects both — see below). This requires a reboot and is deliberately the *last* step.

Why each cmdline token:

- `threadirqs` — IRQ handlers run in kernel threads so the RT audio scheduler can preempt them.
- `usbcore.autosuspend=-1` — never suspend USB devices; keeps a USB interface and USB MIDI controllers awake through long idle stretches.
- `isolcpus=3` — CPU core 3 is off the scheduler's normal pool, reserved for the DEMIURGE audio chain. **This token alone is useless without `CPUAffinity=3` on `demiurge.service`** — `isolcpus` only tells the general scheduler to *avoid* CPU 3, it does not move anything onto it. The pairing drop-in at `config/systemd/demiurge.service.d/cpu-affinity.conf` pins the launcher and every child it forks (wrappers → csound / chuck / pd / sc / faust, `demiurge-clock`, `pw-mon`) onto CPU 3 exclusively. Without the drop-in, CPU 3 sits empty while the audio stages float across CPU 0-2 alongside browser tabs / systemd housekeeping. Verify with `taskset -p $(systemctl show demiurge.service -p MainPID --value)` — the hex mask should be `8` (CPU 3 only), not `f` (all cores). If a chain is ever too dense for one core, widen to `CPUAffinity=2-3` **and** add CPU 2 to the `isolcpus` token in the same boot. See `gotchas.md` Q0d.
- `nohz_full=3` — full dyntick on core 3 (no scheduler tick interrupts while a single task runs). **Rejected by the stock Raspberry Pi OS kernel** — `CONFIG_NO_HZ_FULL` is off, the kernel logs `Housekeeping: nohz unsupported`, and `/sys/devices/system/cpu/nohz_full` doesn't exist. Kept on cmdline for forward compatibility with custom kernels; the real win is `isolcpus` + the per‑process RT pin + `performance` governor. See `gotchas.md` Q0b.
- `rcu_nocbs=3` — would move RCU callbacks off the isolated core, but is **rejected by the same kernel** (it appears in the `Unknown kernel command line parameters` line alongside `nohz_full`). Verified by `dmesg` 2026-08-10. Do not count it as active.

`force_turbo=0` keeps the governor in control so thermal protection still applies. Never push `arm_freq` past 2800 MHz without validated cooling and instrumentation. Drop 100 MHz at a time if the watchdog screams.

The full drop‑in file set (services, scripts, config snippets) lives in `config/pi5-performance/` — see its README for the file‑by‑file index.

Enable order matters: safety first (RT limits + watchdog), performance second (quantum + governor), overclock last:

```bash
# 1. RT limits
sudo install -m 0644 config/pi5-performance/demiurge-audio.limits.conf \
    /etc/security/limits.d/demiurge-audio.conf

# 2. PipeWire quantum
sudo install -m 0644 config/pi5-performance/demiurge-pipewire-lowlatency.conf \
    /etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf
systemctl --user restart pipewire wireplumber

# 3. Power level (live.conf power = low|medium|high)
sudo install -m 0755 config/pi5-performance/demiurge-power.sh /usr/local/bin/
sudo install -m 0644 config/pi5-performance/demiurge-power.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now demiurge-power.service

# 4. Thermal watchdog
sudo install -m 0755 config/pi5-performance/demiurge-thermal-watchdog.sh /usr/local/bin/
sudo install -m 0644 config/pi5-performance/demiurge-thermal-watchdog.service /etc/systemd/system/
sudo install -m 0644 config/pi5-performance/demiurge-thermal-watchdog.timer /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now demiurge-thermal-watchdog.timer

# 5. Overclock + cmdline — manual, review before reboot
#    Append config/pi5-performance/config.txt.snippet to /boot/firmware/config.txt
#    Append tokens from config/pi5-performance/cmdline.txt.additions to cmdline.txt
sudo reboot
```

Verify after reboot:

```bash
vcgencmd measure_clock arm                                   # ~2.8 GHz under load
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor    # performance
cat /sys/devices/system/cpu/isolated                          # 3
cat /proc/cmdline                                             # new tokens present
ulimit -r                                                     # 99
pw-metadata -n settings 0 clock.force-quantum                 # 128
taskset -p $(systemctl show demiurge.service -p MainPID --value)   # affinity mask 8 = CPU 3 only
pw-top                                                         # zero xruns under load
```

Back‑out: comment the `arm_freq` / `over_voltage_delta` lines in `config.txt` and reboot — stock 2400 MHz immediately.

---

## 12. Failure modes & how to escape them

Every item here cost real debugging time.

### Audio / PipeWire / JACK

**Audible artifact with zero xruns after a rebuild:** run `demiurge-audit-audio` first. It checks rate, quantum, device period, csound RT policy, STATE_DIR, librtjack and mixer in one pass. Then `demiurge-mixer show`: an unstored codec mixer leaves the input gain at the driver default (see `incidents/2026-10-02-bitcrush-after-rebuild.md`, Investigation 2).

**F1. `libjack-jackd2-dev` + pipewire‑jack collision.** (See §5 for the root cause.) Installing `libjack-jackd2-dev` places jackd2's real `libjack.so.0` in the default loader path; any JACK‑linked program now blocks waiting for a non‑existent `jackd`. Silent hang. Fix: prepend `LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack` to every JACK client launch. The launcher does this automatically.

**F2. `ldd` lies, `/proc/<pid>/maps` tells the truth.** `ldd $(which pd)` shows compile‑time linking only — it always prints the jackd2 path regardless of runtime overrides. The actually‑loaded libjack is only visible in `/proc/<pid>/maps | grep libjack` on a *running* process.

**F3. `update-alternatives` doesn't manage `libjack.so.0` on Trixie.** No alternative exists — `update-alternatives --set libjack.so.0-aarch64-linux-gnu …` errors out. `LD_LIBRARY_PATH` is the only clean fix short of removing jackd2 entirely (which also removes anything depending on `libjack-jackd2-0`, including Faust's build chain — not worth it).

**F4. `dpkg -S /lib/aarch64-linux-gnu/libjack.so.0` returns no match.** `/lib` is a compat symlink tree. The real file lives at `/usr/lib/aarch64-linux-gnu/libjack.so.0` owned by `libjack-jackd2-0`. Use `readlink -f` or query the `/usr/lib/...` path.

**F5. PipeWire JACK clients don't auto‑link to non‑default sinks.** JACK client outputs auto‑route only to the default sink. Since DEMIURGE keeps the physical interface as default (see F6), the launcher must explicitly `pw-link <client>:output_1 demiurge-sink:playback_FL` after the client registers — which is what `apply_patch_graph` does.

**F6. `demiurge-sink` as default creates a loopback cycle.** The loopback's stream‑output follows default; if default = `demiurge-sink`, the stream‑output targets its own input → cycle → PipeWire silently drops the path → no audio anywhere. Keep the physical interface default.

**F7. `wpctl status` shows `demiurge-sink` under Filters, not Sinks.** Because it's a loopback filter node. Still a valid sink for `PIPEWIRE_NODE=demiurge-sink`. Check the **Filters** section or `pw-link -i | grep demiurge`.

**F8. `demiurge-sink` vs `demiurge-sink-output`.** Apps write to `demiurge-sink:playback_FL/FR`. The stream‑output side `demiurge-sink-output:output_FL/FR` is what feeds the interface. Don't confuse them in `pw-link -l`.

### Pure Data

**F9. Pd's `-alsa` is not PipeWire‑compatible.** Pd with `-alsa` probes raw hw devices and bypasses the pipewire‑alsa interception. Silent no‑audio. **Use `-jack`.**

**F10. Pd DSP is OFF by default under `-nogui`.** The #1 silent‑failure mode. Every DEMIURGE Pd patch must include a `[loadbang] → [; pd dsp 1(` trigger. A running Pd with JACK ports registered but no audio is almost always this bug.

**F11. Pd prints nothing on silent failure.** Empty log ≠ working quietly. Use `-verbose` in dev.

**F12. Pd `.pd` file object indices count `#X text` and `#X array` but not `#N canvas` or `#A <data>`.** When hand‑editing, `#X connect` indices are zero‑based across every `#X obj / msg / text / floatatom / array`. Getting this wrong silently breaks connections.

**F13. `pd -alsamidi -midiindev 1` can open a physical MIDI device directly** instead of creating a virtual `Pure Data` seq client, bypassing `Midi Through`. Workaround: rely on the launcher's post‑launch `aconnect "Midi Through":0 "Pure Data":0`.

**F14. `aconnect "Midi Through":0 "Pure Data":0` errors `invalid destination address` when Pd isn't registered yet.** Launcher polls for ~5 s.

### Faust

**F15. `faust` apt package doesn't pull `libasound2-dev` / `libjack-jackd2-dev`.** Install both before any `faust2jackconsole` compile — and then accept F1.

**F16. `faust2jackconsole` prints `[: =: unary operator expected`.** Harmless shell bug. Binary still builds.

**F17. Faust `run` lines point at the compiled binary, not the `.dsp` source.**

**F18. `faust2jackconsole` binaries auto‑connect to system playback.** The launcher's hardware‑playback scrub pass cleans this up — if you kill the launcher, the scrub stops too.

### MIDI

**F19. Bidirectional shared MIDI bus can create feedback loops.** Every program reads AND writes `Midi Through`. A patch that echoes input to output (a Pd `[notein] → [makenote] → [noteout]` passthrough) floods the bus. Diagnostic: `aconnect -d "Midi Through" "<suspect>"` to break the output side.

**F20. `snd-virmidi` does nothing useful.** It loads silently — PipeWire owns raw MIDI devices exclusively.

### Csound

**F21. `--sched` exits with "cannot set scheduling policy to SCHED_RR"** unless the user has `CAP_SYS_NICE`. The `demiurge-run-csound` wrapper does not grant that capability, and csound dies before rtjack initializes — the launcher only logs `program 'foo' marked dead`. `--realtime` alone is sufficient. **Never put `--sched` in a DEMIURGE `.csd`.**

**F21a. `-B 128` is below rtjack's minimum buffer size.** Csound exits with `rtjack: buffer size (-B) is too small`, the stage silently disappears, and `walk_to_live` re‑links upstream straight to downstream. Same `marked dead` symptom as F21. **Use `-B 1024 -b 128`** in every `.csd` `<CsOptions>` block — `-B` is the JACK ring size, `-b` is the software block which must equal the system quantum (128). Any `-B >= 256` works; 1024 is the safe default. The launcher's journal won't tell you any of this — read `~/.demiurge/logs/<id>.log` to see csound's actual stderr.

**F22. `-M0` subscribes to System Timer (ALSA seq 0:0), not `Midi Through` (14:0).** Use `-+rtmidi=alsaseq -M0` so csound opens the ALSA seq backend; the launcher then `aconnect`s Midi Through to it after launch.

**F23. `liblinear_algebra.so` warning** — harmless missing optional plugin.

### Shell / terminal / session

**F24. `sudoedit` / `nano` fail with `Error opening terminal: xterm-ghostty`.** Ghostty's `TERM` isn't in the Pi's terminfo db. Prefix TTY tools with `TERM=xterm-256color`, or `ssh -t` and set it per‑session.

**F25. `pkill -9 pd` spams "Operation not permitted" for kernel threads** like `kswapd` and `irq/174-vc4 hdmi hpd connected`. Scope to user: `pkill -9 -u $USER pd`.

**F26. `pgrep -af "pd "` also matches kernel threads.** Use `pgrep -x pd`.

**F27. Background SSH: `&` inside the quoted command, not outside.** `ssh host 'pd … &'` backgrounds pd on the Pi. `ssh host 'pd …' &` backgrounds the whole ssh session on the Mac.

**F28. Quoted multi‑line commands break when pasted through terminal wrapping.** Keep pasted commands on single lines under ~75 chars, or `scp` a script first.

### Audio recording verification

**F29. `pw-record` / `pw-cat --target=` are unreliable.** They silently auto‑link to the interface's own capture node instead of the node you asked for, producing all‑zero WAVs while the chain is actually playing fine. **Use `pw-jack jack_rec`** with quoted JACK‑style port names:

```bash
pw-jack jack_rec -d 4 -f /tmp/mix.wav "DEMIURGE:monitor_FL" "DEMIURGE:monitor_FR"
sox /tmp/mix.wav -n stats   # look for non -inf RMS
```

### Diagnostic techniques that actually work

- `speaker-test -c2 -twav -l1` — ground truth for the physical sink. Front Left / Front Right spoken = hardware and default routing are fine.
- `pw-link -l` — single source of truth for audio routing. If an edge isn't here, the app is disconnected regardless of everything else.
- `sudo journalctl -u demiurge -f` — the launcher's decision log (parsing, launch, link/scrub, reconcile, child-exit).
- `tail -f ~/.demiurge/logs/<id>.log` — a specific stage's stdout+stderr. Read this when a stage launches but exits silently.
- `tail -f ~/.demiurge/logs/<id>.compile.log` — Faust/cpp compile diagnostics. Read this when a `.dsp` or `.cpp` stage refuses to come up.
- `/proc/<pid>/maps | grep libjack` — only reliable way to confirm which libjack is actually loaded.
- `pw-top` — xrun counter + per‑node realtime CPU load. Watch during M8 validation.
- `aseqdump -p "Midi Through"` — single source of truth for MIDI bus traffic.
- Verbose mode on everything: `pd -verbose`; `csound -m0` off; check each language's equivalent. Silent tools hide failures.

---

## 13. Verification checklist

Walk this list after install, before trusting the system to a performance.

```bash
# Virtual layer
pw-cli ls Node | grep demiurge                     # demiurge-sink + demiurge-source
pw-link -i | grep demiurge                          # input ports visible
aconnect -l | grep -i "Midi Through"                # client 14:0

# PipeWire running + low-latency active
systemctl --user status pipewire wireplumber        # both active
pw-metadata -n settings 0 clock.force-quantum                 # 128
ulimit -r && ulimit -l                              # 99, unlimited
taskset -p $(systemctl show demiurge.service -p MainPID --value)   # mask 8 (CPU 3 only)

# Hot-plug resilience
speaker-test -D pipewire -c 2 -r 48000 &
# unplug the interface → speaker-test survives
# replug → audio resumes
kill %1

# Session
sudo systemctl restart demiurge
journalctl -u demiurge -f                           # "=== DEMIURGE Launcher starting ==="
tail -f ~/.demiurge/logs/csound_reverb.log          # per-stage stderr (substitute your stage id)
pw-link -l                                           # full chain visible
aseqdump -p "Midi Through" | head                    # 0xF8 + CC119 flowing

# Thermal (if overclocked)
journalctl -u demiurge-thermal-watchdog -f
vcgencmd measure_temp && vcgencmd get_throttled      # < 80 C, 0x0
```

**Hard‑mode 60‑second xrun stress test.** With `~/demiurge/live.conf` carrying the default four‑stage chain (Strudel → Faust delay → C++ crush → Csound ring‑mod), `sudo systemctl restart demiurge` and watch `pw-top`. The xruns column must stay at zero for 60 s at `clock.force-quantum 128`. The two‑chain stress test under `demiurge/examples/plucky_ambient/live.conf` (a dry plucky ChucK arpeggio summed with a Faust drone fed through Csound `reverbsc`) is the next step up — both chains terminate at `out` and `demiurge-sink:playback_FL/FR` mixes their feeds. `pw-link -l` should show two distinct edges into each sink port; both chains should coexist without xruns.

**Back‑out ladder** when something breaks:

- Overclock misbehaves → comment `arm_freq`/`over_voltage_delta` in `config.txt`, reboot → stock 2400 MHz.
- Thermal throttle persists → drop `arm_freq` 100 MHz, reboot.
- Launcher wedged → `sudo systemctl restart demiurge`. Logs at `~/.demiurge/logs/` and `journalctl -u demiurge`.
- PipeWire wedged → `systemctl --user restart pipewire wireplumber`.
- Lost audio after a graph change → `pw-link -l` first, `aseqdump -p "Midi Through"` second, `journalctl -u demiurge` third, `~/.demiurge/logs/<id>.log` fourth (this is where a silently-dying stage's real error lives).

---

## 14. Reference: paths, ports, client names

| What | Where |
|---|---|
| Boot config (pointer) | `/boot/firmware/demiurge.conf` |
| Live config (primary, hot-reloaded) | `~/demiurge/live.conf` |
| Launcher binary | `/opt/demiurge/bin/demiurge-launcher` |
| Clock binary | `/opt/demiurge/bin/demiurge-clock` |
| Systemd unit | `/etc/systemd/system/demiurge.service` |
| User staging tree | `~/demiurge/` |
| Example stages for the chain | `~/demiurge/examples/hello_patching/` |
| Parallel‑chain stress test | `~/demiurge/examples/plucky_ambient/live.conf` |
| Launcher journal | `journalctl -u demiurge` (canonical) |
| Per‑stage stdout/stderr | `~/.demiurge/logs/<id>.log` (truncated each spawn) |
| Faust/cpp compile output | `~/.demiurge/logs/<id>.compile.log` (only on rebuild) |
| Virtual sink (write to) | `demiurge-sink:playback_FL/FR` |
| Virtual sink (monitor/capture) | `demiurge-sink-output:output_FL/FR` |
| MIDI bus | ALSA seq `Midi Through` at client `14:0` |
| PipeWire JACK shim | `/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack/libjack.so.0` |
| PipeWire virtual layer | `/etc/pipewire/pipewire.conf.d/demiurge-virtual.conf` |
| PipeWire low‑latency | `/etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf` |
| RT limits | `/etc/security/limits.d/demiurge-audio.conf` |
| MIDI hot‑plug helper | `/usr/local/bin/demiurge-midi-connect.sh` + `/etc/udev/rules.d/99-demiurge-midi.rules` |
| Power level script | `/usr/local/bin/demiurge-power.sh` (+ `demiurge-power.service`) |
| Thermal watchdog | `/usr/local/bin/demiurge-thermal-watchdog.sh` (+ service + timer) |

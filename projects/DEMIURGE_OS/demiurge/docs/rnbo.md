# RNBO on DEMIURGE

How Cycling '74 RNBO patches run as a first-class DEMIURGE stage — authored in
Max, pushed to the Pi over the network, compiled **on-device**, and patched into
the audio chain (e.g. into the NEPTR Phase 4 Csound effects), all driven by
`live.conf`. No Cycling '74 SD-card image; the runner is built from source.

> Status (2026-06-06, PM update): **FULL CHAIN LIVE** — `untitled.rnbo`
> (440 L / 660 R) → `neptrPhase4.csd` (csound6) → `demiurge-sink` → **Scarlett
> 2i2**, with **Teensy MIDI → Midi Through pool → RNBO `midiin1`** (§7a). All
> proven by ear / graph. Three things got fixed this session: (1) **csound was
> stack-smashing in `librtjack`** on long pipewire port names — rebuilt plugin,
> Open issues #1; (2) **auto-reload on push** (§5a); (3) **MIDI pool → RNBO**
> wired in the launcher (§7a). The aggregate auto-bridged `demiurge-sink` → the
> Scarlett's USB sink with no manual step (it only enumerated **after a Pi
> reboot** this session).
>
> Earlier status (AM): RNBO compile + load + signal-through-Csound proven once.
> See **Open issues** for what's resolved vs still open.

---

## 0. Re-applying this whole integration from scratch

Everything below was built incrementally; this is the fast path to redo it on a
fresh (or reverted) Pi. **Deploy gotchas first:**
- `!`-prefixed commands in the Claude session run on the **Mac**, not the Pi. To
  run something on the Pi (esp. `sudo`), wrap it: `ssh -t gorpi7@demiurge.local
  'sudo …'` (the `-t` lets sudo prompt). `demiurge.service` runs as `User=gorpi7`.
- The long `~/_______DEMIURGE` path mangles when hand-typed → use the Pi-side
  deploy scripts (`~/deploy-*.sh`) instead of typing the path.
- The repo is **not** a git repo; "sync" = `rsync`/`scp` Mac → Pi
  `~/_______DEMIURGE/`.

**Option A — one shot (recommended).** Two pre-builds, then the installer:
```
# on the Pi: build the patched csound JACK plugin (fixes the librtjack crash)
ssh gorpi7@demiurge.local 'bash ~/_______DEMIURGE/setup-script/build-rtjack-fix.sh'
# on the Pi: build the launcher (rnbo stage type + MIDI-pool wiring)
ssh gorpi7@demiurge.local 'PATH=$HOME/.cargo/bin:$PATH cargo build --release \
  --manifest-path ~/_______DEMIURGE/src/demiurge-launcher-rs/Cargo.toml'
# install everything (launcher, wrappers, runner+config+service, autoreload watcher):
ssh -t gorpi7@demiurge.local 'sudo bash ~/_______DEMIURGE/setup-script/deploy-rnbo-wave1.sh'
# install the patched plugin (separate, backs up the stock one):
ssh -t gorpi7@demiurge.local 'sudo ~/deploy-rtjack.sh'
```
Then set the chain in `~/demiurge/live.conf` (§6) and push your patch from Max (§8).

**Option B — step by step** (what each piece is; details in the linked sections):
1. **Runner** (always-on, compile-enabled, built from source): §3–§4. Binary
   `/opt/rnbo/bin/rnbooscquery`, config `config/rnbo/demiurge-runner.json`,
   service `config/rnbo/rnbo-runner.service`.
2. **Patched csound JACK plugin** (the `librtjack` 64-byte stack-overflow fix —
   Open issues #1): `setup-script/build-rtjack-fix.sh` then
   `setup-script/deploy-rtjack.sh` (prebuilt `config/csound/librtjack.so`).
3. **Launcher with the `rnbo` stage + MIDI-pool wiring** (§5, §7a): build then
   `setup-script/deploy-launcher.sh`. `graph.rs` adds `midi_pool_capture()` and
   puts the pool→`midiin1` link in `desired` so scrub doesn't strip it.
4. **Wrappers**: `src/wrappers/demiurge-run-rnbo` (OSC load slot 0),
   `demiurge-run-csound` (cd-to-dir for `#include`s). Installed by wave1.
5. **Auto-reload watcher** (§5a): `config/rnbo/demiurge-rnbo-autoreload.{sh,service}`,
   a `--user` unit; installed by wave1 (`enable --now`).
6. **live.conf chain** (§6): `bypass = off`, `untitled.rnbo` →
   `~/NEPTR_phase4/csound/neptrPhase4.csd`.
7. **Verify** end to end: `pw-link -l | grep -E 'untitled-0|csound6'` shows
   `untitled-0:out1/2 → csound6:input1/2 → csound6:output1/2 → demiurge-sink`;
   `pw-link -l | grep -A1 'untitled-0:midiin1$'` shows the `Midi Through` pool in.
   Confirm by ear out the Scarlett.

## 1. The short version (how it works)

```
Max (your Mac) ──network──► rnbo-runner (Pi, :1234 OSC / :5678 HTTP)
                              │  compiles the patch ON THE PI into a .so
                              ▼
   DEMIURGE launcher loads it via OSC ──► JACK client  "<patch>-0"
                              │
   live.conf chain wires:  <patch>-0:out1/2 ─► csound6:input1/2
                              │                 (neptrPhase4.csd effects)
                              ▼
                       csound6:output1/2 ─► demiurge-sink (the sync layer)
                              ▼
                  aggregate bridges demiurge-sink ─► USB interface
                              (HDMI is meant to be ignored)
```

- **The runner is always on** (`rnbo-runner.service`) so Max can push any time.
- **The launcher owns routing** — the runner auto-connects nothing.
- **`live.conf` is the control surface** — `rnbo: <patchname>` in the chain.

---

## 2. Why this was hard (the wall)

The "just flash our SD image" path was a dead end here, for layered reasons:

1. **OS mismatch.** This Pi is **Debian 13 Trixie, arm64, Pi 5**. C74's image is
   **Bookworm, armhf (32-bit)**.
2. **No arm64 package… then a worse one.** C74 *does* now ship a **Trixie/arm64**
   apt repo, and `rnbooscquery 1.4.4-9` installs — but that prebuilt package is
   built with **on-device compile DISABLED** (`#ifdef RNBOOSCQUERY_ENABLE_COMPILE`
   in `src/Controller.cpp`; `/rnbo/info/unsupported_cmds` lists `compile`). It can
   only *receive* a precompiled `.so` (cloud-compile workflow). Max reports
   **"don't know how to build for this host."**
3. **So we build the runner from source** with `SUPPORT_COMPILE=On` (the default),
   which is the standard on-device-compile behavior — fully offline.

---

## 3. Building the runner from source (the recipe)

Source: `git clone --recursive https://github.com/Cycling74/rnbo.oscquery.runner`
(on the Pi at `~/rnbo.oscquery.runner`).

CMake options that matter (top-level `CMakeLists.txt`):
- `SUPPORT_COMPILE On` (default) → adds `-DRNBOOSCQUERY_ENABLE_COMPILE`.
- `USE_JACK ON` (default) → `-DRNBO_USE_JACK`.
- RNBO C++ lib via `-DRNBO_DIR=/usr/src/rnbo` (provided by the apt package; **keep
  the apt package installed** — it also provides `/usr/share/rnbo/so/CMakeLists.txt`
  and `/usr/bin/rnbo-compile-so`).

### Build deps (`setup-script/rnbo-build-deps.sh`)
apt: `libavahi-compat-libdnssd-dev build-essential libssl-dev libjack-jackd2-dev
libdbus-1-dev libxml2-dev libgmock-dev libsdbus-c++-dev libsndfile1-dev cmake
ccache git python3-pip`.

### Conan — **the Python 3.13 trap**
C74 pins **conan 1.61.0**, which `import imp` — a module **removed in Python
3.12**. Trixie has **Python 3.13**, so conan 1.61 crashes with
`ModuleNotFoundError: No module named 'imp'`. **Fix: use conan 1.66.0** (last 1.x,
runs on 3.13, satisfies the project's min):
```
pip3 install --break-system-packages --user conan==1.66.0
conan profile new default --detect --force          # accepts gcc 14
conan profile update settings.compiler.libcxx=libstdc++11 default
conan profile update settings.compiler.cppstd=20 default
conan remote add -f cycling-public https://conan-public.cycling74.com   # libossia
```

### Configure + build
```
cd ~/rnbo.oscquery.runner && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DRNBO_DIR=/usr/src/rnbo ..
cmake --build . -j3
```
Configure ~20 min (it builds **boost 1.80** + **libossia** from source via conan),
total ~22 min on a Pi 5. Non-fatal noise: a `numpy` missing notice and a
`constexpr` warning. Result: `build/bin/rnbooscquery` (~11 MB), installed to
`/opt/rnbo/bin/rnbooscquery`.

Verify compile is enabled:
```
curl -s localhost:5678/rnbo/info/unsupported_cmds   # -> []   (was ["compile",...])
curl -s localhost:5678/rnbo/info/target_id          # -> aarch64-Linux-GNU-14.2.0
```

---

## 4. The runner service + config

`rnbo-runner.service` (`config/rnbo/rnbo-runner.service`) — system unit,
`User=gorpi7`, always on so Max can push any time. Critical bits:
- Runs under the **pipewire-jack shim** so it connects to PipeWire and never
  spawns a `jackd`: `Environment=LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack`
  (also `JACK_NO_AUDIO_RESERVATION=1`). This is the same libjack-collision fix the
  rest of DEMIURGE uses (see `INSTRUCTIONS.md` §5/§12).
- `ExecStart=/opt/rnbo/bin/rnbooscquery -c ~/.config/rnbo/demiurge.json -d`
  (`-d` = come up empty; DEMIURGE loads patches explicitly via OSC).
- **Quote `PIPEWIRE_PROPS`** in the unit (`Environment="PIPEWIRE_PROPS=node.autoconnect=false node.dont-reconnect=true"`) — unquoted, systemd splits it at the space and warns `Invalid environment assignment, ignoring: node.dont-reconnect=true`.

Config `~/.config/rnbo/demiurge.json` (`config/rnbo/demiurge-runner.json`) — the
keys that matter, **two of which are non-obvious and were the cause of the
"compile failed" saga**:

| key | value | why |
|---|---|---|
| `so_build_exe` | `/usr/bin/rnbo-compile-so` | **absolute path** to the compile helper. The runner forks `bp::child("rnbo-compile-so", …)` (bare name); under the system service that bare-name `exec` fails → the child **zombies before exec'ing** → compile silently never runs (Max hangs on "Compiling"). Absolute path fixes it. |
| `rnbo_cpp_dir` | `/usr/src/rnbo` | the RNBO C++ source dir passed to the compiler. Its **default is resolved relative to the runner binary's location**; moving the binary to `/opt/rnbo/bin` broke that default → `RNBO.cpp` not found → `compile failed with status: 1`. Pin it explicitly. |
| `compile_cache_dir` | `~/Documents/rnbo/cache/so` | where the compiled `.so` lands. |
| `instance_auto_connect_audio` | `false` | DEMIURGE owns routing, not the runner. |
| `instance_auto_connect_audio_indexed` | `false` | same. |
| `instance_auto_connect_midi` / `control_auto_connect_midi` | `false` | same. |
| `instance_auto_start_last` | `false` | launcher decides what loads. |
| `db_path` | `~/Documents/rnbo/oscqueryrunner.sqlite` | the patcher store (survives restarts). |

The on-device compile chain (for reference): runner → `/usr/bin/rnbo-compile-so`
(a **Ruby** script) → `cmake` on `/usr/share/rnbo/so/CMakeLists.txt` with
`-DRNBO_CLASS_FILE=<patcher.cpp> -DRNBO_CPP_DIR=/usr/src/rnbo` → `cmake --build`
→ moves the `.so` into `compile_cache_dir`. Needs `ruby`, `cmake`, `g++` (all
present). It does **not** need conan (that's only for building the runner itself).

---

## 5. Launcher integration (the `rnbo` stage type)

DEMIURGE's Rust launcher (`src/demiurge-launcher-rs`) learned an `rnbo` stage.
Changes (additive — `aggregate.rs` and existing languages untouched):

- **`live.rs`** `resolve_lang_from_ext`: `.rnbo` → lang `rnbo`.
- **`supervisor.rs`** `resolve_lang`: `rnbo` passthrough; `candidate_client_names`:
  for `rnbo`, the JACK client is `"<patchername>-0"` (the runner names a loaded
  instance `<patcher>-<slot>`). Added `basename_no_ext()` helper.
- **`graph.rs`** port-pair tables: added `out1/out2` + `in1/in2` (RNBO instance
  ports) and `input1/input2` + `output1/output2` (Csound's JACK ports) so the
  stereo wiring resolves deterministically.
- **`src/wrappers/demiurge-run-rnbo`** (new): the stage "process." It does **not**
  host audio — it waits for the always-on runner (`:5678`), OSC-loads the named
  patcher into slot 0 (`/rnbo/inst/control/load is 0 <patcher>`), then blocks so
  the launcher tracks it as a live stage. On `SIGTERM` it unloads the slot. The
  patcher name = the chain token's basename without extension.
- **`src/wrappers/demiurge-run-csound`**: now `cd`s to the csd's directory before
  exec, so multi-file csds resolve relative `#include`s (NEPTR's `neptrPhase4.csd`
  pulls 29 includes: `globals.orc`, `effects/*.orc`, `signal_chain.orc`,
  `midi_handler.orc`).
- **`config/pi5-performance/demiurge-mode-select.sh`**: unified to read `bypass`
  and the csd from the **`launch=` target** (`/boot/firmware/demiurge.conf` →
  `~/demiurge/live.conf`) instead of a hardcoded NEPTR path, so one file is the
  source of truth.

Build on the Pi: `cd ~/_______DEMIURGE/src/demiurge-launcher-rs && cargo build
--release`, then `sudo install -m0755 target/release/demiurge-launcher
/opt/demiurge/bin/demiurge-launcher`.

---

## 5a. Auto-reload on push (live-switch the instance)

**Problem.** `demiurge-run-rnbo` loads the patcher into slot 0 once at startup
then blocks. When you re-export the **same patcher name** from Max, the always-on
runner recompiles a new `.so` and bumps the patcher's `created_at`, but the
**loaded instance does not switch** — you keep hearing the old version.

**Mechanism (proven this session).** Re-loading slot 0 by name picks up the
newest `.so`, and the launcher's graph applier **auto-rewires** the fresh
`<patcher>-0` JACK client into the chain (verified: unload→load → `untitled-0`
reappears and is reconnected to `demiurge-sink` within ~1–2 s, no manual wiring).
So all that's needed is to detect the new compile and re-send load.

**Deployed solution — a `--user` watcher service (no sudo, no `/opt` write):**
- `config/rnbo/demiurge-rnbo-autoreload.sh` → installed at
  `~/.local/bin/demiurge-rnbo-autoreload.sh` on the Pi.
- `config/rnbo/demiurge-rnbo-autoreload.service` → installed at
  `~/.config/systemd/user/`, `enable --now`. Survives reboot (`Linger=yes`).
- It reads the first `*.rnbo` token from `~/demiurge/live.conf`, polls
  `http://localhost:5678/rnbo/patchers/<name>/created_at` every 2 s
  (`RNBO_RELOAD_POLL`), and on change does
  `/rnbo/inst/control/unload i 0` → `load is 0 <name>`. `created_at` changes only
  on a real push (not on our own reload), so it never self-retriggers.
- Logs: `journalctl --user -u demiurge-rnbo-autoreload`.

> The runner runs as a system service but the **demiurge launcher runs as
> `User=gorpi7`**, and so does this watcher — that's why a user-level unit is
> sufficient and nothing needs root.

**Alternative (not yet deployed):** the same logic is folded inline into
`src/wrappers/demiurge-run-rnbo` (replaces its `sleep 3600` block with the poll
loop). Deploy it the next time `/opt/demiurge/bin` is updated with sudo
(`sudo install -m0755 src/wrappers/demiurge-run-rnbo /opt/demiurge/bin/`), then
disable the standalone watcher to avoid double-reloading.

---

## 6. `live.conf` schema

```
bypass = off          # REQUIRED: rnbo+csound need the PipeWire/JACK hub, not bypass

chain =
  untitled.rnbo                              # load patcher "untitled" into the runner, slot 0
  ~/NEPTR_phase4/csound/neptrPhase4.csd      # its outs feed csound inch 1/2
```

- The `*.rnbo` token's **basename is the patcher name** as exported from Max. The
  file need not exist on disk — the patch lives in the runner's store.
- Rename the token to match whatever you named the patch in Max.
- Example file: `demiurge/examples/rnbo_neptr/live.conf`.

`bypass = on` (NEPTR's old default) runs csound direct-ALSA as the sole audio
owner — incompatible with an RNBO source stage. Must be `off` here.

---

## 7. Audio routing & the sync layer

- The chain terminates at **`demiurge-sink`** (the sync-layer DAC hub). The
  **aggregate layer** (`aggregate.rs`, *do not modify*) bridges
  `demiurge-sink`/`demiurge-source` ↔ any **USB-class** interface
  (`alsa_output.usb-*` / `alsa_input.usb-*`) and re-runs on USB hotplug
  (`Event::UsbDeviceChange`). So it works headless and "just starts working" when
  you plug an interface in. **HDMI/Bluetooth/built-in are meant to be excluded.**
- RNBO output verified a **clean 440 Hz sine** (Goertzel: fundamental dominant,
  harmonics ~2%). Csound passes it through the effects chain; signal confirmed at
  `demiurge-sink` (L+R ≈ 0.49).
- To feed hardware input (guitar) into csound *as well* as RNBO, add an
  `inputs =` block pointing at the csd (it reads `inch 1/2`).

---

## 7a. MIDI routing (Teensy → pool → RNBO)

DEMIURGE has a shared **MIDI pool**: the ALSA-seq client **`Midi Through`** (kernel
`client 14`). Physical devices are merged into it (`demiurge-midi-connect.sh` wires
the Teensy `16:0 → 14:0`), and `midi.rs` joins every ALSA-seq-registered program to
it bidirectionally. So "everything MIDI" lives on `Midi Through`.

**The catch:** an RNBO instance is **JACK-MIDI only** (`untitled-0:midiin1`/
`midiout1`); it never registers an ALSA-seq client, so `midi.rs` can't join it to
the pool. And you **can't** just `pw-link` `Midi Through` → `midiin1` by hand: the
launcher's `graph.rs::scrub_stray_session_links` deletes, on every tick, **every
incoming link to a session client's ports that isn't in the `desired` set** — so an
external MIDI link to `midiin1` (an input port) is stripped within ~2 s. (RNBO's
`midiout1` is an *output* port, so its links survive — that's the asymmetry you'll
see in `pw-link -l`.)

**Fix (in the launcher, deployed):** `graph.rs::apply()` now, for every
`lang == "rnbo"` stage, links the pool's PipeWire bridge port
`Midi-Bridge:Midi Through Port-0 (capture)` → `<client>:midiin1` **and inserts that
edge into `desired`**, so scrub spares it. It re-applies on every reload via
`graph::apply`, so MIDI survives auto-reload too. Helper: `midi_pool_capture()`.
Net path, exactly as intended: **Teensy → Midi Through (pool) → RNBO `midiin1`** —
and anything else on the pool (e.g. `demiurge_clock`) reaches RNBO as well.

Verify:
```
pw-link -l | grep -A1 'untitled-0:midiin1$'     # <- Midi-Bridge:Midi Through Port-0 (capture)
aconnect -l                                      # Teensy 16:0 -> Midi Through 14:0
```
Patch-side: the RNBO patch must actually consume MIDI (a `notein`/`midiin` object),
which shows up as `midi_in ≥ 1` in `/rnbo/patchers/<name>/io` (`[ain,aout,min,mout]`).

---

## 8. The Max workflow + operations

**Discovery (from the Mac):** the runner advertises over Bonjour/Avahi as
`rnbo:demiurge`. Confirm with `dns-sd -B _oscjson._tcp local.`. Max shows it under
the RNBO export sidebar's Raspberry Pi / Devices section.

**Push:** double-click the device → export. It compiles on the Pi and stores the
patcher under the name you gave it in Max. **Max's RNBO version should match the
runner (1.4.x)** — a too-old Max (1.3.x) predates aarch64 and errors.

**Make sound:** a bare `cycle~` with **no frequency** outputs `sin(0)=0` =
silence. Give it a frequency (`cycle~ 440`) or a real synth.

**Useful OSC / OSCQuery** (`:1234` UDP / `:5678` HTTP):
```
oscsend localhost 1234 /rnbo/inst/control/load is 0 <patcher>   # load into slot 0
oscsend localhost 1234 /rnbo/inst/control/unload i 0            # unload slot 0
oscsend localhost 1234 /rnbo/patchers/<name>/destroy           # delete a stored patcher
curl  localhost:5678/rnbo/patchers                             # list stored patchers
curl  localhost:5678/rnbo/info/unsupported_cmds               # [] means compile enabled
```

**Recompile-while-loaded gotcha:** if a patcher is currently loaded as an
instance, re-pushing the **same name** can hang/conflict. Unload slot 0 first (or
the launcher's stage handles load/unload on chain edits). To start fully clean,
`destroy` the patcher, then re-export.

**Restart the runner without sudo:** it has `Restart=on-failure`, so
`pkill -9 -x rnbooscquery` makes systemd respawn it with the current config.

**Verify signal at any node** (no GUI):
```
PIPEWIRE_PROPS="stream.capture.sink=true" pw-record --target demiurge-sink \
  --channels 2 --rate 48000 --format s16 /tmp/s.wav   # then check peak
pw-record --target untitled-0 ...                     # capture the RNBO instance directly
```

---

## 9. Gotchas & fixes discovered (the saga, condensed)

1. **Prebuilt package can't compile** → build from source with `SUPPORT_COMPILE=On`.
2. **conan 1.61 + Python 3.13 (`imp` removed)** → use **conan 1.66.0**.
3. **Compile hangs, child zombies before exec** → set **`so_build_exe`** to the
   absolute `/usr/bin/rnbo-compile-so`.
4. **`compile failed with status: 1`** → set **`rnbo_cpp_dir=/usr/src/rnbo`** (the
   default is relative to the binary; broke when installed to `/opt`).
5. **Silent patch** → a bare `cycle~` is 0 Hz = silence; set a frequency.
6. **Csound can't find its includes** → wrapper `cd`s to the csd dir.
7. **`systemd` env split warning** → quote `PIPEWIRE_PROPS`.
8. **Two configs, one truth** → `mode-select` reads the `launch=` target.

---

## 10. Open issues (as of 2026-06-06)

- **Csound `stack smashing` — RESOLVED (2026-06-06 PM). It was NEVER NEPTR.**
  csound is back in the chain (`untitled-0 → csound6 → demiurge-sink → Scarlett`,
  stable for minutes). Root cause: **csound 6.18's `librtjack.so` overflows a
  64-byte stack buffer.** In `InOut/rtjack.c`, both `listDevices` and
  `listDevicesM` enumerate every JACK port on open and copy each name into
  `char port[64]` via `strNcpy(port, portNames[i], strlen(name)+1)` — the length
  is **uncapped**. pipewire-jack port names can exceed 63 chars; the **Scarlett's**
  do (`alsa_input.usb-Focusrite_Scarlett_2i2_USB-00.HiFi__Mic1__source.split:capture_AUX0`
  ≈ 80) → stack-canary trip → `*** stack smashing detected ***` on *every* JACK
  open. That's why it ran 25 min before the reboot (only HDMI+Teensy, names <64)
  and died after (Scarlett enumerated). A *trivial* `oscili` csd crashed
  identically → proves it was never `neptrPhase4.csd` and never the service
  context; `resolve_client: FAILED` was the downstream symptom (csound aborts
  before registering `csound6`).
  Fix: rebuild `librtjack.so` capping the copy at `sizeof(port)`. Build (no sudo):
  `setup-script/build-rtjack-fix.sh`; deploy (sudo): `setup-script/deploy-rtjack.sh`
  (backs up original → `librtjack.so.orig`). Prebuilt `config/csound/librtjack.so`,
  patched source `config/csound/rtjack.c.patched`. Validated standalone via
  `OPCODE6DIR64` before install.
- **HDMI not suppressed on Pi 5.** WirePlumber rule (`50-demiurge.conf`) matches
  `~alsa_output.*vc4.*hdmi.*` and `~alsa_output.*HDMI.*`, but the Pi 5 node is
  `alsa_output.platform-107c701400.hdmi.hdmi-stereo` (**lowercase `hdmi`, no
  `vc4`**) → it isn't caught, so WirePlumber auto-routes `demiurge-sink-output` to
  HDMI. Intended behavior is **no HDMI ever**. Fix (pending owner's OK to touch the
  WirePlumber config): broaden the rule to match `~alsa_output.*hdmi.*` (case-
  insensitive). Do **not** change `aggregate.rs`.
- **Auto-reload on push — RESOLVED (2026-06-06 PM).** Implemented as a `--user`
  watcher service that polls the patcher's `created_at` and reloads slot 0 on
  change; the launcher auto-rewires. See **§5a**. Files:
  `config/rnbo/demiurge-rnbo-autoreload.{sh,service}`.
- **USB interface (Scarlett) — RESOLVED, with a caveat.** The Focusrite Scarlett
  2i2 now enumerates (`card 1`, `alsa_output.usb-Focusrite_Scarlett_2i2_USB-…
  Line1__sink`) and the aggregate auto-bridged `demiurge-sink` → it with no manual
  step. **Caveat:** this session it only appeared **after a Pi reboot** — before
  the reboot `lsusb`/`aplay -l` showed only the Teensy MIDI. If the Scarlett isn't
  in `lsusb`, reboot (or re-seat USB) before debugging the audio graph.

- **HDMI suppression** still unfixed (rule misses the lowercase Pi 5 `hdmi` node),
  but with the Scarlett present the aggregate routes to USB so it's moot in
  practice; `demiurge-sink-output` went to the Scarlett, not HDMI.

---

## 11. File manifest

| Path | What |
|---|---|
| `/opt/rnbo/bin/rnbooscquery` | from-source, compile-enabled runner binary |
| `~/.config/rnbo/demiurge.json` ← `config/rnbo/demiurge-runner.json` | runner config |
| `/etc/systemd/system/rnbo-runner.service` ← `config/rnbo/rnbo-runner.service` | always-on runner |
| `src/wrappers/demiurge-run-rnbo` | the `rnbo` stage wrapper. OSC-loads slot 0; now also carries the auto-reload poll loop inline (NOT yet deployed to `/opt` — needs sudo; the user service below is what's live). |
| `config/rnbo/demiurge-rnbo-autoreload.sh` | **deployed** auto-reload watcher → `~/.local/bin/` on the Pi (§5a) |
| `config/rnbo/demiurge-rnbo-autoreload.service` | `--user` unit → `~/.config/systemd/user/`, `enable --now` (§5a) |
| `src/wrappers/demiurge-run-csound` | `cd`-to-dir fix for `#include`s |
| `src/demiurge-launcher-rs/src/{live,supervisor,graph}.rs` | `rnbo` stage type + port pairs; `graph.rs` also wires the MIDI pool → rnbo `midiin1` into `desired` (§7a) |
| `setup-script/deploy-launcher.sh` → `~/deploy-launcher.sh` (Pi) | rebuild-then-`sudo`-install the launcher + restart `demiurge.service` |
| `config/csound/librtjack.so` | **patched** csound JACK plugin (64-byte buffer overflow fixed) → `/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/` |
| `config/csound/rtjack.c.patched` | the patched csound 6.18 `rtjack.c` source (for reference/diff) |
| `setup-script/build-rtjack-fix.sh` | rebuild the patched plugin from upstream source (no sudo) |
| `setup-script/deploy-rtjack.sh` → `~/deploy-rtjack.sh` (Pi) | `sudo`-install the patched plugin (backs up original → `.orig`) |
| `config/pi5-performance/demiurge-mode-select.sh` | reads bypass/csd from `launch=` target |
| `demiurge/examples/rnbo_neptr/live.conf` | example chain (rnbo → NEPTR csound) |
| `setup-script/rnbo-build-deps.sh` | build toolchain + conan 1.66 |
| `setup-script/rnbo-add-repo.sh` / `rnbo-install-runner.sh` | (spike) C74 repo + prebuilt pkg for `/usr/src/rnbo` + helpers |
| `setup-script/deploy-rnbo-wave1.sh` | install built launcher/wrappers/runner + service |
| `~/rnbo.oscquery.runner/` (Pi) | runner source + `build/` |
| `~/Documents/rnbo/` (Pi) | runner store: `cache/{src,so}`, `oscqueryrunner.sqlite` |

---

## 12. Getting Csound back in the chain — DONE (kept for the diagnostic trail)

> ✅ **RESOLVED 2026-06-06 PM.** The crash was csound's `librtjack` overflowing a
> 64-byte stack buffer on long pipewire port names (the Scarlett's) — **not**
> `neptrPhase4.csd` and **not** the service context. Fixed by rebuilding the
> plugin (`setup-script/build-rtjack-fix.sh` + `deploy-rtjack.sh`); csound now runs
> the full chain. See **Open issues #1** for the final root cause. The diagnostic
> steps below are how we got there (control test → backtrace → source → fix) and
> are kept as a template for future audio-stage crashes.

**Goal (achieved):** restore `untitled.rnbo → neptrPhase4.csd → demiurge-sink →
Scarlett` and re-enable it in `live.conf` (the
`~/NEPTR_phase4/csound/neptrPhase4.csd` line is now **uncommented / live**).

**What we actually know (2026-06-06 PM):** running the wrapper by hand, csound
loads all ftables, prints `system sr: 48000`, then **aborts in ~0.4 s with
`*** stack smashing detected ***`** — i.e. a glibc stack-canary trip = a stack
buffer overflow somewhere in the orchestra/an opcode, NOT a sample-rate or
service-context problem. The launcher's `resolve_client: FAILED` is just the
downstream symptom (csound aborts before it registers the `csound6` JACK client).
So **fix the overflow first; the routing already works** (proven: when csound
*did* run earlier this session it wired RNBO→csound→sink fine).

### Step 1 — Confirm it's NEPTR-specific (control test)
Run a trivial csd through the *same* wrapper. If it registers `csound6` and
survives, the toolchain/flags are fine and the bug is in `neptrPhase4.csd`.
```
# tiny.csd: one oscil to out, no includes
cd ~/NEPTR_phase4/csound
DEMIURGE_RATE=48000 DEMIURGE_QUANTUM=256 /opt/demiurge/bin/demiurge-run-csound /tmp/tiny.csd
# expect: stays up; `pw-link -o | grep csound6` shows ports
```

### Step 2 — Get a real backtrace (find the overflowing opcode)
The canary abort kills the process before we see where. Two ways:
- Run under a debugger to catch the abort frame:
  ```
  gdb -q -ex run -ex bt -ex quit --args csound -d -+rtaudio=jack -odac -iadc \
      -r 48000 -B 1024 -b 256 --realtime ~/NEPTR_phase4/csound/neptrPhase4.csd
  ```
  (Drop `chrt`/RT for the debug run.) The backtrace names the opcode/function
  whose local buffer overflowed.
- Or run with `-+rtaudio=null -odac` to take JACK out of the picture; if it still
  smashes, it's pure orchestra, not the audio backend.

### Step 3 — Bisect the orchestra
`neptrPhase4.csd` pulls ~29 `#include`s (`globals.orc`, `effects/*.orc`,
`signal_chain.orc`, `midi_handler.orc`). Comment out effect blocks / includes by
half until the abort disappears, to localize the offending opcode or a
mis-sized array/`ftgen`/string buffer. Prime suspects: any opcode writing into a
fixed-size local array, `sprintf`/`strcpy`-style string ops, or a table read/write
with an index that can exceed its `ftlen`.

### Step 4 — Fix the overflow
Likely a too-small `i`-array, an `ftgen` size mismatch vs. how it's indexed, or a
string buffer. Fix the size/bounds in the `.orc`. Re-run Step 1's manual command
until csound stays up and `csound6` ports appear.

### Step 5 — Sample-rate / buffer correctness ("proper sample rate")
The wrapper already pins `-r $DEMIURGE_RATE` (48000) and sets `-b` = quantum,
`-B` = max(quantum,1024) so pipewire-jack doesn't silently resample or output
`-inf`. After the crash is fixed, verify the **whole graph is at one rate**:
```
pw-metadata -n settings 0 clock.rate ; pw-metadata -n settings 0 clock.force-quantum
# live.conf: rate=48000, quantum=256 → csound -r 48000 -b 256 -B 1024
```
RNBO runs at the graph rate automatically (it's a pipewire-jack client). Mismatch
shows up as detuned 440/660 — so the ear test doubles as a rate check.

### Step 6 — Re-enable and verify end-to-end
1. Uncomment the csd line in `~/demiurge/live.conf`; save (launcher hot-reloads).
2. `pw-link -l | grep -E 'untitled-0:out|csound6'` → expect
   `untitled-0:out1/2 → csound6:input1/2` and `csound6:output1/2 → demiurge-sink`.
3. Confirm by ear out the Scarlett: 440/660 now coloured by the NEPTR effects.
4. The auto-reload watcher (§5a) keeps working — it reloads the RNBO instance;
   the launcher re-inserts it ahead of csound on each push.

> If Step 1's trivial csd *also* smashes under the service but not standalone,
> THEN revisit the service-context theory (RT limits / cgroup) from the AM notes —
> but current evidence says fix the csd first.

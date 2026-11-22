# NAM Mode

Run Neural Amp Modeler profiles as a DEMIURGE audio stage. Drop a `.nam` file
into `live.conf` and it works like any other stage — JACK-wired, RT-scheduled,
OSC-controllable.

---

## Setup (once)

```sh
# From your Mac (or SSH into Pi and run directly):
./setup-script/deploy-nam.sh
```

This builds `csound-nam.so` on the Pi, installs it to
`/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/` (the dir csound actually
loads), and installs the CSD template to `/opt/demiurge/lib/nam_stage.csd`.

**Build from exactly one place.** The opcode source is
`neptr/csound/nam/src/` + the vendored `neptr/csound/nam/libs/NeuralAmpModelerCore`
(v0.5.3), built only by `neptr/csound/nam/build-on-pi.sh` (deploy-nam.sh and
setup-demiurge.sh Phase 5b both call it). The old `nam/src` single-instance copy
is retired in `nam/_attic_src_single_instance/`: it shares one model across L and
R and sounds like bitcrush. Verify any build with
`nm -C <so> | grep get_active_pair` (must be present) and
`testing/nam/test-nam.sh`.

**Brownout rule: never `-j"$(nproc)"`.** Build with `-j2` at most. A `-j4` build
browned out and rebooted a Pi 5 mid-compile (2026-10-03).

**Prerequisite:** GCC 12+ with libstdc++14 (default on Debian Bookworm/Trixie).
`g++ --version` should show 12 or higher.

---

## Getting profiles

`.nam` files are free community-captured amp models. Two main sources:

- **ToneHunt** — tonehunt.org
- **tone3000** — tone3000.com

Download `.nam` files and copy them to the Pi:

```sh
scp my_model.nam gorpi7@demiurge.local:~/demiurge/profiles/nam/
```

Profiles in a flat directory are sorted alphabetically and addressed by 0-based
index. Recommended layout:

```
~/demiurge/profiles/nam/
├── 73_plexi_bright.nam
├── fender_super_reverb_1977.nam
└── peavey_6505.nam
```

**Profile load time:** 289 KB `.nam` files take ~2–3 s to parse on Pi 5. If you
send OSC profile switches too fast (< 3 s apart), a second switch arriving while
the first is still loading will cancel it. Wait ~3 s between switches or watch
the `[NAMProcess]` log lines to confirm loading.

---

## live.conf usage

Point at a specific `.nam` file or a whole directory:

```ini
# Specific profile — loads its sibling directory, starts at that profile's index
chain =
  in1 ->
  ~/demiurge/profiles/nam/jcm800_crunch.nam

# Whole directory — starts at index 0 (alphabetically)
chain =
  in1 ->
  ~/demiurge/profiles/nam/
```

NAM in a chain with other stages:

```ini
chain =
  in1 ->
  ~/demiurge/profiles/nam/jcm800_crunch.nam
  ~/fx/reverb.csd
  ~/fx/delay.csd
```

---

## Hot-swapping with demiurge-set

`demiurge-set` switches between saved sets (full `live.conf` snapshots):

```sh
demiurge-set nam     # switch to NAM mode
demiurge-set neptr   # switch back to NEPTR
demiurge-set         # list available sets and show active
```

Sets live in `~/demiurge/sets/`. The shipped `nam.conf` points at the profiles
directory with `sync_layer = on` (PipeWire/JACK path).

**Why it always restarts the service:** When `sync_layer = off`, the launcher
has `exec()`'d into the audio program and the file-watcher is gone. A file copy
alone won't trigger a reload. `demiurge-set` always calls
`sudo systemctl restart demiurge` regardless of which mode you're switching
from or to — the NOPASSWD rule at `/etc/sudoers.d/gorpi7-demiurge` makes this
TTY-free so it works from SSH scripts and deploy pipelines.

---

## OSC control

The NAM stage listens on `DEMIURGE_OSC_BASE` (default **9000**).

| Address       | Type | Action                                       |
|---------------|------|----------------------------------------------|
| `/nam/idx`    | `i`  | Switch to profile N (0-based, alpha-sorted)  |
| `/nam/bypass` | `i`  | 1 = passthrough, 0 = NAM active              |

From the Mac (requires `liblo-tools`: `brew install liblo`):

```sh
oscsend demiurge.local 9000 /nam/idx i 2       # switch to profile 2
oscsend demiurge.local 9000 /nam/bypass i 1    # bypass NAM
oscsend demiurge.local 9000 /nam/bypass i 0    # NAM active
```

Profile switching is hot — no audio gap, no restart. The background loader
swaps the model off the audio thread.

---

## How it works

`demiurge-run-nam` is a wrapper around `demiurge-run-csound`. It:

1. Resolves the `.nam` argument to an absolute directory path and starting index.
2. Injects `NAM_PROFILE_DIR`, `NAM_PROFILE_IDX`, and `DEMIURGE_OSC_BASE` as
   CSound macros via `--omacro:` (CSound 6.18 has no `getenv` opcode).
3. Execs `demiurge-run-csound /opt/demiurge/lib/nam_stage.csd`.

CSound auto-loads `csound-nam.so` (the NAMProcess plugin), which spawns a
background thread to load models without blocking the audio callback.

The JACK client name is `csound6`. The launcher wires it into the patch graph
the same way as any other CSound stage.

### NAMProcess in other orchestras

`csound-nam.so` is a system-level plugin. **Any CSound orchestra running on
this Pi can call `NAMProcess`**, including NEPTR's `neptrPhase4.csd`. You don't
need to be in NAM mode — just add `NAMProcess` to any orchestra and it works.

```csound
; Inside neptrPhase4.csd or any .csd on this Pi:
aOutL, aOutR NAMProcess aInL, aInR, kProfileIdx, "/home/gorpi7/demiurge/profiles/nam"
```

---

## Troubleshooting

**Stage starts but no audio** — `csound-nam.so` not installed or the symlink is
stale. Run `deploy-nam.sh`.
Check: `ls /usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so`

**"no .nam files found" in logs** — `NAM_PROFILE_DIR` is empty or wrong. Check
the path exists and contains `.nam` files.

**Profile doesn't change on OSC** — there are three possible causes, in order
of likelihood:

1. **Wait longer.** .nam files take ~2–3 s to load on Pi 5. Sending a second
   OSC message before the first load finishes cancels the first. Wait 3 s
   between switches and check `[NAMProcess]` log output.

2. **OSClisten global-variable bug (CSound 6.18).** `OSClisten` may silently
   fail to write to a `gk` global. The CSD template works around this by
   receiving into a local `kRcvIdx` and assigning to `gkNAMIdx` inside an
   `if kIdx == 1 then` block. If you've modified `nam_stage.csd`, check that
   pattern is intact.

3. **Wrong port.** The stage listens on `DEMIURGE_OSC_BASE` (default 9000).
   If another CSound stage is already on 9000, set `DEMIURGE_OSC_BASE=9001`
   before the `chain =` block.

**Launcher ignores `.nam` files** — old launcher binary. Run `deploy-nam.sh` to
rebuild with `.nam` support.

**Zombie `[csound] <defunct>` on reboot** — PipeWire/JACK takes ~8 s to
initialize after boot. The launcher retries automatically; the zombie clears
once JACK is ready.

**`sudo` fails in deploy scripts / SSH** — the NOPASSWD rule at
`/etc/sudoers.d/gorpi7-demiurge` must cover the commands you're running
(`/usr/bin/install`, `/usr/bin/systemctl`,
`/usr/local/bin/demiurge-scarlett-bounce.sh`). Check with:
`sudo -l | grep NOPASSWD`

---

## CSound 6.18 gotchas

These were discovered during the 2026-06-12 integration. They affect any CSound
plugin or orchestra built against 6.18 on this Pi.

### 1. No `getenv` opcode

CSound 6.18 removed the `getenv` opcode. Use `--omacro:NAME=value` on the
command line and `#define`/`#ifndef` guards in the CSD:

```csound
#ifndef NAM_PROFILE_DIR
#define NAM_PROFILE_DIR # /home/gorpi7/demiurge/profiles/nam #
#endif
```

`demiurge-run-nam` injects `NAM_PROFILE_DIR`, `NAM_PROFILE_IDX`, and
`DEMIURGE_OSC_BASE` this way. Standalone `csound nam_stage.csd` (no macros)
falls back to the `#ifndef` defaults.

### 2. Macro terminator must be `.` not whitespace

In CSound 6.18, bare macro expansion `$NAME` is terminated by whitespace. In
some contexts (especially at end of expression) this silently swallows trailing
characters. Always write `$NAME.` (period terminates the token explicitly):

```csound
gkNAMIdx init $NAM_PROFILE_IDX.   ; correct
giOSCIn  OSCinit $DEMIURGE_OSC_BASE.
```

### 3. `OSClisten` may not write directly to global k-vars

In 6.18, `OSClisten` can silently fail to update a `gk` variable passed as its
output operand. Workaround: receive into a local k-var, then conditionally
assign to the global:

```csound
kRcvIdx  init 0
kIdx     OSClisten giOSCIn, "/nam/idx", "i", kRcvIdx
if kIdx == 1 then
  gkNAMIdx = kRcvIdx
endif
```

### 4. `OSCinit` should be at global scope

Placing `OSCinit` inside `instr 1` can cause i-time conflicts if the instrument
ever restarts. Move it above `instr 1` at the global orchestra level:

```csound
giOSCIn OSCinit $DEMIURGE_OSC_BASE.

instr 1
  ; use giOSCIn here
endin
```

### 5. LINKAGE `thread=5` (aperf) silently broken — use `thread=3` (kperf)

**This was the root cause of OSC profile switching not working.**

CSound 6.18's `LINKAGE` macro exports `csound_opcode_init`, which passes the
`OENTRY` array through the `AppendOpcode` code path. In that path,
`ep->thread` maps to the `flags` slot (not `thread`), so setting
`thread=5` (init + aperf) makes CSound see `flags=5` and a garbage `thread`
value — the aperf callback is never scheduled.

Fix: use `thread=3` (init + kperf) with `kopadr` set. CSound allocates a-rate
output buffers based on `outypes="aa"` regardless of thread value; the kperf
callback fills them once per ksmps block, which is correct for a per-block DSP
opcode.

```cpp
// OENTRY in csound_nam_opcode.cpp:
{
    (char*)"NAMProcess",
    (uint16_t)sizeof(NAMOpcode),
    (uint16_t)0,
    (uint8_t)3,           // init + kperf  (thread=5/aperf broken in 6.18)
    (char*)"aa",          // outypes: a-rate arrays allocated by CSound
    (char*)"aakS",
    (SUBR)nam_init,
    (SUBR)nam_kperf,      // kopadr: called once per ksmps block
    nullptr,              // aopadr: unused
    nullptr
},
```

This bug affects **any** CSound 6.18 plugin that tries to use aperf scheduling
via the `LINKAGE` macro. If you write a new plugin and the audio callback never
fires, check this first.

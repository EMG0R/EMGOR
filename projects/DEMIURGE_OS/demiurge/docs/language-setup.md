# Per-Language Setup

How each audio language plugs into DEMIURGE, and the quirks each one brings.

All languages route audio through `demiurge-sink` and MIDI through `Midi Through` (ALSA seq `14:0`) — you never reference hardware directly. The launcher handles port wiring after each program registers.

## Latency design principle

DEMIURGE optimizes hardware↔software latency as aggressively as possible. The transport path is:

```
Guitar → Scarlett ADC → ALSA capture ring → PipeWire/JACK → audio language DSP → ALSA playback ring → Scarlett DAC → output
```

The hardware layers (ALSA capture + playback) have a fixed floor of ~192 samples each (USB isochronous latency + one-period ALSA buffer), totalling ~384 samples = ~8ms at 48 kHz. The DSP quantum (one processing block) adds another 128 samples = 2.67ms. **Total minimum: ~512 samples ≈ 10.7ms** — this is the achievable target with current hardware.

Anything that adds buffering between those fixed layers is pure waste:
- `api.alsa.period-num` on the Scarlett output is pinned to 4 (256-sample ring, ~5ms max) — not the PipeWire default of 512 (32768-sample ring = 21ms of slack).
- `api.alsa.headroom` is set to 0 (PipeWire enforces a 64-sample minimum, so effective headroom is 64 samples).
- `clock.force-quantum` is 128 (2.67ms per DSP block). This is the direct dial for DSP latency — halving it halves DSP latency. Current value chosen for headroom margin with `chrt --fifo` RT isolation on CPU 3; could go to 64 if CPU isolation is solid.
- JACK clients must negotiate `PIPEWIRE_LATENCY=128/48000` — set via `demiurge-env.sh`. Any client running at a larger period (e.g. 256) doubles its contribution to the latency budget. See `gotchas.md` Q7e for why the deployed `demiurge-env.sh` can silently lag the correct value after a quantum change.

**Buffers only live in audio software (DSP algorithms), not in the transport.** If your Csound patch needs a 4096-sample delay line, that's fine — it's part of the effect, not transport overhead. The ring buffers between ALSA↔PipeWire↔JACK are kept as thin as the hardware permits.

Every language launch is prefixed with:

- `LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack` so pipewire‑jack's libjack shim wins over jackd2's real libjack (see `INSTRUCTIONS.md` §5 and §12 F1 for *why*).
- `PIPEWIRE_PROPS="node.autoconnect=false node.dont-reconnect=true"` so WirePlumber can't drag new nodes onto the default sink in parallel with the declared patch graph.

---

## Csound

**Audio.** JACK via `pipewire-jack`. The launcher runs:

```
csound -d -+rtaudio=jack -odac -iadc --realtime <file.csd>
```

No `--sched`: on Pi 5 without `CAP_SYS_NICE` granted to the user, `--sched` makes csound exit immediately with "cannot set scheduling policy to SCHED_RR". `--realtime` alone gives low‑latency behaviour.

**JACK client name.** Usually `csound6` on Debian Trixie. The launcher polls `csound6`, `csound`, `Csound` in order and uses whichever registers first.

**MIDI.** `-+rtmidi=alsaseq -M14:0` subscribes directly to `Midi Through`. The older `-M0` form subscribes to ALSA seq 0:0 (System Timer), not Midi Through — a classic footgun, do not use.

**Clock reading.** In `<CsOptions>` add `-+rtmidi=alsaseq -M14:0`; in orc code, `ctrl7 16, 119, 0, 1` gives you the BPM fraction on channel 16 CC119. See `clock.md` for the one‑instrument reader.

**Known harmless noise.** `liblinear_algebra.so` warning on startup is a missing optional plugin.

---

## ChucK

**Audio.** JACK. The launcher runs:

```
chuck --driver:jack --out:2 --in:2 --srate:48000 --bufsize:128 <file.ck>
```

**JACK client name.** `ChucK` — with a capital C. The launcher polls `ChucK`, `chuck`, `Chuck`.

**Port names.** ChucK emits `outport 0` and `outport 1` — literally, with a space. The launcher's port‑pair matcher is built to handle this (pairs are encoded as `"left|right"` internally because port names with spaces break the usual tokenisers).

**`--bufsize` must match the system quantum.** DEMIURGE pins PipeWire to `clock.force-quantum = 128` at service start, and `demiurge-env.sh` sets `PIPEWIRE_LATENCY=128/48000`. ChucK's `--bufsize` must equal that value (currently `128`). If it's smaller, ChucK's JACK client registers and its ports show up in `pw-link -o`, but it silently produces zero samples forever. No error, no log, just `-inf dB` out of `ChucK:outport 0/1`. Every other JACK client on DEMIURGE has this same rule — internal block size must match the system quantum, or go up to a multiple of it. See `gotchas.md` Q7a.

**MIDI.** `MidiIn.open(0)` opens port 0 which is `Midi Through` once the launcher has wired the bus. ChucK's RtMidi seq client registers as `"RtMidi Input Client"` rather than `"ChucK"`, so the launcher's ALSA‑seq‑name lookup falls back to a literal match.

**Debugging silence.** `chuck --probe` prints the audio devices ChucK sees; if nothing PipeWire‑shaped appears, you have a libjack collision (INSTRUCTIONS F1).

---

## Pure Data

**Audio.** JACK backend is **mandatory**. The `-alsa` backend probes raw hw devices and silently bypasses pipewire‑alsa — the patch runs fine with JACK ports registered and produces no audio. The launcher runs:

```
pd -nogui -jack -alsamidi -r 48000 -blocksize 64 -audiobuf 5 <file.pd>
```

**DSP is OFF by default under `-nogui`.** Every DEMIURGE Pd patch **must** start DSP explicitly or it will never produce sound:

```
[loadbang]
     |
[; pd dsp 1(
```

This is the #1 silent‑failure mode. A Pd patch with JACK ports registered, a non‑empty process, and no audio is almost always this bug. Use `-verbose` in dev — silent Pd is lying Pd.

**JACK client name.** `pure_data`. Output ports `output_1` / `output_2`, input ports `input_1` / `input_2`.

**MIDI.** Raw ALSA MIDI (`-alsamidi`) cannot see the shared ALSA seq bus. For ordinary note/CC traffic the launcher's post‑launch `aconnect "Midi Through":0 "Pure Data":0` is enough — but for clock sync, Pd needs the dedicated `pd_clock_bridge.py` sidecar in `~/demiurge/bridges/` that forwards `ch16 CC119` (BPM) and `ch16 CC117` (transport) to Pd as FUDI over UDP 9997. Run the sidecar as a normal `run pdclock python …` entry in your session file and receive with `[netreceive 9997 1] → [route bpm transport]`.

**Editing `.pd` files by hand.** `#X connect` indices are zero‑based across every `#X obj / msg / text / floatatom / array`. `#N canvas` and `#A <data>` do not count. `#X text` **does** count. Getting this wrong silently breaks connections.

---

## SuperCollider

**Audio.** JACK via pipewire‑jack. The launcher runs:

```
QT_QPA_PLATFORM=offscreen sclang <file.scd>
```

`QT_QPA_PLATFORM=offscreen` stops headless `sclang` from trying to spawn a Qt window. Use `sclang`, not `scide` — `scsynth` alone is insufficient, you need `sclang` to drive it.

The script itself should boot the server with stereo I/O:

```supercollider
s.options.numInputBusChannels_(2);
s.options.numOutputBusChannels_(2);
s.boot;
```

**JACK client name.** `SuperCollider`.

**MIDI.** `MIDIClient.init; MIDIIn.connectAll;` inside the `.scd` — `connectAll` picks up `Midi Through`.

**Status.** SC is installed and the `hello.scd` example works. Drop `~/demiurge/examples/hello_patching/sc_grain.scd` (or your own `.scd`) into the `chain =` list and save to add SuperCollider to the pipeline.

---

## Faust

**Build model.** Faust DSP compiles to a standalone JACK binary via `faust2jackconsole`. The launcher does this for you on demand: drop the `.dsp` source straight into `live.conf`'s `chain =` list and on first run the launcher invokes `faust2jackconsole -midi <file>.dsp` to produce the sibling binary, then launches it. Subsequent runs reuse the cached binary unless the `.dsp` is newer.

```ini
chain =
  ~/demiurge/examples/hello_patching/faust_delay.dsp
```

You can still run `faust2jackconsole -midi faust_delay.dsp` by hand if you want to inspect the build output:

`-midi` registers a JACK MIDI input port on the resulting binary, which the launcher then bridges to `Midi Through` via PipeWire's Midi‑Bridge (see "Faust MIDI bridge" below).

**Build dependencies.** `libasound2-dev` and `libjack-jackd2-dev` must be installed — and installing the latter is the trigger for the libjack collision (INSTRUCTIONS F1). The launcher's `LD_LIBRARY_PATH` prefix is what makes the compiled binaries actually run.

**`[: =: unary operator expected`** on line 111 of `faust2jackconsole` output is a harmless shell bug — the binary still builds.

**JACK client name.** Matches the binary basename (`faust_delay`, `faust_crush`, etc.).

**MIDI.** Faust `[midi:ctrl <num> <chan>]` metadata on sliders lets the DSP read CC values directly. Channel is 1‑indexed — `16` means MIDI channel 16. Example for the clock BPM:

```faust
bpm_cc = hslider("bpm[midi:ctrl 119 16]", 0.35, 0, 1, 0.001) : si.smoo;
bpm    = 40.0 + bpm_cc * 200.0;
```

**Faust MIDI bridge.** Faust stages built with `-midi` register a JACK MIDI input but cannot see ALSA seq directly. PipeWire exposes `Midi Through` as `Midi-Bridge:Midi Through Port-0 (capture)` on the JACK graph, and the launcher's `wire_faust_midi` step `pw-link`s that source into every Faust client's MIDI input port at graph‑build time.

**Auto‑connect to system playback.** `faust2jackconsole` binaries auto‑link their outputs to the first hardware playback device at startup, bypassing the patch graph. The launcher's hardware‑playback scrub pass cleans this up — if you stop the launcher, the scrub stops too.

---

## C++ (native)

**Build.** Standalone JACK binary linking against `libjack` (the pipewire‑jack shim) and ALSA seq for MIDI. The launcher builds on demand when only a `.cpp` source sits next to the missing binary:

```bash
g++ -O2 -o cpp_crush cpp_crush.cpp -ljack -lasound -lm -lpthread
```

`live.conf` usage — drop the `.cpp` source straight into `chain =`; the launcher compiles it on first run if no sibling binary exists:

```ini
chain =
  ~/demiurge/examples/hello_patching/cpp_crush.cpp
```

**JACK client name.** Whatever the program passes to `jack_client_open`, usually the binary basename.

**MIDI.** Open an ALSA seq client, connect to `Midi Through` (`14:0`), read events in a thread:

```cpp
if (ev->type == SND_SEQ_EVENT_CONTROLLER
    && ev->data.control.channel == 15      // ch16 == idx 15
    && ev->data.control.param == 119) {
    double bpm = 40.0 + (ev->data.control.value / 127.0) * 200.0;
    g_bpm.store(bpm);
}
```

This is the most direct path onto the DEMIURGE bus — no shim, no bridge. The reference implementation is `demiurge/examples/hello_patching/cpp_crush.cpp`.

**Why C++ on DEMIURGE.** When a DSP stage needs sample‑accurate control that `.dsp` or `.csd` can't express cleanly — per‑note randomisation on note‑on events, coin flips on individual samples, look‑ahead buffers with custom shapes — a hand‑written JACK client is the escape hatch.

---

## Strudel (headless)

**Two modes, one runner.** `~/demiurge/strudel/strudel-runner.mjs` is DEMIURGE's Node port of Strudel. It wires `@strudel/core` + `/mini` + `/transpiler` + `/tonal` into a minimal repl and supports:

- **Audio (default).** `node-web-audio-api` hosts an `AudioContext` backed by `cpal` → PipeWire‑JACK. Each hap becomes a saw + AR‑envelope voice on the audio graph. The cpal client shows up as **`cpal_client_out:out_0/out_1`** and the launcher links it into the patch graph like any other audio source.
- **MIDI (`--midi`).** Emits MIDI notes/CCs on `Midi Through` via `easymidi`, for sessions where downstream languages produce the sound. In this mode Strudel has no JACK audio client.

**Both** modes always *listen* on `Midi Through` for `ch16 CC119` and call `scheduler.setCps(bpm / 60 / 2)` — one cycle = two beats — so Strudel tempo‑locks to `demiurge_clock` automatically. `0xFA` / `0xFC` follow transport.

**Pinning is non‑negotiable.** `package.json` locks `@strudel/*` to **1.2.0**. `1.2.6+` pulls `@kabelsalat/web@0.4.1` which dropped the `SalatRepl` export and breaks the whole import chain in Node. `easymidi` wraps `@julusian/midi` (RtMidi ALSA seq); `jzz` will not work here because it only speaks raw MIDI and can't see seq ports.

**`live.conf` usage:**

```ini
chain =
  ~/demiurge/examples/hello_patching/strudel_drive.strudel
```

…for an audio Strudel stage, or in the `midi =` block instead if you want it to drive the chain via MIDI without producing audio. The `strudel` dispatch runs `node strudel-runner.mjs <file>` with `NODE_PATH=~/demiurge/strudel/node_modules`, and the `--midi` flag is added automatically when the entry is in the `midi =` block.

---

## Python

**Runtime.** The launcher prefers `/opt/demiurge/venv/bin/python` when the venv exists, and falls back to `/usr/bin/python3` otherwise. Either install `mido python-rtmidi python-osc ctcsound numpy` in the venv, or use the apt packages `python3-mido python3-rtmidi`.

**Audio.** DEMIURGE uses Python primarily to drive *other* languages — the `pd_clock_bridge.py` sidecar is the canonical example. Embedded audio is possible via `ctcsound`, but heavier DSP lives in C++ / Faust / Csound.

**MIDI.** `mido` with the `rtmidi` backend — open `Midi Through` by name:

```python
import mido
port = next(p for p in mido.get_input_names() if "midi through" in p.lower())
inp  = mido.open_input(port)
```

---

## NAM (Neural Amp Modeler)

**What it is.** NAM runs `.nam` amp-model profiles as a real-time DEMIURGE audio
stage. A `.nam` file in `chain =` is processed by the `NAMProcess` CSound opcode
via the `csound-nam.so` plugin. The stage is stereo in / stereo out — NAM's mono
model is applied symmetrically to each channel.

**Setup.** One-time install from the Mac (or SSH into Pi):

```sh
./setup-script/deploy-nam.sh
```

Builds and installs `csound-nam.so`, deploys the CSD template, rebuilds the
launcher. See `nam.md` for the full setup and profile-sourcing guide.

**Audio.** JACK via `demiurge-run-csound`. The wrapper execs:

```
demiurge-run-csound /opt/demiurge/lib/nam_stage.csd
```

**JACK client name.** `csound6` — identical to a plain CSound stage, because
NAM runs *inside* CSound.

**live.conf usage.**

```ini
chain =
  in1 ->
  ~/profiles/jcm800_crunch.nam   # specific profile
  ~/fx/reverb.csd

# or: whole directory, OSC switches profiles
chain =
  in1 ->
  ~/profiles/
```

**OSC.** Listens on `DEMIURGE_OSC_BASE` (default 9000).

| Address       | Type | Meaning                              |
|---------------|------|--------------------------------------|
| `/nam/idx`    | `i`  | 0-based profile index (hot-swap)     |
| `/nam/bypass` | `i`  | 1 = passthrough, 0 = NAM active      |

**Profile hot-swap.** Profile changes take effect within ~10 ms. The loader
runs a background thread — the audio callback never blocks on disk I/O.

**MIDI.** Not used by the NAM stage itself. Upstream stages can still receive
MIDI normally; it passes through the DEMIURGE bus unaffected.

**Known harmless noise.** Same `liblinear_algebra.so` warning as any CSound stage.
`NAMCount: 0 profiles found` is correct if the profile directory is empty — the
opcode falls back to passthrough.

---

## Clock (`demiurge_clock`)

Not a user language — it's a system service — and the launcher always supervises it implicitly. You never declare it in `live.conf`. The `link =` line in `live.conf` is the user-visible knob: on save, the launcher respawns `demiurge-clock` with the new `--link=on/off` flag. See `clock.md` for the full protocol and per‑language reader recipes.

---

## Common pattern across languages

After every program launches, the launcher does three things for it:

1. **Polls** `pw-link -o` for the expected JACK client name (up to 20 × 0.5 s = 10 s).
2. **Links** its output ports into the declared patch graph (or directly to `demiurge-sink:playback_FL/FR` if it's the terminal stage).
3. **Wires MIDI:** `aconnect "Midi Through":0 "<client>":0` and the reverse.

When debugging, look in this order: `journalctl -u demiurge -f` for the launcher's view (resolve_client failures, link/scrub decisions, child-exit notifications), then `~/.demiurge/logs/<id>.log` for the program's own stdout+stderr (this is where a silently-dying stage's real error lives — csound buffer-size errors, ChucK syntax problems, Pd `[loadbang]` missing). For Faust/cpp stages that fail to build, the compiler diagnostic is in `~/.demiurge/logs/<id>.compile.log`.

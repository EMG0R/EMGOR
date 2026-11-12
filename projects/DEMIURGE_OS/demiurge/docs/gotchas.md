# Gotchas

Failure modes you'll hit on DEMIURGE. Each one includes the actual workaround — none of these are theoretical, they cost real debugging time at some point.

---

## Hardware / boot

### Q0. Pi 5 caps USB power at 600 mA total — a second USB audio interface silently fails

The Raspberry Pi 5 defaults to a **600 mA total current limit across all four USB ports combined**. One bus-powered USB audio interface (~400 mA for a Scarlett Solo) fits. A *second* interface — any Scarlett, most class-compliant boxes — pushes past the ceiling and the Pi refuses to enumerate it with almost no log output. Symptom: "works on my laptop, works on my old Pi, will not turn on here no matter what cable/port I use."

**Fix (requires the official 5V / 5A USB-C PSU):** add `usb_max_current_enable=1` to `/boot/firmware/config.txt` and reboot. This raises the cap to ~1.6 A per port / ~5 A total — enough for two Scarletts plus a USB MIDI controller. DEMIURGE ships this flag pre-set in `config/pi5-performance/config.txt.snippet`.

**Never enable this flag with an underpowered PSU.** A 3 A phone charger browns out when USB devices start drawing real current, and the Pi hard-resets mid-session.

### Q0a. M8 perf config in the repo ≠ live on the Pi

`setup-demiurge.sh` Phase 7 installs the governor service, thermal watchdog, and RT limits, but **prints a NOTE that `config.txt` and `cmdline.txt` are manual**. Easy failure mode: audit the repo configs, declare the Pi "perf-tuned", while the Pi is still on `ondemand` with zero core isolation and is nowhere near what the configs specify.

Before diagnosing any perf issue, run the live checks:

```bash
cat /proc/cmdline                                              # expect threadirqs usbcore.autosuspend=-1 isolcpus=3 nohz_full=3 rcu_nocbs=3
cat /sys/devices/system/cpu/isolated                           # 3
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor      # performance
systemctl is-active demiurge-power.service demiurge-thermal-watchdog.timer
```

If any of those are wrong, the repo is fine — the install just didn't run.

### Q0b. Stock Raspberry Pi OS kernel has no `CONFIG_NO_HZ_FULL`

`nohz_full=3` on the cmdline is **rejected**, not merely inert: the kernel logs `Housekeeping: nohz unsupported. Build with CONFIG_NO_HZ_FULL` and the sysfs file `/sys/devices/system/cpu/nohz_full` **doesn't exist** on the stock Pi kernel. `rcu_nocbs=3` is rejected too — both appear in the kernel's `Unknown kernel command line parameters` line (verified by `dmesg` on 2026-08-10). Don't spend time chasing why, and don't quote either token as part of the isolation. `isolcpus=3 threadirqs` + `performance` governor + the per-process RT pin is the real win; skipping full dyntick costs a few µs of tick-interrupt jitter per isolated core per ms, negligible at 48 kHz / 128-frame quantum. Building a custom kernel to flip the Kconfig isn't worth it.

### Q0d. `isolcpus=3` without `CPUAffinity=3` wastes the isolated core

`isolcpus=3` on the kernel cmdline only tells the general scheduler **never to put anything on CPU 3**. It does **not** move anything *to* CPU 3. Without an explicit affinity pin on `demiurge.service`, the launcher and its audio children keep floating across CPU 0–2 alongside browser tabs / systemd housekeeping — you pay the cost of losing a quarter of the machine to the general workload and get none of the jitter-isolation benefit. The fix is the `cpu-affinity.conf` drop-in (`[Service] CPUAffinity=3`) installed by `setup-demiurge.sh` Phase 6. Verify with `taskset -p $(systemctl show demiurge.service -p MainPID --value)` — the final hex should be `8` (= CPU 3 only), not `f` (all cores). If a chain is too dense for one core, widen to `CPUAffinity=2-3` **and** add CPU 2 to the `isolcpus` token in the same boot — otherwise the general scheduler will co-schedule unrelated userspace on CPU 2 and the pinning does nothing.

### Q0e. USB interface cold-boot bit-crush — cleared by a re-enumeration bounce

**Symptom:** on a hard power cycle with a USB interface plugged in, engine output sounds bit-crushed / aliased from the moment audio starts. Physically unplugging and replugging the USB cable clears it instantly. Every numeric indicator — `/proc/asound/*/hw_params`, `pw-metadata clock.force-rate`, csound log `sr=`, ChucK `--srate:` — reads 48000 in both the broken and clean states. PipeWire reports no resampler nodes, no format mismatches, zero xruns.

**Root cause:** on cold power-on the interface's internal ADC/DAC clock circuit comes up in a state that doesn't match what the USB enumeration reports. This is a property of bus-powered devices that lose power with the host, so it affects hot-pluggable interfaces generally and cannot affect a permanently-powered card (I2S HAT, HDMI, on-SoC codec) at all. The clock *reports* 48000 but *runs* slightly off. Sysfs unbind/bind resets the kernel driver but not the firmware state, so a bounce before pipewire is settled has no effect. What clears it is the same thing a physical replug does: cutting USB bus power after the audio graph is fully up and consuming the device.

**Fix (current):** the launcher's `ensure_usb_rates_stable()` (`src/demiurge-launcher-rs/src/util.rs`) does it condition-driven at startup — only when a USB audio node actually exists — by calling `/usr/local/bin/demiurge-audio-bounce.sh` (sysfs unbind → 1s → bind → WP settle) and then hammering an enforce loop until every node reports the target rate. The bounce script derives its device set from the attached sound cards and contains no vendor IDs, so any brand of USB interface gets the same recovery; a permanently-powered card makes it a silent no-op.

**Retired 2026-08-10:** `demiurge-scarlett-bounce.service` and `demiurge-scarlett-rebounce.{service,timer}`. They were gated on USB vendor ID 1235, so they did nothing for any other interface; the rebounce additionally stopped and restarted `demiurge.service` at every boot and **failed** whenever WirePlumber took more than 15s to come up, costing 15s of boot and leaving a permanent entry in `systemctl --failed`. Condition-driven recovery in the launcher covers the same case without either problem.

**Files:** `config/pi5-performance/demiurge-audio-bounce.sh`, `src/demiurge-launcher-rs/src/util.rs`.

**Manual escape hatch** if audio ever comes up wrong: `sudo /usr/local/bin/demiurge-audio-bounce.sh && sudo systemctl restart demiurge` — or just unplug and replug the cable, which is the same thing.

**WP pattern gotchas** found while diagnosing this:
- `device.bus = "usb"` in `monitor.alsa.rules` silently never matches — `device.bus` is on the Device object, not the Node. Use `node.name = "~alsa_(output|input)\\.usb-.*"` instead.
- WirePlumber creates an invisible split-parent node named `alsa_input.hw_USB_0` that holds the actual ALSA open for multichannel inputs. Without a `hw_.*` match clause in the WP USB rule, this parent opens at its negotiated rate (may be 44100) and silently resamples into every split channel. See `config/wireplumber/50-demiurge.conf` Rule 2 comments.

### Q0c. `video=...,rotate=...` on cmdline is silently ignored by Pi 5 KMS

`vc4-kms-v3d` on Pi 5 parses the `rotate=` suffix but does not apply it — `sudo cat /sys/kernel/debug/dri/1/state` still reports `rotation=1` (DRM_MODE_ROTATE_0) and the framebuffer stays landscape. Rotate at the application layer instead (Kivy `Window.rotation`, `xrandr`, or a compositor) — or flip the panel physically. Not worth fighting the driver.

### Q0f. Two live.confs exist — only `~/demiurge/live.conf` is the launch target

`/boot/firmware/demiurge.conf` sets `launch = /home/gorpi7/demiurge/live.conf` (repo copy: `DEMIURGE_OS/demiurge/live.conf`). `~/NEPTR_phase4/live.conf` ALSO exists and its header used to claim it was the launch target — it is not; it's a leftover from an earlier wiring. Its `chain =` / `quantum =` are dead config.

**Symptom (cost a real session, 2026-06-11):** edited `quantum =` in the NEPTR live.conf, synced, restarted demiurge — csound came back with the OLD `-b`. The launcher never read that file.

**Rule:** to change quantum / sync_layer / chain, edit `~/demiurge/live.conf` on the Pi AND `DEMIURGE_OS/demiurge/live.conf` in the repo (no automatic sync between them — `sync_to_pi.sh` only pushes the NEPTR tree). Verify what's actually running with `ps aux | grep csound` after restart. Also note: in bypass mode there is NO hot-reload (the launcher exec'd into csound), so live.conf edits always need `sudo systemctl restart demiurge` — or, without a tty for sudo: `kill -9 $(systemctl show demiurge -p MainPID --value)` and let `Restart=on-failure` respawn it.

---

## Audio / PipeWire / JACK

### Q1. Pd's `-alsa` backend is not PipeWire-compatible

Csound with `-odac` hits ALSA `default` PCM which pipewire-alsa intercepts, routes to `demiurge-sink`, works fine. Pd with `-alsa` probes raw hw devices differently and silently produces no audible output through the PipeWire virtual layer. **Never use `-alsa` with Pd on DEMIURGE. Use `-jack`.**

### Q2. `libjack-jackd2-dev` + pipewire-jack collision

Installing `libjack-jackd2-dev` (required for `faust2jackconsole` to build) pulls in jackd2's `libjack.so.0`, which becomes the default `/usr/lib/aarch64-linux-gnu/libjack.so.0`. Any app that dynamically links libjack now loads jackd2's real libjack and sits waiting for a non-existent `jackd` server. Silent hang.

**Fix:** Prepend `LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack` when launching affected apps. The `pw-jack` wrapper is supposed to do this but is unreliable on this Pi — set the env var directly. The DEMIURGE launcher handles this automatically for Pd and Faust.

### Q3. Verify `LD_LIBRARY_PATH` took effect with `/proc/<pid>/maps`

`ldd $(which pd)` shows compile-time linking — always prints the jackd2 path regardless of runtime overrides. The actual loaded libjack is only visible in `/proc/<pid>/maps | grep libjack` on a running process.

### Q4. PipeWire JACK clients do not auto-link to a non-default sink

A JACK client's outputs auto-route only to the **default sink**. DEMIURGE keeps Scarlett as the default sink (see Q5 for why), so the launcher must explicitly `pw-link <client>:output_1 demiurge-sink:playback_FL` after the client registers.

The inverse also bites: WirePlumber's default routing policy will speculatively wire _incoming_ links on any newly-appeared JACK client's input ports — mic capture, speaker monitors, even other chain programs' outputs. On a cold boot with a 6-stage chain the symptoms are brutal:

- Raw ChucK (stage 0) gets auto-linked into `cpp_crush:in_1/2` alongside the legitimate `faust_delay:out_0/1`. You hear bitcrushed audio because the bitcrusher is processing the raw sine pad.
- The Scarlett mic capture gets fanned out into every stage's input port. You hear live-room bleed through the chain.
- `alsa_output.<hw>:monitor_FL` gets routed into `SuperCollider:in_2`, closing a feedback loop through the speakers.

The launcher handles this in `graph::scrub_stray_session_links`: after applying the desired graph, it walks every session program's input ports and deletes any incoming link that isn't in the desired set — regardless of source. Hardware capture is only permitted when the chain explicitly declares `in1 ->` or `in2 ->` (mono, numbered — there is no stereo input pair on DEMIURGE). Every scrubbed link is logged to `journalctl -u demiurge` as `scrub stray: <src> -X-> <dst>`.

### Q5. `demiurge-sink` as default sink creates a loopback cycle

`demiurge-sink` is a PipeWire loopback: a virtual sink node and a stream-output node. The stream-output node follows the default sink. If default is `demiurge-sink`, the stream-output targets its own input → loop → PipeWire silently drops the path → no audio anywhere.

**Keep Scarlett as the default sink.** The pinning is enforced by `/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf`, which gives `demiurge-sink` / `demiurge-sink-output` `priority.session = 0` and `node.dont-reconnect = true`. Both flags together are load-bearing: priority alone lets WirePlumber destroy the loopback when a higher-priority sink appears, and `dont-reconnect` alone still allows promotion-to-default. With both, the sink stays a passive virtual hop nothing competes with, and the launcher's explicit `pw-link` approach is never racing WirePlumber's auto-routing.

### Q6. `wpctl status` shows `demiurge-sink` under "Filters", not "Sinks"

Because it's a loopback filter node. It still acts as a sink for `PIPEWIRE_NODE=demiurge-sink`, but `wpctl status | grep Sinks` won't show it. Check the **Filters** section, or use `pw-link -i | grep demiurge`.

### Q7. `demiurge-sink` vs `demiurge-sink-output`

- `demiurge-sink` (Audio/Sink node) — what apps **write to**. Target port: `demiurge-sink:playback_FL/FR`.
- `demiurge-sink-output` (Stream/Output/Audio node) — the **other side** of the loopback, pushes audio to the physical device. Source port: `demiurge-sink-output:output_FL/FR`.

Do not confuse the two when reading `pw-link -l`.

### Q7g. Direct-output mode: the chain bypasses the loopback when there's ONE wired device

**Why it looks like a bug but isn't.** The general rule (Q5/Q7 + `scrub_hw_playback`) is "only `demiurge-sink-output` may drive the physical playback ports; every stage routes through `demiurge-sink`." That loopback exists **purely as a mixing point** for aggregation — multiple USB interfaces, or a BT speaker taking the front pair. It also costs a **full quantum of output latency** (a loopback's stream side runs one cycle behind its sink side).

When there is **exactly one wired USB sink and no Bluetooth**, there is nothing to mix, so the launcher routes the chain's last stage **straight to that device's `:playback_FL/FR`**, skipping `demiurge-sink` entirely. That removes the output quantum (input was already direct via `__Mic{n}__source:capture_MONO`). At 128/48k that's ~2.7 ms off the round trip — the difference people feel as the sync layer being "~2–3× raw."

**Single source of truth:** `aggregate::direct_output_device()` returns `Some(dev)` iff the sink list is exactly one `alsa_output.usb-*` (no `bluez_output.*`, no second interface). Both `graph::out_sink_ports` (where `out` terminates) and `aggregate::apply` (whether the loopback drives the device) consult it, so they can never disagree. The instant a second sink or a BT speaker appears, it returns `None` and routing falls back to the loopback — automatic and self-healing on every `pw-mon` hot-plug event. The switch is **not glitch-free** (it's a reroute), but it only happens when you change the rig.

**So in `pw-link -l` with one Scarlett you will see `csound6:output1 → alsa_output.usb-...:playback_FL` directly, with NOTHING on `demiurge-sink`.** That is correct, not a stray link. `scrub_hw_playback` preserves it because it's in the patch graph's `desired` set; do not "fix" it. Confirm the mode from the launcher log line `graph: OUT direct → ...` vs `graph: OUT via demiurge-sink loopback`.

### Q7a. JACK client `--bufsize` must match `clock.force-quantum` or the client goes silent

DEMIURGE pins the PipeWire graph quantum with `clock.force-quantum = 128` (set by the `demiurge.service` `ExecStartPre=/usr/bin/pw-metadata -n settings 0 clock.force-quantum 128` drop-in) and publishes the same value through `PIPEWIRE_LATENCY=128/48000` in `demiurge-env.sh`. Every JACK client launched by a `demiurge-run-*` wrapper inherits that env var.

If a client's internal buffer size is **smaller** than the system quantum, pipewire-jack accepts the registration, the ports appear in `pw-link -l`, the patch graph links happily, and the client produces **exactly zero samples**. No log, no error, no warning on stdout or stderr — the client just outputs `-inf dB` forever. This bit us twice in the same session:

1. `demiurge-run-chuck` hardcoded `--bufsize:64` from when the graph quantum was 64. After bumping the quantum to 256 for xrun headroom, ChucK registered fine, the launcher logged `link: ChucK:outport 0 → csound6:input1` as expected, but `pw-jack jack_rec ChucK:outport 0` captured pure silence. Fix: `--bufsize:128`.

2. `demiurge-env.sh` hardcoded `PIPEWIRE_LATENCY=64/48000`. Even with the ChucK wrapper bumped to `--bufsize:128`, any client that honored `PIPEWIRE_LATENCY` would still request 64 and silently die. Fix: `PIPEWIRE_LATENCY=128/48000`.

**The rule:** every JACK client's internal block size must equal the system quantum (or be an integer multiple — never a divisor). When you change `clock.force-quantum`, you must touch three places in lockstep:

- `/etc/systemd/system/demiurge.service.d/wait-pipewire.conf` — `ExecStartPre=/usr/bin/pw-metadata -n settings 0 clock.force-quantum <N>`
- `/opt/demiurge/lib/demiurge-env.sh` — `PIPEWIRE_LATENCY=<N>/48000`
- Every `demiurge-run-*` wrapper that passes an explicit buffer flag (`demiurge-run-chuck`'s `--bufsize:<N>` today; any others added later)

**Diagnosing this:** if a stage's ports exist in `pw-link -o` but `pw-jack jack_rec "<client>:<port>"` captures `-inf dB`, check the client's buffer flag against `pw-metadata -n settings 0 clock.force-quantum`. Mismatched = silent.

### Q7c. PipeWire graph rate follows hot-plugged USB devices unless `force-rate` is pinned

`default.clock.rate = 48000` in `99-demiurge-lowlatency.conf` is read at PipeWire startup but is **not** load-bearing at runtime. When a USB audio class device is hot-plugged, the ALSA monitor opens it at whatever rate the device wants — and most class-compliant interfaces (Scarlett, MOTU, RME-class) remember the last-used rate across power cycles. So plug a Scarlett into a Mac at 96 kHz, plug it into the Pi: the device returns at 96 kHz, the PipeWire graph follows, every loopback and SRC in the chain starts resampling between 48 and 96, and audio "sounds like shit" — pitchy, glitchy, intermittent crackle — with no error log. A reboot doesn't fix it because the device still reports 96 kHz on the next plug.

**The fix is layered. All four pieces are load-bearing.**

1. `/etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf` — `default.clock.allowed-rates = [ 48000 ]`. Tells PipeWire that 48 kHz is the *only* graph rate it's allowed to use; any device that requests anything else is forced to 48 k via SRC at the device boundary instead of dragging the whole graph.
2. `/etc/systemd/system/demiurge.service.d/wait-pipewire.conf` — `ExecStartPre=/usr/bin/pw-metadata -n settings 0 clock.force-rate 48000`. Runtime metadata override that survives WirePlumber's initial settings push (same trick as `clock.force-quantum 128`).
3. `/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf` — the USB audio rule sets `audio.rate = 48000` and `audio.allowed-rates = [ 48000 ]`. This tells the WirePlumber ALSA monitor to open the device at exactly 48 kHz, refusing to negotiate down. The device's own preferred rate is overridden at the alsa-monitor level before PipeWire even sees the node.
4. The launcher's `util::pin_clock_metadata()` re-asserts both `clock.force-rate` and `clock.force-quantum` on every `UsbDeviceChange` event (and once at startup). Belt-and-suspenders against any race window where a device joins between the systemd ExecStartPre and the first launcher tick.

`demiurge-virtual.conf` also pins `audio.rate = 48000` on both sides of each loopback so the virtual `demiurge-sink` / `demiurge-source` nodes can never get dragged off-rate even if some other graph member briefly is.

**Diagnosing this:** `pw-metadata -n settings 0 clock.force-rate` should always print `48000`. `pw-top` should show the same number in the `RATE` column for every line. If you see `96000` or `44100` in either, the pin failed — check `journalctl -u demiurge` for a `clock: pinned rate=48000 quantum = 128` line at startup and after every `event: usb device change`. To prove you can reproduce the bug, run `pw-metadata -n settings 0 clock.force-rate 96000` manually with a chain running and listen — that's exactly what hot-plug used to do.

**Quantum/rate lockstep — now ONE knob (2026-10-02).** Set `rate =` and `quantum =` in `~/demiurge/live.conf` and nothing else. `demiurge-quantum-limits lockstep` generates every device-side file from it, as an `ExecStartPre=-` of `demiurge.service` (installed by `config/systemd/demiurge.service.d/10-clock-lockstep.conf`) and from `demiurge-audio quantum|rate`. No sudo: the fragments live in the user config dirs, which shadow same-named `/etc` files:

- `~/.config/wireplumber/wireplumber.conf.d/60-demiurge-period.conf` — `api.alsa.period-size` / `period-num`, both directions. Period = quantum, never below 64, period-num never below 4, ring at least 4 x quantum.
- `~/.config/wireplumber/wireplumber.conf.d/61-demiurge-rate.conf` — `audio.rate` / `audio.allowed-rates` on every `alsa_*` node.
- `~/.config/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf` — `default.clock.*`. Same name as the `/etc` file, so it shadows it.

It restarts WirePlumber (and PipeWire if its fragment changed) only when a generated file changed, only while the engine is down, then pins `clock.force-rate` and `clock.force-quantum`. In bypass mode (`sync_layer = off`) it writes files and leaves PipeWire alone. It never fails the service. `demiurge-quantum-limits lockstep --dry-run --live <file>` prints what it would write. The static repo files (`50-demiurge.conf`, `demiurge-pipewire-lowlatency.conf`) are only 48000 / 128 boot seeds for a rig that never ran lockstep. The old `/etc/.../60-demiurge-period.conf` is retired.

Why: the 2026-10-02 rebuild lost the hand-generated period file, so the device fell back to 128x4 while live.conf said 64, and the rate pins were hardcoded 48000. Check with `demiurge-audit-audio`.

Still outside lockstep (change by hand): `src/demiurge-launcher-rs/src/util.rs` constants, `demiurge-env.sh` `PIPEWIRE_LATENCY`, `demiurge-run-chuck` `--bufsize`. Old history, the six places before lockstep:

When you change `clock.force-rate` *or* `clock.force-quantum`, touch all of these:

- `/etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf` — `default.clock.{rate, allowed-rates, force-rate, quantum, force-quantum}`
- `/etc/systemd/system/demiurge.service.d/wait-pipewire.conf` — both `ExecStartPre` lines
- `/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf` — USB rule `audio.rate` / `audio.allowed-rates`
- `src/demiurge-launcher-rs/src/util.rs` — `FORCE_RATE` / `FORCE_QUANTUM` constants
- `/opt/demiurge/lib/demiurge-env.sh` — `PIPEWIRE_LATENCY=<Q>/<R>`
- `/opt/demiurge/bin/demiurge-run-chuck` — `--bufsize:<Q>`

---

## Csound

### Q7b. Csound `.csd` files need `-B 1024 -b 128` and NO `--sched`

Two traps, both silent:

1. **`-B 128` is below the rtjack minimum.** Csound's JACK backend refuses to start and exits with `rtjack: buffer size (-B) is too small`. The launcher wrapper pipes stderr to `/dev/null`, so all you see in `journalctl -u demiurge` is `program 'csound_...' marked dead` and the next graph-apply pass linking upstream straight through to downstream (walk_to_live degradation). The stage silently disappears from the chain. **Use `-B 1024 -b 128`** — `-B` is the I/O ring size, `-b` is the software block matching the quantum. Any `-B >= 256` technically works, 1024 is the safe default.

2. **`--sched` requires `CAP_SYS_NICE`.** The `demiurge-run-csound` wrapper does not grant that capability, so csound exits with `cannot set scheduling policy to SCHED_RR` before it even initializes rtjack. Same silent-death symptom as above. **Never put `--sched` in a DEMIURGE `.csd`.** `--realtime` alone is fine — it only controls csound's internal scheduling, not the Linux scheduler class.

**Diagnosing a dying csound stage:** the launcher hides stderr. Run the wrapper directly to get the real error:

```bash
/opt/demiurge/bin/demiurge-run-csound ~/demiurge/examples/your_file.csd
```

The first line of csound output usually tells you which of these (or something else) is the cause.

### Q7c. In bypass mode csound's ALSA backend ignores `-B` — the ring is always 1024 frames, so `quantum` does NOT control latency

Under `sync_layer = off` the `demiurge-run-csound` wrapper drives ALSA directly with `-B $((Q*8)) -b $Q`. **The `-B` half is inert.** Measured on the HiFiBerry DAC+ADC Pro (2026-08-10), the negotiated playback ring is `buffer_size: 1024` in *every* configuration tried:

| varied                    | values tested                | resulting `buffer_size` |
| ------------------------- | ---------------------------- | ----------------------- |
| `-B` (hardware buffer)    | 128, 256, 512, 1024, 2048, 4096, 8192 | **1024, always** |
| `-b` (quantum)            | 32, 64, 128, 256             | **1024, always** |
| device string             | `plughw:` vs `hw:`           | **1024, always** |
| `--realtime`              | present vs absent            | **1024, always** |
| `-+ignore_csopts=1`       | present vs absent            | **1024, always** |

Csound reports it itself at startup — `ALSA output: total buffer size: 1024, period size: 64` — regardless of what `-B` asked for. The driver is *not* the constraint: `aplay --buffer-size=128/256/512/2048` on the same card gets exactly what it asks for. The 1024 is imposed by csound's ALSA backend.

**Root cause — confirmed in csound's source, it is an upstream bug.** In `InOut/rtalsa.c`, `open_device()` zeroes the device struct and then copies exactly one of the two buffer parameters across:

```c
memset(dev, 0, sizeof(DEVPARAMS));
...
dev->period_smps = parm->bufSamp_SW;   /* -b  — copied  */
/* dev->buffer_smps = parm->bufSamp_HW;   -B  — NEVER copied */
```

`parm->bufSamp_HW` (the value of `-B`) is never assigned to `dev->buffer_smps`, so it stays 0 from the `memset`. `set_device_params()` then hits its own fallback:

```c
if (dev->buffer_smps == 0)
  dev->buffer_smps = 1024;
```

That is the 1024. **No command-line flag can reach it** — the value is dropped before any option processing matters, which is why `-B`, `hw:` vs `plughw:`, `--realtime` and `-+ignore_csopts=1` all measure identically.

The same function also explains why the ring sits *full* rather than merely being large — for playback the start threshold is set to the whole buffer:

```c
n = (play ? dev->buffer_smps : 1);
snd_pcm_sw_params_set_start_threshold(dev->handle, sw_params, n);
```

ALSA does not start the stream until all 1024 frames are queued, and csound then tops it up a period at a time. So **bypass output latency ≈ `buffer_size` exactly**, and capture (threshold 1) is ~free. That matches the measured `delay ≈ 950–1010` / `0–48`.

**Upgrading csound does not fix it.** The identical code is present on the `csound6`, `master` and `develop` branches (verified 2026-08-10) — i.e. in 6.18.x *and* in the in-development 7.x. Csound 7 has no stable release as of 2026-08; 6.18.1 (Nov 2022) is still the last stable and 6.x is EOL. **Do not upgrade csound hoping to fix latency** — it would gain nothing and risks the NAM opcode plugin, which is built against the installed ABI and lives only in `/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/`.

**The one untested lever left for bypass:** since csound asks ALSA for 1024 via `snd_pcm_hw_params_set_buffer_size_near()`, the *device* can refuse. An ALSA config PCM whose `slave` block pins `buffer_size`/`period_size` constrains that refinement, and csound honours the result — it re-reads the negotiated value (`dev->buffer_smps = nn`) and prints `ALSA: -B 1024 not allowed on this device; using N instead`, which also drops the start threshold to `N`. Sketch, in `~/.asoundrc` on the Pi:

```
pcm.demlowlat {
    type plug
    slave { pcm "hw:sndrpihifiberry,0"  buffer_size 256  period_size 128 }
}
```

then point the bypass device at `demlowlat`. Expected result: `delay ≈ 256` frames ≈ 5.3 ms instead of 19.4 ms. **NOT YET MEASURED** — needs a run on the rig before it goes anywhere near the wrapper.

**Consequence — this is the important part.** csound keeps that ring nearly full (`avail` sits at one period), so `/proc/asound/<card>/pcm0p/sub0/status` reads `delay ≈ 950–1010` frames at *every* quantum. That is **~20.4 ms of fixed output latency** that `quantum` cannot touch. Capture is the opposite — `delay ≈ 0–48` frames, essentially free. So in bypass mode:

> **Round-trip ≈ 20.5 ms, set by the DAC ring, not by `quantum`.**

Dropping `quantum` 64 → 32 buys **zero** measurable latency and only raises the wakeup rate. Do not chase it. Conversely, *raising* `quantum` costs no real latency either, so it is a free stability knob: on the NEPTR patch (csound ~77 % of a core) a 3-minute window with the UI and touch daemon running logged **154 xruns at `quantum = 64` vs 72 at `quantum = 128`** — half the xruns, same measured `delay`.

Rules of thumb for bypass mode on this card:
- Tune `quantum` for **CPU headroom**, not latency. 128 is the better default.
- If you need real low latency, the 1024-frame ring is the thing to attack — not the quantum. `sync_layer = on` (PipeWire sizes the ring from the quantum) is the only currently-known lever, at the cost of the sync-layer hops.
- Remaining xruns at any quantum are a **DSP-load** problem, not a buffering one.

**Sync mode is the fix, and it is 10× better (measured 2026-08-10).** With `sync_layer = on`, `quantum = 64` and the platform ring pinned to 128×4, the same patch on the same card measures `delay ≈ 93 frames ≈ 1.94 ms` (worst case 2.67 ms) against bypass's `932 frames ≈ 19.4 ms`. The reason is the inverse of the bypass bug above: PipeWire writes to the DMA ring **just-in-time** instead of keeping it full, so the delivered latency is set by the *quantum*, not by the ring size.

Full sweep, UI + touch daemon running, ≥90 s per point:

| mode | ring (frames) | quantum | `delay` | worst | xruns |
|---|---|---|---|---|---|
| bypass, `-+rtaudio=alsa` | 1024 (csound hardcoded) | any | 932 fr | **19.4 ms** | — |
| sync, ring untuned | 32768 (PipeWire default) | 128 | 208 fr | 4.33 ms | — |
| sync, ring 256×4 | 1024 | 128 | 192 fr | 3.99 ms | 0 |
| **sync, ring 128×4** | **512** | **64** | **93 fr** | **1.94 ms** | **0** |
| sync, ring 256×4 | 1024 | 64 | csound dies | — | — |
| sync, ring 64×4 | 256 | 32 | csound dies | — | — |

**Two separate effects live in that table. Conflating them wastes hours.**

1. **Ring depth is not a latency lever.** At `quantum = 128`, shrinking the ring **32×** (32768 → 1024) moved delivered latency by *seven frames*. A deeper ring buys xrun headroom and nothing else. This is the exact inverse of bypass, where csound fills its ring so the ring *is* the latency and `quantum` does nothing.

2. **`api.alsa.period-size` is a floor on `quantum` — which is how the ring matters after all.** PipeWire hands the device one period per graph cycle, so a quantum below the pinned period size cannot be satisfied: csound xrun-storms (`rtjack: xrun in real time audio`), fails to hold its PipeWire registration, and the launcher logs `resolve_client: FAILED` then `marked dead`. Measured: **`quantum = 64` FAILS at `period-size = 256` and WORKS at `period-size = 128`.**

So the rule is: **lower `period-size` in order to unlock a lower `quantum`; the quantum is what actually buys the latency.** Halving quantum 128 → 64 halved the latency, 3.99 ms → 1.94 ms.

The floor on this patch is `quantum = 64`. `quantum = 32` could not keep csound alive (pre-DSP-gating; may be worth retesting once the NAM/FREEZE/H90 gates are deployed, since that failure is CPU-driven, not geometric).

### Q7c-i. A too-shallow PipeWire ring hard-locks the Pi, and the watchdog turns that into a reboot

Sweeping `api.alsa.period-size`/`period-num` downward on the HiFiBerry, `period-size = 64` with `period-num = 2` — a **128-frame (2.7 ms) ring** — locked the machine solid: no ssh, no console, no clean shutdown in the logs.

Two things make this worse than an ordinary xrun storm:

1. **It is an RT-priority livelock, not a dropout.** PipeWire's data thread runs at `SCHED_FIFO 83`, above csound (74) and just under the audio IRQ (85). A ring that shallow means a wakeup every ~1.3 ms; when it can't be met, the RT thread doesn't yield politely — it starves every `SCHED_OTHER` task on the box, including sshd and the network stack.
2. **systemd runs a hardware watchdog with a 60 s timeout** (`bcm2835-wdt`, visible in `dmesg`). Once userspace stops being scheduled the watchdog fires and the Pi **reboots**. So the symptom is not a hang you can debug — the box vanishes and comes back with `uptime` near zero.

Consequences worth internalising before the next sweep:

- **`/tmp` does not survive it.** Back up configs to `$HOME`, never `/tmp`, or the known-good copy is gone exactly when it's needed. (`journalctl` is volatile here too — `--list-boots` shows only the current boot, so there is no post-mortem log from the crashed boot either.)
- **A timer-based auto-revert does not work.** A starved box cannot run the timer, and after the reboot it comes straight back up in the lethal config. The revert has to be a **boot-time** unit that restores the known-good conf on next boot and then disables itself.
- **Sweep from the safe end downward,** so a lockup ends the run instead of hitting on step one with the whole matrix still queued.
- Do not go below a **256-frame total ring** on this card. There is nothing to win: see the table above — the ring is not the latency lever.

### Q7d. Phase vocoder overlap collapses to zero if FFT size equals hop size

PVS opcodes (`pvsanal`, `pvsfread`, etc.) require hop size ≤ FFT size / 2 for meaningful overlap-add reconstruction. If `giHop = ksmps` and `giFFT = ksmps`, you get exactly zero overlap — every frame is independent, spectral continuity is broken, and the output sounds bit-crushed / ring-modulated / xrun-like even though `pw-top` shows zero errors.

**Symptom:** spectral effects (delays, freeze, pitch shift) sound heavily distorted at all dry/wet ratios. No xruns in `pw-top`, no errors in journalctl.

**Root cause vector:** quantum change. DEMIURGE lets you set `quantum =` in `live.conf`. If you increase the quantum (e.g. from 128 to 256 for headroom), csound's `ksmps` follows, and `giHop = ksmps` doubles. If `giFFT` was already equal to the old `ksmps`, the ratio collapses to 1:1.

**Fix:** pin `giFFT` to an absolute value at least 4× the nominal `ksmps`:

```csound
giFFT = 1024   ; at ksmps=128 → 8x overlap; at ksmps=256 → 4x overlap
giHop = ksmps  ; MUST equal runtime ksmps — never set to a literal
giWin = giFFT  ; analysis window = FFT size
```

At `ksmps=128` this gives 8× overlap (excellent quality). At `ksmps=256` it gives 4× (still clean). Setting `giFFT = ksmps` or `giFFT < 4*ksmps` produces audible garbage.

### Q7e. `demiurge-env.sh` on the Pi can lag behind the repo after a quantum change

`/opt/demiurge/lib/demiurge-env.sh` contains `PIPEWIRE_LATENCY="${PIPEWIRE_LATENCY:-128/48000}"`. This is a conditional set — it only takes effect if the variable is **unset**. However, the installed copy on the Pi may have an older hardcoded value (e.g. `PIPEWIRE_LATENCY=256/48000` without the conditional) if it was installed before the quantum was tuned.

**Symptom:** `DEMIURGE_QUANTUM=128` is correct in csound/ChucK's environment but `PIPEWIRE_LATENCY=256/48000` is also present, causing pipewire-jack to negotiate a 256-sample JACK callback even though the system quantum is 128. `pw-top` shows `QUANT=256` for csound6 and ChucK instead of 128. The audio works but latency is double what it should be.

**Diagnose:** `cat /proc/$(pgrep csound | head -1)/environ | tr '\0' '\n' | grep PIPEWIRE_LATENCY`

**Fix:** redeploy the wrapper files: `scp src/wrappers/demiurge-env.sh gorpi7@demiurge.local:/tmp/ && ssh gorpi7@demiurge.local "sudo cp /tmp/demiurge-env.sh /opt/demiurge/lib/demiurge-env.sh"`. Then `sudo systemctl restart demiurge`.

### Q7f. Bypass mode: live.conf `quantum` IS csound's `-b` — CsOptions are ignored

In bypass (`sync_layer = off` / `bypass = on`), `demiurge-run-csound` launches csound with `-+ignore_csopts=1`, so **whatever `-b`/`-B` is written in the `.csd`'s `<CsOptions>` does nothing**. The real flags come from live.conf: `-b = quantum`, `-B = quantum × 8`. Editing the `.csd` to "lower the buffer" silently changes nothing; editing `quantum =` is the one lever. Verify what's actually running with `ps aux | grep csound` (the full flag line is visible) or the `audio buffered in N sample-frame blocks` line in the stage log.

**The latency ladder (bypass, 48 kHz, measured 2026-05-14):**

| `quantum` (= `-b`) | software RTT (≈ 2×b) | total RTT (+ ~1.7 ms USB/DAC) |
|---|---|---|
| 256 | 10.7 ms | ~12 ms |
| 128 | 5.3 ms | ~7 ms |
| 64 | 2.7 ms | ~4 ms (USB floor — below 64 the 125 µs-microframe URB granularity and the interface's internal buffering dominate; not reliable) |

**`ksmps` must move in lockstep, downward only.** csound forces `-b` up to a multiple of `ksmps`, so `quantum = 64` with `ksmps = 256` silently runs at `-b 256` — the live.conf edit appears to do nothing. Drop `ksmps` in the orchestra header to match the quantum. Two invariants when lowering it:

1. Every `pvsanal` hop in the chain must stay **≥ ksmps**. A hop below ksmps emits more than one fsig frame per k-cycle while downstream `pvsynth` consumes one — stale-frame state that can crash at FX-engage (the PFORK v3 fix). Hops at or above ksmps are the normal, safe case.
2. Pin hops/FFT sizes to literals, not `ksmps` (see Q7d) — already done in NEPTR (`giHop = 256` locked), which is exactly what makes lowering ksmps cheap: spectral CPU is hop-driven and does not rise when ksmps falls. Only k-rate glue code scales with 1/ksmps.

**"Dynamic ksmps" is not a thing.** `ksmps` and `-b` are fixed at csound compile/device-open time; `setksmps` only goes *lower* inside an instrument, never higher, and nothing can change the hardware period at runtime. The only runtime switch is a csound restart (`sudo systemctl restart demiurge` after a live.conf quantum edit, ~3 s gap). In practice it's also unnecessary: heavy effects (pvs chains, sndwarp granular) cost the same at any ksmps, so if the worst-case preset survives a soak test at the small quantum, run it there permanently.

### Q7g. Bypass mode: playback-only cards (HDMI, output-only DACs) restart-loop with `-iadc`

Csound in bypass opens BOTH directions (`-odac:` + `-iadc:` on the same device). A card with no capture PCM — HDMI (`vc4hdmi*` have only `/proc/asound/<id>/pcm0p`), playback-only USB DACs — makes `-iadc` fail at device-open, csound exits, systemd restart-loops. Fix (2026-08-06, corrected 2026-08-09): the launcher checks `/proc/asound/<card-id>/` for any `pcm*c` entry; if none, it exports `DEMIURGE_AUDIO_CAPTURE=none` and `demiurge-run-csound` passes `-iadc:null` (ALSA null plugin) instead of the real device — the capture buffer stays calloc-zeroed, so input opcodes (`ins`/`inch`) read silence and patches that process live input run silently instead of crashing. Merely omitting `-iadc` does NOT work: csound 6.18 still honours a bare `-iadc` in the CSD's `<CsOptions>` even under `-+ignore_csopts=1` and restart-loops on the unopenable `default` capture device. Generic, not HDMI-special-cased. Separately: the vc4-hdmi driver returns error 524 (`ENOTSUPP`) on playback open when the attached display's EDID advertises no audio — HDMI sound needs an audio-capable sink.

### Q7h. Any `write()` from an audio thread is a dropout generator — journald, stderr, or a file. Check libcsound's message handler too.

Found three separate ways, three separate nights, same root cause every time:

1. **`printks` in the orchestra** → journald → **829 xruns in 120 s**. Fixed by putting the prints behind an `#ifdef`.
2. **Per-CC MIDI debug prints** (`DRIVELEV` and friends) → same cause, same result.
3. **libcsound's own default message handler** → `~/.demiurge/logs/<stage>.log`, **on the SD card** → ~3 xruns/s under `csound_host = embedded`.

(3) is the one worth internalising, because the code looked correct. `demiurge-csound-host.cpp`'s `process()` is scrupulous about never printing and says so in its comments — but it calls `csoundPerformKsmps()`, and **any message the engine or the orchestra emits during that call goes to Csound's default handler, which does `vfprintf(stderr, ...)`**. The author covered their own printfs and left libcsound's. The trigger was NEPTR's own master clock broadcasting BPM on ch16 CC119, which Csound answers with `ctrl 119 has no exclus list` — 2.5 writes/s, forever, whether or not any effect is engaged.

**Why `standalone` hides all three and `embedded` cannot.** Standalone Csound computes on its own engine thread behind the `-B` ring: a print stalls that thread and the ring covers it. The embedded host has no ring *by design* — that is the entire point of it — so a stalled compute **is** a dropped buffer **is** an audible click.

**The tell is that the clicking is load-INDEPENDENT.** It persisted with the amp models (~49% of DSP load) bypassed. Anything that tracks the *clock* rather than the *DSP* is not a headroom problem, and no amount of shrinking the patch will fix it.

**It is not enough to keep prints off the audio thread.** Every thread in the host runs at `rtprio 74`, and the `cpu-affinity.conf` drop-in pins them all to the isolated core (`isolcpus=3`). In the CC119 case the prints were on Csound's ALSA-seq **MIDI** thread, not the audio thread — a blocking SD write there stalls the box's only audio core just as effectively. **The rule is per-process, not per-thread: nothing in an RT audio process may touch the disk except a dedicated low-priority drainer.**

Fix, in `demiurge-csound-host.cpp`: `csoundSetMessageCallback` installed *before* `csoundCompile`. Messages off the audio thread print immediately (compile diagnostics, plugin banners — no deadline to miss); messages on it are `vsnprintf`'d into a fixed lock-free SPSC ring and drained by the supervise loop at 20 Hz. Ring overflow is counted and reported **unconditionally**, because the reason this survived so long is that the evidence was behind an unset env var.

**Raising `quantum` masks this and is not a fix.** 64 frames = 1.33 ms budget; 128 = 2.67 ms. Baseline DSP was ~400 µs, so a ~1 ms write stall overruns the small budget and not the large one. Going 64 → 128 silenced the clicking with the spam rate completely unchanged — which is how the mechanism was confirmed, but it buys silence with 1.33 ms of latency.

**Diagnosis, in order:**

```bash
# 1. Is the node actually xrunning? ERR column, watch it CLIMB.
#    Do NOT trust the host's own counter — it only prints under DEMIURGE_HOST_STATS=1.
pw-top -b -n 30 | grep csound6

# 2. Is something writing from the audio process? If this grows, that is your bug.
a=$(stat -c %s ~/.demiurge/logs/<stage>.log); sleep 10
b=$(stat -c %s ~/.demiurge/logs/<stage>.log); echo "+$((b-a)) bytes/10s"

# 3. What is actually being printed?
tail -400 ~/.demiurge/logs/<stage>.log | sort | uniq -c | sort -rn | head
```

A log-growth rate that matches the xrun rate is the case closed. Here it was 2.5 writes/s against 2.9 xruns/s.

**Reading the health of the engine** (`DEMIURGE_HOST_STATS=1`, printed every 5 s and unconditionally at exit):

```
[stats] budget=1.33ms mean=19% p50<=12% p95<=50% p99<=56% max=205%
        over-budget=2/104987 (0.002%) rt-msgs=0 drop=0
```

Percentages are of one JACK period. **A mean cannot tell you whether you are about to click; the tail can** — judge headroom by `p99`, not `mean`. `over-budget` counts callbacks that finished late (each one a dropped buffer). A `max` far above `p99` that never recurs is startup (NAM model load, cold cache) and is not interesting. `rt-msgs` is how many messages the orchestra emitted *from the audio thread* — **it should be 0**; anything else means someone added a print. Note `xruns=` on the adjacent line is **graph-wide** (`jack_set_xrun_callback` fires for the whole graph), so it can climb while this host is blameless — cross-check the per-node `ERR` in `pw-top` before blaming the engine.

---

## Pure Data

### Q8. Pd's DSP engine is OFF by default under `-nogui`

Pd loads the patch, opens JACK, registers ports, runs forever, and produces zero samples because DSP is off and nobody turned it on. **Every DEMIURGE Pd patch must include a `[loadbang] → [; pd dsp 1(` trigger.** Without this, `-nogui` Pd is silent. A running Pd with JACK ports registered but no audio is almost always this bug.

### Q9. Pd prints nothing on silent failure

An empty log file does NOT mean Pd is working quietly — it usually means Pd failed silently. Run Pd with `-verbose` to get actual init messages (audio backend init, DSP state, MIDI device open).

### Q10. Pd `.pd` file object index counting

When hand-editing a `.pd` file, the `#X connect A_idx out B_idx in` indices are zero-based across **all** `#X text`, `#X array`, `#X obj`, `#X msg`, `#X floatatom`, etc. `#N canvas` and `#A <data>` do not count. `#X text` **does** count. Getting this wrong silently breaks connections.

---

## Faust

### Q11. `faust` apt package doesn't pull in `libasound2-dev` / `libjack-jackd2-dev`

`faust2jackconsole hello.dsp` fails at the C++ compile step with `Package alsa was not found in the pkg-config search path`. Install both dev packages first. (And accept the Q2 consequence for jack.)

### Q12. Faust stages in `live.conf` reference the `.dsp` source, not the compiled binary

```
chain =
  ~/demiurge/examples/hello_patching/faust_crush.dsp
```

Drop the `.dsp` path straight into `chain =`. The launcher invokes `faust2jackconsole -midi` on first run to produce the sibling binary and caches it; subsequent runs reuse the cached binary unless the `.dsp` is newer. You never reference the compiled output directly from `live.conf`.

---

## Shell / terminal / session

### Q13. `sudoedit` / `nano` fail with `Error opening terminal: xterm-ghostty`

Ghostty sets `TERM=xterm-ghostty` which isn't in the Pi's terminfo database. Any curses tool fails. **Fix:** `TERM=xterm-256color sudo nano /boot/firmware/demiurge.conf`. Prefix every TTY tool invocation with `TERM=xterm-256color`.

### Q14. `pkill -9 pd` spams "Operation not permitted" for kernel threads

pkill matches against kernel threads like `kswapd` and `irq/174-vc4 hdmi hpd connected`. Scope to user: `pkill -9 -u $USER pd`.

### Q15. Background SSH commands need `&` inside the quoted command

`ssh host 'pd ... &'` backgrounds pd on the Pi. `ssh host 'pd ...' &` backgrounds the entire ssh session on the Mac. Different things.

---

## MIDI loop danger

### Q16. Bidirectional shared MIDI bus can create feedback loops

Every language reads from AND writes to the shared `Midi Through` bus. If a language echoes its input to its output (even unintentionally — e.g. a Pd patch with `[notein] → [makenote] → [noteout]` as a passthrough), that echoed message goes back into the bus and every other language sees it again, including the original sender.

**Guardrails:**
- The DEMIURGE `hello_world` and `hello_patching` files are written so that nothing echoes input to output blindly.
- If you write your own patches and notice MIDI freaking out, check first whether your patch is re-sending received messages.
- A temporary diagnostic: `aconnect -d "Midi Through" "<suspect>"` to break the output side of one language and see if the storm stops.

---

## Diagnostic techniques that work

- **`speaker-test -c2 -twav -l1`** — ground truth for Scarlett + default sink. If you hear "Front Left / Front Right", the hardware + default route is fine.
- **`pw-link -l`** — single source of truth for audio routing. If an expected edge isn't there, the app is disconnected regardless of anything else.
- **`sudo journalctl -u demiurge -f`** — the launcher logs every compile / launch / link / scrub / reconcile decision it makes straight to journald. When the patch graph misbehaves, this is where it tells you why.
- **`/proc/<pid>/maps | grep libjack`** — the only reliable way to verify which libjack a running process actually loaded.
- **`pw-top`** — xrun counter + real-time CPU load per node. When chasing audio glitches at 64/48k, watch this continuously. Use `pw-top -b -n 30` for a scriptable 30-sample batch and **watch the `ERR` column climb** — it is PipeWire's own per-node accounting, needs no cooperation from the client, and costs nothing to sample while the user is playing. It is the right first instrument when a stage's own xrun counter says zero (see Q7h: that counter may simply never be printing). `BUSY` vs `QUANT` gives the DSP time against the period budget; a **bimodal** `BUSY` — a low baseline with periodic excursions — means something is intermittently blocking the callback rather than the DSP being too heavy.

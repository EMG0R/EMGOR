# Cardinal always-on beside the NEPTR engine: plan

Written 2026-10-23 by -C- PRIME NEPTR (prep job). **Nothing here is installed, launched or activated.**
Every statement marked UNVERIFIED has not been observed on this box. Files are in this directory.

## 1. Design in one paragraph

`cardinal.service` (system unit, User=gorpi7v5) runs `cardinal-ctl.py`, which supervises one child
(`xvfb-run -> /opt/demiurge/cardinal/bin/Cardinal`), routes body controls to Cardinal over a private MIDI
port, and cycles patches. Cardinal is a pipewire-jack client with `node.async=true`, pinned to core 1, kernel-capped at
FIFO 60 (target 50), CPU-quota'd, memory-capped, and watched by `cardinal-guard.py`. The dependency is one-way:
cardinal follows demiurge (`After=`, `PartOf=`); demiurge has no reference to cardinal.

| file | role |
|---|---|
| `cardinal.service` | unit: core 1, Nice 5, LimitRTPRIO=60, CPUQuota 85%, MemoryMax 2G, Restart=always + RestartSteps backoff, ExecStartPre gate |
| `cardinal-wait-engine.sh` | gate: wait until `csound6:output1` is linked to `playback_AUX0` (lists the graph only) |
| `cardinal-ctl.py` | supervisor, radio-slot switcher (3 backends), body router (`--selftest` passes; pure logic, no ALSA) |
| `slots.json` | slot -> .vcv, button CCs, sensor calibration |
| `proposed/serial_bridge_tap_gate.patch` | bridge edit so tap tempo needs the top-left encoder held (NOT applied) |
| `cardinal-link.sh` | idempotent MIDI link (+ opt-in audio link); never deletes, never touches csound |
| `graph.conf.cardinal.proposed` | the two audio links, in the launcher's own `connect =` format |
| `cardinal-guard.py` / `.service` | csound ERR watchdog; acts on Cardinal only |
| `pw/jack.conf` | copy of the pipewire-jack config with `rt.prio = 50`, used only by the Cardinal child |
| `sensors.cardinal.proposed.json` | inactive demiurge-io maps (alternative to ctl's analog mirror) |

## 2. Why Cardinal cannot hurt the guitar path

1. **Async node.** `PIPEWIRE_PROPS` carries `node.async=true`. An async node does not hold up the graph deadline; its output is
   one quantum late. Mechanism: pipewire-jack (libjack.so) lists `node.async` among the client properties it reads
   (same table as `jack.writable-input`, `jack.flag-midi2`), and PIPEWIRE_PROPS is the documented way to hand client props
   (`man pipewire-jack.conf`: `PIPEWIRE_PROPS='{ jack.short-name=true }'`). Equivalent per-app route:
   a `jack.rules` entry in `~/.config/pipewire/jack.conf.d/` matching `application.process.binary = "Cardinal"` with
   `update-props = { node.async = true }`; not used because a jack.conf.d drop-in would be global to every JACK client
   (csound6 included) and the env route is scoped to the child. **Confidence: 70%** that the flag reaches the node and is honored.
   `man pipewire-props` does not document `node.async` (string exists in libpipewire and libjack only).
   Precedent on this box: `primewd_play/rec` (hat-watchdog pilot streams) show `node.async: true` in pw-dump, csound6 shows none.
2. **Quantum/rate lock.** `node.lock-quantum=true node.lock-rate=true` (both in libjack's table): the client cannot move the graph quantum or rate.
   `PIPEWIRE_LATENCY` is deliberately NOT set for Cardinal. The graph is already pinned by `clock.force-quantum`.
3. **Cores.** `CPUAffinity=1`. Not core 2 (HAT DMA IRQ FIFO 85 + PipeWire data-loop 83), not core 3 (csound FIFO 74, isolcpus).
   `CPUQuota=85%` caps it below saturating core 1, so IRQ threads that also land on core 1 (spi, xhci-usb1) are never starved for long.
4. **Priority.** Three layers, strongest first:
   - `LimitRTPRIO=60`: kernel-enforced hard cap for the whole unit. Any attempt above 60 fails. **Confidence 95%.**
   - `pw/jack.conf` (`PIPEWIRE_CONFIG_DIR` for the child only): `libpipewire-module-rt` `rt.prio = 50`, so the client's
     data-loop thread (the JACK process thread) gets FIFO 50 instead of the module default 83. **Confidence 70%** (the module's
     args are documented in its strings: `rt.prio=<default 83>`, `nice.level`; reading it from a private CONFIG_DIR is the unverified part).
   - `cardinal-guard.py` L0: every 5 s any Cardinal thread above 60 is lowered with `chrt -f -p 50`.
   `node.lock-quantum`/`rt.prio` as *PIPEWIRE_PROPS* values: `rt.prio` is also read by libjack (JACK max-RT-priority API) but is not
   what sets the data-loop priority, so it is not relied on.
   Result expected: Cardinal process thread FIFO 50 on core 1, below csound 74 / data-loop 83 / IRQ 85.
5. **Guard.** csound6 pw-top ERR delta >= 1 in 60 s while Cardinal runs -> Cardinal threads to SCHED_OTHER nice 15 (L1);
   still rising -> PAUSE file, cardinal-ctl SIGTERMs Cardinal and keeps it down (L2). Never SIGSTOP (a stopped sync client
   would stall the graph; allowed only after node.async is proven). Never acts on csound, PipeWire, HAT or any service.
6. **Isolation of control traffic.** Cardinal's MIDI input is a private ALSA-seq virtual port ("Cardinal Macros") carrying only
   ch9 macros and forwarded clock/start/stop. Cardinal never sees body ch1, and ch9 never reaches Midi Through, so the engine
   cannot see it. (Why this matters: the engine's CC dispatch is **channel-agnostic** except ch7/ch16; midi_handler.orc line 506.
   Today it reads no CC 30-39 or 53-60, so ch9 on the shared bus would be harmless *now*, but any later handler on those numbers
   would fire from ch9. The private port removes that class of bug.)
7. **Cannot restart the engine.** No Wants/Requires/BindsTo/drop-in on demiurge.service. Cardinal crash, stop, disable, guard
   pause, or backoff loop touch only cardinal.service. `systemctl restart demiurge` DOES restart cardinal (PartOf), after the gate.

### Remaining ways it could still hurt (measure, do not assume)
- Memory bandwidth (section 7). - Graph churn when the client joins/leaves (patch switch) - UNMEASURED.
- `node.async` not honored -> Cardinal is a sync node and a late Cardinal makes the graph late -> HAT stream restart bug.
  First-launch step 6 checks the property before any load is applied; guard L1/L2 are the net, not the plan.
- hat-watchdog-v2's loopback pilot listens on the HAT outputs; extra program audio on AUX0/1 could confuse its silent/misaligned test. UNVERIFIED.

## 3. Audio and MIDI wiring

- **Audio:** `Cardinal out 1/2 -> alsa_output.platform-soc_107c000000_sound.pro-output-0:playback_AUX0/1` (PipeWire sums with csound6).
  Carry it in `~/demiurge/graph.conf` (`graph.conf.cardinal.proposed`), not in a script. Reason, from
  `DEMIURGE_OS_work/src/demiurge-launcher-rs/src/graph.rs`: `scrub_hw_playback()` deletes every link into a hardware playback port that is not in
  `desired`; `read_graph_overrides()` merges graph.conf into `desired` on every graph pass and calls `pw-link` for each line
  (missing port = silent no-op). So graph.conf links survive scrubs, are re-made after a HAT reset, and cost nothing when Cardinal is down.
  The launcher re-reads the file each pass: no restart to activate. A script-made link would be scrubbed on the next graph event.
  Port names are a guess (`Cardinal:audio-out1/2`, from DPF strings `audio-out1`, `audio-out2`); confirm at first launch.
- **MIDI:** `Midi-Bridge:Cardinal Macros... (capture) -> Cardinal:events-in` by `cardinal-link.sh --once` (discovery by pattern, because
  the bridge's exact port name for a virtual port is unknown). The launcher does not scrub MIDI ports.
  In every patch the MIDI modules (`MIDI_CV`, `MIDICC_CV`, `MIDIMap`) have `data: null`: no device and no channel set.
- **Clock:** demiurge-clock realtime messages are forwarded by cardinal-ctl from Midi Through to the private port (callback thread, no tick quantisation).

## 4. Patch compatibility (vcv-check, fixed this session)

vcv-check could not read these files: macOS tar writes members as `./patch.json` and AppleDouble `._*` entries. Fixed in
`vcv-check.py` and `~/vcv/bin/vcv-check` (matches `./patch.json`); `Core` is now reported separately instead of as "missing plugin".
Cardinal 26.02 manifests: 85 plugins; its `Fundamental` ships 26 modules (not Rack's full set).

| module | A | B | C | D | 05 flsamp | verdict |
|---|---|---|---|---|---|---|
| NullSet/WeightedQuantizer | x2 | x2 | x2 | x2 | x2 | not in Cardinal |
| SynthKit/Addition | x2 | x2 | x2 | x2 | - | not in Cardinal |
| SynthesisTechnology/E350 | - | x2 | - | - | - | not in Cardinal (B's tone voice) |
| Fundamental/Push | 1 | 1 | 1 | 1 | 1 | not in Cardinal: **CC31 manual trigger has no target** |
| Fundamental/SHASR | 2 | 2 | 2 | 2 | 4 | not in Cardinal |
| Fundamental/Rescale | 6 | 6 | 10 | 6 | - | not in Cardinal (voice shaping chain) |
| Fundamental/Process | 1 | 1 | 1 | 1 | - | not in Cardinal |
| Core/AudioInterface2 | 1 | 1 | 1 | 1 | 1 | Cardinal remaps to HostAudio2 (string table in binary); OK |
| Core/MIDI_CV, MIDICC_CV, MIDIMap, Notes | 1 each | | | | | remap to Cardinal Host* UNVERIFIED (no such strings in the binary) |

Everything else (Impromptu Clocked, AudibleInstruments Plaits/Braids/Rings/Branches, Ripples, 8vert, LFO2, ADSR, Scope ...) is present.
All five fail the "every module present" test, so every patch will load partially (Rack skips unknown modules and drops their cables).
**05_flsamp (default) is the least broken**: lacks only WeightedQuantizer x2, Push x1, SHASR x4. It still has no manual trigger.
Replacements are MacBert's call; Fundamental/Pulses is a candidate for Push, Cardinal's HostMIDIGate/HostParameters for macros.
Audio settings, identical in all five: driver 5 (JACK), deviceName "vcv", 48000, blockSize 256, dcFilter on; Cardinal ignores them (HostAudio).
MIDI settings: none (null) in all five; a Cardinal host-MIDI module must be set to channel 9 once per patch (drop-in learn is manual).
Cardinal exposes HostMIDI/HostMIDICC/HostMIDIGate/HostMIDIMap/HostParameters; whether Rack's Core MIDI modules are remapped to them
on load is the first thing to read in the first-launch log.

## 5. Patch slots (radio buttons) and fast switching

**Spec (Emory, supersedes cycling):** A B X Y UP DOWN LEFT RIGHT = slots 1-8, one patch each, press = switch, wrap/cycle gone.
`slots.json`: slot -> .vcv (empty = null, a press does nothing; pressing the active slot does nothing). Defaults: A..UP = patches 01..05, DOWN/LEFT/RIGHT empty.
Buttons are CC61-68 per body_control_map.md (A65 B66 X67 Y68 UP61 DOWN62 LEFT63 RIGHT64), read as press edges (127). NEPTR's engine and UI read none of CC61-68
(engine CC list in section 6; UI only logs 61-64 as dashboard telemetry via `companion_input.py`).

**UP vs RIGHT collision.** The firmware tables are distinct (`BTN_CC` b6=UP=61, b7=RIGHT=64), but the capture recorded both gestures as CC64 on the USB and old-bridge paths, so UP is unmeasured and UP might physically be b7.
Failure mode if UP really emits 64: UP selects slot 8 (RIGHT). The default layout puts the empty slot on RIGHT, so the failure is "UP dead, RIGHT dead", never a wrong patch. Slot 5 (the boot default) is booted by name anyway.
Resolve with `tools/map_controls.py` (read-only capture, no flashing), then edit two `cc` values in `slots.json`. No code change.

**Why the switch cannot be "instant" with one process that restarts:** measured nothing yet; estimate 4-8 s (section 5.1 a). Three architectures for fast:

### 5.1 Options, ranked by risk to the guitar path

| | (c) one merged patch, per-instrument bypass | (b) one Cardinal, fast patch load (OSC /load) | (a) N preloaded Cardinals, idle ones frozen |
|---|---|---|---|
| graph change at a switch | none, ever | none | link/unlink + SIGSTOP/SIGCONT of a PipeWire client |
| switch time (estimate) | 1-20 ms (a gate); with crossfade VCAs ~30-100 ms | 0.3-1.5 s if /load works | 0.3-0.6 s (thaw 250 ms + two pw-link) |
| idle cost | bypassed modules skip process(); shared engine + GUI only | none | 4 extra GUIs+engines resident (frozen: ~0 CPU, 0 bandwidth, 300-600 MB RAM each, a guess) |
| risk to csound | lowest | low | highest of the three (frozen client in a live PipeWire graph; N x startup storms) |
| status | needs a merged .vcv built in Rack on the Mac; missing modules still apply | blob format, GUI toggle persistence UNVERIFIED (confidence 40%) | code written (`backend: multi`), nothing verified |
| confidence it works as described | 60% (Strip/Bypass modules exist in this build; external drive of bypass unproven) | 40% | 55% |

**Recommendation: (c) as the end state, (b) as the stepping stone, (a) only as a measured experiment and never enabled without the private-PipeWire test below.**
Rationale: (b) and (c) keep exactly one PipeWire node alive forever; nothing joins, leaves, freezes or relinks, so the only thing a switch can disturb is Cardinal's own audio.
(a) is the only option that manipulates a live graph member.

**(c) detail.** Rack 2 skips `process()` on bypassed modules (it runs the bypass routes instead), so a bypassed instrument costs approximately its cable/UI overhead, not its DSP.
Bypass is a per-module flag with no external API in stock Rack. This build bundles `Stoermelder-P1/Strip` and `StripBay4` (group bypass/mute from a gate), `Befaco/Bypass` and `Venom/Bypass`, `Stoermelder-P1/MidiCat`. A MIDI gate (ch9 CC64-71 style, set by cardinal-ctl) -> Strip "ON" would toggle a whole instrument.
Construct: one patch = the five instruments (each already self-contained and stereo; they only share clock/utility banks, duplicated in each), each behind a Strip, outputs into one stereo mixer with an equal-power fade VCA per instrument driven by the same CC (gives a 30-100 ms crossfade, no click). Authoring is a Rack-GUI job (MacBert); a script can merge the five `patch.json` files (offset positions, renumber module ids) but the Strip/VCA wiring needs a human.
Memory: all five instruments' state resident in one process; bypassed ones do not touch it per sample.
Unknowns: whether Strip honors a CV gate while bypass-driven from MIDI headless; whether bypassed modules' *clocks* stop (they should; a resumed instrument restarts mid-phrase); worst-case CPU of the always-on shared part.

**(b) detail.** `/load` expects a blob (`osc_load_handler` checks `types[0]=='b'`): the raw `.vcv` bytes (20-27 KB, fits one UDP datagram). OSC remote is a GUI toggle ("Enable OSC remote control", port field default 2228) in the prebuilt, and the string
"OSC Remote control is not available on plugin variants" says it is a standalone-only feature, which this is. Hot load clears the Rack engine under its mutex; with node.async the graph deadline is not tied to it.
`cardinal-ctl` `backend: osc` implements it, falls back to a restart if no reply in 2 s. Ack behavior is a guess.

**(a) detail and what a frozen client does to the graph (from PipeWire's scheduling model; not tested here):**
- A PipeWire node is scheduled by a driver when it is *active* and in that driver's group. Grouping comes from links; an unlinked node is not in any driver's group **unless** `node.always-process` is true. pipewire-jack clients default to always-process = true (the shipped `jack.conf` has a rule setting it false for qsynth "to make qsynth idle", which only makes sense if the default is true). So by default an unlinked JACK client is still driven every cycle by a driver (I believe the default/best driver, i.e. the HAT). Confidence 65%.
- If it is driven and the process is SIGSTOPped, the driver triggers it, it never signals completion, and the cycle ends with the node unfinished: counted as an xrun (ERR on the *driver*). That is exactly the HAT-restart trigger. So freezing a default JACK client is not safe.
- **node.async:** an async node's completion is not required for the cycle (the node reads the previous cycle's data). It should survive a stalled process without the driver counting a miss, but a stalled async node is still a member of the activation chain that gets triggered each cycle; whether the driver treats "still pending from last cycle" as an error or skips triggering is not something I can confirm from docs/man. 50%. Do not rely on it.
- **Fix used in the multi backend:** idle instances start with `node.always-process=false` and are unlinked. An unlinked, non-always-process node is not in any driver group, is never triggered, and a stopped process cannot stall anything. Confidence 65%. pw-top shows it: an idle instance must display state I/S and WAIT/BUSY 0.
- The invariants: freeze only after `pw-link -l` proves no links; thaw 250 ms before linking; never more than one instance linked; unlink old after linking new (no gap), then verify, then freeze.
- **Clean deactivation:** DPF/Cardinal does not expose `jack_deactivate` over any control path. `pw-cli` cannot suspend a client-owned node (the node belongs to the client; `pw-cli destroy <node>` kills the client's node, and a pipewire-jack client then dies or must reconnect). `pw-metadata`/node `Props` has no run-state. So the options are exactly: unlink + always-process=false (inert while running), SIGSTOP (inert and frozen), or destroy (defeats preloading). "Keep instances alive with the node removed from the graph" = unlink + always-process=false; there is no finer knob.
- **Socket backlog:** a frozen client does not read its PipeWire socket; the server queues registry events (every HAT reset, link change, new node). PipeWire has no keepalive timeout I know of, but an overflowing queue can get a client disconnected. Unmeasured; the multi backend notices a dead child and re-queues it.
- **Idle GUI cost is why they must be frozen at all:** Rack redraws continuously (animated lights, scopes) under software GL at 1280x800, a per-instance CPU and memory-bandwidth cost. A frozen instance has none.
- Boot cost: N sequential 5-10 s CPU/bandwidth-heavy startups on core 1 (software GL init); staggered 12 s apart after the engine gate. Risk of csound ERR during the staggered loads: measured by the same phases as section 8.
- **Test without touching the HAT graph** (recommended before enabling `multi`): a *private* PipeWire daemon on a scratch `PIPEWIRE_RUNTIME_DIR` with only the Dummy driver at quantum 128 pinned to core 1, containing (i) a trivial pipewire-jack client with and without node.async, SIGSTOPped while linked and unlinked, (ii) Cardinal itself. Read the dummy driver's ERR counter. This answers "does a frozen/unlinked client stall the driver, and does node.async protect against it" empirically with zero effect on the live graph. It still costs CPU and memory bandwidth on the live rig, so it needs Emory's go and csound watched during it.

### 5.2 Implementation status
- `cardinal-ctl.py backend=restart` (default): works as designed on paper, slow; `osc`, `multi` experimental.
- `slots.json` holds slot assignments, button CCs, shaping calibration. Debounce 150 ms (last press wins), min gap 3 s for restart (0.3 s for multi).
- Multi backend writes only the managed block between `# >>> cardinal-ctl` and `# <<< cardinal-ctl` in `~/demiurge/graph.conf` (atomic rename, other lines untouched) so the launcher keeps the active instance's links and does not re-create the old one's.
  It reads port names from `pw-dump` by `application.process.id`, so the per-instance JACK client name (Cardinal, Cardinal-01 ...) does not matter.

## 6. Control routing: what NEPTR does today, and every double-drive

Sources: `csound/handler/handler_remap.orc`, `handler/dispatch/*.orc`, `handler_looper.orc`, `midi_handler.orc` (channel-agnostic dispatch at line 506; only ch7 excluded, ch16 specifics),
`neptrPhase4_UI.py`, `companion.py`, `companion_input.py`, `serial_bridge.py`. Engine-read CC numbers (any channel unless noted): 20-26, 29, 40-52, 70-76, 102-107, 110, 111, 114, 116, 117, 119-122.

| control | wire | NEPTR today | double-drive with the VCV spec? |
|---|---|---|---|
| top-left encoder TURN | ch1 CC26 (CW127/CCW0) | -> logical CC20: **secondary parameter of the current effect** (`rot_p2_cc20.orc`); MIXER page: channel A | **YES.** Every probability turn also moves NEPTR's secondary param |
| top-left encoder CLICK / HOLD | ch1 CC76 press/release | -> logical CC70: **menu PREVIOUS on the press edge** (`nav.orc`) | **YES.** The tap-tempo hold gesture steps NEPTR back one menu every time. Not the looper (looper gestures are on slot encoder clicks CC71,72,74,75,76) |
| arcade | ch1 CC60 | engine ignores. UI: companion-mode control (hold 1.2 s toggles it; tap inside companion activates the selected row, incl. patch switch = engine restart and power/RESTART). **Bridge sends ch16 CC113 tap tempo on every press** | **YES (big).** As built, every VCV trigger press also taps the clock and retunes the rig. Needs `proposed/serial_bridge_tap_gate.patch` (tap only while top-left click is held); then plain-arcade tap tempo is gone from NEPTR, which is Emory's request but a behavior change. A trigger held >= 1.2 s opens companion mode |
| 8 buttons A B X Y UP DOWN LEFT RIGHT | ch1 CC61-68 | nothing reads them (engine: none; UI: dashboard telemetry for 61-64 only) | no |
| accel X / Y / Z | ch1 CC31 / 32 / 33 (bridge `ax`, `ay`, `az`; `accel_scale` +-17000 -> 0..127, centre 64) | nothing reads them (UI dashboard telemetry) | no |
| TOF1 / TOF2 | ch1 CC30 / CC36 (`tof_scale`: 2-46 cm -> 0..127, out of range = no message) | nothing reads them (UI dashboard telemetry). The engine parks the body pedal on an internal CC30 that no handler reads: internal only, not on the wire | no |
| FSR1/2, joystick X/Y | CC34/35, CC27/28 | not read by the engine (UI companion navigation uses joystick X/Y/button CC27/28/77) | joystick: navigation only while companion mode is open |
| expression | ch1 CC29 (and DUODECIMUS ch7 CC14/15) | -> OSC `/cc 1 29` through demiurge-io | never touched by this work |
| ch9 | - | nothing reads ch9 specifically, but dispatch is channel-agnostic. The private virtual port keeps ch9 off the engine's bus entirely | no, by construction |

**Double-drive resolution options (needs Emory):**
1. Top-left TURN also moves NEPTR's secondary param. Accept, or engine-side suppression (a flag in `handler_remap.orc`), or pick a non-conflicting encoder. A hold-modifier would not help: Emory wants the turn itself.
2. Top-left HOLD steps the NEPTR menu back on each press. Accept, or make menu PREV fire on release-if-short (engine edit), or use a different hold control (X is unused).
3. Arcade: apply the bridge patch (`proposed/serial_bridge_tap_gate.patch`, restart `neptr-bridge.service`; body controls blink for ~1 s, audio untouched). Existing test `tests/test_serial_bridge.py` replays captures; check it still passes (the tap assertion, if any, needs the encoder-held fixture).
Until these are decided the daemon is safe to run for slots + sensors (no double-drive there) but its encoder/arcade outputs double-drive NEPTR exactly as listed.

**Hold vs turn:** the daemon tracks the top-left click state (CC76). While held, a turn sends nothing to VCV and the arcade is suppressed (the bridge, not ctl, emits the tap). Released: turn = probability, arcade = CC31 trigger. An arcade pressed before the encoder still gets its release sent. NEPTR itself sees the raw turn regardless (item 1).

**Sensor macros (ch9, all through `Shaper`: calibrated range, one-pole smoothing, centre snap, deadband, 50 Hz limit):**
| macro | source | notes |
|---|---|---|
| CC32 Tone | accel X = ch1 CC31 (`ax`) | range 24..104 (about +-40 of centre 64 = +-0.6 g, roughly +-37 deg tilt), tau 60 ms, deadband 1, centre snap 3% |
| CC33 Filter | accel Y = ch1 CC32 (`ay`) | same |
| CC34 Secondary tone | TOF1 = ch1 CC30 | 4..100 (about 3-36 cm), tau 100 ms, deadband 2; near = low. Out-of-range frames are not sent, so the value holds (no snap on lift-off) |
| CC35 Send/amount | TOF2 = ch1 CC36 | same |
| CC30 Probability | top-left turn | step 4, persisted, accumulated |
| CC31 Manual trigger | arcade | momentary 127/0 |
| CC36/37, 38/39, 53 | FSR0/1, joystick X/Y, accel Z | spare, for MIDI-learn |
The X/Y assignment (X = CC31, Y = CC32) comes from `ANALOG_MAP` in `serial_bridge.py` (`"ax": 31, "ay": 32, "az": 33`). Which physical tilt axis each is depends on how the IMU is mounted: **unverified, the first bench check is to tilt and watch** (swap `in_cc` values in `slots.json` if reversed).
This overrides MacBert's "joystick X -> filter"; joystick X/Y move to spare CC38/39.
Calibration numbers are defaults from the scaler maths, not measurements; tune on the bench (they are in `slots.json` `shaping`).

## 7. demiurge-io mapping (task 3) and what activation needs

- The pool lives in `~/demiurge/sensors.json` (hot reload on mtime), code in `DEMIURGE_OS_work/src/demiurge-io` (deployed copy `/opt/demiurge/src/demiurge-io`, identical).
  Presence-conditional maps are `profiles[].when`. I did not edit either; the proposed maps are in `sensors.cardinal.proposed.json` (all `enabled:false`).
- The pool **cannot** do the encoder, slot switching or the tap: no relative-to-absolute accumulation (daemon.py line 81 excludes encoders on purpose), and its midi sink blocks ch16 CC104-119.
  So cardinal-ctl owns those. The pool JSON is only an alternative for the continuous mirrors; choose exactly one (`CARDINAL_MIRROR_ANALOG=0` if the pool is used).
- Activation of the pool route: merge `maps` into sensors.json (hot reload). Whether a newly mapped source CC joins the `--midi-mapped-only` subscription without a restart is UNVERIFIED;
  if not, it needs a demiurge-io restart (expression pedal dead ~2 s): needs Emory's go. Pool-route ch9 goes onto Midi Through, which then needs Cardinal's MIDI modules set to ch9 and a link from the Midi-Bridge capture port. cardinal-ctl's route needs neither.

## 8. First launch (needs Emory's go; commands only, none run)

```
# 0. baseline, 5 min with the rig playing as normal, Cardinal absent
taskset -c 0 nice -n 19 bash -c 'for i in 1 2 3 4 5; do pw-top -b -n 2 | grep csound6 | tail -1; sleep 60; done'
wc -l ~/PRIME_RECOVERY/logs/spikes.log                      # spike logger is running (UDP 9177)
cat /proc/pressure/memory; grep -E 'MemAvailable|SwapFree' /proc/meminfo
# 1. install (no enable)
sudo cp ~/PRIME_RECOVERY/vcv/cardinal.service /etc/systemd/system/ && sudo systemctl daemon-reload
# 2. start with NO audio link yet (graph.conf untouched)
sudo systemctl start cardinal && journalctl -u cardinal -f
# 3. what it really is
ps -eLo pid,tid,cls,rtprio,psr,ni,pcpu,comm | grep -iE 'cardinal|xvfb'      # expect psr=1, rtprio <= 60 (target 50), never 2 or 3
# 4. real port names and the autosave path
pw-link -o | grep -iE 'cardinal'; pw-link -i | grep -i cardinal
find ~/.local/state/cardinal-home -newer ~/PRIME_RECOVERY/vcv/cardinal.service -type f | head
# 5. patch really loaded? (journal: "Loading autosave ...", "Could not find module ...")
# 6. async check
pw-dump | ~/kivy-venv/bin/python -I -c 'import json,sys;[print(o["id"],o["info"]["props"].get("node.name"),o["info"]["props"].get("node.async")) for o in json.load(sys.stdin) if o["type"].endswith("Node") and "ardinal" in str(o["info"]["props"].get("node.name",""))]'
#    expect: true. If None/false: STOP here (sudo systemctl stop cardinal). Do not link audio.
# 7. fix names in graph.conf.cardinal.proposed, then append the two lines to ~/demiurge/graph.conf (launcher re-reads it; no restart)
# 8. 5 min with Cardinal audible on 05_flsamp, repeat the step-0 loop; compare csound6 ERR (delta must be 0), spikes.log line count, Cardinal CPU:
top -H -b -n1 -p "$(pgrep -d, -f /opt/demiurge/cardinal/bin/Cardinal)" | head -20
# 9. one slot switch (press B) with the step-0 loop running: csound6 ERR delta across the switch must be 0; time it from the ctl log line 'took N ms'
# 10. heaviest patch (03_instrument-C) 5 min, same comparison; then `sudo systemctl start cardinal-guard` after parsing `pw-top -b -n 2` once by hand
```
Pass = csound6 ERR delta 0 and no new spike lines in every phase, Cardinal on core 1 only, FIFO <= 60, `node.async` true.

## 9. Memory-bandwidth risk

isolcpus and CPU pinning do not partition the memory controller or LPDDR. Prior finding (DEMIURGE/agent-memory-bandwidth-vs-live-audio): memory-heavy agent work
on other cores caused 2,543 underruns while csound sat on an isolated core. Cardinal is a worse offender than an agent in principle: software GL under Xvfb
(Mesa llvmpipe renders a 1280x800 frame, plus 85 plugins' resident data, plus NAM/IR-style buffers in some modules), and its DSP walks module state every sample.
CPUQuota bounds core-1 *time*, not bytes moved.
How the first launch measures it, in order of isolation:
1. Phase A baseline (step 0) vs phase B "Cardinal started, no patch audio linked, idle GUI" vs phase C (05_flsamp, linked) vs phase D (03_instrument-C, heaviest). Same csound ERR + spikes.log
   (the engine's own per-k-cycle probe) counters per phase; the signal is any increase in spike rate or max k-cycle time that appears with Cardinal and disappears when it stops, not csound CPU.
2. `/proc/pressure/memory` and CPU-core 3 `pw-top` BUSY for csound6 across phases: BUSY rising on csound6 at constant load with Cardinal up is the contention signature (the same instructions taking longer).
3. A/B with the GUI cost removed: run phase C with `LIBGL_ALWAYS_SOFTWARE=1` and a 640x400 Xvfb screen (set in `-s`), compare. GL rasterisation is the likely largest byte-mover.
4. If any phase shows spikes: the mitigations in order are smaller Xvfb, `CPUQuota` down to 50%, MemoryHigh down, fewer modules in that patch; the guard (L1/L2) acts automatically only on ERR, not on spikes below ERR, so step 1 is a manual judgement.
No perf/sysbench on this box (`perf` absent), so there is no direct DRAM bandwidth counter; the measurement is the effect on csound, which is the only thing that matters.

## 10. Rollback

```
sudo systemctl disable --now cardinal cardinal-guard 2>/dev/null; sudo rm -f /etc/systemd/system/cardinal.service /etc/systemd/system/cardinal-guard.service && sudo systemctl daemon-reload
```
Stopping alone is `sudo systemctl stop cardinal` (never touches demiurge). Also delete the two `Cardinal:` lines from `~/demiurge/graph.conf` (harmless if left; missing ports are skipped).
Nothing else is modified: `serial_bridge.py`, `sensors.json`, `live.conf`, the wrapper and the launcher are untouched.

## 11. Open unknowns

1. node.async honored for a pipewire-jack client (70%); rt.prio=50 via private PIPEWIRE_CONFIG_DIR (70%); LimitRTPRIO cap (95%).
2. Cardinal's JACK client/port names (`Cardinal:audio-out1/2`, `events-in`) and whether it exposes 2 or 8 outputs.
3. Autosave path and that it is restored on start; xvfb-run + Mesa llvmpipe CPU cost; startup time (switch gap).
4. Whether Core MIDI modules load in Cardinal; patches lose Push/Rescale/SHASR/Process/WeightedQuantizer/Addition/E350.
5. Graph churn from client join/leave on csound's deadline (switch measurement).
6. UP vs RIGHT physical wiring; pw-top column layout for the guard parse; hat-watchdog pilot interaction with extra AUX0/1 audio.
7. Double-drives in section 6 need Emory's decision (top-left turn and hold also act on NEPTR; the arcade tap gate needs the bridge patch).
9. `multi` backend: whether an unlinked always-process=false client is truly unscheduled, whether node.async protects a stopped client, whether a frozen client's socket backlog gets it disconnected, idle-GUI cost vs RAM per instance. Test on a private PipeWire first (section 5.1).
10. Linking the Midi-Bridge capture port to Cardinal pulls the ALSA-seq bridge node into the HAT driver's group (passive link may avoid it); its RT-thread cost is unmeasured.
8. Cardinal's persisted OSC-remote toggle (osc switch mode); whether `PartOf=` plus `After=demiurge.service` is enough if demiurge is `Type=simple` and "active" before csound is up (the gate covers it).

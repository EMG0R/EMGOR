# PRIME NEPTR (gorpi7v5) — integrated to-do

Updated 2026-10-22 07:45. One list for everything EMGOR asked for. `[x]` done+verified, `[~]` built but not verified on the rig, `[ ]` open. **GO** = needs EMGOR's word in his own window.

## 1. Demiurge OS install
- [x] Installer running on gorpi7v5, phases 1-12 rc=0 (demiurge 6e4746f, ce4d956, f6ad3af on `gorpi7v5-install`)
- [x] Phase 8 boot config: base block, NO overclock, `sd_cqe=off`, no hifiberry overlay line
- [x] netguard disabled; `demiurge.service` disabled until first start
- [x] RNBO runner 1.4.5 built from source with compile support; installed, DISABLED. Run: `rnbo = on` in live.conf, then `sudo systemctl enable --now rnbo-runner`. Unit has a quoting bug (PIPEWIRE_PROPS line 24). Notes: rnbo-install.md
- [x] Web UI :8080: ported from private DEMIURGE_OS (345ad39, private, NOT for public), active, curl 200. Phase 9 bug: installs only 4 web files (misses `neptr_model.py`, `static/`); fix setup-demiurge.sh
- [x] Name variable: one place: `~/demiurge/pi-rig.conf` (ba8b4dc) for the Mac-side scripts; on the Pi, the installer derives the user (`TEMPLATE_USER` placeholder in unit files) and scripts use `$HOME`. Left: `gorpi7` only in docs and a test fixture
- [ ] Send the installer patches to -A- gorcave (only A publishes the public repo)

## 2. NEPTR engine + UI
- [x] NAM plugin built (`csound-nam.so`, NeuralAmpModelerCore pinned e49c93e)
- [x] `neptrPhase4.csd` parses, 0 errors, in both the 8ch and the MIXER build (`demiurge_params.udo` installed to `/opt/demiurge/resources`)
- [x] live.conf → NEPTR chain, `sync_layer = off`, hifiberry card; UI, bridge and routing map deployed
- [x] Engine + UI started 08:41 on EMGOR's direct permission, enabled at boot (demiurge, neptr-ui, neptr-bridge). 8ch + MIXER build live; csound ~55% CPU; 3 dropouts at startup only, 0 after. Not yet played.
- [ ] Stale `~/NEPTR_phase4/live.conf` (Scarlett era): mark it as not used on PRIME, or remove it via `_attic/`
- [ ] Layer the mpwork changes onto NEPTR_phase4 (multipath)
- [ ] ng1 AlsaSink patch
- [ ] twink parts

## 3. 8in / 8out HAT
- [x] Loopback audit run twice: ch1,2,4,5,6,7 map 1:1 with 47-57 dB isolation
- [x] I2S ruled out by measurement. The 4 data lanes each carry a pair (SDO1 = out3+4, SDI1 = in3+4, SDO3 = out7+8, SDI3 = in7+8). out4→in4 and out7→in7 pass, so lanes 1 and 3 work both ways. All 8 ADC slots return live noise, not digital zero. Pins 18-27 are all on I2S0.
- [x] Per-channel solo test (`solo.py`), one known-good cable moved pair to pair: ch1-7 pass at unity (-13 dB in for -12 dB out). Physical jack order on the box = software 3/4, 5/6, 7/8, 1/2. ch3 earlier = bad cable + loose plug (fixed).
- [ ] **out8 dead**: silent everywhere on a cable proven good on 3/4 and 5/6. in8 alive (it picks up out7's normal leak at -69 dB). Suspect the out 7/8 jack's ring contact or the HAT's channel 8. Check the jack with the power off. Interim choice: PMOR mono on out7, or PMOR moves to 3/4.
- [x] Gain staging: unity loop gain in the per-channel test. The -37 dB came from how ioaudit measures, not from the HAT.

## 4. Looper mid-record cutoff
- [~] Fix ee71917: tail-protect on close quantise; parses inside the full engine. Audit doc: `NEPTR_phase4/docs/incidents/2026-10-22_looper_midrecord_cutoff.md`
- [ ] Bench test once the engine runs: record long takes, tap late, confirm no cut
- [ ] Decide: ignore presses in the first 0.25 s of recording (the spurious-press cause)? Recommended yes

## 5. Clock
- [~] `PRIME_RECOVERY/clock/deploy-clock.sh` + README (0be28a8). Sets `sync.conf role = opgorator, source = manual`; PMOR is master
- [ ] Decide: with `sync_layer = off` the clock daemon likely never starts, so PMOR's BPM (CC119) won't reach `chase_bliss_clock`. Turn on the sync layer, or start the daemon some other way?
- [ ] Run the deploy, verify with `aconnect -l` and `aseqdump` while PMOR plays

## 6. Vocal chain (ch5,6)
- [~] Built to spec 8.1 (218991f): HPF 80 → -4 dB @1k Q1.2 → 500 ms delay fb .30 wet .20 → reverbsc fb .85 cut 10k wet .25; no AGC or added gain
- [ ] Decide: the vocal comes out on outs 5,6 only. Also send it to main 1-2?
- [ ] Controls: add a UI menu, OSC and state_persist slots for the `voc_*` channels
- [ ] Bench test once the engine runs

## 7. Board stability / network
- [x] Off the "Home" WiFi (auto-connect off); on EMGOR at -41 dBm
- [x] SD command queueing off; 0 SD stalls since
- [ ] **Hard resets: power-on resets, not crashes.** 5th reset at ~07:44. Evidence:
  - firmware reset register `rsts=0x1000` (power-on reset); `power_reset=0`; no watchdog record
  - load 0.4, 5.00-5.09 V, throttled=0 in the 2-second samples right up to the cut
  - USB-PD negotiated 5 V / 5 A
  Means the board lost power abruptly. Check (EMGOR at the bench): reseat or swap the USB-C cable, try another PSU, check whether anything plugged in can backfeed or short 5 V (the HAT, USB devices, the Teensy body). SD and network are ruled out.
- [ ] Ethernet to the router when possible; then remove the wifi-wedge watchdog drop-in

## 8. Housekeeping
- [x] All work pushed to gorcave: NEPTR_SYNC `sync/gorpi7v5`; recovery + demiurge bundles verified by sha256 and a fresh clone
- [ ] Update `mesh/nodes/gorpi7v5/device.md` and the NEPTR/DEMIURGE project memory with the final state

## 9. Touchscreen (fixed 2026-10-22 08:47)
- [x] The ADS7846 touch panel was missing because the installer skipped the SPI touch setup (`HAVE_NEPTR=0`). Fixed:
  - `dtparam=spi=on` in config.txt, also applied at runtime
  - `demiurge-touch-poll.service` from the rig backup, user changed to gorpi7v5
  - python3-evdev / spidev
  - uinput udev rule (`input` group)
  - touch-cal.conf from the old rig
  The UI reads `/dev/input/event5`. Copies in `touch/`.
- [ ] Installer fix: run the touch/SPI/uinput block whenever the panel is present, not only on HAVE_NEPTR. Send to -A-.
- [x] MIDI: `demiurge-midi-connect` daemon + unit restored from the rig backup (user gorpi7v5), enabled. Keeps the Teensy body (and later DUODECIMUS/PMOR) wired into Midi Through. At boot the Teensy→14:0 link had been missing.
- [x] Final audit 09:01: reboot → all 6 autostart (demiurge, neptr-ui, neptr-bridge, touch-poll, web, midi-connect), 0 failed units, 3 dropouts at engine start only, touch + cal loaded, web 200. Teensy→14:0 is intentionally NOT linked (the body is read over serial by neptr-bridge; the daemon excludes "Teensy MIDI").

## 10. Bela power (2026-10-22 09:50)
- [x] Root cause of the Bela hangs: the USB ground-loop isolator can't power it (7 s boot loop, -71). It works with the isolator bypassed. Memory: HYPERGUITAR/bela-usb-power-isolator.md
- [ ] EMGOR: find a replacement isolator with an aux power input (Bela on its own supply), or keep the Bela un-isolated. Collect links only, no buying.

## 11. Power — undervoltage proven (2026-10-22 09:54)
- [ ] **The firmware recorded undervoltage**: `throttled=0x50000` (under-voltage + throttling occurred), first set 09:54:05. Lowest 2-second sample 4.75 V, last before the 09:58:10 power-on reset 4.85 V. Load: engine + demiurge-io + PMOR + Teensy (+ Bela at times). This is the cause of the resets: the PSU/cable can't hold 5 V under rig load. Fix: thick short USB-C cable, a known-good 5 A PSU, and peripherals (Bela, PMOR) on their own power or a powered hub.
- Also seen 09:56:53: PMOR re-enumerated as "Daisy Bootloader" then "Pocket OpGorator" (brownout reboot of PMOR, not a flash).

## 12. Sync layer ON (2026-10-22 10:10, EMGOR: "sync layer needs to be on")
- [x] `live.conf sync_layer = on`: NEPTR now runs through the Demiurge graph (csound6 = pipewire-jack client, quantum 64 @ 48k, clock daemon running, PMOR auto-set as clock master, role=opgorator).
- [x] HAT opened 8ch in PipeWire: `~/.config/wireplumber/wireplumber.conf.d/62-demiurge-dac8x-8ch.conf` (AUX0-7). Overrides the installer's 50-demiurge.conf, which forced every platform card to 2ch.
- [x] Channels 3-8 linked via `demiurge-graph connect`, persisted in `~/demiurge/graph.conf`. The launcher wires 1-2 itself. 16 links survive a reconcile; 0 dropouts after startup; PipeWire ERR 3 at startup only.
- [ ] Installer/launcher fix: N-channel cards should get N channels in the WP rule, and the launcher should wire all engine I/O (`nchnls`) to AUX0..N-1 itself, so graph.conf isn't needed. Send to -A- / private DEMIURGE_OS.
- [ ] 8ch loopback re-verify through the graph (solo per channel). Needs a PipeWire-side test tool, since the card is now PipeWire's.

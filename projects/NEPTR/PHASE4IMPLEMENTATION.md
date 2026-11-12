# NEPTR Phase 4 — Implementation Reference

Last updated: 2026-05-28

---

## 1. Architecture

NEPTR Phase 4 runs as part of the DEMIURGE audio system on the Pi 5.
**The Mac is the source of truth.** All edits happen here; `scripts/sync_to_pi.sh` pushes to the Pi.

### Boot path

```
/boot/firmware/demiurge.conf
  └─ launch = /home/gorpi7/NEPTR_phase4/live.conf

demiurge.service (system, auto-start)
  └─ reads live.conf via /opt/demiurge/bin/demiurge-launcher
       ├─ pins clock.force-rate=48000, clock.force-quantum=128
       ├─ runs demiurge-aggregate: links demiurge-sink/source ↔ Scarlett
       ├─ spawns demiurge-clock (BPM=124, Link=off)
       ├─ spawns chuck via demiurge-run-chuck  → -r 48000 --bufsize:128
       └─ spawns csound via demiurge-run-csound → -r 48000 -b 128 -B 1024 -M 14:0
```

### Audio graph

```
ChucK:outport 0/1  ──────────────────────┐
                                          ▼
alsa_input.usb-Scarlett..Mic1:capture  → csound6:input1 → demiurge-sink → Scarlett out
alsa_input.usb-Scarlett..Mic2:capture  → csound6:input2
```

Chuck feeds csound's inputs alongside the mic captures. csound outputs to the DEMIURGE virtual hub — never directly to hardware. Hot-unplug and rate pinning are handled transparently by the hub.

---

## 2. `live.conf` — the only config file you touch

```
link = off
bpm  = 124
rate    = 48000
quantum = 128

chain =
  ~/NEPTR_phase4/chuck/synthi.ck
  ~/NEPTR_phase4/csound/neptrPhase4.csd

inputs =
  ~/NEPTR_phase4/csound/neptrPhase4.csd
```

Edit on Mac, save, `sync_to_pi.sh` — the launcher hot-reloads on file change. **Do not add a second config format.** `phase4.conf` is a leftover from the old architecture and no longer drives anything.

**Critical:** `live.conf` must exist in `_______PHASE4/` so rsync syncs it. If it is missing from the Mac, `rsync --delete` will delete it from the Pi and `demiurge.service` will exit in 27ms silently.

---

## 3. `chuck/synthi.ck` — LOCKIN voice

Standalone ChucK voice derived from PHASE3's LOCKIN mode. Key behaviour:

- **Boot state:** `clockOn = 0` — sequencer is OFF. Piano is silent until activated.
- **Sequencer rate:** 16th notes = `15.0 / BPM` seconds per tick at 124 BPM.
- **Arcade button (Teensy CC 68, ch 1):**
  - Press (val 127) → fires one note immediately via `pickAndPlayNote()`.
  - Hold ≥ 5 s → toggles `clockOn` (sequencer enable) when threshold is reached.
  - Release (val 0) → clears `arcadeHeld` only; no extra note.
- **Master clock sync:** `clockSync()` listens on Midi Through ch16 CC119 from `demiurge-clock`. Rewrites `BPM` in place (0–127 → 40–200).

Signal path per note: 4× `SawOsc` → stereo `LPF` → `ADSR` → `Gain(0.6)` → dac.

---

## 4. MIDI system

### Bus

All MIDI flows through ALSA seq **Midi Through (port 14:0)**:
- Teensy auto-connects to it on plug-in.
- csound subscribes via `-M 14:0` (in `demiurge-run-csound`).
- chuck opens it directly via `MidiIn.open("Midi Through")`.
- `claude_midi.sh` writes to it via `aseqsend`.

### Menu system (csound/midi_handler.orc)

6 menus selected by CC73 (next) / CC70 (prev). **Effect CCs map to different effects per menu.**

```
MENU 0 VOLUME:  s0=Vol(74) s1=RM(75) s2=Leslie(76) s3=Attk(71) s4=Drop(72)
MENU 1 PITCH:   s0=PS(74)  s1=GPS(75) s2=Formant(76)
MENU 2 MOD:     s0=Vib(74) s1=Chorus(75) s2=Phaser(76) s4=Flanger(72)
MENU 3 DRIVE:   s0=Dist(74) s1=TS(75) s2=AutoMoog(76) s4=WAH(72)
MENU 4 TIME:    s0=Delay(74) s1=SpecDly(75) s2=Reverb(76) s3=Granular(71) s4=MOOD(72)
MENU 5 GLITCH:  s0=BBcut(74) s1=WGuide(75) s2=SqMod(76) s3=FreqShift(71) s4=DropOut(72)
```

Toggle CCs (val 127 = toggle on/off, **val 0 = GLITCH_PRESS release**):
`CC74`=s0, `CC75`=s1, `CC76`=s2, `CC71`=s3, `CC72`=s4

Encoder rotation CCs (val>64=CW increment, val<64=CCW decrement, val=64=no change):
`CC24`=s0 rot, `CC25`=s1 rot, `CC26`=s2 rot, `CC21`=s3 rot, `CC22`=s4 rot

### Secondary / tertiary params (CC20 / CC23)

Dispatched by `gkLastToggle` — set to the toggle ID of the **last toggled effect**.

| lastToggle | CC20 | CC23 |
|---|---|---|
| 16 (SpecDly) | maxdel (ENC_ROT) | FB (ENC_ROT) |
| 15 (Delay) | time | fback |
| 6 (Vibrato) | rate | fback |
| … | … | … |

### Expression pedal / probability (CC29)

Absolute 0–127. Dispatched by `gkLastToggle29` — set when toggling a CC29-using effect. Glitch group (lastToggle29 ∈ {20,23,24,25}) → sets `gkGLITCH_chance`.

**FreqShift effective probability = `gkGLITCH_chance × 0.2`**
So CC29=64 → gkGLITCH_chance=0.5 → FreqShift fires ~10% of ticks.

### GLITCH_PRESS effects (BBcut, WGuide, SqMod, FreqShift)

These use press-hold-release semantics, not simple toggles:
- **Press (val 127):** starts hold; `_held` flag = 1 → forced 100% chance while held.
- **Release (val 0):** classifies as click (<0.5 s) or hold (≥0.5 s) and sets bypass accordingly.
- **Always send val 0 to release.** If you only send val 127 and never val 0, `_held` stays 1 → effect fires at 100% regardless of gkGLITCH_chance.

### Claude MIDI injection

```sh
ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh cc <num> <val> [ch]'
ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh ports'
ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh map'
```

**aseqsend quirk:** the hex-string form (`-s "B0 4C 7F"`) silently exits 0 and sends nothing. The script writes bytes to a temp file and passes the path.

---

## 5. Encoder math

`ENC_ROT` macro: `val > 64` → increment by step, `val < 64` → decrement by step, clipped to [min, max].

Key init values and steps:

| Global | Init | Step | Min | Max |
|---|---|---|---|---|
| gkSpecDelMix | 0.5 | 0.01 | 0 | 1 |
| gkSpecDelFB | 0.8 | 0.01 | 0 | 1 |
| gkSpecDelMaxDel | 0.3 | 0.002 | 0 | 1 |
| gkGranMix | 0.5 | 0.01 | 0 | 1 |
| gkGLITCH_chance | 0.25 | 0.01 | 0 | 1 |
| gkverbMix | 0.5 | 0.01 | 0 | 1 |

To max a mix from init (e.g. 0.5→1.0): send CC val=65 × 60 times.
To min maxdel (0.3→0): send CC val=63 × 200 times.
To set FB 0.8→0.99: send CC val=65 × 19 times.

---

## 6. Presets

Stored under `presets/`. Each preset is a markdown file with the full activation sequence (CCs, step counts, timing). Apply via `claude_midi.sh`.

Current presets:
- `presets/chiller_ambient.md` — reverb + granular (100% mix) + FreqShift (~10%) + SDL (100% mix, shortest time, FB=0.99)

---

## 7. Quantum / rate lockstep

The graph clock must match everywhere or csound outputs -inf dB silently:

| Where | Value |
|---|---|
| live.conf `quantum` | 128 |
| `demiurge.service` ExecStartPre `clock.force-quantum` | 128 |
| csound `-b` flag (via demiurge-run-csound) | 128 |
| csound/globals.orc `ksmps` | 128 |
| `clock.max-quantum` in PipeWire config | 128 (caps at this — do not set quantum above this) |

`clock.max-quantum = 128` is set in the system PipeWire config. Do not change `quantum` in live.conf above 128 without also raising `clock.max-quantum` — the force-quantum setting will be ignored and csound's `-b` will mismatch.

---

## 8. Sync and deploy

```sh
# Mac → Pi
./scripts/sync_to_pi.sh

# Restart audio engine on Pi
ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'

# Check status
ssh gorpi7@demiurge.local 'journalctl -u demiurge --no-pager -n 40'

# Apply a preset
ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh map'
```

---

## 9. Removal

```sh
ssh gorpi7@demiurge.local 'rm -rf ~/NEPTR_phase4'
# Also revert /boot/firmware/demiurge.conf `launch =` to point elsewhere
```

---

## 10. Files

| File | Role |
|---|---|
| `live.conf` | Boot config for demiurge.service — the only user-facing surface |
| `chuck/synthi.ck` | LOCKIN saw voice, 16th-note sequencer, arcade button, BPM sync |
| `csound/neptrPhase4.csd` | Main csound entry, #includes all effects |
| `csound/globals.orc` | All global vars, step sizes, macros (ENC_ROT, FX_TOGGLE, GLITCH_PRESS) |
| `csound/midi_handler.orc` | 6-menu MIDI system, all CC mappings |
| `csound/signal_chain.orc` | Effect routing order |
| `csound/effects/*.orc` | Individual effect instruments |
| `scripts/sync_to_pi.sh` | Mac → Pi rsync push (source of truth enforcement) |
| `scripts/claude_midi.sh` | SSH MIDI injector for remote control |
| `scripts/launch_phase4.sh` | Manual launcher (fallback — normally demiurge.service handles this) |
| `scripts/neptr-phase4.service` | Systemd user unit (alternative to system demiurge.service) |
| `scripts/neptr-ui.service` | Systemd unit for Kivy UI |
| `neptrPhase4_UI.py` | Kivy portrait UI — OSC mirror of csound state |
| `scripts/neptr_visualizer.py` | Mac pygame UI — live top-down view of Teensy sensors over USB serial (debug/bring-up tool) |
| `presets/` | Named sound presets with full MIDI activation sequences |
| `docs/ui.md` | UI OSC protocol reference |
| `phase4.conf` | Legacy launcher config — superseded by live.conf, kept for reference |
| `TEENSYY/TEENSYY.ino` | Teensy firmware. Source lives on the Mac (compiled with the Arduino IDE Teensy core). To flash from the Pi: build a `.hex`, `scp` it over, run `demiurge-flash-teensy <hex>`. See `demiurge/docs/microcontroller-flashing.md`. **Pin map / hardware reference: `plan.md`.** |

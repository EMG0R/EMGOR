# hyperGuitar — noise investigation handoff

**Date:** 2026-09-23
**Status:** RESOLVED — see §0
**Purpose:** factual record of every experiment run, its result, and the
owner's stated constraints. Observations only. No recommendations.

---

## 0. Resolution

**The NeoTrellis was sharing a ground pin with the audio side. Moving it to a
different ground pin on the board fixed it.**

Found by the owner. Everything in §3–§4 below was an attempt to filter, damp or
reduce a noise whose path was a shared ground return — which is why brightness,
supply voltage and LED on/off all changed the symptom (they changed the current
through that shared pin) while capacitors, series resistors and I2C changes did
nothing (they never touched the return path).

The BC517 piezo buffer was **not** at fault. It was faithfully reproducing the
ground it was referenced to. `gVolPiezo` is back to 2.50.

The cheap test that would have found this immediately — move the ground wire to
a different pin — was never run.

---

## 1. System context

- Bela board, 10 audio in / 10 audio out, 44100 Hz, block 64.
- I²C bus `/dev/i2c-1`: NeoTrellis 0x2E, MPU6050 0x68, VL53L1X 0x29.
- Owner's description of the wiring: **"I have the i2c star outside the casing,
  just 1 wire going into the controller."**
- Contact mics feed audio in 2/3 through **BC517 Darlington buffer circuits**
  built for this enclosure. These did not exist in the previous working setup.
- Owner states the rig **worked previously with the NeoTrellis and subsix**,
  and that the problem appeared after the move into the current housing.

---

## 2. Two distinct noises

Established as separate; they respond to different variables.

| | NeoTrellis noise | Lidar noise |
|---|---|---|
| Character (owner) | tonal; pitch tracks LED **colour**; two lit pads sound like "unison / phased analog oscillator"; "decay as brightness goes down" when switching menus; each pad press produces "a synth note analogly" | ticking at the poll rate |
| Responds to | LED brightness, supply voltage, LEDs on/off | ToF polling on/off |
| Does **not** respond to | I²C series resistors, LED data traffic | I²C series resistors, physical presence of the sensor |

---

## 3. Experiments and results

### 3.1 NeoTrellis

| # | Experiment | Result |
|---|---|---|
| 1 | Remove NeoTrellis entirely | noise gone |
| 2 | **Hot-unplug** the board while software continued writing LED data (I²C traffic unchanged) | **noise stopped** |
| 3 | Move power 3.3 V → **5 V** | **substantially worse**; owner: "makes crazy evil sounds now all the time", "as it is booting it makes crazy evil sounds" |
| 4 | Remove from 5 V | "all this noise just disappeared" |
| 5 | Touch power to 3.3 V | **the board rebooted** |
| 6 | `HGSET bright 0` — all LEDs off, pads still functional, I²C traffic unchanged | **zero noise**, including on pad presses |
| 7 | `dark` mode — unlit pads rendered black, lit pads normal | noise reduced ("dark mode def did cut out the neotrellis noise"). **Rejected by owner**: "now i can't tell what effects are on or off" |
| 8 | Full brightness, piezo muted | "still humming just quieter" |

Note on #2 and #6: both leave I²C traffic byte-identical and change only LED
current.

### 3.2 Lidar (VL53L1X)

| # | Experiment | Result |
|---|---|---|
| 1 | `HGSET tof 0` (stop ToF polling), MPU still polling at 15 Hz on same bus | **silent** |
| 2 | `HGSET tof 1` | ticking returned |
| 3 | Lidar **physically unplugged**, software still polling | ticking continued. **Caveat: this was a hot-unplug.** `tofOk` remained latched from boot, so the code continued issuing (failing) ToF transactions. The hardware was removed; the polling was not |
| 4 | New continuous-ranging driver (laser duty ↑, per-tick bus traffic ↓) | owner: "louder than ever" |
| 5 | LEDs off + **piezo muted**, lidar polling and ranging at 15 Hz | **silent** |

Note on #1: the MPU6050 reads 14 bytes per tick at the same rate on the same
bus and produces no noise.

### 3.3 Signal-path isolation

| # | Experiment | Result |
|---|---|---|
| 1 | Mute piezo + TRS (`piezo 0`, `trs 0`) | "much quieter but still there" |
| 2 | Additionally mute all six string channels (every input at zero) | "no difference between this and the piezo mute" |
| 3 | LEDs off + piezo muted, lidar active | silent |

---

## 4. Things that did NOT work

| Attempt | Result |
|---|---|
| **1 µF decoupling capacitors at the sensors** | no measurable difference |
| **100 Ω series resistors on SDA and SCL** (wired in, not soldered) | "literally nothing, no difference" on either noise |
| **Moving NeoTrellis from 3.3 V to 5 V** | made the noise substantially worse |
| **1000 µF electrolytic across NeoTrellis power** | system "just stops and self oscillates" on power-up. **No heat.** Consistent with a brownout loop, not a short. Cap was removed |
| **`dark` mode** | reduced noise but hid populated-but-off slots; rejected on usability |
| **Slowing the I²C clock** | not attempted — owner: "i2c clock just makes it slower doesn't fix it from past experience" |
| **Slower LED refresh** | not attempted — owner: "slower behavior is just slower pulsing right that is unacceptable" |
| **Ferrite beads** | declined by owner |
| **Second USB / separate supply** | declined by owner: "IM NOT PLUGGING TWO USB INTO THIS" |

Both soldering tasks above (the sensor caps and the 100 Ω resistors) were
carried out on advice given **before** a test had pointed at them.

---

## 5. Owner's stated constraints

- The instrument is **not to be rebuilt**: "NOT FUCKING REBUILDING IT."
- Slower LED refresh / pulsing is unacceptable.
- No ferrite beads.
- No second USB connection.
- Full brightness is **not** required: "i dont even want full brightness i just
  want like any amount of consistent light on."
- Target is elimination, not reduction: "i don't want it quiet i want it
  fucking gone."
- Pi audio must not be restarted without explicit permission.

---

## 6. Unresolved questions

1. Why the residual NeoTrellis hum persists at full brightness with every audio
   input muted (§3.3 #2), given that LEDs off produces silence (§3.1 #6).
2. Whether the lidar noise enters exclusively via the piezo buffer (§3.2 #5) or
   whether §3.2 #3 indicates a second path.
3. Whether §3.2 #3's hot-unplug caveat invalidates that observation entirely.
4. Whether the noise reaching the piezo buffer arrives via its **supply** or via
   its **input leads**. Not yet tested. The discriminating test — disconnect the
   piezo elements with the buffer still powered and unmuted — has not been run.
5. What the NeoTrellis's actual current draw and the supply's actual current
   limit are. Neither has been measured. All capacitance figures used in
   discussion were estimates from datasheet typicals, not measurements.

---

## 7. Invalidated tests

Two conclusions during this investigation were drawn from invalid states. Both
are recorded so they are not cited as evidence.

1. **Stale log.** A startup banner read as current was from the previous day.
   The Bela has no RTC and its clock runs a day behind, which disguised it.
2. **Dead bus read as a clean no-traffic test.** `i2cdetect` showed an empty
   bus and the code's sensor reads were gated off, which was interpreted as
   "I²C exonerated." A reboot restored all three devices — the bus had a loose
   connection. The conclusion drawn from it does not hold.

---

## 8. Current board state

| Item | State |
|---|---|
| NeoTrellis | enabled, normal brightness, stock colours (`bright 1.0`, `offdim 1.0`, `dark 0`) |
| Piezo | **muted** (`gVolPiezo = 0`) |
| TRS, 6 strings | normal levels |
| MPU6050 | working, streaming accel/gyro to UI |
| VL53L1X | **working** — returns real distances (~400 mm observed) |
| Audio outputs | 1/2 stereo mix + piezo hard L/R; 3–8 one string each lowE→highE; 9/10 unused |
| `PIEZO_ON_ALL_OUTS` | **still 1** — piezo test tone duplicated across outs 3–10, pending output confirmation |
| Joystick SW | disconnected — wire came out. Pin is **digital 6** |

---

## 9. Live controls (no rebuild, no reboot)

```
printf 'HGSET <name> <value>' | nc -u -w1 192.168.7.2 9001
```

| name | effect |
|---|---|
| `bright` | 0..1 global LED brightness (applied last, includes press flash) |
| `offdim` | 0..1 brightness of unlit pads |
| `dark` | 1 = unlit pads fully off |
| `trellis` | 0/1 LED + pad half |
| `mpu` / `tof` | 0/1 per-sensor polling |
| `tofnorange` | 1 = ranging stopped, poll unchanged (separates laser from transaction — **not yet run**) |
| `polldiv` | 1 = 30 Hz, 2 = 15 Hz, 30 = 1 Hz |
| `piezo`, `trs`, `sub0`..`sub5`, `outA`, `outB` | channel gains |

---

## 10. Separate work completed this session

- **VL53L1X driver rewritten.** It previously returned `-1` indefinitely. Two
  causes: ST's 91-byte configuration blob (registers 0x2D–0x87) was never
  written, so the sensor powered up unconfigured; and the data-ready check used
  the wrong interrupt polarity, which is read from `GPIO_HV_MUX__CTRL` bit 4
  rather than fixed. Ranging now starts once and each poll is a single byte
  read, instead of restarting ranging every tick.
- **Output routing implemented.** Confirmed 10 output channels.
- **Live control channel** extended for runtime A/B without rebuilds.

# THE WATCH — Design Doc

A DIY forearm-mounted "hardcore" smartwatch: an always-on AMOLED watch fused with a
Flipper-Zero-grade radio suite, environmental/motion sensing, lidar, audio, and an
on-demand Linux compute brain. Hand-built (deadbug / protoboard, not PCB), sealed in a
custom hybrid enclosure on a rigid spine with flex straps up the forearm.

Status: **design locked, pre-build.** Author: emgor. Date: 2026-07-16.

---

## 1. Concept & vibe

A wrist-to-forearm device that is equal parts smartwatch, RF multitool, and wearable
sensor platform. It should look like a nice watch at the wrist (premium round AMOLED in a
CNC metal case) and reveal its "hacker exosuit" nature down the forearm (exposed guts
behind intentional windows, status LEDs, antennas, the tool spine). Longer-and-thinner
forearm form factor. Built by hand and proud of it — quality construction, but blobs of
deadbug/protoboard under hot glue in a custom shell, not a fab-house PCB.

**Design tension resolved:** it's a *tool* first (dense data UI, radios, sensors) wearing
the *skin* of a clean round watch. The metal-cased round AMOLED gives the watch look; the
forearm spine carries the tool payload.

---

## 2. Goals & non-goals

**Goals**
- **Activity/fitness tracking** — steps + workout motion (QMI8658 IMU), heart rate + SpO2
  (MAX30101+MAX32664 bio-hub), elevation (BMP280), outdoor routes (GPS); syncs to phone via
  Gadgetbridge. Firmware feature; hardware already in the BOM.
- **Gym-wearable durability** — sweat/splash-tolerant (gasket-sealed enclosure + conformal
  coat), secure sport strap, protected screen (cover glass/recessed bezel), solid connector-
  based internal construction (§7). NOT swim-rated. (Wrist optical HR is approximate under
  intense motion — inherent to all wrist wearables.)
- All-day wearability as a watch (multi-day standby on battery).
- Flipper-equivalent RF capability: sub-GHz, NFC, LF RFID, IR.
- Rich sensing: 9-DOF motion, lidar/depth, GPS, environmental.
- Audio in/out (mic + speaker), haptics done well.
- On-demand heavy compute (Linux) for lidar/mapping/tooling without killing battery.
- Survivable mechanical design on a flexing forearm.
- Serviceable-enough to debug and extend despite deadbug construction.

**Non-goals**
- Not a fabricated PCB product. Hand-built is a feature.
- Not 3D SLAM / ROS-heavy robotics (form factor caps this — see §6).
- Not water-resistant to any real rating (splash-tolerant at best).
- Not a commercial device; personal/authorized use only (see §11 safety/legal).

---

## 3. System architecture — dual MCU (no Linux)

Two microcontrollers, linked over SPI/SDIO + a UART side-channel. **No Linux, no boot wait.**
(Earlier drafts used a Raspberry Pi Zero 2 W as the compute brain; replaced by the ESP32-P4
— instant-on, ~10× lower power, ~50 GPIO, native display/camera. Pi demoted to optional dock.)

### 3.1 Watch brain (always-on) — Waveshare ESP32-S3-Touch-AMOLED-1.43 (CNC metal case)
The integrated core. Chosen after evaluating the full Waveshare round/squircle lineup.

- **MCU:** ESP32-S3 (WiFi 2.4GHz + BLE 5), native USB.
- **Display:** 1.43" round AMOLED, **466×466**, 16.7M color, capacitive touch.
- **Enclosure:** CNC metal case — provides the premium watch look *and* a rigid shell for
  the core for free.
- **Onboard (verify exact ICs on arrival):** QMI8658 6-axis IMU, PCF85063 RTC,
  AXP2101 PMIC (LiPo charging + fuel gauge), battery connector.
- **Role:** the *watch* — runs the watch face + touch UI, IMU/RTC/PMIC (onboard), provides
  **WiFi/BLE for the whole system** (incl. commanding the Chameleon Ultra over BLE), and links
  to the P4. Pin-light on purpose — the P4 hosts the sensor army. Runs 24/7; AMOLED = near-zero
  cost for an always-on face.

Fallback boards if the 1.43" doesn't work out: 2.06" AMOLED squircle (410×502) or 2.1" round
IPS (480×480). Design doesn't depend on the exact board.

### 3.2 Workhorse brain — Espressif ESP32-P4
- Dual RISC-V ~400MHz + a low-power core, up to 32MB PSRAM, **~50 GPIO + multiple UART/I2C/SPI/I2S**,
  USB 2.0 HS host, native **MIPI-DSI (display)** + **MIPI-CSI (camera) with ISP**, 2D accel,
  H.264/JPEG, AI/vector extensions. **No wireless** — the S3 provides it (non-issue).
- **Instant-on (~ms, not ~20s) and low power with an LP core → can be always-on or wake
  instantly.** Beast mode has no boot penalty and no battery cliff.
- **Role/why it fixes everything:** its ~50 GPIO + real UARTs **host the sensor army directly**
  — I2C sensor bus, the **UART gas sensors with no SC16IS752 bridge**, SPI radios (CC1101/nRF),
  I2S field-recording mics, GPS, SD, and the **second display over MIPI-DSI** (bigger/sharper
  than the S3 could drive). Does light on-device vision (QR, object/face, YOLO-nano) — but
  camera here is mainly to **capture/transmit data**, not for its own sake.
- **Boards:** Waveshare ESP32-P4 Module-DEV-KIT (cleanest 40-pin/HAT header for protoboard) or
  M5Stack Tab5 (5" 1280×720 screen + 2MP camera + P4 in one — could BE the forearm console).
- **Link to S3:** SPI or SDIO for bulk (frames/data) + a UART side-channel for commands
  (ESP-Hosted-style RPC).

### 3.3 Optional Pi dock — only for general SDR
- **Dropped from the daily build.** The only thing that still needs Linux is **open-ended
  wideband SDR** (tune-anywhere spectrum/waterfall, decode arbitrary unknown signals) via
  RTL-SDR + librtlsdr. *Named* signals (planes/ships/ISM sensors/pagers) decode on-MCU — see §4.1b.
- If you ever want the SDR playground, add a Pi as a **detachable dock** powered only during a
  session — so you never pay its power/boot cost in daily wear.

---

## 4. Subsystems & bill of materials

Grouped by function. "On core" = already integrated on the watch board.

### 4.1 Radios (the Flipper-equivalent stack)
| Function | Part | Bus | Notes |
|---|---|---|---|
| WiFi + BLE | ESP32-S3 (on core) | — | 2.4GHz only |
| Sub-GHz (315/433/868/915) | **Ebyte E07-M1101D-TH** (CC1101, through-hole DIP) | SPI, 3.3V | No Adafruit/SparkFun board; Ebyte DIP is protoboard-friendly. Buy variant for target band; antenna (whip/coil or -SMA) OUTSIDE metal case |
| **RFID — LF + HF, full read/write/clone/emulate** | **Chameleon Ultra** (~$100–130) | USB→Pi / **BLE→ESP32** | **Unified RFID engine — replaces separate NFC + LF reader.** Coin-sized (40×24×8mm), 8 LF + 8 HF emulation slots, ~99% of 125kHz + full MIFARE/NTAG/DESFire HF. Finished module (not raw IC). ESP32 can command it over BLE without waking Pi. |
| (optional) deep cracking | **Proxmark3 Easy** (~$60–110) | USB→Pi (headless client) | docked/bench companion for nested attacks/sniffing Chameleon can't do. ~87mm — NOT wrist-wearable. |
| (optional) always-on HF tap | PN532 (~$10) | I2C | only if you want a soldered-in HF reader independent of the Chameleon |
| IR TX + RX + learn | **Adafruit IR TX/RX board** (kit, capture+replay) | GPIO | best single board; drive LED via transistor for range |
| 2.4GHz extra radio | generic **nRF24L01+** module (+ regulator adapter) | SPI, 3.3V | dual-row 2×4 header; add 10µF cap across VCC/GND (noisy power) |
| iButton/1-Wire | DS9092-style | 1-Wire | one pin, keep |
| BadUSB / USB HID | ESP32-S3 native USB (+ Pi USB-OTG) | USB | keyboard/mouse emulation, Rubber-Ducky; zero added parts |
| U2F / FIDO token | via USB HID | USB | watch acts as hardware 2FA key |
| GPIO "hacker header" | few free pins | GPIO | expose for hacking, Flipper-style |
| (optional) wideband SDR | RTL-SDR dongle | USB→**Pi dock only** | ONLY for general spectrum/waterfall/unknown signals; named signals decode on-MCU (§4.1b) |

**RFID resolved:** the **Chameleon Ultra** does full LF+HF read/write/clone/emulate in one
coin-sized module — no separate LF reader, no T5577-writer gap, no separate NFC chip needed.
Other radios (CC1101/nRF) are **3.3V** → single clean 3.3V rail; keep CC1101 + nRF antennas
separated.

### 4.1b Radio LISTENING (receive/decode named signals) — no Linux needed
Goal is to receive specific data and log/transmit it, not general spectrum analysis. Each of
these decodes ONBOARD on a microcontroller — no RTL-SDR, no Linux:
All decode ONBOARD → structured data (JSON/frames/NMEA) into a **signal-browser UI** on the
console display (nearby aircraft, ships, ambient sensors, pager traffic, AQI map).
| Target | How (on-MCU) | Output |
|---|---|---|
| ISM sensors 433/915MHz (weather, TPMS, meters) | **CC1101 (already have) + rtl_433_ESP** | JSON/MQTT — the "ambient sensor harvest," zero extra HW (OOK subset, ~½ range, 1 freq at a time) |
| ADS-B aircraft (1090MHz) | **GNS5892R module** + dump5892 on-MCU (or ADSBee board) | UART → ICAO/callsign/lat-lon/alt/speed |
| AIS ships (162MHz) | **dAISy FeatherWing/HAT** (self-contained) | UART → NMEA (MMSI/pos/course/name) |
| POCSAG pagers | CC1101 + **OpenPager** library | address + message text (FLEX not supported on-MCU) |

**Still needs Linux + RTL-SDR (optional Pi dock only, §3.3):** open-ended wideband scanning,
waterfall, arbitrary unknown signals, and NOAA APT weather-sat imaging (DSP-heavy).

### 4.2 Sensing
| Function | Part | Bus | Notes |
|---|---|---|---|
| 6-axis IMU | QMI8658 (on core) | I2C | accel + gyro |
| GPS (separate) | **SparkFun MAX-M10S** (Qwiic, ~$40) — u-blox M10, UART+I2C | UART | Local → **standalone geotagged AQI mapping**. Duty-cycle (~25–50mA); antenna on top/outside metal. (Alt: Adafruit PA1010D ~$30, built-in antenna, MediaTek.) |
| Magnetometer (separate) | **PNI RM3100 breakout** (~$20, I2C+SPI) | I2C/SPI | premium mag (23× res, 33× lower noise). Fuse with onboard QMI8658. **Mount away from speaker/haptic magnets, power, metal case**; figure-8 cal. |
| Lidar / depth (core) | VL53L5CX 8×8 ToF | I2C | wrist-friendly depth "camera"; ESP32 can read it |
| Lidar (demo/flex, optional) | RPLidar A1 / LD06 | UART→Pi | full 360° radar sweep; bulky, Pi-driven, high current |
| Barometric pressure / altitude | BMP280 | I2C | pressure/altimeter (T/H/VOC now covered by SEN55, §4.2b) |
| Heart rate / SpO2 | **SparkFun MAX30101 + MAX32664** (Qwiic, ~$40) | I2C | **MAX86141 is raw-chip-only (no breakout) — dropped.** This board's MAX32664 "bio-hub" runs HR/SpO2 algorithms on-chip. (Alt: ProtoCentral MAX86150 ~$35 for PPG **+ ECG**.) |
| Skin/body temperature | **MAX30208** (~$3) | I2C | mainstream-parity: cycle tracking, illness/fever, sleep temp — more accurate than ambient or the thermal cam for skin contact |
| ECG (single-lead, optional) | MAX86150 (PPG+ECG) **or** AD8232 + electrodes | I2C/analog | heart-rhythm/AFib. Needs a case-back electrode + a touch electrode (other hand). Mainstream health feature if wanted |
| Gesture / prox / color / light | APDS-9960 | I2C | wave-to-control UI |
| Thermal camera | MLX90640 (32×24) | I2C | low-res thermal vision; very on-brand |
| **EMF audio probe** ("Elektrosluch"-style) | differential coil + contact plate → high-Z JFET/CMOS preamp → I2S/audio → P4 | audio | **Touch/point at electronics, hear their EM emissions.** Self-noise is the hard part (watch = big EMI source) — mitigations below |
| UV index | LTR390 | I2C | sun/UV exposure — high value at the beach |
| Sound level (dB SPL) | (uses the mic) | — | noise-pollution tracking, zero added hardware |
| Lightning detector | AS3935 — **SparkFun (SPI) or DFRobot Gravity (I2C)** | I2C/SPI | storm activity + distance. **AVOID cheap CJMCU/GY clones** (mistuned antenna = dead detection) |
| (optional) Pi camera / Geiger / fingerprint | — | CSI / GPIO / UART | vision-ML / radiation / biometric unlock |

### 4.2b Air quality — single all-in-one sensor
**Primary pick: Sensirion SEN55** — one I2C module covering the most in a single part:
**PM1.0/2.5/4.0/10 particulates, VOC index, NOx index, humidity, temperature.**
| Metric set | Part | Bus | Notes |
|---|---|---|---|
| PM + VOC + NOx + RH + T | **Sensirion SEN55** | I2C | all-in-one; **5V supply**, has a fan (~chunky, duty-cycle it); check I2C level (3.3V-compatible, else level-shift) |
| (cheaper) drop NOx | SEN54 | I2C | same minus NOx |
| (add if CO2 wanted) true CO2 | Sensirion SCD41 | I2C | SEN55 does NOT do true CO2; add this small NDIR part for it |
| (compact alt, no PM) | Bosch BME688 | I2C | tiny, no fan, VOC+T/H/P only — big coverage drop vs SEN55 |

**SEN55 coverage reality (esp. for LA / Santa Monica use):**
- ✅ Great for the #1 acute hazard — **wildfire smoke / PM2.5** (real laser PM, tracks AQI well).
- ❌ **No ozone (O3)** — LA's signature pollutant and a serious respiratory hazard. Real gap.
- ⚠️ NOx & VOC are **relative indices** (metal-oxide), not calibrated ppb — trend, not threshold.
- ⚠️ Consumer-grade: good for trends, humidity-sensitive, not reference-accurate absolute values.
- ⚠️ No CO.

**Minimum trustworthy AQI set for LA (fewest sensors that actually cover it):** no single
consumer sensor does full AQI — pro/research low-cost monitors all pair laser PM + separate
electrochemical gas sensors. Floor is **3 sensors:**
1. **SEN55** — PM + VOC/NOx index + RH/temp (particulate backbone; great for smoke/smog)
2. **Electrochemical ozone** — closes LA's biggest gap
3. **Electrochemical NO2** — traffic pollution
Add **SCD41** (true CO2) and **electrochemical CO** for the complete set (5 total).

**Critical gotcha — NO2/ozone cross-sensitivity:** electrochemical NO2 sensors are
cross-sensitive to O3 and can't separate them alone. Standard fix: measure **"OX" (O3+NO2
combined) + NO2 separately, then subtract** for true ozone. So the accurate pair is an
**OX sensor + an NO2 sensor.**

**Parts — CHOSEN: SPEC Sensors DGS2 digital modules** (best + protoboard-friendly, no raw IC):
- One each for O3/NO2/CO (~$100–150/gas, ~20×44mm), **single-row 0.1" pins**. Onboard
  LMP91000 + temp compensation + **factory calibration** + Arduino libs = least DIY pain.
- They're **UART** → connect via an **SC16IS752 I2C-to-UART bridge** so they ride the
  always-on I2C bus without eating the ESP32's hardware UARTs (see §5.1). Best of both:
  easiest/most-accurate gas modules AND always-on AQI.
- Rejected: ULPSM analog+ADC (more raw/DIY calibration); Alphasense B4+ISB (bulky ~20mm cans,
  overkill); bare LMP91000 (Soldered Electronics is the only breakout, O3 unconfirmed).
- Avoid metal-oxide O3 for health decisions.

**Why raw AQI data is inaccurate:** electrochemical gas sensors drift (baseline wanders,
electrolyte ages), depend on temp + humidity, and cross-react (NO2↔O3). Optical PM sensors
read high in humid air (particles absorb water) and aren't factory-calibrated to a reference.

**Onboard LOCAL calibration (no internet needed):**
- **Zero/baseline** — expose to known clean air, set zero. Some parts self-baseline (SCD41
  auto-cal assuming periodic ~400ppm fresh air; VOC sensors auto-baseline).
- **Temp/RH compensation** — run the correction math on-device using the onboard SEN55/BME280
  RH+temp. Fixes most of the PM humidity error + gas temp dependence. **Fully local.**
- **Cross-sensitivity** — OX − NO2 subtraction for true ozone. Local math.
- Build these into an **onboard calibration sequence.**

**The one thing needing an external reference (ONCE):** span/absolute accuracy — co-locate
next to a reference instrument or trusted station **one time**, derive correction coefficients,
**store them onboard.** After that, run fully local/real-time forever. Phone/WiFi becomes
optional, not required — and onboard RH/temp means you don't even need phone weather data.

**Optional API cross-reference over WiFi (phone 5G):** PurpleAir, AirNow/EPA, SCAQMD — only
for the one-time span check or occasional sanity checks, not for normal operation.

### 4.3 Human I/O
| Function | Part | Bus | Notes |
|---|---|---|---|
| Display + touch (watch) | 1.43" AMOLED (on core) | QSPI + I2C | 466×466 round |
| Display 2 (console) | bar/stretched MIPI LCD → **P4** | MIPI-DSI | see §4.6; runs up the forearm, P4-driven (hosts the signal-browser UI) |
| Voice mic | onboard (watch board) | I2S/PDM | voice-grade only |
| **Stereo mics + dB meter (double duty)** | **Infineon IM69D130 Shield2Go** (~$21) | I2S→**ESP32** | **Breakout EXISTS, single board, already STEREO** (2 matched elements), I2S via onboard ADAU7002 (castellated 2.54mm, 3.3V). Does field recording AND calibrated SPL: 130dBSPL AOP (loud), low noise floor (quiet), 69dB(A) SNR. **On the ESP32** for always-on dB meter + WAV-to-SD. Power from clean LT3045 (§6.1); short I2S runs. Fallback: 2× Adafruit SPH0645 (~$7 ea). |
| Speaker | I2S amp (MAX98357A) + small speaker | I2S | verify onboard; likely add |
| Piezo buzzer | piezo | GPIO/PWM | simple alerts, backup to speaker |
| Haptics | DRV2605L + LRA | I2C | real haptic patterns, not one dumb buzz |
| Status/flair LEDs | WS2812 x few | 1 GPIO | addressable RGB |
| Buttons | **2 tactile side buttons** (Yes/Select + No/Back) | GPIO/expander | confirm + cancel; long-press = secondary action (~4 functions from 2 buttons). Debounced, active-low |
| **Rotary encoder ("crown")** | mini mechanical encoder w/ push, **or AS5600 magnetic** (I2C) | GPIO / I2C | scroll + press-to-select; LVGL native encoder support. AS5600 = tiniest, contactless, 0 extra pins (no detent feel) |
| **5-way nav switch** (small "joystick") | SMD 5-way tactile (~6–7mm) | GPIO/expander | up/down/left/right/press — menu nav without the screen; far smaller than a thumbstick |

### 4.4 Power & storage
| Function | Part | Notes |
|---|---|---|
| PMIC / charger / fuel gauge | AXP2101 (on core) | charges + monitors the LiPo |
| Battery | 1S LiPo **~5000mAh** | balanced weight/runtime; hard battery bay |
| Charge/data | USB-C | flashing + charging; **no micro-USB** |
| Storage | microSD (onboard if present, else SPI breakout) | data logging; shares SPI bus + 1 chip-select |
| Power-distribution board | 5V boost + 3.3V rail (Pololu buck-boosts) + load switches | **separate from AXP2101** — powers P4/screen/radios; see §6.1 |
| Protection | inline fuse + LiPo protection board | non-negotiable on a wearable cell |

### 4.5 Bus/pin expansion (the real constraint — see §5)
- **TCA9548A** I2C multiplexer (fan out to 8 I2C buses; resolves address collisions).
- **MCP23017** GPIO expander (extra chip-selects / GPIO if short).

### 4.6 Second display — the forearm "console"
A rectangular bar screen running up the forearm behind the watch core, driven by the **Pi**
(so it's only lit in beast mode). **Orient the long axis wrist→elbow** — the forearm's long,
narrow shape matches a ~3:1–4:1 bar/stretched panel.

**Size it to your own arm — measure first:** (1) flat run from behind the watch core to just
before the elbow crease = length budget; (2) comfortable flat width across the top of the
forearm = width budget. Typical adult ≈ **15–20cm length × 5–6cm width.**

| Option | Active area | Res | Fit |
|---|---|---|---|
| **Recommended** | ~150 × 55mm (~6" bar) | ~1280×400 | fits most forearms; good console |
| Max flex | ~210 × 58mm (8.8" ultrawide bar) | 1920×480 | ~21cm long — only if you've got the reach |
| Conservative | 4–5" (800×480) | 800×480 | fits any arm, less spine coverage |

Interface: mini-HDMI or DSI off the Pi Zero 2 W. Adds ~200–500mA when lit (hits beast-mode
runtime — see §6).

---

## 5. Interconnect & pin strategy

**The old constraint (S3-only) is gone.** The integrated watch board's ESP32-S3 has few free
pins (AMOLED QSPI/PSRAM/touch/IMU/RTC/PMIC eat most). Rather than cram everything onto it, the
**ESP32-P4 hosts the sensor army** — it has ~50 GPIO + real UARTs. The S3 stays pin-light.

### 5.1 Pin budget — SOLVED by the ESP32-P4
**ESP32-S3 (watch, pin-light):** onboard display/touch/IMU/RTC/PMIC · USB (GPIO19/20) ·
**SPI or SDIO link + UART to the P4** · **BLE to Chameleon Ultra** (no pins) · optionally IR
+ WS2812. Comfortable within the board's exposed pins.

**ESP32-P4 (workhorse, ~50 GPIO) hosts everything pin-hungry:**
| Function | Interface |
|---|---|
| I2C sensor bus → **TCA9548A mux** → ~20 sensors | I2C |
| **SPEC DGS2 gas sensors — direct on P4 UARTs (no SC16IS752 bridge)** | 3× UART |
| CC1101 (sub-GHz + ISM/POCSAG RX) + nRF24 | SPI + CS |
| Stereo I2S mics (recording + dB meter) | I2S |
| GPS, dAISy (AIS), GNS5892R (ADS-B) | UART |
| SD card logging | SPI/SDIO |
| **Second display** | MIPI-DSI |
| Camera (light vision / data capture) | MIPI-CSI |

- P4's UART count kills the SC16IS752-bridge workaround; gas + listening modules connect natively.
- Keep the **TCA9548A** I2C mux for sensor count/address/capacitance management.
- **Noise:** I2C/mux digital & robust; analog-sensitive parts (mics, RF) on the clean LT3045
  LDO, physically separated; short I2S runs.
- Verify GPIO breakout on both boards (§13) — with the P4 there's ample headroom.

---

## 6. Power architecture, budget & runtime

### 6.1 Architecture — one cell, two power domains
*(User is building their own power system — this section is reference/constraints.)*

**Topology:** battery-in-the-middle — USB-C **charges** the LiPo; the LiPo **powers**
everything. Safest, no load-sharing. The board's onboard USB-C is dual-purpose: it charges
the cell (via AXP2101) *and* carries ESP32-S3 native USB, so **BadUSB/HID out to a target
computer works through that same port** (and charges from the target while plugged in). USB
*host* to external gadgets (RTL-SDR etc.) is the **Pi's** port, not this one.

**Charger decision — use the onboard AXP2101 (sole charger).** It's a real X-Powers PMIC
(CC/CV, I2C-configurable charge current ~1A, thermal regulation, fuel gauge, optional NTC),
as good or better than a standalone TP4056. ~1A into a 5000mAh cell = safe 0.2C (~5h). Only
add a dedicated charger if you want 2A fast-charge or can't tap VBAT. **Do not run two
chargers on one cell.** (Charging while the Pi runs hard just slows charge — normal/safe.)

**External USB-C jack (recommended — onboard port is buried in the CNC case):** bring USB
out to an enclosure-mounted USB-C breakout (**Adafruit #4090** or **SparkFun USB-C Horizontal**
— both include the 5.1kΩ CC pulldowns + full data breakout) by tapping ESP32-S3 **D− = GPIO19,
D+ = GPIO20, + VBUS + GND**. That port then does charge + flash + **BadUSB** just like the onboard one.
Requirements: (1) breakout must have **5.1kΩ CC pulldowns** or it won't enumerate as a
device; (2) **don't parallel two live receptacles** — use only the external jack (leave
onboard unused) or plug one at a time; (3) keep the D+/D− stub short, don't swap the pair.

**Chosen topology — AXP2101 as SOLE charger, one shared VBAT node:**
```
USB-C ──charge──► [AXP2101 = SOLE charger] ──► charges CELL
              ┌─ inline FUSE ─┬──► watch board battery connector (AXP2101 runs watch, always on)
   BATTERY + ─┤               ├──► [Pololu S13V30F5 buck-boost] VBAT→5V   ──►(load switch)──► P4, 2nd screen, SEN55
              │               └──► [Pololu S13V25F3 buck-boost] VBAT→3.3V ─►(load switch)─► radios+sensors (+ local LDO at RF)
   BATTERY − ─┴──────────── COMMON GROUND: tie ALL grounds together ──────────────────────────
S3 ◄─SPI/SDIO+UART─► P4   (P4 = workhorse: sensors, listening modules, 2nd display, camera)
```
Wiring rules at the split:
- **One node, three parallel taps** (watch board + boost input + buck input) — all just draw
  from the cell; converters take raw VBAT in, output 5V/3.3V.
- **Watch face runs on the raw cell (~3.7V), NOT the 5V rail** — its battery input expects
  3.0–4.2V; feeding 5V overvolts it (or via USB-C creates a wasteful charge-loop).
- **Common ground everywhere** — watch board, boost, buck, every component. I2C/UART/USB all
  reference ground; broken common ground = intermittent hell.
- **Inline fuse at battery +** before the split; LiPo protection board on the cell.
- **ESP32-gated load switches** (high-side MOSFET) on the P4 and radio branches → "few at a
  time" enforced in hardware. Watch-board branch is NOT switched (always on). (P4 is low-power
  enough to leave on, but keep the switch for optional deep-sleep of heavy peripherals.)

**Two rails — chosen parts: Pololu buck-boost regulators (slim, reliable, low heat).**
Switching buck-boost runs 85–90%+ efficient → minimal heat (vs. an LDO that burns the
delta as heat). Buck-*boost* also holds regulation across the full LiPo range.
- **5V rail → Pololu S13V30F5** (5V, 3A buck-boost, Vin 2.8–22V) → ESP32-P4, second
  screen, SEN55, other 5V modules. Keep physically away from antennas.
- **3.3V rail → Pololu S13V25F3** (3.3V, 2.5A buck-boost) → radios/sensors. Run straight off
  VBAT — the boost stage holds 3.3V even when the cell sags below 3.3V near empty, so **no
  brown-out and no "derive from 5V" workaround needed.** (Smaller S7V7F5 if less current OK.)
- **Noise (audio + RF):** the switcher is the right *bulk* supply (cool, efficient); clean up
  sensitive rails downstream with an **ultra-low-noise LDO — LT3045** (~0.8µV RMS, RF/audio
  grade). **No Adafruit/SparkFun breakout exists** → use the **ldovr.com "LT3045-A" assembled
  module (~$25–40)** (or PatrickBaus open-source PCB). Avoid TPS7A20/ADP150 — raw-chip-only.
  Chain: `battery → Pololu switcher → LT3045 module → field-recording mics + RF (CC1101/NFC)`.
  Low heat from the switcher **and** ultra-low noise where it matters. Keep the LDO + mics
  physically away from switchers/radios; tidy analog ground.
- "5V isn't safer" — match the rail to each part's spec; 3.3V-logic parts fed 5V die.
  **Level-shift** I2C between any 5V-supplied sensor and the 3.3V logic.

**Fuse placement:** at the **battery + terminal, before the split** — protects ALL
downstream. Order from cell: `CELL(+) → LiPo protection board → FUSE → split node → loads`.

**Noise toolkit (ranked by bang-for-buck — ferrite is NOT the important one):**
1. **Decoupling caps everywhere** — 0.1µF ceramic at every IC power pin + 10–100µF bulk at each
   board/domain power entry. The biggest easy win.
2. **Star grounding** — separate analog/RF ground (mics, gas AFE, EMF probe, radios) from
   digital/switching ground; join at ONE point near the battery. Most overlooked win; free.
3. **Physical separation** — switcher + radios (noise SOURCES) far from mics/gas AFE/EMF probe
   (noise SINKS). Distance is free.
4. **Twisted pairs** on signal+gnd / power+gnd runs (I2C, audio, analog).
5. **Clean LT3045 LDO** feeds the analog domain (§6.1).
6. **Ferrite only as a supplement, as a pi-filter** (cap→bead→cap) on a noisy module's power —
   more reliable than a lone bead. Optional; skip if 1–3 are solid.
7. **Grounded shield** (copper tape / tin can, or the grounded CNC case) over the switcher or a
   sensitive front-end — targeted fix.
Model: switcher/radios = sources; mics/gas AFE/EMF/analog = sinks. Separate grounds + separate
physically + decouple + clean-LDO the sinks = ~90% done without a ferrite.

**Grounding plan — ONE ground system, 3 return domains, 1 star point:**
- **AGND (quiet):** mics, EMF probe, gas AFE, RF front-ends (everything on the LT3045).
- **DGND (digital):** MCUs, I2C sensors, display logic.
- **PGND (power/dirty):** switching regs, radio TX, haptic motor, P4, backlight, battery return.
- All three meet at **ONE star point = battery negative** ("ground mecca"). Each domain returns
  with its OWN wire — never daisy-chain quiet→dirty. Local ground bus/pour per protoboard; RF
  modules keep short direct local grounds. **Star, not mesh** (avoids ground loops).

**Decoupling scheme (baked into every subsystem):**
- Per IC: 0.1µF ceramic at VCC–GND (**breakouts already include this**).
- Each domain/board entry: 10–100µF bulk.
- Every spiky load gets a LOCAL bulk cap: **nRF24 needs 10µF across VCC/GND (its #1 failure
  mode)**, haptic motor, P4, backlight, SEN55 fan.
- VBAT node: 100–470µF bulk (absorbs pulses, prevents watch-core brownout).
- LT3045 + Pololu regs: datasheet in/out caps (Pololu modules have theirs).
- Optional pi-filter (cap→ferrite→cap) on any still-noisy module.

**Two separate "manages":** *power* (charging + boost/buck + load switches — your circuit) vs.
*compute/peripheral* (the P4 runs the heavy peripherals). Don't conflate.

**Key fact: the ordered board's AXP2101 can *charge* the battery, but it is NOT sized to
*power* the P4 + second screen + radios.** One LiPo feeds two separate domains:

- **Domain A — watch core:** the onboard **AXP2101 charges the cell and runs the ESP32-S3
  board only.** No separate charger needed (one cell, one charger — do not add a second
  charger to the same cell). Onboard charge current is ~1A, so ~5h to fill 5000mAh; only
  raise it if the AXP2101's charge current is configurable.
- **Domain B — everything else (dedicated power-distribution protoboard, fed from raw VBAT):**
  - **5V boost (Pololu S13V30F5)** → ESP32-P4 + second display.
  - **3.3V rail (Pololu S13V25F3)** → radios + sensors.
  - **Load switches** on each rail → P4 (optional deep-sleep), screen, and radio bank only
    draw when enabled. The "only a few at a time" strategy, enforced in hardware.
  - **Bulk capacitance** near spiky loads + **inline fuse** + **LiPo protection board.**

So: `LiPo → [AXP2101: charge + watch core]  +  [Pololu rails, load-switched: P4 + screen
+ radios]`. A 5000mAh cell handles the current pulses; keep bulk caps near the boost input
so the watch core doesn't see VBAT sag/brownout.

### 6.1b Power-gating & shared-antenna strategy (duty-cycling)
Most things OFF by default → only the S3 watch + a couple sensors are always-on (this is what
buys the ~4–7 day runtime). Gate everything else.
- **Regulator still required** — the Pololu rails make clean 3.3V/5V; **load-switch ICs**
  (TPS22918-class: EN pin + soft-start/inrush limit + reverse blocking) gate power from the rail.
- **Don't do 1 switch per sensor — group into ~7 POWER DOMAINS** by when they're used, one
  load switch each: (1) always-on (S3/RTC/IMU, never gated), (2) air-quality cluster (SEN55 +
  gas + SCD41 + BME280), (3) radio cluster (CC1101/nRF/RX front-ends), (4) nav (GPS + RM3100),
  (5) misc sensors (MAX30101/APDS/LTR390/AS3935/MLX90640), (6) audio (mics + amp), (7) beast
  block 5V (P4 + display + camera).
- **Drive all the EN pins from one MCP23017 I2C GPIO expander** — ~7 domain switches + the RF
  switch select lines, all over the 2 I2C wires, **zero dedicated GPIO**. Firmware writes the
  expander to bring a domain up/down. (Direct P4 GPIO also works given ~50 pins, but the
  expander centralizes control and frees real pins.)
- **5V-rail gating needs a load-switch IC** (not a bare P-FET) — a 3.3V logic pin can't fully
  turn off a P-FET whose source is 5V; the IC handles the gate internally.
- **Direct-from-GPIO power** works ONLY for tiny loads (I2C sensor ~1–5mA); a pin sources
  ~20mA safe (40mA max). Anything bigger (SEN55 fan, radios on TX, display, P4) needs a load switch.
- **Gate on the right rail:** 3.3V devices off the 3.3V rail, 5V devices (P4/display/SEN55) off
  the 5V rail. Can't power a 5V device from a 3.3V GPIO.
- **I2C back-powering gotcha:** a power-gated I2C sensor can parasitically self-power through
  SDA/SCL (pull-ups keep the bus high). Fix: gate the pull-ups with the sensor, or power down a
  whole TCA9548A mux segment together, or use an isolating load switch.
- **Shared antenna = RF SWITCH, not parallel wiring.** You can't hang multiple radios on one
  antenna (detunes/couples). Use a GPIO-controlled **RF switch IC** (PE4259-class) that routes
  the shared antenna to the active radio: radio powers on → its GPIO also sets the switch.
  Works for time-shared radios (sub-GHz, listening bands); **GPS + always-on 2.4GHz BLE stay on
  dedicated antennas.**

### 6.2 Runtime
Usable ~4500mAh from a 5000mAh 1S cell. **The P4 (instant-on, low-power) transforms this vs.
the old Pi plan — no 20s boot, no ~1W idle drain, so "beast mode" is far cheaper.**

| Mode | What's on | Avg draw | Est. runtime |
|---|---|---|---|
| **Watch** | AMOLED wake-on-raise, IMU+BLE, P4 light-sleep, radios idle | ~20–40mA | **~4–7 days** |
| **Active** | screen on, radios/listening decoding, P4 working | ~150–300mA | **~1 day** |
| **Beast** | P4 full tilt + 2nd screen lit + camera + radios hot | ~0.5–0.9A | **~5–8 hrs** |

Takeaways:
- No Linux/Pi = no 20s boot and no big idle sink → beast mode roughly doubled vs. the Pi plan.
- AMOLED makes the always-on watch face nearly free.
- 5000mAh is the right balance vs. a 10000mAh brick or a tiny cell.
- Provide a **2A-capable USB-C charge path**; the AXP2101 handles charge management.

---

## 7. Mechanical & physical

**Form:** rigid spine running wrist→forearm, longer and thinner. Flex straps at points.

- **Rigid spine** carries the electronics and the battery bay so the deadbug/protoboard
  joints never flex. **The forearm bends constantly — rigid solder blobs on a flexing
  substrate crack over weeks.** Keep all rigid mass on the spine; let only fabric/silicone
  straps flex.
- **Watch core** (CNC-cased round AMOLED) mounts at the wrist end.
- **Battery bay:** hard-walled, no flex on the cell, puncture-protected. This is the single
  most safety-critical mechanical element (LiPo against skin).

### 7.1 Deployable sensor mast ("the spar")
A single reinforced, foldable **gooseneck mast** anchored to the rigid spine that consolidates
everything wanting distance from the noisy metal body. One structure solves many separation
problems at once.
- **Payload (staggered along the mast, NOT all at the tip):** GPS antenna (top, sky view),
  sub-GHz (CC1101) + 2.4GHz (nRF) antennas (spaced apart to avoid desense), ADS-B (1090MHz) +
  AIS (162MHz) listening antennas, the **EMF probe tip** (away from watch EMI = the whole point),
  optional camera/thermal.
- **Form:** reinforced **gooseneck** (bend-and-hold, tough, internal wire routing) — best fit
  for "bendy + hyper-reinforced." **Parts:** SnakeClamp raw gooseneck stock (8–10mm OD, keep
  ~20–25cm to limit weight) or Harfington threaded goosenecks. Hybrid option: a telescoping
  whip (doubles as antenna + strut) as the rigid base + a short gooseneck tip. **Foldable/
  stowable** flat along the forearm when not deployed.
- **Wiring up the mast:** **RG316** thin coax on repeatedly-flexing joints; **RG402
  hand-formable** coax for a set-once rigid tip; **26–30AWG silicone** wire for DC/EMF leads —
  all bundled inside the gooseneck bore.
- **Gotchas (why it's "hyper-reinforced"):**
  - **Magnetometer does NOT go on the bendy mast** — a flexing mast rotates it relative to the
    watch and breaks compass heading. Keep the RM3100 on a **separate short RIGID outrigger**
    (or give the mast a locked "deployed" detent if you insist on mounting it there).
  - **RF feedlines:** each antenna needs its own coax up the neck — several shielded cables,
    real loss; stagger antennas by height and mind impedance.
  - **Flex fatigue:** repositioning flexes internal wiring → use **flex-rated stranded silicone
    wire / flex-PCB with strain relief both ends** inside the reinforced sheath. This is what
    the reinforcement actually protects.
  - **Base:** solid anchor on the rigid spine (leverage/torque); stow to avoid catching.

### 7.2 Antenna strategy
**Buy connectorized module variants + quality antennas where range depends on it; don't force
external on everything.**
- **External-antenna (u.FL/SMA) + quality antenna:** GPS (active patch w/ LNA), sub-GHz
  (CC1101 **-SMA** + tuned 433MHz whip), ADS-B (GNS5892R), AIS (dAISy), nRF24 (**+PA/LNA+SMA**
  for range). The antenna is the biggest lever on range — a good antenna beats a better radio.
- **Leave built-in:** Chameleon Ultra (internal coils, proximity), 2.4GHz WiFi/BLE (small band,
  on-board fine — just route outside the metal case).
- **Connector standard:** `board u.FL → u.FL-to-SMA pigtail → SMA bulkhead on mast → SMA antenna`.
  SMA = rugged swap point; u.FL = fragile board stub only (few mating cycles). Also routes the
  antenna OUTSIDE the CNC metal case (fixes shielding).
- **Merging:** wideband RX antenna for the listening bands; **diplexer** to share one
  antenna across different-band radios; **RF switch** to time-share one antenna among radios
  used one-at-a-time. GPS + TX radios stay dedicated (weak-signal / resonant).
- **Caveats:** quality ≠ beating physics (AIS 162MHz wants ~46cm — wrist size limits its range
  regardless); buy tuned/reputable 50Ω parts (cheap untuned antennas = bad SWR = lost range).

**Minimal antenna set — 4 physical antennas is the floor (researched):**
1. **Active GPS patch** (Uputronics ~$30, or 18×18mm ~$8) — standalone (LNA+bias, can't share).
2. **433MHz tuned whip** (~16cm, Mayhem/Tindie) — dedicated (it TRANSMITS; passive share is lossy).
3. **2.4GHz flexible u.FL** (Adafruit #2308, ~$4) — tiny, no reason to merge.
4. **Wideband RX whip (25–1300MHz) + GPIO-controlled SP3T/SP4T RF switch** (PE4259-class) —
   covers **AIS 162 + ADS-B 1090 + scanning in ONE antenna** (collapses 3→1). The switch time-
   shares it among the RX front-ends (Nooelec 1090 antenna optional for better ADS-B).
Merging hardware: **RF switch** (PE4259 SPDT ~$3, or Skyworks SP3T/SP4T) for time-share;
**chip diplexer** (Abracon/Johanson) only if two different-band radios must listen at once.
Connectors: u.FL→SMA bulkhead pigtail (Adafruit #851); swap antennas at the SMA, not u.FL.
- **Parts philosophy: breakout boards wherever possible**, else protoboard-mountable modules
  with 0.1" headers — avoid raw fine-pitch chips (most sensors are QFN/LGA, near-unsolderable
  by hand; breakouts also bundle the decoupling/pull-ups/regulators and survive flex better).
  Size is the only tradeoff — fine on a forearm (see space note).
- **Space realism:** forearm spine ≈ 15–20cm × 5–6cm (~90–120cm²) — area is ample; small I2C
  breakouts tuck in easily. **Constraint is thickness/weight**, driven by the bulky items:
  5000mAh battery (~10×6cm, biggest object), ESP32-P4 board, 2nd display, SEN55 (fan), and the
  SPEC DGS2 gas modules (~20×44mm each). It will be a chunky brick — distribute weight along
  the spine. **Phase the build:** Stage 1 = watch core + P4 + small always-on I2C sensors;
  Stage 2 = the bulkier block (2nd screen, gas modules, camera, listening modules). Consider
  starting gas with just the O3+NO2 pair (LA essentials) to cut bulk.
- **Construction:** one solid unit (sealed) in a custom shell.
  - **Sensor cluster on one shared protoboard** — all the I2C sensors together: shared ground
    pour, tidy short I2C bus, decoupling caps next to each chip. Good for noise.
  - **Deadbug / point-to-point** for the RF and power sections; keep the switching boost
    physically away from the sensor board and antennas.
  - Bus spine between watch board ↔ sensor protoboard = 4 wires: SDA, SCL, quiet 3.3V LDO, GND.
  - Hot glue softens ~60–70°C and is a thermal insulator — do NOT entomb heat sources in it.
- **Assembly method — "solid but repairable" (don't drown it in hot glue):**
  - **Connectors (JST-GH/headers) at every subsystem boundary** → rework by unplugging; rarely
    cut a soldered joint. This is the backbone of repairability.
  - **Solder joint must be mechanically strong FIRST** — hook/wrap deadbug wires before soldering
    so the joint holds, never the glue.
  - **Localized strain relief per joint (not a shared blob):** adhesive-lined **heat-shrink** over
    each joint (top pick — slit off to rework); or a small **UV-cure resin** bead (cures hard on
    demand, chip/cut off to rework — the "solid but I can still cut the wire" behavior); or
    **silicone/RTV**. Keep each joint individually accessible.
  - **Conformal coat** finished boards (removable with solvent/heat). **Avoid two-part epoxy**
    except truly permanent structural mounts (e.g. mast base). Selective silicone potting ONLY
    on proven, finalized sections.
- **Enclosure filament: PETG or ASA, NOT PLA** (PLA softens ~55–60°C → deforms from body heat/
  sun/hot car). TPU for flexible strap parts. **High infill only at stress points/mounts —
  100% everywhere is too heavy for a wrist.** Openable (screwed) shell for service.
- **Aesthetic:** hybrid — structural shell with intentional windows/cutouts showing select
  internals + status LEDs. Antennas (CC1101, NFC, LF RFID, GPS) routed *outside* any metal.

---

## 8. Thermal

Real issue: strapped to skin, sealed, hot glue insulating. **Much easier than the old Pi plan
— the P4 runs far cooler than a Linux Pi.**
- The P4 under load + regulators + radios still generate some heat against the forearm.
- Hot glue softens at 60–70°C and traps heat. **Do not encase heat sources in glue.**
- Give the P4 and regulators an air gap / vented window / small copper spreader to the
  metal case or a shell heatsink surface.
- Consider an NTC thermistor on the P4/battery read by the S3, with a thermal-cutoff that
  throttles/sleeps heavy peripherals.

---

## 9. Firmware & software

Both brains run ESP-IDF/FreeRTOS (Arduino-ESP32 also fine) — one toolchain, no Linux.

**Watch brain (ESP32-S3):**
- UI via **LVGL** (round-display support). Watch faces + touch, provides WiFi/BLE for the
  system, commands the **Chameleon Ultra over BLE**, power/thermal management, link to P4.
- Power discipline: aggressive light-sleep, wake-on-wrist-raise (IMU).
- Start firmware **now on the 1.69" dev board you already have** — same ESP32-S3, same
  IMU/touch APIs; code ports to the final board with minimal change.

**Workhorse brain (ESP32-P4):**
- Drives the console display (LVGL over MIPI-DSI) and the **signal-browser UI** (planes/ships/
  ISM sensors/pagers/AQI). Hosts the sensor + listening-module army, runs the on-MCU decoders
  (rtl_433_ESP, dump5892, dAISy NMEA parse, OpenPager), audio recording + SPL, light vision.
- Links to the S3 over SPI/SDIO + UART (ESP-Hosted-style RPC).

**Data:** log sensor/audio/decoded-signal data to microSD (on the P4).

---

## 10. Concerns & risks (ranked)

1. **Power** — every radio is a current spike; deadbug wiring resistance worsens brownouts.
   Mitigate with clean rails (Pololu buck-boosts), bulk capacitance near spiky loads, load
   switches on heavy peripherals. (Much easier now — no ~1W Pi sink.)
2. **LiPo safety** — puncture/fire against skin is the worst-case. Hard bay, protection
   board, fuse, no flex on cell, thermal watch. Highest-severity risk in the build.
3. **Thermal** — see §8. Don't entomb heat sources in hot glue; vent the P4/regulators.
4. **Mechanical fatigue** — flexing forearm cracks rigid joints. Rigid spine + flex straps.
5. **RF interference & grounding** — 5 radios inches apart with hand-soldered grounds will
   desense each other. Plan antenna placement; keep antennas outside metal; short ground
   returns; separate the sub-GHz/NFC/GPS antennas spatially.
6. **Pin budget** — solved by the P4 (§5.1); the S3 stays pin-light. Verify board breakouts.
7. **Weight/length** — forearm brick gets heavy fast; 5000mAh (not 10000) helps. Distribute
   mass along the spine.
8. **Metal case as Faraday cage** — keep RF antennas outside the CNC case; check WiFi/BT
   range after assembly.

---

## 11. Safety & legal

- Sub-GHz TX, RFID/NFC clone/emulate, and IR have real legal lines. **Personal /
  authorized / educational use only.** Don't transmit on bands or clone credentials you're
  not authorized to. This device is a learning + personal tool.
- Continuous RF TX pressed against the body all day: be intentional about duty-cycling and
  antenna placement.

---

## 12. Build phases

1. **Firmware bring-up (now):** dev on the existing 1.69" ESP32-S3 board — UI, IMU, touch,
   one radio (CC1101 or IR) on a breadboard. Prove the LVGL watch UI + one function.
2. **Watch core + P4 link:** final 1.43" AMOLED core + ESP32-P4; bring up the S3↔P4
   SPI/SDIO+UART link and the Chameleon Ultra over BLE.
3. **Sensor spine (on P4):** shared I2C bus + TCA9548A mux, add the sensor army (VL53L5CX,
   RM3100, BMP280, APDS-9960, LTR390, AS3935, MAX30101, SEN55, gas modules, haptics).
4. **Radios + listening:** CC1101 (sub-GHz + ISM/POCSAG), nRF24, IR, GNS5892R (ADS-B),
   dAISy (AIS), GPS; test isolation/desense; build the signal-browser UI.
5. **Second display + audio:** MIPI-DSI console screen on the P4, IM69D130 mics (recording +
   dB meter), camera.
6. **Power system:** 5000mAh LiPo, AXP2101 charging, Pololu rails + LT3045, protection/fuse,
   USB-C. Validate the three power modes against §6.
7. **Mechanical:** rigid spine, battery bay, flex straps, hybrid shell with windows + LEDs.
8. **Integration & hardening:** seal, thermal test on-arm, RF range test, wear/fatigue test.

---

## 13. Open items to verify on arrival

- [ ] 1.43" AMOLED board: exact onboard ICs (IMU/RTC/PMIC), and whether mic/speaker present.
- [ ] **Pin breakout reachable with the CNC metal case on** (or case opens to route out).
- [ ] Battery connector type (JST-PH 1.25 vs 2.0) to match the LiPo.
- [ ] WiFi/BT range with the metal case (Faraday effect).
- [ ] Whether onboard speaker exists or an I2S amp + speaker must be added.
- [ ] **Measure forearm** (length behind core → elbow; comfortable width) to pick the §4.6 bar screen.
- [ ] Confirm AXP2101 charge current (and whether it's configurable higher).
- [ ] Air-quality: SEN55 module size/fan fit on the spine + duty-cycle plan.
- [ ] Confirm the watch board exposes the battery lead/pads to tap VBAT as the shared node.
- [ ] ESP32-P4 board choice: Waveshare P4 Module-DEV-KIT (clean pins) vs M5Stack Tab5 (screen+cam built in).
- [ ] Confirm S3↔P4 link transport (SPI vs SDIO) + verify P4 board GPIO breakout.

# THE WATCH — Parts / Shopping List

> Derived from **[DESIGN.md](./DESIGN.md)** — that doc is the source of truth. This is only a
> buildable shopping list of the *currently chosen* parts (superseded options are omitted).
> Where the design leaves a choice open, look for a **PICK ONE** note.
>
> Prices are rough USD, single-quantity, before shipping/tax. Where DESIGN.md gives a price it's
> used verbatim; otherwise it's a common-knowledge estimate (marked *est.*). Items flagged
> **VERIFY ON ARRIVAL** map to DESIGN.md §13 open items.
>
> Architecture reminder (§3): **dual-MCU, no Linux.** ESP32-S3 watch core + ESP32-P4 workhorse.
> The Pi is demoted to an **optional SDR-only dock** (group 13). The second display is
> **P4-driven over MIPI-DSI** (not Pi), and the SC16IS752 I2C-UART bridge is **no longer needed**
> — gas sensors ride the P4's native UARTs (§5.1).
>
> **Newer groups (from DESIGN.md §6.1/§6.1b + §7.1/§7.2):** group 10 **Power control & switching**
> (MCP23017 + per-domain load switches + RF switch), group 11 **Mast & antennas** (gooseneck,
> coax, the 4-antenna set), group 12 **Assembly consumables** (decoupling caps, connectors,
> heat-shrink, UV resin, coatings, filament).

---

## 1. Brains & display

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Waveshare ESP32-S3-Touch-AMOLED-1.43** (CNC metal case) | Waveshare | ~$45 *est.* | **Core / always-on watch brain.** 1.43" round AMOLED 466×466, ESP32-S3 (WiFi+BLE), onboard QMI8658 IMU / PCF85063 RTC / AXP2101 PMIC. **VERIFY ON ARRIVAL:** exact ICs, mic/speaker presence, pin breakout with case on, battery connector (JST-PH 1.25 vs 2.0). | — |
| 1 | **ESP32-P4 workhorse board** — **PICK ONE:** Waveshare ESP32-P4 Module-DEV-KIT *or* M5Stack Tab5 | Waveshare / M5Stack | ~$35 (DEV-KIT) / ~$135 (Tab5) *est.* | Sensor-army host + 2nd-display driver. DEV-KIT = cleanest 40-pin/HAT header for protoboard. **Tab5** bundles a 5" 1280×720 screen + 2MP camera + P4 → could BE the forearm console *and* the camera (see groups 3/5). | — |
| 1 | **Second display — bar/stretched MIPI LCD** (console, forearm) | — | ~$40–90 *est.* | **PICK ONE (measure your arm first — §4.6, VERIFY ON ARRIVAL):** Recommended ~150×55mm ~1280×400 (~6" bar); Max-flex ~210×58mm 1920×480; Conservative 4–5" 800×480. Driven by **P4 over MIPI-DSI**. *Skip/covered if you chose the M5Stack Tab5 above.* | — |
| — | **1.69" ESP32-S3 dev board** — **ALREADY OWNED, DO NOT BUY** | — | $0 | Prototype/dev unit only. Bring up firmware/UI/IMU/touch now; code ports to the 1.43" core (§9, §12). | — |

## 2. RFID & radios

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Chameleon Ultra** | Proxgrind / RRG | $100–130 | Unified LF+HF RFID read/write/clone/emulate — replaces separate NFC + LF reader. Commanded over **BLE from the S3** (no pins). Coin-sized finished module (internal coils — no external antenna). | — |
| 1 | **Ebyte E07-M1101D-TH** (CC1101, through-hole DIP) | Ebyte / AliExpress | ~$8 *est.* | Sub-GHz 315/433/868/915. SPI, 3.3V. Buy variant for target band. Antenna sits **outside** the metal case — see 433MHz whip in group 11. Also does ISM-RX + POCSAG (group 3). | — |
| 1 | **nRF24L01+** module (+ regulator/adapter) | generic / AliExpress | ~$3 *est.* | Extra 2.4GHz radio. SPI, 3.3V, 2×4 header. **Needs a 10µF cap across VCC/GND (its #1 failure mode — group 12).** Keep antenna separated from CC1101. *(For range, the +PA/LNA+SMA variant per §7.2.)* | — |
| 1 | **Adafruit IR TX/RX board** (learn/capture+replay kit) | Adafruit | ~$10 *est.* | IR transmit + receive + learn. Drive LED via transistor for range. | — |
| 1 | *(optional)* **PN532 breakout** | Adafruit / generic | ~$10 | Only if you want a soldered-in always-on HF reader independent of the Chameleon. I2C. | — |
| 1 | *(optional)* iButton / 1-Wire reader (DS9092-style) | Maxim / generic | ~$5 *est.* | 1-Wire, one pin. | — |

## 3. Radio listening modules (on-MCU decode, no Linux)

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **GNS5892R** ADS-B module (or ADSBee board) | GNS / distributor | ~$25 *est.* | 1090MHz aircraft. UART → ICAO/callsign/lat-lon/alt/speed via dump5892 on-MCU. | — |
| 1 | **dAISy FeatherWing / HAT** | Wegmatt | ~$65 *est.* | 162MHz AIS ships, self-contained. UART → NMEA (MMSI/pos/course/name). | — |
| — | **rtl_433 (ISM 433/915) + POCSAG pagers** — **NO SEPARATE BUY** | — | $0 | Both reuse the **CC1101** already bought in group 2 (rtl_433_ESP + OpenPager library). | — |

## 4. Motion / position

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **PNI RM3100 magnetometer breakout** | SparkFun / generic | ~$20 | Premium mag, I2C/SPI. Fuse with onboard QMI8658. **Mount away from speaker/haptic magnets, power, metal case;** figure-8 cal. **Do NOT put it on the bendy mast** — needs a separate short RIGID outrigger or a locked "deployed" detent (§7.1). | — |
| 1 | **SparkFun MAX-M10S GPS** (Qwiic, u-blox M10) | SparkFun | ~$40 | Standalone geotagged AQI mapping. UART+I2C. Duty-cycle (~25–50mA); antenna outside metal (active GPS patch, group 11). *(Alt: Adafruit PA1010D ~$30, built-in antenna.)* | — |

## 5. Depth / vision

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **VL53L5CX** 8×8 ToF | ST / Adafruit / SparkFun | ~$20 *est.* | Wrist-friendly depth "camera." I2C. | — |
| 1 | **MLX90640** thermal camera (32×24) | Adafruit / Pimoroni | ~$60 *est.* | Low-res thermal vision. I2C. | — |
| 1 | **Camera (MIPI-CSI)** for the P4 | — | ~$10–15 *est.* | Light vision / data capture. **NOT NEEDED if you chose the M5Stack Tab5** (2MP camera included). | — |

## 6. Air quality

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Sensirion SEN55** | Sensirion / DigiKey / Mouser | ~$45 *est.* | All-in-one: PM1/2.5/4/10 + VOC + NOx + RH + T. **5V supply**, has a fan (duty-cycle it; local bulk cap — group 12). Level-shift I2C if needed. **VERIFY ON ARRIVAL:** module size/fan fit + duty-cycle plan (§13). | — |
| 1 | **Adafruit SEN5x adapter #5964** | Adafruit | ~$3 *est.* | Breaks SEN55's fine connector out to breadboard-friendly pins. | — |
| 1 | **JST-GH cable** (for SEN55) | Adafruit / Sensirion | ~$2 *est.* | Connects SEN55 to the #5964 adapter. | — |
| 1 | *(optional)* **Sensirion SCD41** true CO2 (NDIR) | Sensirion / Adafruit / SparkFun | ~$25 *est.* | SEN55 does NOT do true CO2. I2C, self-calibrating. | — |
| 3 | **SPEC Sensors DGS2 digital modules** — one each **O3 / NO2 / CO** | SPEC Sensors / Digi-Key | $100–150 **each** | Factory-cal, onboard LMP91000 + temp comp, single-row 0.1" pins. **UART → connect direct to P4 UARTs (no SC16IS752 bridge).** **Cross-sensitivity gotcha:** the accurate ozone read is **OX (O3+NO2) − NO2 subtracted**, so treat the "O3" module as an OX sensor and always run it *with* the NO2 module. LA-essentials minimum = OX+NO2 pair; add CO for the full set. | — |
| — | ~~SC16IS752 I2C-UART bridge~~ — **NOT NEEDED / DO NOT BUY** | — | $0 | Design confirms (§5.1) gas rides the P4's native UARTs; the bridge workaround is dropped. | — |

## 7. Other sensors

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **BMP280** | Adafruit / generic | ~$5 *est.* | Barometric pressure / altitude. I2C. | — |
| 1 | **APDS-9960** | Adafruit / SparkFun | ~$7 *est.* | Gesture / proximity / color / light — wave-to-control UI. I2C. | — |
| 1 | **LTR390 UV** | Adafruit / generic | ~$5 *est.* | UV index — high value at the beach. I2C. | — |
| 1 | **AS3935 lightning detector** — **PICK ONE:** SparkFun (SPI) *or* DFRobot Gravity (I2C) | SparkFun / DFRobot | ~$25 *est.* | Storm activity + distance. **AVOID cheap CJMCU/GY clones** (mistuned antenna = dead detection). | — |
| 1 | **SparkFun MAX30101 + MAX32664** (Qwiic bio-hub) | SparkFun | ~$40 | Heart rate / SpO2, algorithms on-chip. I2C. **PICK ONE:** this **vs. ProtoCentral MAX86150 (~$35)** if you want PPG **+ ECG** instead. | — |
| 1 | **EMF audio probe** (mostly DIY): 2× pickup coil + contact plate + high-Z JFET/CMOS op-amp preamp | DIY / mouser parts | ~$10–20 *est.* | "Elektrosluch"-style — touch/point at electronics, hear their EM emissions. Reuses audio path + LT3045 rail. Differential coil + "listen mode" reject the watch's own EMI (§4.2). Probe **tip mounts on the mast**, away from watch EMI (§7.1). | — |

## 8. Audio & feedback

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Infineon IM69D130 Shield2Go** stereo mic | Infineon / Mouser | ~$21 | Single board, already **stereo** (2 matched elements), I2S via onboard ADAU7002 (2.54mm, 3.3V). Field recording + calibrated dB SPL. On the **S3** for always-on. Power from clean LT3045. *(Fallback: 2× Adafruit SPH0645 ~$7 ea.)* | — |
| 1 | **MAX98357A** I2S amp | Adafruit / generic | ~$6 *est.* | **VERIFY ON ARRIVAL:** whether the core has an onboard speaker; likely add this (§13). | — |
| 1 | Small speaker (4–8Ω) | generic | ~$3 *est.* | Pairs with the MAX98357A. | — |
| 1 | Piezo buzzer | generic | ~$1 *est.* | Simple alerts, backup to speaker. GPIO/PWM. | — |
| 1 | **DRV2605L** haptic driver + LRA | Adafruit / generic | ~$10 *est.* | Real haptic patterns. I2C. Keep its magnet away from the RM3100. Local bulk cap (group 12). | — |
| 1 | **WS2812** addressable RGB LEDs (few) | Adafruit / generic | ~$5 *est.* | Status/flair. 1 GPIO. | — |
| 2 | Tactile buttons | generic | ~$1 *est.* | Hard controls independent of touch. | — |

## 9. Power (rails, cell, protection, distribution)

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Pololu S13V30F5** (5V, 3A buck-boost) | Pololu | ~$25 *est.* | 5V rail → ESP32-P4, second screen, SEN55. Keep away from antennas. | — |
| 1 | **Pololu S13V25F3** (3.3V, 2.5A buck-boost) | Pololu | ~$20 *est.* | 3.3V rail → radios + sensors, straight off VBAT (holds 3.3V as cell sags). | — |
| 1 | **ldovr.com "LT3045-A" ultra-low-noise LDO module** | ldovr.com | $25–40 | ~0.8µV RMS RF/audio-grade. Feeds field-recording mics + RF (CC1101/NFC). No Adafruit/SparkFun breakout exists. | — |
| 1 | **1S LiPo ~5000mAh** | generic | ~$20 *est.* | Balanced weight/runtime. Hard battery bay. **VERIFY ON ARRIVAL:** match connector to the core (JST-PH 1.25 vs 2.0). | — |
| 1 | **LiPo protection board** | generic | ~$3 *est.* | Non-negotiable on a wearable cell. Order: CELL(+) → protection → fuse → split. | — |
| 1 | **Inline fuse** (+ holder) | generic | ~$2 *est.* | At battery + terminal, before the split — protects all downstream. | — |
| 1 | **USB-C breakout** — **PICK ONE:** Adafruit #4090 *or* SparkFun USB-C Horizontal | Adafruit / SparkFun | ~$5 *est.* | External enclosure jack (onboard port is buried in the case). **Must have 5.1kΩ CC pulldowns** + full data breakout. Taps S3 D−=GPIO19 / D+=GPIO20 / VBUS / GND. | — |
| 1 | **TCA9548A** I2C multiplexer | Adafruit / SparkFun | ~$7 *est.* | Fan out to 8 I2C buses; resolves address collisions. Kept even with the P4. Note the I2C back-powering gotcha when gating segments (§6.1b). | — |

> Load switches, the MCP23017 that drives them, and the RF antenna switch have moved to
> **group 10 (Power control & switching)**. Decoupling/bulk caps are in **group 12**.

## 10. Power control & switching (§6.1b power-gating + shared antenna)

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **MCP23017** I2C GPIO expander | Adafruit / generic | ~$6 *est.* | **The gate controller.** Drives all ~7 load-switch EN pins **plus the RF-switch select lines** over the 2 I2C wires — **zero dedicated GPIO.** Firmware writes the expander to bring a power domain up/down. (Also serves as the §4.5 GPIO/chip-select expander if the P4 runs short.) | — |
| ~7 | **TPS22918-class load-switch ICs** (one per power domain) | TI / generic | ~$3 *est. ea.* (~$20 total) | **One load switch per §6.1b power domain:** (1) always-on S3/RTC/IMU is **NOT switched**, then (2) air-quality cluster, (3) radio cluster, (4) nav (GPS+RM3100), (5) misc sensors, (6) audio, (7) beast-block 5V (P4+display+camera) — so ~6 switched domains + spare. EN pin + soft-start/inrush limit + reverse blocking. **5V-rail domains need the IC (a bare P-FET won't fully turn off from a 3.3V logic pin).** Gate each device on its own rail's switch. *(Breakout modules run ~$5 ea if you don't want to solder the SOT-23 IC.)* | — |
| 1 | **RF switch — PE4259-class SPDT** (or Skyworks SP3T/SP4T) | Mini-Circuits / Skyworks / generic | ~$3 *est.* (SPDT) / ~$5 (SP3T/SP4T) | **Antenna time-sharing** — routes the shared wideband RX antenna to the active radio (can't parallel radios on one antenna). GPIO/MCP23017-selected: radio powers on → its select line sets the switch. Time-shares AIS 162 + ADS-B 1090 + scanning on ONE whip; **GPS + always-on 2.4GHz BLE stay on dedicated antennas.** *(Physical antenna + optional diplexer are in **group 11** — this is the switch IC itself; listed here because the MCP23017 drives its select lines.)* | — |

## 11. Mast & antennas (§7.1 deployable mast / §7.2 antenna strategy)

**Structure — the "spar":** one reinforced, foldable gooseneck mast on the rigid spine, payload
staggered by height (GPS on top; sub-GHz + 2.4GHz spaced apart; ADS-B + AIS RX; EMF probe tip).
Connector standard: `board u.FL → u.FL→SMA pigtail → SMA bulkhead on mast → SMA antenna` (swap at
the SMA, never the fragile u.FL). **4 physical antennas is the floor.**

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Gooseneck mast stock** — SnakeClamp raw gooseneck (8–10mm OD) *or* Harfington threaded gooseneck | SnakeClamp / Harfington | ~$15 *est.* | Bend-and-hold mast, internal wire bore. Keep ~20–25cm to limit weight. Foldable/stowable flat along the forearm. | — |
| 1 | *(hybrid, optional)* **Telescoping whip** (rigid base + short gooseneck tip) | generic | ~$5 *est.* | Doubles as a strut **and** an antenna; rigid base with a short gooseneck tip. | — |
| 1 | **RG316 thin coax** (spool/lengths) | generic | ~$10 *est.* | Feedline on the **repeatedly-flexing** joints of the mast. Each antenna needs its own coax up the neck. | — |
| 1 | **RG402 hand-formable coax** | generic | ~$8 *est.* | Set-once **rigid tip** feedline. | — |
| 1 | **26–30AWG silicone (flex-rated stranded) wire** | generic | ~$8 *est.* | DC + EMF-probe leads bundled inside the gooseneck bore; flex-rated with strain relief both ends. | — |
| 4 | **u.FL → SMA pigtails** (Adafruit #851) | Adafruit | ~$4 *est. ea.* (~$16) | One per external-antenna radio (CC1101, nRF24, ADS-B, AIS/RX). Routes the antenna outside the CNC case; SMA = rugged swap point. | — |
| 4 | **SMA bulkhead connectors** | generic | ~$2 *est. ea.* (~$8) | Panel-mount swap points on the mast/enclosure. | — |
| 1 | **Active GPS patch antenna** (LNA+bias) — Uputronics ~$30 *or* 18×18mm ~$8 | Uputronics / generic | ~$8–30 | **Dedicated** (can't share — has LNA+bias). Mounts on top of the mast for sky view. | — |
| 1 | **433MHz tuned whip** (~16cm) | Mayhem / Tindie | ~$10 *est.* | **Dedicated** for the CC1101 (it transmits — passive sharing is lossy). Buy tuned 50Ω. | — |
| 1 | **2.4GHz flexible u.FL antenna** (Adafruit #2308) | Adafruit | ~$4 | For the nRF24. Tiny, no reason to merge. *(WiFi/BLE 2.4GHz can stay on the S3's onboard antenna — just route outside metal.)* | — |
| 1 | **Wideband RX whip (25–1300MHz)** | generic | ~$15 *est.* | Shared RX antenna time-shared by the **RF switch** (group 10) across AIS 162 + ADS-B 1090 + scanning — collapses 3 antennas → 1. | — |
| 1 | *(optional)* **Nooelec 1090MHz ADS-B antenna** | Nooelec | ~$10 *est.* | Better dedicated ADS-B reception than sharing the wideband whip. | — |
| 1 | *(optional)* **Chip diplexer** (Abracon / Johanson) | Abracon / Johanson | ~$5 *est.* | Only if two **different-band** radios must listen at once (share one antenna simultaneously — vs. the RF switch's time-share). | — |

> **RF switch IC** for time-sharing the wideband antenna lives in **group 10** (MCP23017 drives
> its select lines). **AIS caveat:** 162MHz wants ~46cm — wrist size caps its range regardless.

## 12. Assembly consumables (§6.1 decoupling + §7 assembly method)

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Decoupling / bulk cap assortment** | generic | ~$15 *est.* | Per §6.1 scheme: **0.1µF ceramics** at IC power pins (breakouts already include these); **10–100µF bulk** per domain/board entry; **100–470µF at the VBAT node** (absorbs pulses → no watch-core brownout); **the nRF24's 10µF** across VCC/GND (its #1 failure mode); local bulk at every spiky load (haptic, P4, backlight, SEN55 fan). *(Optional pi-filter ferrite beads for any still-noisy module — supplement only.)* | — |
| 1 | *(optional)* **Ferrite beads** (for pi-filters) | generic | ~$4 *est.* | cap→bead→cap on a noisy module's power. Skip if star-ground + decoupling + separation are solid. | — |
| 1 | **JST-GH connector kit + crimp tool** | generic / iuniker | ~$25 *est.* | **Connectors at every subsystem boundary** = the repairability backbone (rework by unplugging, rarely cut a soldered joint). Plain 0.1" headers also fine at boundaries. | — |
| 1 | **Adhesive-lined heat-shrink assortment** | generic | ~$10 *est.* | **Top-pick per-joint strain relief** (slit off to rework). Localized, not a shared blob. | — |
| 1 | **UV-cure resin + UV torch/light** | generic | ~$15 *est.* | Alternative per-joint strain relief — cures hard on demand, chip/cut off to rework ("solid but I can still cut the wire"). Torch cures it. | — |
| 1 | **Conformal coating** (solvent/heat-removable) | MG Chemicals / generic | ~$12 *est.* | Coat finished boards. **Avoid two-part epoxy** except permanent structural mounts (mast base). | — |
| 1 | **Silicone / RTV** | generic | ~$6 *est.* | Softer per-joint relief + selective potting ONLY on proven/finalized sections. | — |
| 1 | **Lacing cord + Kapton tape** | generic | ~$10 *est.* | Wire dressing / high-temp masking; keep heat sources out of hot glue (softens ~60–70°C, insulates). | — |
| 1 | **PETG or ASA filament** — **NOT PLA** | generic | ~$20 *est.* | Enclosure shell. PLA softens ~55–60°C → deforms from body heat / sun / hot car. High infill only at stress points/mounts. | — |
| 1 | **TPU filament** | generic | ~$20 *est.* | Flexible strap parts. | — |

## 13. Optional / future

| Qty | Part | Vendor | ~Price | Note | Link |
|---|---|---|---|---|---|
| 1 | **Raspberry Pi Zero 2 W** (detachable SDR dock) | Raspberry Pi | ~$15 *est.* | **Optional-only.** ONLY for open-ended wideband SDR / waterfall / NOAA APT. Powered only during a session (§3.3). Not part of the daily build. | — |
| 1 | **RTL-SDR dongle** | RTL-SDR Blog | ~$30 *est.* | USB → Pi dock. General spectrum/unknown signals only; named signals decode on-MCU. | — |
| 1 | **Proxmark3 Easy** | generic | $60–110 | Deep RFID cracking (nested attacks/sniffing). ~87mm — bench companion, NOT wrist-wearable. | — |
| 1 | **Spinning lidar** — RPLidar A1 / LD06 | Slamtec / generic | ~$100 *est.* | Full 360° sweep. Bulky, UART→Pi, high current. Beast-mode/demo only. | — |

---

## Cost roll-up

Rough single-quantity subtotals (mid-point of ranges; *est.* items are common-knowledge estimates).

| Group | Core-build subtotal (~USD) |
|---|---|
| 1. Brains & display (DEV-KIT + bar LCD; 1.69" owned) | ~$150 |
| 2. RFID & radios (Chameleon + CC1101 + nRF + IR; PN532/iButton optional) | ~$135 |
| 3. Radio listening (GNS5892R + dAISy) | ~$90 |
| 4. Motion / position (RM3100 + MAX-M10S) | ~$60 |
| 5. Depth / vision (VL53L5CX + MLX90640 + CSI cam) | ~$95 |
| 6. Air quality (SEN55 + adapter/cable + 3× DGS2 @ ~$125) | ~$425 |
| 7. Other sensors (BMP280, APDS-9960, LTR390, AS3935, MAX30101, EMF probe) | ~$97 |
| 8. Audio & feedback (mic, amp+spkr, piezo, haptic, LEDs, buttons) | ~$47 |
| 9. Power (Pololu ×2, LT3045, LiPo, protection, fuse, USB-C, TCA9548A) | ~$107 |
| 10. Power control & switching (MCP23017 + ~7 load switches + RF switch) | ~$29 |
| 11. Mast & antennas (gooseneck, coax, wire, 4× pigtail/bulkhead, 4 antennas) | ~$110 |
| 12. Assembly consumables (caps, connectors, heat-shrink, UV resin, coatings, filament) | ~$130 |
| **Core subtotal** | **~$1,475** |

**Notes on the range:**
- The **3× SPEC DGS2 gas modules dominate** (~$300–450 of the total). Cutting to the **OX+NO2 pair**
  (LA-ozone essentials, drop CO) saves ~$125 and bulk → core build closer to **~$1,350**.
- Choosing the **M5Stack Tab5 (~$135)** for group 1 replaces the separate bar LCD *and* the CSI
  camera, roughly a wash on cost but fewer parts.
- Group 11 uses the cheap **18×18mm GPS patch (~$8)**; the Uputronics active patch (~$30) adds ~$22.
  Group 11/10 optionals (Nooelec 1090 antenna, chip diplexer, telescoping whip) add ~$20.
- Optional group 13 (Pi Zero 2 W + RTL-SDR + Proxmark3 + spinning lidar) adds **~$225–260**.

**Estimated grand total:**
- **Core build (no group 13 optionals): ~$1,350 – $1,650 USD**
- **With optionals (SDR dock, Proxmark3, spinning lidar): ~$1,650 – $1,950 USD**

### Verify-on-arrival items (DESIGN.md §13)
1.43" AMOLED core (exact ICs, mic/speaker, pin breakout with metal case on, battery connector
JST-PH 1.25 vs 2.0) · WiFi/BT range through the metal case · second bar-screen size (measure
forearm first) · AXP2101 charge current (and whether configurable) · SEN55 module size/fan fit +
duty-cycle plan · confirm the core exposes the battery lead/pads to tap VBAT · confirm whether an
onboard speaker exists or the I2S amp + speaker must be added.

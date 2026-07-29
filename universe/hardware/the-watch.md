---
id: emgor.hardware.the-watch
title: THE WATCH
blurb: Forearm AMOLED smartwatch fused with an RF multitool and a sensor army
parent: emgor.hardware
source: ______2026NEW/the_watch/
downloads:
  - files/the-watch-design.md
  - files/the-watch-parts.md
tags: [wearable, esp32, rf, sensors, hand-built]
updated: 2026-07-16
draft: false
---

# THE WATCH

A wrist-to-forearm device that is equal parts smartwatch, RF multitool, and wearable sensor
platform. It looks like a nice watch at the wrist — premium round AMOLED in a CNC metal case —
and reveals its hacker-exosuit nature down the forearm: exposed guts behind intentional
windows, status LEDs, antennas, a tool spine.

Hand-built. Deadbug and protoboard under a custom shell, not a fab-house PCB. That's the point.

**Status: design locked, pre-build (2026-07-16).**

## Architecture — dual MCU, no Linux

Two microcontrollers over SPI/SDIO plus a UART command side-channel. Earlier drafts used a Pi
Zero 2 W as the compute brain; the ESP32-P4 replaced it — instant-on, roughly 10× lower power,
~50 GPIO, native display and camera interfaces. The Pi is demoted to an optional dock.

**Watch brain — Waveshare ESP32-S3-Touch-AMOLED-1.43.** 1.43" round AMOLED at 466×466 with
capacitive touch, in a CNC metal case that gives the watch look and a rigid core shell for free.
Onboard QMI8658 IMU, PCF85063 RTC, AXP2101 PMIC. Runs the LVGL watch face, provides WiFi/BLE for
the whole system, and commands the Chameleon Ultra over BLE. Deliberately pin-light.

**Workhorse brain — Espressif ESP32-P4.** Dual RISC-V at ~400MHz plus a low-power core, up to
32MB PSRAM, ~50 GPIO with real UARTs, MIPI-DSI and MIPI-CSI. No wireless — the S3 handles that.
It hosts the sensor army directly: the I2C bus behind a TCA9548A mux, the UART gas sensors with
no bridge IC, SPI radios, I2S mics, GPS, SD, and the second bar display up the forearm.

## Radios

Flipper-equivalent, and then some. CC1101 sub-GHz (Ebyte E07-M1101D-TH, through-hole DIP,
protoboard-friendly). Chameleon Ultra for LF + HF RFID — full read/write/clone/emulate in one
coin-sized module, which collapses what would otherwise be a separate NFC chip, an LF reader,
and a T5577-writer gap. IR transmit/receive/learn. nRF24L01+ for extra 2.4GHz. BadUSB/HID and
FIDO over the S3's native USB, zero added parts.

Listening decodes onboard, no Linux and no SDR: ISM sensors at 433/915 via rtl_433_ESP on the
CC1101 already present, ADS-B aircraft via a GNS5892R, AIS ships via a dAISy board, POCSAG
pagers via OpenPager. All of it lands in a signal-browser UI on the console display. Only
open-ended wideband SDR still wants Linux, and that's the optional detachable Pi dock.

## Sensing

9-DOF motion (onboard QMI8658 fused with a PNI RM3100 magnetometer), VL53L5CX 8×8 lidar,
MLX90640 thermal camera, GPS, BMP280 altimeter, APDS-9960 gesture, LTR390 UV, AS3935 lightning,
MAX30101 + MAX32664 heart rate and SpO2, MAX30208 skin temperature, stereo IM69D130 mics doing
double duty as field recorders and a calibrated dB SPL meter, and an Elektrosluch-style EMF
audio probe for listening to electronics.

Air quality is a Sensirion SEN55 (PM1/2.5/4/10, VOC, NOx, RH, temp) plus SPEC Sensors DGS2
electrochemical modules. The honest caveat: no single consumer sensor covers real AQI. SEN55
nails wildfire PM2.5 but has no ozone — LA's signature pollutant. The accurate ozone read is
OX (O3+NO2) minus NO2, subtracted locally, which is why the gas modules come in pairs.

## Power

One 5000mAh 1S cell, two domains. The onboard AXP2101 is the sole charger and runs the watch
core. Everything else hangs off Pololu buck-boost rails — S13V30F5 for 5V, S13V25F3 for 3.3V —
with an LT3045 ultra-low-noise LDO downstream feeding the mics and RF front ends. Seven power
domains, one TPS22918-class load switch each, all enable pins driven from a single MCP23017 over
I2C. Inline fuse at the battery positive before the split, LiPo protection board on the cell.

| Mode | Draw | Runtime |
|---|---|---|
| Watch (AMOLED wake-on-raise, IMU+BLE) | ~20–40mA | ~4–7 days |
| Active (screen, radios decoding, P4 working) | ~150–300mA | ~1 day |
| Beast (P4 full tilt, 2nd screen, camera, radios hot) | ~0.5–0.9A | ~5–8 hrs |

## Mechanical

Rigid spine wrist→elbow carrying the electronics and a hard-walled battery bay; only fabric and
silicone straps flex. The forearm bends constantly, and rigid solder joints on a flexing
substrate crack within weeks — all rigid mass stays on the spine.

A foldable reinforced gooseneck mast (the "spar") carries everything that wants distance from
the noisy metal body: GPS antenna at the top for sky view, sub-GHz and 2.4GHz antennas spaced
apart, the RX whip, and the EMF probe tip. RG316 coax on the flexing joints, RG402 hand-formable
at the rigid tip. The magnetometer explicitly does *not* go on the mast — a flexing mast rotates
it relative to the watch and destroys compass heading.

Enclosure is PETG or ASA, never PLA, printed with high infill only at stress points. Assembly is
"solid but repairable": connectors at every boundary, mechanically strong solder joints first,
adhesive-lined heat-shrink per joint rather than one shared blob of hot glue. Hot glue softens
at 60–70°C and insulates — heat sources never get entombed in it.

## Risks, ranked honestly

1. **Power** — every radio is a current spike, and deadbug wiring resistance makes brownouts worse.
2. **LiPo safety** — a puncture against skin is the worst case here. Hard bay, protection, fuse.
3. **Thermal** — sealed, strapped to skin. Vent the P4 and the regulators.
4. **Mechanical fatigue** — the flexing forearm cracks rigid joints.
5. **RF desense** — five radios inches apart with hand-soldered grounds will interfere.

Core build lands around $1,350–1,650, dominated by the electrochemical gas modules. Cutting to
the OX+NO2 pair drops both cost and bulk.

**Legal:** sub-GHz TX, RFID clone/emulate, and IR have real lines. Personal, authorized, and
educational use only.

## Build phases

Firmware bring-up on the 1.69" ESP32-S3 dev board already on the bench → watch core + P4 link →
sensor spine on the P4 → radios and listening → second display and audio → power system →
mechanical → integration and hardening.

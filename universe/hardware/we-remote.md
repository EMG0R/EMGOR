---
id: emgor.hardware.we-remote
title: We-Remote
blurb: Open-source Wii remote — BLE gamepad, keyboard, and MIDI CC in one shell
parent: emgor.hardware
source: ______2026NEW/We-Remote/
downloads:
  - files/we-remote.ino
tags: [controller, esp32-s3, ble, midi, 3d-print]
updated: 2026-05-19
draft: false
---

# We-Remote

An open-source Wii remote. Three BLE modes — Gamepad, Keyboard, MIDI CC — with gyro-driven
haptic feedback, lidar, an analog joystick, and USB-C for data and charging. Everything in one
printed shell, firmware in a single `.ino`.

**Status: firmware complete, shell printed and split, in use.**

## Guts

SparkFun Thing Plus ESP32-S3. BLE 5.0 via NimBLE, with an onboard MCP73831 charger and MAX17048
fuel gauge, so the battery plugs straight in and charges off the same USB-C that programs it. A
2000mAh 103450 cell gives roughly 10–13 hours active and charges in about 3.

Sensors: MPU-6050 IMU and VL53L0X time-of-flight lidar sharing the Qwiic I2C bus (GPIO 8/9, both
breakouts have their own pull-ups). KY-023 analog thumbstick on GPIO 1/2 — powered from 3.3V,
because the ESP32-S3's ADC is not 5V tolerant. Seven tactile buttons, all INPUT_PULLUP and
active low. Four WS2812B pixels cut from a stick. A 10mm coin vibration motor.

## Haptics

```
GPIO 11 ──[1kΩ]──► 2N2222 base
                   emitter ──► GND
                   collector ──► motor (−)
                   motor (+) ──► 3.3V
                   1N4001 across the motor (cathode to +)
```

Gyro magnitude in rad/s maps to PWM with a 0.5 rad/s threshold. Harder swing, stronger buzz. The
flyback diode is not optional — motor back-EMF kills transistors.

## The B trigger

An Omron D2FC-F-7N mouse microswitch drops into a molded pocket in the rear shell. A printed
trigger pad slides into a channel above it and is captive once the shell halves close. Finger
presses pad, pad presses plunger. No lever, no linkage, nothing to break.

## Modes

Joystick click cycles Gamepad → Keyboard → MIDI → Gamepad, saves to flash, and reboots into the
new mode. The pixels flash the mode color: green gamepad, blue keyboard, purple MIDI, red blink
for battery under 15%. Otherwise they run a slow lava-lamp drift between light blue and pink
that brightens with gyro movement.

**MIDI CC** puts everything on channel 11 using undefined CCs to avoid conflicts — joystick XY
on 20/21, accel on 22–24, gyro on 25–27, lidar on 28, and the seven buttons on 29–36 sending 127
on press and 0 on release. Sensor CCs are continuous and smoothed (accel α 0.15, gyro 0.3, lidar
0.2). Raw NimBLE characteristics on the standard BLE MIDI service UUID, so any DAW that speaks
BLE MIDI just sees it.

**Keyboard** mode is mapped for held-sideways web games: WASD on the stick, space to jump, shift
on the trigger.

Hold 1 + 2 for three seconds to sleep — pixels fade, descending buzz, ~20µA. Joystick click
wakes it.

## Field notes

- **Check JST polarity before plugging in a battery.** JST-PH connectors get wired either way
  depending on the supplier. Use Adafruit or SparkFun cells, which are pre-tested.
- A cell that arrives fully dead at 0V is below the onboard charger's minimum threshold and will
  not charge. Trickle it externally at 0.5A to ~3.0V and the onboard charger takes over.
- Counterfeit VL53L0X V2 modules answer at 0x29 but return 0xFF on register reads — they respond
  to the I2C address and have no real silicon behind it.
- No serial output? Tools → USB CDC On Boot → Enabled.

The power switch (SS12D00G SPDT, center bottom edge) cuts the battery line entirely — OFF means
off, not standby.

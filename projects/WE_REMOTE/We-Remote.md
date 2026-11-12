# WE REMOTE

Open-source Wii remote. Three BLE modes: Gamepad, Keyboard, MIDI CC. Gyro haptic feedback, LiDAR, analog joystick. USB-C data + charging.

---

## Parts

| Part | Qty | Link |
|------|-----|------|
| SparkFun Thing Plus ESP32-S3 | 1 | [SparkFun](https://www.sparkfun.com/products/20287) (recommended) or [Amazon](https://www.amazon.com/SparkFun-Thing-Plus-Development-Dimensions/dp/B0D1BB79SM) |
| 3.7V 2000mAh LiPo 103450 (JST-PH 2.0) | 1 | [Amazon](https://a.co/d/0455KYNb)|
| MPU-6050 GY-521 IMU | 1 | [Amazon](https://www.amazon.com/HiLetgo-MPU-6050-Accelerometer-Gyroscope-Converter/dp/B078SS8NQV) |
| VL53L0X ToF LiDAR | 1 | [Amazon](https://www.amazon.com/AITRIP-VL53L0X-Breakout-GY-VL53L0XV2-Distance/dp/B0B9GP4BZM) |
| Analog Thumb Joystick (KY-023) | 1 | [Amazon](https://a.co/d/0i8iaHig) |
| Tactile Buttons | 7 | [Amazon](https://a.co/d/02QhDdu3) |
| NeoPixel Stick 8x WS2812B (cut to 4) | 1 | [Amazon](https://a.co/d/0ekp2enC) |
| Coin Vibration Motor 10mm 3V | 1 | [Amazon](https://www.amazon.com/Vibration-Button-Type-Vibrating-Household-Appliances/dp/B0BVMJ962F) |
| 2N2222 NPN Transistor | 1 | [Amazon](https://a.co/d/06Mk7mfO) |
| 1N4001 Flyback Diode | 1 | [Amazon](https://a.co/d/0ektysLr) |
| 1K Resistor (transistor base) | 1 | [Amazon](https://a.co/d/0bfYrMGL) |
| USB-C Extension Cable 6in | 1 | [Amazon](https://a.co/d/013GzJk6) |
| Chanzon SS12D00G SPDT Mini Slide Switch (70pcs) | 1 | [Amazon](https://a.co/d/0eQjUerw) |
| 3D Print Enclosure | 1 | Custom |

---

## MCU + Power

SparkFun Thing Plus ESP32-S3. BLE 5.0 via NimBLE. Built-in MCP73831 LiPo charger + MAX17048 fuel gauge. USB-C in = charges battery + data. Battery JST plugs directly into board. 2000mAh 103450 (10x34x50mm), ~10-13 hrs active, ~3 hr charge.

**Deep sleep:** hold 1 + 2 for 3 seconds. Pixels fade out, descending buzz, then ~20µA sleep. Wake by pressing joystick click.

### Battery Notes
- **Check polarity before plugging the battery into the microcontroller.** JST-PH connectors can be wired either way depending on the supplier — confirm red = + and black = − matches the SparkFun board's JST pads before connecting
- **Use Adafruit or SparkFun batteries only** — they pre-test and ship with the correct polarity
- The battery may arrive fully dead (0V) and won't charge on the onboard charger. In that case, use an external trickle charger at 0.5A until it reaches ~3.0V, then the onboard charger takes over

---

## Pin Assignments

All pins match the `#define` block at the top of `we-remote.ino`.

### Analog Inputs

| ESP32-S3 Pin | Code Define | Connection | Notes |
|--------------|-------------|------------|-------|
| GPIO 1 | `PIN_JOY_X` | KY-023 VRx | Joystick X axis. Power joystick from 3.3V (not 5V) |
| GPIO 2 | `PIN_JOY_Y` | KY-023 VRy | Joystick Y axis. Power joystick from 3.3V (not 5V) |

### Digital Inputs (all INPUT_PULLUP, active low)

| ESP32-S3 Pin | Code Define | Connection | Notes |
|--------------|-------------|------------|-------|
| GPIO 16 | `PIN_JOY_BTN` | KY-023 SW | Joystick click. Wire SW to GPIO 16, other leg to GND. Also wakes from deep sleep. |
| GPIO 4 | `PIN_BTN_A` | Tactile button | A button (center face) |
| GPIO 5 | `PIN_BTN_MINUS` | Tactile button | - button (left of A) |
| GPIO 6 | `PIN_BTN_PLUS` | Tactile button | + button (right of A) |
| GPIO 7 | `PIN_BTN_1` | Tactile button | 1 button (bottom left) |
| GPIO 14 | `PIN_BTN_2` | Tactile button | 2 button (bottom right) |
| GPIO 15 | `PIN_BTN_B` | Omron D2FC-F-7N | B trigger (rear). COM to GPIO 15, NO to GND |
| GPIO 10 | `PIN_BTN_PWR` | Tactile button | Power/Pair (top right) |

### Digital Outputs

| ESP32-S3 Pin | Code Define | Connection | Notes |
|--------------|-------------|------------|-------|
| GPIO 11 | `PIN_MOTOR` | 1K resistor → 2N2222 base | Haptic motor PWM. See haptic circuit below |
| GPIO 12 | `PIN_NEOPIXEL` | WS2812B DIN | NeoPixel data in. Cut stick to 4 LEDs |

### I2C Bus (SparkFun Thing Plus ESP32-S3 Qwiic pins)

| ESP32-S3 Pin | Connection | Notes |
|--------------|------------|-------|
| SDA (GPIO 8) | MPU-6050 SDA + VL53L0X SDA | Shared bus — use Qwiic splitter or wire to GPIO 8 |
| SCL (GPIO 9) | MPU-6050 SCL + VL53L0X SCL | Shared bus — use Qwiic splitter or wire to GPIO 9 |

SparkFun Thing Plus ESP32-S3 default I2C is GPIO 8 (SDA) / GPIO 9 (SCL) via the Qwiic connector. Both breakouts have onboard pull-ups. Two devices sharing the bus is fine.

### Power Connections

| Source | Destination | Notes |
|--------|-------------|-------|
| 3.3V | KY-023 +5V pin | Must use 3.3V, ESP32-S3 ADC is not 5V tolerant |
| 3.3V | MPU-6050 VCC | |
| 3.3V | VL53L0X VIN | |
| 3.3V | WS2812B VCC | Fine at 4 pixels, ~24mA max |
| 3.3V | Haptic motor VCC | Via transistor circuit, motor rated 3V |
| GND | All module GND pins | Common ground for everything |
| JST-PH | SparkFun battery connector | LiPo plugs directly in, charges via USB-C |

---

## Haptic Circuit

```
GPIO 11 ──[1K resistor]──► 2N2222 Base
                            2N2222 Emitter ──► GND
                            2N2222 Collector ──► Motor (-)
                            Motor (+) ──► 3.3V
                            1N4001 cathode ──► Motor (+)
                            1N4001 anode ──► Motor (-)
```

Flyback diode protects transistor from motor back-EMF. Gyro magnitude (rad/s) maps to PWM: threshold at 0.5 rad/s, scales up from there. Harder swing = stronger buzz.

---

## Physical Layout

Held vertically like a Wii remote:
- **Top:** XY joystick (left), Power/Pair button (right), LiDAR + 4 NeoPixels at very top edge
- **Middle:** A button (center), - button (left of A), + button (right of A)
- **Bottom:** 1 button (left), 2 button (right)
- **Rear:** B trigger (Omron micro switch in molded pocket, trigger pad in channel, index finger)
- **Bottom:** Power slide switch (SS12D00G), center bottom edge
- **Internal:** SparkFun board + MPU-6050 stacked, coin motor, LiPo, USB-C port at bottom

---

## Power Switch

Chanzon SS12D00G SPDT mini slide switch (8.7×3.7×11.7mm, 3-pin 2.54mm pitch). Mounted center bottom edge of enclosure. Cuts battery power line to SparkFun board. Wire: battery (+) → switch COM pin → SparkFun VBAT+. OFF position disconnects battery entirely.

---

## B Trigger

Omron D2FC-F-7N mouse micro switch. Drops into molded pocket in rear shell. 3D-printed trigger pad slides into channel above it, captive when shell halves close. Finger presses pad, pad presses plunger. No lever mechanism. Wire COM to GPIO 15, NO (normally open) to GND. Code uses INPUT_PULLUP so the pin reads HIGH when released, LOW when pressed.

---

## BLE Libraries

| Mode | Library |
|------|---------|
| Gamepad | [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad) |
| Keyboard | [HijelHID_BLEKeyboard](https://github.com/Hijel/HijelHID_BLEKeyboard) |
| MIDI | Implemented directly via NimBLE-Arduino — no external MIDI library needed |

**HijelHID_BLEKeyboard** uses NimBLE-Arduino 2.3.8+. All three modes share the same NimBLE stack.

**MIDI** uses raw NimBLE characteristics with the standard BLE MIDI service UUID (`03B80E5A-EDE8-4B33-A751-6CE34EC4C700`). Compatible with any DAW or app that supports BLE MIDI.

Additional libraries: Adafruit_MPU6050, Adafruit_NeoPixel, VL53L0X (Pololu), SparkFun_MAX1704x.

---

## Mode Switching

**Joystick click** cycles Gamepad → Keyboard → MIDI → Gamepad. Saves to flash, reboots into new mode. NeoPixels flash the mode color on boot and on switch:
- Green = Gamepad
- Blue = Keyboard
- Purple = MIDI
- Red blink = low battery (<15%)

---

## NeoPixel Behavior

4 pixels run a slow lava lamp animation drifting between light blue and pink. Brightness increases with gyro movement (shake harder = brighter). On mode switch or boot, pixels flash the mode color then fade back to lava lamp.

---

## Sleep

Hold **1 + 2** for 3 seconds. Pixels fade out, descending haptic buzz, then deep sleep (~20µA draw). Press **joystick click** to wake — firmware reboots fresh.

---

## Button Map

**Gamepad** — standard Wii remote output. Joystick=left stick, tilt=right stick, all buttons mapped as gamepad buttons 1-8.

**Keyboard** — held sideways for web games:

| Button | Key | Purpose |
|--------|-----|---------|
| Joystick | WASD | Movement |
| A | Space | Jump |
| B trigger | Shift | Run/sprint |
| 1 | E | Interact |
| 2 | Q | Ability |
| - | Tab | Inventory |
| + | Enter | Confirm/chat |
| Power | Escape | Menu/pause |
| Joy click | R | Reload |

**MIDI CC** — all on channel 11, all undefined CCs to avoid conflicts:

| Input | CC |
|-------|-----|
| Joystick X | CC20 |
| Joystick Y | CC21 |
| Accel X | CC22 |
| Accel Y | CC23 |
| Accel Z | CC24 |
| Gyro X | CC25 |
| Gyro Y | CC26 |
| Gyro Z | CC27 |
| LiDAR | CC28 |
| A | CC29 |
| B trigger | CC30 |
| 1 | CC31 |
| 2 | CC32 |
| - | CC33 |
| + | CC34 |
| Joy click | CC35 |
| Power | CC36 |

Buttons send 127 on press, 0 on release. Sensor CCs are continuous smoothed values.

---

## Troubleshooting

### No serial output
- Arduino IDE: Tools → USB CDC On Boot → **Enabled**
- Open Serial Monitor before pressing reset to catch boot output

### Battery won't charge / dies immediately when unplugged
- **Symptom:** Red blink on NeoPixels, runs on USB only, no charge indication
- **Cause:** Battery arrived fully dead (0V) — below the onboard charger's minimum threshold
- **Fix:** Use an external trickle charger at 0.5A until the cell hits ~3.0V, then the onboard charger takes over
- **Before plugging in:** double-check JST polarity matches the SparkFun board (red = +, black = −)

### I2C devices not found
- Check SDA → GPIO 8, SCL → GPIO 9
- Both devices need 3.3V and GND
- VL53L0X V2 (GY-VL53L0XV2) shows up at 0x29 but may return 0xFF on register reads — counterfeit sensors respond to I2C address but have no real silicon

---

## Arduino IDE Setup

Board Manager URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

Add to **Preferences → Additional Board Manager URLs**, then install **esp32** board package via Board Manager.

**Required board settings:**

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | **Enabled** |
| Upload Mode | UART0 / Hardware CDC |
| CPU Frequency | 240MHz |

---

## Firmware

Single file: `we-remote.ino`. Select "ESP32S3 Dev Module" in Arduino IDE.

Pin definitions at top of file. All button pins use INPUT_PULLUP — wire each button between its GPIO pin and GND. Joystick analog pins read 0-4095 (12-bit ADC at 3.3V). Sensor smoothing: accelerometer alpha 0.15, gyro 0.3, LiDAR 0.2. Serial debug prints at 50ms intervals (115200 baud). Loop runs at ~100Hz (10ms delay).

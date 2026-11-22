# HYPERTRUMPET V2: I/O list (for approval)

Status of every row: **PROPOSED**. Nothing is wired or coded. Parts are the as-built set (`../BOM.md` "As-built parts", 2026-11-12). Pins come from `../../HYPER_CORE/docs/SEED3_PINOUT.md` (official Seed3 datasheet v2.1.0 + libDaisy). Roles are proposals, not decisions.

Pin policy: same pin = same function on the trumpet and the hat wherever both have the part (joystick, NeoTrellis I2C, strip data), so the firmware can share drivers.

Columns: Seed pin = header position, Dxx/Axx name, MCU pin. Range/res = electrical range and resolution as read in firmware. Rate = how often it is read or written (proposed).

## 1. Audio

| # | Name | Part (as-built) | Type | Seed pin | Range / resolution | Rate | Proposed default role | Status |
|---|---|---|---|---|---|---|---|---|
| A1 | Bell mic in | Dynamic clip-on mic -> 10k pot (level) -> line in | audio, analog | pin 16 Audio In L (codec TAC5242) | +-1.8 V abs max, about 1 Vrms full scale, AC-coupled; 24-bit @ codec. Dynamic mic is only a few mV to tens of mV, expect -40 to -60 dBFS: **no preamp, noise floor is the risk** | 48 kHz, block 64 | Main instrument input | PROPOSED |
| A2 | Line in R | none | audio | pin 17 Audio In R | same | 48 kHz | Unused (reserve: 2nd mic or return) | PROPOSED |
| A3 | Line out L | 3.5 mm panel jack (BLLNDX, unswitched) and TX-L | audio | pin 18 Audio Out L | 0 dBFS = 1 Vrms | 48 kHz | Processed left | PROPOSED |
| A4 | Line out R | 3.5 mm panel jack and TX-R | audio | pin 19 Audio Out R | 0 dBFS = 1 Vrms | 48 kHz | Processed right | PROPOSED |
| A5 | Wired stereo jack | BLLNDX 3.5 mm stereo panel jack | audio | A3 + A4 in parallel | as above | - | Wired out; stays connected while TX run | PROPOSED |
| A6 | Wireless TX left | Xvive A58 TX (one of the stereo pair) | audio | 3.5 mm to dual 6.35 mm Y adapter from the jack, L leg | line level into a guitar-input TX, **level vs instrument input unverified** | 48 kHz | Left of stereo wireless pair | PROPOSED |
| A7 | Wireless TX right | Xvive A58 TX | audio | Y adapter, R leg | as above | 48 kHz | Right of pair | PROPOSED |
| A8 | TX power | Xvive battery pads fed from BMS output | power | none (no Seed pin) | 3.6-4.2 V cell via BMS, TX draw about 0.75 W each (estimate) | - | Powers TX from the instrument battery | PROPOSED |

## 2. Analog sensors (Seed ADC, 0 to +3V3A, 16-bit, ADC-wide oversampling; none of these pins is 5 V tolerant where marked *)

All sensors powered from +3V3A (pin 21) or +3V3D, return to the sensor ground branch (DGND), 100 nF at each sensor.

| # | Name | Part | Type | Seed pin | Range / resolution | Rate | Proposed default role | Status |
|---|---|---|---|---|---|---|---|---|
| S1 | Valve 1 Hall | 49E linear Hall + 10x3 mm magnet in cap | analog | A0 / D15, pin 22, PC0 | 0-3.3 V ratiometric, 16-bit raw, about 10-12 bit useful; per-valve calibrated | 750 Hz (block rate) | Valve 1 depth | PROPOSED |
| S2 | Valve 2 Hall | 49E | analog | A1 / D16, pin 23, PA3 | same | 750 Hz | Valve 2 depth | PROPOSED |
| S3 | Valve 3 Hall | 49E | analog | A2 / D17, pin 24, PB1 * | same | 750 Hz | Valve 3 depth | PROPOSED |
| S4 | Fader 1 | uxcell 45 mm B10K slide | analog | A3 / D18, pin 25, PA7 * | 0-3.3 V, 16-bit | 100 Hz | Dry/wet or master mix | PROPOSED |
| S5 | Fader 2 | B10K slide | analog | A4 / D19, pin 26, PA6 | same | 100 Hz | FX slot level A | PROPOSED |
| S6 | Fader 3 | B10K slide | analog | A5 / D20, pin 27, PC1 | same | 100 Hz | FX slot level B | PROPOSED |
| S7 | Pot 1 | rotary pot (owned) | analog | A6 / D21, pin 28, PC4 * | same | 100 Hz | Macro 1 (time / space) | PROPOSED |
| S8 | Pot 2 | rotary pot | analog | A7 / D22, pin 29, PA5 * | same (also DAC OUT 2, unused) | 100 Hz | Macro 2 (tone / interval) | PROPOSED |
| S9 | FSR | FSR402, 330 ohm pulldown (PMOR value, retune) | analog | A8 / D23, pin 30, PA4 * | 0-3.3 V, cubed curve (`fsr_scale.h`) | 750 Hz | Aftertouch: effect intensity / freeze | PROPOSED |
| S10 | Joystick X | KY-023 | analog | A9 / D24, pin 31, PA1 | 0-3.3 V, centre-learned, deadzone | 750 Hz | Stereo width / pitch bend | PROPOSED |
| S11 | Joystick Y | KY-023 | analog | A10 / D25, pin 32, PA0 | same | 750 Hz | Brightness / filter | PROPOSED |

## 3. Digital inputs and I2C

| # | Name | Part | Type | Seed pin | Range / resolution | Rate | Proposed default role | Status |
|---|---|---|---|---|---|---|---|---|
| D1 | Encoder 1 A | KY-040 | digital in, pull-up | D0, pin 1, PB12 | 4 edges/detent (`hg_enc.h`) | 750 Hz poll | FX slot select / preset browse | PROPOSED |
| D2 | Encoder 1 B | KY-040 | digital in | D1, pin 2, PC11 | - | 750 Hz | as above | PROPOSED |
| D3 | Encoder 1 switch | KY-040 SW | digital in | D2, pin 3, PC10 | 15 ms debounce | 750 Hz | Slot bypass toggle; **also DFU escape key (with D6)** | PROPOSED |
| D4 | Encoder 2 A | KY-040 | digital in | D3, pin 4, PC9 | 4 edges/detent | 750 Hz | Edit selected parameter | PROPOSED |
| D5 | Encoder 2 B | KY-040 | digital in | D4, pin 5, PC8 | - | 750 Hz | as above | PROPOSED |
| D6 | Encoder 2 switch | KY-040 SW | digital in | D5, pin 6, PD2 | 15 ms debounce | 750 Hz | Fine / shift | PROPOSED |
| D7 | Joystick button | KY-023 SW | digital in | D6, pin 7, PC12 | 15 ms debounce | 750 Hz | Latch / page; **DFU escape key (with D3)** | PROPOSED |
| D8 | I2C SCL | shared bus, bit-bang (PMOR `bbus`) | I2C, 3.3 V logic | D11, pin 12, PB8 | about 100 kHz | main loop | NeoTrellis + IMU + lidar | PROPOSED |
| D9 | I2C SDA | shared bus | I2C | D12, pin 13, PB9 | - | main loop | as above | PROPOSED |
| D10 | IMU | MPU6050 @ 0x68 | I2C | D8/D9 bus | 16-bit, +-2 g (16384 LSB/g), 6 axis | 100 Hz | Tilt -> modulation, shake -> freeze | PROPOSED |
| D11 | Lidar | VL53L0X @ 0x29 | I2C | D8/D9 bus | mm, about 30-1200 mm usable | 33 Hz | Hand distance in front of the bell -> wah / filter | PROPOSED |
| D12 | NeoTrellis keys | Adafruit NeoTrellis 4x4 @ 0x2E (seesaw) | I2C | D8/D9 bus | 16 keys, press + release | 50 Hz poll | Slot toggles, presets (16 or 4 pages x 4) | PROPOSED |

I2C pull-ups: none on the Seed; cut breakout jumpers so the combined value is 2.2-4.7 kohm. All I2C traffic in the main loop, never the audio callback.

## 4. Outputs

| # | Name | Part | Type | Seed pin | Range / resolution | Rate | Proposed default role | Status |
|---|---|---|---|---|---|---|---|---|
| O1 | NeoTrellis LEDs | 16 RGB WS2812 on the seesaw | I2C writes | D8/D9 bus | 8 bit per channel, brightness cap about 0.45 (PMOR value) | 12.5 Hz (80 ms, frame-diff) | Slot state, level meter | PROPOSED |
| O2 | FCOB strip data | BTF FCOB WS2812B, about 157 px, via 74AHCT-class shifter | LED, SPI1 MOSI + DMA | D10, pin 11, PB5 (SPI1_MOSI) | 8 bit per channel, firmware cap about 20/255; frame about 1.9 KB, about 4.8 ms | 30-60 Hz | Envelope / pitch visual | PROPOSED |
| O3 | Strip SPI clock (reserved) | - | do not connect | D8, pin 9, PG11 (SPI1_SCK) | toggles at about 3 MHz during a frame | - | Keep the header pin unconnected | PROPOSED |
| O4 | Strip 5 V load switch | high-side MOSFET (not in the as-built list; the BOM and SHARED_POWER_KIT call for it) | digital out | D26, pin 33, PD11 | 3.3 V logic, gate drive | on/off | Cut idle draw of the strip (80-160 mA) | PROPOSED |
| O5 | Seed user LED | on-board red LED | digital out | internal (PC7) | on/off | - | Heartbeat / error code | PROPOSED |

## 5. Power, ground, USB (no GPIO)

| # | Name | Part | Type | Seed pin | Range | Proposed role | Status |
|---|---|---|---|---|---|---|---|
| P1 | Power switch | Twidec SPST toggle in battery positive | power | none | 5 A rated, 2 cells 1S2P | Master on/off | PROPOSED |
| P2 | Boost to VIN | TPS61088, set 5.0-5.1 V | power | VIN, pin 39 | VIN range 4-17 V | Seed supply; strip and TX are separate branches to the star | PROPOSED |
| P3 | AGND / DGND | star at battery negative | ground | pin 20 AGND, pin 40 DGND | DS3: AGND must be connected to DGND outside the Seed | Audio and mic shield to AGND, sensors to DGND, LED/TX/boost return to the star on their own wires | PROPOSED |
| P4 | 3V3 rails | Seed regulators | power out | pin 21 +3V3A, pin 38 +3V3D | current limit not documented, keep light | Sensor supply (Halls, pots, FSR, joystick, I2C parts) | PROPOSED |
| P5 | USB | Seed onboard USB-C, through the shell | USB (CDC + MIDI, no audio) | on-board connector (OTG_FS, PA11/PA12), not on the header | 5 V 500 mA, may stay plugged with VIN | Flash (buttonless DFU), config/web editor, MIDI; does not charge | PROPOSED |
| P6 | Battery | 2 x 21700 5000 mAh, eMagTech 1S BMS | power | none | 28.5 Wh new (about 7.1 h at 4.0 W) | - | PROPOSED |

## 6. Pins left over

Used: 11 of 12 ADC. Of D0-D14, ten are used (D0-D6, D10, D11, D12), D8 is held back and D7, D9, D13, D14 are free. D26 drives the strip switch.

| Spare | Pin | Notes |
|---|---|---|
| A11 / D28 | pin 35, PA2 | the one free ADC channel |
| D7 | pin 8, PG10 | free GPIO |
| D9 | pin 10, PB4 | free GPIO |
| D13 | pin 14, PB6 | free, USART1 TX / I2C alt |
| D14 | pin 15, PB7 | free, USART1 RX / I2C alt |
| D27 | pin 34, PG9 | digital only |
| D29, D30 | pins 36, 37, PB14, PB15 | free because the onboard USB-C is used |

Total spare: 1 analog, 7 digital. D8 stays disconnected.

## 7. Could add (one line each)

1. Battery voltage on A11/D28 through a resistor divider from the BMS output (replaces the dropped fuel gauge; read at 1 Hz).
2. Hardware MIDI out on D13 (USART1 TX) with the standard opto-free MIDI-out circuit, for driving hardware or the DEMIURGE rig.
3. Foot or breath footswitch jack on D7 (latch/looper without hands on the valves).
4. Second mic on line-in R (pin 17) for a stereo or bell-plus-room pair.
5. VL53L0X XSHUT and interrupt on D13/D14 for clean lidar reset and data-ready.

## 8. Approve / add / remove (Emory)

Mark each line. Anything not ticked stays PROPOSED.

- [ ] Pin map in sections 2-4 as listed (ADC order: Halls A0-A2, faders A3-A5, pots A6-A7, FSR A8, joystick A9-A10)
- [ ] Hall valves at 750 Hz on the first three ADC pins
- [ ] Bell mic stays direct (dynamic -> 10k pot -> line in L, no preamp) for the first noise-floor test; add a MAX4466 like the hat only if the measured floor is bad
- [ ] Both encoders as listed; DFU escape = Encoder 1 switch + joystick button held at power-on
- [ ] One shared I2C bus on D11/D12 (bit-bang), IMU 100 Hz, lidar 33 Hz
- [ ] Strip on D10 (SPI1 MOSI + DMA), D8 left unconnected, D26 drives the strip's 5 V MOSFET
- [ ] Line-in R unused for now
- [ ] Roles in the "Proposed default role" column (edit freely)
- Add: ______________________________________________
- Remove: ___________________________________________
- Change pin: _______________________________________

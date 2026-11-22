# HYPERTRUMPET V2: Daisy Seed 3 pin map

Status: plan, 2026-11-12. Nothing built or measured. Tags: **[V]** verified against a primary source, **[I]** inferred, **[U]** unverified, check on the bench. Replaces `_attic/BELA_VARIANT/docs/PINMAP_V2.md`. Same map applies to VOCAL_HAT (subset).

## 0. Sources
| Tag | Source |
|---|---|
| DS | Daisy Seed datasheet v1.0.5 (original Seed, micro-USB), read directly: https://www.electrokit.com/upload/product/41020/41020659/Daisy_Seed_datasheet_v1.0.5.pdf |
| LD | libDaisy `src/daisy_seed.h` (master): pin aliases and `BoardVersion` enum |
| FRM | PedalPCB forum thread on Seed3 (secondhand): https://forum.pedalpcb.com/threads/new-pin-for-pin-compatible-daisyseed-seed3.29897/ |
| TI | TAC5242 datasheet SLASF27A (chip, not the Seed 3 board) https://www.ti.com/product/tac5242 |
| NOT READ | The Seed 3 datasheet itself (daisy.audio/hardware/Seed/ returned 404; no Seed 3 PDF found). Every Seed 3 board-level claim below is [I] or [U]. |

## 1. Seed 3 vs Seed / Seed 2
- Codec: **TAC5242** (TI), USB-C connector, debug-connector pads [V, LD `DAISY_SEED_3` comment]. Original Seed: AK4556 (rev4), WM8731, PCM3060 (Seed 1.2 / Seed 2 DFM).
- Pin-to-pin compatible with the Seed (same 40-pin footprint, same STM32H750 pin names) [FRM, secondhand; I]. libDaisy aliases (D0-D30, A0-A11) are one table for all non-DFM boards [V, LD]. Only the Seed 2 DFM adds D31/D32 (A12/A13) [V, LD].
- Seed 3 board's analog front end (instrument-level in/out circuitry, datasheet p.19 per forum) differs from older boards [FRM]. Details [U].
- Seed 3 DFM: forum says none exists [U].

## 2. Audio
| Item | Seed (original) | Seed 3 |
|---|---|---|
| Inputs | 2 ch, AC-coupled, 3.6 Vpp (~1 Vrms); abs max -1.8..+1.8 V [V DS] | 2 ch. TAC5242 chip: 1 Vrms single-ended full scale / 2 Vrms differential [V TI, chip]. Board-level level and coupling [U]. Design to <= 1 Vrms (+/-1.4 V peak) and the old +/-1.8 V abs max [I] |
| Outputs | 2 ch line out | 2 ch. Chip: 1 Vrms SE / 2 Vrms diff full scale; whether the board does SE or diff [U] |
| Pins | Pin 16 in[0] (L), 17 in[1] (R), 18 out[0], 19 out[1]; pin 20 AGND; pin 21 +3V3A [V DS] | Same positions [I] |
| Rates | 96 kHz / 24-bit | 192 kHz / 32-bit marketed [FRM]; -120 dB noise floor claimed by Electro-Smith email, unconfirmed [U] |
| Mic bias | none | none known [U]. Bell mic needs the external preamp (BOM section 3) |

Bell mic: clip-on electret -> bias network + OPA1612 low-noise preamp (LT3045 rail) -> **Seed line in L (pin 16)**. **R (pin 17) free** (reserve: second mic or tone/feedback return). Preamp output must be <= 1 Vrms at the loudest bell SPL: set gain with the pad, check on the scope. Shield and preamp ground at GNDA/AGND (pin 20) only, see GROUNDING_PLAN Seed 3 notes.

## 3. Power pins
| Pin | Function | Note |
|---|---|---|
| 39 VIN | **+4..+17 V** abs range [V DS, original Seed] | 5 V from U3V40F5 is inside this range on the original Seed. Seed 3 VIN range [U]: confirm before first power-up (a one-line check in the Seed 3 datasheet) |
| 38 +3V3D | 3V3 digital out | Current limit [U] (not in the v1.0.5 text). Budget ~100 mA: VL53L0X, MPU6500, BME680, ADS1115, Halls, MAX17048, pull-ups, pots |
| 21 +3V3A | analog 3V3 | Audio-side supply; [U] whether usable externally. Do not load it |
| 40 DGND, 20 AGND | grounds | Two pins/nets; join point on the board [U on Seed 3] |
| USB-C | data + 5 V | Safe together with VIN on the original Seed [V DS]; Seed 3 [U] |

## 4. Pin assignment (proposed)
ADC: original Seed has **12 channels, A0-A11 = D15-D25 and D28** [V DS, LD]; 0-3.3 V [V DS table: 3V3 range]. Seed 3 [I] same.

| Signal | Seed pin | STM32 | Notes |
|---|---|---|---|
| Slider 1 | A0 / D15 | PC0 | |
| Slider 2 | A1 / D16 | PA3 | |
| Slider 3 | A2 / D17 | PB1 | |
| Pot 1 | A3 / D18 | PA7 | |
| Pot 2 | A4 / D19 | PA6 | |
| FSR (with divider) | A5 / D20 | PC1 | |
| Joystick X | A6 / D21 | PC4 | power joystick from 3V3 |
| Joystick Y | A7 / D22 | PA5 | A7/A8 are also DAC out, unused |
| spare ADC | A8..A11 | | A9-A11 are SAI2 pins (second codec only) |
| Encoder 1 A / B / btn | D1 / D2 / D3 | PC11/PC10/PC9 | D1-D4 double as SD data lines: fine, no SD card on the trumpet |
| Encoder 2 A / B / btn | D4 / D5 / D6 | PC8/PD2/PC12 | |
| Joystick button | D7 | PG10 | |
| Arcade button | D8 | PG11 | |
| I2C1 SDA / SCL | **D12 / D11** | PB9 / PB8 | I2C1 [V DS]. Alt: I2C4 on D14 SDA / D13 SCL. **Seed 3 [U]** (forum-only "pin-compatible") |
| ADS1115 ALERT/RDY (optional) | D9 | PB4 | |
| spare GPIO | D10, D13, D14, D26, D27 | | |
| avoid | D0 (USB ID), D29/D30 (USB D-/D+) | | |

All GPIO 0-3.3 V logic; 5 V tolerant except ADC-pin set A2, A3, A6, A7, A8 which are 3.3 V only [V DS]. Buttons/encoders: internal pull-ups through libDaisy [I], or 10 k external as in BOM. Encoder library: v1 used one detent = 4 counts, 127 cw / 0 ccw on CC36/37; keep in the param schema.

## 5. I2C devices (single bus, 3.3 V logic)
| Device | Addr | Role |
|---|---|---|
| NeoTrellis 4x4 | 0x2E | pads + LEDs (seesaw, ATtiny); LED power from the 5 V boost, own wire to STAR |
| MAX17048 | 0x36 | gauge |
| VL53L0X | 0x29 | lidar |
| ADS1115 | 0x48 (ADDR GND) | 3 analog Halls (valves) |
| MPU6500 | 0x68 | IMU (v1 on Wire1; now shared bus, 0x68 only, AD0 low) |
| BME680 | 0x77 (0x76 fallback) | breath pressure |

No address conflicts [I]: 0x29, 0x2E, 0x36, 0x48, 0x68, 0x77 are all distinct. The seesaw ATtiny816 NeoDriver is dropped (section 6), which also removes the "likely 0x49" question. NeoTrellis is itself a seesaw: 0x2E is its default address; other seesaw devices on the bus would need A0/A1 pads.
Pull-ups: Daisy documents none [U]; each Adafruit breakout adds 10 k, 6 devices in parallel ~ 1.7 k. **Cut pull-up jumpers on all but one or two breakouts and measure SDA/SCL to 3V3 (target 2.2-4.7 k combined).** I2C speed: 400 kHz; NeoTrellis + VL53L0X + BME680 polling must stay off the audio callback (main loop only).

Single I2C bus on v1 was split (Wire / Wire1) to keep the IMU separate. On Seed 3, I2C4 (D13/D14) can host the IMU + BME680 if one bus is too busy [I].

## 6. Hall sensors (valves)
3x SS49E, powered from 3V3 through an RC filter (1 k + 100 nF per Hall), outputs to ADS1115 A0-A2, A3 spare. ADS1115 VDD = 3.3 V, so inputs must stay inside 0..3.3 V; use PGA +/-4.096 V (inputs cannot exceed VDD + 0.3 V anyway). 860 SPS max total; 3 channels round-robin at 475 SPS ~ 150 Hz per valve, enough for valve depth [I]. Keep the ADS read in the main loop, not the audio callback.

## 7. NeoPixel strips: recommendation
Options:
1. libDaisy `dev/neopixel.h` drives NeoPixels **through a seesaw I2C module** (NeoDriver); no native WS2812 driver [V, LD src/dev listing, forum thread 3596].
2. Native WS2812 via SPI + DMA (3 SPI bits per WS bit at ~2.4 MHz) is community-reported (SPI2 / D20 used; SPI1 conflicts, SPI6 has no DMA) with no verified complete example [forum 8100, 5908; U]. Needs the DMA buffer in D2 SRAM and a 74AHCT125.
3. Drop the strips.

**Recommend 3: drop the strips on the trumpet.** The NeoTrellis (16 px) already gives all the visual feedback, saves the seesaw driver, the 74AHCT125 and ~0.3-0.7 W, and removes the LED-return-current noise path next to a mic preamp (the hyperGuitar failure). If strips return later, use option 1 (libDaisy neopixel.h + NeoDriver) rather than hand-rolled SPI. BOM rows removed accordingly. VOCAL_HAT keeps its 30 px strip for now.

## 8. Open items
- Read the real Seed 3 datasheet: VIN range, 3V3 current, audio levels/coupling, pin deltas.
- Confirm Seed 3 I2C1 on D11/D12 on the bench with a scanner before wiring.
- Confirm 3V3 current budget by measurement.
- libDaisy version with `DAISY_SEED_3` support pinned in the shared library.

# Daisy Seed 3: full pinout analysis (HYPER_CORE)

Written 2026-11-13 for HYPERTRUMPET_V2 and VOCAL_HAT. Supersedes `HYPERTRUMPET_V2/docs/PINMAP_SEED3.md` section 0-3 (that file said no Seed 3 datasheet existed; one does).

Marks: **[V]** read directly in a primary source, cited. **[I]** inferred from a primary source. **[U]** unverified, bench check.

## 0. Sources (all local copies in `docs/ref/`)

| Tag | Source | Local copy |
|---|---|---|
| DS3 | Daisy Seed3 datasheet v2.1.0 (28 Aug 2026), official, Qu-Bit Electronix DBA Daisy. https://daisy.nyc3.cdn.digitaloceanspaces.com/products/seed3/Daisy_Seed3_datasheet.pdf (link found in https://forum.pedalpcb.com/threads/new-pin-for-pin-compatible-daisyseed-seed3.29897/). daisy.audio/hardware/Seed and the electro-smith/Hardware GitHub URLs return 404 now; docs live at docs.daisy.audio | `ref/Daisy_Seed3_datasheet_v2.1.0.pdf` |
| LDm | libDaisy master `src/daisy_seed.h` / `.cpp`, fetched 2026-11-13 from https://raw.githubusercontent.com/electro-smith/libDaisy/master/src/ (commit SHA not obtainable, API redirected) | `ref/libDaisy_master_daisy_seed.{h,cpp}.txt` |
| LDv | PMOR's vendored libDaisy, `Pocket-OpGorator/DaisyExamples/libDaisy` @ 85172e2b (2024-01-08). **No Seed 3 support** (BoardVersion has only Seed, 1.1, 2 DFM) | in place, read only |
| TI | TAC5242 datasheet SLASF27A (Nov 2024), chip not board | `ref/TI_TAC5242_SLASF27A.pdf` |
| PMOR | `Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_live` (git ff86e4c), running on a real Seed 3 | in place, read only |
| MEM | `~/EMGOR_SKILLS/memory/projects/POCKET_OPGORATOR/{v2-rebuild,usb-oled-conflict-solved,buttonless-usb-flashing}.md` | |

Important: PMOR is built on the 2024 libDaisy and still works on Seed 3, because an unrecognised board falls into the `default:` branch of `ConfigureAudio`, which is the Seed 3 audio path (LDm daisy_seed.cpp:304-314 comment "Same configuration used for Seed Rev4, Seed Rev7 (1.2), and Seed3"; MEM v2-rebuild: "libDaisy v4.0.0 builds correct Seed3 firmware ... proven by flashing 2026-09-12"). Seed 3 detection is master-only (LDm daisy_seed.cpp:357-370, PH6 tied to GND; daisy_seed.h:176-182). For the hyper library pin a libDaisy that has `DAISY_SEED_3`, or keep the old one and accept the fallthrough.

## 1. The 40 pins [V DS3 p.5 pinout, p.6 Table 2; aliases LDm daisy_seed.h:210-254]

Alt functions are the STM32H750 ones listed by DS3 Table 2. Whether libDaisy's drivers can actually select each one is a separate question (section 8).

| # | Name | MCU | Alt functions (DS3 p.6) | Notes |
|---|---|---|---|---|
| 1 | D0 | PB12 | USB_HS_ID, UART5_RX, USART3_CK, TIM1_BKIN | PMOR uses it as encoder A (config.h PIN_ENC_A) [V]. USB ID unused by FS device |
| 2 | D1 | PC11 | SDMMC1_D3, USART3_RX, UART4_RX, SPI3_MISO, I2S3_SDI | |
| 3 | D2 | PC10 | SDMMC1_D2, USART3_TX, UART4_TX, SPI3_SCK, I2S3_CK | |
| 4 | D3 | PC9 | SDMMC1_D1, UART5_CTS, I2S_CKIN, MCO2 | |
| 5 | D4 | PC8 | SDMMC1_D0, UART5_RTS | |
| 6 | D5 | PD2 | SDMMC1_CMD, UART5_RX | PMOR/new PMOR: USART5 MIDI Rx |
| 7 | D6 | PC12 | SDMMC1_CK, UART5_TX, USART3_CK, SPI3_MOSI, I2S3_SDO | |
| 8 | D7 | PG10 | SPI1_NSS, I2S1_WS, HRTIM_FLT5 | |
| 9 | D8 | PG11 | SPI1_SCK, I2S1_CK, LPTIM1_IN2, HRTIM_EEV4 | |
| 10 | D9 | PB4 | SPI1_MISO, UART7_TX, SPI3_MISO, SPI6_MISO | |
| 11 | D10 | PB5 | SPI1_MOSI, UART5_RX, I2S1_SDO, SPI3_MOSI, SPI6_MOSI, I2C4_SMBA, TIM17_BKIN | **WS2812 data pin (section 6)** |
| 12 | D11 | PB8 | I2C1_SCL, UART4_RX, I2C4_SCL, TIM16_CH1, TIM4_CH3 | |
| 13 | D12 | PB9 | I2C1_SDA, UART4_TX, SPI2_NSS, I2S2_WS, I2C4_SDA, TIM17_CH1, TIM4_CH4 | |
| 14 | D13 | PB6 | USART1_TX, LPUART1_TX, UART5_TX, I2C1_SCL, I2C4_SCL, TIM16_CH1N, TIM4_CH1 | |
| 15 | D14 | PB7 | USART1_RX, LPUART1_RX, I2C1_SDA, I2C4_SDA, TIM17_CH1N, TIM4_CH2 | |
| 16 | Audio In L | - | codec | AC-coupled, +-1.8 V abs max |
| 17 | Audio In R | - | codec | |
| 18 | Audio Out L | - | codec | 0 dBFS = 1 Vrms |
| 19 | Audio Out R | - | codec | |
| 20 | AGND | - | | section 4 |
| 21 | +3V3A | - | | analog 3V3, **output only** |
| 22 | A0 / D15 | PC0 | ADC0, SAI2_FS_B | |
| 23 | A1 / D16 | PA3 | ADC1, USART2_RX, TIM2_CH4, TIM5_CH4 | |
| 24 | A2 / D17 | PB1 | ADC2, TIM1_CH3N, TIM3_CH4 | not 5 V tolerant |
| 25 | A3 / D18 | PA7 | ADC3, SPI1_MOSI, I2S1_SDO, SPI6_MOSI, TIM1_CH1N, TIM3_CH2 | not 5 V tolerant |
| 26 | A4 / D19 | PA6 | ADC4, SPI1_MISO, I2S1_SDI, SPI6_MISO, TIM1_BKIN, TIM3_CH1 | |
| 27 | A5 / D20 | PC1 | ADC5 | |
| 28 | A6 / D21 | PC4 | ADC6, I2S1_MCK | not 5 V tolerant |
| 29 | A7 / D22 | PA5 | ADC7, DAC1_OUT2, SPI1_SCK, SPI6_SCK, TIM2_CH1 | not 5 V tolerant |
| 30 | A8 / D23 | PA4 | ADC8, DAC1_OUT1, SPI1_NSS, SPI3_NSS, SPI6_NSS | not 5 V tolerant |
| 31 | A9 / D24 | PA1 | ADC9, SAI2_MCLK_B, UART4_RX, TIM2_CH2, TIM5_CH2 | |
| 32 | A10 / D25 | PA0 | ADC10, SAI2_SD_B, UART4_TX, TIM2_CH1/ETR, TIM5_CH1 | |
| 33 | D26 | PD11 | SAI2_SD_A, I2C4_SMBA, LPTIM2_IN2 | digital only (no ADC) |
| 34 | D27 | PG9 | SAI2_FS_B, USART6_RX, SPI1_MISO, I2S1_SDI | digital only |
| 35 | A11 / D28 | PA2 | ADC11, SAI2_SCK_B, USART2_TX, TIM2_CH3, TIM5_CH3 | |
| 36 | D29 | PB14 | USB_HS_D-, USART1_TX, TIM1_CH2N | external USB-C option, DS3 Fig 1.3 |
| 37 | D30 | PB15 | USB_HS_D+, USART1_RX | external USB-C option |
| 38 | +3V3D | - | | digital 3V3, output only |
| 39 | VIN | - | | 4-17 V |
| 40 | DGND | - | | |

Checks against sources: every Dxx -> port/pin above matches LDm daisy_seed.h:210-240 (D0=PB12 ... D30=PB15) [V]. A0-A11 = D15-D25 and D28 [V LDm daisy_seed.h:243-254]. 12 ADC channels, 16-bit, ADC-wide oversampling, per-pin read speed [V DS3 p.29]. D31/D32 (A12/A13) exist only on the Seed 2 DFM [V LDm daisy_seed.h:257-262]; not on Seed 3's 40 pins.

Pin alternates that are real in silicon but **not exposed by vendored libDaisy** are listed in section 8.

5 V tolerance: all GPIO are 5 V tolerant **except D17, D18, D21, D22, D23** (PB1, PA7, PC4, PA5, PA4), which are 3.3 V only [V DS3 p.4]. GPIO range 0..+3V3 operating; abs max 0..+5 V [V DS3 p.4, p.7].

## 2. Power [V unless marked]

| Item | Value | Source |
|---|---|---|
| VIN (pin 39) | **+4 V to +17 V** DC, reverse protected. Higher VIN means more regulator heat | DS3 p.4, p.7, p.10 |
| Our 5.0-5.1 V boost | inside range | |
| USB-C onboard | 5 V, default 500 mA setting, reverse protected, **may be connected while VIN powers the board** | DS3 p.10 |
| +3V3D (38), +3V3A (21) | outputs only, from the on-board regulators ("Both the Digital and Analog +3.3V pins will output the voltage from their regulators") | DS3 p.7, p.10 |
| 3V3 current limit | **not stated anywhere in DS3** [U]. Keep external 3V3 load small; budget under 100 mA combined as a bench-measured guess |
| ADC reference | 0 V = 0, 65535 = +3V3A (the +3V3A pin voltage) | DS3 p.29 |
| Recommended filtering | 100 uF-class bulk on VIN, or 3.3 ohm series R / 1 mH on 5 V USB-VBUS-style feeds. Fig 1.4 | DS3 p.12 |

Current draw of the Seed itself is not in DS3 [U]. Existing estimate 0.5 W is Emory's figure.

## 3. Codec facts

| Item | Fact | Source |
|---|---|---|
| Part | TAC5242 (TI), up to 192 kHz / 32-bit | DS3 p.1; LDm daisy_seed.h:176-182 |
| Control | **Pin/hardware controlled** (MD0-MD5 strap pins, no I2C/SPI control port). Mode is fixed by how Electrosmith strapped it on the board; codec wakes when BCLK/FSYNC run | TI SLASF27A sec 7.3.1 and 8.2.3 (steps 1-4) |
| Input PGA settable from libDaisy? | **No.** TAC5242 in hardware-control mode has no gain register path, and libDaisy master has no TAC5242 driver: the Seed 3 case falls to `default:` which only calls `codec.Init(PB11 reset)` (the AK4556 driver) and sets SAI | LDm daisy_seed.cpp:304-314; TI sec 8.2.3 |
| Input full scale | Seed pins 16/17: AC-coupled, **-1.8..+1.8 V abs max, 3.6 Vpp typ, about 1 Vrms** | DS3 p.4 footnote, p.7 Table 3 |
| Chip full scale | 1 Vrms single-ended / 2 Vrms differential | TI features list |
| Input impedance | Chip AC input impedance 5 kohm (+-20 %) per pin [V TI]. Seed 3 board's own coupling network is **not specified** [U]. The 20 kohm and 1 Mohm figures in DS3 p.19 and p.22 belong to the dev-kit OPA1652 front ends, not the bare Seed | TI sec 6 table; DS3 p.19, 22 |
| Output | 0 dBFS = 1 Vrms; dev-kit buffer shows 100 ohm out. Bare-pin source impedance [U] | DS3 p.7 Table 3, p.19 |
| Channel count | 2 in, 2 out | DS3 pinout |
| Bias / mic power | none. No mic bias on the Seed | DS3 p.18 shows bias is made externally in the dev kits |
| Default libDaisy audio | SAI1, 48 kHz, 24-bit, block 48, postgain 1.0. PMOR sets block 64 | LDm daisy_seed.cpp:255-268, 332-336; PMOR config.h `BLOCK 64`, hw/boot.cpp:171 |
| Sample rates | libDaisy exposes 48 kHz default; 96 kHz exists in libDaisy [I]; 192 kHz via libDaisy not verified [U] | |
| Noise floor | forum quote of Electrosmith email "-120 dB" is unconfirmed marketing; DS3 p.20 plots idle noise but measured with the dev-kit circuit | [U] |

Practical consequences:
- A dynamic mic through a 10k pot straight into the line-in has no gain stage. Typical dynamic output is a few mV to tens of mV at close range, so the signal will sit roughly -40 to -60 dBFS before the pot. Expect a poor noise floor and rely on digital make-up gain. Measure at bring-up step 3; if bad, add the MAX4466 (as on the hat) to the trumpet.
- Hat: MAX4466 output is a DC-biased (VCC/2) signal; the 10 uF series cap into the Seed is right because Seed input is AC coupled and would otherwise be loaded by the bias.
- No PGA means level is set only by analog front end. Keep a pad or pot ahead of the Seed.

## 4. Grounds [V DS3 p.12]

- Two ground pins: **AGND (20)** and **DGND (40)**. DS3: "These should be connected outside of the Daisy, and connected to whatever power supply ground is shared for the entire circuit. GND should always be connected prior to connecting anything to VIN." Fig 1.5 and note: "For all applications, AGND must be connected to DGND."
- This contradicts PMOR's memory note "joined once inside the Seed - never bridge externally" (MEM v2-rebuild, AGND vs DGND). The datasheet says connect them outside. Whether they are also joined on the board is **[U]**; a continuity check with the board unpowered settles it. For our star plan (GROUNDING_PLAN sec 9) the safe reading: tie AGND and DGND together at the star at battery negative, each on its own wire, as the datasheet figure shows. Do not leave AGND floating.
- Connect GND before VIN when hot-plugging cells.

## 5. What the board reserves

| Resource | Pins / nets | Header-exposed? | Source |
|---|---|---|---|
| Codec SAI1 | MCLK PE2, FS PE4, SCK PE5, SD_A (TX) PE6, SD_B (RX) PE3 | no | LDm daisy_seed.cpp:262-268 |
| Codec reset/strap | PB11 pulsed by `codec.Init(PB11)` in the default branch. Meaning on Seed 3 [U] | no | LDm :313-314 |
| Board ID strap | PH6 pulled up, tied low = Seed 3 | no | LDm :357-370 |
| QSPI flash (IS25LP064A, 8 MB) | PF8 IO0, PF9 IO1, PF7 IO2, PF6 IO3, PF10 CLK, PG6 NCS | no | LDm :257-265 (ConfigureQspi) |
| SDRAM 64 MB | FMC pins | no | DS3 p.1; LDm daisy_seed.cpp:136 |
| User LED | PC7 via 1 kohm | no | DS3 p.37; LDm daisy_seed.cpp:5-6 |
| USB onboard | USB-C -> OTG FS on PA11 (D-) / PA12 (D+), PA9 VBUS sense (PMOR drives these AF10) | no | DS3 Fig 1.0 net names USB_OTG_FS_*; PMOR usb/board_stm32h7.c:29-42 |
| USB on header | D29 PB14 USB_HS_D-, D30 PB15 USB_HS_D+ for an optional external USB-C | yes | DS3 Fig 1.3, p.6 |
| SD (if used) | D1-D6 are SDMMC1 D3,D2,D1,D0,CMD,CK | yes | DS3 p.6, p.33 |

Not used by our builds: SD card, SPI OLED. So D1-D6 are free for encoders, buttons and the like.

For the instruments: with the onboard USB-C used, D29/D30 are not needed for USB and are free GPIO [I] (DS3 says every non-power, non-audio pin is GPIO, p.13). PMOR left them alone on purpose because it planned the external breakout.

## 6. WS2812 drive: recommendation

Constraint: 2 strips total, but **one per instrument** (each Seed drives one FCOB strip, about 157 px, brightness capped about 20/255). Level shift 3.3 V -> 5 V with the owned 74AHCT part.

libDaisy has no native WS2812 driver. `dev/neopixel.h` only talks to a seesaw module over I2C (NeoTrellis style) [V LDv src/dev/neopixel.h]. PMOR drives the NeoTrellis LEDs via seesaw I2C (hw/trellis.cpp), not WS2812 direct [V].

Options:

| Method | Verdict |
|---|---|
| **SPI MOSI + DMA** | **Chosen.** CPU free during the frame, jitter-free, uses an SPI bus we do not otherwise need. libDaisy `SpiHandle::DmaTransmit` exists [V LDv per/spi.h:174] and SPI1-4 have DMA (`kNumSpiWithDma = 4`, per/spi.cpp:103) |
| TIM PWM + DMA burst | Needs custom HAL code: libDaisy has no PWM-DMA path [I]. More code, more clash risk. Only if SPI is unavailable |
| Bit-bang | Blocks the CPU about 4.7 ms per frame and breaks audio/USB timing. Rejected |
| Seesaw NeoDriver | Extra board, not owned. Rejected |

**Pin: D10 (PB5) = SPI1_MOSI**, supported pin for SPI1 MOSI in libDaisy (per/spi.cpp:731 `{DSY_GPIOB,5}, GPIO_AF5_SPI1`) [V]. Alternative MOSI: D18/PA7 (SPI1) or D6/PC12 (SPI3, per/spi.cpp:743-745) [V]. D10 is a digital-only pin, no ADC cost. Leave **D8 (PG11, SPI1_SCK) physically unconnected** (it will clock at 3 MHz while a frame is sent; keep it away from audio and sensor wires). Unused MISO/NSS: configure NSS soft, TX-only direction. Whether libDaisy tolerates an unconfigured MISO pin in `SpiHandle::Init` is **[U]**; if it insists, give it D9 and leave it unconnected.

Timing plan [I, to be proven on a scope]:
- SPI1 kernel clock = PLL2 (libDaisy sets `Spi123ClockSelection = PLL2`, system.cpp:504) and PLL2P is 25 MHz by libDaisy's own comment (system.cpp:481 "P = 8, 25MHz") [I: derived from comments, not measured].
- `BaudPrescaler::PS_8` (libDaisy default, per/spi.h:93) -> 3.125 MHz, 320 ns per SPI bit, so **4 SPI bits per WS2812 bit = 1.28 us** (spec 1.25 us +-600 ns).
- Encoding: WS bit 0 = `1000` (320 ns high), WS bit 1 = `1100` (640 ns high) or `1110` (960 ns). WS2812B T1H spec is 0.8 us +-150 ns, so 640 ns and 960 ns each miss the window by about 10 ns. Real FCOB parts normally tolerate this but **bench-test both 1100 and 1110 on the actual strip** [U].
- Frame size: 157 px x 24 bits x 4 SPI bits = 15,072 bits = **1,884 bytes**, plus at least 280 us reset low (about 110 bytes of zeros at 3.125 MHz) -> about 2.0 KB. Send time about 4.8 ms (1884*8/3.125 MHz); at 60 Hz refresh that is under 3 % bus time.
- Buffer must sit in D2 SRAM, uncached: `DMA_BUFFER_MEM_SECTION` (LDv sys/dma.h:25-40) [V]. **Never in DTCM** (PMOR memory warns DTCM at about 98 % and the engine heap lives in SDRAM; DMA cannot see DTCM).
- DMA stream: libDaisy SPI uses `DMA2_Stream3` (TX) / `DMA2_Stream2` (RX) (per/spi.cpp:321-333) and queues jobs across SPI1-4 [V]; the SAI audio DMA is on DMA1 [I], so no direct clash. Do not use another libDaisy SPI DMA user at the same time.
- Send from the main loop, never from the audio callback. Start a frame only when none is in flight (end callback sets a flag).
- Brightness cap in the encoder (about 20/255 as built). Idle draw of 157 WS2812-class ICs is 80-160 mA; the BOM already plans a high-side MOSFET on the strip's 5 V.
- Strip supply from the boost, returning to the star. 5 V strip data via 74AHCT125/74AHCT-class buffer, 330 ohm series at the strip end, per standard practice.

No clash: D10 is not used by the codec, USB, I2C, or ADC.

## 7. I2C and the PMOR gotchas

| Item | Finding |
|---|---|
| PMOR does not use hardware I2C | `hw/bbus.h` is a bit-banged open-drain I2C at about 100 kHz: **tbus** (NeoTrellis) on D11/D12 and **sbus** (MPU6050, VL53L1X) on D13/D14. Every byte pumps USB. [V PMOR hw/bbus.h:1-80, hw/hw_priv.h:25] |
| Why | NeoTrellis (seesaw, SAMD09) stretches SCL; fixed-bound waiting desynced the bus and killed USB. Fix: wait for SCL up to 5 ms while pumping USB, set `stretch_err`, run `recover()` (9 clocks + STOP) and back off 250 ms. [V MEM usb-oled-conflict-solved "FINAL KILLER"; bbus.h waitScl/recover] |
| Hardware I2C on Seed 3 | D11/D12 = I2C1 SCL/SDA (PB8/PB9); D13/D14 are also I2C1 (PB6/PB7) [V DS3 p.6]. Vendored libDaisy maps every `I2C_4` pin with AF4, which is I2C1 on these pins, not I2C4 (per/i2c.cpp ~ line 633) [I]. So **two hardware I2C buses cannot be made from D11/D12 + D13/D14 with stock libDaisy**; the second bus would have to be bit-banged (as PMOR does) or use a custom AF6 init. Hardware I2C with seesaw clock stretching on this part is untested here [U] |
| Pull-ups | none on the Seed [U per DS3 silence]. Each Adafruit breakout carries its own (about 10 kohm); cut jumpers so the combined value is 2.2-4.7 kohm |
| Recommendation | One bus on D11/D12 for NeoTrellis (0x2E), MPU6050 (0x68), VL53L0X (0x29). Use PMOR's `BBus` logic (copied, see IMPORT_LOG) behind a tiny HAL so it can be swapped for hardware I2C later. All I2C only from the main loop |

Other PMOR facts that apply:

| Gotcha | Evidence |
|---|---|
| **USB before OLED**; in PMOR the OLED boost's draw broke enumeration. Boot order: `hw.Init` -> DFU gesture -> USB init, 500 ms settle, `tusb_init`, forced disconnect/reconnect 150 ms -> wait until `tud_mounted()` (3 s, then 6 s cap) -> OLED -> buses -> ... -> audio -> NeoTrellis **last**. Hyper instruments have no OLED, but **the same ordering applies to anything with a boost or a heavy load**: the FCOB strip and NeoTrellis LEDs | PMOR hw/boot.cpp:46-185 [V] |
| `System::Delay` is fatal next to USB; use a delay that pumps `tud_task()` | PMOR hw/usb_service.cpp:24-42 [V] |
| NeoTrellis reset plus a 500 ms `System::Delay` once killed USB; use the USB-safe delay | PMOR usb_service.cpp:30-34 [V] |
| NeoTrellis seesaw module `0x0E` bytes: PIN 3, SPEED 1, BUFLEN 48, BUF write in 24-byte chunks with 2 ms between chunks; SHOW is exactly `{0x0E,0x05}`; keypad FIFO read at `0x10`. 2 ms chunk gap fixed random pixel glitches | PMOR hw/trellis.cpp:16-50 [V] |
| SPI1 IRQ disabled (`NVIC_DisableIRQ(SPI1_IRQn)`) in PMOR because the OLED used SPI1 polling and the IRQ was a suspect. **Do not copy this if SPI1 + DMA drives the WS2812 strip**: DMA completion needs the IRQ | PMOR hw/oled.cpp:62-78 [V] |
| NeoTrellis LED noise into audio on hyperGuitar: the fix was moving the ground wire; so give LEDs their own return to the star | GROUNDING_PLAN sec 1 |
| Flashing: `APP_TYPE = BOOT_SRAM` puts the app in QSPI at 0x90040000 with the Daisy bootloader; buttonless DFU uses `BootloaderMode::DAISY_INFINITE_TIMEOUT`. ROM-DFU (STM mode) cannot write QSPI ("Last page ... not writeable"). A shell `printf dfu > /dev/cu...` silently fails (no DTR) | PMOR flash.sh header, hw/dfu.cpp:14-23; MEM buttonless-usb-flashing [V] |
| Hardware DFU escape: both buttons held at power-on jump to the bootloader. Keep that on the instruments (enclosed housings) | PMOR hw/dfu.cpp:36-43 [V] |
| PMOR audio block 64, SR 48000 | PMOR config.h [V] |
| Two ADC users: `AdcChannelConfig` uses 8.5-cycle speed and default 32x oversampling | LDv per/adc.h:59, 118 [V]; PMOR hw/controls.cpp:169-174 [V] |
| FSR: 330 ohm pulldown, cubed curve, full scale at 13/16 of span, from `_fsr_scale.h` | MEM v2-rebuild [V] |
| Joystick orientation was rotated and inverted by flags | PMOR config.h [V] |
| PMOR memory-bandwidth note: any heavy main-loop work (like I2C polling) stays out of the audio ISR | MEM hardware-quirks [V] |

## 8. Pins unavailable or constrained for our builds

| Constraint | Detail |
|---|---|
| D8 | SPI1 SCK for the strip: leave unconnected |
| D29/D30 | Free GPIO because the onboard USB-C is used; if an external USB-C is ever added they become USB again |
| D22/D23 | DAC pins; fine as ADC inputs, DAC unused |
| D17, D18, D21, D22, D23 | 3.3 V only (not 5 V tolerant). Safe for 3V3-powered sensors |
| D1-D6 | SDMMC pins; fine as GPIO, no SD card on either instrument |
| D26, D27 | digital only |
| A9-A11, D26, D27 | also SAI2 pins (second codec) [V DS3 p.5]; unused |
| ADC | 12 channels share one converter; adding channels lowers each channel's rate; set per-pin speed [V DS3 p.29] |
| Alternate-function selection | libDaisy's SpiHandle/I2CHandle/UartHandle only accept the pins hard-coded in their tables (per/spi.cpp:720-800 etc.); not every datasheet alternate is reachable without custom HAL code |

## 9. Verified vs unverified summary

Verified [V]: 40-pin table, port/pin and alternates (DS3 vs libDaisy aliases agree); VIN 4-17 V; GPIO 3.3 V levels and the five non-5V-tolerant pins; USB-C 5 V coexists with VIN; codec is TAC5242 pin-controlled; no PGA in libDaisy; audio in +-1.8 V / 3.6 Vpp, out 0 dBFS = 1 Vrms; AGND must be tied to DGND outside the module; codec SAI and QSPI pins; PMOR boot order, bit-bang I2C, DFU method.

Unverified [U]: 3V3A/3V3D current limit; Seed 3 board-level input impedance and coupling; whether AGND/DGND are also joined on the module; PB11 meaning on Seed 3; 192 kHz in libDaisy; hardware I2C with the NeoTrellis; the SPI kernel clock (25 MHz) and exact WS2812 timing on the real strip; libDaisy's tolerance for an unconfigured MISO pin; the strip's real 1100 vs 1110 margin.

Inferred [I]: SPI1 DMA path and DMA1/DMA2 separation from audio; D29/D30 usable as GPIO; I2C4 mapping gotcha.

## 10. Bench checks to close the unknowns (no code needed first)

1. Unpowered continuity AGND (pin 20) to DGND (pin 40).
2. Measure +3V3A and +3V3D under the sensor load with USB only, then with VIN 5.0 V.
3. Scope MOSI at the 74AHCT output with a 1884-byte frame; adjust `1100` vs `1110`.
4. Short the line-in (input shorted) and log the idle noise floor over CDC: the reference number for the bring-up order in `HYPERTRUMPET_V2/docs/RESUME_NEXT.md`.
5. Dynamic mic -> 10k pot -> line-in: record peak level during loud bell playing.

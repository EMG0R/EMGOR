# HYPERTRUMPET V2: bring-up plan (one component per test)

Written 2026-11-15. Order from `docs/RESUME_NEXT.md`. Pins from `docs/IO_LIST.md`, cross-checked with `HYPER_CORE/docs/SEED3_PINOUT.md` and libDaisy v9.0.0 `daisy_seed.h` (cloned in `HYPER_CORE/third_party/libDaisy`).
Status of every number below marked **PROV** is a provisional threshold: set from the ADC/part spec, to be tightened after the first real measurement. The first fader run produces the first real noise number.

## 0. Rules for every test

- Tests are **cumulative**: test N keeps every earlier component wired and streaming (`TEST_CHANNELS` in `fw/tests/` lists earlier groups too), so the end state is everything working together. **Test 01 is the exception that proves the rule: it stands alone**, nothing else wired, Seed on USB only.
- Grounds (GROUNDING_PLAN, mapped to the Seed; that document is written for the Bela, the Seed mapping is `SEED3_PINOUT.md` sec 4 and IO_LIST P3): analog and digital sensors return to **DGND pin 40**, sensor supply from **+3V3A pin 21** (analog parts) or **+3V3D pin 38** (I2C and digital parts), audio shield/jack grounds to **AGND pin 20**, AGND tied to DGND outside the Seed at the star (DS3 p.12), LED / TX / boost returns on their own wire to the star, never to a Seed pin.
- 100 nF at every analog sensor, soldered at the sensor, wiper/out to DGND.
- Noise-floor recheck after every test: all earlier ADC channels stay still (hands off), rest p-p over a 1 s window (200 samples) must be <= `max(1.5 x baseline, baseline + 16)` counts, where baseline = that channel's rest p-p recorded in its own test. The web UI stores the test 01 baseline in the browser (`Save noise baseline`). From P2 on, the audio shorted-input noise floor must also be within +3 dB of the P2 value.
- If a recheck fails at step N, fix N before adding N+1. Noisy parts (LEDs, TX) come last.
- ADC facts: 16-bit, 0 = 0 V, 65535 = +3V3A (pin 21 voltage), 32x hardware oversampling, 64.5-cycle sample time per channel (high-Z sources), 200 Hz frames. 1 count = 50.4 uV. Counts p-p to "noise-free bits": 16 - log2(p-p).
- Firmware: `fw/`, `make TEST=<id>` then `./flash.sh <id>`. Web UI: `web/index.html` (README for the one command). Flash path (default): BOOT_NONE, internal flash 0x08000000 via ST ROM DFU, no Daisy bootloader; buttonless DFU re-enters ROM DFU. QSPI/BOOT_SRAM (`INTERNAL=0`) stays available for later apps over 128 KB.

## 1. Fader wiring check (loud, as requested)

Verified three ways: IO_LIST S4-S6, SEED3_PINOUT sec 1, libDaisy v9.0.0 `daisy_seed.h` (`D18 = PA7`, `D19 = PA6`, `D20 = PC1`, `A3 = D18`, `A4 = D19`, `A5 = D20`).

**No mismatch found.** Observations that are not mismatches but matter:
1. Physical pin numbers (25/26/27) exist only in the datasheet table (DS3 p.5/6) and IO_LIST; libDaisy has no physical numbers. Both agree, and they agree with the classic Seed layout (A0 = pin 22 ... A10 = pin 32).
2. A3/D18 (PA7) is one of the five 3.3 V-only pins (not 5 V tolerant). Faders are powered from 3V3A so this is safe. Never wire a 5 V fader supply.
3. `HYPER_CORE/src/hal/hyper_hw.h` defaults the hardware DFU-escape keys to **D2 and D24**. D24 is Joystick X (an analog wiper), the IO_LIST says the escape keys are Encoder 1 SW (**D2**, pin 3) and Joystick button (**D6**, pin 7). `fw/Makefile` overrides to D2/D6 with `-D`. (hyper_hw.h itself was not edited: it is not a file created this week.) Do not wire a pull-down on D2 or D6.
4. GROUNDING_PLAN.md is a Bela document. Seed equivalent used here: sensors to DGND pin 40 (task and IO_LIST P3); DS3 says AGND must be tied to DGND outside the module. The ADC reference is the +3V3A pin voltage, so a fader ratio is exact only if the fader's high end and the ADC see the same 3V3A; that is why the fader high ends go to pin 21, not pin 38.
5. dfu gesture: with D2 and D6 floating and the internal pull-ups on, nothing happens. If both are ever shorted low at power-on the Seed enters the bootloader.

| Fader | Seed ADC pin | Physical pin | MCU | High end of track | Low end of track | Wiper |
|---|---|---|---|---|---|---|
| FADER1 | A3 / D18 | 25 | PA7 | +3V3A pin 21 | DGND pin 40 | pin 25 |
| FADER2 | A4 / D19 | 26 | PA6 | +3V3A pin 21 | DGND pin 40 | pin 26 |
| FADER3 | A5 / D20 | 27 | PC1 | +3V3A pin 21 | DGND pin 40 | pin 27 |

Build the three faders' ends as two short bus wires (one to pin 21, one to pin 40) from a small local junction, not a daisy chain through the faders. 100 nF from each wiper to the DGND bus at the fader. "Bottom" of the slider = the end tied to DGND (reads 0), "top" = the end tied to 3V3A (reads 65535). Load on 3V3A: 3 x 10k across 3.3 V = 1 mA.

## 2. Tests

Each test: **Wire** (Seed physical pin, Dxx/Axx, from -> to), **Firmware measures**, **UI shows**, **PASS**, **Noise recheck**.

### P0. Power only (no Seed)
- Wire: 2 x 21700 (1S2P) -> eMagTech 1S BMS -> Twidec toggle in battery positive -> TPS61088 in. Star at battery negative: boost GND, BMS B-, later every return on its own wire. Output trimmed to 5.0-5.1 V. 100 ohm / 0.5 W dummy load.
- Measures: DMM (no firmware, no UI).
- PASS (PROV): 5.00-5.10 V no load and with the dummy load; toggle off = 0 V on output; no hot part after 5 min; ripple < 50 mV p-p if a scope is available (otherwise record "not measured").
- Recheck: none yet.

### P1. Seed alone
- Wire: USB-C only. Nothing on any header pin. Then repeat with 5.0 V on VIN pin 39 / GND pin 40 (via the star) **and** USB plugged (DS3 says allowed).
- Firmware: `TEST=01_faders` with the faders unwired is a valid P1 build (floating ADC values). Proves: CDC enumerates, 200 Hz stream, LED blinks 1 Hz, buttonless DFU round trip.
- UI: link panel (rate, dropped frames), `Enter DFU`.
- PASS: stream rate 195-205 Hz; dropped frames < 0.1 % over 10 min; board LED blinks 1 Hz; `Enter DFU` -> reflash -> reconnect works 3 times in a row with no button; +3V3A and +3V3D (pins 21, 38) read 3.30 V +-0.05 with a DMM (the datasheet gives no tolerance: PROV).
- Recheck: n/a. Record the no-load idle current from VIN.

### P2. Audio baseline (nothing else wired)
- Wire (shorted): Audio In L pin 16 and R pin 17 each to AGND pin 20 with the shield. Then (mic): dynamic mic -> 10k pot -> pin 16, pot/mic shield to AGND pin 20. Out pins 18/19 to a scope/interface or the panel jack (AGND pin 20 as the jack ground).
- Firmware: needs `TEST=P2_audio` (**not written yet**, next deliverable): audio callback at 48 kHz block 64 reporting per-channel RMS dBFS, peak, DC offset, and 1 s of FFT bins for tonal spurs; analog channels none.
- UI: dBFS meters, RMS/peak history, spur list.
- PASS (PROV): shorted input RMS < -80 dBFS (20 Hz-20 kHz), DC offset < 0.5 % FS, no spur above -90 dBFS. Mic: record noise floor with pot at the working position and peak while playing. **No pass/fail on the mic number: it is the reference every later step is compared against** (IO_LIST says no preamp until this number is bad).
- Recheck: this test sets the audio reference. If the shorted number is worse than -80 dBFS, stop and inspect grounding before sensors.

### 01. Faders (standalone, test 01)
- Wire: the table in section 1. Nothing else connected except USB.
- Firmware measures: 3 channels raw 16-bit at 200 Hz, 32x oversampling; min/max since reset; rest noise p-p over a sliding 1 s window now and best (lowest) seen; 32-bin sweep-coverage mask per channel; stream stats (seq gaps).
- UI shows: per channel big raw value, percent, volts, live bar with the 2 %/98 % marks, 32-cell coverage strip, noise p-p (now, best, in mV), min/max; rolling 10 s scope of all three; guided steps: rest, sweep F1, sweep F2, sweep F3, rest recheck; auto PASS/FAIL per check; `Copy report`.
- PASS (all must hold):
  1. Rest noise p-p, worst 1 s window over 3 s, **<= 64 counts** per fader (0.1 % FS, 3.2 mV, about 10 noise-free bits). Warn above 64, "high" above 160, "looks floating" above 2000. (PROV: first real number replaces it.)
  2. Full sweep reaches **< 2 % (< 1311) and > 98 % (> 64224)**.
  3. Monotonic: worst backtrack <= 2 % FS (1311 counts) along the sweep segment.
  4. No dropouts: largest one-sample jump <= 10000 counts (a 5 ms frame).
  5. Sweep coverage >= 30 of 32 bins of 2048 counts (move 2-4 s end to end).
  6. Crosstalk: while one fader sweeps full scale, each other fader moves <= 1 % FS (655 counts; the UI also prints it as % of the mover).
  7. Rest recheck at the end: each fader <= max(1.5 x first rest, first rest + 16) counts.
  8. Stream rate >= 150 Hz, dropped frames < 2 %.
- Noise recheck: step 7. Baseline stored in the browser for the later tests.

### 02. Pots (faders stay live)
- Wire: POT1 wiper -> A6/D21 pin 28 (PC4, 3.3 V-only pin); POT2 wiper -> A7/D22 pin 29 (PA5, 3.3 V-only pin, shares the DAC2 pad: leave DAC unused). Both track ends: +3V3A pin 21 / DGND pin 40. 100 nF at each wiper.
- Measures: same as 01 for 5 channels.
- UI: five cards, sweeps for the two new pots, crosstalk across all five.
- PASS: same numbers as 01 for the pots; faders must still pass the rest p-p (recheck) and show no movement (<= 1 % FS) while a pot sweeps.
- Recheck: faders vs their 01 baseline.

### 03. FSR (all of 02 live)
- Wire: FSR402 leg 1 -> +3V3A pin 21; leg 2 -> node N; N -> 330 ohm -> DGND pin 40; N -> A8/D23 pin 30 (PA4, 3.3 V-only, DAC1 pad, leave DAC unused). 100 nF N to DGND.
- Measures: raw, plus rest value and pressed maximum.
- UI: FSR card, press step.
- PASS (PROV): rest value < 1 % FS (< 655), rest p-p <= 64; firm press gives > 60 % FS; return to < 1.5 % FS after release (no stuck); monotonic rise on a slow press (backtrack <= 2 %); other channels unmoved (<= 1 % FS).
- Recheck: all earlier channels.

### 04. Joystick analog (all of 03 live)
- Wire: KY-023 +5V pin -> +3V3A pin 21 (module works at 3.3 V; never 5 V), GND -> DGND pin 40, VRx -> A9/D24 pin 31 (PA1), VRy -> A10/D25 pin 32 (PA0). SW not yet (test 07). 100 nF at each wiper.
- Measures: raw X/Y, centre (mean of 1 s hands off), per-axis learned min/max.
- UI: X/Y cards plus a 2D position dot with deadzone ring.
- PASS (PROV): centre within 45-55 % FS on both axes; hands-off p-p <= 64; each axis reaches < 5 % and > 95 % at the stops; centre returns within +-2 % FS after release; X sweep moves Y <= 3 % FS (a mechanical joystick couples a little, record it).
- Recheck: all earlier channels.

### 05a/b/c. Hall valve sensors, one at a time (all of 04 live)
One test per valve. Magnet 10 x 3 mm in the valve cap, valve travel about the stroke of the cap.
- Wire (49E / DRV5055, TO-92, check the datasheet face: pin 1 VCC, pin 2 GND, pin 3 OUT): VCC -> +3V3A pin 21 (never 5 V: it would clip), GND -> DGND pin 40, OUT: Valve 1 -> A0/D15 pin 22 (PC0), Valve 2 -> A1/D16 pin 23 (PA3), Valve 3 -> A2/D17 pin 24 (PB1, 3.3 V-only). 100 nF + (optional) 1 uF at the sensor.
- Measures: raw, rest value, pressed value, per-valve calibration (open/closed), crosstalk.
- UI: calibrate button (rest then press), depth bar 0-100 % of calibrated span, scope.
- PASS (PROV): calibrated span (pressed - rest) >= 4000 counts (6 % FS) with the magnet; rest p-p <= 64; monotonic press (backtrack <= 3 % of span); repeatability: 10 presses, closed value spread <= 3 % of span. After valve 3: press all three at once, each reading within 3 % of its single-press value (magnet crosstalk between valves).
- Recheck: all earlier channels; a Hall sensor on 3V3A draws a few mA, check the faders did not move.


### FW-05 (fw 0.4). Faders + 2 pots + FSR + MPU6050 + 2 x VL53L1X together (firmware `TEST=05_faders_imu_lidar`, fw 0.2)
This is a combined bench test for what is wired first: it is test 01 (faders) plus test 08 (MPU6050) and test 09 (VL53L1X) from the full order. Tests 08/09 below stay as the formal numbering; "FW-05" here is the firmware/UI id (`TEST=05_...`), not the Hall tests 05a-c.
- Wire: faders as section 1. **FSR (4th ADC channel, kind fsr)**: FSR402 leg 1 -> +3V3A pin 21; leg 2 -> node N; N -> 10 k to DGND pin 40; N -> A8/D23 pin 30 (PA4, 3.3 V-only). (Test 03 below uses 330 ohm; this bench uses 10 k, more sensitive at light force.) Lidar board = ACEIRMC TOF400C (VL53L1X): VIN -> +3V3D pin 38 (onboard regulator), GND, SDA/SCL as below, INT and SHUT unconnected (data-ready is polled over I2C). I2C bus **D11 = SCL pin 12 (PB8), D12 = SDA pin 13 (PB9)**, bit-banged open-drain at about 100 kHz (PMOR BBus logic, ported to libDaisy v9 in `fw/bbus_v9.h`: `dsy_gpio` no longer exists in v9), main loop only. Both breakouts: VCC -> +3V3D pin 38, GND -> DGND pin 40. GY-521: AD0 -> GND (0x68), INT open. VL53L1X: XSHUT open. Combined pull-up of all I2C breakouts should be 2.2 to 4.7 kohm (each breakout carries its own, about 4.7 k on the GY-521, 10 k on many VL53L1X boards).
- Firmware: boot scan (0x08-0x77), re-scan on request and whenever a device state changes; each device probed and (re)initialised independently every 2 s while missing or in error, so one missing sensor never blocks the other. MPU6050: reset, wake, DLPF 44 Hz (CONFIG 0x03), 100 Hz (SMPLRT 9), +-250 dps, +-2 g, 14-byte burst read at 100 Hz, pitch/roll computed on the Seed (centi-degrees). VL53L1X (the breakout is an L1X, confirmed by 0x010F = 0xEA, 0x0110 = 0xCC): PMOR VL53L1X_mini driver (Pololu VL53L1X lineage, MIT) with 16-bit register addressing, SHORT distance mode, 33 ms timing budget, continuous ranging every 40 ms; range status 9 = valid. Data-ready is polled at about 50 Hz, polarity-aware (GPIO_HV_MUX__CTRL 0x30 bit 4; ready when (reg 0x31 & 1) == !bit4), interrupt cleared (0x86 = 0x01) after every read, ST ULD default config blob 0x2D..0x87 written in init, SYSTEM__MODE_START 0x87 = 0x40 sent by startContinuous. If no data-ready in 200 ms the driver stops ranging and uses single-shot (0x87 = 0x10, poll, read); the UI shows which path runs and polls/ready/reg30/reg31/reg89. Streams `imu` (raw accel/gyro/temp, pitch/roll) at 100 Hz, `lid` (mm + range status) per result, and per-device ok/missing/error + WHO_AM_I + I2C error counter + scan list in the 1 Hz hello.
- UI shows: test selector (follows the firmware), I2C scan chips with names, device status and error counter, tilt horizon, accel and gyro traces, |a| in g, rest p-p, pitch/roll, temperature; lidar distance bar, trace, sigma and p-p, status code, rate, optional true-distance box.
- Guided steps and PASS (PROV numbers):
  1. I2C devices (10 s, hands off): scan contains 0x68 and 0x29; MPU6050 ok and WHO_AM_I 0x68 (0x70/0x71/0x72/0x98 = clone, accepted and reported); VL53L1X ok and model id 0xEACC; **zero new I2C errors** in 10 s and neither device left "ok".
  2. Fader rest, sweeps, rest recheck: exactly the test 01 criteria, now **with I2C traffic live**. The first rest is also compared against the saved test 01 baseline: <= max(1.5 x baseline, baseline + 16) counts. This is the noise recheck for adding the I2C parts.
  3. IMU rest (flat, still, 3 s): |a| = 1.00 +-0.05 g; each accel axis p-p <= 20 mg over a 1 s window; each gyro axis |bias| <= 5 dps and p-p <= 2 dps; read rate >= 90 Hz.
  4. IMU six faces: each of +X -X +Y -Y +Z -Z reached with the dominant axis 0.9 to 1.1 g and the other two <= 0.1 g, held 0.5 s.
  4b. FSR (step after the fader sweeps): hands off 0.5 s, then press firmly and release. Rest max < 2 % FS (< 1311); firm press peak > 60 % FS (> 39321); after release the value falls from above 10 % FS to below 2 % FS within 200 ms; other channels move <= 1 % FS. The rest steps also require FSR mean < 2 % FS and p-p <= 64 counts.
  5. Lidar fixed target (3 s): >= 95 % valid (status 9, 30 to 1200 mm); sigma <= 5 mm and p-p <= 20 mm; accuracy within +-15 mm of the typed distance (if typed); result rate >= 25 Hz.
- Also required, not auto-checked: USB stays enumerated for 10 min with both sensors streaming (fw stream rate >= 195 Hz and dropped frames < 0.1 % on the Link panel), and the audio floor (once P2 firmware exists) within 3 dB of the P2 value with lidar and IMU running.

**FW-05 additions (fw 0.4), wiring table**

| Part | Seed pin | Name / MCU | From -> to |
|---|---|---|---|
| POT1 C500K (reverse-log) | 28 | A6/D21, PC4 | wiper -> pin 28; ends -> +3V3A pin 21 and DGND pin 40; **100 nF wiper to DGND** |
| POT2 C500K | 29 | A7/D22, PA5 | wiper -> pin 29; ends -> +3V3A pin 21 and DGND pin 40; **100 nF wiper to DGND** |
| FSR402 | 30 | A8/D23, PA4 | FSR leg 1 -> pin 21; leg 2 -> node N; N -> 10 k -> DGND pin 40; N -> pin 30 |
| LIDAR1 SHUT | 8 | D7, PG10 | TOF400C SHUT -> pin 8 (free GPIO; was only a "could add" footswitch in IO_LIST); runs at 0x30 after boot |
| LIDAR2 SHUT | 10 | D9, PB4 | TOF400C SHUT -> pin 10 (free GPIO per SEED3_PINOUT/IO_LIST sec 6); stays 0x29 |
| both lidars | 12 / 13 / 38 / 40 | D11 SCL, D12 SDA, +3V3D, DGND | VIN -> +3V3D pin 38 (onboard regulators), GND -> DGND pin 40, SCL -> pin 12, SDA -> pin 13, INT unconnected |

- Pots: 500 k is a high source impedance, so firmware uses the longest ADC sample time (810.5 cycles) on A6/A7 and the 100 nF at the wiper is required; expect higher rest noise than the faders (the 64-count rest limit is provisional for pots: if it fails, record the number and set the limit from it). Default linearization = inverse C taper (85 % at mid rotation, n = 0.234); the same Calibrate card (bottom/25/50/75/top, PCHIP, 9-point LUT, Copy cal JSON) applies. 5-point fits of a steep reverse-log curve are good to about 6 % FS worst case near the bottom end (about 2.7 % for n = 0.5); capture the 25 % point carefully or ask for a finer LUT.
- Lidar address assignment at boot: both SHUT low, raise LIDAR1, write I2C_SLAVE__DEVICE_ADDRESS (0x0001) = 0x30, raise LIDAR2 (0x29), init both (short mode, 50 ms budget, continuous, polled). If LIDAR1 fails it is powered down so LIDAR2 can use 0x29; a failed unit is retried every 2 s (LIDAR2 alone by an XSHUT pulse, LIDAR1 by a full both-off sequence). Streamed as `lid` with index `i`; status 9 = valid.
- PASS additions: scan shows 0x68, 0x29 and 0x30; both lidars ok with model id 0xEACC; each lidar passes the fixed-target step (>= 95 % valid, sigma <= 5 mm, p-p <= 20 mm, rate >= 25 Hz; note the 50 ms period gives about 20 Hz, so use 15 Hz as the limit if 25 fails and record it); pots pass sweep/monotonic/coverage on the linearized value and crosstalk <= 1 % FS.

### FW-06 (`TEST=06_pots2w_enc`, fw 0.5). Faders + 2 pots wired 2-wire + 2 encoders + IMU + 2 lidars
Build: `APP_TYPE=BOOT_SRAM` (the Seed now has the Daisy bootloader at 0x08000000; a BOOT_NONE build would erase it). Flash: `cd fw && INTERNAL=0 ./flash.sh 06_pots2w_enc` (buttonless "dfu", catches the bootloader, retries the 0x90040000:leave download for about 2 s, venv python only).

| Part | Seed pin | Name / MCU | Wiring |
|---|---|---|---|
| POT1 (C500K, 2-wire) | 28 | A6/D21, PC4 | one end lug -> +3V3A pin 21; wiper -> pin 28; **47 k from pin 28 to DGND pin 40** (+100 nF wiper to DGND) |
| POT2 (C500K, 2-wire) | 29 | A7/D22, PA5 | same, wiper -> pin 29, 47 k pin 29 to pin 40 |
| ENC1 A / B / SW | 2 / 3 / 4 | D1 PC11 / D2 PC10 / D3 PC9 | common -> DGND pin 40, internal pull-ups |
| ENC2 A / B / SW | 5 / 6 / 7 | D4 PC8 / D5 PD2 / D6 PC12 | common -> DGND pin 40, internal pull-ups |
| DFU escape (both low at power-on) | 4 + 7 | D3 + D6 (the two encoder buttons) | changed from D2/D6 because D2 is now ENC1 B |

(IO_LIST section 3 still lists the older encoder pins D0-D5; PINMAP_SEED3.md and this table are the ones in use.)
- Pot math: Vout = 3.3 * 47k / (47k + Rx): about 3.3 V at Rx = 0, about 0.28 V at 500 k (raw about 65535 down to about 5600). Firmware default normalization: Rx = 47k * (65535 / raw - 1), lin = clamp(Rx / 500k, 0, 1) * 65535 (lin falls as the raw rises; "Invert direction" flips it). Raw is always streamed; the 5-point calibration card (bottom, 25, 50, 75, top) works on top of it: if the captured raws fall with travel the UI fits the reversed order and sets the firmware invert flag (`calinv`). "Reset cal" clears the firmware cal and the saved `ht_cal` in the browser. ADC sample time stays 810.5 cycles.
- Encoders: 4x Gray-code state machine polled on every main-loop pass (tens of kHz idle; an IMU/lidar transaction takes a few ms, so a very fast spin during one can drop a step: that shows up as an invalid transition), 4 counts per detent, buttons debounced 15 ms, press counter, invalid-transition counter. Streamed in the 200 Hz frame as `e` = [count, button, presses, bad] x 2.
- PASS: pots as the fader sweep checks on the normalized value (monotonic within 2 %, no jumps above 10000, coverage >= 30 of 32 bins, crosstalk <= 1 % FS); the pot raw range shows about 5600 to 65000. Each encoder: 20 detents CW then 20 CCW ends within +-1 detent of the start, exactly 5 button events for 5 presses, 0 invalid transitions. IMU and lidar steps as FW-05; all channels pass the noise recheck.

**Correction (fw 0.7): there is exactly ONE lidar.** LIDAR2 (D9 / pin 10) and the XSHUT readdressing are gone. The lidar's SHUT goes to D7 (pin 8), VIN to +3V3D pin 38, SDA/SCL to pins 13/12; it stays at 0x29. Everything above that mentions two lidars, 0x30 or D9 is superseded.

**FW-06 addition (fw 0.8): joystick (KY-023).** VRx to A9/D24, VRy to A10/D25, SW to D9 (internal pull-up, active low, 15 ms debounce), the module's +5V pin to the 3V3 analog rail, GND to DGND. Pins are named by D/A label only from here on.
- Firmware: two ADC channels, kind "joy". Centre = average of 200 ms starting 50 ms after boot (`recenter` repeats it; leave the stick alone). Radial deadzone, default 6 % (`joydz <percent>`), output -1..1 per axis with the direction kept. Auto-span: running min and max start at centre +-20000 counts and widen with the largest travel seen. Streamed in the 200 Hz frame as `j` = [x, y (per mille), button, presses], raw in `v`, normalized (centred on 32768) in `l`; hello carries `joy` {dz, centre, span, ready}.
- UI: square XY pad with the deadzone as an outline, button lamp that fills when pressed, raw numbers in small text; Recenter and the deadzone slider live in the drawer. Guided step: let go 0.5 s, circle fully, press 5 times. PASS: raw centre within 8 % of the middle and the output inside the deadzone at rest; x and y each beyond +-0.9; all 8 directions reached beyond 0.6; exactly 5 button events.
- D9 was the second lidar's SHUT in fw 0.4 to 0.6; there is one lidar now (SHUT on D7).

**FW-06 addition (fw 0.9): three Hall valve sensors (49E).** Power from the 3V3 analog rail and ground to DGND. OUT to A0/D15 (VALVE1), A1/D16 (VALVE2), A2/D17 (VALVE3), optional 100 nF from each OUT to DGND. Test 06 uses 3-wire pots (kind "pot") and the joystick button is on D10.
- Firmware: three "hall" channels (64.5-cycle sample time, 11 ADC channels in total, libDaisy allows 16). Rest = average of 200 ms starting 50 ms after boot (`valvecal rest` repeats it, valves up). Pole sign is detected from the first press beyond 1500 counts and the span follows the largest travel seen (starts at 6000 counts); `valvecal full <1|2|3>` takes the current press as full for that valve. Depth 0..1000 per mille with a 3% deadzone, streamed as `h` in the 200 Hz frame; raw in `v`, depth in `l`; hello carries `hall` {rest, sign, span, ready}.
- ADC rate: the scan runs all 11 channels with 32x oversampling. The two pots at 810.5 cycles dominate: about 73,000 ADC clocks per scan, so each channel updates roughly every 3 to 6 ms (about 160 to 330 Hz depending on the real ADC clock, about 12 to 25 MHz; not measured). The 200 Hz stream therefore repeats a value now and then. Dropping the pots to 387.5 cycles would cut the scan by about a third.
- UI: three flat piston bars with raw and offset from rest in small text, plus a live crosstalk readout (one valve down: how far the other two move, as a share of ADC full scale). Guided step: each valve alone 3 presses, then all three together 3 presses. PASS: each valve reaches 80 % depth, crosstalk onto the others at most 5 % of ADC full scale (3277 counts), and the three-together presses reach 60 %.

### FW-07 (`TEST=07_lights`, fw 1.0): test 06 plus FCOB strip, NeoTrellis and audio-reactive lights
- Strip: BTF FCOB WS2812B 5 V, 157 px. Data from D29 (PB14, which is SPI2_MISO, AF5; the SPI2 CFG2.IOSWP bit swaps the MISO and MOSI functions so the master transmits on it; hello reports `lights.strip.iosw` = 1 when the bit is set) through a 74AHCT125 and 330 ohm to DIN; strip 5 V and ground to the star, not through the Seed. No sclk or nss pin is used. libDaisy has no IOSwap option, so the pin is muxed through SpiHandle (direction TWO_LINES, PB14 as the miso pin) and IOSWP is set right after Init and before every frame. Fallback only if IOSWP fails on the bench: PB14 is also TIM12_CH1 / TIM1_CH2N / TIM8_CH2N, PWM with DMA; it would cost a timer, so it is not used unless needed. SPI + DMA, 4 SPI bits per WS bit (0 = 1000, 1 = 1110, T0H 0.32 us, T1H 0.96 us, period 1.28 us), rate picked from the real SPI kernel clock nearest 3.125 MHz (reported in hello as `lights.strip.hz`), 9 bytes per pixel plus a reset gap, buffer in D2 SRAM cleared at run time. About 60 fps from the main loop, brightness cap 64 of 255, gamma on the palette, temporal dithering so the low floor still glows.
- NeoTrellis 4x4: seesaw at 0x2E on D11/D12 (protocol from Adafruit_Seesaw: reset, HW ID 0x55, neopixel pin 3, GRB, keypad events for all 16 keys), keys polled at 100 Hz (edge 3 = pressed), LEDs at about 15 fps in 24-byte chunks, one I2C job per main-loop pass.
- Audio: Seed audio started, line in L drives the lights, in to out passthrough unity. Envelope (5 ms attack, 250 ms release) plus an onset detector in the callback; the main loop reads them. Frame carries `a` = [level per mille, onset count] and `k` = key mask.
- Look: dark purple (#2a0845 to #5b1a9e) with light-blue (#7fd8ff) blobs from 1D domain-warped noise, always on at a floor of 9 % of the cap, louder = brighter and faster drift, onsets spawn a blue bloom travelling outward from the middle, sustained loud notes push toward blue; the Trellis samples the same field at 16 points and flashes light blue on key presses.
- UI: Lights group (decimated strip preview and Trellis mirror at 10 Hz, key state, audio level) and drawer sliders for cap (max 64), floor and sensitivity (commands `lcap`, `lfloor`, `lsens`). Guided step: press and release all 16 keys.
- PASS: all 16 keys give press and release, no stuck key; strip colours stay in the purple/blue palette, no flicker at 60 fps for 10 min; ADC rest noise and the stream rate unchanged against test 06; audio out equals audio in.

### FW-08 (`TEST=08_mic`, fw 1.1): the full build
Everything from FW-06 (faders, pots, FSR, joystick, valves, encoders, IMU, lidar), FW-07 (strip with 4-bit WS encoding, NeoTrellis, audio-reactive lights) and a mic meter, all live at once. Mic in L goes dry to out L and out R at unity. The frame carries `m` = [peak dBFS x10, rms dBFS x10, clipped-sample count]; the UI Mic group shows a vertical level fader (green below -12, yellow -12 to -3, red above -3 dBFS; the one colour exception), a peak-hold tick that falls over 1.5 s, a clip lamp that latches until clicked, and a target band of -12 to -6 dBFS peak.
- ADC under audio (bug: fw 1.0 07b froze all 11 ADC channels at a constant, and the main loop slowed about 3x, as soon as audio started): cause not proven. Measures in this build: ADC started 300 ms after the audio, recalibrated once at 2.5 s, watchdog (all 11 channels identical for 1 s restarts the ADC, stops it first), and a VDDA readback through ADC3 VREFINT (hello `adc.vdda` in mV) to show whether the analog supply sags when audio starts. If vdda drops, the cause is the 3V3A load.
**FW-08 addition (fw 1.2): output gain.** `ogain <dB>` (-40 to +6, default -12) scales the dry mono output (out L and out R) with a 20 ms one-pole ramp in the audio callback; the mic meter still measures the input. The value is stored on the Seed with libDaisy PersistentStorage in the QSPI sector at offset 0x700000 (0x90700000; the app is at 0x90040000, the bootloader region is below 0x40000, PMOR uses 0x7C0000 and 0x7F0000), loaded before the audio starts, saved from the main loop at most once per second after a change. Hello: `ogain` (tenths of dB), `ogsaved`, `ogloaded`. UI: Output knob next to the Mic meter (drag, wheel, double-click for -12 dB); the page syncs from the hello.

### 06. Encoders and buttons (all of 05 live)
- Wire: KY-040 modules powered from **+3V3D pin 38** (the modules have pull-ups to `+`, so `+` must be 3.3 V) and GND -> DGND pin 40.
  - Enc 1: CLK -> D0 pin 1 (PB12), DT -> D1 pin 2 (PC11), SW -> D2 pin 3 (PC10).
  - Enc 2: CLK -> D3 pin 4 (PC9), DT -> D4 pin 5 (PC8), SW -> D5 pin 6 (PD2).
  - Joystick SW -> D6 pin 7 (PC12), internal pull-up, other side DGND.
- Measures: Gray-code decode (`hg_enc.h`, 4 edges per detent), switch state with 15 ms debounce, counters, bounce count.
- UI: two dials with detent counters and direction, three button lamps, raw A/B/SW traces, bounce counter.
- PASS: 20 detents clockwise = +20, 20 counter-clockwise = -20 (no lost or doubled counts, fast spin included); 20 presses of each switch = exactly 20 events (0 double fires after debounce); hardware DFU escape: hold Enc 1 SW + joystick button, power-cycle, Seed shows up on `dfu-util -l`.
- Recheck: ADC channels p-p unchanged while spinning an encoder (switching noise).

### 07. (reserved: folded into 06; joystick button and encoder switches are tested there)

### 08. MPU6050 (all of 06 live)
- Wire (GY-521): VCC -> +3V3D pin 38, GND -> DGND pin 40, SCL -> D11 pin 12 (PB8), SDA -> D12 pin 13 (PB9), AD0 -> GND (address 0x68), INT unconnected. Bit-banged bus (`bbus`), pull-ups: the breakout carries its own; total bus pull-up after all I2C parts must be 2.2-4.7 kohm.
- Measures: WHO_AM_I (0x75 = 0x68), accel X/Y/Z at 100 Hz (16384 LSB/g), gyro, bus errors and recoveries.
- UI: three accel bars, gravity magnitude, tilt dial, error counters.
- PASS: WHO_AM_I 0x68; flat on a table |g| = 1.00 +-0.05; each axis shows +-1 g when stood on its edge (+-0.1); rest noise p-p < 0.02 g; 100 Hz sustained with 0 bus errors in 10 min; **USB stays enumerated during the whole run** (PMOR gotcha).
- Recheck: ADC p-p and the audio floor.

### 09. VL53L1X lidar (all of 08 live)
- Wire: VIN -> +3V3D pin 38, GND -> DGND pin 40, SCL -> D11 pin 12, SDA -> D12 pin 13 (same bus). XSHUT unconnected (option: D13 pin 14).
- Measures: range mm at 33 Hz, status, bus errors.
- UI: distance bar and trace, valid-sample ratio.
- PASS (PROV): flat target at 100 / 300 / 600 mm reads within +-15 mm (30-1200 mm usable); stdev at a fixed target < 5 mm; valid ratio > 95 %; no bus errors in 10 min; sensor laser pulses do not move ADC channels (rest p-p unchanged).
- Recheck: ADC p-p and the audio floor (the hyperGuitar lidar ticked at its poll rate).

### 10. NeoTrellis keys (LEDs off, all of 09 live)
- Wire: NeoTrellis 4x4 power from **+3V3D pin 38** for this test, GND -> DGND pin 40, I2C SCL/SDA -> D11 pin 12 / D12 pin 13 (address 0x2E). Brightness 0, NeoPixel path unused. Boot order (PMOR): USB up, settle, then trellis last, USB-safe delays only (`usbDelay`, never `System::Delay` once TinyUSB is used).
- Measures: key press/release events for 16 keys, stuck-key watchdog, bus recovery count.
- UI: 4x4 grid lighting on press, event log with timestamps.
- PASS: every one of 16 keys gives press + release exactly once per tap, 3 passes in a row; no stuck keys; 0 stretch errors in 10 min; USB stays up through a seesaw reset at boot.
- Recheck: ADC p-p and the audio floor.

### 11. NeoTrellis LEDs (all of 10 live)
- Wire: as test 10; first run the LEDs from **+3V3D** at the PMOR cap (brightness 0.45) and measure current from the 3V3D pin; if the draw is near the unknown limit (keep < 100 mA, PROV) move the NeoTrellis power to the 5 V LED branch from the star (never to a Seed pin) and first check that its I2C pulls are 3.3 V and that the seesaw accepts 3.3 V logic at 5 V supply (**unverified**).
- Measures: frame diff push at 12.5 Hz, push time, recover count.
- UI: LED colour grid driven from the page (all red, green, blue, white, sweep), push timer.
- PASS: all 16 pads show each primary correctly; no flicker over 10 min; USB stays up; **ADC rest p-p and the audio shorted-input floor within the recheck limits at brightness 0, 0.25 and 0.45 (the hyperGuitar failure mode)**. If it fails, first try the 10-second fix: move the NeoTrellis ground wire.

### 12. FCOB strip
- Wire: D10 pin 11 (PB5, SPI1 MOSI) -> 74AHCT125 input (74AHCT powered 5 V) -> 330 ohm -> strip DIN. D8 pin 9 left **unconnected**. D26 pin 33 -> gate of the high-side MOSFET that switches the strip's 5 V. Strip 5 V and GND directly to the star, not through the Seed. Do **not** copy PMOR's `NVIC_DisableIRQ(SPI1_IRQn)` (DMA needs it).
- Measures: frame send time (~4.8 ms for 157 px), 1100 vs 1110 bit encoding, brightness cap.
- UI: brightness slider (0 to cap 20/255), colour fills, frame counter.
- PASS: correct colours on all pixels at brightness 5, 10, 20/255; no flicker at 60 Hz for 10 min; D26 off = 0 mA on the strip branch; **ADC rest p-p and audio floor rechecked at each brightness** (most likely noise source with test 13).
- Recheck: as above.

### 13. Xvive TX (wireless out)
- Wire: panel jack signal from Seed pin 18 (L) and 19 (R), jack ground to AGND pin 20; 3.5 mm to dual 6.35 mm Y to the two Xvive A58 TX; TX power from the BMS output pads. TX returns on their own wire to the star.
- Measures: audio floor (P2 firmware) with TX on and the wired jack connected.
- UI: audio meters.
- PASS (PROV): wired jack floor unchanged within +3 dB of P2 with both TX transmitting; ADC rest p-p within recheck limits; no new tonal spur above -90 dBFS.
- Recheck: full system, everything live. This is the end state.

## 3. What is not done yet
- `TEST=P2_audio` and tests 05-13 have no firmware yet. `fw/tests/test_02..04` (pots, FSR, joystick) compile today (analog channel tables only); the web UI uses the generic sweep logic for them.
- Hall calibration, encoder/IMU/lidar UI pages: written when each part arrives.

## FW-10 effects (test 10_delay, bringup-1.4): DRONE -> REVERB
- Chain: mic in L -> DRONE (additive send) -> REVERB -> output gain -> out L/R. HYPER_CORE headers are used unedited (except HX_MAXP 12 -> 16, approved).
- Controls: FADER 1 = reverb MIX, FADER 2 = drone MIX (both remapped n=clamp((raw-0.30)/0.70), smoothed), accel Y = reverb WARP + MODDEPTH. Locked in the UI, labelled with the source.
- CPU gate per effect: mix 0 = not computed at all; wake clears state and fades in; back to 0 fades out, stops at < -80 dB or 50 ms.
- Params: generic, from the HYPER_CORE descriptors. Serial: `p <fx> <PARAM> <value>`, `pd <fx>`, `preset reset`. Persisted in QSPI 0x700000 (prec.h: version + CRC, entries keyed by effect:PARAM name, saved 2 s after the last change). Host test: fw/tests/host_prec_test.cpp.
- Telemetry: rv {on,mix,wet,warp,ay,cyc,pk,cpu4[idle,reverb,drone,both]}, dr {on,mix,wet,seg,cyc}; hello params {loaded,saved,svms,saves,fx[]}. svms = how long the main loop blocked on the last QSPI save: read it on the bench and listen for a glitch while dragging a slider.

## FW-10 update (bringup-1.5): faders, all effects, pads
- Faders: one path, `fw/fader_cal.h`: raw, 20 ms one-pole, linearize() (the curve the UI shows), per-fader endpoints lo/hi (raw), exact 0 below 3 % (back at 1.5 %), exact 1 above 97 % (back at 95.5 %). Endpoints learn by themselves until stored (move each fader to both ends, settles after 3 s), `fcal learn|save|reset|set i lo hi`, stored in the same QSPI record (prec.h v4). UI shows calibrated 0..100 big, raw and lin small.
- Effects: every HYPER_CORE effect except pitchtrack (not an effect: it is the shared f0 tracker, run only while harmonizer / MOOD / ringmod is on) through `fx_glue_tmpl.h` + one `fx_glue_<name>.h` each; reverb uses the same template, the drone glue (other owner) is adapted in `fx_glue_chain.h`.
- Pads: `docs/TRELLIS_MAP.md`.

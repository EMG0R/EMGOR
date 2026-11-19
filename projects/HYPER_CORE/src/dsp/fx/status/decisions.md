
## Decisions locked by Emory (2026-11-15) and where they live

1. **Harmonizer pitch source.** Period-synchronous grain length comes from the tracked f0 when the tracker is confident (conf > 0.4, 70..1600 Hz); otherwise from the **valve pitch hint** (`FxCtx::valvePitchHz`); otherwise the 24 ms unvoiced fallback. Implemented in `fx_shifter_core.h` (`ShifterCore::setPitch(f0, conf, sr, valveHz)`, `src` = 0 fallback / 1 tracked / 2 valve), used by HARMONIZ and MOOD; unit-tested in `test_fx_harmonizer.cpp`. The hint is built in `mapping/fx_mapping.cpp`: valve combination -> pedal-tone fundamental (116.54 Hz lowered by the pressed valves, drops 2/1/3 semitones) x **partial**. The partial is learned from the tracker while it is confident and **remembered when it drops out**, so a fingering change still moves the hint (default partial 4 until one is learned; no hint when there is no breath: `env` < 0.04). Embouchure has no sensor: the "partial estimate" is the remembered tracked partial. Interpretation note: "pitch-correct" = make the shifter's grain period-correct (coherent); the intervals themselves are already exact ratios. Unverified by ear.
2. **Looper memory.** 4 layers x 60 s mono at 48 kHz = 46,080,000 B, from a dedicated SDRAM looper pool (`hx::g_loopPool`, bound in `firmware/common/hx_seed.h`). Host tests and the sim keep 12 s layers in the slot arena (default `HX_LOOP_SECONDS` 12; firmware builds pass `-DHX_LOOP_SECONDS=60`). Only ONE looper can exist at a time (it owns the pool; constructing a second one wipes the first).
3. **Real Seed CPU numbers.** `firmware/bench/` runs every effect alone (defaults + a stress preset), the default 4-slot patch (with and without the mapper), and the worst 4 in the real 48 kHz / block-48 audio callback; avg and peak % via DWT cycles and libDaisy `CpuLoadMeter`, JSON over USB CDC, table via `bench_read.py`. **Until a bench table is pasted here, every Seed CPU figure above is the host-scaled guess.**

## SDRAM memory map (Seed 3, 64 MiB = 67,108,864 B; static arrays in `.sdram_bss`)

| region | bytes | % of 64 MiB | notes |
|---|---|---|---|
| looper pool | 46,080,128 | 68.7 % | 4 x 60 s x 48 kHz x 4 B + 128 B arena slack |
| slot arenas 4 x 3 MiB | 12,582,912 | 18.8 % | largest non-looper effect = GRAIN 1.02 MB, REVERB 0.47, DELAY 1.00, FFT effects 0.05..0.14; 3 MiB = ~3x headroom; any effect in any slot |
| schema scratch (main fw only) | 2,097,152 | 3.1 % | builds each effect once to read its Param descriptors for the `schema` command; looper pool is unbound while doing so |
| **used (bench / main)** | **58,663,040 / 60,760,192** | **87.4 % / 90.5 %** | |
| **free** | **8,445,824 / 6,348,672** | **12.6 % / 9.5 %** | margin |

Internal RAM: the firmware image is ~210 KB (text 207 KB main, 179 KB bench) so it does NOT fit the 128 KB internal flash: APP_TYPE=BOOT_SRAM (QSPI 0x90040000 via the Daisy bootloader, executes from AXI SRAM, 480 KB budget, ~250 KB used). SDRAM is not zeroed at reset; arenas zero what they hand out, so creating the looper zero-fills 46 MB (control thread, a few hundred ms, audio keeps running).

## Firmware (HYPERTRUMPET_V2/firmware)

- `bench/` -> `build/hx_bench.bin` (see `bench/bench_read.py`, `bench/flash_bench.sh`). Build only, bench run by the coordinator.
- `main/` -> `build/hx_main.bin`: audio callback (mono bell mic on line L -> 4-slot engine -> stereo out), 5 ms control loop (sensors -> `HxMapper::update` -> engine), USB CDC protocol v1 (header of `main/main.cpp`: hello, schema, get/set/fx/byp/act/sel, `ctl` input override, `cal`, `stream` telemetry, `dfu`). Sensors behind one abstraction (`main/hx_sensors.h`) over the bring-up wiring: faders A3-A5 (LUT inverse A taper), pots A6/A7 (810.5-cycle sample, LUT inverse C taper), FSR A8 (PMOR `FsrScale`, cubed), MPU6050 0x68, VL53L1X 0x30 (XSHUT D7) + 0x29 (XSHUT D9), bit-banged I2C D11/D12; bring-up drivers copied verbatim to `main/drivers/`.
- **Not done / unverified in main:** nothing has run on hardware. Hall valves (A0-A2, `-DHX_HALLS=1`) and joystick (`-DHX_JOY=1`) are off by default (not in the bring-up wiring; floating pins would fake valve presses); encoders, pad clicks and the NeoTrellis are not wired in the skeleton, so bank/effect browsing is via USB (`fx`, `patch default`); IMU axis/sign vs mounting, lidar polarity and which lidar is the "hand" one (LIDAR1 used) are guesses; the sim UI side of the protocol is not written.

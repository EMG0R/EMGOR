# HYPERTRUMPET V2 bring-up

Plan: `BRINGUP_PLAN.md`. Firmware `fw/` (one libDaisy app, `TEST=` selector). Web UI `web/index.html` (WebSerial, Chrome/Edge).

## Run the web UI (one command, no python)
    cd ~/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2/bringup/web && node -e "require('http').createServer((q,r)=>{r.setHeader('content-type','text/html');require('fs').createReadStream('index.html').pipe(r)}).listen(8731,'127.0.0.1')"
Open http://localhost:8731 in Chrome, press Connect, pick the Seed. (`Demo` runs synthetic data with no hardware; `?selftest=ok|bad|xtalk` runs the PASS/FAIL logic headless.) WebSerial needs a secure context: localhost counts, plain http to another host does not. file:// was not verified.

## Build and flash
(Default = BOOT_NONE: app in internal flash 0x08000000, written through the ST ROM DFU. No Daisy bootloader needed, app must be <=128 KB; test 01 is 85 KB.)
    cd fw
    make                       # TEST=01_faders -> build/bringup_01_faders.bin
    ./flash.sh                 # build + buttonless "dfu" (ROM DFU) + dfu-util -s 0x08000000:leave
    ./flash.sh 02_pots         # another test

First flash, or if the Seed is not running this firmware: hold BOOT, tap RESET, release BOOT (`dfu-util -l` shows 0483:df11), then `./flash.sh`. Afterwards the web UI "Enter DFU" button or `./flash.sh` reboots into ROM DFU with no buttons. Nothing here writes option bytes or uses mass erase. Optional QSPI path (needs the Daisy bootloader installed, for apps >128 KB later): `INTERNAL=0 ./flash.sh` (BOOT_SRAM, 0x90040000, PMOR method).

Flashed state 2026-11-15 (by the coordinator): test 01 BOOT_NONE at 0x08000000, enumerates as /dev/cu.usbmodem327F347733331, streams 200 Hz (verified: 5 ms frame spacing, floating values since faders unwired).

### Test 05 (faders + MPU6050 + VL53L1X)
    ./flash.sh 05_faders_imu_lidar     # sends "dfu" over CDC, ROM DFU, writes 0x08000000
Same flash path as above. Extra messages: `{"t":"imu","ms","a":[x,y,z],"g":[x,y,z],"tmp","pr":[pitch,roll]}` (raw, pitch/roll in 0.01 deg) at 100 Hz, `{"t":"lid","ms","mm","st"}` per result (st 11 = valid), and an `i2c` object in hello (scan list, per-device state, error counter). Extra command: `scan`.

### Test 06 (2-wire pots + encoders), BOOT_SRAM
    cd fw && make TEST=06_pots2w_enc APP_TYPE=BOOT_SRAM     # build/bringup_06_pots2w_enc.bin, linked for 0x24000000, stored at QSPI 0x90040000
    INTERNAL=0 ./flash.sh 06_pots2w_enc                       # needs the Daisy bootloader on the Seed; never overwrites 0x08000000
Extra commands: `calinv <ch> <0|1>`.

## Serial protocol (newline JSON, 200 Hz)
Seed to host: `{"t":"hello","test","fw","rate","ch":[{"n","k","pin","phys"}]}` 1 Hz; `{"t":"s","n","ms","v":[raw16...]}` 200 Hz; `{"t":"st","min","max","pp","ppmin","cov":[32-bit bin masks],"fill"}` 20 Hz.
Host to Seed: `reset` (clear stats), `hello`, `dfu` (reboot to bootloader). Send them with DTR asserted (the web UI does).

## Notes
- USB is libDaisy's own CDC (interrupt driven), not HYPER_CORE's unfinished TinyUSB layer (`src/usb/*` still needs `audio_format.h` and trimmed descriptors). `HYPER_CORE/src/hal/dfu.cpp` is reused as is (compiled from its path) so the DFU logic is the shared one.
- Add a test: copy `fw/tests/test_01_faders.h`, add a branch in `fw/main.cpp`, `make TEST=<name>`.
- libDaisy v9.0.0 in `HYPER_CORE/third_party/libDaisy` (has `DAISY_SEED_3`, built there: `make` in that folder; PMOR's vendored 2024 copy is untouched).

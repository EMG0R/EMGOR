# HYPERTRUMPET V2

Daisy Seed 3 hyper-instrument for trumpet: mic → effect chain → mono out, NeoTrellis pad menu, faders/pots/FSR/joystick/encoders/IMU/lidar, a 157 px WS2812B strip on the bell.

Repos (gorcave is origin, see EMGOR_SKILLS/cod/git.md):
- `HYPERTRUMPET.git` (this repo, folder `HYPERTRUMPET_V2/`)
- `HYPER_CORE.git` (shared DSP/HAL lib, folder `HYPER_CORE/`, libDaisy v9.0.0 as a submodule)

Clone both side by side; the firmware Makefile expects `../../../HYPER_CORE` from `bringup/fw`:
```
git clone server@gorcave.lan:git/HYPER_CORE.git && git -C HYPER_CORE submodule update --init
git clone server@gorcave.lan:git/HYPERTRUMPET.git HYPERTRUMPET_V2
```

Build (GNU Arm 10.3-2021.10): `cd HYPERTRUMPET_V2/bringup/fw && make TEST=14_padfx APP_TYPE=BOOT_SRAM BUILD_DIR=build_padfx`.
The current instrument build is **TEST=14_padfx** (`padfx-*` version strings). 13_rig is an older layout; 12_drone is drone-only.

Flashing: only the Mac with the Seed on USB (`bringup/tools/flash.sh <bin>`; QSPI 0x90040000 via the Daisy bootloader; it refuses the ST ROM loader). Never write option bytes, never mass-erase.

Rules for agents: edit in place, additive; bump the version string on every behaviour change; say `CLAIMING <path> on <node>` on the bus before editing shared files; `git pull --rebase`, never force-push.
Docs: `docs/` (TRELLIS_MAP, FX_BUDGET, LOOPER, V1_MAP_AUDIT, GROUNDING_PLAN, MIC_CHAIN), `bringup/BRINGUP_PLAN.md`.

## Building and flashing from NEPTR (Linux Pi, `gorpi7v5`)
Toolchain: GNU Arm 10.3-2021.10 (aarch64) in `~/toolchains/gcc-arm-none-eabi-10.3-2021.10/` (the system gcc 14 overflows SRAM). libDaisy is cloned, uncommitted, into `HYPER_CORE/third_party/libDaisy` and built with the same toolchain. The Pi plays live audio: builds run `nice -n 19`, `make -j1`.
`bringup/tools/ht`:
- `ht build`: pull, build 14_padfx, archive to `~/HYPERTRUMPET_BUILDS/<version>-<sha>.bin` + `.sha256` (never overwritten)
- `ht list`: archive, newest first, `*` = last flashed (`~/HYPERTRUMPET_BUILDS/FLASHED`)
- `ht flash <version|file>` / `ht switch <version>`: verify sha256, refuse if any other Daisy Seed (bench Seed 18063371376) is attached, ask the trumpet Seed (by-id serial 327F34773333) for DFU, refuse ROM DFU (no `/0x90` region), then only `dfu-util -a 0 -s 0x90040000:leave`; reads the hello `fw` string afterwards.

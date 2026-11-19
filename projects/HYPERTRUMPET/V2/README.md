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

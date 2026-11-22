# HYPER_CORE import log

2026-11-13. Every file in `HYPER_CORE/src`, `build`, `tools` that came from PMOR or hyperGuitar carries a header comment with origin path, commit, what changed, licence. Sources were **copied**; nothing in PMOR (`Pocket-OpGorator/**`, all `PMOR_V3*` clones) or `hyperGuitar/**` was edited, moved or deleted. No git commit was made anywhere. Nothing flashed.

Source commits: `PMOR_V3_live` ff86e4c, `PMOR_V3_max` 9fd83ba (read for the portable `pmor_core` and the protocol spec; same PMOR family), `hyperGuitar` 6088064 (the imported DSP files have no uncommitted changes; only bench binaries and a .pyc were dirty).

Verification done on the host only:
- `src/dsp/hg/hosttest/test_main.cpp` + `dsp_core`, `pitch_tracker`, `shifter`, `lpc` built from the HYPER_CORE copy against `src/hal/compat/Bela.h`: **ALL PASS (18)**, including the analog-synth patch tests (`synth.cpp` is textually included by the test).
- `proto_util.cpp` compiles; `engine_util.cpp` syntax-checks (host).
- NOT built for the Seed: `trellis.cpp`, `bbus`, `dfu`, `usb_service`, `usb/*`, `Makefile.template`. They need the first on-board bring-up; see "Known open items".

## Licence and credit summary

| Material | Licence / credit |
|---|---|
| hyperGuitar code (`src/dsp/hg`, `hg_enc.h`, hosttest) | Emory's own work. No LICENSE file in the repo; treat as Emory-owned. Algorithms credited in the code comments: NSDF/McLeod peak picking (McLeod and Wyvill), Derrien (DAFx-14) and Reboursiere et al. (NIME 2010) tracker timing limits, LPC/Levinson (standard method), SOLAD resampler pitch shifting |
| PMOR code (`src/hal`, `src/drivers`, `src/proto`, `engine_util`) | Emory's own work |
| **libDaisy** (`daisy_seed.h`, `per/*`, `sys/*` are only *referenced* and copied as reference text in `docs/ref/libDaisy_master_daisy_seed.*.txt`) | MIT, Electrosmith. Keep the copyright notice with any copy |
| **TinyUSB** (not vendored; `src/usb/tusb_config.h`, `usb_descriptors.*` follow its example layout; `board_stm32h7.c` feeds its dwc2 driver) | MIT, Ha Thach. Third party. Fetch it separately (`TINYUSB_DIR`) |
| ST HAL types used by `board_stm32h7.c` | BSD-3-Clause, STMicroelectronics |
| Daisy Seed3 datasheet PDF (`docs/ref`) | Copyright 2026 Qu-Bit Electronix DBA Daisy, MIT-style permission text printed on page 3 |
| TI TAC5242 datasheet PDF (`docs/ref`) | Copyright Texas Instruments; kept as private reference, do not redistribute publicly |
| `docs/ref/PMOR_PROTOCOL.md`, `docs/ref/hyperGuitar/*.md` | Emory's own work |
| **Iain McCurdy flanger / grain delay (Csound)** | **CC BY-NC-SA**. NOT imported: no McCurdy code exists in PMOR or hyperGuitar. The VOCAL_HAT README and PLAN already require credit and share-alike if a C++ port is written later. Do not copy any McCurdy-derived code into HYPER_CORE without that notice |
| DaisySP (Electrosmith, MIT) | Not imported. It already has reverb, delay, pitch-shift and filters; evaluate it before porting more effects |

## Taken as-is or adapted

| Source file (under `~/Documents/_EMGOR_SYNTH/______2026NEW/`) | Status | Destination (`HYPER_CORE/`) | Why / what changed |
|---|---|---|---|
| `Pocket-OpGorator/.../PMOR_V3_live/flash.sh` | adapted | `tools/flash.sh` | Proven buttonless-DFU flash: DTR-asserted CDC `dfu`, then a fast `dfu-util` catcher loop for the QSPI address. Only APP name made env-overridable and the title generalised |
| `PMOR_V3_live/Makefile` | adapted | `build/Makefile.template` | Keeps `APP_TYPE = BOOT_SRAM` (app in QSPI at 0x90040000, bootloader installed once via `make program-boot`). Replaced PMOR source wildcards with `src/*`, dropped FatFS, DEMIURGE tap header and the TinyUSB audio/clock sources. **Unbuilt** |
| `PMOR_V3_live/hw/dfu.cpp` | adapted | `src/hal/dfu.cpp` | Hardware both-buttons-at-boot escape + CDC-`dfu` reset, both via `System::ResetToBootloader(DAISY_INFINITE_TIMEOUT)`. Pins now macros; the flag comes from a hook instead of `clock_engine` |
| `PMOR_V3_live/hw/usb_service.cpp` | adapted | `src/hal/usb_service.cpp` | `pmor_usb_service` pump, USB-safe `usbDelay`, line-buffered CDC reader (64-byte line, silent overflow drop). Audio-stream and clock-engine calls replaced by two hooks |
| `PMOR_V3_live/usb/board_stm32h7.{c,h}` | taken | `src/usb/` | OTG_FS PHY pin/clock/VBUS bring-up for TinyUSB on the H750 |
| `PMOR_V3_live/usb/tusb_config.h` | taken | `src/usb/` | Review for CDC+MIDI only before first build |
| `PMOR_V3_live/usb/usb_descriptors.{c,h}` | taken | `src/usb/` | Composite descriptors; still contains PMOR audio-class parts under `CFG_TUD_AUDIO`, to be trimmed |
| `PMOR_V3_live/hw/bbus.{h,cpp}` | taken | `src/hal/` | Bit-bang open-drain I2C with the 5 ms clock-stretch wait, sticky `stretch_err`, 9-clock `recover()`, seesaw read helper. This is the code that fixed "NeoTrellis kills USB" (see SEED3_PINOUT section 7). Hardware I2C is untested with seesaw on this part, so this stays the default |
| `PMOR_V3_live/hw/trellis.cpp` | adapted | `src/drivers/trellis.cpp` | NeoTrellis seesaw driver: init, 2 ms chunked LED push, frame-diff, watchdog, keypad FIFO with stuck-pad releases, one rotation table. Only includes changed |
| `PMOR_V3_live/hw/_fsr_scale.h` | taken | `src/drivers/fsr_scale.h` | FSR conditioning (330 ohm, cubed curve). Retune values for the FSR402 on the bench; the structure is the point |
| `PMOR_V3_live/engine/engine_util.{h,cpp}` | adapted | `src/dsp/` | Header-only helpers: LCG, NaN/denormal flush, `fastPow2`/`fastExp`, `OnePole`, `ADSR`, `Env`, clamp/lerp. Only the `config.h` include replaced by `hyper_config.h` (new, 3 constants) |
| `PMOR_V3_max/pmor_core/proto_util.{h,cpp}` | taken | `src/proto/` | CRC-16/CCITT-FALSE, CRC-32, FNV-1a, base64, decimal float codec. These are exactly the pieces the CDC frame format (`!TAG CMD args*CRC\n`) needs |
| `PMOR_V3_max/pmor_core/param.h` | taken | `src/proto/param.h` | `Param`/`ParamPage` descriptor and clamp/normalise helpers. The seed of the param schema |
| `PMOR_V3_max/pmor_core/platform.h`, `host/platform_host.cpp` | taken | `src/proto/` | Two-function platform seam (`pmorNowMs`, `pmorCycles`) so the core builds on host and Seed |
| `PMOR_V3_max/docs/PROTOCOL.md` | taken as reference | `docs/ref/PMOR_PROTOCOL.md` | Contract for the framing, schema, errors. Reuse sections 1, 4, 9, 11; drop PMOR sample/bin/archive commands |
| `hyperGuitar/bela/hyperGuitar6chPan/dsp_core.cpp` | taken | `src/dsp/hg/` | Shared primitives (pBLEP etc.), the portable core of the DSP |
| `.../pitch_tracker.cpp` | taken | `src/dsp/hg/` | Per-string NSDF/McLeod pitch tracker v2 (pick-referenced, string-range prior). Logic intact. For one voice (hat) use string slot 0 only |
| `.../shifter.cpp` | taken | `src/dsp/hg/` | SOLAD resampler pitch shift down, soft-splice up, mark-synchronous COLA formant. Logic intact. Two `rt_printf` diagnostics remain (Bela call seam) |
| `.../lpc.cpp` | taken | `src/dsp/hg/` | True spectral-envelope formant (LPC lattice with warped targets) |
| `.../effects.cpp`, `synth.cpp` | taken | `src/dsp/hg/` | Guitar-voice effects (FM, autowah, resonator, jawari, sustainer, sitar, pluck-sag, vibrato, pog, chorus) and 23 analog pad patches that track pitch. Optional for the hat; kept because the host test covers them. No decision yet which ship |
| `.../hyperguitar.h`, `hg_mc.h` | taken | `src/dsp/hg/` | Shared declarations; still `#include <Bela.h>` and `Encoder.h`, satisfied by the compat stubs below. 6 voices are the compile-time default (`NUM_STRINGS 6`); a one-voice build is a later config change, not done here |
| `.../hg_enc.h` | taken | `src/dsp/hg/hg_enc.h` | Header-only Gray-code quadrature decoder with settle rule, host-tested. Chosen over PMOR's 750 Hz ISR poll (`controls.cpp` ePoll) because it is pure and has the lost-edge rule. KY-040: `ENC_EDGES_PER_DETENT 4` |
| `.../hosttest/stub/Bela.h`, `stub/libraries/Encoder/Encoder.h` | taken | `src/hal/compat/` | These stubs are the small HAL seam for the Bela API (`rt_printf`, `BelaContext`, `AuxiliaryTask`, `Encoder`). `src/dsp/hg/hosttest/stub` is a symlink to this folder so the test Makefiles run unmodified |
| `.../hosttest/{Makefile,test_main.cpp,names.inc,test_synth.inc}` | taken | `src/dsp/hg/hosttest/` | 18-test host suite for tracker, shifter, LPC, synth. `names.inc` is checked in because its Makefile rule reads `render.cpp`, which was not imported |
| `.../hosttest/{pitchbench,lpcbench,synthbench}/*` sources, Makefiles, findings | taken | `src/dsp/hg/hosttest/` | Offline harnesses. The 125 MB `bench/` audio fixtures and built binaries were NOT copied |
| `hyperGuitar/docs/{pitch-tracker-overhaul,engine-rewrite-2026-10-01,eventide-pitch-shift-research,tracker-diagnosis-2026-10-01,NOISE-HANDOFF}.md` | taken | `docs/ref/hyperGuitar/` | Design notes for the imported DSP and the LED-noise history |
| New, written for HYPER_CORE (not copied) | new | `src/hal/hyper_hw.h`, `src/dsp/hyper_config.h` | Declarations only: the HAL surface the imported drivers need, and three constants. No feature code |

## Left behind (and why)

| Source | Why not imported |
|---|---|
| PMOR `usb/clock_engine.{c,h}` (2051 lines), `engine/clock.*`, `transport_link.*` | MIDI clock master/follower, PO-sync and SPP bar-lock for PMOR/NEPTR. Not relevant: neither instrument follows or sends clock yet. Revisit if tempo-synced delays are wanted (then take only the follower) |
| PMOR `usb/usb_audio_stream.*`, `audio_format.h`, `usb_dma_mem.h` | USB audio is ruled out ("no USB audio, ever") |
| PMOR `hw/boot.cpp` | The boot order is the valuable part and is documented in `SEED3_PINOUT.md` section 7 (USB init, 500 ms settle, forced disconnect/reconnect, wait for `tud_mounted`, buses, audio, NeoTrellis last). The file itself is welded to PMOR engines, OLED, SD |
| PMOR `hw/oled*.{cpp,h}`, `ui/*` | No OLED on either instrument. Also the source of the USB-vs-OLED power problem |
| PMOR `hw/sd.cpp`, `store/files.*`, SD/FatFS | No SD card on either instrument |
| PMOR `store/patch.{cpp,h}` | PMOR-shaped (64 patches, sequencer lanes, sampler audio sidecar). The reusable *pattern* is documented for the preset step: a `{magic, version, payload}` record via libDaisy `PersistentStorage` at a fixed QSPI offset (PMOR: 0x7F0000 globals, 0x7C0000 patch mirror, both far above the 0x90040000 app), version mismatch resets to defaults once, 3 s debounced autosave. Write new for hyper; do not copy |
| PMOR `hw/sensors.cpp`, `VL53L1X_mini.*` | Coupled to PMOR `g` shared state. VL53L1X driver is the wrong chip: the trumpet has a VL53L0X (different register set). MPU6050 sequence worth reusing from it: `0x6B=0x80` reset, 100 ms, `0x6B=0x01`, WHO_AM_I `0x75` (0x68 for the 6050), accel regs from `0x3B`, `/16384` for +-2 g |
| PMOR `hw/controls.cpp` | Joystick centre-calibration (skip 200 polls, average 64) and learned max deflection are good ideas; the code is entangled with `g`. Rewrite, citing this file |
| PMOR `engine/*` (synth, drums, sampler, sequencer, fx, fx_v2) | PMOR instrument engines. `fx_v2.cpp` has a small 4-comb "GVERB" and delays, but they are fused into PMOR's punch-in bank logic; DaisySP is the cleaner source for reverb/delay |
| PMOR `pmor_core/proto.{h,cpp}` (672 lines) | PMOR protocol server: samples, recycle bin, archive, JOIN. Reuse the *design* (frame format, schema push, one multi-line request at a time, OK/ERR finals, session reset causes) via `docs/ref/PMOR_PROTOCOL.md`; write a smaller server for hyper |
| PMOR `pmor_core/params.{h,cpp}`, `patch_format.*` | PMOR domains and a frozen PMOR patch layout. The id scheme (`domain<<10 | index`, append-only, never renumber) is worth copying as a rule |
| PMOR `max/` (Max externals, `.maxpat`, `pmor_*.js`) | Max-for-Live editor stack. The hyper editor is a WebSerial page (planned), not Max. Revisit `pmor_edit.js` / `schema.json` when the web UI starts |
| PMOR monitor app (`PMOR.app/monitor/*.py`), `tools/*` | macOS pygame monitor and PMOR drum-level tools |
| hyperGuitar `render.cpp` (1642 lines), `presets.cpp`, `trellis_ui.cpp` | Bela render loop, 6-string presets and Trellis UI on Bela. The per-instrument glue must be rewritten for Seed 3 |
| hyperGuitar `hg_mc.cpp` (multicore worker pool, EVL threads) | Bela/Xenomai specific; Seed is one core. `hg_mc.h` symbols stay for the DSP; a single-thread shim is future work |
| hyperGuitar `hg_ctrl`, `hg_lanes`, `hg_link.h`, `hg_osc`, `hg_mon` | NEPTR OSC control link (lane model). Linux sockets; not applicable to a standalone Seed |
| hyperGuitar `i2c_sensors.*`, `deploy.sh`, `usbaudio-bridge/`, `hgTest/`, `sdcard/`, `trellis/` | Bela Linux I2C, deployment, USB audio bridge |
| hyperGuitar `archive/` | Superseded versions |
| McCurdy-derived flanger/grain delay | Not present in either source tree (Csound originals). CC BY-NC-SA, see above |

## Known open items before anything here goes on a board

1. Trim `usb_descriptors.c` / `tusb_config.h` to CDC + MIDI (no audio), then build `Makefile.template` against a pinned libDaisy that has `DAISY_SEED_3`, or the old one with the default-branch fallthrough (SEED3_PINOUT section 0).
2. `hyper_cdc_line`, `hyper_usb_extra_poll`, `hyper_dfu_requested` are declared but have no definition yet; weak defaults are to be added with the first firmware main.
3. `hyperguitar.h` assumes 6 voices and Bela types; a one-voice configuration and a `rt_printf` replacement are future work, intentionally not started.
4. `bbus` tbus/sbus both expect libDaisy `dsy_gpio` pins; hyper only needs tbus on D11/D12.
5. Hardware-I2C vs bit-bang for the NeoTrellis is open; bit-bang is the known-good path.
6. WS2812-over-SPI-DMA is **not** imported because no source in either tree contains it. Plan only, in SEED3_PINOUT section 6.

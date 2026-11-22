# PMOR_V2 firmware audit — source map for the PMOR_V3 rewrite

Date: 2026-09-14. Subject: `DaisyExamples/seed/PMOR_V2/PMOR_V2.cpp` (5470 lines) plus
`oled_pmor.h` (349), `VL53L1X_mini.cpp/.h` (162/34), `_fsr_scale.h` (89), `Makefile` (68),
`usb/` (board_stm32h7.c/h, clock_engine.c/h, usb_audio_stream.c/h, usb_descriptors.c/h,
tusb_config.h, usb_dma_mem.h, audio_format.h). Target: `/home/claude/PMOR-V3-SPEC.md`.

All line numbers are `PMOR_V2.cpp` unless a file is named. "L" = line.

---

## 1. FILE MAP — top-level sections of PMOR_V2.cpp

| Lines | Section | Purpose |
|---|---|---|
| 1–62 | Header comment | Architecture (two planes), menu grammar, safety invariants. |
| 64–104 | Includes / types | libDaisy, `dev/oled_ssd130x.h`, lidar, FSR, FatFS, PersistentStorage, TinyUSB (`extern "C"`), `oled_pmor.h`, `MyOled = OledDisplay<pmor::BitBangSSD130x128x64>`. |
| 113–135 | Core globals | `hw`, `display`, `lidar`, `g_fsr`, IMU mirrors `g_ax..g_gz`, boot-time marks, `g_en_usbaudio=false`, `g_usb_sine`. |
| 137–281 | **BBus** bit-bang I2C | Open-drain GPIO I2C with `waitScl` stretch timeout, `recover()`, seesaw `ssR1/ssRN`. Instances `tbus`, `sbus`. |
| 283–291 | Lidar register shims | `vl53_write_reg/vl53_read_reg` → `sbus.w16a/r16a(0x29,…)` with USB pump. |
| 293–438 | **NeoTrellis seesaw** | `TREL=0x2E`, `lavaColor()`, `ntShow()`, `ntEnableKeys()`, `usbDelay()`, `ntInit()`, `ntSoftInit()`. |
| 440–566 | **Encoder + joystick** | `Enc` struct, `enc1/enc2`, joystick globals, `joyAxis()`, `joyPoll()`, `ePoll()`, `eInit()`. |
| 568–665 | Synth scale + tables | `FLYD[28]`, `MODES[9][7]`, `rebuildScale()`, `DETUNE[5]`, global params, `WAVE_NAMES[13]`, `kpbuf` (Karplus), wavetable `wtbl[8][256]` + `buildWavetables()`. |
| 666–720 | Engine params / Voice | `kit_sel`, `g_vsyn/g_vdrum/g_vsmp`, `engVol()`, `struct Voice`, `psin()`, PolyBLEP/BLAMP, `voices[12]`, `lcg()`, `oct_shift`. |
| 722–825 | UI/mode state + param editors | menu enum, `step_ms`, engine/seq enums, `MAXSTEPS 128`, `syn_seq[]`, seq bit helpers, form (bars/beats), `globAdj()`, `songAdj()`, `engAdj()`. |
| 827–939 | **FX banks 1–8** state | enums + name tables for 128 pads, `fx_held..fx8_held`, `fx_eff..fx_eff8`, automation arrays `fx_auto[8][16]`, `fx_auto_fsr[16]`, `fx_bank`. |
| 941–1009 | Sessions + FX buffers | session globals, `RBUF_N` ring, all SDRAM FX delay buffers, ring sync (`rb_w`, `step_rw`, `fx_vs`), live latch. |
| 1011–1024 | Sampler state | `smp[16][96000]`, `smp_len`, `rec_slot`, `smp_pat[16][16]`, `sel_smp`, `smp_press`. |
| 1026–1075 | **Persistence structs** | `Persist`, `SessionBank`, `SESS_MAGIC 'OPG4'`, `static_assert(sizeof==2568)`, `markDirty()`. |
| 1077–1112 | Drum pattern + master volume | `dr_pat[4][16]`, `dr_pad[4]`, `kVolTable[16]`, `vol_pidx=10`, `mvol`, `sel_drum`. |
| 1114–1242 | **voiceCharacter()** | Per-note synth patch generator for all 13 waves (the "Thief" logic). |
| 1244–1290 | Euclid + SD patch mirror | `euclidHit()`, `pstore`, `patchSaveSD()` (atomic TMP→BIN/BAK), `patchLoadFile/SD()`. |
| 1292–1393 | Sampler audio on SD | `PATCHnn.AUD` chunked save (`audBeginSave/audSaveTick/audFlushSave/audEnsureSaved`) + load. |
| 1395–1432 | FX automation on SD | `PATCHnn.FXA` (`FXA_MAGIC 'PAXF'`). |
| 1434–1480 | Sequence sidecar on SD | `PATCHnn.SEQ` (`SEQ_MAGIC 'PSEQ'`): bars/beats + dr_pat + smp_pat + syn_seq. |
| 1482–1642 | Session bank mgmt | `persistSnapshot()`, `persistApply()`, `sessionsBoot()`, `sessionStash()`, `sessionLoad()`, `sessionNew()`, `sessionDelete()`. |
| 1644–1684 | Manual synth notes | `out_amp/cut_t`, `key_voice[16]`, `held_mask`, `filterTrack()`, `keyNoteOn/Off()`, `allKeysOff()`. |
| 1686–1801 | **Drum engine defs** | `DrumGo`, `drumTrig()`, `DrumDef`, `KIT[16]`, `PADN`, `KIT2[16]`, `PADN2`, `drumPadHit()`, `EUK`. |
| 1803–2138 | **gridTick()** | The entire sequencer/clock (audio-ISR plane): safeties, FX effective masks, sampler rec arming, WASH, HALF/DBL/STOP shadow clock, stutter, step firing, swing, live-record heads, BZZ. |
| 2139–2200 | USB capture bridge | 64→48 sample ring `usbCapturePush()` (disabled by `g_en_usbaudio=false`). |
| 2202–3378 | **AudioCallback** | Voices → per-voice SVF → master SVF → drive → chorus(off) → drums → sampler → drum SVF → tanh mix → FX1 → FX2..FX8 → RING/DIST/FLT/CRUSH → master vol → hard clip. |
| 3380–3580 | **SD bit-bang SPI + FatFS glue** | `spiXfer`, `sdCmd`, `ssd_initialize(_once)`, `ssd_status/read/write/ioctl`, `SPISD_Driver`. |
| 3582–3588 | OLED text helper | `row[32]`, `orow()`. |
| 3590–3660 | Example beat (disabled) + CDC console | `plantExampleBeatIfRoomExists()` (not called), `PollCdc()`. |
| 3662–3881 | Probe gates / CDC test hooks / staging | `g_en_*` gates, `pmor_usb_service()`, `oledCmdRaw`, `pmor_oled_test`, `pmor_pb5_test`, `pmor_oled_contrast`, `pmor_audio_ctl`, `pmor_probe_set/tick`, `PMOR_STAGE` macro. |
| 3883–4318 | **main() boot sequence** | DFU escape, USB init, mount wait, OLED init, sensors, lidar, SD mount, encoders/ADC, sessions, SDRAM clear, wavetables, audio start, `ntInit()`, intro, lava state. |
| 4320–5470 | **Main loop** | USB/CDC/DFU, trellis recovery+LED push (4376–4577), keypad (4579–4707), encoder/joystick menu logic (4711–4915), sensors + watchdog (4917–5020), FSR (5021–5045), session gestures (5046–5060), autosave (5062–5078), OLED render (5080–5468). |

---

## 2. HARDWARE CODE TO PORT VERBATIM

Every block below is proven on hardware and locked by the spec §0. Column "Touches" lists the
globals each block depends on, so the V3 module can own them or receive them.

### 2.1 Bit-bang I2C — `struct BBus` — L150–278, instances L280–281
- `init(sp,dp)` L154–159 (open-drain, NOPULL, both high, 2 ms).
- `waitScl()` L174–184: 10 000-iteration spin, then patient ≤5 ms wait **pumping `pmor_usb_service()`**, sets sticky `stretch_err`.
- `recover()` L188–192: 9 clocks + STOP.
- `start/stop` L194–195; `byteW` L197–210 (**pumps USB per byte**); `byteR` L211–224 (**no pump — deliberate**, see comment).
- `send/w8/r8/r16be/w16a/r16a` L227–261; seesaw `ssR1/ssRN` L265–277 (STOP + 500 µs gap, not repeated-START).
- Bus speeds: `us=5` default (100 kHz) for `tbus`; `sbus.us = 3` set at L4103.
- Touches: `pmor_usb_service()` (extern C, defined L3724), `System::Delay/DelayUs/GetNow`.
- Port note: V3 module `hw_i2c.h/.cpp`; the only external dependency is the USB pump. Give it a function pointer or a weak `pmor_usb_service` so the module compiles standalone.

### 2.2 NeoTrellis seesaw driver — L293–438 + main-loop pieces
- Constants/protocol comment L296–312; `TREL = 0x2E` L312.
- `ntShow(grb48)` L334–350: two 28-byte chunks at offsets 0/24 with `System::Delay(2)` between, then `{0x0E,0x05}` SHOW. Returns ACK status.
- `ntEnableKeys()` L364–370: seesaw pin = `(k/4)*8 + (k%4)`, keystate `0x19`.
- `usbDelay(ms)` L386–392 (pumps full USB service).
- `ntInit()` L394–414: SW reset `{0x00,0x7F,0xFF}`, `usbDelay(500)`, HW_ID==0x55, pin=3, speed=1, buflen=0x30, blank, show, enable keys.
- `ntSoftInit()` L421–438: same without reset (2 ms delays).
- Main-loop recovery/watchdog L4379–4407: 250 ms probe when down (manual 9-clock bus clear L4390–4395 then `ssR1` then `ntSoftInit`), 3 s health check when up.
- LED push cadence L4409–4577: every 80 ms, global brightness `*179>>8` L4561, **frame-diff** `grb_prev` L4565–4568, post-SHOW `System::Delay(1); next_keys = now+3` L4574–4575.
- Keypad FIFO poll L4582–4607: every 20 ms; `kcount = ssR1(0x10,0x04)`; read ≤8 with `ssRN(0x10,0x10)`; `stretch_err` → `recover()` + 250 ms back-off; key decode L4612–4614 `key = (pin/8)*4 + (pin%8)`, edge `3`=press `2`=release.
- Touches: `tbus`, `trel_ok`, `g_en_trel/g_en_keys`, `held_mask`, `fx_held`, `allKeysOff()` (release-all when trellis drops, L4382).
- Port note: keep as `hw_trellis` with an API `trellisInit()`, `trellisSoftInit()`, `trellisShow(const uint8_t grb[48])`, `trellisPollKeys(callback(key,pressed))`, `trellisTick(now)` wrapping the recovery/watchdog. The 90° rotation (spec §10) is a single remap table applied in `trellisShow` (LED index) and in the key decode (L4613) — nowhere else.

### 2.3 OLED — `oled_pmor.h` (whole file) + init in main
- Driver class `pmor::BitBangSSD130xDriver<128,64>` oled_pmor.h L61–345: `Init` L97–138 (MOSI **open-drain PULLUP** L124), `Reset` L140–146, `SendCommand/SendData` L169–186 with `ReleaseBus()` L259–263 (MOSI left high — load-bearing, PB5 shared with trellis SCL), `Update()` L189–213 (dirty-page push + **30-frame `AssertDisplayConfig()` re-assert** L193 + `pmor_usb_service()` between pages L211), `WriteByte` L272–282 with `Nops()`, `AssertDisplayConfig()` L294–304 (`0xA0`,`0xC0`,`0x40`,`0xD3 00`,`0xA8 3F`,`0xA6`,`0x81 01`,`0xAF`), `InitPanel()` L306–339 (contrast `0x01` L330; `0xA0`+`0xC0` L322–324).
- main: config L4041–4070 (`sclk=D8 mosi=D10 cs=D7 dc=D3 reset=D4`, **`nop_delay=48`** L4070), `display.Init(oc)` L4071, **`NVIC_DisableIRQ(SPI1_IRQn)`** L4089, dark first frame L4097–4099.
- Boot ordering rule (comments L3980–3998): pump `tud_task()` until `tud_mounted()` or 3 s (L3994–3998) and again 6 s (L4012–4019) **before** `display.Init`.
- Also keep `dsy_oled_page_yield_hook = nullptr` L3927.
- Touches: `pmor_usb_service` (extern), `tusb.h`. `oledCmdRaw` L3737–3758 is a CDC diagnostic; optional.
- Port note: copy `oled_pmor.h` unchanged into V3; move the `MyOled::Config` block into `hw_oled.cpp::oledInit()`.

### 2.4 SD card — bit-bang SPI + FatFS shim — L3380–3580
- Pins L3384, L3452–3455 (`D26 CLK`, `D27 MOSI`, `D25 MISO` w/ PULLUP, `D28 CS`).
- `spiXfer()` L3395–3421: fast path (no delay) after init; **slow path `DelayUs(5)` — DO NOT CHANGE** L3408–3419.
- `sdCmd()` L3423–3434. `ssd_initialize()` L3441–3449 (**3 retries**, 150 ms + `tud_task()` between). `ssd_initialize_once()` L3451–3523: **250 ms settle** L3465, **160-clock preamble** (20 bytes) L3469, CMD0 L3472–3475, CMD8 L3478–3482, **ACMD41 5 s deadline** with `tud_task()` pump L3488–3499, CMD58 L3502–3512, CMD16 L3515–3518, `ssd_fast=true` L3521.
- `ssd_status/read/write/ioctl` L3525–3576 (read/write pump `pmor_usb_service()` per sector L3535, L3554). `SPISD_Driver` L3578–3580.
- Mount: L4149–4161 `FATFS_LinkDriver(&SPISD_Driver, sdPath)` + `f_mount(&sdFS, sdPath, 1)`; `g_sd_ok` L1255/L4161. Makefile `USE_FATFS = 1`.
- Touches: `hw` (GetPin), `tud_task`, `pmor_usb_service`. Independent of everything else.
- Port note: `hw_sd.cpp` exposing `sdMount()` → bool and `sdOk()`. Patch file I/O (§8) sits above it.

### 2.5 MPU6050 — init L4102–4135, read L4922–4930, watchdog L4945–4968
- Init: `w8(0x68,0x6B,0x80)` reset +100 ms, `w8(0x68,0x6B,0x01)` +50 ms, `r8(0x68,0x75)` ID accepted `{0x71,0x73,0x70,0x68,0x69,0x98}` L4114–4115. Mag probe gated by `has_mag_hw` L4128 (skip for 6050).
- Read (50 ms, L4919–4930): accel `/16384`, gyro `/131`, mirrored to `g_ax..g_gz`. Spec says gyro unused in V3 — keep the read (cheap) or drop the 3 gyro transactions to save ~1 ms/50 ms of bus time.
- Watchdog L4949–4960: 1 s ID probe, reinit after 2 consecutive failures. Mag watchdog L4961–4968 can be dropped in V3 (no mag hardware) — it currently still probes 0x0C every second even on 6050 (`mag_ok` false but the probe reads `wia` regardless).
- Accel→cutoff mapping L4993–5002 (only on FX page): `t=(0.9-axf)/1.1`, `fc = 500*10^t`, `filter_f = 2 sin(π fc/48000)`.
- Touches: `sbus`, `imu_ok`, `mag_ok`, `filter_f`, `cut_t`, `menu`.

### 2.6 VL53L1X — `VL53L1X_mini.cpp/.h` + call sites
- Shims L290–291 (pump + `sbus.w16a/r16a(0x29,…)`). Init L4143–4145 (`lidar.init()` then `startContinuous(100)`). Read L4937–4938 (`dataReady()` gate, `dist`, `fx_lid = dist/600` clamp 1). Watchdog L4975–4983 (one reinit ever). Octave zones L5003–5018 (now unused by the synth — `oct_shift` only shown on DASH L5154; pitch is "lidar unhooked" L1672, L1932).
- Port the driver file as-is for stage 1; overhaul per §11 for stage 8.

### 2.7 Joystick — L453–549, init L4166–4180
- Globals L466–482 (`joy_sw_g`, `g_joy_x/y/sw/nav`, calibration state). `joyAxis()` L488–499 (DZ 3000 of 65535, normalise ±1000). `joyPoll()` L513–549: **200-poll defer then 64-sample average** L515–528; rate-based detent synthesis (accumulator 64000, ~12/s at full tilt) L535–543; SW → `enc2.psw/clicked` L545–548. Called from `AudioCallback` L2203 (750 Hz).
- ADC config L4175–4180: ch0 FSR D15, ch1 joy X D16, ch2 joy Y D17; SW D24 PULLUP L4168–4169.
- Spec change: V3 wants game-console nav (one flick = one step, repeat 8/s after a pause) — replace L535–543 accumulator with an edge+repeat state machine, keep everything else. Usable max raw 950 (spec) is not in code — the current normalisation assumes full 16-bit span (`32768-DZ`), so real deflection tops out ~ ±(950·16−centre)… V3 should normalise against measured ±extent, not 32768.
- Touches: `hw.adc`, `enc2`.

### 2.8 FSR — `_fsr_scale.h` (whole) + main loop L5021–5045
- `FsrScale::Process(raw12)` `_fsr_scale.h` L34–89: auto-learned span, `kCeiling 16/16`, `kNoise 120`, cubed. Main loop: 10 ms sampling into 8-slot buffer, median-of-8 L5025–5035, `fx_fsr = g_fsr.Process(fsr)` L5044.
- Spec says idle raw ≈1600 is zero; the auto-`lo` learning handles that (first touch sets lo≈1600). Keep verbatim.
- Touches: `hw.adc.GetFloat(0)`, `fx_fsr` (volatile, read by ISR).

### 2.9 Encoder — L443–450, L551–566, poll L2203, init L4164
- `Enc` struct, quadrature LUT `ePoll()` L551–558, `eInit()` L559–566 (PULLUP). `enc1` = D0/D1/D2. Detent accumulation `acc1 ±4` L4769–4781 (one physical detent = 4 counts). Keep.

### 2.10 USB / TinyUSB integration
- Files: `usb/*` port unchanged (Makefile L38–56 lists sources; `TINYUSB_DIR` L36 is a Mac path — parametrise).
- Includes L82–87. Boot: `usb_audio_stream_init(); clock_engine_init();` L3916–3917, `board_usb_init(); Delay(500); tusb_init();` L3937–3939, **`tud_disconnect(); Delay(150); tud_connect();`** L3951–3953, mount waits L3994–3998 & L4012–4019.
- Service pump `pmor_usb_service()` L3724–3728 (`tud_task + usb_audio_stream_task + clock_engine_poll`), called from BBus, OLED, SD, lidar, `usbDelay`.
- Main loop head L4329–4374: `tud_task`, `usb_audio_stream_task`, `PollCdc` L3642–3660, DFU check, BPM reconciliation with `clock_engine_set_local_bpm(60000/(step_ms*4))` (master) or `step_ms` from `clock_engine_get_bpm()` (follower) L4346–4365, `clock_engine_poll()`.
- Audio ISR: `clock_engine_note_audio_block(sz)` L2209 (required or MIDI clock never starts), `usbCapturePush` L2377 (no-op with `g_en_usbaudio=false`).
- Descriptors (`usb_descriptors.c`) still advertise UAC2; keep as-is per spec ("port as-is").

### 2.11 DFU bring-up
- Hardware escape at boot L3899–3906: both buttons (D2, D17) low → `ResetToBootloader(DAISY_INFINITE_TIMEOUT)`. **Note:** D17 is now JOY_Y (analog), so this gesture reads the joystick Y wiper as a button — V3 should use D2 + D24 (JOY_SW).
- CDC `dfu` line → `clock_engine_dfu_requested()` → reset L4341–4344 and L4371–4374 (checked before and after the loop gate). `PMOR_STAGE` macro L3851–3881 (only with `make CHUNK=n`).
- Makefile: `APP_TYPE = BOOT_SRAM` L8, `OPT = -Os` L3, bisect defines L11–31.

### 2.12 Boot sequence in `main()` L3883–4318 (order matters — keep)
1. `hw.Configure(); hw.Init(true)` L3885 (480 MHz boost).
2. DFU gesture L3899–3906.
3. USB init + soft disconnect L3916–3954.
4. Mount waits L3994–4019, `g_en_intro=true` L4037.
5. OLED init + SPI1 IRQ off + blank L4041–4099.
6. `sbus.init(D13,D14); sbus.us=3; tbus.init(D11,D12)` L4102–4104.
7. IMU reset/wake/ID L4107–4115; mag probe skip L4128–4135.
8. Lidar init + continuous 100 ms L4143–4145.
9. SD link + mount L4149–4161.
10. enc1 + joystick + ADC L4164–4180.
11. `sessionsBoot()` → `patchLoadSD()` override → `persistApply` → `pstore.Save()` L4186–4194; `rebuildScale()`, `audLoad/fxaLoad/seqaLoad(sess_cur)` L4203–4206.
12. Zero every SDRAM buffer L4210–4223 (SDRAM not cleared at reset), `buildWavetables()` L4224.
13. `SetAudioBlockSize(64); StartAudio(AudioCallback)` L4226–4227.
14. `ntInit()` **last** L4244 (comment L4230).
15. Intro animation L4255–4289 (`usbDelay(30)` per frame).
16. Lava state L4292–4293, loop-hoisted state L4296–4313.
- Boot-time marks `g_t_usb/g_t_oled/g_t_sens/g_t_sd/g_t_boot` L121 (printed over CDC).

---

## 3. FX INVENTORY

**Architecture.** 8 banks × 16 pads = 128 punch-in effects. Pads set bits in `fx_held..fx8_held`
(L4628–4635); `gridTick` resolves live-or-automation into `fx_eff..fx_eff8` every block
(L1850–1867); the ISR reads those masks. `ffsr = fx_fsr_eff` (live FSR or replayed automation
FSR) is the universal depth; `fx_lid` (lidar 0..1, L4938) modulates a handful. Chain order in
the ISR: FX1 buffer/space (L2520–2579) → FX2 (L2582–2738) → FX3 (L2740–2887) → FX4
(L2889–3019) → FX5 (L3021–3107) → FX6 (L3109–3199) → FX7 (L3201–3257) → FX8 (L3259–3313)
→ ordered master chain RING→DIST→FLT→CRUSH (L3315–3353) → master vol (L3357) → hard clip
(L3360). Bank switching: right-encoder turn on the FX page (L4896–4901); rotating banks or
menus clears all held bits (L4776, L4900). Release of any pad clears the bit in all 8 banks
(L4674–4681). Stuck-FX cap 20 s on bank 0 only (L1844–1845).

**Automation.** `fx_auto[8][16]` + `fx_auto_fsr[16]` (L846–847). Length: **16 steps = one bar
of 16ths** — indexed by `gstep & 15` (L1851, L2044) regardless of `songLen()`, so an 8-bar song
replays the same one-bar automation. Recorded while the joystick button is held on the FX page
(`enc2.psw==0 && enc1.psw!=0`, L2040–2046); empty hands scrub the step. Left-hold ≥700 ms on
FX page wipes all banks (L4786–4789). Persisted: bank 0 + FSR in QSPI `Persist` (L1034, 1045),
all 8 banks in `PATCHnn.FXA` (L1395–1432). Spec §6 wants automation spanning the longest
engine loop → make it `[N][MAXSTEPS]` (or per-bar with `gstep % songLen()`).

Quality key: **strong** = keep and polish; **ok** = works, generic; **weak** = cut or rewrite.
"Clocked" = derives timing from `step_ms`/`gstep`.

### FX1 — bank 0 (L833–836; DSP split between gridTick and ISR)

| # | Name | What it does | DSP lines | Modulation | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | ST4 | Grid-locked retrigger of the last step's notes/drums every 1/4 | gridTick L1909–1915, L2085–2110 (fireNote L1923–1942, stutFire L2090–2094) | none | yes | **strong** — true note retrig, drift-free, arms to next subdivision |
| 1 | ST8 | same at 1/8 | same | none | yes | strong |
| 2 | ST16 | same at 1/16 | same | none | yes | strong |
| 3 | ST32 | 1/32: hit + midpoint hit re-anchored to `step_t` | L2097, L2104, L2108 | none | yes | strong |
| 4 | REV | Play last beat of the capture ring backwards, edges faded, ring frozen | L2250–2253, L2526–2539 | none | yes (len = `step_ms*192` = 1 beat) | **ok** — clean; no FSR; anchor is last step boundary |
| 5 | CHOR | Full-mix feedback chorus, ±740-sample sweep, "scream zone" | L2542–2562 (`xcL/xcR[4096]`) | FSR depth 0.45–1, fb 0.55–0.97 | no (0.35 Hz LFO) | **ok** — loud, wet 0.9; can run away at full FSR |
| 6 | PAN | Hard parabolic autopan, ×1.4 makeup | L2565–2571 | FSR speed 0.5→8 Hz | no | ok — makeup gain can clip pre-tanh |
| 7 | CRUSH | S&H rate + amplitude quantiser (24→6 levels) | L2245–2246, L3350–3353 | FSR rate 4→24 & depth | no | ok |
| 8 | HALF | Shadow clock at 0.5×, pitch glides to 0.5 (tape) | gridTick L1893–1903, L2058–2060; `vss` slew L2278; applied L2300, L2476, L2502 | none | yes | **strong** — master clock untouched, snap-back on release |
| 9 | DBL | Shadow clock 2×, pitch 2× | L1896, L2061–2075 | none | yes | strong |
| 10 | BZZ | Generative roll: each 16th picks div {2,3,4,6,8} of the step | L2112–2137 | FSR biases faster divisions | yes | strong — musical, Elektron-ish |
| 11 | MUTE | 4 ms-ish ramped full drop | L2574–2579 | none | no | ok (trivial, keep) |
| 12 | FLT | Resonant BP "DJ sweep" driven by a triangle LFO 200 Hz→3.5 kHz | L3333–3348 | FSR = LFO speed | no | **ok** — spec §6 wants joystick-swept, hold-position; LFO version reads as random |
| 13 | WASH | Forces every voice release into a 3–8 s tail | gridTick L1883–1886 | FSR longer | no | ok (voice-level, nice) |
| 14 | DIST | Asymmetric tanh with DC-offset pre-bias and compensation | L3328–3331 | FSR gain 4→14 | no | ok — moderate; candidate for the §4.4 "FL-style" slot after the 5 ms crossfade rule |
| 15 | STOP | Tape stop: `fx_vs=0.06`, pitch grinds via `vss` slew | L1895–1896, L2278 | none | quasi | **strong** — but rise/fall share one 0.0005 slew (≈40 ms), a slower fall would sell it |

### FX2 — bank 1 (L858–866; ISR L2582–2738 + RING at L3317–3326)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | DUB | Mono dub echo, tanh in the feedback path, time smears on change | L2590–2602 (`f2dub[32768]`) | FSR fb 0.45→1.05 (runaway), lidar time 80→680 ms | no | **strong** — the runaway is the fun |
| 1 | GVERB | 4-comb "reverb" gated by an energy envelope | L2603–2615 (`f2cmb[4096]`) | FSR gate hold | no | **weak** — 4 short combs, metallic; no allpass |
| 2 | TGATE | 16-step trance gate `0xB6D6`, double-time above FSR 0.6 | L2616–2623 | FSR depth / double-time | yes | ok |
| 3 | FLANG | Jet flanger, fast/short biased | L2624–2639 (`fbuf[1024]`) | FSR rate+centre | no | ok |
| 4 | RING | Ring mod 40 Hz→1.5 kHz exp, lidar wobbles the carrier | L3317–3326 | FSR freq+mix, lidar ±30 % | no | **strong** |
| 5 | FOLD | `sin(x·g·π/2)` wavefolder | L2641–2645 | FSR fold gain 1→7 | no | ok |
| 6 | COMB | Tuned comb resonator on a pentatonic table | L2646–2657 (**shares `f2cmb` with GVERB** — both on = corrupt) | lidar pitch, FSR fb | no | ok; fix buffer sharing |
| 7 | VOWEL | Two SVF band-passes morph U→A→I | L2658–2669 | FSR morph | no | ok |
| 8–10 | ISO-LO/MD/HI | One-pole crossovers ~200 Hz / ~3 kHz, kill a band | L2670–2684 | FSR = kill amount | no | **strong** (DJ isolator — and cheap) |
| 11 | PHONE | Band-pass + coarse quantise | L2685–2693 | FSR tightness | no | ok |
| 12 | GRAIN | Hann-windowed grain loop read from the 2 s capture ring | L2694–2704 | FSR size 120→5 ms, lidar scrub position | no | **strong** — this is the spec's "lidar freeze/grain" |
| 13 | WOW | Wow/flutter amplitude wobble + crackle | L2706–2718 | FSR degradation | no | ok (amp only; no pitch wow) |
| 14 | DOWN | Sample-rate reducer | L2719–2725 | FSR hold 2→42 | no | **weak** — duplicates CRUSH/DSTRY |
| 15 | SCAT | Random 16th-slice scatter from the ring | L2726–2737 | FSR chaos | yes | ok |

### FX3 — bank 2, "musical/spatial" (L872–884; ISR L2740–2887)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | PLATE | 4 combs (1789…3119) + one 241-sample allpass | L2746–2755 (`f3plt[24000]`) | FSR decay 0.74→0.95 | no | **weak-ok** — small room, ringy; not a plate |
| 1 | SHIMR | Rectified sign-alternating "octave" fed back into the plate tank | L2756–2764 (**shares `f3plt`**) | FSR octave amount | no | **weak** — the octave-up is a rectifier, sounds like distortion |
| 2 | PONG | Ping-pong at 1.5 steps, tanh fb | L2765–2775 (`f3pngL/R[96000]`) | lidar fb, FSR mix | yes | ok |
| 3 | SLAP | Slapback 60→250 ms | L2776–2782 (`f3slap[12000]`) | FSR time | no | ok |
| 4 | PHASE | 4-stage first-order allpass phaser | L2783–2791 | FSR rate | no | ok |
| 5 | WAH | Envelope-follower SVF wah 300→5500 Hz | L2792–2801 | FSR sensitivity | no | **strong** |
| 6 | TREM | Cosine tremolo at 1/4 or 1/8 | L2802–2810 | FSR depth, >0.6 → 1/8 | yes | ok |
| 7 | HAAS | Haas widener 8–30 samples + M/S boost | L2811–2817 | FSR width | no | ok |
| 8 | VIBRA | Delay-line vibrato 5.5 Hz, collapses to mono | L2818–2826 | FSR depth | no | ok (mono collapse) |
| 9 | DIMEN | Dimension-D style dual-tap chorus | L2827–2838 | FSR depth | no | ok |
| 10 | SPRNG | 3-stage allpass chain in a 50 ms loop, tanh | L2839–2847 (**shares `f3slap`** with SLAP) | FSR fb | no | **weak** — not a spring; spec §4.4 wants a real spring for the drum perf page |
| 11 | SIREN | Sine siren 180→1080 Hz by lidar, FSR wobble+level, tanh | L2848–2855 | lidar pitch, FSR | no | **strong** — spec keeps as the theremin |
| 12 | WARM | tanh + one-pole LP blend | L2856–2863 | FSR drive 1→5 | no | ok |
| 13 | SUB | Env-follower sine sub at fixed ~28.8 Hz (`subph += 0.0006`) | L2864–2870 | FSR level | no | ok — pitch not key-tracked |
| 14 | AIR | HF exciter (tanh of HP) | L2871–2877 | FSR amount | no | ok |
| 15 | PUMP | Beat-synced exponential duck | L2878–2885 | FSR depth | yes | **strong** (sidechain feel) |

### FX4 — bank 3, "experimental/glitch/pitch" (L876–889; ISR L2889–3019)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | OCT+ | Full-wave rectifier "octave up", DC-blocked | L2896–2901 | FSR mix | no | weak |
| 1 | OCT- | Flip-flop octave-down (zero-cross divider) | L2902–2907 | FSR mix | no | ok — fuzz-octave character |
| 2 | DSTRY | S&H + power-of-2 bit quantise | L2908–2913 | FSR hold + bits 8→1 | no | ok |
| 3 | REPT | Beat-repeat 1/2-step slices from `f4rep` | L2914–2924 (`f4rep[48000]`) | FSR probability | yes | ok |
| 4 | RGRN | Grain from `f4rep` | L2925–2932 | FSR size | no | ok (dup of GRAIN, no lidar) |
| 5 | FREEZ | Windowed loop of the last 12→112 ms | L2933–2940 | FSR length | no | ok |
| 6 | METAL | 4 short combs 149…251, scaled | L2941–2948 (`f4mtl[4096]`) | FSR scale | no | ok (metallic resonator) |
| 7 | FRMNT | 2 SVF formant band-passes 500/1500 Hz shifted | L2949–2957 | FSR shift | no | **weak** — duplicate of VOWEL |
| 8 | FSHFT | L/R quadrature rotation at 20→920 Hz | L2958–2964 | FSR rate | no | **weak** — a stereo rotator, not a frequency shifter |
| 9 | FUZZ | Exponential clip with bias, ×0.6 | L2965–2970 | FSR gain 2→20 | no | ok |
| 10 | GSEQ | Random 16-bit gate pattern re-rolled each bar + FSR fill | L2971–2977 | FSR fill | yes | ok |
| 11 | SRAMP | Slice read speed ramps up within each step | L2978–2985 | FSR accel | yes | ok |
| 12 | VSTOP | Buffer-read tape-stop (read head falls behind) | L2986–2993 | FSR speed/damp | no | ok (dup of STOP) |
| 13 | RADIO | BP + AM carrier 60→260 Hz + hiss | L2994–3004 | FSR carrier+hiss | no | ok |
| 14 | NGATE | Noise gate | L3005–3010 | FSR threshold | no | weak |
| 15 | SHMOD | Random S&H amplitude modulation | L3011–3017 | FSR rate | no | ok |

### FX5 — bank 4, "Pocket Operator" (L896–902; ISR L3021–3107)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | RPT2 | 1/2-step beat repeat (mono) | L3027–3032 (`f5rpt2[6000]`) | FSR slice | yes | ok |
| 1 | RPT6 | 0.5–1.5 step repeat (stereo) | L3033–3038 (`f5repL/R[24000]`) | FSR slice | yes | ok |
| 2 | SHUF | Offbeat swing gate | L3039–3043 | FSR amount | yes | weak |
| 3 | Q6/8 | Triplet tremolo | L3044–3048 | FSR depth | yes | ok |
| 4 | PUNCH | Transient-envelope gain boost + tanh | L3049–3052 | FSR amount | no | ok |
| 5 | FILL | 1/4-step retrigger, faster with FSR | L3053–3057 (`f5fill[4000]`) | FSR | yes | ok |
| 6 | BUILD | Riser: HP sweep + gate speed on lidar | L3058–3062 | lidar position | no | ok |
| 7 | BALL | Bouncing-ball accelerating decaying retrig | L3063–3067 (`f5bnc[8000]`) | FSR restart period | no | **strong** (fun) |
| 8 | LPF | Fixed 2-pole LP kill (mono) | L3068–3070 | FSR cutoff | no | ok |
| 9 | HPF | 1-pole HP kill (mono) | L3071–3073 | FSR cutoff | no | ok |
| 10 | SQUSH | tanh limiter with makeup | L3074–3076 | FSR 1→11× | no | ok |
| 11 | SCRTC | Vinyl jog on 1 s buffer | L3077–3082 (`f5scrL/R[48000]`) | FSR speed | no | ok |
| 12 | SCRTF | Faster jog (mono) | L3083–3088 | FSR | no | ok (dup) |
| 13 | GLCH | Random slice-hold | L3089–3096 (**writes `f5scrR`** shared with SCRTC) | FSR probability | no | ok; buffer conflict |
| 14 | BLIND | Hard shutter tremolo 8→68 Hz | L3097–3100 | FSR rate | no | ok |
| 15 | S&H | Random-rate sample-and-hold | L3101–3105 | FSR rate | no | ok |

### FX6 — bank 5, "delay/space/pitch" (L908–914; ISR L3109–3199)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0 | DELAY | Tempo 1/8 stereo delay, tanh fb | L3112–3116 (`f6dL/R[48000]`) | FSR fb 0.2→0.75 | yes | **strong** — the spec's "delay throw" |
| 1 | PNGPG | Cross-fed ping-pong at 1/8 | L3117–3121 (`f6ppL/R`) | FSR fb | yes | strong |
| 2 | FADE | Echo whose feedback rises to 0.85 while held | L3122–3127 (`f6echo[24000]`) | FSR time 1→3 steps | yes | ok |
| 3 | FBACK | Resonant short-loop howl (4–33 ms) | L3128–3132 | FSR pitch/fb | no | ok |
| 4 | DRONE | Capture 250 ms bed then loop it, lidar mixes | L3133–3138 (`f6loopL/R[96000]`) | lidar mix | no | **weak/bug** — `frozen` static never resets: captures once per power cycle |
| 5 | FREEZ | Feedback-held slice | L3139–3144 (`f6echo` shared with FADE) | FSR freeze | no | ok |
| 6 | UNI | Detuned double (read rate 1+0.012·FSR over 6000-sample window) | L3145–3149 (`f6pchL/R[12000]`) | FSR detune | no | **weak** — un-windowed wrap clicks |
| 7 | UNILO | Same, downward | L3150–3154 | FSR | no | weak |
| 8 | DETUN | Two LFO-modulated taps | L3155–3161 | FSR depth | no | ok |
| 9 | PLFO | 6 Hz delay-line vibrato (mono) | L3162–3167 | FSR depth | no | ok |
| 10 | BND+ | Read speed glides up while held | L3168–3172 | FSR target | no | ok |
| 11 | BND- | Glides down | L3173–3177 | FSR | no | ok |
| 12 | WARBL | 7→19 Hz flutter | L3178–3183 | FSR | no | ok |
| 13 | 5TH | `rp+=0.5` = octave-down grain, forces mono (`oR=oL`) | L3184–3188 | FSR mix | no | **weak/wrong** — not a fifth |
| 14 | SUB | Zero-cross sub-octave square-ish | L3189–3193 | FSR level | no | ok |
| 15 | LOOP | Capture and loop up to 16 steps | L3194–3198 | FSR length | yes | ok — `cap` static never resets after first use |

### FX7 — bank 6, "live/performance" (L920–926; ISR L3201–3257)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0–2 | GATE4/8/16 | 50 % duty gates at 4/2/1 sixteenths, phase from playhead | L3208–3212 | none | yes | **strong** (grid-exact) |
| 3 | TRNCE | 16-step pattern `0xB3A6` | L3213–3216 | none | yes | ok |
| 4 | TREM | Sine tremolo at 1/8 | L3217–3220 | FSR depth | yes | ok |
| 5 | PAN | Autopan at 1/4 (×1.4) | L3221–3224 | FSR width | yes | ok |
| 6 | PUMP | Linear duck rising over each beat | L3225–3228 | none | yes | **strong** |
| 7 | SWELL | Reverse of PUMP | L3229–3232 | none | yes | ok |
| 8 | NOISE | White-noise overlay | L3233–3236 | FSR level | no | **weak** |
| 9 | WIDE | Haas 7.5 ms on R | L3237–3240 (`f7dl[2048]`) | none | no | ok |
| 10 | MONO | Sum to mono | L3241 | none | no | **weak — spec says cut** |
| 11 | BRITE | HF tilt up | L3242–3245 | FSR | no | ok |
| 12 | DARK | One-pole LP tilt | L3246–3249 | FSR | no | ok |
| 13 | DRIVE | tanh 1→5× | L3250 | FSR | no | ok |
| 14 | SGATE | Soft 1/8 gate | L3251–3254 | none | yes | ok |
| 15 | KILL | Hard silence | L3255 | none | no | ok |

### FX8 — bank 7, "MIDI/sequence" (L932–938; ISR L3259–3313)

| # | Name | What | Lines | Mod | Clocked | Verdict |
|---|---|---|---|---|---|---|
| 0–3 | RAT2/3/4/8 | Amplitude gates at 1/32, 1/48, 1/64, 1/128 | L3264–3269 | none | yes | ok — gates, not retrigs (real ratchets are FX1 ST32/BZZ) |
| 4 | STRUM | Rising ramp per 1/8 | L3270–3272 | none | yes | ok |
| 5 | TRNC2 | Pattern `0xC5B2` | L3273–3276 | none | yes | weak (dup) |
| 6 | STAB | Squared decay per 1/16 | L3277–3279 | none | yes | ok |
| 7 | RINGL | Ring mod 90 Hz | L3280–3283 | FSR mix | no | ok |
| 8 | RINGH | Ring mod 360 Hz | L3284–3287 | FSR mix | no | ok |
| 9 | VIBR | Amp "vibrato" 7 Hz (really tremolo) | L3288–3291 | FSR depth | no | weak |
| 10 | CHOP | Random gate per step (75 % on) | L3292–3295 | none | yes | ok |
| 11 | HALF | Half-note gate | L3296–3299 | none | yes | ok |
| 12 | DBL | Two pulses per 1/16 | L3300–3303 | none | yes | ok |
| 13 | GLCH | Random 1/32 gate | L3304–3307 | none | yes | ok |
| 14 | REVG | `ph²` swell per 1/16 | L3308–3310 | none | yes | ok |
| 15 | SIL | Silence | L3311 | none | no | weak (dup KILL) |

### Distortion algorithms present (candidates for the §6 locked set + 5 ms crossfade)
1. **DIST** asymmetric tanh + DC bias/compensation — L3328–3331.
2. **FOLD** `sin(x·g·π/2)` sine folder — L2642–2645; **wavefold** triangle reflect (synth voice) — L2331–2347.
3. **FUZZ** exponential `1-e^{-x}` clip with bias — L2965–2970.
4. **WARM** tanh + LP blend — L2857–2863.
5. **DRIVE** plain tanh — L3250; **SQUSH** tanh limiter with makeup — L3074–3076; **PUNCH** envelope-scaled tanh — L3049–3052.
6. **CRUSH** bit + rate — L3350–3353; **DSTRY** S&H + 2^n quantise — L2909–2913; **DOWN** rate only — L2721–2725; **PHONE** BP + quantise — L2687–2693; **RADIO** BP + AM — L2995–3004.
7. Fixed stages: synth drive `tanh(lp·2.0)` L2437–2438; mix `tanh(x·2.0)` L2517–2518; hard clip L3360–3361; per-effect tanh in DUB/PONG/DELAY feedback paths.
- **None is the Fruity-Soft-Clipper parabolic knee** described in `research/clipper-and-posync.md` §A.3/A.5 — that must be written new for the §9 output stage.

### Cross-cutting FX defects to not carry forward
- Shared buffers with no arbitration: `f2cmb` (GVERB/COMB), `f3plt` (PLATE/SHIMR), `f3slap` (SLAP/SPRNG), `f6echo` (FADE/FREEZ), `f5scrR` (SCRTC/GLCH), `f6pchL` (UNI/5TH).
- Statics that never reset: DRONE `frozen`, LOOP `cap`, BALL `per`.
- No anti-click on engage/release except MUTE, SGATE and the gate envelopes; spec §6 demands fades on every on/off.
- Automation is 16 steps only (see above).
- Many effects collapse to mono (`oL=oR`) — fine for glitch, wrong for spatial.

---

## 4. SYNTH ENGINE

### Algorithms and where they render (all inside the voice loop L2286–2428)
| `g_wave` | Name | `Voice.type` | Character set in `voiceCharacter()` | Render |
|---|---|---|---|---|
| 0 | **SAW** | 10 | L1127–1137 mono, `nosc=1`, gentle pluck | PolyBLEP saw L2389–2391 |
| 1 | SQUARE | 11 | same block | PolyBLEP square L2392–2396 |
| 2 | TRI | 12 | same | PolyBLAMP tri L2402–2406 |
| 3 | SINE | 13 | same | `psin` L2407 |
| 4 | PULSE | 14 | same | PolyBLEP 25 % pulse L2397–2401 |
| 5 | **THIEF** | 0 / 1 / 2 (random per note) | L1219–1241: 30 % FM pluck (harmonic ratios `FMR[6]` L1223), 55 % naive saw stack, 15 % naive tri stack; random VCF fc/fcT/fcR; 15 % swell, 10 % glide/drift | naive saw `default:` L2409, tri L2408, 5 detuned oscs `DETUNE[5]` L616 |
| 6 | FM | 2 | L1206–1218: random ratio (whole 1–6 ⅔ of the time else 0.5–8), index 0.05–0.75 decaying | 2-op phase-mod L2303–2311 |
| 7 | WT | 20 | L1138–1148: detuned trio, morph = `g_wtscan` + env | table interp L2312–2330 |
| 8 | FOLD | 23 | L1149–1160 | sine → triangle fold L2331–2347 |
| 9 | PLUCK | 24 | L1161–1169 (`kinit`) | Karplus-Strong L2348–2364 (`kpbuf[12][672]` SDRAM L632) |
| 10 | SUPER | 10 | L1170–1181: 5 PolyBLEP saws, spread = `g_wtscan` | as SAW with `nosc=5` |
| 11 | CZ | 26 | L1182–1192 | phase distortion L2365–2376 |
| 12 | ARCADE | 14 | L1193–1205: mono pulse, filter parked open, instant attack | as PULSE |

**The plain-saw entry to remove** (spec §3.1 "Thief (saw/tri stack, remove its plain-saw entry)"):
two readings — (a) `WAVE_NAMES[0] "SAW"` L628, the mono band-limited saw (`type 10`, L1127–1137)
that sits as a separate algorithm; (b) inside THIEF itself, `V.type = 0` naive-saw stack chosen
55 % of the time (L1229, rendered by `default:` L2409). Reading (a) matches "entry"; if EMGOR
means the Thief's own saw *stack*, the change is L1229 → `V.type = 1` (tri only) or FM-only.
Confirm with him; either is a one-line change.

### Voice struct — L677–691
`phase[5]`, `ratio[5]`, `phaseInc`, `env`, `envTarget`, `pan`, `relCoef`, `atk`, `drift/driftT`,
`type`, per-voice SVF `fc/fcT/fcR/q/lp/bp`, FM `fmi/fmiT`, Karplus `kpos/klen/kinit`, `nosc`,
pitch-rise `prise/pmul/pmulT`. **Voice count: 12** (`voices[12]` L715; the header comment at
L569 saying "4 voices" is stale). Gate-time cap `vgate_t[12]` L714 (8 s at L1823–1825).

### Envelope shape — L2289–2290
One-pole exponential toward `envTarget`: `env += (target−env)·coef`, coef = `atk` while rising
(0.0025 swell / 0.012 / 0.08 / 0.5 instant — all set in `voiceCharacter`) else `relCoef`
(generative: `0.000048·30^r` L1939; manual: 0.0004 L1676; WASH overrides L1885). Generative
notes hold 100 ms (`vrel = now+100` L1941) then release. **No sustain/decay stages** — this is
AR, not ADSR. V3's ADSR (spec §3.1) is new code.

### Filters
- Per-voice Chamberlin SVF L2416–2423: `fc` slews from `fc` to `fcT` at `fcR` per sample (the
  "VCF envelope"), damping `q` 0.70–1.05 (L1124), states hard-clamped ±6. Key-tracking
  `filterTrack()` L1655–1661 (×0.5…2.6 around A3, cap 0.9).
- Master Chamberlin SVF (accel cutoff) L2429–2434, `Q1=0.8` L2213, `ff_s` slews toward
  `filter_f` L2429; drums get an identical copy L2509–2512. Output taken from `lp` (LP only);
  the comment "FLT swaps in bandpass" is stale — FX_FLT is a separate post-mix SVF (L3333).
- NaN flush L2257–2261.
- Spec wants cutoff × resonance 2-D and keytracked: the per-voice SVF already has both knobs; the
  master SVF's Q is fixed.

### Glide / pitch motion
- `drift` slews toward `driftT` at 0.00002/sample (L2291): krell drift ±0.8 % (L1121) or 10 %
  "glide notes" ±6–17 % over the note (L1240–1241).
- Pitch-rise `prise` (chance `g_prise`): `pmul` walks 0.5→1 at 0.5/48000 per sample ≈ 1 s per
  octave (L2294–2299); on release walks back to 0.5. This is the "Thief-style slide" the spec's
  performance page wants for joystick-Y octave glide (reuse `pmul/pmulT` with a settable slew).
- Varispeed `vss` (HALF/DBL/STOP) multiplies every increment L2300.
- **No portamento between successive notes** exists.

### Pan
- Manual notes: by column `pan = 0.2 + 0.6·(k%4)/3` L1678. Generative: random 0.15–0.85 L1937.
- Mix: `mixL += sig·pan; mixR += sig·(1−pan)` L2426–2427 — **deliberately swapped** for this
  unit's mirrored L/R (header L52–53). Drums use the opposite convention (`panl = 1−pan`
  L2470) so drums and synth pan in opposite directions for the same `pan` value — V3 must pick
  one convention in the mixer and apply it once.
- Spec §3.2's poly-2 (25/75) and poly-3 (15/50/85) fixed positions map directly onto `Voice.pan`.

### Scale / key generation — L590–615
`MODES[9][7]` (Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Harmonic minor,
Major pentatonic padded `{0,2,4,7,9,9,9}`), `MODE_NAMES` L602. `rebuildScale()` fills
`FLYD[28]` = 4 octaves × 7 degrees from root F3 174.61 Hz × 2^(g_key/12). `g_key` clamps ±24
(L791) — spec wants continuous ±12 st: make `g_key` float and drop the `int` cast. Pad→note
`note = (3−k/4)·4 + k%4` (bottom row lowest, L1668, L1974), then `phaseInc = FLYD[note]·0.5/48000`
(one octave down, L1672/L1932). Generative picks `lcg()%28` across all 4 octaves (L1964–1966).
Drums reuse `g_key` for kick tuning (L1788).

### Sequencing modes present (for §3.2 mapping)
`SYN_SEQ` polyphonic 16-bit-mask steps `syn_seq[MAXSTEPS]` (L751, fired L1970–1975), `SYN_ARP`
random walk over the union of sequenced notes (L1978–1992 — not a real arpeggiator),
`SYN_GEN` density/poly generative (L1960–1969, `g_csyn`, `g_cpoly`). Live record: hold joystick
button in the SEQ editor writes `held_mask` per step (L2050–2053); place-mode chord entry
L4841–4845. Voice allocation: free-voice scan then round-robin (L1663–1665, L1924–1928) — hard
steal, no 20 ms release (spec §3.2 poly-1 rule is new).

---

## 5. DRUM ENGINE

- **Synthesis** (ISR L2456–2486): 4 render channels `drun[4]`, one per pad row (`ch = 3−k/4`,
  L1791). Each hit = sine at `f` gliding from `ft·drop` down to `ft` (one-pole 0.0012/sample,
  L2475) + optional 2nd partial `part` (L2478–2479) + LCG white noise (L2480–2481, one-pole HP
  when `noiz>0.97` for hats L2482), single exponential envelope `env *= dec` (L2474). Latched
  from `DrumGo dgo[4]` on `stamp` change (L2458–2472, clamps 5 Hz–16 kHz). Then the drum SVF
  copy (L2509–2512) and ×1.5 into the mix tanh (L2517). **Retriggering a row cuts the previous
  hit** (monophonic per row). No click transient generator, no pitch envelope beyond the
  one-pole drop, no filtered-noise snare body, no multi-partial hats — the §4 64-sound library
  (research/drum-synthesis.md) is a full rewrite.
- **Kits**: `struct DrumDef {f, amp, dec, noiz, drop, pan, part}` L1710; `KIT[16]` L1716–1740
  (HCL HMD HOP RIDE / SNP1 SNP2 SNRF CHOP / TOMM TOMH BELL BLIP / KCK1 KCK2 KCK3 K808),
  `KIT2[16]` L1749–1770 (EHAT1-3 ECYM / CLAP ESNR RIM ZAP / LZTOM FMBEL MTL COSMO / K909 KCLUB
  KSUB KHARD), names `PADN/PADN2` L1741, L1771, selector `kit_sel` L667.
- **Pad mapping**: key 0 = top-left; row 0 (top) = hats/ch3, row 1 snares/ch2, row 2 percs/ch1,
  row 3 (bottom) kicks/ch0. Spec §4.1 wants quadrants (BL kicks, BR snares, TR hats, TL perc) —
  new mapping table.
- `drumPadHit()` L1776–1793: random f/pan for `f<=1` "blip" entries; kicks (k≥12) retuned to
  `43.65·2^(g_key/12)·(f/52)` (L1787–1790 — kick = root, matches spec). `live_dr` latch for
  stutters.
- **Sequencer data**: `dr_pat[4][16]` bytes = 128-bit mask per *row/channel* (L1081), not per
  pad; `dr_pad[4]` remembers which flavour pad each row last played (L1082, set L4668). So only
  4 lanes exist; spec §4.3 wants 16 independent lanes → `dr_pat[16][16]`. Step firing
  L1954–1956, L1994–1996 with `g_cdrum` chance. Place-mode toggling L4649–4657; clear L4793–4795.
- **Panning**: static per `DrumDef.pan` (hats ~0.6, kicks 0.5), blip random 0.1–0.9. Drums
  `panl=1−pan` (see §4 note on the swapped synth convention).
- Euclid leftovers (`EUK`, `dr_k`, `dr_rot`, `euclidHit`) L1244–1253, L1795–1801 are dead —
  no caller fires them (Euclid mode was removed, L744).

---

## 6. SAMPLER

- Buffers: `smp[16][SMPL_N=96000]` floats in SDRAM = **2 s mono per slot, 6 MB total** (L1016–1017);
  `smp_len[16]`, `rec_slot`, `rec_pos`, `smp_trig[16]` L1018–1021.
- Record path: pad press stamps `smp_press[k]` (L4663); gridTick arms `rec_slot` after **300 ms
  hold** (L1870–1879) — so recording starts 300 ms late, not on press (spec §5 wants
  press-exact); ISR writes `in[0][i]` while `rsl>=0` (L2489–2492); release finalises
  `smp_len = rec_pos`, sets `aud_dirty` (L4692–4696). No fades. Re-recording over a filled slot
  is allowed today (spec wants 5 s-hold delete first).
- Play path: `smp_trig` bump on short release (L4697–4700) or step (L2033–2035, `g_csmp`
  chance); ISR L2493–2506 plays at `vss` varispeed, **gain `0.9·gvSmp`** (L2501) into the drum
  bus (pre drum-SVF, pre ×1.5 → effectively ×1.35 at the mix tanh — this is the "hot" level
  spec §9 wants tamed). No pitch/formant/filter/reverse.
- Slots: 16 (one per pad). Pattern `smp_pat[16][16]` (L1022).
- SD format `PATCHnn.AUD` (L1292–1393): `[AUD_MAGIC 'PAUD' 0x44554150][ver=1][uint32 lens[16]]`
  then int16 PCM for each non-empty slot in order, then trailing `AUD_MAGIC`. Written 1024
  samples per main-loop pass (`audSaveTick` L1323–1352) to `.TAU`, promoted to `.AUD` with
  `.ABK` backup on completion. Load L1363–1393 (partial read keeps what loaded). Triggered by
  `aud_dirty && !aud_saving && rec_slot<0` L5075–5078; forced flush on patch switch
  L1359–1362.
- Trellis feedback: filled = teal `{10,55,42}`, recording = red L4491–4499.

---

## 7. SEQUENCER / CLOCK

- `gridTick(now)` L1810–2138 runs once per 64-sample block from the ISR (L2210). Master step
  deadline `next_step` L2015–2026: absolute, `step_ms` per 16th, swing (`g_swing/9·0.34`
  lengthens odd steps, average preserved) L2020–2025; catch-up guard L2026; `sct` counter,
  `gstep = (gstep+1) % songLen()` L2028; ring anchor `step_rw = rb_w` L2029.
- `step_ms` L733 (default 125 = 120 BPM), clamped 60–500 ms (250–30 BPM) at L1510–1511 and tap.
  BPM display `15000/step_ms` (integer ms → BPM is quantised unevenly, e.g. 125→120, 124→120.97).
  Spec's unquantized tap needs a float period (or samples-per-step) instead of `uint32 step_ms`.
- Layout: `MAXSTEPS 128` L748; `songLen() = form_blen × form_bars` L779–783 with `form_blen`
  1–16, `form_bars` clamped **8** in `songAdj` L802 and `seqaLoadFile` L1470 but **16** in
  `persistApply` L1545 and form-mode pads L4643 (inconsistent). Bar view auto-follows
  (`view_page = gstep>>4` L4479, L4513).
- Sub-step phase for gate FX: `gstep_samp` reset per block on step change L2274, `gslen =
  step_ms·48` L2275.
- Tap tempo L4881–4894: quarter-note taps, window 240–2000 ms, average of last ≤4, `step_ms =
  avg/4`; out-of-range resets. No phase snap. Cursor must be on the TAP tile. Spec §1 moves tap
  to the encoder click with rolling N 2→5, 2 s expiry, phase snap — new code, but the
  `next_step = tap_time + period` snap is a one-liner given `next_step` is static inside
  gridTick (hoist it).
- Clock out: USB MIDI 24 PPQN from `clock_engine` (`master_tick_poll` clock_engine.c L846),
  BPM handed over each loop L4352. Clock in: follower PLL, `step_ms` written from bus BPM
  L4358–4364.
- **PO-sync**: click generator + SY0–SY5 routing + 2 PPQN sync-in edge detector exist in
  `clock_engine.c` (`render_click_sample` L422–435, `start_click` L437–441, routing inside
  `clock_engine_process_audio` L515–657, sync-in `po_sync_in_record_interval` L387–420, CDC
  `SY0..SY5`/`WAVE0/1` L996+). **Unwired**: `clock_engine_process_audio()` is never called
  (comment L2204–2208), so no click reaches the codec and no sync-in is read. V3 §8 CLOCK OUT
  MODE: call it (or lift the click renderer) from the ISR on the output jack per
  `research/clipper-and-posync.md` §B.
- Safety code to keep: gate cap L1823–1825, orphan sweep L1829–1836, stuck-note/FX L1840–1846,
  trellis-down release L4382.

---

## 8. SESSIONS / PATCHES

- `struct Persist` L1030–1048 (step_ms, 16-step `dr_pat[4]`, `smp_pat[16]`, `fx_auto[16]`,
  `dr_pad/dr_k/dr_rot`, euclid, `seq_mode[3]`, `vol_pidx`, menu/sel, globals, `g_wave`,
  `kit_sel`, form, `fx_auto_fsr[16]`, `rsvd[32]` — rsvd[0..2] = engine vols +1, [19] = fx_bank
  +1, [20] = wtscan +1, L1500–1506).
- `struct SessionBank` L1050–1057: `magic`, `exists` bitmask, `active`, `Persist s[16]`.
  `SESS_MAGIC = 0x4F504734 'OPG4'` L1062; `static_assert(sizeof == 2568)` L1073. Storage:
  QSPI via `PersistentStorage<SessionBank> pstore(hw.qspi)` L1254, plus SD mirror `PATCHES.BIN`
  / `.BAK` / `.TMP` (L1262–1290, SD wins at boot L4188–4194).
- Sidecars per patch on SD: `PATCHnn.AUD` (audio), `PATCHnn.FXA` (all-bank automation,
  `'PAXF'`), `PATCHnn.SEQ` (`'PSEQ'`: bars, beats, `dr_pat`, `smp_pat`, `syn_seq`). All
  temp→rename→backup atomic.
- Slots: 16 (one page); spec wants 4 pages × 16 = 64 → `SessionBank.s[64]` (10 KB) or one file
  per patch. Slot 0 undeletable (L1637). Gestures L5046–5060: tap <800 ms = load (L4685–4689),
  hold 3 s on empty = new, hold 10 s on existing = delete with warning from 5 s.
- Autosave: `markDirty()` L1075 sets `save_at = now+3000`, `save_state=1`; fires L5063–5071
  (`sessionStash`, `pstore.Save`, `patchSaveSD`, `fxaSave`, `seqaSave`). Spec: ~2 s.
- **Saving star drawing — keep exactly**: L5446–5465. Busy spinner L5449–5455 (`cx=124,cy=4`
  orbiting pixel), 5-point star L5457–5464 (`cx=123, cy=4`: two pixels + three lines).
  Condition `!saver && !vol_mode`, busy = `save_state || aud_saving || aud_dirty`.
- `sessionsBoot()` L1556–1578 zeroes all pattern data in the default seed (v2 clean slate);
  `plantExampleBeatIfRoomExists()` L3608–3640 is disabled (L4202).

---

## 9. UI

### Menu enum and navigation
- `enum { MENU_SYNTH=0, MENU_DRUM, MENU_SMP, MENU_FX, MENU_SESS, MENU_DASH, MENU_GLOB,
  MENU_COUNT=7 }` L727–728; names for the dive transition `MN[]` L5125 ("SYNTH","DRUMS",
  "SAMPLER","FX","PATCHES","DASH","GLOBALS"). Rotation wraps (`% MENU_COUNT` L4772) — spec
  wants hard stops; ring order changes to DRUMS·SYNTH·SAMPLER·FX·PATCHES·GLOBALS·DASH.
- Left encoder: turn = menu (±4 counts) L4769–4781 (also kills FX, resets sub-states);
  click = back L4802–4807; hold ≥700 ms = clear L4783–4800; both buttons = volume mode
  L4733–4756 (130 ms click deferral, `both_latch`). Right (joystick): context branches
  L4809–4914 (form mode, drum/smp seq view, synth seq view, engine pages with
  `[modes][params][TAP]` cursor L4847–4895, FX bank L4896–4901, globals L4902–4913).
- `last_enc` screensaver timer L732, wakes on pad L4617 and any encoder activity L4725–4726;
  saver after **25 s** (L5088, L5095 — header says 10 s, stale).
- Dive transition L5111–5128 (6 frames of nested rects).

### Menu-map strip to remove
- Top-right pips: L5439–5443 (`DrawRect(92+i*4, 0, 94+i*4, 2, …)` for each menu). Nothing else
  depends on it. (The FX page also draws `"\x11 FX%d \x10"` at L5299–5301 as a bank header.)

### Dashboard rows — L5129–5161
Row y=0 accel `A:±x.x ±y.y ±z.z`; y=9 gyro `G:`; y=18 joystick scaled + `C` click + raw hex
ADC (`r%04X/%04X`); y=27 lidar `D:mm S:status O:oct`; y=36 `ENC/JOY` activity strings;
y=45 output meter 19 chars; y=54 `F:fsr SD:OK/-- K:heldcount`. Spec: drop gyro row (unused)
and the raw joy hex once calibrated, add SD/USB/clock-role status.

### Screensaver / "fractal" — L5162–5207
Harmonograph: two counter-rotating Lissajous loops of 110 segments, size `R = 8 + out_amp·40`
capped 30 (pixel-budget note L5183–5188: >~half panel lit killed USB on the old power
arrangement), accel sets frequency ratios, gyro sets spin speed. Spec §10 replaces with
amplitude-normalised outline particles (≤96, fixed-point, every other frame) — keep the R cap
philosophy in mind for OLED current. Border particles during play L5416–5437 (`np =
out_amp·16`, mirrored on all four edges; skipped on FX page). Intro tunnel L4255–4289.

### Page renderers (for reference / partial reuse)
Volume overlay L5097–5110; form editor L5208–5215; sampler seq info L5216–5230; drum seq info
L5231–5248; SYNTH page L5249–5289 (3 mode tiles + 8-row scrolling param list with highlight
bar — the "single-column param list" pattern L5266–5287 is worth keeping as the 1-D list
widget); FX page L5290–5319 (active names + FSR bar + REC badge); SAMPLER L5320–5348; PATCHES
grid 4×4 with page letters L5349–5371; GLOBALS L5372–5387; DRUMS L5388–5414. Text helper
`orow()` L3586–3588, `Font_6x8` everywhere, `Font_7x10` only in `PMOR_STAGE`.

### Trellis colour tables / palette constants
- Lava palette `lavaColor()` **L316–331**: 8 keys (light blue 70,150,255 / soft blue 75,175,240
  / light yellow 240,235,75 / warm yellow 255,215,65 / cool yellow 245,240,80 / light blue
  80,150,255 / blue-purple 155,65,245 / light purple 185,65,220), linear interpolation over an
  8-bit phase. Per-pad drift `DRIFT[16]` L4292, phases `pphase[16]` L4293, advanced every LED
  frame L4413.
- Idle lava animation L4539–4558: `lavaColor >> 5` (~3 %) idle; held pad ramps with raw FSR
  1200→2600 (`>>2` at floor) — note this uses raw `fsr`, not `fx_fsr`, and the 1200/2600 window
  is stale for the 330 R pulldown.
- Global brightness scale `*179>>8` (70 %) L4561. Spec: default noticeably lower, 0 → near max.
- FX bank palettes `FXC[8][4][3]` L4445–4453 (one hue per row per bank; idle = `>>4`).
- Drum lane colours `VCOL[4][3]` L4504–4509 (kick 255,60,40 / perc 200,60,255 / snare
  255,200,40 / hat 60,220,255).
- Volume fader L4415–4429: ≤pad 9 green `{30,120,20}`, 10–12 amber `{120,90,10}`, 13–15 red
  `{160,25,10}`, set pad +60/+60/+30.
- Form mode L4430–4442 (cyan `{10,90,120}`, current bar white 200).
- Patches L4464–4474 (warn red, active white 200, exists amber `{120,70,10}`).
- Sampler L4475–4500 (teal `{40,220,170}` steps / `{10,55,42}` filled, rec red `{220,30,20}`,
  playhead grey 140).
- Synth editor L4524–4538 (armed `{200,200,255}`, sounding `{120,180,255}`, edit-step
  `{20,40,120}`).
- All of this gets replaced by the mint-anchored palette (spec §10), but the *structure* —
  lava base layer + per-page overlay + brightness scaler + frame diff — should be kept as the
  `ui_trellis` module's render pipeline.

---

## 10. VOLUME / OUTPUT

- `kVolTable[16]` L1097–1103: 0 = silence; pads 1–12 span −40 dB…0 dB in 3.64 dB steps; 13–15 =
  1.0772/1.1604/1.25. Selected by pad in volume mode L4619–4623; `vol_pidx` default **10**
  (`mvol = 0.43287`, −7.27 dB) L1110–1111. Display percent `pct = (vol_pidx·125+7)/15`
  (L5099): pad 9 = 75 %, pad 12 = 100 %, pad 15 = 125 %.
- **Where 75 % sits numerically**: pad 9 → `kVolTable[9] = 0.28480` (−10.91 dB). Spec §9 "0–75 %
  of today's range is the new 0–100 %" ⇒ the new fader's top = 0.2848 linear at the master
  stage, pushing higher only into the clipper. (Today's default pad 10 = 83 % on that display.)
- Signal path gains: synth `sig = lp·0.05·env·gvSyn` L2424; synth drive `tanh(lp·2.0)` L2437;
  chorus wet 0 (`cwet=0` L2265 — the header's "chorus 0.55" is stale); drums `×gvDrum` L2484
  then `×1.5`; sampler `0.9·gvSmp` into the drum bus L2501; mix `tanh((dL·0.8 + drm·1.5)·2.0)`
  L2517–2518 (`mgain=2.0` L2269); master `×mvol` **after all FX** L3357; hard clip ±1
  L3360–3361. Engine volumes `engVol(v) = v·0.10`, 0–120 % (L668–671; spec: 0–125 %).
- Tanh limiter: the mix-stage tanh (L2517) is the only "limiter"; there is no end-stage clipper
  after the FX — FX feedback can exceed ±1 and is hard-clipped. Spec's always-on soft-knee
  clipper goes between L3357 and L3360.

---

## 11. LIDAR

### Current configuration (`VL53L1X_mini.cpp`)
- `init()` L144–176: ID check `0xEACC`, soft reset, firmware-ready poll ≤100 ms (pumped), I2C HV
  extsup, `fast_osc`/`osc_cal`, DSS target rate `0x0A1C`, GPIO active-low, sigma/ambient widths,
  crosstalk/range-ignore/min-clip/consistency, thresholds 0, aperture 0x38, sigma thresh 360,
  min count 192, GPH, quantifier, seed, sequence `0x8B`, **`DSS_MANUAL_SPADS = 200<<8` and
  `DSS_ROI_MODE = 0x02`** (manual "requested effective SPADs"), **distance mode LONG**
  (`setTimingBudget50msLong` L128–142: VCSEL A 0x0F / B 0x0D, valid phase 0xB8, WOI 0x0F/0x0D,
  init phase 14/14), **timing budget 50 ms**, part-to-part offset = `MM_OUTER_OFFSET·4`.
- **No ROI written** (registers 0x007F/0x0080 untouched → default full 16×16 array).
- `startContinuous(100)` L177–181: timed mode `0x40`, 100 ms inter-measurement.
- `read()` L182–196: non-blocking; status = reg `0x0089 & 0x1F`; **accepts only raw status 9
  (RANGE_COMPLETE), 6 (SIGMA_THRESHOLD), 8 (MIN_CLIP)** with `dist>0`; 7 (PHASE_CONSISTENCY /
  wrap) returns the last good value; **anything else returns 0** — including 4 (MSRC no
  target / signal), 5 (range phase check / out of bounds), 12 (range-ignore threshold /
  crosstalk), 13 (user ROI clip), 18 (stream count). Main loop then maps `dist==0` to
  "hand gone" (L5008) and `fx_lid=0` (L4938).

### Hypothesis for "reads 0 at ~4 in, fine further out"
1. **Missing dynamic SPAD update (most likely).** The driver puts DSS in *manual requested
   effective SPADs* mode (`ROI_MODE 0x02`, 200 SPADs) — exactly what the Pololu driver does —
   but Pololu then calls `updateDSS()` after **every** measurement to recompute
   `DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT` from the measured signal/ambient rate so the
   return rate stays near the target (`0x0A1C`). This driver never rewrites it. At 100 mm the
   return signal from a hand is orders of magnitude stronger than at 500 mm; with 200 SPADs
   fixed the pile-up/rate checks fail and the sensor reports a non-valid status (4/5/12) →
   `read()` returns 0. Far targets happen to fall inside the acceptable rate window, so it
   "works further out". The other MCU presumably ran ST's ULD or Pololu, both of which
   adapt. **Fix**: read `RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0` (0x008C), peak signal
   (0x0098) and ambient (0x0090) in the same 17-byte burst from 0x0089, then port
   `updateDSS()`; also port Pololu's one-time `setupManualCalibration()` (VHV init disable,
   loop bound, phasecal override) after the first result.
2. **Status filter too strict**: even with DSS fixed, treat the raw status per Pololu's table
   and log it — the DASH row `S:%02X` (L5154) already shows the raw code; capture what it reads
   at 4 in before changing anything. If it reads `0x05` or `0x0C`, hypothesis 1 is confirmed;
   `0x04` means signal/pile-up.
3. **Distance mode**: LONG is fine to ~4 cm on paper, but SHORT mode (VCSEL A 0x07 / B 0x05,
   valid phase 0x38, WOI 0x07/0x05, init phase 6/6) is the mode ST recommends under 1.3 m in
   ambient light and has more tolerant phase-check behaviour at close range. For a theremin
   the useful span is 5–60 cm: switch to SHORT with a 20–33 ms budget.
4. Timing budget 50 ms / IM 100 ms is not the cause of zeros, but 100 ms IM = 10 Hz updates,
   sluggish for a siren; 33 ms budget + 40 ms IM is ~25 Hz.
5. ROI is default (full FOV 27°) — irrelevant to zeros, but a 8×8 centre ROI narrows the beam
   for hand tracking; write `ROI_CONFIG__USER_ROI_CENTRE_SPAD 0x007F = 199` and
   `…REQUESTED_GLOBAL_XY_SIZE 0x0080 = 0x77`.
- Boot behaviour: `lidar.init()` failing only sets `lidar_ok=false` (L4144) — boot already
  continues; the "skip-on-fail [?]" is effectively how it works, except the init's 100 ms poll
  and ~40 pumped register ops when the sensor is absent cost a few seconds of bus timeouts.

---

## 12. MEMORY / FLASH

### SDRAM (`DSY_SDRAM_BSS`, 64 MB available) — 10.7 MB used
| Buffer | Elements | Bytes | Lines |
|---|---|---|---|
| `smp[16][96000]` sampler | 1,536,000 f | 6,144,000 | L1017 |
| `rbufL/R[96000]` capture ring | 192,000 | 768,000 | L950–952 |
| `f3pngL/R[96000]` ping-pong | 192,000 | 768,000 | L960, 964–965 |
| `f6loopL/R[96000]` drone/loop | 192,000 | 768,000 | L984–985 |
| `f6dL/R`, `f6ppL/R` [48000] | 192,000 | 768,000 | L979–982 |
| `f5scrL/R[48000]` | 96,000 | 384,000 | L974–975 |
| `f4rep[48000]` | 48,000 | 192,000 | L969 |
| `f5repL/R[24000]` | 48,000 | 192,000 | L972–973 |
| `f2dub[32768]` | 32,768 | 131,072 | L956 |
| `f3plt[24000]`, `f6echo[24000]` | 48,000 | 192,000 | L962, 983 |
| `f6pchL/R[12000]`, `f3slap[12000]` | 36,000 | 144,000 | L965, 986–987 |
| small (`kpbuf` 8064, `wtbl` 2048, `f2cmb`/`f4mtl`/`f6fb`/`f6plfo`/`f6bu`/`f6bd`/`f6wrb` 4096 ea, `f7dl`/`f6dtL/R` 2048 ea, `f5rpt2` 6000, `f5fill` 4000, `f5bnc` 8000) | ~62,900 | ~251,600 | L632, 640, 957, 970, 977, 989–998 |
| **Total** | | **≈10.70 MB** | |
All zeroed at boot L4210–4223 (SDRAM is not cleared by reset). Cost of the memset is small.

### Internal RAM (statics outside SDRAM, approximate)
- ISR statics: `xcL/xcR[4096]` 32 KB (L2240), `cbufL/R[1024]` 8 KB (L2219), `fbuf`/`vbuf`/`cbuf`
  [1024] 12 KB (L2625, 2818, 2827), `plapbuf[512]` 2 KB, `hbuf[256]` 1 KB, USB capture ring
  1.5 KB (L2157–2158) → **~57 KB**.
- Data: `syn_seq` 256 B, `fx_auto` 256 B, `smp_pat` 256 B, `dr_pat` 64 B, `aud_buf` 2 KB
  (L1304), `voices[12]` ~1.5 KB, OLED framebuffer 1 KB + dirty, `SessionBank` in `pstore`
  2.5 KB (+2.5 KB stack copy in `patchLoadFile`), FatFS `sdFS` ~0.6 KB + `FIL` objects
  (~0.6 KB each, several on stack), `row[32]`, misc → **~12 KB**.
- TinyUSB buffers in `.sram1_bss` (D2, non-cacheable) per `usb_dma_mem.h`; UAC2 FIFO sized in
  `tusb_config.h`.
- Estimate: **~70 KB static + stack**, comfortably inside the 128 KB DTCM + 512 KB AXI. The
  comment at L988 ("too big for the 128 KB DTCM") is why the work buffers went to SDRAM.
- **Flash/SRAM budget**: `APP_TYPE = BOOT_SRAM` — code *and* data live in AXI SRAM (~480 KB
  ceiling per Makefile L4–5). The 2026-09-13 DFU log in PMOR-V2-STATE.md §11 shows the app
  ending at `0x90053d6b`, i.e. ≈ 81 KB image at that build (with `-Os`). Plenty of headroom; the
  128 FX bodies are the bulk of the code.

---

## 13. RECOMMENDED MODULE SPLIT for PMOR_V3

```
PMOR_V3/
├── Makefile                 from V2 Makefile L1–68; TINYUSB_DIR parametrised; add new .cpp files
├── main.cpp                 boot order §2.12 (~150 lines), main loop scheduler calling modules
├── config.h                 pins, rates, buffer sizes, MAXSTEPS, palettes (one place)
│
├── hw/                      VERBATIM PORTS
│   ├── usb_service.h/.cpp   pmor_usb_service(), usbDelay()               L386–392, L3724–3728
│   ├── bbus.h/.cpp          struct BBus + tbus/sbus                      L137–281
│   ├── trellis.h/.cpp       ntInit/ntSoftInit/ntShow/ntEnableKeys, recovery+watchdog tick,
│   │                        key FIFO poll → callback, rotation remap    L293–438, L4376–4614
│   ├── oled_pmor.h          unchanged                                    oled_pmor.h
│   ├── oled.h/.cpp          MyOled + oledInit() (config, SPI1 IRQ off)   L104, L4041–4099
│   ├── sd.h/.cpp            bit-bang SPI + SPISD_Driver + sdMount()      L3380–3580, L4149–4161
│   ├── imu.h/.cpp           MPU6050 init/read/watchdog → accel struct    L4102–4135, L4919–4968
│   ├── VL53L1X_mini.cpp/.h  ported, then overhauled per §11 (updateDSS, short mode, ROI)
│   ├── lidar.h/.cpp         init/read/watchdog glue, `lid` 0..1          L290–291, L4143–4145, L4937–4983
│   ├── joystick.h/.cpp      calibration + axes + game-style nav repeat   L453–549, L4166–4180
│   ├── encoder.h/.cpp       Enc/ePoll/eInit                              L443–450, L551–566
│   ├── fsr.h/.cpp           _fsr_scale.h + median sampler                _fsr_scale.h, L5021–5045
│   ├── dfu.h/.cpp           boot gesture (D2+D24) + CDC dfu check        L3899–3906, L4341–4344
│   └── usb/                 unchanged TinyUSB glue + clock_engine
│
├── engine/                  AUDIO-ISR PLANE (no I2C, no blocking)
│   ├── clock.h/.cpp         master step clock, swing, tap tempo (rolling N, phase snap),
│   │                        songLen, bars/steps, gridTick skeleton      L1810–1867, L2015–2029
│   ├── synth.h/.cpp         Voice, algorithms (Thief w/o plain saw, FM, WT, +candidates),
│   │                        ADSR, per-voice SVF, glide, poly/pan rules   L677–720, L1118–1242, L2286–2428
│   ├── scale.h/.cpp         MODES, rebuildScale (float key), circle-of-fifths weights L590–615
│   ├── drums.h/.cpp         16 lanes × 16 sounds from research/drum-synthesis.md; group vols,
│   │                        pan rules; replaces                          L1686–1793, L2456–2486
│   ├── sampler.h/.cpp       16 slots, press-exact record w/ fades, pitch/formant/reverse,
│   │                        playback                                     L1011–1024, L2489–2506
│   ├── sequencer.h/.cpp     per-engine lanes [16][MAXSTEPS/8], syn_seq, MANUAL/ARP/GEN,
│   │                        generative arp, octave displacement, live record  L749–765, L1951–2012
│   ├── fx_common.h          shared ring (rbuf), grid phase, anti-click fade helper, FSR/lid inputs
│   ├── fx_stutter.cpp       ST4/8/16/32, BZZ, HALF/DBL/STOP (clock-side)  L1888–2137
│   ├── fx_up.cpp / fx_down.cpp / fx_left.cpp / fx_right.cpp
│   │                        the 4 families × 16 (home 16 + 12 each) picked from §3 "strong/ok";
│   │                        each effect = struct with process(), onEngage(), onRelease()
│   ├── fx_automation.h/.cpp [banks][MAXSTEPS] masks + FSR, longest-loop length
│   ├── distortion.h/.cpp    locked algorithm set + 5 ms crossfade         from §3 list
│   ├── output.h/.cpp        engine/group gains, master fader (0.2848 = 100 %), Fruity-style
│   │                        soft clipper, hard ±1, NaN guard              L2514–2518, L3355–3361
│   └── audio_callback.cpp   the ISR: polls, clock tick, engines → fx → output, out_amp
│
├── ui/                      CONTROL PLANE
│   ├── nav.h/.cpp           menu ring (hard stops), cards, 1-D/2-D editors, both-button volume,
│   │                        game-style repeat, encoder-click tap          replaces L4711–4915
│   ├── pages/*.cpp          drums/synth/sampler (timbre/seq/perf), fx, patches, globals, dash
│   ├── screensaver.cpp      outline particles (fixed-point, ≤96)         replaces L5162–5207, L5416–5437
│   ├── widgets.cpp          card, slider, xy-pad, param list (from L5266–5287), saving star (L5446–5465 verbatim)
│   ├── trellis_render.cpp   lava base + page overlays + brightness + frame-diff + rotation
│   │                        (structure of L4410–4577, new palette)
│   └── palette.h            mint-anchored swatches (after approval)
│
└── store/
    ├── patch.h/.cpp         Persist v3 (64 slots), SESS_MAGIC 'OPG5', QSPI + SD mirror,
    │                        sidecars .AUD/.FXA/.SEQ, debounced autosave  L1026–1075, L1262–1642, L5062–5078
    └── files.h/.cpp         atomic temp→rename→backup helper (dedupe the 4 copies at L1262, 1329, 1402, 1441)
```

Interdependencies to break during the lift:
- `pmor_usb_service()` is the one symbol every hardware module needs — define it in
  `hw/usb_service.cpp` first (stage 1).
- `enc2` doubles as the joystick; V3 should give the joystick its own struct and let `nav`
  translate to actions, so no engine code ever reads `enc2.psw` (today gridTick does, L2040,
  L2051).
- ISR-shared volatiles (`fx_fsr`, `fx_lid`, `filter_f`, `step_ms`, `fx_*_held`, `smp_trig`,
  `rec_slot`, `held_mask`) belong in one `engine/shared_state.h` with a comment on which plane
  writes each.
- The SDRAM buffer zeroing (L4210–4223) becomes each FX module's `init()`.

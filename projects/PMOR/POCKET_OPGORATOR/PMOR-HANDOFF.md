# PMOR — Engineering Handoff

**Device:** PMOR (project name "Pocket OpGorator"). Electrosmith Daisy Seed / STM32H750 groovebox.
**Written:** 2026-08-18. Supersedes and replaces `PMOR-OLED-USB-HANDOFF-2026-08-15.md` and `PMOR-USB-OLED-DEBUG-LOG.md` (both deleted; everything of value is carried below).
**Audience:** a fresh agent with zero prior context.

**Epistemic tags used throughout:**
- **[VERIFIED]** — observed on hardware, or read directly out of a file in this tree. A `file:line` or a literal command is given.
- **[INFERRED]** — a conclusion drawn from verified facts. Attackable.
- **[PLANNED, NEVER OBSERVED]** — a test was built/queued but the transcript contains no record of it running.
- **[RETRACTED]** — asserted confidently in an earlier doc or code comment and later shown false.

**The single most important warning:** this codebase — and the summaries written about it — contain confident, well-written claims that are *factually wrong* or simply *out of date*. §5 lists the known ones. **Trust a measurement, a datasheet, or the source on disk over any comment or narrative summary, including one dated yesterday.** §5.1 is the instance that matters most: a compacted summary re-opened an already-solved bug.

---

## §0 — TL;DR / current state

1. **[VERIFIED] The "OLED kills USB" bug was SOLVED on 2026-08-16**, in a session (`bcc0918d-ec68-4006-9f11-0f022f230a52`) that is **not** in the transcript this handoff was commissioned from. Evidence: the fixes are physically present in the source on disk (§2.2), plus a first-party memory record.
2. **[VERIFIED] The transcript ends 2026-08-15 15:25 with the bug unsolved**, then jumps to 2026-08-18 09:34 with a `Continue from where you left off.` The agent that resumed is operating on a **compacted, pre-solve summary** and has been re-debugging an already-solved problem. Do not inherit that framing.
3. **[VERIFIED] Full instrument + USB audio + MIDI + CDC coexist with the display on**, per the solve. Root cause was **three** independent bugs, not one (§2.2), plus a fourth found later the same day (NeoTrellis seesaw clock-stretch).
4. **[VERIFIED] The firmware source is correct as of now.** `POCKETGORATOR_CODE.cpp:121` reads `volatile bool g_en_usbaudio = true;` and the tree was rebuilt clean on 2026-08-18 08:12. It was briefly set `false` **during this session** by the resuming agent, acting on the stale summary — a near-miss, not an 08-16 defect. See §2.3 and §5.1.
5. **The single open question:** does the full RC4 instrument still hold USB audio + MIDI + CDC + display through a ≥300 s soak, or did something regress after 2026-08-16? Re-establish that baseline before changing anything.
6. **"No audio in the Mac app" now has no confirmed firmware-side cause.** The remaining suspects are host-side (Mac app / Aggregate Device), or the symptom may simply be stale. See §2.3.
7. Demiurge/NEPTR side (clock PLL, role auto-detect, protocol, looper fix, Mac app) is **done and tested** — §8. Do not re-derive it.

---

## §1 — Hardware & build facts

### Hardware
- **[VERIFIED]** Electrosmith **Daisy Seed**, **STM32H750**, Cortex-M7, run at **480 MHz boost** (`hw.Init(true)`, `POCKETGORATOR_CODE.cpp:3722`). Boost mode is **ruled out** as a USB cause (§3).
- **[VERIFIED]** `APP_TYPE = BOOT_SRAM` (`Makefile:6`). The app lives in **QSPI flash at `0x90040000`**, is copied to SRAM at boot by the **Daisy bootloader** (which itself lives in internal flash at `0x08000000`). This lifts the 128 KB internal-flash ceiling to ~480 KB. The app is ~170 KB (`build/POCKETGORATOR_CODE.bin`, 175,604 bytes).
- **[VERIFIED]** OLED: **SSD1309, 128×64, 2.42", 4-wire SPI**, on a breakout module with an onboard boost converter for the panel rail. Driven by PMOR's own **bit-bang GPIO driver**, `oled_pmor.h` (not libDaisy's hardware-SPI transport).
- **[VERIFIED]** OLED pin map, `oled_pmor.h:25-30` — **verified against `libDaisy/src/daisy_seed.h:204-208`**:
  ```
  SCK  PG11  (Daisy D8)
  MOSI PB5   (Daisy D10)   ** also the NeoTrellis bit-banged I2C SCL **
  CS   PG10  (Daisy D7)
  DC   PC10  (Daisy D3)
  RST  PC9   (Daisy D4)
  ```
  **PB5 is shared and soldered. The user cannot change pins. Any fix must be software-only.** (The PB5-collision theory is nevertheless **dead** — see §4.)
- **[VERIFIED]** NeoTrellis 4×4 pad grid on a **bit-banged I2C** bus (`tbus`), D11=SCL(PB5)/D12=SDA(PB8), seesaw SAMD09 @ 0x2E.
- **[VERIFIED]** VL53L1X lidar — **parked**, built out with `make NO_LIDAR=1` (`PMOR_NO_LIDAR`, `POCKETGORATOR_CODE.cpp:3952`). It also turned out to have simply been **unpowered all session** (its power-converter switch was off).
- **[VERIFIED]** USB core: TinyUSB drives the **`USB1_OTG_HS` register block on rhport0**, despite the macro being named `USB_OTG_FS_PERIPH_BASE`. Read the port table, not the names: `tinyusb/src/portable/synopsys/dwc2/dwc2_stm32.h:69` (`#define USB_OTG_FS_PERIPH_BASE USB1_OTG_HS_BASE`) and `:131` (`{ .reg_base = USB_OTG_FS_PERIPH_BASE, .irqnum = OTG_FS_IRQn }`). This cost two wrong "fixes" (§5).
- **[VERIFIED]** `libDaisy/src/sys/system.cpp` already defines `OTG_HS_IRQHandler`. Do not define it again — you get a linker multiple-definition error. TinyUSB uses `OTG_FS_IRQn`; `OTG_HS_IRQn` is deliberately left to libDaisy (`usb/board_stm32h7.c:105-108`).
- **[VERIFIED]** OTG NVIC priority is set to **0** (`usb/board_stm32h7.c:96`, `NVIC_SetPriority(OTG_FS_IRQn, 0)`) — same preempt level as the SAI audio DMA, so neither ISR can interrupt the other mid-TXFIFO-copy.

### USB composite
TinyUSB composite device: **UAC2 capture-only (48 kHz) + USB-MIDI 1.0 + CDC-ACM**, product string `Pocket OpGorator`.
Sources live in `usb/` (copied from the proven `Pocket-OpGorator/firmware-tinyusb/` scaffold, which is read-only reference):
`usb/board_stm32h7.c`, `usb/usb_descriptors.c`, `usb/usb_audio_stream.c`, `usb/clock_engine.c`, `usb/tusb_config.h`.
TinyUSB itself is **not vendored** — `TINYUSB_DIR ?= .../Pocket-OpGorator/firmware-tinyusb/lib/tinyusb` (`Makefile:38`).

**Capture-only is a hard design requirement.** A USB *playback* sink would knock Demiurge out of direct-output mode and add a quantum of latency to the main path (`aggregate.rs::direct_output_device`).

### How to build
```sh
cd /Users/emgor/Desktop/DaisyExamples/seed/POCKETGORATOR_CODE
make clean && make            # full instrument
```
Build flags (`Makefile:9-33`), all `ifdef`-gated:

| Flag | Define | Effect |
|---|---|---|
| `CHUNK=n` | `PMOR_CHUNK=n` | staged bring-up: halt after init stage n, pump USB only (§6) |
| `NO_LIDAR=1` | `PMOR_NO_LIDAR` | skip VL53L1X init (it wedges USB — §3) |
| `NO_SD=1` | `PMOR_NO_SD` | skip SD card |
| `QUIET_BOOT=1` | `PMOR_QUIET_BOOT` | main loop gated off at boot (`g_en_loop=false`), enable subsystems live over CDC |
| `NO_USB=1` | `PMOR_NO_USB` | skip `board_usb_init()`/`tusb_init()` entirely |
| `OLED_USB_ONLY=1` | `PMOR_OLED_USB_ONLY` | NeoTrellis never initialized |
| `MOSI_OD=1` | `PMOR_MOSI_OPEN_DRAIN` | MOSI open-drain + pull-up (legacy, from the dead PB5 era) |

Runtime gates (toggled over CDC, **auto-revert after `g_probe_ms`, default 4000 ms** so a subsystem that kills USB turns itself back off and CDC/DFU return without a flash) — `POCKETGORATOR_CODE.cpp:3520-3556`, dispatch at `:3663-3672`:
`TRel 1|0`, `TKeys 1|0`, `TOled 1|0`, `TLoop 1|0`, `TIntro 1|0`, `TAll 1|0`, `TMs <ms>`.
Other CDC probes: `STATUS`, `DIAG`, `RATE`, `SINE1`/`SINE0` (USB capture test tone, `POCKETGORATOR_CODE.cpp:123`), `OC*`/`OB*`/`OP*` (OLED contrast / pixel-budget test kit), `ASTOP`/`ASTART`, `dfu`.

### How to flash
**Path A — hands-free, only works while USB is alive.** CDC `dfu` command → `System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT)` → then the catcher loop:
```sh
./chunktest.sh 7       # build CHUNK=7, self-DFU over CDC, flash, score
./chunktest.sh         # same for the FULL build
```
`chunktest.sh` writes the `dfu\r\n` string to `/dev/cu.usbmodem*` via a raw termios open, polls `dfu-util -l` for up to 10 minutes, then flashes.

**Path B — manual, when USB is dead.**
1. Tap **RESET**. LED breathes (2.5 s grace).
2. Tap **BOOT** during the pulse → grace extends indefinitely.
3. `dfu-util -a 0 -s 0x90040000:leave -D build/POCKETGORATOR_CODE.bin`
4. **Success is the literal string `File downloaded successfully`.** `Error during download get_status` on the *leave* request is harmless and appears on every successful write.

**Do NOT hold BOOT while resetting/plugging.** That pulls BOOT0 high and gives you **ST's ROM bootloader**, whose DFU exposes only `@Internal Flash /0x08000000` and `@Option Bytes` — it has **no QSPI region**, so every write of this app to `0x90040000` is rejected with `Last page ... is not writeable`.

Correct Daisy bootloader installed: **`dsy_bootloader_v6_2-intdfu-2000ms.bin`**, which presents
`@Flash /0x90000000/64*4Kg/0x90040000/60*64Kg/0x90400000/60*64Kg`.
`intdfu` vs `extdfu` refers to **which USB port the bootloader listens on**, not internal vs external flash. `extdfu` listens on the Daisy's external USB pins (D29/D30) and is invisible on the onboard connector.

Makefile targets: `make program-boot` (bootloader → 0x08000000), `make program-dfu` (app → 0x90040000). `make program` (openocd) **errors out** under `APP_TYPE=BOOT_SRAM`.

### SAFETY RULES — never violate
1. **NEVER write DFU alt 1 (option bytes).** Setting RDP level 2 is **permanently irreversible** — the chip is bricked forever. Every write must be `-a 0`.
2. **Never run `make program` unsupervised.** Never run two `dfu-util` processes at once — a background `dfu-util -w` holds the device claimed and makes a foreground `dfu-util -l` return empty, which reads as "the bootloader isn't enumerating." It probably is.
3. **Do not flash without the human present.** Every failed flash costs a physical BOOT+RESET button press by a human (§6).
4. `system_profiler` and `ioreg` return empty in this sandbox, and an **ioreg-based USB watcher produced FALSE down-events**. The trustworthy probes are `dfu-util -l`, `ls /dev/cu.usbmodem*`, and `ffmpeg -f avfoundation -list_devices true -i ""`.
5. ST's ROM bootloader is in silicon and always reachable via BOOT+RESET. As long as rule 1 holds, the device cannot be bricked.

### Backups — `Pocket-OpGorator/flash-backup/`
| File | What it is |
|---|---|
| `device-flash-20260811-full-128k.bin` | Full 128 KB dump of **internal flash** (the Daisy bootloader region) read off the device 2026-08-11. |
| `POCKETGORATOR_CODE-jun27-0204-asbuilt.bin` | The **pre-USB, pre-clock working instrument** app binary (149,624 B). This is the "put it back the way it was" artifact. |
| `POCKETGORATOR_CODE-jun27-0255-workingcopy.cpp` | Matching source for the above. |
| `POCKETGORATOR_CODE-preMerge-20260812/` | Full source tree snapshot immediately before the TinyUSB merge. |

Also: `POCKETGORATOR_CODE/backups/` holds ~30 numbered historical firmware variants (`9_xAxisFsrFix`, `8_dimLavaBluYellow`, …) — these are UI/feature iterations, not USB work.

---

## §2 — The core problem: precisely stated, and its resolution

### §2.1 The symptom as it stood on 2026-08-15
**OLED display refresh and USB device operation appeared mutually exclusive.**

- **Observation:** any build in which the panel actually blits pixels loses USB **instantly and permanently**. Reproducible live: with a healthy device answering `STATUS` over CDC, sending `TOled 1` dropped the device off the bus **mid-reply**.
- **Permanent** means it does not retry or recover; only a power cycle (or physical BOOT+RESET) brings it back.
- Any build where the panel stays dark: USB is **perfect** — UAC2 audio streams, MIDI clock runs, CDC answers.

**How a test is scored** — `chunktest.sh:70-85`, and this is the operative definition of USB OK vs DEAD:
```sh
AUD=$(ffmpeg -hide_banner -f avfoundation -list_devices true -i "" 2>&1 | grep -ciE "opgorator|pmor")
SER=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
# USB OK   iff  AUD >= 1  AND  SER non-empty
# USB DEAD otherwise
```
A distinction that was repeatedly muddled and matters: **"never enumerated" vs "enumerated then dropped"** are different failures. The `PMOR_STAGE` halt loop exists partly to tell them apart — it renders `irq <count>` and `mnt <0|1> rdy <0|1>` on the OLED, so a live IRQ count with `mnt 0` means the core is alive and the host is talking but the device never presented.

### §2.2 Resolution — 2026-08-16 [VERIFIED against code on disk]
It was never one bug. It was **three, each masking the others**, plus a fourth found later the same day.

**1. OLED pixel-load kill — the headline.**
The panel module's onboard boost converter, under **lit-pixel load**, sags the rail enough to kill the Daisy's USB (device off bus until power cycle). It scales with **lit pixel count, not bus traffic**: full-black blits are harmless; half-white kills even at minimum contrast.
Fix shipped: init contrast **`0x01`** in `oled_pmor.h` `InitPanel` — `oled_pmor.h:297-302`:
```c
SendCommand(0x81);              // contrast
// 0x01 = minimum segment current (2026-08-16 pixel-current hunt):
// full-white blits at the old 0x8F killed USB (rail sag from the
// panel boost under max pixel load); black blits never did.
SendCommand(0x01);
```
The real UI (sparse, dark, ~10% lit) then runs indefinitely with USB up. **The safe ceiling is between sparse-text-bright (OK) and half-white-dim (dead) — it has not been measured.** Fractal/dense pages and any big fill are the risk. Raise contrast only after measuring headroom with the `OC0`/`OC1`/`OC9` + `OBB`/`OBW`/`OBH`/`OBS` CDC probes.
**This retracts** the 2026-08-15 claim that "minimum contrast did not help" (§5).

**2. VL53L1X lidar init wedges USB** — deterministic (IRQ storm, never enumerates), **mechanism still unexplained**. Parked via `make NO_LIDAR=1`. Rehab ideas: init after enumeration completes, slow the bit-bang bus, bisect its register writes. Separately, the lidar was **unpowered all session** (its own power-converter switch was off), so some earlier lidar observations are suspect.

**3. Audio-ISR → TinyUSB FIFO race** — nondeterministic enumeration wedge; two byte-identical builds behaved differently. Fixed **structurally**, not by masking: `usb/usb_audio_stream.c` now writes into a private **SPSC ring** from the audio ISR (`s_ring`/`s_ring_w`/`s_ring_r`, `:30-93`) and drains it into `tud_audio_write()` from the main loop via `usb_audio_stream_task()` (`:100-108`), called next to `tud_task()`. OTG IRQ raised to preempt priority **0** so neither ISR can interrupt the other mid-TXFIFO-copy. This is the ownership model TinyUSB actually assumes.

**4. FINAL KILLER — NeoTrellis seesaw I2C clock-stretch desync.**
Key-FIFO **reads** (not LED writes) let the SAMD09 stretch SCL past the old fixed 10,000-iteration `waitScl` bound. Clocking on anyway desynced the bus into multi-millisecond spin storms on every 20 ms poll → **USB dead in 35–120 s of idle**.
Proven by survival bisect: LEDs-on survived 300 s; keys-on died at 73 s.
Fix: `waitScl` waits up to 5 ms while pumping USB, sets a sticky `stretch_err` (`POCKETGORATOR_CODE.cpp:162-171`), which triggers `BBus::recover()` (9 clocks + STOP) and a 250 ms backoff in the keypad poll (`:4388-4396`).
Verified: 300 s keys-on gauntlet + 393 s DIAG flight recorder, all counters clean.
**Failed fixes worth remembering:** masking the OTG IRQ around the reads (no effect); removing the per-byte pump from `byteR` (made it worse).

**Same-day polish (RC4, on device):**
- OLED bit-bang `nop_delay` **8 → 24** (`POCKETGORATOR_CODE.cpp:3903`). 8 rode the SSD1309's ~10 MHz ceiling and miswrote bytes — user-confirmed fixed.
- UI refresh 8/4 fps → **15/5 fps** (safe once the audio ring removed the CPU-contention reason).
- Master volume linear → dB table `kVolTable[16]` (`:984`): silence, −40…0 dB in 3.6 dB steps to the 100% tick, +1.94 dB max.
- `PMOR_STAGE` macro had `//` comments **inside** a `\`-continued macro → **CHUNK builds had not compiled at all**; every "CHUNK n proven" claim from before that fix is **void**. Comment now lives outside the macro (`:3678-3689`).
- `dfu` check moved above the `g_en_loop` gate so a loop-gated build can still be reflashed.

### §2.3 What is actually open right now

**Firmware source: correct. [VERIFIED]** `POCKETGORATOR_CODE.cpp:121` reads `volatile bool g_en_usbaudio = true;` (gate site `:2065`, `if(g_en_usbaudio) usb_audio_stream_push_block(chp, 2, 48);`). The tree was rebuilt clean 2026-08-18 08:12. **The firmware pushes USB audio and there is no known firmware-side defect.**

**A near-miss worth recording, because it is the failure mode of this whole project in miniature.**
During the 2026-08-18 session, the resuming agent — working from a **compacted summary that predated the 2026-08-16 solve** — believed the `g_en_usbaudio=false` mechanism test (§3 row 18) was still pending, and set line 121 to `false` to run it. A handoff pass read the file **mid-edit** and reported the `false` as a stale 08-16 defect, "explaining" the missing audio. Both halves were wrong:
- The line was never broken on 08-16 — the RC4 firmware shipped correct.
- The mechanism test it was re-running had already been superseded: the structural SPSC-ring fix that test would have justified was **implemented and shipped on 08-16** (§2.2 bug 3).
The edit has been reverted and rebuilt. **The lesson, not the line, is the artifact here** — see §5.1.

**"No audio in the Mac app": cause UNKNOWN, but the firmware is exonerated.**
With line 121 correct, there is no firmware-side explanation for the symptom. The remaining suspects, in order, are all host-side:
1. **No Aggregate Device present**, so the app fell back to the dual-stream path (~43 ms, crackling) or to nothing. `monitor/aggregate_device.py` creates one programmatically, but the aggregate it makes is **private** — it is invisible to any other process and dies with the creating process, so "I don't see it in Audio MIDI Setup" is expected and is *not* evidence it failed.
2. **The app's narrow auto-detect** — it only adopts a duplex device whose name contains `opgorator` (deliberate, to avoid grabbing Loopback/NDI/Omniplex). A renamed or half-flashed device falls through.
3. **The symptom may simply be stale** — it was reported during the dead-USB era, before the 08-16 solve.
**Do not pick one of these without testing.** The cheap discriminator is `ffmpeg -hide_banner -f avfoundation -list_devices true -i ""`: if PMOR appears there, the firmware and enumeration are fine and the fault is entirely inside the app.

**Open question, in one line:** does the full RC4 instrument hold USB audio + MIDI + CDC + display through a ≥300 s soak, or has something regressed since 2026-08-16?

---

## §3 — Evidence table: everything actually tested on hardware

Rows marked **[08-15]** are from the transcript that ends 2026-08-15 15:25. Rows marked **[08-16]** come from the solve-session's first-party memory record and are corroborated by code on disk. "Confidence" is confidence that *the observation is real*, not that its interpretation was right.

| # | What was changed | Prediction | Actual observed result | Confidence & why |
|---|---|---|---|---|
| 1 | **Chunk 0** — USB only, no instrument init, straight out of the Daisy bootloader handoff `[08-15]` | if BOOT_SRAM/bootloader is the problem, dead | **USB OK** | **High** — repeated many times; it was the base of every later flash |
| 2 | **Chunks 1–7** — +OLED `Init()` only (panel dark), +both bit-bang I2C sensor buses, +SD, +encoders/ADC, +patches/SDRAM/audio start, +NeoTrellis `ntInit()` `[08-15]` | one of these kills USB | **All PASS, USB OK** | **Medium** — *later invalidated in part*: the `PMOR_STAGE` macro did not compile at all until 08-16 (`//` inside a `\`-continued macro). Any chunk claim predating that fix is void. The *shape* of the result (init is not the killer) was independently confirmed by the solve. |
| 3 | **Chunk 7 with the halt loop actually RENDERING the display** (vs chunks 1–7's dark-panel halt loops) `[08-15]` | if drawing is the killer, dead | **USB DEAD** | **High** — but this row is a **self-inflicted confound**: the `PMOR_STAGE` debug macro itself blits `STAGE n / irq / mnt rdy` to the OLED every 250 ms. The chunk-7 "pass" and this "fail" differ by *the debug instrument*, not by the code under test. |
| 4 | **Chunk 8** (after the boot intro animation) `[08-15]` | — | **USB DEAD** | **Medium** — same macro confound; also the intro is a heavy full-screen animation (dense lit pixels), which the pixel-load finding retro-explains |
| 5 | **Flashing to internal flash (0x08000000) vs QSPI/BOOT_SRAM** with byte-identical USB code `[08-15]` | if bootloader handoff is the issue, only QSPI dies | **Both enumerate** (`usb/board_stm32h7.c:51-55`) | **High** — direct A/B, isolates the bootloader |
| 6 | **OLED off (`g_en_oled=false`) vs on** in the full build `[08-15]` | — | **off → USB OK, full instrument stable; on → USB DEAD, instantly** | **High** — reproduced live over CDC many times, including mid-reply drop on `TOled 1` |
| 7 | **Minimum contrast 0x01 vs 0x8F** `[08-15]` | if it's panel current draw, survives | **"Minimum contrast didn't help"** | **LOW — [RETRACTED].** The 08-16 session found contrast **is** the fix. The 08-15 test was almost certainly run on a *dense/bright* page (intro or full blit) where 0x01 was insufficient, or was confounded by bugs 2/3/4 still present. Do not cite this row. |
| 8 | **Pixel-budget / lit-pixel-count test** (black blit vs half-white blit vs sparse text, at fixed contrast) `[08-16]` | — | **Scales with LIT PIXEL COUNT, not bus traffic.** Full-black blits harmless. Half-white kills even at minimum contrast. Sparse text (~10%) at bright OK. | **High** — this is the decisive experiment; it is what produced the contrast-0x01 fix |
| 9 | **480 MHz boost mode ON vs OFF** (`hw.Init(true)` vs `hw.Init(false)`) `[08-15]` | — | **Identical. Boost is not a factor** (`POCKETGORATOR_CODE.cpp:3479-3481`) | **High** — direct A/B |
| 10 | **OTG peripheral reset, variant A** — reset `USB2_OTG_FS` `[08-15]` | bootloader hands over a dirty core; reset fixes it | **No change** | **High** that nothing changed; **but the test was invalid** — that is not the core in use |
| 11 | **OTG peripheral reset, variant B** — reset `USB1_OTG_HS` (the correct core, per `dwc2_stm32.h:69,131`) `[08-15]` | — | **No change. Ruled out.** | **High** |
| 12 | **OTG NVIC priority 6 → 1** `[08-15]` | if priority inversion vs SPI1(prio 0)/SAI DMA(prio 0), survives | **USB DEAD** | **High** — and correctly explained at the time: raising 6→1 still leaves OTG outranked by priority-0 ISRs |
| 13 | **OTG NVIC priority → 0** (equal to SAI DMA) `[08-16]` | neither ISR can preempt the other | **Shipped as part of the working fix** (`usb/board_stm32h7.c:96`) | **Medium** — shipped together with the SPSC ring, so its individual contribution was never isolated |
| 14 | **`dcd_int_disable()`/`dcd_int_enable()` wrapped around `tud_audio_write()`** in `usb_audio_stream_push_block()` `[08-15]` | masks the ISR race | **USB DEAD** | **High** — clean negative. Masking was the wrong shape of fix; the structural ring (row 19) was right |
| 15 | **`oled_ssd130x.h` weak per-page `tud_task()` yield hook** — added, then removed `[08-15]` | yields prevent task starvation | **Adding: no help** (verified genuinely compiled in and called via `objdump`). **Removing: still DEAD.** | **High** — and the hook was itself a **self-inflicted confound**: it made `display.Update()` a *third* concurrent toucher of `ep_in_ff` from thread context. Now permanently nulled at boot (`POCKETGORATOR_CODE.cpp:3512-3521`) |
| 16 | **USB cable swap** `[08-15]` | charge-only cable would look exactly like this | **The cable was fine.** DFU appeared on the same cable minutes later | **High** — and this was a wrong accusation that cost a round (§5) |
| 17 | **Bus/dock power vs external battery** `[08-15]` | if it's a bus-current ceiling, battery survives | **Battery did NOT fix it** (`POCKETGORATOR_CODE.cpp:3589`) | **High** — note this does *not* contradict the pixel-load finding: the sag is on the **panel module's own boost rail**, not the host's 5 V budget |
| 18 | **`g_en_usbaudio=false` mechanism test** — display ON, MIDI+CDC live, no audio pushed to the TinyUSB FIFO `[08-15]` | if audio-ISR race is the story, display works + USB survives | **PLANNED, NEVER OBSERVED.** Built 08-15 15:24:59; the session ended a minute later awaiting a physical DFU. The deleted `PMOR-USB-OLED-DEBUG-LOG.md` listed it as "DEAD" — unsupported by the transcript, almost certainly inferred from a source comment. It was **queued again on 08-18** from the stale summary and again never run. **It is now moot:** the structural fix this test would have justified (row 19) shipped on 08-16 and works, so the test has no remaining decision value. Do not re-queue it. | **N/A — no observation exists** |
| 19 | **SPSC ring + `usb_audio_stream_task()` drain from main loop** (the structural fix row 18 would have justified) `[08-16]` | removes the race by construction | **Shipped; part of the working configuration** (`usb/usb_audio_stream.c:30-108`) | **Medium-High** — shipped in a stack with rows 8 and 13, so not individually isolated |
| 20 | **`NVIC_DisableIRQ(SPI1_IRQn)` right after `display.Init()`** `[08-15]` | kills the "SPI1 prio-0 ISR preempts OTG" theory | **USB DEAD** | **High** on the observation. Moot now — the production driver is bit-bang and never touches SPI1 |
| 21 | **Soft-SPI (bit-bang) OLED vs libDaisy hardware SPI** `[08-15]` | — | **Bit-bang: USB survived, panel never lit** (libDaisy's soft-SPI transport has no chip-select concept, so CS floated and the panel ignored every byte). **Hardware SPI: panel lit, USB died.** | **High** — this drove the decision to write `oled_pmor.h` (own CS framing + timing). Retrospectively the "USB survived" half was because a dark panel draws nothing |
| 22 | **`ntInit()` blocking `System::Delay(500)` → `usbDelay()` (pumps `tud_task()`)** `[08-15]` | starvation of TinyUSB control transfers | **Real, separate bug. Fixed. Chunk 7 then passed.** (`POCKETGORATOR_CODE.cpp:313-341`) | **High** — keep this fix |
| 23 | **NeoTrellis LEDs-on vs keys-on survival bisect** `[08-16]` | — | **LEDs-on: alive at 300 s. Keys-on: DEAD at 73 s.** | **High** — clean, decisive, time-to-failure bisect. This is the model for how to run these tests |
| 24 | **`waitScl` 10k-iteration bound → 5 ms pumped wait + sticky `stretch_err` + bus recover + 250 ms backoff** `[08-16]` | — | **300 s keys-on gauntlet + 393 s DIAG flight recorder, all counters clean** | **High** |
| 25 | **VL53L1X lidar init present vs absent** `[08-16]` | — | **Present → IRQ storm, never enumerates. Deterministic.** Parked with `NO_LIDAR=1` | **High** on the observation; **mechanism unexplained**. Confounded by the lidar's power switch having been off |
| 26 | **MIDI clock rate** — host-side inter-arrival histogram `[08-14/15]` | — | **Tick spacing correct at 120.046 BPM (+0.04%). ~3% of `0xF8` bytes are exact duplicates arriving 0.3–1.5 ms after the real tick.** Root cause: `tud_midi_stream_write()` mis-encoding Real-Time bytes under CIN `0x5` instead of `0xF` and sharing mutable stream state with CC writes. Fixed by switching to `tud_midi_packet_write()` (`usb/clock_engine.c:350-382`) | **High** — device-side paired counters localized it; the earlier "2.3% fast clock" reading is **[RETRACTED]** |

---

## §4 — Theories, ranked

### T1 — OLED panel-module boost-converter rail sag under lit-pixel load — **CONFIRMED, this was the headline bug**
- **Mechanism:** the SSD1309 breakout has an onboard ~12 V boost for the panel. Segment current scales with lit pixel count. Under a bright/dense blit the boost pulls hard enough to disturb the Daisy's supply, and the DWC2 core drops off the bus permanently.
- **Supports:** row 8 (scales with pixel count, not traffic — the decisive test); row 6 (instant, not gradual, and unaffected by yields — a power event, not a timing one); the fix (contrast 0x01) demonstrably works in production.
- **Contradicts / caveats:** row 7 (08-15 "minimum contrast didn't help") — reconciled by the other three bugs still being present, and by the test page being dense. Row 17 (battery didn't help) — reconciled because the sag is on the *module's own* rail, not the host budget.
- **Cheapest remaining experiment:** measure the headroom. `OC0`/`OC1`/`OC9` step contrast, `OBB`/`OBW`/`OBH`/`OBS` blit black/white/half/sparse. Sweep to find the (contrast × lit-fraction) failure boundary and write it down. Until that exists, **any new dense UI page is a USB regression risk.**

### T2 — SAI DMA ISR (prio 0) → unlocked `tu_fifo_write_n` into `ep_in_ff`, preempting the OTG ISR mid-TXFIFO-copy — **REAL, but was never the whole story**
- **Mechanism:** `AudioCallback` runs inside the SAI DMA interrupt at NVIC preempt priority **0**. It called `tud_audio_write()`, which writes into TinyUSB's `ep_in_ff` FIFO **with no lock**. The OTG ISR ran at priority **6** and streams out of that *same* FIFO while copying into the hardware TXFIFO. Priority 0 preempts priority 6 mid-copy and moves the cursors underneath it. The FIFO is created **overwritable**, so a full buffer can even yank the read cursor. Documented DWC2 silicon behaviour: interrupt a TXFIFO packet copy and the state machine wedges **permanently — power-cycle only**. That matches the symptom exactly.
- **Supports:** the race is real and source-verified, not hypothesised. It explains why the failure was instant and permanent, and why thread-mode `tud_task()` yields could never help (a yield cannot stop a prio-0 ISR preempting a prio-6 one). It also explains the **nondeterminism** the solve session observed: two byte-identical builds behaving differently.
- **Contradicts:** **three fixes derived from this theory all failed on hardware** — row 14 (mask OTG around the write), row 12 (raise OTG 6→1), row 15 (remove the yield hook). That weakens it as *the* explanation but does not kill the mechanism: masking narrows a window rather than closing it, 6→1 is still outranked by prio 0, and the yield hook removal only deleted a third toucher. The structural fix (row 19) plus prio-0 equalization (row 13) *is* in the shipping firmware.
- **Cheapest decisive experiment (still worth running):** revert only `usb/usb_audio_stream.c` to the direct-`tud_audio_write()`-from-ISR form, keep everything else at RC4, and soak. If enumeration goes nondeterministic again, the ring is load-bearing. One flash.

### T3 — NeoTrellis seesaw I2C clock-stretch → multi-ms spin storms — **CONFIRMED, the final killer**
- **Mechanism:** SAMD09 stretches SCL on key-FIFO reads beyond the old fixed 10k-iteration `waitScl` bound; clocking on desyncs the bus, and the recovery spin blocks the main loop for milliseconds every 20 ms poll. TinyUSB's control-transfer work happens in `tud_task()` (task context), so this starves enumeration/keep-alive → dead in 35–120 s **of idle**, which is why it hid behind everything else.
- **Supports:** row 23's clean time-to-failure bisect (LEDs 300 s alive / keys 73 s dead), row 24's 300 s + 393 s clean soaks.
- **Contradicts:** nothing known.
- **Note:** this is a *starvation* mechanism, and it is the reason "USB dies after a while with everything on" persisted after T1 and T2 were fixed. **Any future "dies after N seconds" symptom should be scored by time-to-failure, not pass/fail.**

### T4 (alternative, still live) — SPI1 peripheral interrupt at hardwired priority 0 preempting the OTG ISR
- **Mechanism:** `HAL_SPI_MspInit()` unconditionally does `HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0); HAL_NVIC_EnableIRQ(SPI1_IRQn);` (`libDaisy/src/per/spi.cpp:998-1001`) — priority 0, the highest on the chip — even though the OLED transport only ever calls the *polling* `HAL_SPI_Transmit()` (`:679-687`) and needs no interrupt. Any pended SPI1 flag (overrun, framing, mode fault) fires a prio-0 ISR that can preempt OTG mid-TXFIFO-copy.
- **Supports:** explains the original hardware-SPI-only failure; the row-20 `NVIC_DisableIRQ` negative is not airtight because `HAL_SPI_MspInit()` re-enables on *every* `SpiHandle::Init()`.
- **Contradicts:** **mostly moot** — production uses the bit-bang driver and never initializes SPI1. This matters only if someone reverts to hardware SPI (e.g. to fix display lag).
- **Cheapest decisive experiment:** add `volatile uint32_t g_spi1_irqs` incremented at the top of `SPI1_IRQHandler` (`libDaisy/src/per/spi.cpp:1081-1084`), surface it over CDC next to `g_usb_fs_irqs`. If it climbs after `NVIC_DisableIRQ(SPI1_IRQn)`, something re-enabled it.

### T5 (alternative, **DEAD**) — PB5 pin collision between SPI1 AF5-MOSI and the NeoTrellis bit-banged SCL
- **Mechanism:** the same physical pad claimed by two peripherals with conflicting mode registers; a stray edge could set an SPI1 fault flag (feeding T4) or cause a genuine bus fight.
- **Killed by:** chunk 7 (OLED + NeoTrellis both initialized, screen dark) **passes**; and the 08-16 measurement that PB5 held high for 10 ms with the panel deselected is **harmless**. Recorded here only so nobody re-derives it from the old docs, which pushed it hard.

### T6 (alternative, worth keeping in mind) — the sandbox/harness is lying
Not a hardware theory, but it produced more false results than any hardware theory did. `ioreg`-based watchers give false down-events; `system_profiler` returns empty; a `grep -c` exit code silently aborted a flash; a `timeout`-wrapped retry loop never invoked `dfu-util` at all on macOS. **Before believing a negative result, prove the flash actually landed** (`File downloaded successfully`) and prove the probe works on a known-good device.

---

## §5 — Mistakes made, as rules for the next agent

1. **A compacted session summary can predate a solution and silently resurrect a solved problem. This is the highest-value lesson in this document.**
   The 2026-08-16 session solved the OLED/USB bug outright. The 2026-08-18 session resumed from a **compacted summary written on 08-15** — before the solve — and spent its effort re-debugging a problem that was already fixed, re-running a mechanism test whose structural conclusion had already shipped, and briefly editing correct firmware to do it (§2.3). Nothing in the compacted narrative was false; it was simply *old*, and a summary carries no timestamp you can feel.
   **It was avoidable.** `MEMORY.md` carried a line reading **"Project: PMOR USB vs OLED conflict — SOLVED 2026-08-16"** at the very top of the session that re-opened it. It was in context the whole time and was not acted on.
   **Rule for the next agent — do this before writing a single line of debugging:**
   ```sh
   cat ~/.claude/projects/-Users-emgor-Documents--EMGOR-SYNTH-------2026NEW-DEMIURGE-OS/memory/MEMORY.md
   # then read every matching memory file, e.g.
   cat ~/.claude/projects/.../memory/project_pmor_usb_oled_conflict.md
   ```
   And then: **trust the source on disk over any narrative summary.** A `grep` for the fix (`grep -n "stretch_err\|SPSC\|contrast" …`) settles in seconds what a summary can mislead you about for hours. If a summary and the code disagree, the code is what runs.

2. **Never compute BPM as byte-count ÷ wall-clock time. Build an inter-tick interval histogram.**
   ~3% of PMOR's MIDI `0xF8` clock bytes were sent **twice, ~0 ms apart**. A naive count/time readout reported "2.3% fast" and later "+9.16% fast", and **three consecutive debugging rounds** chased a nonexistent sample-rate drift (including a self-calibration mechanism that turned out to be a total no-op on hardware, and a 31-minute measurement proving the SAI domain has *zero* drift — `audio_frames_total/audio_cb_count` = exactly 48.0000). The histogram showed spacing was correct at **120.046 BPM** and separately that ~3% of ticks were exact duplicates. **This trap will recur** — anywhere on either side of the link, and in the follower PLL. Both sides now gate on a minimum interval (§8); do not remove those gates.

3. **Do not blame the cable.** It was blamed after a build stopped enumerating; DFU appeared on that same cable minutes later. A dead USB device and a charge-only cable look identical from the host — so distinguish them with a *known-good build*, not with a guess. Blaming the cable also caused three genuine fixes to be dismissed as "chasing a phantom."

4. **Never let a debug instrument touch the subsystem under test.** The `PMOR_STAGE(n)` macro renders `STAGE n / irq / mnt rdy` **to the OLED**. That made "chunk 7 dark = PASS" and "chunk 7 rendering = FAIL" differ by the instrument, not by the code — invalidating a chunk result and sending the investigation at the intro animation. The `oled_ssd130x.h` yield hook was the same class of error: added while chasing a wrong theory, it became a third concurrent writer to `ep_in_ff` and plausibly made things worse.

5. **One variable at a time — always, without exception.** Multiple rounds stacked two or three changes ("intro conditional on `!tud_mounted()`" also switched the whole main loop on) and produced results that could not be attributed. The RC4 fix stack (contrast + prio-0 + SPSC ring) shipped together and consequently none of the three is individually isolated.

6. **Read the port table, not the macro names.** The OTG peripheral was gotten wrong **twice** by inferring from names: first resetting `USB2_OTG_FS`, then "correcting" to `USB1_OTG_HS`, then back. The answer was two lines of TinyUSB: `dwc2_stm32.h:69` (`USB_OTG_FS_PERIPH_BASE` is `#define`d to `USB1_OTG_HS_BASE`) and `dwc2_stm32.h:131` (rhport0 uses that base with `OTG_FS_IRQn`). Two `grep`s would have replaced two flash cycles and an hour.

7. **A shell exit code can silently eat your flash.** A `grep -c` in an `&&` chain returned non-zero, so the `dfu-util` that followed never ran — and the "still dead" result was reported as a hardware finding. Likewise `timeout` **does not exist on macOS**, so a retry loop wrapping every `dfu-util` call produced thousands of "attempt failed" lines while dfu-util never executed. **Score a flash only on the literal string `File downloaded successfully`** — never on the *absence* of an error string (a run that died on `get_status: LIBUSB_ERROR_OTHER` was once reported as `>>> FLASHED`).

8. **Weak-symbol / duplicate-vector collisions are evidence, not noise.** Defining `OTG_HS_IRQHandler` produced a multiple-definition link error, which correctly revealed that **libDaisy's `system.cpp` already owns that vector** and routes it to libDaisy's own ST USB stack. Read the linker error rather than deleting the symbol reflexively.

9. **Declaration-after-use.** A build failed because `g_en_usbaudio` was declared next to the other probe gates (~line 3520) but read from the audio callback (~line 2065). It now lives at `:121` with a comment explaining why. Trivial, but it cost a build cycle in a workflow where cycles are expensive.

10. **`//` comments cannot live inside a `\`-continued macro.** They terminate the definition early. The `PMOR_STAGE` macro carried them for a while, meaning **`make CHUNK=n` never compiled** and every chunk claim from that era is void. The explanatory comment now sits outside the `#define` (`POCKETGORATOR_CODE.cpp:3678-3689`).

11. **Do not build an elaborate argument on an unverified premise.** Hours went into open-drain-vs-push-pull, slew rate, and clock speed on a driver whose pins were all shifted by one — the panel was physically incapable of lighting in *any* configuration, and a permanently-dark screen was read as a signal-integrity problem. Five research subagents were dispatched on that false premise. **Verify the premise first, cheaply.**

12. **Read a file's state, not a file's snapshot — and never diagnose from a tree someone is mid-edit on.** During the 2026-08-18 session a handoff pass read `POCKETGORATOR_CODE.cpp:121` while the main agent had it temporarily set to `false` for an experiment, and reported it as a shipped defect that "explains the missing audio." It was neither. Two rules fall out: **(a)** if you are reading source another agent is actively working in, say so and confirm the value with its owner before building a conclusion on it; **(b)** a `false` next to a comment saying "restored to true" is more likely a live experiment than a bug — check `git status`/mtimes and ask, rather than inferring history from a single line.

---

## §6 — Decision pipeline used

**The binding constraint: every hardware iteration costs a human pressing BOOT+RESET.**
USB is the debug channel (CDC carries `dfu`, `STATUS`, `DIAG`, and all runtime gates). So a build that kills USB also kills the ability to flash the next build. That makes each failed experiment cost a *physical human action*, not just a `make`. Everything about the methodology follows from that:

1. **Bisect by init stage, upward from a known-good base.** `PMOR_STAGE(n)` halts `main()` after stage n and pumps only USB. Because self-DFU works only while the *current* firmware still enumerates, the walk goes **upward from chunk 0** (proven good) — the first failing stage is the culprit and you have not yet lost your flash channel. Stage order: `0 usb only / 1 +OLED / 2 +sensors(I2C) / 3 +SD / 4 +encoders,ADC / 5 +patches / 6 +audio / 7 +NeoTrellis / full`.
2. **Prefer runtime gates over rebuilds.** The `g_en_*` flags with **4-second auto-revert** were the single highest-leverage tool built: a subsystem that kills USB turns *itself* back off, CDC returns, and the next hypothesis costs zero flashes. Any new suspect subsystem should get a gate before it gets a theory.
3. **One variable per flash.** Not a style preference — with a human in the loop per iteration, an unattributable result is a wasted human action. See §5.5 for what happened when this was violated.
4. **Score mechanically, never by impression.** `chunktest.sh`'s verdict is `audio device present AND /dev/cu.usbmodem* present` (§2.1). No "seems to be working."
5. **Score by time-to-failure once pass/fail stops discriminating.** The NeoTrellis bug (35–120 s to death) was invisible to a pass/fail score and only fell to a survival bisect (300 s vs 73 s). If a symptom is intermittent, the metric is seconds-alive.
6. **Distinguish "never enumerated" from "enumerated then dropped."** Different failures, different causes. The stage halt loop renders `irq <count>` and `mnt/rdy` on-panel precisely to separate them from the device side, because the host side cannot.
7. **Prefer a structural fix to a masking fix.** Masking the OTG IRQ around a FIFO write narrowed a race window and failed. Moving the ISR behind an SPSC ring removed the race by construction and held.

---

## §7 — The known-good fallback

**[VERIFIED]** Full instrument + USB audio + USB MIDI + CDC, all working, **with the display disabled**. This configuration was measured clean — audio streamed with RMS on a test beat, MIDI clock present, CDC answered `STATUS` with e.g.
`sy=0 wave=0 role=opg_master bpm=120 running=1 boot=listening beat=on beatlvl=80`.

To get back to it:
```sh
cd /Users/emgor/Desktop/DaisyExamples/seed/POCKETGORATOR_CODE
# 1. Edit POCKETGORATOR_CODE.cpp:3539  ->  volatile bool g_en_oled = false;
# 2. Edit POCKETGORATOR_CODE.cpp:121   ->  volatile bool g_en_usbaudio = true;
make clean && make NO_LIDAR=1
./chunktest.sh          # self-DFU if USB is alive; otherwise waits for BOOT+RESET
```
Or, with USB alive, no source edit at all — just send `TOled 0` over CDC (it auto-reverts after 4 s; re-send every 2 s to hold it off).

**Deeper fallback — the pre-USB instrument, if the firmware needs to be abandoned entirely:**
```sh
dfu-util -a 0 -s 0x90040000:leave \
  -D "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/flash-backup/POCKETGORATOR_CODE-jun27-0204-asbuilt.bin"
```
That restores the original working instrument (no USB audio, no clock engine). Matching source: `flash-backup/POCKETGORATOR_CODE-jun27-0255-workingcopy.cpp`.

---

## §8 — Everything NOT about the OLED/USB bug (done — do not re-derive)

### 8.1 Demiurge/NEPTR follower clock — `src/demiurge-clock.cpp` + `src/demiurge-clock-pll.h`
- **Pure-logic core** extracted into `src/demiurge-clock-pll.h` so it compiles both on the Pi (against real ALSA) and on any dev box for testing. **Do not duplicate this logic anywhere** — the header says so and means it.
- **PLL plausibility gate — the duplicate-tick fix.** The old gate accepted any interval > 0.5 ms; PMOR's duplicate `0xF8` bytes land 0.3–1.5 ms after the real tick, passed the gate, and biased the recovered tempo **+9.16% fast**. New floor is **6.25 ms** = 60/400/24, i.e. the shortest legal 24 PPQN interval at `CLOCK_MAX_BPM 400`. Ceiling is **125 ms** (20 BPM) — above that it is a stall, not a tempo. `demiurge-clock-pll.h:34-46`. With the new gate the regression test recovers **120.0000 exactly**. The firmware's own gate in `usb/clock_engine.c` computes the identical bound in audio samples — **if you change one, change the other.**
- **Trimmed mean** over a 24-sample window (`CLOCK_PLL_WINDOW 24`): sorts a copy, discards the single lowest and highest, then averages. A plain mean let one bad sample in 24 shift recovered tempo ~4%. `demiurge-clock-pll.h:107-113`.
- **Tempo-source priority ladder** (`demiurge-clock-pll.h:221-278`) — lowest index wins while it has been heard within `CLOCK_SRC_RECENCY_WINDOW_S = 2.0`:
  ```
  0 EXTERNAL_MIDI  -- recovered 0xF8 PLL lock (PMOR / role=follower)
  1 CC118_STEER    -- looper-derived tempo (chase_bliss_clock.orc)
  2 LINK           -- Ableton Link (only when role==master)
  3 LIVE_CONF_BPM  -- startup --bpm= ; one-shot, never re-applied
  ```
  It is a **safety net, not a second arbiter** — CC118 senders already self-gate. The 2.0 s window deliberately matches `chase_bliss_clock.orc`'s `iChaseBlissExtWindow`.
- **Tests: 172/172** — `testing/clock/run.sh` (PLL/ladder unit tests via `test_pll.cpp`, realistic tick-stream simulation with duplicates + jitter + USB burst batching, role-flip/stall/replug churn, fuzz) **plus** a whole-daemon compile check of `src/demiurge-clock.cpp` against a stubbed ALSA (`testing/clock/stub/`), zero errors/warnings. Also `testing/neptr-clock/run.sh` — **7/7** csound-side arbiter tests (loop-vs-external priority, `gkGLITCH_extBpmSeen` latches only on CC116, never CC119 alone).

### 8.2 Auto role detection — `src/demiurge-launcher-rs/src/clockrole.rs`
`PresenceTracker`: device appears in the graph → Demiurge becomes **FOLLOWER**; disappears → back to **MASTER**. **26 tests pass** in that crate (the design brief's "22/22" refers to `clockrole.rs`'s own subset; `cargo test` in `src/demiurge-launcher-rs` is the authority). Three doctrines are enforced deliberately and should not be "simplified":
- **One write path.** It never touches `~/demiurge/sync.conf` directly; it shells `demiurge-sync --auto <role>`, the same CLI a human runs. `--auto` exists only so the CLI can stamp `source = auto`.
- **Explicit user choice wins.** A human `demiurge-sync demiurge` stamps `source = manual`; auto-detection reads that and skips. Checked in *both* places on purpose.
- **Edge-triggered, not level-triggered.** Only an actual presence transition emits an action — otherwise the 2 s heartbeat would spam CC116 onto the shared MIDI bus and fight the clock daemon's own re-announce.

### 8.3 The ch16 role/transport/BPM protocol (`demiurge/docs/clock.md`)
| CC (MIDI channel 16 = index 15) | Direction | Meaning |
|---|---|---|
| **CC119** | clock → bus, once per beat | **BPM broadcast, READ-ONLY.** Only the clock ever writes it |
| **CC118** | any → clock | **BPM steer, WRITE.** Clock retunes; next CC119 reflects it |
| **CC117** | any → clock | **Transport.** `>=64` start, `<64` stop. Emits `0xFA`/`0xFC` |
| **CC116** | any ↔ clock, ~every 2 s | **Role.** `1` = this daemon becomes **follower**; `2` = **master** |

**BPM encoding: `bpm = 40 + (value / 127) * 360`, i.e. 40–400 BPM.** (The old `40 + v/127*200` formula was wrong and was corrected in all 7 language examples in `clock.md`.)
CC119 and CC118 are deliberately **separate** (read-only broadcast vs write-only request) so a writer never chases its own echo. Feedback guard: the daemon captures `snd_seq_client_id(seq)` at startup and ignores all clock/transport/CC116 events originating from itself.
Injection uses the existing zero-new-deps path: `aseqsend -p "Midi Through:0" -C 16 -c <cc> -v <value>`.

### 8.4 Looper grid-true capture fix — **pushed to Pi + Cabbage VST/AU**
The master-defining branch (whichever channel closes first) captured raw write position with **zero quantization and zero trim**, so loops gained a sliver of silence and drifted off grid. Fix:
- **CLOCKED** (external clock seen): quantize to the nearest whole-beat multiple of `gkLpBeatSamples = sr*60/gkGLITCH_bpm`. Early → defer to state 5 rather than cut the tail; late/exact → round back immediately.
- **UNCLOCKED**: new **state 6** (trim-closing) — bounded backward scan for trailing silence at 0.001 FS (~−60 dB), 8×ksmps per k-block so it never blows the RT budget.
- New globals in `csound/globals.orc`: `gkGLITCH_extBpmSeen` (latched in `midi_handler.orc` instr 99 on a genuine CC119 change), `gkLpGridAvail`, `gkLpBeatSamples`, `gkLpTrimPos1-5`.
- `vst/generate_vst.py`'s inlined copy of the SET_LENGTH logic updated to match (Cabbage UI does not `#include` the CC macro file), and `vst/NEPTR_VST.csd` / `NEPTR_APP.csd` regenerated. `TRIM` added to `LOOPER_STATE_LABELS`.
- `looper_ref/model.py` gained `quantize_to_beat()` and `trim_trailing_silence()`; **19/19** tests in the primary tree, **28** in the secondary.

### 8.5 The Mac app — `Pocket-OpGorator/monitor/` and `~/Applications/PMOR.app`
Radial pulse ring (one revolution per bar), live BPM, duplicate-tick counter, clock role, transport, stereo level meters, straight-through audio to Mac speakers, CDC control panel (SY modes, BEAT, DIAG, guarded DFU button), role flip via CC116.
- It shows **both** BPMs — host-measured and device-claimed CC119 — flagged when they disagree. That discrepancy is what exposed the duplicate-tick bug; one number on screen would have hidden it for three more rounds.
- Duplicate detection verified against synthetic injected duplicate `0xF8` bytes: **17 of 17 caught**.
- `device_match.py` matches **both** `PMOR` and `Pocket OpGorator`, so the rename cannot break it.

**Launch-hang fixes (all real bugs, all shipped):**
1. **PortAudio deadlock.** Starting the *input* stream before the *output* blocked in C at `audio_engine.py:448` (`stream.start()`) **holding the GIL**, which froze pygame before it could open a window — hence a silent hang with no error. **Output stream first** resolves it.
2. Use `pygame.display.init()`, not `pygame.init()` — full init drags in subsystems that stall.
3. **macOS TCC.** An unsigned `.app` cannot read `~/Documents`; it died on `pyvenv.cfg` with a hard `PermissionError` and **no prompt**. Fixed by bundling the code *and* the venv **inside** the bundle and running it from `~/Applications/PMOR.app`.
4. Dock said "Python" → `exec -a PMOR`; icon flipped to the Python snake → set it explicitly from the app.

**The aggregate-device request — [VERIFIED] DONE, not outstanding.** `monitor/aggregate_device.py` (2026-08-15) creates the CoreAudio Aggregate Device **programmatically**. The earlier refusal (print manual Audio MIDI Setup steps) was reversed by explicit user instruction. Safety is achieved without a manual step: every aggregate it creates is marked **private** (`kAudioAggregateDeviceIsPrivateKey = 1`), so it is never written to CoreAudio's persisted plist, never appears in Audio MIDI Setup, is visible only to the creating process, and is torn down by `coreaudiod` when that process dies — both confirmed empirically. **It is additive-only: it never touches an existing device's properties.** Auto-detection is deliberately narrow — it only adopts a duplex device whose name contains `opgorator`, because the user's Loopback / NDI / Omniplex devices are duplex-capable and would otherwise be silently grabbed. `--aggregate NAME` always overrides. `PMOR.app/.../aggregate_setup.py` creates a "PMOR" aggregate. Latency: dual-stream measured **43 ms** (unacceptable); the aggregate path is the fix, and the UI reports `AGGREGATE MODE — N.Nms` vs `DUAL-STREAM FALLBACK — N.Nms` rather than quoting an unmeasured number.

### 8.6 Timing ceiling — the honest number
**Over USB MIDI, ±1 ms of jitter is a hard floor** (USB polls in 1 ms frames). Tempo, by contrast, locks well under 0.1% because the PLL averages many ticks — that is what stops two sequencers drifting apart over minutes. For reference, ±1 ms is roughly what DIN MIDI has always given. Phase alignment on start is the harder half: MIDI clock carries no bar position.
**If ±1 ms is genuinely not tight enough**, the upgrade path is the analog PO-sync click into a NEPTR audio input, edge-detected in csound at **sample resolution (~0.02 ms, ~50× tighter)**. The firmware already contains that click generator; the merge deliberately left it unwired.

### 8.7 Hardware test plan
`DEMIURGE_OS/testing/PMOR-HARDWARE-CHECKLIST.md` — ordered so an early failure stops you before dependent steps. Every item tagged **[SIMULATED — low risk]** or **[UNKNOWN — genuinely untested]**. Step 1 is USB enumeration on the **Pi** (`lsusb`, `arecord -l`), and it explicitly flags that **a playback interface showing up is a real bug** (it would knock Demiurge out of direct-output mode).

---

## §9 — Next steps, prioritized

1. **Confirm the firmware is where you think it is, then flash and score.** No source edit is needed — `POCKETGORATOR_CODE.cpp:121` is already `true` and the tree was rebuilt 2026-08-18 08:12. Verify, don't assume:
   ```sh
   cd /Users/emgor/Desktop/DaisyExamples/seed/POCKETGORATOR_CODE
   grep -n "g_en_usbaudio =" POCKETGORATOR_CODE.cpp   # expect: true
   make clean && make NO_LIDAR=1
   ./chunktest.sh
   ```
   Score: audio device present in `ffmpeg -hide_banner -f avfoundation -list_devices true -i ""` **and** `/dev/cu.usbmodem*` present.

2. **Re-establish the RC4 baseline with a soak, before touching anything else.**
   Full instrument, display on, `NO_LIDAR=1`, keys enabled. Run **≥300 s** and score by **time-to-failure**, not pass/fail (§6.5). Confirm over CDC with `DIAG` that `stretch_err`/recover counters stay clean. If it holds, the 2026-08-16 solve is intact and the only issue was step 1.

3. **Resolve "no audio in the Mac app" — host-side, now that the firmware is exonerated (§2.3).** Discriminate before fixing:
   ```sh
   ffmpeg -hide_banner -f avfoundation -list_devices true -i ""   # is PMOR an input device?
   ls /dev/cu.usbmodem*                                           # is CDC up?
   ```
   - **PMOR listed** → firmware and enumeration are fine; the fault is inside the app. Check whether `monitor/aggregate_device.py` actually created the aggregate (it is **private**, so its absence from Audio MIDI Setup proves nothing) and whether the app is in `AGGREGATE MODE` or `DUAL-STREAM FALLBACK` — the UI states this on screen.
   - **PMOR not listed** → go back to step 2; this is not an app problem.
   Do not guess between these. And note the symptom may simply be stale — it was last reported during the dead-USB era.

4. **Measure the OLED pixel-load headroom and write it down.** This is the one unquantified boundary in the working system, and every future UI page depends on it.
   Sweep contrast (`OC0`/`OC1`/`OC9`) × lit fraction (`OBB` black / `OBS` sparse / `OBH` half / `OBW` white), recording pass/fail at each point. Deliverable: a documented max (contrast, lit-fraction) pair. **Until it exists, treat any dense or bright new page as a USB regression risk.**

5. **Rehabilitate the lidar, or formally retire it.** Currently `NO_LIDAR=1` and the mechanism is unexplained. First re-test with its **power-converter switch ON** (it was off all session — the original observations may be worthless). Then: init it *after* enumeration completes; if that works, ship it. Otherwise bisect its register writes.

6. **Isolate the RC4 fix stack.** Three fixes shipped together (contrast 0x01, OTG prio 0, SPSC ring) and none is individually attributed. One flash each, reverting exactly one, tells you which are load-bearing — worth doing so nobody "cleans up" a fix that is holding the system together.

7. **Do the PMOR rename, all five places at once** (§10). Do it only *after* step 2 confirms a stable baseline — changing the USB product string mid-debug adds a variable.

8. **Run `testing/PMOR-HARDWARE-CHECKLIST.md` on the Pi.** Everything in §8 is simulated or Mac-verified; the Pi-side integration (real enumeration, role auto-switch on plug, recovered BPM vs PMOR's, audio mixing through the `opgorator` chain token, whether loops recorded under PMOR clock land on grid) is **[UNKNOWN — genuinely untested]**.

---

## §10 — Constraints & etiquette

**Files owned by a concurrent session — DO NOT TOUCH:**
```
DEMIURGE_OS/src/companion/demiurge
DEMIURGE_OS/src/wrappers/demiurge-touch
DEMIURGE_OS/src/wrappers/demiurge-touch-test
DEMIURGE_OS/src/wrappers/demiurge-capture-guard
DEMIURGE_OS/src/wrappers/demiurge-meter
DEMIURGE_OS/src/wrappers/demiurge-ringtest
DEMIURGE_OS/config/demiurge-capture-guard.service
DEMIURGE_OS/web/**
```

**Never commit in the umbrella repo `/Users/emgor/Documents/_EMGOR_SYNTH`.** It contains multi-GB binaries (`gorator.blend` 657 MB, two ~450 MB STLs, etc.). Commit inside `DEMIURGE_OS/` only, and only when asked.

**Naming.** The device's real name is **PMOR**. "Pocket OpGorator" is the project name and is currently the **USB product string**. The rename is **not cosmetic** — that string is load-bearing in **five** places, and if they drift the failure is *silent*:
1. The firmware's **USB product descriptor** (`usb/usb_descriptors.c`).
2. **`src/demiurge-launcher-rs/src/aggregate.rs`** — the exclusion `!contains("OpGorator")`. **If this stops matching, PMOR gets auto-adopted as the default engine input** via the direct-INPUT path — exactly the trap flagged when that feature landed.
3. **`src/demiurge-launcher-rs/src/supervisor.rs`** — `resolve_opgorator_client` client-name candidates.
4. The **WirePlumber rule** `config/wireplumber/95-demiurge-opgorator.conf`, which pins the device off graph-master (`priority.driver = 0`, `priority.session = 0`) and forces 48 kHz.
5. The **Mac monitor app** (`monitor/device_match.py`).
Change all five in one pass, and **match on both old and new names during the transition** so a half-flashed device does not fall through the cracks. (`clockrole.rs::MATCH_PATTERNS` already looks for `OpGorator`/`Daisy`/`PMOR` substrings.)

**Working style the user asked for:** short summaries, not long ones. Say what a test proves and what setup it needs, then get a go-ahead before running audio/hardware tests. Never leave the rig in a broken state at the end of a turn.

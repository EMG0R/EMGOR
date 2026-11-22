# PMOR v2 — State of Play (2026-09-13)

Everything learned rebuilding PMOR on a **Daisy Seed 3**. Read this before
touching the firmware or the wiring.

**Live firmware:** `DaisyExamples/seed/PMOR_V2/` — the Aug-18 PMOR tree with
targeted changes. `POCKETGORATOR_CODE/` is the untouched original; diff against
it to see exactly what v2 changed.

---

## 1. Working on hardware

NeoTrellis · encoder · joystick · MPU6050 · VL53L1X lidar · OLED · **micro-SD**
· FSR · audio in/out · buttonless DFU.

SD transfers are **slow but functional** — see §3.

---

## 2. THE SD MODULE — read this before wiring anything

**The module is `WWZMDiB` ASIN B0BV8ZQ81F — a PASSIVE 3.3V-ONLY breakout.**

Its entire BOM is **4x 10k pull-ups (marked `103`) and two small caps**.
**No regulator. No level shifter.** The power pin is silkscreened **`3V3`**.

> ### 5V ON THIS MODULE GOES STRAIGHT TO THE CARD'S VDD AND DESTROYS IT.
> Cards are absolute-max ~3.6V.

**The v1 BOM line saying "3.3V/5V" is WRONG.** That describes WWZMDiB's *other*
product (ASIN B0B779R5TZ, the large blue board *with* a logic-level chip).
`pocket-OpGorator.md` also says `VCC->+5V` in two places (`:202`, `:599`).
**Both are wrong for the module actually in the build.**

**Telling the two apart by eye:**

| | Passive (WHAT WE HAVE) | Regulated |
|---|---|---|
| Power pin silkscreen | **`3V3`** | `VCC` |
| Size | ~18x18mm | ~42x24mm |
| Chips | only `103` resistors | SOT-223 `AMS1117-3.3` + SOIC-14 `74LVC125A` |
| Supply | **3.3V ONLY** | 4.5-5V required |

Read the power-pin silkscreen. That settles it without a loupe.

Because there is no regulator, the voltage you measure at the pin **is what the
card sees**. 3.2V measured = card at 3.2V = inside the SD spec (2.7-3.6V).

**Wiring (confirmed by CMD0 succeeding):**

| Wire | Signal | Daisy |
|---|---|---|
| yellow | CS | **D28** |
| green | MOSI | **D27** |
| blue | CLK | **D26** |
| purple | MISO | **D25** |
| — | 3V3 | **pin 38** |
| — | GND | **pin 40** |

Left column, counting up from the power corner: skip D30/D29, then
D28 -> D27 -> D26 -> D25.

---

## 3. SD init hardening (and why)

**Symptom that led here:** CMD0 returned `0x01` (card answers reset) but
**ACMD41 timed out with R1=0xFF**. CMD0 draws almost nothing; ACMD41 starts the
card's internal power-up and pulls a real current burst. On a passive board with
no bulk capacitance, the rail holds 3.2V at idle and sags during that burst.

**Changes in `ssd_initialize()`:**

| | was | now |
|---|---|---|
| Power-up settle before CMD0 | 10ms | **250ms** |
| Clock preamble | 80 clocks | **160** (spec min 74) |
| ACMD41 deadline | 2s | **5s** |
| Whole-sequence retries | 1 | **3** |

**DO NOT speed up the init clock.** It is `System::DelayUs(5)` = 100kHz, the
bottom of the spec's 100-400kHz window. A "2us / 250kHz" optimisation was tried
2026-09-13 and **broke the mount outright**. The spec headroom is theoretical;
this wiring is not.

**If SD problems return:** add **10-47uF electrolytic + 100nF at the module's
3V3 pin**, as physically close as possible. That is the real fix for sag; the
firmware changes only work around it.

**Slow transfers** are inherent: after init the driver flips `ssd_fast = true`,
a raw GPIO bit-bang with no delay (~1-2MHz after call overhead). That is the
ceiling of bit-banged SPI. Speeding it up risks the data path — the same class
of failure the OLED hit.

---

## 3b. SD FAILED? CHECK THE 3V3 SOLDER JOINT FIRST

**2026-09-14: hours were lost to a broken 3V3 solder joint on the SD module.**

The symptom set below is a POWER DELIVERY fault, not firmware. If you see it,
put a meter on the joint before touching a line of code:

- **CMD0 succeeds, ACMD41 times out.** CMD0 draws microamps and passes through
  a barely-attached joint. ACMD41 is the card's first real current draw and
  fails. This pairing IS the signature.
- **"It worked earlier, nothing changed."** A joint that is cracked but still
  touching works until it does not. Nothing has to change for it to fail.
- **Intermittent** — passes once, fails the next attempt.
- **R1 flips between 0xFF and 0x01** run to run.

Things that are NOT the cause when you see the above, all of which were chased
and eliminated: card format, card capacity/SDXC, FatFS config, init clock
speed, ACMD41 deadline, retry count, boot ordering, 3V3 rail budget.

**Do this first, always: meter continuity from the Daisy pin to the module pad
on 3V3 and GND, and wiggle the wire while watching.** Voltage at rest tells you
almost nothing -- a cracked joint reads a perfect 3.2V until current flows.

## 4. Diagnosing SD failures — the decision tree

`f_mount` FRESULT tells you which layer failed:

| FRESULT | Meaning | Where to look |
|---|---|---|
| `3` FR_NOT_READY | `ssd_initialize()` failed | card init — see below |
| `1` FR_DISK_ERR | init OK, **data reads** failed | data path / speed |
| `13` FR_NO_FILESYSTEM | card talks fine, bad format | reformat FAT32 |

Within init, instrument which command failed:

| Stage | Meaning |
|---|---|
| **CMD0 no reply, R1=`0xFF`** | MISO idle-high: **no card, no power, or a broken wire**. `0xFF` = nothing ever drove the line. |
| **ACMD41 timeout** | Card answers but will not power up: **supply sag** (or genuinely incompatible card) |

**`R1 = 0xEE`** in our test harness is the untouched sentinel — it means
`ssd_initialize()` **never ran at all**, so the verdict is meaningless. Poking
FatFS state does not reliably re-invoke disk init; call `ssd_initialize()`
directly.

**Card format:** FAT32. FatFS here is `_MAX_SS 512`, `_FS_EXFAT 0`
(`libDaisy/src/sys/ffconf.h`). exFAT will not mount. A 64GB SDXC card formatted
FAT32 **does** work — that was verified, so capacity is not the issue.

---

## 5. OLED

Panel is 4-wire SPI. **Module silkscreen says SCL/SDA but those are SPI**:
`SCL` = clock, `SDA` = MOSI. The presence of CS and DC proves SPI — I2C has
neither. **Do not hang it on the NeoTrellis I2C bus.**

| Silkscreen | Function | Daisy |
|---|---|---|
| SCL | SPI clock | **D8** |
| SDA | MOSI | **D10** |
| CS | chip select | **D7** |
| DC | data/command | **D3** |
| RES | reset | **D4** |
| VCC / GND | | 3V3 pin 38 / DGND pin 40 |

**Orientation: `0xA0` + `0xC0` = 180 degrees.** The stock `0xA1`+`0xC8`
rendered mirrored; `0xA0`+`0xC8` was still backwards (that only flips
horizontally). Both must flip.

**`nop_delay = 48`** (was 24). At 24 this panel corrupted command bytes at
runtime — it mirrored, then split in half and swapped the halves after minutes.
Ladder if it recurs: 48 -> 64 -> 96.

**Self-healing config re-assert** added to `Update()`: every ~30 frames it
re-sends remap, scan direction, start line, offset, multiplex, contrast and
display-on. A corrupted register now repairs itself within ~2s instead of
persisting. ~11 bytes, negligible.

**Contrast stays `0x01`.** The panel's boost converter under lit-pixel load
kills the Daisy's USB. It scales with LIT PIXEL COUNT, not bus traffic.

---

## 6. Joystick replaces enc2 — ONLY enc2

**`enc1` is the physical encoder and is NEVER written from the joystick.** It
keeps doing exactly what it always did.

| Input | Feeds |
|---|---|
| **X** | `enc2.delta` — the old right-encoder rotation role |
| **SW** (D24) | `enc2.psw` / `enc2.clicked` |
| **Y** | `g_joy_y` + `g_joy_nav` — a NEW axis enc2 never had. Not wired to any menu yet. |

Pins: X = **D16** (adc ch1), Y = **D17** (adc ch2), SW = **D24**.
FSR stays **adc ch0** so every `GetFloat(0)` is untouched.

**Centre is measured, not assumed** — deferred 200 polls then averaged over 64
samples. Sampling the first call risks locking in garbage as centre, after which
an axis reads pinned or dead forever. Rate-based: ~12 steps/sec at full tilt,
accumulator zeroed on recentre so scrolling stops dead.

Dashboard row shows scaled axes **plus raw ADC counts** — raw frozen while the
stick moves = that axis is not wired; raw moving but scaled not = calibration.

---

## 7. FSR

**330R pulldown, CUBED curve, full-span (16/16) ceiling.**
Canonical implementation: **`DaisyExamples/seed/_fsr_scale.h`**.

PMOR's original fixed window `(fsr-400)/2200` was tuned for the v1 10k pulldown;
with 330R the raw values sit outside it and it snapped low-to-high.

**Why 330R:** a big FSR spreads force over a large area, so under a normal
button-panel press its resistance drops to a few hundred ohms. 1.5k and 4.7k
both pinned near full scale on the lightest touch. **And an FSR is ~1/force, so
linear feels like a hair trigger at ANY resistor value** — the exponent is what
makes it playable.

---

## 8. IMU — MPU6050, not MPU9250

**The watchdog only accepted `0x71/0x73/0x70` (MPU9250 IDs). An MPU6050 returns
`0x68`.** So `imu_ok` never went true and it reinit-stormed the sensor once a
second, forever. Fixed in all three places; now accepts `0x68/0x69/0x98` too.

**Magnetometer probe skipped** unless a real 9250 is fitted. The MPU6050 has no
AK8963, and probing 0x0C cost three 10ms delays plus failed I2C transactions
(each able to burn the 5ms `waitScl` stretch timeout) at boot **and every second
in the watchdog**.

`ax` still drives the SVF tilt-filter cutoff, unchanged. All six accel/gyro
values are mirrored to globals for the dashboard.

---

## 9. Patches / sessions

**`plantExampleBeatIfRoomExists()` is DISABLED.** It wrote a hardcoded demo beat
into the first empty slot on **every boot** — with `seq_mode[ENG_DRUM] =
SEQ_MACHINE` ("drum pattern actually plays on load") and `beat.menu = MENU_DRUM`.
That single function caused all three symptoms: phantom extra patches, a beat
starting when you tapped one, and being thrown off the patches page.

**Loading a patch no longer leaves the patches page.** `persistApply()` restores
the saved `menu`; `sessionLoad()` now preserves the current page across it.
Loading a patch changes the SOUND, not where you are.

**`SESS_MAGIC` bumped `'OPG3'` -> `'OPG4'`** to factory-reset all 16 slots.
Bump it again any time you want a clean slate.

**Clean seed:** `sessionsBoot()` used to seed slot 0 from whatever was live at
boot, so test patterns got baked in as "defaults". All pattern data is now
explicitly zeroed.

---

## 10. Volume

**Default pad 10 (-7.27 dB).** History: pad 9 (-10.91) -> pad 12 (unity, slammed
the limiter on real speakers) -> **pad 10**. Unity at the DSP bus is already hot
with the PAM8403's fixed gain in front of the drivers. Pads 11-15 remain
available; 13-15 deliberately lean on the tanh limiter.

---

## 11. Flashing — bootloader mode MUST match APP_TYPE

| APP_TYPE | App lives in | DFU mode to use |
|---|---|---|
| `BOOT_NONE` | internal flash `0x08000000` | **`BootloaderMode::STM`** |
| `BOOT_SRAM` | QSPI `0x90040000` | **`DAISY_INFINITE_TIMEOUT`** |

**Get this wrong and reflash silently cannot work.** `BootloaderMode::STM` boots
the ST ROM loader, whose DFU exposes **only** internal flash — writing a
BOOT_SRAM app to QSPI is rejected with
`dfu-util: Last page at 0x90053d6b is not writeable`.

`_bringup_dfu.h` now takes `BRINGUP_DFU_MODE` (defaults to `STM`).

**PMOR_V2 is `BOOT_SRAM`**, so it needs the Daisy bootloader installed:

```
make program-boot     # once (also needed after any BOOT_NONE build)
# then catch the bootloader window:
printf 'dfu\n' > /dev/cu.usbmodem*
dfu-util -a 0 -s 0x90040000:leave -D build/PMOR_V2.bin -d ,0483:df11
```

A **catcher loop** (poll `dfu-util -l`, flash on sight) snipes the window
reliably. Success is the literal string `File downloaded successfully`.

---

## 12. Boot time

`PMOR_STAGE` compiles to nothing without `make CHUNK=n` — it is not the cost.
The magnetometer skip (§8) removed real waste. Remaining known blocking delays:
500ms USB PHY settle, 150ms soft-disconnect, 150ms MPU reset/wake, ~545ms
`ntInit`, plus `lidar.init()` and the SD mount.

---

## 13. USB

**The USB adapter/hub breaks enumeration.** Repeatedly: board invisible through
it, appears immediately on a direct connection. Not the board, not the firmware.

`ioreg -p IOUSB -w0` shows the whole tree — when the Daisy vanished, a whole
branch of hubs had vanished with it.

---

## 14. Known-good / DO NOT CHANGE

- SD init clock `DelayUs(5)` = 100kHz
- SD module on **3.3V**, never 5V
- OLED contrast `0x01`
- `NVIC_DisableIRQ(SPI1_IRQn)` after `display.Init()`
- The trellis `waitScl` clock-stretch hardening + `BBus::recover()` + 250ms backoff
- Daisy Seed 3 works on **libDaisy v4.0.0** — undetected board falls through to
  `default:`, which IS the correct Seed3 audio path. Seed3 detection is
  master-only (PR #710, merged 2026-09-11).

---

## 15. Next

- **Software refactor** (the reason this doc exists — capture before compaction)
- Wire `g_joy_nav` into a submenu
- NeoTrellis 90-degree rotation (needs remapping BOTH the LED buffer and the key decode)
- Sampler audio save: chunked, only promoted from temp on completion — needs a
  `SAVING/SAVED/FAIL` indicator and an explicit flush
- Housing: see `HOUSING-v2-BRIEF.md`
- **This repo has no git remote.** Push it somewhere.

## Related docs
- `WIRING-v2.md` — pin map, power topology, ground rules
- `JACK-PINOUT.md` — 5-pin 3.5mm jacks, measured and locked
- `HOUSING-v2-BRIEF.md` — panel-mount housing spec
- `_fsr_scale.h` · `_bringup_dfu.h` — shared, documented in-file

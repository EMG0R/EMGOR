# Pocket-OpGorator v2 — Wiring

Supersedes the v1 pin map in `pocket-OpGorator.md`.

**v2 changes:** stereo speakers kept (8Ω, PAM8403) · right encoder → analog joystick ·
2× XL3608 (split digital / audio rails) · isolated clock island (analog sync + MIDI TRS-A) ·
USB audio dropped (USB = MIDI + DFU + charging only).

Pin numbering D0–D30 is identical across Daisy Seed revisions, so this is revision-independent.
Only the codec init differs, and that is firmware-side.

---

## 1. Pin map — LOCKED

Derived from the **current PMOR build** (`DaisyExamples/seed/POCKETGORATOR_CODE/POCKETGORATOR_CODE.cpp`,
Aug 18), not from `pocket-OpGorator.md` — the doc and the code disagree, and the code wins.

Only three things change from working PMOR: enc2 becomes the joystick (same pins),
plus two new outputs for the isolated clock.

| Daisy | Net | Status | PMOR ref |
|-------|-----|--------|----------|
| D0  | ENC_A        | unchanged | `:3977` |
| D1  | ENC_B        | unchanged | `:3977` |
| D2  | ENC_SW       | unchanged | `:3977` |
| D3  | OLED_DC      | unchanged | |
| D4  | OLED_RST     | unchanged | |
| **D5**  | **USART5_RX -> MIDI IN**  | **NEW** | PD2, was free |
| **D6**  | **USART5_TX -> MIDI OUT** | **NEW** | PC12, was free |
| D7  | OLED_CS      | unchanged | SPI1 NSS |
| D8  | SPI1_SCK     | unchanged | |
| D9  | *free*       | | |
| D10 | SPI1_MOSI    | unchanged | |
| D11 | `tbus` SCL   | unchanged | NeoTrellis, bit-banged `:3936` |
| D12 | `tbus` SDA   | unchanged | |
| D13 | `sbus` SCL   | unchanged | MPU9250 + VL53L1X, bit-banged `:3934` |
| D14 | `sbus` SDA   | unchanged | |
| D15 | FSR          | unchanged | A0 `:3983` |
| **D16** | **JOY_X** | **CHANGED** — was enc2 B | A1, analog |
| **D17** | **JOY_Y** | **CHANGED** — was enc2 SW | A2, analog |
| D18 | SHDN_CTRL    | unchanged | -> PAM8403 SHDN |
| D19 | HP_DETECT    | unchanged | <- PJ-307 pin 4 via RC |
| D20 | VBAT_SENSE   | unchanged | A5 |
| **D21** | **SYNC_OUT** | **NEW** | -> PC817 opto |
| D22 | *free*       | | |
| D23 | *free*       | | |
| **D24** | **JOY_SW** | **CHANGED** — was enc2 A | |
| D25 | SD_MISO      | unchanged | `:3303`, internal PULLUP |
| D26 | SD_CLK       | unchanged | `:3301` |
| D27 | SD_MOSI      | unchanged | `:3302` |
| D28 | SD_CS        | unchanged | `:3304` |
| D29 | *free* (PB14) | USART1_TX **or** external USB_HS D-. Reserved for OTG. |
| D30 | *free* (PB15) | USART1_RX **or** external USB_HS D+. Reserved for OTG. |

### SD card — DO NOT CHANGE

PMOR drives the SD with a **hand-written FatFS diskio shim** (`SPISD_Driver` =
`ssd_initialize / ssd_status / ssd_read / ssd_write / ssd_ioctl`) over **bit-banged SPI**,
mounted via `FATFS_LinkDriver` + `f_mount`. `f_open` works, sample streaming works.

Same module, same 3V3 supply, same four pins, same driver. Port it forward untouched.
No SDMMC, no socket rewiring, no external pull-ups (D25 uses the internal pull-up).

### I2C — both buses are bit-banged

`tbus` (D11/D12) and `sbus` (D13/D14) are software I2C via PMOR's `BBus` class, not the
STM32 I2C peripheral. Keep both. Every breakout keeps its factory pull-ups — this is
already proven working hardware.

The `waitScl` stretch-timeout + `BBus::recover()` + 250ms backoff hardening
(PMOR-HANDOFF item 24, 300s gauntlet clean) must be preserved.

### MIDI on D5/D6 — USART5

**Board is Daisy Seed 3** (confirmed from the official pinout, 2026-09-12).

**Correction (research, 2026-09-12):** an earlier revision of this doc claimed D29/D30 became
USB-only on Seed3. **False alarm.** The official Seed3 pinout CSV row 36/37 reads
`D29,PB14,GPIO,USB_HS_D-/USART1_TX` and `D30,PB15,GPIO,USB_HS_D+/USART1_RX` — *identical* to
the Seed 1.x CSV. Those pins are dual-purpose on every revision; the Seed3 diagram just draws
the USB label more prominently. All 40 pinout rows are byte-identical between Seed 1.x and Seed3.

**So both options work. Choosing USART5 on D5/D6 anyway**, to leave D29/D30 free for external
USB / OTG host — the Seed3 datasheet Figure 1.3 documents them as the external USB-C pins, and
MIDI at 31250 baud needs nothing USART1 offers over USART5.

UART options and their collisions with current PMOR usage:

| UART | Pins | Collision |
|------|------|-----------|
| USART1 | D13/D14 | `sbus` (sensors) |
| UART4  | D11/D12 | `tbus` (NeoTrellis) |
| USART3 | D1/D2   | enc1 B/SW |
| **USART5** | **D5/D6** | **none — free** |

D5/D6 carry SD CMD/CLK in Seed 3's *native* SDMMC role, but **PMOR does not use native SD** —
it bit-bangs SPI on D25-D28. So D5/D6 are genuinely unused.

- **D6 = USART5 TX -> MIDI OUT** (PC12)
- **D5 = USART5 RX -> MIDI IN** (PD2)

> Confirm against `per/uart.cpp`: `uart5_pins_tx[] = {PC12, ...}`, `uart5_pins_rx[] = {..., PD2, ...}`.

### Seed 3 simplifications vs the v1 plan

- **Onboard USB-C.** Delete the v1 "USB-C breakout -> flying leads -> micro-USB male plug"
  hack entirely. Cable goes straight into the Seed.
- **Pin 39 = VIN, pin 40 = DGND, pin 38 = 3V3 Digital, pin 21 = 3V3 Analog, pin 20 = AGND.**
- D0-D28 keep the same STM32 mapping as earlier Seeds (D1-D6 = SD Data3..CLK = PC11/PC10/PC9/PC8/PD2/PC12),
  so all existing PMOR pin usage ports unchanged. **Only D29/D30 changed meaning.**

### Seed3 vs libDaisy v4.0.0 — NOT a blocker (resolved 2026-09-12)

An earlier revision called this a build blocker. It is not.

- **Seed3 support landed in libDaisy PR #710, merged 2026-09-11 — master only, no tagged
  release** (latest tag v8.1.0 predates it). Our checkout is v4.0.0 (`85172e2b`).
- **v4.0.0 still produces correct firmware for Seed3.** Undetected board versions fall through
  to `default:` in `ConfigureAudio()`, and that default *is* the correct Seed3 path —
  reset-pin-only codec init. libDaisy master's own comment: *"Same configuration used for
  Seed Rev4, Seed Rev7 (1.2), and Seed3"*.
- **PROVEN HERE:** TRELLIS_ONLY built on v4.0.0 flashed and ran on Seed3, 2026-09-12.
- **PMOR is safe** — it contains no `CheckBoardVersion()` / `BoardVersion` branching, so there
  is no wrong-branch risk. (That risk is real for code that special-cases `DAISY_SEED_1_1`,
  which is the one revision with a WM8731 and *reversed* SAI directions.)
- `System::ResetToBootloader(BootloaderMode::STM)` is unchanged on master; PR #710 did not
  touch `sys/system.h`. Buttonless DFU workflow is intact.

**Upgrade to master only if** you want `CheckBoardVersion()` to report Seed3, or want 192kHz.

### Seed3 hardware facts worth knowing

- **Codec is the TI TAC5242** (not PCM3060, and Seed 1.1 was actually WM8731 — the v1 doc's
  "PCM3060" claim was for Rev7/Seed 1.2). The TAC5242 is **hardware-configured — it ignores
  I2C.** Any code poking codec registers becomes a silent no-op.
- **192kHz / 32-bit is opt-in, not automatic.** `DaisySeed::Init()` still hardcodes
  `SAI_48KHZ` / `SAI_24BIT` for Seed3. The `SAI_192KHZ` enum exists but nothing selects it.
- **Not 5V tolerant: D17/PB1, D18/PA7, D21/PC4, D22/PA5, D23/PA4.** Note D17 is JOY_Y and
  D18 is SHDN_CTRL — both in this design. Another reason the joystick runs on 3V3.
- **JTAG/SWD header is unpopulated** on Seed3 — 2x7 1.27mm pads, connector not in the box
  (MPN 20021111-00010T4LF). STDC14 pins 1/2/13/14 are NC, so no virtual-COM over the debugger.
- Onboard USB-C is `USB_OTG_FS`; D29/D30 are the separate `USB_HS` external pair.


---

## 2. Power — two boosts

```
LiPo ── TP4056 + DW01A ── VBAT_RAW ── Q1 AO3401 (P-MOSFET load switch) ── VBAT_SW
                                          gate ← EG1218 SPDT, 10k pullup to source
                                                                             │
                    ┌────────────────────────────────────────────────────────┴──┐
                    ▼                                                           ▼
          XL3608 #A  "DIGITAL 5V"                                XL3608 #B  "AUDIO 5V"
                    │                                                           │
                    ├── Daisy VIN                                               └── PAM8403 VCC ONLY
                    ├── NeoTrellis VIN
                    ├── SD module VIN
                    └── OLED VIN
```

MAX4466 mic runs from **Daisy 3V3**, not either 5V rail. (Unchanged from v1 — lowest-noise rail.)

### Decoupling — do not skip

| Location | Parts |
|---|---|
| XL3608 #A output | 10µF + 100nF |
| XL3608 #B output | **470µF electrolytic + 100nF** |
| PAM8403 VCC pins | **470µF + 100nF, as physically close as possible** |
| NeoTrellis VCC | 100µF + 100nF |
| Every other module VCC | 10µF + 100nF |

The 470µF at the amp is the single most important part in this document. It is the local
energy reservoir that supplies bass transients so the rail never sags.

### Star ground

One join point: **battery negative.** Run separate returns to it — do not daisy-chain.

```
                    ┌── XL3608 #A GND  ──→ digital module grounds
BATTERY (−) ────────┼── XL3608 #B GND  ──→ PAM8403 GND + speaker returns
   STAR POINT       ├── NeoTrellis GND  (its own wire — LED bursts)
                    └── Daisy GND
```

Speaker return currents must never share a conductor with digital grounds.

### DW01A warning

Peak battery-side current is ~3.0A (5V × 1.9A ÷ 3.7V ÷ 0.85). A 10Ah cell handles 0.3C
trivially, but the **DW01A/8205A protection on the TP4056 module trips somewhere around
3–6A** depending on FET Rds(on), and the module traces are thin.

Symptom if you hit it: device cuts out under bright LEDs + loud audio. That is the protection
IC, not the cell. Fix is to bypass the module's discharge FETs or move to a BQ24074.

---

## 3. Audio out + speaker mute (unchanged from v1, plus ground lift)

```
Daisy AUDIO_OUT_L ──┬──→ PJ-307 TIP  (headphones)
                    └──→ PAM8403 L_IN
Daisy AUDIO_OUT_R ──┬──→ PJ-307 RING
                    └──→ PAM8403 R_IN

PAM8403 L_OUT+/−  ──→ Left speaker  (8Ω, SEALED rear chamber)
PAM8403 R_OUT+/−  ──→ Right speaker (8Ω, SEALED rear chamber)

PJ-307 pin 4 ── 10k to 3V3 ── 100nF to GND ──→ D19 (HP_DETECT)
D18 (SHDN_CTRL) ──→ PAM8403 SHDN, with 10k pulldown to GND
```

`HP_DETECT` LOW = no plug → drive `SHDN_CTRL` HIGH (speakers on).
`HP_DETECT` HIGH = plug in → drive `SHDN_CTRL` LOW (muted).


### 3b. Jack pin mapping — MEASURED AND LOCKED 2026-09-13

Physical positions, viewed with **pins toward you and the barrel pointing away**:

| Physical | Function |
|----------|----------|
| **T** top (single pin, upper row) | **SLEEVE** |
| **L** leftmost (lower row)        | **RING** |
| **LM** left-middle                | **NC→RING** |
| **RM** right-middle               | **NC→TIP** |
| **R** rightmost                   | **TIP** |

Outer pins are the live contacts; the inner pin beside each is its switch.
Full detail + the measurement procedure: `JACK-PINOUT.md`.

**Both jacks use the NC contacts for HARDWARE auto-switching — no firmware, no GPIO.**
- INPUT jack: mic is live with no plug, disconnects when a cable is inserted.
- OUTPUT jack: amp gets signal with no plug, disconnects when headphones go in.

**`D19` HP_DETECT is FREED** — the headphone mute is mechanical now. `D18` SHDN_CTRL
stays, but only for power-event pop suppression.

### Ground lift — NEW

**PJ-307 sleeve → 10Ω resistor ∥ 100nF capacitor → device GND.**

Two parts in parallel, between the jack sleeve and ground. This breaks the DC ground loop
that forms when you plug into the Pi, while keeping RF shielding intact.

### Sealed rear chambers — NEW, and the biggest audio win here

v1 mounted the drivers open-backed, firing sideways into the external "ears". That is a
dipole: front and back radiation cancel through the midrange, which is where small drivers
make their loudness.

Give each driver a **sealed 5–10cc rear chamber** in the housing. No parts, no current,
no cost. Do this even if you change nothing else.

---

## 4. Clock — USB MIDI + PO-sync. NO isolation hardware. (decided 2026-09-12)

**Decision: clock goes over USB MIDI. The isolated clock island is CANCELLED.**
Do not order the B0505S, the PC817/6N137, or the extra TRS jacks.

Why it is safe: a ground loop needs TWO ground paths. USB alone is one shared
reference, which is fine. The loop only appears if audio ALSO runs between PMOR
and the Pi. PMOR's audio goes to its own speakers / its own jack, so there is no
second path.

Already proven on hardware: 120.046 BPM measured (+0.04%), duplicate-`0xF8` bug
fixed via `tud_midi_packet_write()`, Pi-side PLL 172/172 tests green. Keep the
6.25ms / 125ms plausibility gates on both sides.

Limit: **+/-1ms jitter is a hard floor** over USB (1ms frames) -- about what DIN
MIDI has always given. Tempo still locks under 0.1% because the PLL averages many
ticks. Escalation if ever needed: PO-sync click into a NEPTR audio input at sample
resolution (~0.02ms); the click generator already exists in firmware, unwired.

**PO-sync out stays** for pocket operators, over the audio jack as POs expect.
Those are battery powered with no mains reference, so no loop there either.

## 4b. External USB-C breakout (Seed3)

Seed3 has an onboard USB-C, but a panel-mount breakout is still wanted so the
port location is independent of where the Seed sits.

```
Panel USB-C breakout
  VBUS (5V) --> TP4056 IN+        <- charging power, straight from the cable
  GND       --> star ground
  D+        --> Daisy D30
  D-        --> Daisy D29
```

- **Take charging power from VBUS, never from the Seed's 3V3.** A LiPo sits at
  3.0-4.2V, so 3.3V is *below* a charged cell and cannot charge it; the 3V3 pin is
  a regulated output with a small current budget, not a supply.
- **This is why D29/D30 were kept free** -- Seed3 datasheet Figure 1.3 documents
  them as the external USB pair. MIDI lives on D5/D6 precisely to leave these open.
- Do not bridge breakout VBUS to Seed VIN. The Seed's D1/D2 diodes block
  VIN->VBUS backfeed (verified in v1), but the reverse direction is on you.

## 4c. (CANCELLED) Isolated clock island

One galvanic barrier, two connector formats. Nothing downstream shares ground with the device.

```
DEVICE SIDE                    │ BARRIER │            ISOLATED SIDE
                               │         │
+5V (rail A) ──→ B0505S-1W ────┼─────────┼──→ ISO_5V  (floating)
GND          ──→              ─┼─────────┼──→ ISO_GND (floating — NOT device GND)
                               │         │
D21 SYNC_OUT ─220Ω─→ PC817 LED─┼──[opto]─┼──→ transistor: collector ISO_5V
                               │         │                emitter  → SYNC TRS TIP
                               │         │                10k tip → ISO_GND (pulldown)
                               │         │
D29 MIDI_TX  ─220Ω─→ 6N137 ────┼──[opto]─┼──→ standard MIDI OUT resistor pair
                               │         │    referenced to ISO_5V / ISO_GND
```

**Both TRS sleeves connect to ISO_GND. Never to device ground.** That is the whole point.

| Part | Why |
|---|---|
| **B0505S-1W** | Isolated 5V DC-DC, ~$1.50, ~12×10mm. Not optional — a bare opto can only pull *down*, and Volca/PO sync inputs idle low and need a positive 5V pulse. The far side needs its own supply. |
| **PC817** | Cheap opto, fine for a 15ms 2-PPQN sync pulse. |
| **6N137** | Fast opto — needed for 31250 baud MIDI. A PC817 is too slow here. |

### Analog sync jack (PO / Volca)
Tip = 5V pulse out · Ring = unused or mono audio · Sleeve = **ISO_GND**

### MIDI jack (TRS Type-A)
> **VERIFY:** take the exact resistor values and tip/ring polarity from the official
> MIDI Association TRS Type-A reference schematic (220Ω pair for 5V logic). I am not
> going to guess the polarity for you — getting it backwards is a silent no-op and an
> annoying thing to debug.

### For NEPTR
Use the **MIDI jack, not USB.** USB to the Pi shares ground and reintroduces the exact loop
this island exists to kill. The Pi-side PLL (`demiurge-clock-pll.h`) already handles DIN-rate
MIDI clock — keep the 6.25ms / 125ms plausibility gates on both sides.

---

## 5. Joystick — replaces right encoder

Self-centering (spring-return), with center push. **ALPS RKJXV122400R** or equivalent.

| Joystick | Daisy | |
|---|---|---|
| X wiper | D16 (A1) | analog |
| Y wiper | D17 (A2) | analog |
| Push SW | D6 | to GND, internal pullup |
| VCC | 3V3 | **3V3, not 5V** — Daisy ADC is 3.3V max |
| GND | GND | |

100nF from each wiper to GND. Firmware needs a center deadzone (±3–5% of full scale)
and per-axis center calibration stored on SD — these sticks never center perfectly.

> **VERIFY:** your locked encoder-R position is (57.9, 62.17) on a 16mm-high hump. Check the
> ALPS body depth against the PCB-to-top-face budget there before committing to the housing.

> **VERIFY:** confirm D16/D17 are ADC-capable on your board revision (they are A1/A2 in the
> v1 doc, so this should be fine, but check the DaisyWiki CSV).

---

## 6. Free diagnostic tests — run these before building

Both cost nothing and tell you what you are actually dealing with.

1. **NeoTrellis brightness to zero, listen.**
   Tick changes → confirmed supply sag → the second boost + 470µF fixes it.
   Tick unchanged → not the LED rail → it is ground loop, sync click, or mute logic.

2. **Set SY mode to SY0, listen.**
   Tick vanishes → you were hearing the 2-PPQN click generator the whole time and
   there was never a hardware bug.

---

## 7. Build order

1. Power: battery → TP4056 → MOSFET switch → both XL3608s. **Verify both outputs read 5.0V
   on a meter before connecting anything downstream.**
2. Star ground wiring.
3. Daisy on machine-pin sockets. Blink test.
4. Digital rail modules: OLED, SD, NeoTrellis, sensors on I2C1.
5. Encoder, joystick, FSR, VBAT divider.
6. Audio: codec out → jack + amp. Ground lift. Speakers in sealed chambers.
7. Isolated clock island. Verify with a meter that ISO_GND is **not** continuous with
   device GND — this is the one test that proves the barrier exists.
8. Diagnostic tests from §6.

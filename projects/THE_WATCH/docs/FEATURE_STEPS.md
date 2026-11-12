# FEATURE: Step Tracking (Pedometer) — QMI8658 on the AMOLED 1.43 watch

Implementation plan (RESEARCH ONLY — no firmware changed by this doc).

Target: `firmware/the_watch_amoled/` (Waveshare ESP32-S3-Touch-AMOLED-1.43).
IMU: **QMI8658** 6-axis, I2C addr **0x6B**, on the shared bus **SDA=47 / SCL=48**
(same bus as FT3168 touch @0x38 and PCF85063 RTC).

---

## 0. The hard constraint (why this is not "just add SensorLib")

The vendor touch/display bring-up (`FT3168.cpp` → `Touch_Init()`) installs the
**LEGACY** ESP-IDF I2C driver on **port 0** via `i2c_driver_install(I2C_NUM_0,…)`.
Merely linking Arduino `Wire`/`SensorLib` pulls in the **new "driver_ng"**, and
ESP-IDF `abort()`s at boot with `CONFLICT! driver_ng is not allowed to be used
with this old driver` → reboot loop (confirmed on hardware 2026-07-27; see
`amoled-143-verified-baseline.md` and the note in `the_watch_amoled.ino`).

**Therefore the pedometer module MUST talk to the QMI8658 using the legacy
driver already installed on port 0** — i.e. `i2c_master_write_to_device()` /
`i2c_master_write_read_device()` from `driver/i2c.h`, exactly like
`FT3168.cpp`'s `I2C_writr_buff()` / `I2C_read_buff()`. **Do NOT** `#include
<Wire.h>` or `<SensorLib.h>` / `SensorQMI8658.hpp` anywhere in the build. We
reimplement the tiny slice of SensorLib we need (init + pedometer) against the
legacy driver.

NVS/`Preferences` (Section 4) is fine — it does **not** touch I2C, so it does
not trigger the driver_ng conflict.

---

## 1. QMI8658 pedometer: hardware engine vs software fallback

### 1a. Built-in hardware pedometer (RECOMMENDED)

The QMI8658 has an on-chip step-counting engine that runs autonomously from the
accelerometer stream. We enable it once, then just read a 24-bit step counter —
almost zero CPU/power cost on the ESP32 side. Values below are from the QMI8658A
datasheet register map (QST Doc# 13-52-25 Rev A) cross-checked against the
reference driver `lewisxhe/SensorLib` `SensorQMI8658.hpp` (the same chip family
Waveshare ships).

**Register map (all hex, UI/I2C register bank):**

| Reg  | Name         | Use                                                        |
|------|--------------|------------------------------------------------------------|
| 0x00 | WHO_AM_I     | must read **0x05** (chip present) — device ID              |
| 0x01 | REVISION_ID  | revision                                                   |
| 0x02 | CTRL1        | serial iface: bit6=1 → **address auto-increment, little-endian** (write 0x40) |
| 0x03 | CTRL2        | accel: bits[7:4]=range, bits[3:0]=ODR                      |
| 0x08 | CTRL7        | enable engines: **bit0 = aEN** (accel), bit1 = gEN        |
| 0x09 | CTRL8        | motion engines: **bit4 = pedometer enable (PedoEN)**      |
| 0x0A | CTRL9        | command register (host↔engine handshake)                  |
| 0x0B..0x12 | CAL1_L..CAL4_H | parameters passed to CTRL9 commands              |
| 0x2D | STATUSINT    | **bit7 (0x80) = CmdDone** (CTRL9 handshake flag)          |
| 0x2E | STATUS0      | data-ready flags                                          |
| 0x2F | STATUS1      | event flags (incl. pedometer/sig-motion when INT mapped)  |
| 0x5A | STEP_CNT_LOW  | step counter bits [7:0]                                   |
| 0x5B | STEP_CNT_MIDL | step counter bits [15:8]                                  |
| 0x5C | STEP_CNT_HIGH | step counter bits [23:16]                                 |

Step count is a **24-bit** running total (wraps at 16,777,215), little-endian:
`steps = low | (mid<<8) | (high<<16)`.

**CTRL9 command handshake** (how CONFIGURE/RESET commands are executed):
1. Write the command byte to **CTRL9 (0x0A)**.
2. Poll **STATUSINT (0x2D)** until **bit7 (0x80)** is set (CmdDone), with a
   timeout (~1 s).
3. Write **CTRL_CMD_ACK = 0x00** to CTRL9 (0x0A).
4. Poll STATUSINT until bit7 clears again. Done.

**CTRL9 command codes used here:**
- `CTRL_CMD_ACK` = **0x00**
- `CTRL_CMD_CONFIGURE_PEDOMETER` = **0x0D**
- `CTRL_CMD_RESET_PEDOMETER` = **0x0F** (zeros the hardware step counter)

**Configuring the pedometer** — the tuning parameters are loaded into the CAL
registers, then committed with **two** `CONFIGURE_PEDOMETER (0x0D)` CTRL9
commands (a two-block protocol; CAL4_H selects which block, 0x01 then 0x02):

Block 1 (CAL4_H=0x01, CAL4_L=0x02):
- CAL1 (L,H) = `ped_sample_cnt`   (samples per detection window, e.g. 0x0032 = 50)
- CAL2 (L,H) = `ped_fix_peak2peak`(min peak-to-peak accel, e.g. 0x00CC)
- CAL3 (L,H) = `ped_fix_peak`     (min peak accel, e.g. 0x0066)

Block 2 (CAL4_H=0x02, CAL4_L=0x02):
- CAL1 (L,H) = `ped_time_up`      (max samples between steps, e.g. 0x00C8 = 200)
- CAL2_L = `ped_time_low`         (min samples between steps, e.g. 0x14 = 20)
- CAL2_H = `ped_time_cnt_entry`   (consecutive steps to start counting, e.g. 0x0A = 10)
- CAL3_L = `ped_fix_precision`    (0x00)
- CAL3_H = `ped_sig_count`        (steps kept after inactivity, e.g. 0x04)

Those defaults are SensorLib's recommended starting values and are a good
baseline; tune `peak2peak`/`peak` down if steps under-count, up if wrist motion
false-triggers.

**Bring-up sequence (init):**
1. WHO_AM_I (0x00) must == 0x05 (else bail, log, mark IMU absent).
2. Soft-reset optional (write 0xB0 to reg 0x60 RESET, wait ~15 ms) — optional;
   or just configure fresh.
3. CTRL1 (0x02) = 0x40 (auto-increment, little-endian).
4. Configure accel: CTRL2 (0x03) — range **±8g** (0x20) is fine for stepping;
   ODR **125 Hz** (low nibble 0x03) or lower. A modest ODR keeps the engine fed
   while saving power. e.g. CTRL2 = 0x23.
5. `configPedometer(...)` (the two-block CAL/CTRL9 dance above).
6. Enable accel: set **CTRL7 (0x08) bit0** (aEN). Gyro stays OFF (power).
7. Enable pedometer: set **CTRL8 (0x09) bit4** (PedoEN).
   (SensorLib requires accel enabled before pedometer enable.)
8. Poll `STEP_CNT_*` at leisure.

**Polling vs interrupt:** The engine counts autonomously, so **polling** every
few seconds from the existing UI timer is the simplest and cheapest approach —
no INT pin wiring assumptions needed (the QMI8658 INT1/INT2 lines are not
brought out to a known GPIO in this firmware, and `STATUS1` pedometer flags only
latch when an INT is mapped via CTRL8/`configActivityInterruptMap`). Recommend
**poll-only** for v1. Interrupt-driven (INT pin → GPIO ISR → wake) is a later
power optimization if a free INT GPIO is identified on the board.

**Power:** With only the accelerometer on at ≤125 Hz and gyro off, the QMI8658
draws on the order of tens–hundreds of µA; the step engine adds negligible host
cost since the ESP32 only reads 3 bytes every few seconds. No extra thermal
concern (board heat is dominated by the always-on USB regulator per the
baseline notes).

### 1b. Software fallback (accel-magnitude peak detection)

Only needed if the hardware engine proves unreliable or WHO_AM_I fails. Read raw
accel (AX/AY/AZ at 0x35..0x3A, 16-bit LE) at ~25–50 Hz, compute magnitude
`m = sqrt(ax²+ay²+az²)`, high-pass (subtract a slow running mean), and count
peaks that (a) exceed a dynamic threshold and (b) are ≥ ~250 ms apart (step
cadence 0.5–3 Hz). This costs far more CPU (must sample continuously — cannot
sleep the loop) and RAM, and is less accurate than the tuned hardware engine.
**Recommendation: use 1a; keep 1b documented as fallback only.**

---

## 2. Reading the QMI8658 via the LEGACY I2C driver (port 0, pins 47/48)

Self-contained helpers modeled on `FT3168.cpp`. **No Wire, no SensorLib.** The
legacy driver is already installed by `Touch_Init()` before this runs, so we do
**not** call `i2c_driver_install` again (the FT3168 owns port 0 config).

```c
// imu_pedometer.cpp  (compiles as C++; uses ESP-IDF legacy I2C on I2C_NUM_0)
#include "driver/i2c.h"
#define IMU_PORT   I2C_NUM_0
#define IMU_ADDR   0x6B          // WATCH_IMU_ADDR

static esp_err_t imu_rd(uint8_t reg, uint8_t *buf, size_t len) {
  return i2c_master_write_read_device(IMU_PORT, IMU_ADDR, &reg, 1, buf, len, 1000 / portTICK_PERIOD_MS);
}
static esp_err_t imu_wr(uint8_t reg, uint8_t val) {
  uint8_t b[2] = { reg, val };
  return i2c_master_write_to_device(IMU_PORT, IMU_ADDR, b, 2, 1000 / portTICK_PERIOD_MS);
}
static esp_err_t imu_set_bit(uint8_t reg, uint8_t bit) {
  uint8_t v; if (imu_rd(reg, &v, 1) != ESP_OK) return ESP_FAIL;
  return imu_wr(reg, v | (1u << bit));
}
static esp_err_t imu_clr_bit(uint8_t reg, uint8_t bit) {
  uint8_t v; if (imu_rd(reg, &v, 1) != ESP_OK) return ESP_FAIL;
  return imu_wr(reg, v & ~(1u << bit));
}

// CTRL9 command handshake (STATUSINT 0x2D bit7 = CmdDone)
static bool imu_ctrl9(uint8_t cmd) {
  if (imu_wr(0x0A, cmd) != ESP_OK) return false;           // CTRL9
  uint32_t t0 = millis(); uint8_t s;
  do { if (imu_rd(0x2D,&s,1)!=ESP_OK) return false;         // STATUSINT
       if (millis()-t0 > 1000) return false;
     } while (!(s & 0x80));
  if (imu_wr(0x0A, 0x00) != ESP_OK) return false;           // CTRL_CMD_ACK
  t0 = millis();
  do { if (imu_rd(0x2D,&s,1)!=ESP_OK) return false;
       if (millis()-t0 > 1000) return false;
     } while (s & 0x80);
  return true;
}

bool imu_pedometer_init(void) {
  uint8_t who = 0;
  if (imu_rd(0x00, &who, 1) != ESP_OK || who != 0x05) {     // WHO_AM_I == 0x05
    Serial.printf("IMU: WHO_AM_I=0x%02X (expected 0x05) — pedometer disabled\n", who);
    return false;
  }
  imu_wr(0x02, 0x40);                    // CTRL1: auto-increment, little-endian
  imu_wr(0x03, 0x23);                    // CTRL2: accel ±8g, 125 Hz ODR

  // configPedometer block 1
  imu_wr(0x0B, 0x32); imu_wr(0x0C, 0x00);   // CAL1 = sample_cnt (50)
  imu_wr(0x0D, 0xCC); imu_wr(0x0E, 0x00);   // CAL2 = fix_peak2peak
  imu_wr(0x0F, 0x66); imu_wr(0x10, 0x00);   // CAL3 = fix_peak
  imu_wr(0x12, 0x01); imu_wr(0x11, 0x02);   // CAL4_H=0x01, CAL4_L=0x02
  imu_ctrl9(0x0D);                          // CONFIGURE_PEDOMETER
  // configPedometer block 2
  imu_wr(0x0B, 0xC8); imu_wr(0x0C, 0x00);   // CAL1 = time_up (200)
  imu_wr(0x0D, 0x14); imu_wr(0x0E, 0x0A);   // CAL2_L=time_low, CAL2_H=cnt_entry
  imu_wr(0x0F, 0x00); imu_wr(0x10, 0x04);   // CAL3_L=precision, CAL3_H=sig_count
  imu_wr(0x12, 0x02); imu_wr(0x11, 0x02);   // CAL4_H=0x02, CAL4_L=0x02
  imu_ctrl9(0x0D);

  imu_set_bit(0x08, 0);                  // CTRL7 bit0: accel enable (aEN)
  imu_set_bit(0x09, 4);                  // CTRL8 bit4: pedometer enable
  Serial.println("IMU: QMI8658 pedometer up");
  return true;
}

uint32_t imu_pedometer_read(void) {      // 24-bit LE step counter
  uint8_t b[3];
  if (imu_rd(0x5A, b, 3) != ESP_OK) return 0;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
}

void imu_pedometer_reset_hw(void) {      // zero the on-chip counter
  imu_ctrl9(0x0F);                       // CTRL_CMD_RESET_PEDOMETER
}
```

Header `imu_pedometer.h` exposes `imu_pedometer_init()`,
`imu_pedometer_read()`, `imu_pedometer_reset_hw()`.

**Notes:**
- `i2c_master_write_read_device` / `i2c_master_write_to_device` are the exact
  primitives already proven on this board (`FT3168.cpp`). Reusing them means the
  QMI8658 shares the bus with touch cleanly — the legacy driver serializes
  transactions on port 0.
- `Touch_Init()` configures port 0 at 300 kHz with pull-ups enabled — fine for
  the QMI8658 (supports up to 400 kHz).
- Optional: could instead reuse FT3168.cpp's `I2C_read_buff`/`I2C_writr_buff`
  directly, but they're not declared in `FT3168.h`; keeping the IMU module
  self-contained (its own static helpers) is cleaner and avoids touching the
  known-good vendor file.

---

## 3. Firmware integration

### 3a. New module
Add `src/hal/imu_pedometer.cpp` + `src/hal/imu_pedometer.h` (code above). This
sits alongside `battery.cpp` in the HAL layer.

### 3b. setup() ordering (`the_watch_amoled.ino`)
Call `imu_pedometer_init()` **after `Touch_Init()`** (so the legacy port-0 driver
exists) and before/after `lcd_lvgl_Init()` (order vs LVGL doesn't matter; it's
just I2C). Replace the current "IMU disabled" log block:

```cpp
Touch_Init();
if (imu_pedometer_init()) Serial.println("Steps: pedometer active");
else                      Serial.println("Steps: IMU absent/failed");
lcd_lvgl_Init();
```

Add `#include "src/hal/imu_pedometer.h"` at the top (NOT `<Wire.h>`).

### 3c. watch_state model (`src/ui/watch_state.h` / `.cpp`)
Add to `struct WatchState`:
```cpp
int32_t  steps_today;      // steps since local midnight (-1 = unknown)
uint32_t steps_hw_base;    // hw counter value at start of today (baseline)
uint32_t steps_day;        // local day-number the baseline belongs to
```
Add a dirty bit (next free is bit 13):
```cpp
WS_DIRTY_STEPS = 1 << 13,
```
In `ws_init()` set `g_ws.steps_today = -1;`.

### 3d. Polling (in `ui.cpp` `ui_timer_cb`, mirroring the battery block)
The UI pump runs every 150 ms; poll the pedometer on the same slow cadence as
the battery (~every 6 s → counter=40), which is plenty (steps change slowly):
```cpp
static int step_ctr = 0;
if (step_ctr-- <= 0) {
  step_ctr = 40;                         // ~6 s
  uint32_t hw = imu_pedometer_read();
  ws_lock();
  // daily-reset + baseline math (Section 4) computes steps_today from hw
  steps_apply(hw);                       // sets g_ws.steps_today, may reset baseline
  ws_mark_dirty(WS_DIRTY_STEPS);
  ws_unlock();
}
```
Then handle the dirty bit in the same `if` chain:
```cpp
if (d & WS_DIRTY_STEPS) update_steps();
```

### 3e. UI element
Two clean options (either or both):
- **Simplest — a line on the SYSTEM/menu page** (`build_menu`, `src/ui/ui.cpp`
  ~line 576). Add an `lbl_menu_steps` label styled like `lbl_menu_batt`, e.g.
  `" 8,432 STEPS"` with `LV_SYMBOL_...` (LVGL 8 has no shoe glyph; use a small
  custom icon or just text `"STEPS  8,432"`). Update it in `update_steps()`.
- **A dedicated page / clock complication.** To add a full tile: extend the
  `enum { PAGE_CAMERA=0, … PAGE_MENU, PAGE_COUNT }` with a `PAGE_STEPS`, add a
  `build_steps(make_page(PAGE_STEPS))` call in `ui_init()` (mind the L→R column
  order documented there), and a `build_steps()` that shows a big number + an
  `lv_arc`/`lv_bar` progress ring toward a daily goal (e.g. 10,000). This
  matches the existing round-face aesthetic (see the analog `lv_meter`). Adding
  a page shifts `PAGE_COUNT`; verify the default face id (`PAGE_ANALOG`) still
  points at the analog clock after insertion.

`update_steps()`:
```cpp
static void update_steps() {
  ws_lock(); int s = g_ws.steps_today; ws_unlock();
  char buf[24];
  if (s < 0) snprintf(buf, sizeof buf, "STEPS  --");
  else       snprintf(buf, sizeof buf, "STEPS  %d", s);
  lv_label_set_text(lbl_menu_steps, buf);
}
```

### 3f. Timing / power summary
- Poll every ~6 s (hardware counts continuously; no data lost between polls).
- Gyro stays OFF; accel at 125 Hz (or drop to 31.25 Hz — CTRL2 low nibble 0x06 —
  for even lower power; still adequate for gait).
- No new task/timer needed — piggyback on the existing 150 ms `ui_timer_cb`.

---

## 4. Persistence & daily reset

The QMI8658 step counter is a free-running 24-bit total. "Steps today" =
`hw_now - baseline`, where `baseline` is the hw value captured at the start of
the current local day.

### 4a. What "today" means — time source
Wall-clock time in this firmware comes from the **phone over BLE** (`g_ws.epoch`
+ `tz_offset_min`), the same source `update_clock()` uses. Compute the local
day-number:
```cpp
uint32_t day = (uint32_t)(((int64_t)g_ws.epoch + (int64_t)g_ws.tz_offset_min*60) / 86400);
```
The on-board **PCF85063 RTC** is currently NOT read anywhere in the firmware. It
is on the same legacy I2C bus (addr 0x51) and could be read with the same
helpers as a **fallback** when the phone has never synced (`!g_ws.time_valid`) —
worth a follow-up, but for v1 rely on the phone epoch. If neither time source is
valid yet, don't roll the day over (hold the last baseline).

### 4b. Daily-reset logic (`steps_apply`)
```cpp
// called under ws_lock
void steps_apply(uint32_t hw) {
  if (g_ws.time_valid) {
    uint32_t day = (uint32_t)(((int64_t)g_ws.epoch + (int64_t)g_ws.tz_offset_min*60)/86400);
    if (day != g_ws.steps_day) {                 // new local day → rebase
      g_ws.steps_day  = day;
      g_ws.steps_hw_base = hw;                    // (hw may have wrapped; see note)
      nvs_save_steps();                           // persist baseline+day
    }
  }
  uint32_t delta = hw - g_ws.steps_hw_base;       // unsigned wrap-safe over 24-bit range
  g_ws.steps_today = (int32_t)delta;
}
```
- Using unsigned subtraction makes the 24-bit wrap self-correcting for a day's
  worth of steps (a day never approaches 16.7M). We deliberately do **NOT** call
  `imu_pedometer_reset_hw()` at midnight — leaving the hw counter free-running
  avoids a race where a step lands between read and reset. (If you prefer a hw
  reset, call `imu_pedometer_reset_hw()` then set `steps_hw_base = 0`.)

### 4c. Store across reboot — NVS via `Preferences`
`Preferences` uses NVS (flash), **not** I2C — safe (no driver_ng conflict). On a
reboot the QMI8658 hw counter also resets to 0 (power-cycled), so persist the
*derived* daily total, not just the baseline:

```cpp
#include <Preferences.h>
static Preferences s_prefs;
void nvs_init_steps()  { s_prefs.begin("steps", false);
  g_ws.steps_day = s_prefs.getUInt("day", 0);
  g_ws.steps_today = s_prefs.getInt("today", 0);
  g_ws.steps_hw_base = 0;                         // hw restarts at 0 after reboot
}
void nvs_save_steps()  { s_prefs.putUInt("day", g_ws.steps_day);
  s_prefs.putInt("today", g_ws.steps_today); }
```
Because the hw counter is 0 at boot, set `steps_hw_base = 0` on boot and ADD the
restored `steps_today` as an offset (i.e. `steps_today = restored_offset + hw`
while the day matches; simplest is: keep `steps_hw_base = -restored_offset`
conceptually — implement as `g_ws.steps_today = restored + (hw - base)`).
Save `today` on a throttle (e.g. every ~60 s or every +50 steps, and on day
rollover) to limit flash wear — NOT every 6 s poll.

**Reboot-day-match detail:** on boot, if the restored `steps_day` == today's
day-number, resume adding to the restored total; if it differs (watch was off
across midnight, or time not yet synced), start today's count fresh once time is
valid.

---

## 5. Summary of file touches (for the implementer)

| File | Change |
|------|--------|
| `src/hal/imu_pedometer.{cpp,h}` | NEW — legacy-I2C QMI8658 init + step read + hw reset |
| `the_watch_amoled.ino` | include header; call `imu_pedometer_init()` after `Touch_Init()`; remove "IMU disabled" note |
| `src/ui/watch_state.h` | add `steps_today/steps_hw_base/steps_day` + `WS_DIRTY_STEPS` (bit 13) |
| `src/ui/watch_state.cpp` | init `steps_today=-1`; NVS load in `ws_init` |
| `src/ui/ui.cpp` | poll block in `ui_timer_cb`; `WS_DIRTY_STEPS` handler; `update_steps()`; steps label on menu page (and/or new `PAGE_STEPS` tile) |
| (NVS) | `Preferences` namespace `"steps"` — no new dependency, no I2C |

**Do NOT** add `<Wire.h>` or SensorLib to the build. Verify on hardware: (1)
WHO_AM_I reads 0x05, (2) touch still works (shared bus), (3) no boot loop, (4)
step count increments while walking, (5) rolls to 0 at local midnight, (6)
survives reboot.

## Sources
- QMI8658A Datasheet, QST Corp, Doc# 13-52-25 Rev A —
  https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf
- Reference driver register/command values: `lewisxhe/SensorLib`
  `src/SensorQMI8658.hpp` (configPedometer / enablePedometer /
  getPedometerCounter / writeCommand / register + command enums).

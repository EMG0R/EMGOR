# Porting: 1.69" ST7789 dev board → 1.43" AMOLED (466×466)

The firmware was structured so this move touches **almost only the HAL** (`src/hal/*`) and `src/config.h`. The BLE layer, the shared model, and the whole UI (`src/ui/*`, `src/theme.*`) are **display-agnostic** and should compile unchanged. This document is the checklist.

## TL;DR — what actually changes

| Area | Change | File(s) |
|---|---|---|
| **Display driver** | ST7789 SPI → AMOLED controller (usually **SH8601** or **CO5300/RM690B0** on QSPI). New init + flush. | `src/hal/display.cpp` |
| **Resolution / offsets** | 240×280 → **466×466**, drop the ST7789 20-px row offset. | `src/config.h` |
| **Pins** | New QSPI data lines (D0–D3), CS, SCK, RST, TE; new I2C pins for touch/IMU. | `src/config.h` |
| **Touch** | Likely a different controller (**FT3168 / CST9217 / CHSC**) — swap driver, keep the LVGL indev shape. | `src/hal/touch.cpp` |
| **Backlight** | AMOLED has **no backlight**; brightness is a controller command. Make `display_set_backlight()` a no-op or map to a brightness register. | `src/hal/display.cpp` |
| **Draw buffer size** | Bigger frame → size the LVGL partial buffer against 466×466 (and prefer PSRAM). | `src/hal/display.cpp` |
| **Fonts / layout** | Optionally bump font roles up for the larger, round panel — **one place**. | `src/theme.cpp` |
| **Round shape** | Optional: inset content / round cards for the circular panel. | `src/ui/ui.cpp` (cosmetic) |

Nothing in `src/ble/*`, `src/ui/watch_state.*`, or the BLE/UI data flow needs to change.

---

## 1. `config.h`

- Set `WATCH_LCD_WIDTH = 466`, `WATCH_LCD_HEIGHT = 466`.
- Set `WATCH_LCD_COL_OFFSET = 0`, `WATCH_LCD_ROW_OFFSET = 0` (the 20-px offset is ST7789-panel-specific).
- Replace the SPI pin block with the AMOLED board's **QSPI** pins (typically `CS`, `SCK/CLK`, `D0`, `D1`, `D2`, `D3`, `RST`, and often a `TE`/tearing pin). Get exact GPIOs from that board's schematic / `pin_config.h`.
- Update `WATCH_I2C_SDA/SCL`, `WATCH_TP_RST/INT`, `WATCH_TP_ADDR`, `WATCH_IMU_ADDR` for the new board.

## 2. `hal/display.cpp` — the real work

Most 1.43"/466×466 AMOLED watch boards use an **SH8601** (or CO5300/RM690B0-class) controller over **QSPI**, not SPI ST7789.

- **Driver:** `Arduino_GFX` supports these via `Arduino_ESP32QSPI` + an AMOLED class (e.g. `Arduino_SH8601` / `Arduino_CO5300`). Swap:
  ```cpp
  // before (ST7789 SPI):
  Arduino_DataBus *bus = new Arduino_ESP32SPI(DC, CS, SCK, MOSI);
  Arduino_GFX     *gfx = new Arduino_ST7789(bus, RST, 0, true, W, H, colOff, rowOff, ...);
  // after (AMOLED QSPI), sketch shape:
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(CS, SCK, D0, D1, D2, D3);
  Arduino_GFX     *gfx = new Arduino_SH8601(bus, RST, 0 /*rot*/, false, 466, 466);
  ```
  Keep the **`disp_flush_cb` and LVGL registration identical** — only the `gfx` construction changes. (If you use a vendor library instead of Arduino_GFX, adapt `disp_flush_cb` to its "push a rect of RGB565" call.)
- **AMOLED has no DC pin and no backlight.** Remove `WATCH_LCD_DC`/`WATCH_LCD_BL` usage; implement brightness by sending the controller's brightness command (SH8601: `0x51`), or leave `display_set_backlight()` as a no-op if you don't need dimming.
- **Buffer:** `lv_color_t s_buf[466*466/10]` is ~43 KB — fine, but consider allocating from **PSRAM** (`ps_malloc`) and using a larger fraction for smoother scrolling on the bigger panel.
- **Color order/inversion:** AMOLEDs sometimes need `setRotation`/color-inversion tweaks; verify with a test fill. `LV_COLOR_16_SWAP` in `lv_conf.h` may need to flip.

## 3. `hal/touch.cpp`

- The 1.43" AMOLED boards commonly use **FT3168**, **CST9217**, or a CHSC part instead of CST816. Replace the `Arduino_CST816x` construction with the matching driver (Arduino_DriveBus has several; or use the vendor's).
- **Keep `touch_read_cb` and the `lv_indev` registration exactly as-is** — LVGL only needs `state` + `point.x/y`. As long as your driver yields X/Y in the 0..465 range, swipe/tap and the tileview keep working.
- Watch the coordinate range: clamp to the new `WATCH_LCD_WIDTH/HEIGHT` (already done via the config macros).

## 4. `hal/imu.cpp`

- If the AMOLED board keeps the **QMI8658**, this file is unchanged (just the I2C address/pins in `config.h`).
- If it uses a different IMU (e.g. **BHI260**/**LSM6DS**), swap the driver but keep the `imu_read()`/`imu_wrist_raised()` signatures so the UI/menu code is untouched.

## 5. `theme.cpp` / `ui.cpp` (optional, cosmetic)

- The larger, higher-DPI round panel benefits from bigger fonts: bump the `THEME_FONT_*` pointers (enable e.g. Montserrat 60/72 in `lv_conf.h`) — **one edit, whole UI scales**.
- For the **round** display, optionally add a safe inset and round the page/card corners more; the current rectangular layout still renders correctly, just with corners unused.
- `display_width()/height()` already feed the music/notification widths, so they auto-expand to 466.

## 6. Board settings

- Confirm flash size/PSRAM for the new module (many AMOLED S3 boards are 16MB flash + 8MB OPI PSRAM — same as now).
- Re-check the download-mode buttons for the new board.

---

## Sanity checklist after porting

1. Test fill (red/green/blue) fills the whole 466×466 with correct colors → display driver OK.
2. Touch a corner and read serial X/Y → touch mapping OK.
3. Confirm swipe moves between the 5 tiles → indev + tileview OK.
4. Pair Gadgetbridge → time/notifications/music/weather appear exactly as on the 1.69" board (proves the BLE + UI layers were truly display-independent).

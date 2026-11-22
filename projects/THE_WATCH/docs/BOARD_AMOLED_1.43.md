# Board: Waveshare ESP32-S3-Touch-AMOLED-1.43 (466×466 round)

The target watch board. 1.43" **round AMOLED, 466×466**, capacitive touch, CNC metal
case variant. This doc is the verified bring-up reference (pins, drivers, init).

## SoC / build
- **ESP32-S3R8**: dual-core LX7 @240MHz, WiFi+BLE5, **8MB OPI (octal) PSRAM**, 16MB flash.
- Octal PSRAM reserves **GPIO33–37** — do not use them for peripherals.
- **FQBN:** `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`
  - `USBMode=hwcdc` routes `Serial` to the USB-Serial-JTAG port (the `usbmodem1101`-style
    port), which is the one you can actually read. With the default USB mode, `Serial`
    went to a CDC port that didn't enumerate for reading.

## Verified pins (from Waveshare's own demo `07_LVGL_Test/lcd_config.h` + wiki pin table)
### Display — QSPI (controller SH8601 **or** CO5300)
| Signal | GPIO |
|---|---|
| QSPI CS   | 9  |
| QSPI CLK  | 10 |
| QSPI D0   | 11 |
| QSPI D1   | 12 |
| QSPI D2   | 13 |
| QSPI D3   | 14 |
| RST       | 21 |
| Backlight | none — AMOLED brightness is command **0x51** (`setBrightness`) |

Resolution 466×466, **RGB565** (16bpp). Panel controller is auto-detected by reading
register **0xDA**: `0x86` = SH8601 (col offset 0), `0xFF` = CO5300 (col offset 6).

### Touch — FT3168 (I2C)
- Address **0x38**, on I2C **SDA=47, SCL=48**.
- **Required init:** write `0x00` to register `0x00` ("switch to normal mode") — vendor
  `Touch_Init()` does exactly this; without it the chip won't report touches.
- Touch data: read 5 bytes from reg `0x02`: `[TD_STATUS, P1_XH, P1_XL, P1_YH, P1_YL]`.
  fingers = `TD_STATUS & 0x0F`; `x = ((XH&0x0F)<<8)|XL`; `y = ((YH&0x0F)<<8)|YL`.
- (A V2 board variant may use CST816 @0x15 instead — probe 0x38 then 0x15.)

### Shared I2C bus (SDA=47, SCL=48)
| Device | Addr |
|---|---|
| FT3168 touch     | 0x38 |
| QMI8658 IMU      | 0x6B |
| PCF85063 RTC     | 0x51 |

Battery is read via ADC / direct (this board's demo uses **no AXP2101 PMIC**).

## Power
The display needs **no PMIC / power-enable step**. Waveshare's LVGL demo `setup()` is
literally `Touch_Init(); lcd_lvgl_Init();` — the panel comes up on QSPI + RST alone.
(XPowersLib/AXP2101 was installed during research but is not required for this board.)

## Libraries (all under ~/Documents/Arduino/libraries)
- **GFX_Library_for_Arduino** — `Arduino_ESP32QSPI`, `Arduino_SH8601`, `Arduino_CO5300`.
- **Arduino_DriveBus** — `Arduino_FT3x68` (FT3168) and `Arduino_CST816x`.
- **SensorLib** — `SensorQMI8658`, `SensorPCF85063`.
- **lvgl** 8.x (color depth 16, `LV_COLOR_16_SWAP=1`).
- **XPowersLib** — AXP2101 (installed; not needed here).

## Two display-driver options
### A) Arduino_GFX (fits the existing 1.69 display.cpp structure)
```cpp
Arduino_DataBus *bus = new Arduino_ESP32QSPI(9,10,11,12,13,14); // cs,clk,d0,d1,d2,d3
// CO5300 (col offset 6):
Arduino_GFX *gfx = new Arduino_CO5300(bus, 21 /*rst*/, 0 /*rot*/, 466,466, 6,0,0,0);
// or SH8601 (col offset 0):
// Arduino_GFX *gfx = new Arduino_SH8601(bus, 21, 0, 466,466, 0,0,0,0);
gfx->begin(40000000);
gfx->setBrightness(255);                       // must call the concrete type for setBrightness
```
Colors are `RGB565_*` macros (e.g. `RGB565_RED`). LVGL flush can reuse the existing
`draw16bitBeRGBBitmap` path (matches RGB565 byte order).

### B) Vendor esp_lcd driver (ESP-IDF esp_lcd, works inside Arduino-ESP32)
Waveshare's `esp_lcd_sh8601.c` drives both SH8601 and CO5300. Their exact init tables:
```c
// SH8601 (0x86):
{0x11,{0x00},0,120},{0x44,{0x01,0xD1},2,0},{0x35,{0x00},1,0},{0x53,{0x20},1,10},
{0x51,{0x00},1,10},{0x29,{0x00},0,10},{0x51,{0xFF},1,0}
// CO5300 (0xFF):
{0x11,{0x00},0,80},{0xC4,{0x80},1,0},{0x53,{0x20},1,1},{0x63,{0xFF},1,1},
{0x51,{0x00},1,1},{0x29,{0x00},0,10},{0x51,{0xFF},1,0}
```
Flow: `spi_bus_initialize` (SH8601_PANEL_BUS_QSPI_CONFIG) → `esp_lcd_new_panel_io_spi`
(SH8601_PANEL_IO_QSPI_CONFIG, CS=9) → `esp_lcd_new_panel_sh8601` (rst=21, RGB order,
16bpp, `vendor_config.flags.use_qspi_interface=1`, init_cmds per detected ID) →
`reset` → `init` → `disp_on_off(true)` → `draw_bitmap`.

The full Waveshare demo (Arduino + ESP-IDF examples, the `esp_lcd_sh8601.c/.h`,
`read_lcd_id_bsp.c/.h`, `lcd_config.h`, and a prebuilt `Firmware/…-factory.bin`) is in
`ESP32-S3-AMOLED-1.43-Demo-V3.zip` from
`https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/`.

## Bring-up / diagnostic sketches (in `firmware/`)
- `amoled_bringup/`  — Arduino_GFX color-sweep + text + FT3168 touch dots.
- `amoled_bringup2/` — vendor `esp_lcd_sh8601` color-bands test (prints each init step's esp_err).
- `board_test/`      — I2C scanner (lists devices on 47/48).
- `touch_monitor/`   — FT3168 continuous touch reader (does the 0x00→0x00 normal-mode init).
- `display_probe/`   — reads display controller ID regs (0xDA/0xDB/0xDC/0x04) over QSPI.

## Serial / flashing gotchas
- **`arduino-cli monitor` is broken** here by a stale `electrosmith` board index (404),
  so it prints only index errors. Read serial raw instead:
  `stty -f /dev/cu.usbmodem1101 115200; cat /dev/cu.usbmodem1101`
  (the `cu.` port does NOT reset the board on open, so you see the loop, not `setup()`;
  print diagnostics in `loop()` if you need them, or reset to catch `setup()`).
- Flash port is `/dev/cu.usbmodem1101`. Confirm the chip with esptool `read-mac`
  (this board's MAC is `44:1b:f6:84:cf:38`).
- esptool: `~/Library/Arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool`.
- To flash the vendor factory image: `esptool --chip esp32s3 -p <port> write_flash 0x0 <factory.bin>`.

// config.h — board & feature configuration for THE WATCH
//
// This is the ONE place that describes the *current* hardware
// (Waveshare ESP32-S3-Touch-LCD-1.69). When you port to the 1.43" AMOLED
// board, most of what changes lives here and in src/hal/display.cpp.
// See PORTING.md.
#pragma once

// ---------------------------------------------------------------------------
// Display (Waveshare ESP32-S3-Touch-AMOLED-1.43: 466x466 round AMOLED, QSPI,
// SH8601 or CO5300 controller — ID reg 0xDA: 0x86=SH8601, 0xFF=CO5300).
//
// NOTE (the_watch_amoled build): the panel is now driven by the VENDOR esp_lcd
// path (lcd_bsp.c + lcd_config.h in the sketch root), NOT by a HAL display.cpp.
// The QSPI/reset pins + column offset therefore live in lcd_config.h and the
// SH8601/CO5300 auto-detect + 6px CO5300 offset are handled inside lcd_bsp.c.
// The WATCH_LCD_* pin macros below are SUPERSEDED and left only for reference —
// nothing in this build reads them EXCEPT the two marked "USED" below.
// ---------------------------------------------------------------------------
#define WATCH_LCD_CS     9     // (ref only — see lcd_config.h EXAMPLE_PIN_NUM_LCD_CS)
#define WATCH_LCD_SCK    10    // (ref only)
#define WATCH_LCD_D0     11    // (ref only)
#define WATCH_LCD_D1     12    // (ref only)
#define WATCH_LCD_D2     13    // (ref only)
#define WATCH_LCD_D3     14    // (ref only)
#define WATCH_LCD_RST    21    // (ref only)
// Panel power enable. USED: the .ino drives this HIGH before lcd_lvgl_Init().
// From Waveshare's 09_FactoryProgram — the vendor Arduino LVGL demo omits it,
// and without it the AMOLED is completely unpowered (verified-baseline finding).
#define WATCH_LCD_PWREN  42

// USED: display_amoled.cpp returns these from display_width()/display_height().
#define WATCH_LCD_WIDTH   466
#define WATCH_LCD_HEIGHT  466
// (ref only) — CO5300 6px column offset is applied inside lcd_bsp.c's flush_cb.
#define WATCH_LCD_COL_OFFSET 6
#define WATCH_LCD_ROW_OFFSET 0
#define WATCH_LCD_ROTATION   0

// ---------------------------------------------------------------------------
// Shared I2C bus (touch + IMU + RTC) — SDA=47 SCL=48 on this board
// ---------------------------------------------------------------------------
#define WATCH_I2C_SDA    47
#define WATCH_I2C_SCL    48

// Touch (FT3168, capacitive, addr 0x38; no RST/INT wired — polled)
#define WATCH_TP_RST     -1
#define WATCH_TP_INT     -1
#define WATCH_TP_ADDR    0x38

// IMU (QMI8658, 6-axis). QMI8658_L_SLAVE_ADDRESS == 0x6B on this board.
#define WATCH_IMU_ADDR   0x6B

// ---------------------------------------------------------------------------
// Buzzer — the 1.43 AMOLED board has NO buzzer. (On the old 1.69 board this
// was GPIO42+GPIO41, but on THIS board GPIO42 is LCD_PWREN — never tone() it.)
// ---------------------------------------------------------------------------
#define WATCH_BUZZER_PIN     -1
#define WATCH_BUZZER_EN_PIN  -1

// ---------------------------------------------------------------------------
// Power / misc
// ---------------------------------------------------------------------------
#define WATCH_BAT_ADC_PIN 4    // ADC1_CH3, VBAT/3 divider (vendor 01_ADC_Test)

// Default backlight brightness (0..255). User wants it BRIGHT. Heat is mostly
// the board's always-on USB regulator, not the backlight, so run it bright.
#define WATCH_BL_DEFAULT 220   // ~86%

// Runtime CPU frequency (MHz). Bumped to 160 for a noticeably snappier UI
// (faster LVGL redraw/scroll + touch response). Heat on this board is dominated
// by the always-on USB regulator, not the CPU, so 160 is a good responsiveness/
// thermal trade. WiFi stays OFF; BLE runs unchanged. DFS still scales the clock
// down when idle (see the .ino), so the average draw stays low.
#define WATCH_CPU_MHZ 160

// ---------------------------------------------------------------------------
// BLE identity
//
// IMPORTANT: Gadgetbridge recognises this watch as a "Bangle.js" device *by
// name*. The advertised name MUST start with "Bangle.js" for Gadgetbridge to
// offer it the Nordic-UART JSON protocol used below. Do not rename the prefix.
// ---------------------------------------------------------------------------
#define WATCH_BLE_NAME   "Bangle.js EMGOR"

// Feature toggles (compile-time)
#define WATCH_USE_MOCK_DATA_UNTIL_CONNECTED 1  // show demo data before first BLE sync

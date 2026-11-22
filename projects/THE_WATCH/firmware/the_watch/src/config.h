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
// Pins from Waveshare demo lcd_config.h + 09_FactoryProgram.
// ---------------------------------------------------------------------------
#define WATCH_LCD_CS     9
#define WATCH_LCD_SCK    10
#define WATCH_LCD_D0     11
#define WATCH_LCD_D1     12
#define WATCH_LCD_D2     13
#define WATCH_LCD_D3     14
#define WATCH_LCD_RST    21
// Panel power enable. From Waveshare's 09_FactoryProgram — their Arduino LVGL
// demo omits it, and without it the AMOLED is completely unpowered.
#define WATCH_LCD_PWREN  42

#define WATCH_LCD_WIDTH   466
#define WATCH_LCD_HEIGHT  466
// CO5300 needs a 6-pixel column offset; SH8601 uses 0. display.cpp defaults to
// CO5300 (WATCH_PANEL_SH8601=0). AMOLED brightness is controller cmd 0x51 —
// there is no backlight pin.
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

// the_watch_amoled.ino — THE WATCH firmware for the Waveshare
// ESP32-S3-Touch-AMOLED-1.43 (466x466 round AMOLED, SH8601/CO5300 QSPI,
// FT3168 touch, QMI8658 IMU).
//
// Architecture:
//   * Display / touch / LVGL bring-up  = Waveshare's proven 07_LVGL_Test esp_lcd
//     path, VERBATIM (lcd_bsp.c, esp_lcd_sh8601.c, read_lcd_id_bsp.c, FT3168.cpp,
//     lcd_config.h). This is the known-good foundation — do not alter it.
//   * Application (UI pages, BLE, watch_state, theme, imu, battery, starfield)
//     = the_watch/src/*, adapted to the 466x466 ROUND screen.
//
// The vendor lcd_lvgl_Init() creates the LVGL display + touch indev, spawns the
// LVGL pump task, and then calls app_ui_create() (see app_glue.cpp) which builds
// the watch UI. So setup() only has to: power the panel, init the shared model,
// init the vendor display/touch, then bring up IMU / battery / BLE.

#include <Arduino.h>
// NOTE: do NOT include <Wire.h> / SensorLib here. The vendor touch/display use the
// LEGACY ESP-IDF I2C driver (i2c_driver_install, port 0); merely linking Arduino
// Wire (the new "driver_ng") makes that legacy install abort at boot
// ("CONFLICT! driver_ng is not allowed..."), reboot-looping the board. The IMU is
// therefore omitted from this build until it's re-added via the legacy I2C driver.

#include "lcd_bsp.h"        // lcd_lvgl_Init() — vendor display+touch+LVGL
#include "FT3168.h"         // Touch_Init() — vendor FT3168 (ESP-IDF i2c, port 0)

#include "src/config.h"
#include "src/hal/battery.h"
#include "src/ble/ble_gadgetbridge.h"
#include "src/ui/watch_state.h"
#include "esp_task_wdt.h"

// ---------------------------------------------------------------------------
// CRASH / FREEZE AUTO-RECOVERY
//
// Two independent layers, since a "frozen but not crashed" UI won't trip a
// plain hardware fault handler:
//   1. esp_task_wdt on THIS task (loop()): a true CPU lockup (e.g. spinning
//      with interrupts disabled, a deadlock that also blocks loop()) misses
//      its feed and the IDF hardware watchdog reboots the chip.
//   2. An app-level watchdog task that polls lcd_bsp_get_heartbeat() (bumped
//      once per LVGL pump-task pass). If the LVGL task is hung — e.g. stuck
//      waiting on a lock, an infinite loop inside a widget/flush callback —
//      the heartbeat stops advancing even though loop() is still merrily
//      feeding its own watchdog. This layer catches THAT case and force-
//      reboots via esp_restart() once the heartbeat has been stale too long.
// Together: ANY freeze (CPU-locked or just the UI task stuck) self-recovers
// within a few seconds instead of staying dead until someone notices.
// ---------------------------------------------------------------------------
#define WATCHDOG_TASK_TIMEOUT_S   10   // esp_task_wdt trip time for loop()
#define UI_FREEZE_TIMEOUT_MS      8000 // LVGL heartbeat considered "stuck" after this

static void ui_watchdog_task(void *arg) {
  uint32_t last_hb = lcd_bsp_get_heartbeat();
  uint32_t last_change_ms = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    uint32_t hb = lcd_bsp_get_heartbeat();
    if (hb != last_hb) {
      last_hb = hb;
      last_change_ms = millis();
      continue;
    }
    if (millis() - last_change_ms > UI_FREEZE_TIMEOUT_MS) {
      // Print before rebooting so a serial capture (if attached) shows why.
      Serial.println("WATCHDOG: LVGL heartbeat stalled -> UI frozen. Rebooting.");
      Serial.flush();
      delay(50);
      esp_restart();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // CPU stays at the Arduino default (240MHz, uncapped) for max responsiveness.
  // Confirmed 2026-08-03: heat during testing was the CHARGING CABLE (battery
  // stayed cool, board cooled down as soon as it was unplugged from the
  // charger) — not compute load. So no CPU throttle here; keep it fast.

  // --- CRITICAL: LCD_PWREN (GPIO42) HIGH before any panel init. -------------
  // Waveshare's 09_FactoryProgram drives it; their 07_LVGL_Test demo omits it,
  // and without it the AMOLED is completely unpowered (ID reads 0xFF, no init
  // response). Verified baseline finding for THIS board.
  pinMode(WATCH_LCD_PWREN, OUTPUT);      // GPIO42
  digitalWrite(WATCH_LCD_PWREN, HIGH);
  delay(30);

  // Shared data model first: the UI (built inside lcd_lvgl_Init below) reads
  // g_ws at the end of ui_init(), and the UI pump reads the battery gauge.
  ws_init();

  // BLE (NimBLE) BEFORE the display. The 466x466 LVGL DMA buffers claim a big
  // chunk of internal RAM in lcd_lvgl_Init(); if NimBLE inits after that it can't
  // allocate its mutexes and asserts (npl_freertos_mutex_pend, mu->handle NULL) ->
  // reboot loop. Bringing BLE up first reserves its RAM, then the display gets
  // the rest. Advertises as "Bangle.js EMGOR" — phone/Mac apps connect by name.
  bool ble_ok = ble_init();
  Serial.println(ble_ok ? "BLE up, advertising." : "BLE init FAILED.");

  // Watch's own LiPo gauge (local ADC). Configure before the UI pump reads it.
  battery_init();

  // Vendor touch bring-up: installs the ESP-IDF I2C master on I2C_NUM_0
  // (SDA=47 / SCL=48) and probes the FT3168. Must precede lcd_lvgl_Init(), whose
  // LVGL indev polls getTouch().
  Touch_Init();

  // Vendor display + LVGL bring-up. Creates the SH8601/CO5300 QSPI panel, the
  // LVGL display + touch indev, the tick timer + LVGL FreeRTOS pump task, then
  // (inside the LVGL lock) calls app_ui_create() -> theme_init + ui_init +
  // ui_start_timers (see app_glue.cpp). After this returns the UI is live.
  lcd_lvgl_Init();

  // IMU (QMI8658) — DISABLED. The vendor Touch_Init() installs the LEGACY ESP-IDF
  // I2C driver on port 0 (pins 47/48); Arduino Wire/Wire1 is the NEW driver
  // ("driver_ng"), and ESP-IDF aborts (reboot loop) if both are used at once
  // ("CONFLICT! driver_ng is not allowed to be used with this old driver").
  // Confirmed on hardware 2026-07-27. To re-enable, read the QMI8658 through the
  // vendor's legacy-driver I2C helpers (i2c_master_write_read_device on port 0 /
  // the FT3168 I2C_read_buff path), NOT Arduino Wire. Battery is ADC-only, no I2C.
  Serial.println("IMU disabled (I2C driver-model conflict; see note)");

  // Crash/freeze auto-recovery (see block comment above). Start AFTER
  // lcd_lvgl_Init() so lcd_bsp_get_heartbeat() reflects a running pump task,
  // not a startup zero that would look "frozen" from tick 0.
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms = WATCHDOG_TASK_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,   // panic -> IDF's own reset handler reboots
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);    // watch the loop() task (current task)
  xTaskCreate(ui_watchdog_task, "ui_wdt", 3072, NULL, 1, NULL);

  Serial.println("THE WATCH up. (crash/freeze watchdog armed)");
}

void loop() {
  // LVGL is pumped by the vendor's FreeRTOS task (spawned in lcd_lvgl_Init), so
  // we must NOT call lv_timer_handler() here (that would race the task on the
  // non-thread-safe LVGL core). NimBLE runs its own task too, so ble_loop() is a
  // no-op today; call it anyway for future housekeeping. Idle the rest.
  ble_loop();
  esp_task_wdt_reset();   // prove this task is still alive to the HW watchdog
  delay(50);
}

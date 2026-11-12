// the_watch.ino — THE WATCH firmware entry point.
//
// Board : Waveshare ESP32-S3-Touch-LCD-1.69 (ST7789 240x280, CST816 touch,
//         QMI8658 IMU). Dev/prototype board; ports to the 1.43" AMOLED later.
// Role  : Phone-connected clock/UI. Syncs TIME, WEATHER, NOTIFICATIONS and
//         MUSIC (metadata + play/pause/next) from an Android phone running
//         Gadgetbridge (see README.md), over BLE using the Bangle.js protocol.
//
// Architecture (clean separation so the display/board can be swapped):
//   src/hal/*   — display, touch, IMU  (hardware; the ONLY board-specific code)
//   src/ble/*   — Gadgetbridge/NUS transport (no UI knowledge)
//   src/ui/*    — LVGL pages + swipe nav (no hardware/BLE knowledge)
//   src/theme.* — centralized dark-purple theme + fonts
//
// See PORTING.md for the 1.43" AMOLED move.

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>

#include "src/config.h"
#include "src/theme.h"
#include "src/hal/display.h"
#include "src/hal/touch.h"
#include "src/hal/imu.h"
#include "src/hal/battery.h"
#include "src/ble/ble_gadgetbridge.h"
#include "src/ui/ui.h"
#include "src/ui/watch_state.h"

void setup() {
  Serial.begin(115200);

  // --- Thermal / power: this board runs hot. We are BLE-only, no WiFi. -------
  // 1) Kill the WiFi radio entirely (never inited, but make it explicit so the
  //    PHY/RF is powered down instead of idling).
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();      // power down the RF/PHY (harmless if never started)
  // 2) Set the CPU clock to WATCH_CPU_MHZ (160) for a snappy UI. Heat here is
  //    dominated by the board's always-on USB regulator, not the CPU, and DFS
  //    below scales the clock back down when idle to keep the average low.
  setCpuFrequencyMhz(WATCH_CPU_MHZ);
  // CONFIRM it actually took (log the value read back from the hardware).
  Serial.printf("CPU freq = %d MHz\n", (int)getCpuFrequencyMhz());
  // 3) Enable automatic light sleep + DFS so the cores idle between LVGL/BLE
  //    work instead of spinning. min 10 MHz; if the core build rejects that,
  //    retry with 40 (XTAL/1). Log the result so it's verifiable.
  esp_pm_config_t pm = {
      .max_freq_mhz = WATCH_CPU_MHZ,
      .min_freq_mhz = 10,
      .light_sleep_enable = true,
  };
  esp_err_t pmres = esp_pm_configure(&pm);
  if (pmres != ESP_OK) {
    pm.min_freq_mhz = 40;
    pmres = esp_pm_configure(&pm);
  }
  bool light_sleep_ok = (pmres == ESP_OK);
  if (!light_sleep_ok) {
    // The prebuilt Arduino core lacks CONFIG_FREERTOS_USE_TICKLESS_IDLE, so
    // automatic light sleep is ESP_ERR_NOT_SUPPORTED (262). Fall back to pure
    // DFS: scale 80 -> 10 MHz when idle, no light sleep. Still a real heat cut.
    pm.light_sleep_enable = false;
    pm.min_freq_mhz = 10;
    pmres = esp_pm_configure(&pm);
    if (pmres != ESP_OK) { pm.min_freq_mhz = 40; pmres = esp_pm_configure(&pm); }
  }
  Serial.printf("PWR: WiFi off, light-sleep %s, DFS %s (min %d MHz, err=%d)\n",
                light_sleep_ok ? "ON" : "unsupported by core",
                pmres == ESP_OK ? "ON" : "OFF", pm.min_freq_mhz, (int)pmres);

  // Shared model first (BLE + UI both use it).
  ws_init();

  // Shared I2C bus for touch + IMU.
  Wire.begin(WATCH_I2C_SDA, WATCH_I2C_SCL);
  Wire.setClock(400000);

  // HAL bring-up.
  lv_disp_t *disp = display_init();   // does lv_init() internally
  touch_init(Wire);
  imu_init(Wire);                     // non-fatal if absent
  battery_init();                     // watch's own LiPo gauge (GPIO1 ADC)

  // Theme + UI.
  theme_init(disp);
  ui_init();
  ui_start_timers();

  // BLE last, once the UI can render incoming data.
  bool ble_ok = ble_init();

  if (ble_ok) {
    Serial.println("THE WATCH ready. Advertising.");
  } else {
    Serial.println("THE WATCH ready, but BLE advertising did NOT start.");
  }
}

void loop() {
  uint32_t next = lv_timer_handler();   // ms until LVGL next needs servicing
  ble_loop();                           // BLE housekeeping (NimBLE own task)
  // Idle the core between frames instead of busy-spinning. Cap the nap so touch
  // still feels responsive; delay() lets the FreeRTOS idle task run (which is
  // where automatic light sleep kicks in) — a big heat reduction vs delay(5).
  // (next is UINT32_MAX when no timer is pending, so the upper clamp covers it.)
  // Longer naps = more time in light sleep = cooler. 40 ms still tracks touch
  // acceptably; the 10 ms floor guarantees the idle task actually gets slack.
  // Cap the nap SHORT so touch is polled often and swiping stays reactive
  // (was 40 ms, which let fast swipes feel laggy). 16 ms ≈ 60 Hz servicing.
  if (next > 16) next = 16;
  if (next < 5)  next = 5;
  delay(next);
}

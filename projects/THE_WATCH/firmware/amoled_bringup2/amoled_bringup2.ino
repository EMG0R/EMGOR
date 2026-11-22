// amoled_bringup2.ino — bring-up using Waveshare's OWN esp_lcd_sh8601 driver
// (their exact init sequence for the ESP32-S3-Touch-AMOLED-1.43). No LVGL, no
// Arduino_GFX. If THIS lights the panel, the board is good and esp_lcd is the
// path; if it's still black, the problem is hardware/flash, not the driver.
//
// Errors are printed (not ESP_ERROR_CHECK aborts) so a failure shows on Serial
// instead of silently resetting the chip.

#include <Arduino.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
extern "C" {
#include "esp_lcd_sh8601.h"
#include "read_lcd_id_bsp.h"
}
#include "lcd_config.h"

#define LCD_HOST   SPI2_HOST
#define SH8601_ID  0x86
#define CO5300_ID  0xff

// Vendor init tables (verbatim from Waveshare 07_LVGL_Test/lcd_bsp.c).
static const sh8601_lcd_init_cmd_t sh8601_init[] = {
  {0x11, (uint8_t []){0x00}, 0, 120},
  {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
  {0x35, (uint8_t []){0x00}, 1, 0},
  {0x53, (uint8_t []){0x20}, 1, 10},
  {0x51, (uint8_t []){0x00}, 1, 10},
  {0x29, (uint8_t []){0x00}, 0, 10},
  {0x51, (uint8_t []){0xFF}, 1, 0},
};
static const sh8601_lcd_init_cmd_t co5300_init[] = {
  {0x11, (uint8_t []){0x00}, 0, 80},
  {0xC4, (uint8_t []){0x80}, 1, 0},
  {0x53, (uint8_t []){0x20}, 1, 1},
  {0x63, (uint8_t []){0xFF}, 1, 1},
  {0x51, (uint8_t []){0x00}, 1, 1},
  {0x29, (uint8_t []){0x00}, 0, 10},
  {0x51, (uint8_t []){0xFF}, 1, 0},
};

static esp_lcd_panel_handle_t panel = NULL;
static String g_status;   // init results, reprinted in loop() so cat always sees them

static bool ck(const char *what, esp_err_t e) {
  char line[96];
  snprintf(line, sizeof(line), "%s -> %s", what, esp_err_to_name(e));
  Serial.printf("[bringup2] %s\n", line);
  g_status += String(line) + " | ";
  return e == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[bringup2] esp_lcd SH8601/CO5300 vendor-driver test");

  uint8_t id = read_lcd_id();
  Serial.printf("[bringup2] read_lcd_id = 0x%02x  (SH8601=0x86, CO5300=0xff)\n", id);
  g_status = "id=0x" + String(id, HEX) + " | ";

  spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
      EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0, EXAMPLE_PIN_NUM_LCD_DATA1,
      EXAMPLE_PIN_NUM_LCD_DATA2, EXAMPLE_PIN_NUM_LCD_DATA3,
      EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
  ck("spi_bus_initialize", spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io = NULL;
  esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
      EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
  ck("new_panel_io_spi", esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

  sh8601_vendor_config_t vcfg = {};
  vcfg.flags.use_qspi_interface = 1;
  if (id == SH8601_ID) {
    vcfg.init_cmds = sh8601_init; vcfg.init_cmds_size = sizeof(sh8601_init)/sizeof(sh8601_init[0]);
    Serial.println("[bringup2] using SH8601 init");
  } else {
    vcfg.init_cmds = co5300_init; vcfg.init_cmds_size = sizeof(co5300_init)/sizeof(co5300_init[0]);
    Serial.println("[bringup2] using CO5300 init");
  }

  esp_lcd_panel_dev_config_t pcfg = {};
  pcfg.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
  pcfg.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
  pcfg.bits_per_pixel = LCD_BIT_PER_PIXEL;
  pcfg.vendor_config  = &vcfg;
  ck("new_panel_sh8601", esp_lcd_new_panel_sh8601(io, &pcfg, &panel));
  ck("panel_reset",  esp_lcd_panel_reset(panel));
  ck("panel_init",   esp_lcd_panel_init(panel));
  ck("panel_disp_on", esp_lcd_panel_disp_on_off(panel, true));

  // Fill the screen in horizontal color bands.
  const int W = EXAMPLE_LCD_H_RES, H = EXAMPLE_LCD_V_RES, band = 40;
  uint16_t *buf = (uint16_t *)heap_caps_malloc(W * band * 2, MALLOC_CAP_DMA);
  if (!buf) { Serial.println("[bringup2] buf alloc FAILED"); return; }
  const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFE0};  // R G B W Y
  for (int y = 0; y < H; y += band) {
    uint16_t c = colors[(y / band) % 5];
    int h = (band < H - y) ? band : (H - y);
    for (int i = 0; i < W * h; i++) buf[i] = c;
    esp_lcd_panel_draw_bitmap(panel, 0, y, W, y + h, buf);
  }
  Serial.println("[bringup2] drew color bands — screen should show R/G/B/W/Y stripes");
}

void loop() {
  static uint32_t n = 0;
  Serial.printf("[bringup2] hb %lu | INIT: %s\n", (unsigned long)n++, g_status.c_str());
  delay(700);
}

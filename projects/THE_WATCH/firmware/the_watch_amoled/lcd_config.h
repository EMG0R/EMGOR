#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#define EXAMPLE_LCD_H_RES              466
#define EXAMPLE_LCD_V_RES              466

#define LCD_BIT_PER_PIXEL              16

#define EXAMPLE_PIN_NUM_LCD_CS            9
#define EXAMPLE_PIN_NUM_LCD_PCLK          10 
#define EXAMPLE_PIN_NUM_LCD_DATA0         11
#define EXAMPLE_PIN_NUM_LCD_DATA1         12
#define EXAMPLE_PIN_NUM_LCD_DATA2         13
#define EXAMPLE_PIN_NUM_LCD_DATA3         14
#define EXAMPLE_PIN_NUM_LCD_RST           21
#define EXAMPLE_PIN_NUM_BK_LIGHT          (-1)

// FULL-FRAME LVGL draw buffers. Two of these (double buffered) live in PSRAM.
// Sizing each buffer to the WHOLE 466x466 frame means any large invalidation --
// a page swipe, a tile settle, the whole analog dial -- renders + flushes in a
// SINGLE pass instead of 4 vertical strips. That is what eliminates the
// strip/tearing/stale-pixel artifacts seen while swiping between pages.
//
// NOTE: we do NOT use lv_disp_drv_t.full_refresh. This board must rotate 270 deg
// in software (disp_drv.sw_rotate=1) because the SH8601/CO5300 panel does not
// support hardware swap_xy, and LVGL 8.3 explicitly refuses to rotate a
// full_refresh display ("cannot rotate a full refreshed display!", lv_refr.c).
// Full-frame-SIZED partial buffers give the same single-pass redraw benefit while
// staying compatible with the mandatory software rotation.
//
// Cost: 466*466*2 bytes = ~434 KB per buffer, ~868 KB total in the 8 MB PSRAM.
// Buffers are INTERNAL DMA RAM (see lcd_bsp.c) for reliable flush completion —
// PSRAM buffers stalled the flush under BLE load and froze the UI task.
//
// DOUBLE buffer at V_RES/16 -- this is the EXACT same total internal RAM as a
// single V_RES/8 buffer (466*29*2 * 2 buffers == 466*58*2 * 1 buffer), so it
// is just as safe against NimBLE's larger post-CONNECT RAM footprint. BUT
// double-buffering lets the CPU render the next chunk while DMA flushes the
// current one (pipelined) -- single-buffering forces the LVGL task to block
// fully between chunks, which is what made the clock-hand glitch WORSE and
// touch feel laggy (the same task that reads touch is the one stalled on
// flush). Same RAM, restored pipelining. Combined with the QSPI clock bump to
// 80MHz (esp_lcd_sh8601.h) each chunk transfers 2x faster too.
// (full-frame + sw_rotate = black; anything in PSRAM = freezes.)
#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 16)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2                          //Timer time
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500                        //LVGL Indicates the maximum time for a task to run
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1                          //LVGL Minimum time to run a task
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)                 //LVGL runs the task stack
#define EXAMPLE_LVGL_TASK_PRIORITY     2                          //LVGL Running task priority

#define I2C_ADDR_FT3168 0x38
#define EXAMPLE_PIN_NUM_TOUCH_SCL 48
#define EXAMPLE_PIN_NUM_TOUCH_SDA 47

#endif
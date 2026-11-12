// ui/lava.cpp — cheap metaball "lava lamp" background. See lava.h for the why.
//
// DISABLED: the lava-lamp layer was removed because its full-screen canvas
// invalidated the entire 240x280 frame every tick (a full SPI flush), which made
// the UI laggy. ui.cpp no longer includes lava.h or calls lava_attach()/
// lava_start(), so this module never runs or draws. The whole implementation is
// compiled out (#if 0) to also reclaim the ~5 KB static canvas buffer. Kept in
// the tree for reference only; re-enable by removing the guard AND restoring the
// calls in ui.cpp (not recommended — it is the lag source).
#if 0

#include "lava.h"
#include "../theme.h"
#include "../hal/display.h"

#include <esp_timer.h>
#include <math.h>

// --- Low-res goo grid -------------------------------------------------------
// 48x56 keeps the same 240x280 (=6:7) aspect ratio, so the upscale is a clean
// integer 5x with no horizontal/vertical stretch. 48*56 = 2688 px -> 5376 bytes
// in fast internal RAM. Recomputing all of it per frame is trivial for the FPU.
#define LAVA_W    48
#define LAVA_H    56
#define LAVA_ZOOM (256 * (display_width() / LAVA_W))   // 5x -> 1280 (256 == 1.0x)
#define LAVA_TICK_MS 200        // ~5 fps — lava is slow; low fps caps SPI/heat.
#define LAVA_NBLOBS  4

// Canvas pixel buffer (TRUE_COLOR = lv_color_t == 16bpp here).
static lv_color_t s_lava_buf[LAVA_W * LAVA_H];
static lv_obj_t  *s_canvas = nullptr;

// Deep-space base wash the goo blends UP from (must stay very dark so text on
// top is readable). Matches THEME_COL_BG (0x0A0820).
static const uint8_t BASE_R = 0x0A, BASE_G = 0x08, BASE_B = 0x20;

// Each blob: a slow Lissajous orbit around a base point + a dim deep color.
// Colors are intentionally LOW-luminance violets/indigos/blues — big soft dark
// goo, never bright, so white/lavender UI text always wins the contrast.
typedef struct {
  float cx0, cy0;     // orbit center (grid units)
  float ax, ay;       // orbit amplitude (grid units)
  float wx, wy;       // orbit angular speed
  float px, py;       // orbit phase
  float r2;           // blob radius^2 (field strength / size)
  uint8_t cr, cg, cb; // blob color
} lava_blob_t;

static lava_blob_t s_blob[LAVA_NBLOBS] = {
  // spread the orbits across the grid so blobs roam the whole screen
  { 12, 16,  8, 11, 0.021f, 0.017f, 0.0f, 1.7f, 150.0f, 0x34, 0x18, 0x62 }, // violet
  { 34, 24, 10,  9, 0.015f, 0.023f, 2.1f, 0.6f, 185.0f, 0x1C, 0x16, 0x60 }, // indigo
  { 20, 40,  9, 12, 0.019f, 0.013f, 4.0f, 3.3f, 165.0f, 0x14, 0x1E, 0x5A }, // blue
  { 36, 46, 11,  8, 0.012f, 0.020f, 5.2f, 2.4f, 200.0f, 0x2A, 0x12, 0x58 }, // magenta-violet
};

static float s_t = 0.0f;   // animation clock (advances each tick)

// Recompute the whole low-res goo grid for the current time and push it to the
// canvas. Metaball field: each pixel sums 1/dist^2 contributions from every
// blob; overlapping blobs merge into one smooth mass (the goo look). We also
// blend each blob's color weighted by its contribution, so colors mix where
// blobs meet — no sqrt, no divides-in-inner-loop beyond one reciprocal.
static void lava_render() {
  // Current blob centers.
  float bx[LAVA_NBLOBS], by[LAVA_NBLOBS];
  for (int i = 0; i < LAVA_NBLOBS; i++) {
    bx[i] = s_blob[i].cx0 + s_blob[i].ax * sinf(s_t * s_blob[i].wx + s_blob[i].px);
    by[i] = s_blob[i].cy0 + s_blob[i].ay * sinf(s_t * s_blob[i].wy + s_blob[i].py);
  }

  lv_color_t *p = s_lava_buf;
  for (int y = 0; y < LAVA_H; y++) {
    for (int x = 0; x < LAVA_W; x++) {
      float total = 0.0f, ar = 0.0f, ag = 0.0f, ab = 0.0f;
      for (int i = 0; i < LAVA_NBLOBS; i++) {
        float dx = (float)x - bx[i];
        float dy = (float)y - by[i];
        // +4 softens the singularity at the exact center (no hard hotspot).
        float w = s_blob[i].r2 / (dx * dx + dy * dy + 4.0f);
        total += w;
        ar += w * s_blob[i].cr;
        ag += w * s_blob[i].cg;
        ab += w * s_blob[i].cb;
      }
      // a in 0..1: how "inside the goo" this pixel is (soft edge via total/(total+k)).
      float a = total / (total + 8.0f);
      float inv = (total > 0.0001f) ? (1.0f / total) : 0.0f;
      uint8_t br = (uint8_t)(BASE_R + a * (ar * inv - BASE_R));
      uint8_t bg = (uint8_t)(BASE_G + a * (ag * inv - BASE_G));
      uint8_t bb = (uint8_t)(BASE_B + a * (ab * inv - BASE_B));
      *p++ = lv_color_make(br, bg, bb);
    }
  }
  if (s_canvas) lv_obj_invalidate(s_canvas);
}

void lava_attach(lv_obj_t *parent) {
  s_canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(s_canvas, s_lava_buf, LAVA_W, LAVA_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);   // never eat swipes/taps
  lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_canvas, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_IGNORE_LAYOUT);
  // Upscale the tiny buffer to fill the panel. Pivot at the top-left corner so
  // the zoom grows toward the bottom-right and the image origin stays at (0,0).
  lv_obj_set_pos(s_canvas, 0, 0);
  lv_img_set_pivot(s_canvas, 0, 0);
  lv_img_set_antialias(s_canvas, true);                 // bilinear blur = soft blobs
  lv_img_set_zoom(s_canvas, (uint16_t)LAVA_ZOOM);
  lv_obj_move_background(s_canvas);                      // lowest layer in parent

  lava_render();                                        // paint an initial frame
}

static void lava_timer_cb(lv_timer_t *t) {
  LV_UNUSED(t);
  s_t += 1.0f;                    // one step per frame; blob speeds are tiny
  if (s_t > 100000.0f) s_t = 0.0f;
  lava_render();
}

void lava_start() {
  lv_timer_create(lava_timer_cb, LAVA_TICK_MS, NULL);
}

#endif  // 0 — lava-lamp background disabled (lag source, replaced by starfield)

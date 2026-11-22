// ui/starfield.cpp — animated "space" starfield with a flowing wave.
//
// LOOK: a field of small stars over the dark blue-violet background. Each star
// TWINKLES (opacity oscillates) and the whole field UNDULATES along a coherent
// traveling sine wave (a horizontal ripple that depends on the star's y, plus a
// gentler vertical ripple that depends on its x) so a visible wave flows across
// the screen — the signature "waving" motion from emgor.online. A very slow
// drift keeps the field alive so it never looks frozen.
//
// PERFORMANCE (this is the whole point of using objects, not a canvas):
//   * Stars are tiny LVGL objects, so LVGL only invalidates/flushes the few
//     small rectangles that actually change — never the whole 240x280 frame.
//     A full-frame redraw over 40 MHz SPI would be ~27 ms/frame and cook the
//     chip; here each frame touches only a few hundred pixels.
//   * We update a small ROUND-ROBIN SUBSET of stars per tick (SF_PER_TICK) so
//     the number of dirty regions per refresh stays well under LVGL's
//     invalidation-buffer limit (32) — that limit is what would otherwise
//     silently promote us to a full-screen redraw. The global wave phase still
//     advances every tick, so the ripple keeps flowing smoothly regardless.
//   * Only the on-screen face's field animates (lv_obj_is_visible gate), so the
//     off-screen clock face costs nothing.
//
// This adds negligible SPI traffic and no new tearing surface; it does not touch
// display.cpp addressing / clock / buffers.

#include "starfield.h"
#include "../theme.h"
#include "../hal/display.h"

#include <esp_timer.h>
#include <stdlib.h>
#include <math.h>

// --- Tunables ---------------------------------------------------------------
#define SF_MAX_FIELDS 2       // analog + digital clock faces
#define SF_STARS      42      // stars per field (visible count on the active face)
#define SF_PER_TICK   10      // stars refreshed per tick -> ~<=20 dirty rects/frame
#define SF_TICK_MS    45      // ~22 Hz animation timer

// Traveling-wave geometry. Amplitudes are deliberately PROMINENT so the
// undulation clearly reads, while staying small enough that the clock/text on
// top remain perfectly legible.
#define SF_WAVE_AX     9.0f   // horizontal ripple amplitude (px)
#define SF_WAVE_AY     4.0f   // vertical ripple amplitude (px)
#define SF_WAVE_KY     0.035f // horizontal ripple wavelength ~ 2*pi/KY down the screen
#define SF_WAVE_KX     0.045f // vertical ripple wavelength across the screen
#define SF_WAVE_DPH    0.115f // wave travel speed (rad/tick) — the flow

static const uint32_t SF_PALETTE[] = {
  0xFFFFFF, 0xEAE2FF, 0xCFC2F0, 0xB9A6E8, 0xA9BEFF   // whites + light violets/blues
};
#define SF_PAL_N (int)(sizeof(SF_PALETTE) / sizeof(SF_PALETTE[0]))

typedef struct {
  lv_obj_t *dot;
  float   bx, by;     // slowly-drifting base position (layer-local px)
  float   vx, vy;     // slow drift velocity (px per own update)
  float   amp;        // per-star wave amplitude scale (organic variation)
  float   ph;         // per-star phase offset
  float   tw, dtw;    // twinkle phase + advance
  uint8_t op_lo, op_hi, last_op;
  int16_t ix, iy;     // last applied integer position
} sf_star_t;

typedef struct {
  lv_obj_t *cont;
  int       w, h;
  int       cursor;
  sf_star_t star[SF_STARS];
} sf_field_t;

static sf_field_t s_fields[SF_MAX_FIELDS];
static int   s_field_n = 0;
static bool  s_seeded  = false;
static float s_wave    = 0.0f;     // global wave phase (advances every tick)

static inline float frand(float a, float b) {
  return a + (b - a) * ((float)rand() / (float)RAND_MAX);
}

// Recompute a star's rendered position (drift base + traveling wave) and its
// twinkle opacity, and push only what actually changed to LVGL.
static void sf_apply(sf_star_t *s) {
  float wx = SF_WAVE_AX * s->amp * sinf(SF_WAVE_KY * s->by + s_wave + s->ph);
  float wy = SF_WAVE_AY * s->amp * sinf(SF_WAVE_KX * s->bx + s_wave * 0.8f);
  int16_t ix = (int16_t)lroundf(s->bx + wx);
  int16_t iy = (int16_t)lroundf(s->by + wy);
  if (ix != s->ix || iy != s->iy) {
    lv_obj_set_pos(s->dot, ix, iy);
    s->ix = ix; s->iy = iy;
  }
  float t = 0.5f * (1.0f + sinf(s->tw));                 // 0..1
  uint8_t op = (uint8_t)(s->op_lo + t * (float)(s->op_hi - s->op_lo));
  if (op != s->last_op) {
    lv_obj_set_style_bg_opa(s->dot, op, 0);
    s->last_op = op;
  }
}

static void sf_init_star(sf_field_t *f, sf_star_t *s) {
  // Mostly 2 px dots with a few 3–4 px "brighter" stars for depth.
  int r = rand() % 100;
  int sz = (r < 68) ? 2 : (r < 90 ? 3 : 4);

  s->dot = lv_obj_create(f->cont);
  lv_obj_remove_style_all(s->dot);
  lv_obj_clear_flag(s->dot, LV_OBJ_FLAG_CLICKABLE);   // never intercept swipes
  lv_obj_clear_flag(s->dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s->dot, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(s->dot, sz, sz);
  lv_obj_set_style_radius(s->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(s->dot, lv_color_hex(SF_PALETTE[rand() % SF_PAL_N]), 0);
  lv_obj_set_style_bg_opa(s->dot, LV_OPA_COVER, 0);

  s->bx  = frand(0, f->w);
  s->by  = frand(0, f->h);
  s->vx  = frand(-0.12f, 0.12f);   // very slow lateral drift
  s->vy  = frand(0.10f, 0.45f);    // gentle downward drift
  s->amp = frand(0.7f, 1.3f);
  s->ph  = frand(0.0f, 1.2f);
  s->tw  = frand(0.0f, 6.283f);
  s->dtw = frand(0.10f, 0.30f);
  s->op_lo   = (uint8_t)frand(45, 110);
  s->op_hi   = (uint8_t)frand(185, 255);
  s->last_op = 0;
  s->ix = s->iy = -9999;
  sf_apply(s);
}

void starfield_attach(lv_obj_t *page) {
  if (s_field_n >= SF_MAX_FIELDS) return;
  if (!s_seeded) { srand((unsigned)esp_timer_get_time()); s_seeded = true; }

  sf_field_t *f = &s_fields[s_field_n];
  // Cover the WHOLE tile even if the page has padding (digital face has 14 px):
  // position the layer at the tile's true top-left by negating the pad.
  int pl = lv_obj_get_style_pad_left(page, 0);
  int pt = lv_obj_get_style_pad_top(page, 0);
  f->w = display_width();
  f->h = display_height();

  f->cont = lv_obj_create(page);
  lv_obj_remove_style_all(f->cont);
  lv_obj_clear_flag(f->cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(f->cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(f->cont, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_scrollbar_mode(f->cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(f->cont, f->w, f->h);
  lv_obj_set_pos(f->cont, -pl, -pt);
  lv_obj_set_style_bg_opa(f->cont, LV_OPA_TRANSP, 0);  // show the lava bg through
  // NOTE: no move_background here. The field now lives on the shared full-screen
  // background container (see ui.cpp), attached AFTER the lava canvas, so normal
  // creation order already places the stars ABOVE the lava goo but still behind
  // every (transparent) page. Moving it to the back would hide it under the lava.

  f->cursor = 0;
  for (int i = 0; i < SF_STARS; i++) sf_init_star(f, &f->star[i]);
  s_field_n++;
}

static void sf_timer_cb(lv_timer_t *t) {
  LV_UNUSED(t);
  s_wave += SF_WAVE_DPH;                 // advance the flow every tick (smooth)
  if (s_wave > 6.2831853f) s_wave -= 6.2831853f;

  for (int fi = 0; fi < s_field_n; fi++) {
    sf_field_t *f = &s_fields[fi];
    if (!f->cont || !lv_obj_is_visible(f->cont)) continue;  // only the active face

    for (int k = 0; k < SF_PER_TICK; k++) {
      sf_star_t *s = &f->star[f->cursor];
      f->cursor = (f->cursor + 1) % SF_STARS;

      s->bx += s->vx;
      s->by += s->vy;
      s->tw += s->dtw;
      if (s->tw > 6.2831853f) s->tw -= 6.2831853f;

      // Wrap around the edges (leave margin for the wave amplitude).
      if (s->by > f->h + 12) { s->by = -12; s->bx = frand(0, f->w); }
      else if (s->by < -12)  { s->by = f->h + 12; }
      if (s->bx > f->w + 12) s->bx = -12;
      else if (s->bx < -12)  s->bx = f->w + 12;

      sf_apply(s);
    }
  }
}

void starfield_start() {
  lv_timer_create(sf_timer_cb, SF_TICK_MS, NULL);
}

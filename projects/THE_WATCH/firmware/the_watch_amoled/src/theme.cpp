#include "theme.h"
#include "hal/display.h"   // display_width()/display_height() for full-tile sizing

// Map the abstract font roles to concrete LVGL fonts. Bigger across the board
// for the redesign; 48 is the largest built-in Montserrat. Swap these (or a
// custom font) when porting to the 466x466 AMOLED — nothing else changes.
const lv_font_t *THEME_FONT_HUGE   = &lv_font_montserrat_48;
const lv_font_t *THEME_FONT_XL     = &lv_font_montserrat_40;
const lv_font_t *THEME_FONT_LARGE  = &lv_font_montserrat_34;
const lv_font_t *THEME_FONT_MEDIUM = &lv_font_montserrat_24;
const lv_font_t *THEME_FONT_SMALL  = &lv_font_montserrat_18;

static lv_theme_t s_theme;

// Called by LVGL for every object created on the display. Enforce the global
// near-black-purple background + default text color/font; widgets refine on
// top. Keeps the dark-purple identity consistent everywhere.
static void theme_apply_cb(lv_theme_t *th, lv_obj_t *obj) {
  LV_UNUSED(th);
  if (lv_obj_get_parent(obj) == NULL) {  // screen
    lv_obj_set_style_bg_color(obj, THEME_COL_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(obj, THEME_COL_TEXT, 0);
    lv_obj_set_style_text_font(obj, THEME_FONT_MEDIUM, 0);
  }
}

void theme_init(lv_disp_t *disp) {
  lv_theme_t *base = lv_disp_get_theme(disp);
  lv_theme_set_parent(&s_theme, base);
  lv_theme_set_apply_cb(&s_theme, theme_apply_cb);
  lv_disp_set_theme(disp, &s_theme);
}

void theme_style_page(lv_obj_t *page) {
  // TRANSPARENT tiles: the animated lava+starfield lives on ONE shared
  // full-screen background BEHIND the tileview (see ui.cpp), so every page must
  // let it show through. The bg color is still set (harmless) but opacity is
  // TRANSP. Anti-white-edge safety: the shared background fully covers 240x280
  // in opaque dark, so any sub-pixel swipe gap shows the dark animated bg, never
  // white.
  lv_obj_set_style_bg_color(page, THEME_COL_BG, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_outline_width(page, 0, 0);   // kill the default 1px focus/edge outline
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_radius(page, 0, 0);
  // Fill the whole 240x280 tile so layout is unchanged from the opaque version.
  lv_obj_set_size(page, display_width(), display_height());
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *theme_make_bar(lv_obj_t *parent, int w, int h, lv_color_t color) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, w, h);
  lv_obj_set_style_bg_color(bar, color, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  return bar;
}

lv_obj_t *theme_make_card(lv_obj_t *parent) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_style_bg_color(card, THEME_COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  // Sharp, tool-like: near-square corners, no decorative accent rail/border.
  lv_obj_set_style_radius(card, 3, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 12, 0);
  lv_obj_set_style_pad_left(card, 14, 0);
  lv_obj_set_style_text_color(card, THEME_COL_TEXT, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

void theme_add_header(lv_obj_t *page, const char *title) {
  // Page titles removed per design — cleaner, title-less tops. Kept as a no-op
  // so every build_*() call site stays valid without edits. (LV_UNUSED avoids
  // unused-parameter warnings.)
  LV_UNUSED(page);
  LV_UNUSED(title);
}

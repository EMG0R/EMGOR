// theme.h — CENTRALIZED dark-purple theme for THE WATCH.
//
// Everything visual (colors + fonts) is defined here so the whole UI stays
// consistent and is trivial to retune (or scale up for the 466x466 AMOLED).
// Change a value here -> every page updates.
#pragma once

#include <lvgl.h>

// --- Simple two-tone palette: deep purple + white ---------------------------
// A rich DEEP PURPLE background (not near-black) with WHITE text/foreground.
// Deliberately minimal: rails, underlines and accents are white too; the only
// non-white colors are the two functional status colors (ok/warn), kept soft.
// Deep space backdrop: a very dark, BLUE-leaning violet (~2x darker than the
// old vivid violet) so the animated starfield reads like a night sky.
#define THEME_COL_BG          lv_color_hex(0x0A0820)  // very dark blue-violet base
#define THEME_COL_BG_ALT      lv_color_hex(0x100C2C)  // subtle panel wash
#define THEME_COL_SURFACE     lv_color_hex(0x1A1440)  // cards / panels
#define THEME_COL_SURFACE_HI  lv_color_hex(0x271C5C)  // pressed / raised
#define THEME_COL_ACCENT      lv_color_hex(0xFFFFFF)  // white — rails/underlines
#define THEME_COL_ACCENT_DIM  lv_color_hex(0xB9A6E8)  // soft lavender (secondary)
#define THEME_COL_GLOW        lv_color_hex(0xFFFFFF)  // white hairlines
#define THEME_COL_TEXT        lv_color_hex(0xFFFFFF)  // primary text: white
#define THEME_COL_TEXT_MUTED  lv_color_hex(0xCFC2F0)  // secondary text: pale lavender
#define THEME_COL_OK          lv_color_hex(0x8FE8BC)  // connected / good (soft)
#define THEME_COL_WARN        lv_color_hex(0xFF9AAE)  // disconnected / alert (soft)

// --- Analog clock face: WHITE (clean, high-contrast on the dark violet bg) ---
// Reverted from the bright-purple experiment back to white per user request.
// Keeps the redesign otherwise: no ticks, 12 bold hour numbers, thick hands.
#define THEME_COL_CLOCK_NUM   lv_color_hex(0xFFFFFF)  // hour numbers — white
#define THEME_COL_CLOCK_HAND  lv_color_hex(0xFFFFFF)  // hour/min hands — white
#define THEME_COL_CLOCK_SEC   lv_color_hex(0xCFCFCF)  // second hand — light gray (NOT red)

// --- Font scale -----------------------------------------------------------
// Pointers so the whole UI can be re-scaled for a bigger panel in one place.
// These fonts must be enabled in lv_conf.h (see firmware/the_watch/lv_conf.h).
extern const lv_font_t *THEME_FONT_HUGE;    // clock digits — oversized (48)
extern const lv_font_t *THEME_FONT_XL;      // headline temp (40)
extern const lv_font_t *THEME_FONT_LARGE;   // track title / weather text (34)
extern const lv_font_t *THEME_FONT_MEDIUM;  // labels (24)
extern const lv_font_t *THEME_FONT_SMALL;   // captions / status (18)

// Apply the theme to a display (call once after lv_init + display init).
void theme_init(lv_disp_t *disp);

// Helper: style a full-screen page container with the background color.
void theme_style_page(lv_obj_t *page);

// Helper: make a sharp "card" panel (surface bg + left accent rail).
lv_obj_t *theme_make_card(lv_obj_t *parent);

// Helper: a solid accent rectangle — used for header rails, underlines and
// deliberate cyber-style accent lines. Returns the bar object.
lv_obj_t *theme_make_bar(lv_obj_t *parent, int w, int h, lv_color_t color);

// Helper: standard page header — uppercase muted title with an accent rail on
// the left. Draws directly onto the page (top-left). Centralized for a
// consistent, distinctive look across every page.
void theme_add_header(lv_obj_t *page, const char *title);

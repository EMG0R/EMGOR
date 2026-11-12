#include "ui.h"
#include "watch_state.h"
#include "starfield.h"
#include "../theme.h"
#include "../config.h"
#include "../hal/display.h"
#include "../hal/battery.h"
#include "../ble/ble_gadgetbridge.h"

#include <time.h>
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Page indices (left-to-right scroll order). Default (boot) face = analog clock.
//   Camera -> Music -> Analog(DEFAULT) -> Digital -> Weather -> Calendar
//          -> Notifications -> System
// Camera is leftmost; Music sits immediately LEFT of the clock.
// ---------------------------------------------------------------------------
enum { PAGE_CAMERA = 0, PAGE_MUSIC, PAGE_ANALOG, PAGE_DIGITAL, PAGE_WEATHER,
       PAGE_CALENDAR, PAGE_NOTIF, PAGE_MENU, PAGE_COUNT };

static lv_obj_t *s_tv;  // tileview
static lv_obj_t *s_bg;  // shared full-screen animated background (wavy starfield)

// --- analog watch face (lv_meter: ticks + hour/min/sec needles) ---
static lv_obj_t          *analog_meter = nullptr;
static lv_meter_indicator_t *ind_hour = nullptr, *ind_min = nullptr, *ind_sec = nullptr;

// --- widget handles we update at runtime ---
static lv_obj_t *lbl_time, *lbl_ampm, *lbl_date, *lbl_conn_dot;
// digital-face glanceable widgets (batteries top-left, temp bottom-right)
static lv_obj_t *lbl_face_wx, *lbl_face_batt, *lbl_face_phone;
static lv_obj_t *lbl_wx_temp, *lbl_wx_text, *lbl_wx_loc, *lbl_wx_extra;
static lv_obj_t *lbl_cal_title, *lbl_cal_time, *lbl_cal_loc;
static lv_obj_t *lbl_mu_track, *lbl_mu_artist, *btn_play_lbl;
// small MAC / PHONE badge (top-right of the Music page) showing the ACTIVE source
static lv_obj_t *lbl_mu_src = nullptr;
// side transport button labels (swap between prev/next and rewind/forward ~30s)
static lv_obj_t *lbl_mu_left, *lbl_mu_right;
// audiobook sleep-timer: thin full-width button across the TOP of the Music page
// (shown only when an audiobook source is active). Label doubles as a live
// countdown when the phone reports an active timer.
static lv_obj_t *sleep_btn = nullptr, *lbl_sleep = nullptr;
static lv_obj_t *notif_list;
static lv_obj_t *lbl_menu_status, *lbl_menu_batt, *lbl_menu_phone, *lbl_menu_mac;
// --- camera remote page (no mode toggle; just a shutter) ---
static lv_obj_t *lbl_cam_status, *cam_rec_dot;
// --- full-screen notification overlay (floats on the top layer) ---
static lv_obj_t *s_notif_overlay = nullptr;
// --- full-screen permission-approval card (distinct accent; top layer) ---
static lv_obj_t *s_perm_overlay = nullptr;
static uint32_t  s_perm_id = 0;       // id to echo back in permResp
static uint16_t  s_perm_handle = 0;   // BLE conn handle the response routes to

// --- local clock anchor (so the time ticks between BLE syncs) ---
static uint32_t s_epoch = 0;         // UTC epoch from the phone
static uint32_t s_anchor_ms = 0;
static bool     s_time_valid = false;
static int      s_last_min = -1;

// --- audiobook sleep-timer (fully WATCH-LOCAL) ---
// The watch runs the countdown itself; when it hits 0 it sends a music "pause"
// directly, no phone-side timer involved. Tap to arm 10 min. Tapping again while
// running RESETS to 10 min; a second tap within SLEEP_CANCEL_MS of the last tap
// instead CANCELS the timer.
static const uint32_t SLEEP_DURATION_MS = 10UL * 60UL * 1000UL;   // 10 minutes
static const uint32_t SLEEP_CANCEL_MS   = 10UL * 1000UL;          // double-tap window
static bool     s_sleep_active = false;
static uint32_t s_sleep_end_ms = 0;      // millis() when the timer fires
static uint32_t s_sleep_last_tap_ms = 0; // millis() of the previous tap (for cancel)

// ===========================================================================
// Small helpers
// ===========================================================================
static lv_obj_t *make_page(int idx_col) {
  lv_obj_t *tile = lv_tileview_add_tile(s_tv, idx_col, 0, LV_DIR_HOR);
  theme_style_page(tile);
  lv_obj_set_style_pad_all(tile, 14, 0);
  return tile;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt,
                            const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

// Pick a battery glyph for a 0..100 percentage (or "?" if unknown).
static const char *batt_symbol(int pct) {
  if (pct < 0)  return LV_SYMBOL_BATTERY_EMPTY;
  if (pct >= 80) return LV_SYMBOL_BATTERY_FULL;
  if (pct >= 55) return LV_SYMBOL_BATTERY_3;
  if (pct >= 30) return LV_SYMBOL_BATTERY_2;
  if (pct >= 10) return LV_SYMBOL_BATTERY_1;
  return LV_SYMBOL_BATTERY_EMPTY;
}

// Format a UTC epoch as local 12-hour "3:00PM" using the current tz offset.
// (also used by the live clock below as the local offset from E.setTimeZone.)
static int32_t s_tz_min = 0;
static void fmt_local_hhmm(uint32_t utc_epoch, char *buf, size_t n) {
  time_t local = (time_t)((int64_t)utc_epoch + (int64_t)s_tz_min * 60);
  struct tm tmv; gmtime_r(&local, &tmv);
  int h12 = tmv.tm_hour % 12; if (h12 == 0) h12 = 12;
  snprintf(buf, n, "%d:%02d%s", h12, tmv.tm_min, tmv.tm_hour >= 12 ? "PM" : "AM");
}

// ===========================================================================
// Page builders
// ===========================================================================
// --- PAGE 0: high-quality ANALOG watch face (default) ----------------------
// A clean, premium analog clock built on lv_meter: 60 minute ticks with bold
// 12-hour major marks, plus anti-aliased hour / minute / second needle lines and
// a small center cap. Deliberately bare -- nothing but the clock on the violet
// background. Everything sits on a single 0..60 scale so all three hands share
// one geometry (hour = hh*5 + mm/12, minute = mm, second = ss).
static void build_analog(lv_obj_t *p) {
  // This face fills the page edge-to-edge, so drop the page padding and let the
  // dial use the full short dimension of the 240x280 panel.
  lv_obj_set_style_pad_all(p, 0, 0);

  int d = display_width() - 16;   // 224 px dial, centered with a small margin
  analog_meter = lv_meter_create(p);
  lv_obj_remove_style_all(analog_meter);           // strip the default ring/bg
  lv_obj_set_size(analog_meter, d, d);
  lv_obj_center(analog_meter);
  lv_obj_set_style_bg_opa(analog_meter, LV_OPA_TRANSP, 0);   // show violet bg
  lv_obj_set_style_pad_all(analog_meter, 4, 0);
  lv_obj_clear_flag(analog_meter, LV_OBJ_FLAG_SCROLLABLE);

  lv_meter_scale_t *scale = lv_meter_add_scale(analog_meter);
  // MINIMALIST redesign: NO tick marks at all (count 0). The needles only need
  // the scale's angular range/rotation, not visible ticks.
  lv_meter_set_scale_ticks(analog_meter, scale, 0, 0, 0, THEME_COL_BG);
  lv_meter_set_scale_major_ticks(analog_meter, scale, 1, 0, 0, THEME_COL_BG, 0);
  // 0..60 over a full 360 degrees, rotated so value 0 is at the TOP (12 o'clock).
  lv_meter_set_scale_range(analog_meter, scale, 0, 60, 360, 270);

  // --- 12 BOLD hour NUMBERS positioned around the dial (replaces the ticks) ---
  // Placed on a circle of radius r via trig, relative to the meter's center.
  // montserrat_28 is the largest built-in that lets two-digit numbers (10/11/12)
  // sit at 30-degree spacing without overlapping -> big, bold, minimal.
  const int   r = d / 2 - 22;                 // number ring radius (~90 px)
  static const char *nums[12] = { "12","1","2","3","4","5","6","7","8","9","10","11" };
  for (int i = 0; i < 12; i++) {
    lv_obj_t *n = lv_label_create(analog_meter);
    lv_label_set_text(n, nums[i]);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(n, THEME_COL_CLOCK_NUM, 0);
    float ang = i * 30.0f * 3.14159265f / 180.0f;   // 0 = top (12), clockwise
    int ox = (int)lroundf(r * sinf(ang));
    int oy = (int)lroundf(-r * cosf(ang));
    lv_obj_align(n, LV_ALIGN_CENTER, ox, oy);
  }

  // Hands (drawn on the same scale). Thicker/chunkier + all BRIGHT PURPLE (no
  // red second hand). Longer negative r_mod = shorter hand.
  ind_hour = lv_meter_add_needle_line(analog_meter, scale, 10, THEME_COL_CLOCK_HAND, -68);
  ind_min  = lv_meter_add_needle_line(analog_meter, scale, 8,  THEME_COL_CLOCK_HAND, -34);
  ind_sec  = lv_meter_add_needle_line(analog_meter, scale, 4,  THEME_COL_CLOCK_SEC,  -22);

  // Center cap over the needle pivots — bright purple.
  lv_obj_t *cap = lv_obj_create(analog_meter);
  lv_obj_remove_style_all(cap);
  lv_obj_set_size(cap, 16, 16);
  lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cap, THEME_COL_CLOCK_HAND, 0);
  lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
  lv_obj_center(cap);
}

// --- PAGE 1: minimal DIGITAL face ------------------------------------------
// Decluttered: BOTH battery percentages (watch + phone) top-left, big 12h time
// + AM/PM centered, date under it, temperature bottom-right. No brand tag, no
// calendar line (calendar lives on its own page).
static void build_digital(lv_obj_t *p) {
  // Batteries TOP-LEFT: watch (top line) + phone (line under). Distinguished by
  // stack order + color (watch = white, phone = muted lavender).
  lbl_face_batt = make_label(p, "", THEME_FONT_SMALL, THEME_COL_TEXT);
  lv_obj_align(lbl_face_batt, LV_ALIGN_TOP_LEFT, 0, 0);

  lbl_face_phone = make_label(p, "", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_align(lbl_face_phone, LV_ALIGN_TOP_LEFT, 0, 24);

  // BLE connection dot, top-right.
  lbl_conn_dot = make_label(p, LV_SYMBOL_BLUETOOTH, THEME_FONT_SMALL, THEME_COL_WARN);
  lv_obj_align(lbl_conn_dot, LV_ALIGN_TOP_RIGHT, 0, 0);

  // Big 12-hour time, centered.
  lbl_time = make_label(p, "--:--", THEME_FONT_HUGE, THEME_COL_TEXT);
  lv_obj_align(lbl_time, LV_ALIGN_CENTER, -6, -22);

  // AM/PM marker, aligned to the top-right of the big digits (set in update).
  lbl_ampm = make_label(p, "", THEME_FONT_MEDIUM, THEME_COL_ACCENT);
  lv_obj_align_to(lbl_ampm, lbl_time, LV_ALIGN_OUT_RIGHT_TOP, 6, 8);

  // Accent hairline under the time.
  lv_obj_t *bar = theme_make_bar(p, 130, 3, THEME_COL_ACCENT);
  lv_obj_align(bar, LV_ALIGN_CENTER, 0, 22);

  lbl_date = make_label(p, "", THEME_FONT_MEDIUM, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(lbl_date, 2, 0);
  lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, 50);

  // Temperature BOTTOM-RIGHT.
  lbl_face_wx = make_label(p, "", THEME_FONT_MEDIUM, THEME_COL_TEXT);
  lv_obj_align(lbl_face_wx, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
}

static void build_calendar(lv_obj_t *p) {
  theme_add_header(p, "NEXT EVENT");

  lbl_cal_time = make_label(p, "--", THEME_FONT_XL, THEME_COL_ACCENT);
  lv_obj_align(lbl_cal_time, LV_ALIGN_CENTER, 0, -34);

  lbl_cal_title = make_label(p, "No events", THEME_FONT_LARGE, THEME_COL_TEXT);
  lv_obj_set_width(lbl_cal_title, display_width() - 28);
  lv_label_set_long_mode(lbl_cal_title, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl_cal_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_cal_title, LV_ALIGN_CENTER, 0, 20);

  lbl_cal_loc = make_label(p, "", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_width(lbl_cal_loc, display_width() - 28);
  lv_label_set_long_mode(lbl_cal_loc, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lbl_cal_loc, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_cal_loc, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void build_weather(lv_obj_t *p) {
  theme_add_header(p, "WEATHER");

  // Condition ("Sunny") ABOVE the temperature.
  lbl_wx_text = make_label(p, "No data", THEME_FONT_LARGE, THEME_COL_ACCENT);
  lv_obj_set_width(lbl_wx_text, display_width() - 28);
  lv_label_set_long_mode(lbl_wx_text, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(lbl_wx_text, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wx_text, LV_ALIGN_CENTER, 0, -64);

  // Temp: "72°F / 22°C" — F and C same size with a slash between.
  lbl_wx_temp = make_label(p, "--\xC2\xB0", THEME_FONT_LARGE, THEME_COL_TEXT);
  lv_obj_set_width(lbl_wx_temp, display_width() - 8);
  lv_obj_set_style_text_align(lbl_wx_temp, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wx_temp, LV_ALIGN_CENTER, 0, -14);

  lbl_wx_loc = make_label(p, "", THEME_FONT_MEDIUM, THEME_COL_TEXT_MUTED);
  lv_obj_align(lbl_wx_loc, LV_ALIGN_CENTER, 0, 32);

  // AQI (+ humidity/wind) at the bottom.
  lbl_wx_extra = make_label(p, "", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_width(lbl_wx_extra, display_width() - 8);
  lv_obj_set_style_text_align(lbl_wx_extra, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wx_extra, LV_ALIGN_BOTTOM_MID, 0, -6);
}

// music button callback. The two SIDE buttons carry logical "left"/"right"
// tags; the actual command depends on whether an audiobook source is active:
//   music     : left=previous  right=next
//   audiobook : left=rewind    right=forward   (~30s skips, decided by phone)
static void mu_btn_cb(lv_event_t *e) {
  const char *cmd = (const char *)lv_event_get_user_data(e);
  if (!strcmp(cmd, "toggle")) {
    ws_lock(); bool playing = g_ws.mu_playing; ws_unlock();
    ble_music_control(playing ? "pause" : "play");
  } else if (!strcmp(cmd, "left")) {
    ws_lock(); bool ab = g_ws.mu_ab; ws_unlock();
    ble_music_control(ab ? "rewind" : "previous");
  } else if (!strcmp(cmd, "right")) {
    ws_lock(); bool ab = g_ws.mu_ab; ws_unlock();
    ble_music_control(ab ? "forward" : "next");
  } else {
    ble_music_control(cmd);
  }
}

static lv_obj_t *make_ctrl_btn(lv_obj_t *parent, const char *symbol, const char *cmd) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, 62, 62);
  lv_obj_set_style_radius(b, 6, 0);                 // sharp-ish, not pill
  lv_obj_set_style_bg_color(b, THEME_COL_SURFACE, 0);
  lv_obj_set_style_bg_color(b, THEME_COL_SURFACE_HI, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, THEME_COL_ACCENT, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_add_event_cb(b, mu_btn_cb, LV_EVENT_CLICKED, (void *)cmd);
  lv_obj_t *l = make_label(b, symbol, THEME_FONT_MEDIUM, THEME_COL_TEXT);
  lv_obj_center(l);
  return b;
}

// Invisible touch-target column that ENLARGES a transport button's hit area
// without changing anything visible. Coordinates are given in SCREEN space and
// converted to the tile's content space by subtracting the page padding (14px,
// set in make_page). The zone is CLICKABLE but NOT SCROLLABLE, so a tap fires
// its command while a horizontal drag still bubbles up to the tileview for
// swipe-to-change-page (same proven pattern as the whole-page Camera tap). Added
// AFTER the visible buttons so it sits on top and reliably catches the tap; the
// icons underneath are untouched.
#define MUSIC_PAGE_PAD 14
static lv_obj_t *make_music_hitzone(lv_obj_t *parent, int sx, int sw, int sy,
                                    int sh, const char *cmd) {
  lv_obj_t *z = lv_obj_create(parent);
  lv_obj_remove_style_all(z);
  lv_obj_set_pos(z, sx - MUSIC_PAGE_PAD, sy - MUSIC_PAGE_PAD);
  lv_obj_set_size(z, sw, sh);
  lv_obj_set_style_bg_opa(z, LV_OPA_TRANSP, 0);      // invisible
  lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(z, LV_OBJ_FLAG_SCROLLABLE);      // let tileview own swipes
  lv_obj_add_event_cb(z, mu_btn_cb, LV_EVENT_CLICKED, (void *)cmd);
  return z;
}

static void update_sleep_btn();   // fwd decl (defined below; used by the tap cb)

// Sleep-timer button tap (WATCH-LOCAL). Semantics:
//   idle            -> arm a fresh 10-minute countdown
//   running (slow)  -> RESET back to 10 minutes
//   running (fast)  -> if this tap is within SLEEP_CANCEL_MS of the previous
//                      tap, CANCEL the timer instead
// So: first tap arms; tapping again resets; double-tapping quickly cancels.
static void sleep_btn_cb(lv_event_t *e) {
  LV_UNUSED(e);
  uint32_t now = millis();
  bool double_tap = (now - s_sleep_last_tap_ms) <= SLEEP_CANCEL_MS;
  s_sleep_last_tap_ms = now;

  if (s_sleep_active && double_tap) {
    s_sleep_active = false;                 // quick second tap -> cancel
  } else {
    s_sleep_active = true;                  // arm or reset to full 10 min
    s_sleep_end_ms = now + SLEEP_DURATION_MS;
  }
  update_sleep_btn();
}

// Repaint the audiobook sleep button: visible only when an audiobook source is
// active; live "SLEEP M:SS" countdown when a timer is running, else "SLEEP 10 MIN".
static void update_sleep_btn() {
  if (!sleep_btn || !lbl_sleep) return;
  ws_lock();
  bool ab = g_ws.mu_ab;
  ws_unlock();
  if (!ab) { lv_obj_add_flag(sleep_btn, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_clear_flag(sleep_btn, LV_OBJ_FLAG_HIDDEN);

  if (s_sleep_active) {
    int rem = (int)((int32_t)(s_sleep_end_ms - millis()) / 1000);
    if (rem < 0) rem = 0;
    char b[24];
    snprintf(b, sizeof(b), "SLEEP %d:%02d", rem / 60, rem % 60);
    lv_label_set_text(lbl_sleep, b);
  } else {
    lv_label_set_text(lbl_sleep, "SLEEP 10 MIN");
  }
}

static void build_music(lv_obj_t *p) {
  theme_add_header(p, "MUSIC");

  // Small ACTIVE-SOURCE badge, top-right (MAC / PHONE). Follows arbitration so
  // the user can see which device the play screen is currently controlling.
  lbl_mu_src = make_label(p, "", THEME_FONT_SMALL, THEME_COL_ACCENT_DIM);
  lv_obj_set_style_text_letter_space(lbl_mu_src, 2, 0);
  lv_obj_align(lbl_mu_src, LV_ALIGN_TOP_RIGHT, 0, 2);

  // Audiobook sleep-timer: a THIN full-width bar pinned to the very TOP of the
  // tile (edge-to-edge, so it sits above the page's 14px padding). Hidden for
  // regular music; only shown when an audiobook source is active. It is
  // CLICKABLE but NOT SCROLLABLE, so a tap arms/cancels the timer while a
  // horizontal drag still bubbles to the tileview for swipe-to-change-page. It
  // sits well above the transport buttons (bottom of the page), so it never
  // interferes with them.
  sleep_btn = lv_btn_create(p);
  lv_obj_set_pos(sleep_btn, -MUSIC_PAGE_PAD, -MUSIC_PAGE_PAD);   // physical top-left
  lv_obj_set_size(sleep_btn, display_width(), 46);              // taller bar, easy to tap
  // Extend the touch target further DOWN past the visible bar (esp. toward the
  // top edge where fingers land short) so it's forgiving to hit.
  lv_obj_set_ext_click_area(sleep_btn, 18);
  lv_obj_set_style_radius(sleep_btn, 0, 0);
  lv_obj_set_style_bg_color(sleep_btn, THEME_COL_BG_ALT, 0);    // deep, subtle
  lv_obj_set_style_bg_color(sleep_btn, THEME_COL_SURFACE_HI, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(sleep_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sleep_btn, 0, 0);
  lv_obj_set_style_shadow_width(sleep_btn, 0, 0);
  lv_obj_clear_flag(sleep_btn, LV_OBJ_FLAG_SCROLLABLE);         // let tileview own swipes
  lv_obj_add_event_cb(sleep_btn, sleep_btn_cb, LV_EVENT_CLICKED, NULL);
  lbl_sleep = make_label(sleep_btn, "SLEEP 10 MIN", THEME_FONT_SMALL, THEME_COL_TEXT);
  lv_obj_center(lbl_sleep);
  lv_obj_add_flag(sleep_btn, LV_OBJ_FLAG_HIDDEN);              // regular music: hidden

  // FIX 1: single-line horizontal marquee for BOTH title and artist so long
  // names never wrap to multiple lines.
  lbl_mu_track = make_label(p, "Nothing playing", THEME_FONT_LARGE, THEME_COL_TEXT);
  lv_label_set_long_mode(lbl_mu_track, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(lbl_mu_track, display_width() - 28);
  lv_obj_set_style_text_align(lbl_mu_track, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_mu_track, LV_ALIGN_CENTER, 0, -54);

  lbl_mu_artist = make_label(p, "", THEME_FONT_MEDIUM, THEME_COL_ACCENT);
  lv_label_set_long_mode(lbl_mu_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);  // FIX 1
  lv_obj_set_width(lbl_mu_artist, display_width() - 28);
  lv_obj_set_style_text_align(lbl_mu_artist, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_mu_artist, LV_ALIGN_CENTER, 0, -16);

  // Control row: prev / play-pause / next
  lv_obj_t *row = lv_obj_create(p);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, display_width() - 20, 70);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Side buttons carry logical "left"/"right" tags; their glyph/label and the
  // command they send are swapped in update_music() when the audiobook flag flips.
  lv_obj_t *bl = make_ctrl_btn(row, LV_SYMBOL_PREV, "left");
  lbl_mu_left = lv_obj_get_child(bl, 0);
  lv_obj_t *play = make_ctrl_btn(row, LV_SYMBOL_PLAY, "toggle");
  btn_play_lbl = lv_obj_get_child(play, 0);  // remember to swap play/pause glyph
  lv_obj_t *br = make_ctrl_btn(row, LV_SYMBOL_NEXT, "right");
  lbl_mu_right = lv_obj_get_child(br, 0);

  // Enlarge the touch targets WITHOUT moving the icons: three tall invisible
  // columns, all reaching the bottom edge. Left column reaches the LEFT screen
  // edge (prev/rewind), right column reaches the RIGHT edge (next/forward), the
  // center column stays over the play/pause button. Split at the midpoints
  // between the three 62px buttons (screen x-centers ~50 / 120 / 190):
  //   left  = screen x 0..85, center = 85..155, right = 155..240.
  // Each spans from just above the button row down to the screen bottom.
  const int W = display_width(), H = display_height();
  const int zy = H - 100;                 // top of the hit columns (over the row)
  const int zh = H - zy;                  // reach the bottom edge
  make_music_hitzone(p, 0,        85,      zy, zh, "left");    // to LEFT edge
  make_music_hitzone(p, 85,       70,      zy, zh, "toggle");  // center
  make_music_hitzone(p, 155,      W - 155, zy, zh, "right");   // to RIGHT edge
}

// --- PAGE: CAMERA REMOTE ----------------------------------------------------
// A pure shutter — NO photo/video mode toggle on the watch. The phone's camera
// app decides photo vs video; the watch just opens it and fires the shutter.
// The whole page is the tap target:
//   camera closed -> tap sends {"t":"camera","n":"open"}
//   camera open   -> tap sends {"t":"camera","n":"capture"}
// Local state advances optimistically; a real {"t":"cameraState",...} overrides.
// A small red REC dot shows if the phone reports it is recording.
static void update_camera();  // fwd

// White shutter flash: a full-bleed white panel on the TOP layer that appears at
// full opacity then quickly fades out (~150ms) and deletes itself. Casual camera
// feel; fired on every camera tap (open or capture).
static void cam_flash_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}
static void cam_flash_done_cb(lv_anim_t *a) {
  lv_obj_del_async((lv_obj_t *)a->var);
}
static void cam_shutter_flash() {
  lv_obj_t *fl = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(fl);
  lv_obj_set_size(fl, display_width(), display_height());
  lv_obj_align(fl, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(fl, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(fl, LV_OPA_COVER, 0);
  lv_obj_clear_flag(fl, LV_OBJ_FLAG_CLICKABLE);   // never eat input
  lv_obj_clear_flag(fl, LV_OBJ_FLAG_SCROLLABLE);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, fl);
  lv_anim_set_exec_cb(&a, cam_flash_anim_cb);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&a, 160);                       // quick fade back
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_ready_cb(&a, cam_flash_done_cb);     // self-delete when done
  lv_anim_start(&a);
}

static void cam_tap_cb(lv_event_t *e) {
  LV_UNUSED(e);
  ws_lock();
  bool open = g_ws.cam_open;
  ws_unlock();
  if (!open) {
    ble_camera("open");
    ws_lock(); g_ws.cam_open = true; ws_unlock();      // optimistic
  } else {
    ble_camera("capture");                              // shutter (photo or video)
  }
  cam_shutter_flash();                                  // white shutter flash
  update_camera();
}

static void build_camera(lv_obj_t *p) {
  theme_add_header(p, "CAMERA");

  // The whole tile is a tap target (open/capture). Tileview still owns
  // horizontal swipes; a genuine tap fires LV_EVENT_CLICKED here.
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(p, cam_tap_cb, LV_EVENT_CLICKED, NULL);

  // REC dot (top-left, under header) — red circle shown only while recording.
  cam_rec_dot = lv_obj_create(p);
  lv_obj_remove_style_all(cam_rec_dot);
  lv_obj_set_size(cam_rec_dot, 14, 14);
  lv_obj_set_style_radius(cam_rec_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cam_rec_dot, THEME_COL_WARN, 0);
  lv_obj_set_style_bg_opa(cam_rec_dot, LV_OPA_COVER, 0);
  lv_obj_align(cam_rec_dot, LV_ALIGN_TOP_LEFT, 0, 30);
  lv_obj_add_flag(cam_rec_dot, LV_OBJ_FLAG_HIDDEN);

  // Big "TAP" call-to-action, dead centered. That's the whole UI — the page
  // itself is the tap target and the phone decides photo vs video.
  lbl_cam_status = make_label(p, "TAP", THEME_FONT_HUGE, THEME_COL_TEXT);
  lv_obj_center(lbl_cam_status);
}

static void build_notif(lv_obj_t *p) {
  // FIX: the whole page is ONE vertical scroll container and the "NOTIFICATIONS"
  // header is the FIRST item INSIDE it (added in rebuild_notifications), NOT a
  // fixed page overlay. Consequences:
  //   * the header can never be overlapped by list items (flex column stacks
  //     them below it with real spacing), and
  //   * when the user scrolls the list, the header scrolls up and is clipped at
  //     the container's top edge -> it "goes away completely" instead of
  //     sticking and getting drawn over.
  notif_list = lv_obj_create(p);
  lv_obj_remove_style_all(notif_list);
  lv_obj_set_size(notif_list, lv_pct(100), lv_pct(100));
  lv_obj_align(notif_list, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(notif_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(notif_list, 10, 0);
  lv_obj_set_style_pad_right(notif_list, 6, 0);   // keep cards off the scrollbar edge
  // Vertical scroll WITHIN the page; tileview still handles horizontal swipes.
  // Children outside the container bounds are clipped (LVGL default), so the
  // header disappears cleanly once scrolled past the top.
  lv_obj_set_scroll_dir(notif_list, LV_DIR_VER);
}

// The in-list header (first flex child) so it scrolls with the content.
static void add_notif_header() {
  lv_obj_t *hdr = make_label(notif_list, "NOTIFICATIONS",
                             THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(hdr, 3, 0);
  lv_obj_set_style_pad_bottom(hdr, 4, 0);
}

static void menu_find_cb(lv_event_t *e) { LV_UNUSED(e); ble_find_phone(true); }

static void build_menu(lv_obj_t *p) {
  theme_add_header(p, "SYSTEM");

  lbl_menu_status = make_label(p, "DISCONNECTED", THEME_FONT_LARGE, THEME_COL_WARN);
  lv_obj_align(lbl_menu_status, LV_ALIGN_TOP_MID, 0, 40);

  lbl_menu_batt = make_label(p, "WATCH " LV_SYMBOL_BATTERY_EMPTY " --", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(lbl_menu_batt, 2, 0);
  lv_obj_align(lbl_menu_batt, LV_ALIGN_TOP_MID, 0, 82);

  lbl_menu_phone = make_label(p, "PHONE " LV_SYMBOL_BATTERY_EMPTY " --", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(lbl_menu_phone, 2, 0);
  lv_obj_align(lbl_menu_phone, LV_ALIGN_TOP_MID, 0, 104);

  // Mac battery (from devBattery src:"mac"); hidden text until we hear from it.
  lbl_menu_mac = make_label(p, "MAC " LV_SYMBOL_BATTERY_EMPTY " --", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(lbl_menu_mac, 2, 0);
  lv_obj_align(lbl_menu_mac, LV_ALIGN_TOP_MID, 0, 126);

  lv_obj_t *name = make_label(p, "THE WATCH", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(name, 3, 0);
  lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 150);

  lv_obj_t *btn = lv_btn_create(p);
  lv_obj_set_size(btn, display_width() - 44, 56);
  // Two-tone theme: surface button with a white border + white text (a white
  // accent bg would make the white label invisible).
  lv_obj_set_style_bg_color(btn, THEME_COL_SURFACE, 0);
  lv_obj_set_style_bg_color(btn, THEME_COL_SURFACE_HI, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, THEME_COL_ACCENT, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_add_event_cb(btn, menu_find_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *bl = make_label(btn, LV_SYMBOL_CALL "  FIND PHONE", THEME_FONT_MEDIUM, THEME_COL_TEXT);
  lv_obj_center(bl);
}

// ===========================================================================
// Runtime updates (LVGL context)
// ===========================================================================
static void update_clock() {
  if (!s_time_valid) return;
  // FIX 5: epoch from the phone is UTC; add the local offset (E.setTimeZone) to
  // get local wall-clock time, then present as 12-hour with AM/PM.
  uint32_t utc = s_epoch + (millis() - s_anchor_ms) / 1000;
  time_t local = (time_t)((int64_t)utc + (int64_t)s_tz_min * 60);
  struct tm tmv;
  gmtime_r(&local, &tmv);

  // Drive the ANALOG face. All three hands live on one 0..60 scale:
  //   second = ss ; minute = mm ; hour = (hh%12)*5 + mm/12.
  if (ind_hour && ind_min && ind_sec) {
    int hh = tmv.tm_hour % 12;
    lv_meter_set_indicator_value(analog_meter, ind_sec,  tmv.tm_sec);
    lv_meter_set_indicator_value(analog_meter, ind_min,  tmv.tm_min);
    lv_meter_set_indicator_value(analog_meter, ind_hour, hh * 5 + tmv.tm_min / 12);
  }

  int h12 = tmv.tm_hour % 12;
  if (h12 == 0) h12 = 12;                 // 12/0 boundary: midnight & noon = 12
  const bool pm = tmv.tm_hour >= 12;

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d", h12, tmv.tm_min);
  lv_label_set_text(lbl_time, buf);
  lv_label_set_text(lbl_ampm, pm ? "PM" : "AM");
  // Re-anchor AM/PM to the (variable-width) digits.
  lv_obj_align_to(lbl_ampm, lbl_time, LV_ALIGN_OUT_RIGHT_TOP, 6, 10);

  static const char *wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  static const char *mo[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  char db[24];
  snprintf(db, sizeof(db), "%s %d %s", wd[tmv.tm_wday % 7], tmv.tm_mday, mo[tmv.tm_mon % 12]);
  lv_label_set_text(lbl_date, db);

  if (tmv.tm_min != s_last_min) {         // log once per minute, verifiable
    s_last_min = tmv.tm_min;
    Serial.printf("UI clock -> %d:%02d %s  (%s)\n", h12, tmv.tm_min, pm ? "PM" : "AM", db);
  }
}

static void update_conn(bool connected) {
  lv_obj_set_style_text_color(lbl_conn_dot, connected ? THEME_COL_OK : THEME_COL_WARN, 0);
  lv_label_set_text(lbl_menu_status, connected ? "CONNECTED" : "DISCONNECTED");
  lv_obj_set_style_text_color(lbl_menu_status, connected ? THEME_COL_OK : THEME_COL_WARN, 0);
}

static void update_weather() {
  ws_lock();
  bool valid = g_ws.wx_valid;
  int temp_c = g_ws.wx_temp_c, hum = g_ws.wx_humidity, wind = g_ws.wx_wind, aqi = g_ws.wx_aqi;
  char txt[WS_STR], loc[WS_STR];
  strncpy(txt, g_ws.wx_text, WS_STR); strncpy(loc, g_ws.wx_loc, WS_STR);
  ws_unlock();
  if (!valid) return;
  int temp_f = (int)lroundf(temp_c * 9.0f / 5.0f + 32.0f);
  // Temp: F and C same size, slash between.
  char b[24]; snprintf(b, sizeof(b), "%d\xC2\xB0""F / %d\xC2\xB0""C", temp_f, temp_c);
  lv_label_set_text(lbl_wx_temp, b);
  lv_label_set_text(lbl_wx_text, txt[0] ? txt : "No data");
  lv_label_set_text(lbl_wx_loc, loc);
  // AQI at the bottom (+ humidity/wind).
  char e[72];
  if (aqi >= 0)
    snprintf(e, sizeof(e), "AQI %d   HUM %d%%   WIND %d", aqi, hum, wind);
  else
    snprintf(e, sizeof(e), "HUM %d%%   WIND %d", hum, wind);
  lv_label_set_text(lbl_wx_extra, e);
  // Glanceable temp on the watch face stays short (F only).
  char fb[12]; snprintf(fb, sizeof(fb), "%d\xC2\xB0""F", temp_f);
  lv_label_set_text(lbl_face_wx, fb);
}

static void update_calendar() {
  ws_lock();
  bool valid = g_ws.cal_valid;
  uint32_t t = g_ws.cal_time;
  char title[WS_STR], loc[WS_STR];
  strncpy(title, g_ws.cal_title, WS_STR); title[WS_STR-1]=0;
  strncpy(loc, g_ws.cal_loc, WS_STR); loc[WS_STR-1]=0;
  ws_unlock();
  if (!valid || !title[0]) {
    lv_label_set_text(lbl_cal_time, "--");
    lv_label_set_text(lbl_cal_title, "No events");
    lv_label_set_text(lbl_cal_loc, "");
    return;
  }
  char hhmm[16] = "";
  if (t > 1000000000UL) fmt_local_hhmm(t, hhmm, sizeof(hhmm));
  lv_label_set_text(lbl_cal_time, hhmm[0] ? hhmm : "--");
  lv_label_set_text(lbl_cal_title, title);
  lv_label_set_text(lbl_cal_loc, loc);
}

static void update_phone_batt() {
  ws_lock();
  int pct = g_ws.phone_batt_pct;
  bool chg = g_ws.phone_batt_chg, valid = g_ws.phone_batt_valid;
  ws_unlock();
  char m[40], f[24];
  if (!valid || pct < 0) {
    snprintf(m, sizeof(m), "PHONE %s --", LV_SYMBOL_BATTERY_EMPTY);
    lv_label_set_text(lbl_face_phone, "");
  } else {
    snprintf(m, sizeof(m), "PHONE %s %d%%%s", batt_symbol(pct), pct,
             chg ? " " LV_SYMBOL_CHARGE : "");
    // Digital-face glance (top-left, under the watch battery): "P [bolt]90%".
    snprintf(f, sizeof(f), "P %s%d%%", chg ? LV_SYMBOL_CHARGE " " : "", pct);
    lv_label_set_text(lbl_face_phone, f);
  }
  lv_label_set_text(lbl_menu_phone, m);
}

static void update_mac_batt() {
  ws_lock();
  int pct = g_ws.mac_batt_pct;
  bool chg = g_ws.mac_batt_chg, valid = g_ws.mac_batt_valid;
  ws_unlock();
  char m[40];
  if (!valid || pct < 0) {
    snprintf(m, sizeof(m), "MAC %s --", LV_SYMBOL_BATTERY_EMPTY);
  } else {
    snprintf(m, sizeof(m), "MAC %s %d%%%s", batt_symbol(pct), pct,
             chg ? " " LV_SYMBOL_CHARGE : "");
  }
  lv_label_set_text(lbl_menu_mac, m);
}

static void update_watch_batt() {
  ws_lock();
  int pct = g_ws.battery_pct, mv = g_ws.battery_mv;
  ws_unlock();
  char m[48], f[24];
  if (pct < 0) {
    snprintf(m, sizeof(m), "WATCH %s --", LV_SYMBOL_BATTERY_EMPTY);
    lv_label_set_text(lbl_face_batt, "");
  } else {
    snprintf(m, sizeof(m), "WATCH %s %d%%  %d.%02dV", batt_symbol(pct), pct,
             mv / 1000, (mv % 1000) / 10);
    // Digital-face glance (top-left, above the phone battery): "W [glyph]87%".
    snprintf(f, sizeof(f), "W %s%d%%", batt_symbol(pct), pct);
    lv_label_set_text(lbl_face_batt, f);
  }
  lv_label_set_text(lbl_menu_batt, m);
}

static void update_music() {
  ws_lock();
  bool valid = g_ws.mu_valid, playing = g_ws.mu_playing, ab = g_ws.mu_ab;
  int active_src = g_ws.mu_active_src;
  char track[WS_STR], artist[WS_STR];
  strncpy(track, g_ws.mu_track, WS_STR); strncpy(artist, g_ws.mu_artist, WS_STR);
  ws_unlock();
  // ACTIVE-SOURCE badge (MAC / PHONE), blank when no source is known yet.
  if (lbl_mu_src) {
    lv_label_set_text(lbl_mu_src,
                      active_src == WS_SRC_MAC   ? "MAC"   :
                      active_src == WS_SRC_PHONE ? "PHONE" : "");
  }
  if (valid) {
    lv_label_set_text(lbl_mu_track, track[0] ? track : "Nothing playing");
    lv_label_set_text(lbl_mu_artist, artist);
  }
  lv_label_set_text(btn_play_lbl, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  // Audiobook mode -> the side buttons become ~30s rewind/forward; otherwise the
  // usual previous/next media glyphs.
  if (lbl_mu_left && lbl_mu_right) {
    lv_label_set_text(lbl_mu_left,  ab ? LV_SYMBOL_PREV "30" : LV_SYMBOL_PREV);
    lv_label_set_text(lbl_mu_right, ab ? "30" LV_SYMBOL_NEXT : LV_SYMBOL_NEXT);
  }
  // The sleep button's visibility follows the audiobook flag.
  update_sleep_btn();
}

static void update_camera() {
  if (!lbl_cam_status) return;
  ws_lock();
  bool rec = g_ws.cam_recording;
  ws_unlock();

  // The main label is always just "TAP" (open vs capture is decided under the
  // hood). Only the small REC dot reflects the phone's recording state.
  if (rec) lv_obj_clear_flag(cam_rec_dot, LV_OBJ_FLAG_HIDDEN);
  else     lv_obj_add_flag(cam_rec_dot, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// FULL-SCREEN notification overlay.
//
// When a NEW notification arrives (BLE 'notify' -> WS_DIRTY_NOTIF_POPUP), we
// float a full-bleed panel on the TOP layer showing app / title / body over the
// dark background. It NEVER auto-dismisses; a TAP or a FLICK in ANY direction
// closes it (see notif_overlay_event_cb). Closing (deferred delete) drops us
// straight back to whatever page/tile was underneath. The notification is still
// stored in the ring + shown on the Notifications page as before.
// ---------------------------------------------------------------------------
static void notif_overlay_close() {
  if (s_notif_overlay) {
    lv_obj_t *ov = s_notif_overlay;
    s_notif_overlay = nullptr;
    lv_obj_add_flag(ov, LV_OBJ_FLAG_HIDDEN);  // vanish instantly
    // Deleting an object from inside its OWN input event is unsafe (the indev
    // still references it for the rest of this press cycle). Defer the free.
    lv_obj_del_async(ov);
  }
}

// Reliable dismiss for the full-screen overlay. The old LV_EVENT_GESTURE +
// lv_indev_get_gesture_dir() path did not fire on the top-layer object, so
// nothing dismissed it. Instead we handle raw pointer events on the overlay:
//   * TAP        -> LV_EVENT_CLICKED closes it (the reliable primary path).
//   * FLICK/SWIPE in ANY direction -> a press-to-current drag > ~25px closes it
//     (checked live during LV_EVENT_PRESSING so a fast flick dismisses at once).
// Either gesture works; both just close the overlay.
static lv_point_t s_notif_press_pt;
static void notif_overlay_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_get_act();
  if (code == LV_EVENT_PRESSED) {
    if (indev) lv_indev_get_point(indev, &s_notif_press_pt);
  } else if (code == LV_EVENT_PRESSING) {
    if (!indev) return;
    lv_point_t p; lv_indev_get_point(indev, &p);
    int dx = (int)p.x - s_notif_press_pt.x;
    int dy = (int)p.y - s_notif_press_pt.y;
    if (dx * dx + dy * dy > (25 * 25)) notif_overlay_close();  // flick, any dir
  } else if (code == LV_EVENT_CLICKED) {
    notif_overlay_close();                                     // tap
  }
}

// Turn a notification source into a readable app name. The phone SHOULD send a
// friendly label, but some (Gmail etc.) arrive as a raw package like
// "com.google.android.gm". Map the common ones; generically clean the rest.
static void clean_src(const char *in, char *out, size_t outlen) {
  if (!in || !in[0]) { snprintf(out, outlen, "NOTIFICATION"); return; }
  if (strchr(in, ' ') || !strchr(in, '.')) { snprintf(out, outlen, "%s", in); return; }
  static const struct { const char *pkg; const char *name; } tbl[] = {
    {"com.google.android.gm", "Gmail"},
    {"com.google.android.apps.messaging", "Messages"},
    {"com.google.android.youtube", "YouTube"},
    {"com.google.android.apps.maps", "Maps"},
    {"com.google.android.calendar", "Calendar"},
    {"com.google.android.apps.photos", "Photos"},
    {"com.google.android.gms", "Google"},
    {"com.samsung.android.messaging", "Messages"},
    {"com.samsung.android.email.provider", "Email"},
    {"com.whatsapp", "WhatsApp"},
    {"com.instagram.android", "Instagram"},
    {"com.facebook.katana", "Facebook"},
    {"com.facebook.orca", "Messenger"},
    {"org.telegram.messenger", "Telegram"},
    {"com.snapchat.android", "Snapchat"},
    {"com.twitter.android", "X"},
    {"com.spotify.music", "Spotify"},
    {"com.audible.application", "Audible"},
    {"com.reddit.frontpage", "Reddit"},
    {"com.microsoft.office.outlook", "Outlook"},
  };
  for (unsigned i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++)
    if (!strcmp(in, tbl[i].pkg)) { snprintf(out, outlen, "%s", tbl[i].name); return; }
  const char *last = strrchr(in, '.');
  snprintf(out, outlen, "%s", last ? last + 1 : in);
  if (out[0] >= 'a' && out[0] <= 'z') out[0] = out[0] - 'a' + 'A';  // Title-case
}

static void show_notif_overlay() {
  // Read the newest notification from the ring.
  char src[WS_STR], title[WS_STR], body[WS_BODY];
  ws_lock();
  WsNotification n = g_ws.notif[g_ws.notif_head];
  bool ok = n.used;
  strncpy(src, n.src, WS_STR); src[WS_STR-1]=0;
  strncpy(title, n.title, WS_STR); title[WS_STR-1]=0;
  strncpy(body, n.body, WS_BODY); body[WS_BODY-1]=0;
  ws_unlock();
  if (!ok) return;

  // Replace any overlay already up (newest wins).
  notif_overlay_close();

  lv_obj_t *ov = lv_obj_create(lv_layer_top());
  s_notif_overlay = ov;
  lv_obj_remove_style_all(ov);
  lv_obj_set_size(ov, display_width(), display_height());
  lv_obj_align(ov, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(ov, THEME_COL_BG, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(ov, 16, 0);
  lv_obj_set_flex_flow(ov, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ov, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(ov, 10, 0);
  // Must be clickable so the top-layer object captures the press/release and the
  // in-between PRESSING updates; NOT scrollable so drags aren't consumed as
  // scrolls. A tap (CLICKED) or a >25px flick in any direction dismisses it.
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(ov, notif_overlay_event_cb, LV_EVENT_ALL, NULL);

  // App / source caption (cleaned: Gmail etc. instead of com.google.android.gm).
  char srcName[WS_STR]; clean_src(src, srcName, WS_STR);
  lv_obj_t *lsrc = make_label(ov, srcName,
                              THEME_FONT_SMALL, THEME_COL_ACCENT);
  lv_obj_set_style_text_letter_space(lsrc, 3, 0);

  if (title[0]) {
    lv_obj_t *lt = make_label(ov, title, THEME_FONT_LARGE, THEME_COL_TEXT);
    lv_obj_set_width(lt, display_width() - 32);
    lv_label_set_long_mode(lt, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lt, LV_TEXT_ALIGN_CENTER, 0);
  }

  if (body[0]) {
    lv_obj_t *lb = make_label(ov, body, THEME_FONT_MEDIUM, THEME_COL_TEXT_MUTED);
    lv_obj_set_width(lb, display_width() - 32);
    // Cap the body at 4 lines: fix the height to 4 line-heights and use DOT mode
    // so it wraps within the width then clips the last visible line with "…".
    lv_obj_set_height(lb, 4 * lv_font_get_line_height(THEME_FONT_MEDIUM));
    lv_label_set_long_mode(lb, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
  }

  // Dismiss hint pinned to the bottom.
  lv_obj_t *hint = make_label(ov, "DISMISS", THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
  lv_obj_set_style_text_letter_space(hint, 2, 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

// ---------------------------------------------------------------------------
// REMOTE PERMISSION card ("the super key", PROTOCOL_V2.md).
//
// A central (e.g. the Mac) sends a permReq; we pop a DISTINCT full-screen card
// (lavender-accented, unlike the flat notification overlay) with YES / NO. The
// tap is relayed back to the REQUESTING connection via ble_perm_response():
//   * short-tap YES -> approve:true  scope:"once"
//   * long-press YES -> approve:true  scope:"always" ("yes, don't ask again")
//   * NO            -> approve:false scope:"once"
// This card is NEVER gated by notif_suppress_until — permission prompts always
// show. Deletes are deferred (del_async) because we free from inside the card's
// own input event.
// ---------------------------------------------------------------------------
static void reconcile_perm_card();   // fwd decl

static void perm_card_close() {
  if (s_perm_overlay) {
    lv_obj_t *ov = s_perm_overlay;
    s_perm_overlay = nullptr;                 // guard against a second event
    lv_obj_add_flag(ov, LV_OBJ_FLAG_HIDDEN);
    lv_obj_del_async(ov);
  }
}

// Full-screen TAP anywhere = APPROVE. There is no NO button by design — if you
// don't want it, just don't tap and it times out on the requester side. A tap
// approves with scope "session" ("yes for the whole session", so that window
// stops asking). The response routes back to the request's own conn handle+id,
// then that request is popped and the next queued one (if any) is shown.
static void perm_approve_cb(lv_event_t *e) {
  LV_UNUSED(e);
  if (!s_perm_overlay) return;                // already answered / mid-close
  ble_perm_response(s_perm_handle, s_perm_id, true, "session");
  Serial.printf("PERM approve(session): id=%u -> handle %u\n",
                (unsigned)s_perm_id, (unsigned)s_perm_handle);
  // Pop the front request (the one we just answered) from the queue.
  ws_lock();
  if (g_ws.perm_count > 0) {
    for (uint8_t j = 0; j + 1 < g_ws.perm_count; j++) {
      g_ws.perm_id[j] = g_ws.perm_id[j+1];
      g_ws.perm_handle[j] = g_ws.perm_handle[j+1];
      memcpy(g_ws.perm_title[j],  g_ws.perm_title[j+1],  WS_STR);
      memcpy(g_ws.perm_body[j],   g_ws.perm_body[j+1],   WS_BODY);
      memcpy(g_ws.perm_origin[j], g_ws.perm_origin[j+1], WS_STR);
    }
    g_ws.perm_count--;
  }
  ws_unlock();
  perm_card_close();
  reconcile_perm_card();                       // show the next one, or nothing
}

// Bring the on-screen card in sync with the queue: show the FRONT request; if
// the queue is empty, close the card; if it already shows the front id, leave
// it. Called whenever the perm queue changes (new request, cancel, or approve).
static void reconcile_perm_card() {
  char title[WS_STR], body[WS_BODY], origin[WS_STR];
  uint8_t count;
  uint32_t front_id;
  ws_lock();
  count = g_ws.perm_count;
  if (count > 0) {
    front_id      = g_ws.perm_id[0];
    s_perm_handle = g_ws.perm_handle[0];
    strncpy(title,  g_ws.perm_title[0],  WS_STR);  title[WS_STR-1]=0;
    strncpy(body,   g_ws.perm_body[0],   WS_BODY); body[WS_BODY-1]=0;
    strncpy(origin, g_ws.perm_origin[0], WS_STR);  origin[WS_STR-1]=0;
  }
  ws_unlock();

  if (count == 0) { perm_card_close(); return; }
  if (s_perm_overlay && s_perm_id == front_id) return;   // already showing it
  perm_card_close();
  s_perm_id = front_id;

  // Full-screen card: raised surface + thick lavender border. The overlay ITSELF
  // is the tap target (whole screen), so approving is a single forgiving tap.
  lv_obj_t *ov = lv_obj_create(lv_layer_top());
  s_perm_overlay = ov;
  lv_obj_remove_style_all(ov);
  lv_obj_set_size(ov, display_width(), display_height());
  lv_obj_align(ov, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(ov, THEME_COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ov, THEME_COL_SURFACE_HI, LV_STATE_PRESSED);   // tap feedback
  lv_obj_set_style_border_color(ov, THEME_COL_ACCENT_DIM, 0);
  lv_obj_set_style_border_width(ov, 4, 0);
  lv_obj_set_style_pad_all(ov, 14, 0);
  lv_obj_set_flex_flow(ov, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ov, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(ov, 10, 0);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ov, perm_approve_cb, LV_EVENT_CLICKED, NULL);

  // Caption: "PERMISSION · MAC", plus a queue counter when >1 is waiting.
  char cap[WS_STR + 24];
  if (count > 1)
    snprintf(cap, sizeof(cap), "PERMISSION%s%s  (1/%d)",
             origin[0] ? " \xC2\xB7 " : "", origin, (int)count);
  else
    snprintf(cap, sizeof(cap), "PERMISSION%s%s", origin[0] ? " \xC2\xB7 " : "", origin);
  for (char *q = cap; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;   // upper
  lv_obj_t *lc = make_label(ov, cap, THEME_FONT_SMALL, THEME_COL_ACCENT_DIM);
  lv_obj_set_style_text_letter_space(lc, 3, 0);
  lv_obj_add_flag(lc, LV_OBJ_FLAG_EVENT_BUBBLE);   // taps on text still approve

  lv_obj_t *lt = make_label(ov, title[0] ? title : "Approve?",
                            THEME_FONT_LARGE, THEME_COL_TEXT);
  lv_obj_set_width(lt, display_width() - 40);
  lv_label_set_long_mode(lt, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lt, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(lt, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (body[0]) {
    lv_obj_t *lb = make_label(ov, body, THEME_FONT_SMALL, THEME_COL_TEXT_MUTED);
    lv_obj_set_width(lb, display_width() - 40);
    lv_obj_set_height(lb, 3 * lv_font_get_line_height(THEME_FONT_SMALL));
    lv_label_set_long_mode(lb, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(lb, LV_OBJ_FLAG_EVENT_BUBBLE);
  }

  // Big, unmistakable call to action.
  lv_obj_t *cta = make_label(ov, "TAP TO APPROVE", THEME_FONT_MEDIUM, THEME_COL_ACCENT);
  lv_obj_set_style_text_letter_space(cta, 2, 0);
  lv_obj_add_flag(cta, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *hint = make_label(ov, "approves for this session",
                              &lv_font_montserrat_14, THEME_COL_TEXT_MUTED);
  lv_obj_add_flag(hint, LV_OBJ_FLAG_EVENT_BUBBLE);
}

// Notification card dismiss handler (uses the notification id stored as user data)
static void notif_dismiss_cb(lv_event_t *e) {
  uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  ble_notify_action(id, "DISMISS", nullptr);
  ws_lock(); ws_remove_notification(id); ws_mark_dirty(WS_DIRTY_NOTIF); ws_unlock();
}

static void rebuild_notifications() {
  lv_obj_clean(notif_list);
  // (No "NOTIFICATIONS" title header — removed per design for a cleaner top.)
  ws_lock();
  // Walk newest -> oldest through the ring.
  int shown = 0;
  for (int k = 0; k < WS_NOTIF_MAX; k++) {
    int i = (g_ws.notif_head - k + WS_NOTIF_MAX) % WS_NOTIF_MAX;
    WsNotification n = g_ws.notif[i];
    if (!n.used) continue;
    ws_unlock();
    // Flex COLUMN so src / title / body always stack cleanly with real spacing
    // (the old fixed-offset layout overlapped title-on-body). Compact fonts.
    lv_obj_t *card = theme_make_card(notif_list);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_pad_left(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 3, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, notif_dismiss_cb, LV_EVENT_LONG_PRESSED,
                        (void *)(uintptr_t)n.id);

    // App/source: tiny tracked-out accent caption (cleaned app name).
    char srcName[WS_STR]; clean_src(n.src, srcName, WS_STR);
    lv_obj_t *src = make_label(card, srcName,
                              &lv_font_montserrat_14, THEME_COL_ACCENT);
    lv_obj_set_style_text_letter_space(src, 2, 0);

    // Title: small bold-weight look via a compact size, single-line ellipsis.
    if (n.title[0]) {
      lv_obj_t *ti = make_label(card, n.title, &lv_font_montserrat_20, THEME_COL_TEXT);
      lv_obj_set_width(ti, lv_pct(100));
      lv_label_set_long_mode(ti, LV_LABEL_LONG_DOT);
    }

    // Body: smaller, wraps cleanly under the title, capped at 4 lines with "…".
    if (n.body[0]) {
      lv_obj_t *bo = make_label(card, n.body, &lv_font_montserrat_14, THEME_COL_TEXT_MUTED);
      lv_obj_set_width(bo, lv_pct(100));
      lv_obj_set_height(bo, 4 * lv_font_get_line_height(&lv_font_montserrat_14));
      lv_label_set_long_mode(bo, LV_LABEL_LONG_DOT);
    }
    ws_lock();
    if (++shown >= WS_NOTIF_MAX) break;
  }
  bool empty = (shown == 0);
  ws_unlock();
  if (empty) {
    lv_obj_t *l = make_label(notif_list, "NO NOTIFICATIONS",
                             THEME_FONT_MEDIUM, THEME_COL_TEXT_MUTED);
    lv_obj_center(l);
  }
}

// The single periodic pump: repaint whatever changed + tick the clock.
static void ui_timer_cb(lv_timer_t *t) {
  LV_UNUSED(t);
  uint32_t d = ws_take_dirty();

  if (d & WS_DIRTY_TIME) {
    ws_lock();
    s_epoch = g_ws.epoch; s_anchor_ms = millis();
    s_tz_min = g_ws.tz_offset_min; s_time_valid = g_ws.time_valid;
    ws_unlock();
    s_last_min = -1;   // force a fresh clock log after a sync
  }
  if (d & WS_DIRTY_CONN) {
    ws_lock(); bool c = g_ws.ble_connected; ws_unlock();
    update_conn(c);
  }
  if (d & WS_DIRTY_WEATHER)  update_weather();
  if (d & WS_DIRTY_MUSIC)    update_music();
  if (d & WS_DIRTY_NOTIF)    rebuild_notifications();
  // Full-screen popup ONLY for genuinely new notifications — not the backfill
  // burst right after boot/connect (that just lands in the list).
  if ((d & WS_DIRTY_NOTIF_POPUP) && (int32_t)(millis() - g_ws.notif_suppress_until) >= 0) {
    show_notif_overlay();
  }
  // Sleep timer is fully watch-local now; the phone no longer drives it.
  if (d & WS_DIRTY_CAMERA)   update_camera();
  if (d & WS_DIRTY_CAL)      update_calendar();
  if (d & WS_DIRTY_PHONEBAT) update_phone_batt();
  if (d & WS_DIRTY_MACBAT)   update_mac_batt();
  if (d & WS_DIRTY_WATCHBAT) update_watch_batt();
  // Remote permission request -> distinct full-screen YES/NO card. NOT gated by
  // notif_suppress_until (permission prompts always show, PROTOCOL_V2.md).
  if (d & WS_DIRTY_PERM)     reconcile_perm_card();

  // Poll the WATCH's own battery (local ADC) periodically. 150 ms tick * 40 = 6 s.
  // First tick reads immediately so the gauge is populated at boot.
  static int batt_ctr = 0;
  if (batt_ctr-- <= 0) {
    batt_ctr = 40;
    int mv = 0;
    int pct = battery_read(&mv);
    ws_lock();
    g_ws.battery_pct = pct;
    g_ws.battery_mv  = mv;
    ws_unlock();
    update_watch_batt();
  }

  update_clock();  // every tick, so the minute rolls over on its own

  // Tick the audiobook sleep-timer countdown live (only when one is running).
  // When it expires, send the pause ourselves and revert the label.
  if (s_sleep_active) {
    if ((int32_t)(millis() - s_sleep_end_ms) >= 0) {
      s_sleep_active = false;
      ble_music_control("pause");     // watch fires the pause itself
    }
    update_sleep_btn();
  }
}

// ===========================================================================
// Public
// ===========================================================================
void ui_init() {
  // Root/active screen: fill the full 240x280 with violet and zero every
  // default border/outline/pad/radius so no white shows at the left edge or in
  // the sub-pixel gap between pages during a swipe.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_size(scr, display_width(), display_height());
  lv_obj_set_style_bg_color(scr, THEME_COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_outline_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  // --- Shared animated background -------------------------------------------
  // ONE full-screen container behind the tileview holds the animated background
  // (the wavy twinkling starfield). Every page/tile is TRANSPARENT so this same
  // animation shows through consistently on EVERY page. It is opaque dark, fully
  // covering 240x280, so any swipe gap shows the dark animated bg, never white.
  // Created BEFORE the tileview => it sits behind all page content.
  //
  // NOTE: the old full-screen lava-lamp metaball canvas layer was REMOVED — it
  // invalidated the entire 240x280 frame every tick (a full SPI flush) which
  // made the UI laggy. The starfield uses tiny per-star dirty rects instead, so
  // it stays smooth as a global background.
  s_bg = lv_obj_create(scr);
  lv_obj_remove_style_all(s_bg);
  lv_obj_set_size(s_bg, display_width(), display_height());
  lv_obj_set_pos(s_bg, 0, 0);
  lv_obj_set_style_bg_color(s_bg, THEME_COL_BG, 0);
  lv_obj_set_style_bg_opa(s_bg, LV_OPA_COVER, 0);   // opaque dark safety floor
  lv_obj_clear_flag(s_bg, LV_OBJ_FLAG_CLICKABLE);   // never intercept swipes/taps
  lv_obj_clear_flag(s_bg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_bg, LV_SCROLLBAR_MODE_OFF);
  // The wavy starfield is the only animated background layer now.
  starfield_attach(s_bg);

  s_tv = lv_tileview_create(scr);
  // Tileview container: TRANSPARENT so the shared animated bg shows through, and
  // zeroed borders/pads so it never contributes a 1px white edge.
  lv_obj_set_size(s_tv, display_width(), display_height());
  lv_obj_set_style_bg_color(s_tv, THEME_COL_BG, 0);
  lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_tv, 0, 0);
  lv_obj_set_style_outline_width(s_tv, 0, 0);
  lv_obj_set_style_pad_all(s_tv, 0, 0);
  lv_obj_set_style_radius(s_tv, 0, 0);
  lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);

  // Build every tile in its LEFT-TO-RIGHT column order:
  //   Camera -> Music -> Analog(default) -> Digital -> Weather -> Calendar
  //          -> Notifications -> System.
  // Every tile is transparent, so the shared starfield background (built above,
  // behind the tileview) shows through consistently on ALL pages.
  build_camera  (make_page(PAGE_CAMERA));
  build_music   (make_page(PAGE_MUSIC));
  build_analog  (make_page(PAGE_ANALOG));
  build_digital (make_page(PAGE_DIGITAL));
  build_weather (make_page(PAGE_WEATHER));
  build_calendar(make_page(PAGE_CALENDAR));
  build_notif   (make_page(PAGE_NOTIF));
  build_menu    (make_page(PAGE_MENU));

  // Default face = the analog clock.
  lv_obj_set_tile_id(s_tv, PAGE_ANALOG, 0, LV_ANIM_OFF);

  // Kick off the shared background animation timer (continuous, behind the UI).
  starfield_start();

  // NO demo/mock data. Every page stays at its clean blank placeholder
  // ("--:--", "No data", "--\xC2\xB0", "Nothing playing", "No events", "PHONE --")
  // until REAL data arrives over BLE. This avoids ever showing a misleading
  // time/city/temperature before the phone or Mac syncs.
  ws_lock();
  // Suppress full-screen notification popups for the first few seconds after
  // boot so a backfill burst on connect doesn't spam overlays (real behaviour,
  // not demo data).
  g_ws.notif_suppress_until = millis() + 8000;
  // Force an initial render of the Notifications empty-state ("NO NOTIFICATIONS").
  g_ws.dirty |= WS_DIRTY_NOTIF;
  ws_unlock();
}

void ui_start_timers() {
  // 150 ms pump: a real notification (BLE RX -> WS_DIRTY_NOTIF) appears within
  // ~150 ms of arrival, and dismissals (notify-) clear just as fast. This is
  // trivial work (a dirty-flag check + occasional list rebuild + a ~6 s battery
  // ADC read) so it does not materially affect the thermal budget.
  lv_timer_create(ui_timer_cb, 150, NULL);
}

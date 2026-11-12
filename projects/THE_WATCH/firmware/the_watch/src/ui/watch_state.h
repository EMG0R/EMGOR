// ui/watch_state.h — the single shared data model between BLE and UI.
//
// The BLE task WRITES this (from phone messages); the UI task READS it (in the
// LVGL/loop context) and repaints. LVGL is not thread-safe, so the UI never
// touches widgets from the BLE callback — instead BLE sets fields + a "dirty"
// flag, and ui_sync() (called from loop) pushes changes into LVGL safely.
//
// All access is guarded by a FreeRTOS mutex (ws_lock/ws_unlock).
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define WS_STR   64
#define WS_BODY  160
#define WS_NOTIF_MAX 8
#define WS_PERM_QMAX 6    // max simultaneous queued permission requests

// Media/battery sources (see PROTOCOL_V2.md). A central identifies itself with a
// "hello" message; one without a hello defaults to PHONE (back-compat).
enum { WS_SRC_NONE = -1, WS_SRC_PHONE = 0, WS_SRC_MAC = 1, WS_SRC_COUNT = 2 };

enum WsDirty : uint32_t {
  WS_DIRTY_TIME     = 1 << 0,
  WS_DIRTY_WEATHER  = 1 << 1,
  WS_DIRTY_MUSIC    = 1 << 2,
  WS_DIRTY_NOTIF    = 1 << 3,
  WS_DIRTY_CONN     = 1 << 4,
  WS_DIRTY_CAL      = 1 << 5,   // next calendar event
  WS_DIRTY_PHONEBAT = 1 << 6,   // phone battery % (from BLE)
  WS_DIRTY_WATCHBAT = 1 << 7,   // watch's OWN battery % (local ADC)
  WS_DIRTY_NOTIF_POPUP = 1 << 8, // a NEW notification arrived -> show full-screen
  WS_DIRTY_CAMERA   = 1 << 9,   // camera remote state changed (from BLE)
  WS_DIRTY_SLEEP    = 1 << 10,  // audiobook sleep-timer state (from BLE)
  WS_DIRTY_MACBAT   = 1 << 11,  // mac battery % (from BLE devBattery src:"mac")
  WS_DIRTY_PERM     = 1 << 12,  // a remote permission request arrived (YES/NO card)
};

struct WsNotification {
  uint32_t id;
  char src[WS_STR];     // app / source, e.g. "Messages"
  char title[WS_STR];   // sender / title
  char body[WS_BODY];   // message text
  bool used;
};

struct WatchState {
  // --- clock (epoch seconds, local) ---
  uint32_t epoch;         // unix time, already offset to local by the phone
  int32_t  tz_offset_min; // informational
  bool     time_valid;

  // --- weather ---
  int   wx_temp_c;
  int   wx_humidity;
  char  wx_text[WS_STR];  // "Cloudy"
  int   wx_wind;
  char  wx_loc[WS_STR];   // "London"
  int   wx_aqi;           // US AQI (-1 = not available)
  bool  wx_valid;
  uint32_t wx_phone_ms;   // millis() of last PHONE weather (0 = never); mac defers to phone while fresh
  uint8_t  wx_src;        // WS_SRC_* of the currently-applied weather (debug)

  // --- music (the ACTIVE source's resolved snapshot; see PROTOCOL_V2.md) ---
  // The BLE layer keeps per-source media internally, runs the active-source
  // arbitration, and writes the winner's fields here for the UI to display.
  char  mu_artist[WS_STR];
  char  mu_album[WS_STR];
  char  mu_track[WS_STR];
  bool  mu_playing;
  bool  mu_ab;            // audiobook/podcast source active (Audible etc.)
  bool  mu_valid;
  int   mu_active_src;    // WS_SRC_* the play screen is currently following (-1 none)

  // --- audiobook sleep timer (reported by the phone, optional) ---
  bool  sleep_active;     // a sleep timer is currently counting down
  int   sleep_remaining;  // seconds remaining as last reported by the phone
  bool  sleep_valid;      // we have received at least one sleepState message

  // --- notifications (ring of recent) ---
  WsNotification notif[WS_NOTIF_MAX];
  int   notif_count;      // how many slots are populated
  int   notif_head;       // newest index
  uint32_t notif_suppress_until; // millis() until which full-screen popups are suppressed (boot/connect sync burst)

  // --- next calendar event (from phone) ---
  char     cal_title[WS_STR];
  uint32_t cal_time;      // UTC epoch seconds of event start
  char     cal_loc[WS_STR];
  bool     cal_valid;

  // --- connection / battery ---
  bool  ble_connected;
  int   battery_pct;      // WATCH's own battery % (local ADC), -1 = unknown
  int   battery_mv;       // WATCH battery millivolts (informational)

  // --- phone battery (from BLE) ---
  int   phone_batt_pct;   // -1 = unknown
  bool  phone_batt_chg;   // charging
  bool  phone_batt_valid;

  // --- mac battery (from BLE devBattery src:"mac") ---
  int   mac_batt_pct;     // -1 = unknown
  bool  mac_batt_chg;     // charging
  bool  mac_batt_valid;

  // --- remote permission request QUEUE ("the super key", see PROTOCOL_V2.md) ---
  // Centrals pop a full-screen "tap to approve" card on the watch. Multiple
  // requests (e.g. 3 parallel Claude windows at once) are QUEUED and shown one
  // at a time; each tap relays back to that request's own conn handle + id so
  // the right window unblocks. A central can also CANCEL a queued request
  // (permCancel) if it got answered elsewhere (e.g. approved on the Mac).
  // NOT gated by notif_suppress_until — permission prompts always show.
  uint8_t  perm_count;                 // number of queued requests (0..WS_PERM_QMAX)
  uint32_t perm_id[WS_PERM_QMAX];      // echoed back so the sender can correlate
  uint16_t perm_handle[WS_PERM_QMAX];  // BLE conn handle each response routes to
  char     perm_title[WS_PERM_QMAX][WS_STR];
  char     perm_body[WS_PERM_QMAX][WS_BODY];
  char     perm_origin[WS_PERM_QMAX][WS_STR];

  // --- camera remote (state reflected on the Camera page) ---
  bool  cam_open;         // camera app open on the phone
  bool  cam_recording;    // currently recording (phone is in video mode)

  volatile uint32_t dirty;
};

extern WatchState g_ws;

void ws_init();
void ws_lock();
void ws_unlock();
void ws_mark_dirty(uint32_t bits);   // must hold lock
uint32_t ws_take_dirty();            // atomically read+clear (takes lock)

// Append a notification (called under lock by BLE layer).
void ws_push_notification(uint32_t id, const char *src, const char *title, const char *body);
// Remove by id (notify- from phone).
void ws_remove_notification(uint32_t id);

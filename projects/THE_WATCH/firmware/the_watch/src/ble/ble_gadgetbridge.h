// ble/ble_gadgetbridge.h — BLE layer, isolated from the UI.
//
// Implements the Gadgetbridge "Bangle.js" transport: a Nordic UART Service
// (NUS) GATT server carrying newline-delimited JSON.
//
//   Phone -> Watch : lines wrapped as  \x10GB({...})\n  (notify/music/weather/…)
//   Watch -> Phone : raw JSON line     {...}\r\n         (music control, actions)
//   NOTE: watch->phone lines MUST end in "\r\n" — Gadgetbridge strips the byte
//   before '\n' unconditionally (it assumes Espruino's Bluetooth.println).
//
// This is the ONLY integration path that works app-lessly-ish on Android with
// a maintained open-source app (Gadgetbridge from F-Droid). The ESP32-S3 has
// no Bluetooth Classic (no A2DP/AVRCP) and Android exposes no ANCS, so a phone
// helper is unavoidable for notification/music *metadata*; Gadgetbridge is it.
//
// The UI calls ble_music_control()/ble_notify_action(); it never sees GATT.
#pragma once

#include <Arduino.h>

bool ble_init();                 // returns true if BLE advertising actually started
void ble_loop();                 // housekeeping (re-advertise etc.), call from loop()
bool ble_is_connected();

// Watch -> phone: media transport control (Gadgetbridge forwards to the phone's
// active media session). n = "play" | "pause" | "next" | "previous" for music,
// or "rewind" | "forward" (~30s skips) when an audiobook source is active.
void ble_music_control(const char *n);

// Watch -> phone: act on a notification. action = "DISMISS" (reply omitted) or
// "REPLY" with msg. Dismiss removes it from the phone's shade.
void ble_notify_action(uint32_t id, const char *action, const char *msg);

// Watch -> phone: ring the phone ("find my phone"). Gadgetbridge plays a sound.
void ble_find_phone(bool on);

// Watch -> phone: camera remote. n = "open" | "capture". The phone's camera app
// decides photo vs video; the watch is just a shutter. Sent as
// {"t":"camera","n":<n>} over the standard TX path.
void ble_camera(const char *n);

// Watch -> phone: audiobook sleep timer. minutes>0 arms a timer of that length
// (10 = "SLEEP 10 MIN"); minutes==0 CANCELS a running timer. Sent as
// {"t":"sleeptimer","n":<minutes>} over the standard TX path.
void ble_sleep_timer(int minutes);

// Watch -> requesting central: reply to a remote permission request (permReq).
// Routes {"t":"permResp","id":..,"approve":..,"scope":"once"|"always"} back to
// the specific connection handle that sent the request. `always` = the user hit
// a "yes, and don't ask again" affordance (long-press YES) -> scope:"always".
// See PROTOCOL_V2.md. Called by the UI when the YES/NO card is answered.
void ble_perm_response(uint16_t handle, uint32_t id, bool approve, const char *scope);

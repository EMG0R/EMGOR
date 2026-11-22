# The Watch — Firmware (features & architecture)

The working firmware was built on the prototype **1.69" ST7789 board** and is being
ported to the **1.43" AMOLED** (see [BOARD_AMOLED_1.43.md](BOARD_AMOLED_1.43.md)). The
port swaps `src/hal/display.cpp` (→ QSPI AMOLED), `src/hal/touch.cpp` (→ FT3168),
`src/config.h` (pins + 466×466), and LVGL resolution; all UI/BLE logic below carries over.

Stack: Arduino-ESP32, LVGL 8, NimBLE-Arduino 2.5.0, Arduino_GFX, Arduino_DriveBus.

## Layout (`firmware/the_watch/`)
- `the_watch.ino` — setup/loop; loop naps ≤16ms for touch responsiveness.
- `src/config.h` — board pins & feature config (the "one place" for hardware).
- `src/hal/` — `display.cpp`, `touch.cpp`, `imu.cpp`, `battery.cpp`.
- `src/ble/ble_gadgetbridge.cpp` — NimBLE GATT server + protocol (see PROTOCOL_V2.md).
- `src/ui/ui.cpp` — all pages/faces; `watch_state.h/.cpp` = shared model (mutex-guarded, dirty flags).
- `src/theme.cpp` — colors/fonts; `theme_add_header()` is now a **no-op** (page titles removed by design).

## Data model
`g_ws` (WatchState) guarded by `ws_lock/ws_unlock`; BLE writes fields + sets a
`WS_DIRTY_*` bit; the UI loop reads dirty bits and repaints (LVGL is single-threaded).
**No demo/mock data** — every page shows a blank placeholder (`--:--`, `No data`,
`Nothing playing`, `No events`, `PHONE --`) until real BLE data arrives.

## Multi-central BLE (phone + Mac at once) — PROTOCOL_V2.md
- Watch is a peripheral to **two centrals simultaneously** (re-advertises after the first
  connects; NimBLE default max-conn = 3). Per-connection table keyed by handle, each with
  its own `src` (from a `hello` message; defaults to phone) + MTU + RX line buffer.
- **Media arbitration:** per-source media kept separately; active source = the one playing
  (ties → most recent); play screen shows it with a MAC/PHONE badge and routes transport
  to it. `ble_music_control()` targets the active source, so callers are unchanged.
- **Weather is phone-preferred, Mac-fallback:** the watch applies phone weather always
  (stamps `wx_phone_ms`); it applies Mac weather only if the phone's is stale (>25 min) or
  absent. So the phone's (correct-location) weather wins; the Mac fills in so it's never blank.
- Per-device battery (`devBattery` src phone/mac; legacy `phoneBattery` = phone).

## Features
- **Clock faces** (analog + digital), global wavy **starfield** background.
- **Notifications:** full-screen popup for genuinely-new ones (backfill burst on connect is
  suppressed via `notif_suppress_until`), tap/flick to dismiss; app package names cleaned to
  friendly names (`clean_src`); notifications page has **no title header**.
- **Music/audiobook:** transport buttons; audiobook (`ab`) sources show rewind/forward (±30s).
- **Watch-local audiobook sleep timer:** tap = arm 10min; tap again = reset; a second tap
  within 10s = cancel; at 0:00 the watch sends `music "pause"` to the active source itself.
  The timer bar sits at the page top with an enlarged tap target.
- **Weather page:** condition above temp, `°F / °C` same size, AQI + humidity + wind at bottom.
- **Battery** (watch own via ADC, + phone/mac), **calendar** next event, **camera remote**
  (phone-side control), **find phone**.
- **Remote permission cards ("the super key"):** a central sends `permReq`; the watch shows a
  **full-screen TAP-TO-APPROVE** card (no NO button). One tap = approve with `scope:"session"`.
  Multiple simultaneous requests are **queued** (shown one at a time with a `(1/N)` counter),
  each answered back to its own connection/id. A `permCancel {id}` (e.g. approved on the Mac)
  drops that card. See COMPANION_AND_HOOK.md.

## Sound
**Removed.** The buzzer/chime/tone code and the `sound` mute command were deleted (the
AMOLED board's audio path differs). (Legacy note for the 1.69 board only: its passive
buzzer is on GPIO42 but is **silent unless SYS_EN GPIO41 is driven HIGH**; drive with
`tone()`/bit-bang, not low-duty LEDC.)

## Touch responsiveness (tuning that mattered)
- The CST816/FT touch read must **latch PRESSED** across the gaps between the controller's
  periodic interrupts (read coords every poll, hold pressed for ~60ms after last activity),
  or fast drags flicker and drop.
- Set the LVGL **indev read timer to ~10ms** (`lv_timer_set_period(indev->driver->read_timer,10)`)
  — the 30ms default was the real throttle. Lower `scroll_limit` (~2–4) + `gesture_limit` (~20).

## Flash procedure (1.69 board; AMOLED notes in BOARD doc)
- Native USB-CDC ignores esptool auto-reset → manual download mode: **hold BOOT, tap RESET,
  release BOOT.** In download mode the port re-enumerates (e.g. `usbmodem1101`, shown by
  `arduino-cli board list` as "ESP32 Family Device") — flash to *that* port.
- FQBN (1.69): `esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`.

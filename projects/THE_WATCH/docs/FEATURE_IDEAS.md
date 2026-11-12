# The Watch — Feature Ideas & Prioritized Backlog

Research-only backlog for the **Waveshare ESP32-S3-Touch-AMOLED-1.43** build
(`firmware/the_watch_amoled/`). For each idea: what it is, feasibility on THIS
board, rough effort, dependencies, and gotchas. Nothing here has been implemented;
this is a planning doc.

Cross-refs: [BOARD_AMOLED_1.43.md](BOARD_AMOLED_1.43.md) (verified hardware),
[FIRMWARE.md](FIRMWARE.md) (current features/arch), [../PROTOCOL_V2.md](../PROTOCOL_V2.md)
(wire protocol), [COMPANION_AND_HOOK.md](COMPANION_AND_HOOK.md) (apps + hook).

---

## The one thing that gates half this list: the I2C driver conflict

**Verified on hardware 2026-07-27.** The vendor display/touch bring-up
(`Touch_Init()` + `lcd_lvgl_Init()`) installs the **LEGACY ESP-IDF I2C driver**
on port 0 (SDA=47/SCL=48). Arduino `Wire`/`Wire1` is the NEW driver model
("driver_ng"), and ESP-IDF **aborts into a reboot loop** if both are used at once
(`CONFLICT! driver_ng is not allowed to be used with this old driver`).

Consequence: the **QMI8658 IMU (0x6B)** and **PCF85063 RTC (0x51)** are on the
bus but are **currently DISABLED** in this build (`the_watch_amoled.ino` brings
them up as a no-op). SensorLib (`SensorQMI8658`, `SensorPCF85063`) uses Arduino
Wire, so **it cannot be linked as-is**.

**To use the IMU or RTC you must talk to them through the legacy driver** —
`i2c_master_write_read_device(I2C_NUM_0, addr, ...)` / the same path `FT3168.cpp`
already uses — NOT Arduino Wire, and NOT SensorLib unmodified. Budget a one-time
**"legacy-I2C sensor shim"** task (small register read/write helpers on port 0)
as a prerequisite for every IMU/RTC feature below. Once that shim exists, the
per-feature IMU/RTC cost drops a lot. This shim is the highest-leverage single
piece of work on the list.

Effort legend: **S** = hours, **M** = 1–2 days, **L** = 3+ days, plus the shim
where noted.

---

## Tier 1 — Do next (high value, low effort, no new hardware risk)

### 1. Raise-to-wake / wrist-tilt to wake + auto-dim  ★
- **What:** Tilt the wrist up → screen brightens to full; drop the wrist / after a
  timeout → dim to a low level (or off). The signature smartwatch gesture.
- **Feasibility:** High. Needs the QMI8658 accelerometer. Gesture is cheap: read
  accel Z/X, detect the "face-up + rising" transition. Brightness is already a
  single command (`0x51` / setBrightness, 0..255) — no backlight pin. AMOLED dims
  cleanly and instantly.
- **Effort:** M **+ the legacy-I2C shim** (Tier-0 prereq). Gesture tuning is the
  bulk of it.
- **Depends on:** IMU via legacy I2C (shim). Firmware only — no companion change.
- **Gotchas:** Accel-only wake can false-trigger (pocket, typing). QMI8658 has a
  hardware "wake-on-motion"/tap interrupt, but the **INT line isn't wired to a
  free GPIO in our config** (touch INT is -1; verify no IMU INT pad is broken
  out) — so plan on **polling** the accel from the loop/a task at ~20–50 Hz,
  which raises idle draw. Poll rate is the power/latency trade. Pair with #2.

### 2. Auto-dim + display sleep on inactivity  ★
- **What:** After N seconds with no touch (and, optionally, no wrist activity),
  step brightness down, then blank the panel (cmd `0x28` display-off / sleep-in).
  Wake on touch or on raise-to-wake (#1).
- **Feasibility:** High, and **doesn't need the IMU** if you wake on touch only —
  making this the true lowest-risk item. Touch already latches PRESSED; reset an
  idle timer on any touch. Brightness ramp + panel off/on are single commands.
- **Effort:** S–M. Pure firmware.
- **Depends on:** Nothing new (touch-only wake). IMU optional for raise-to-wake.
- **Gotchas:** Confirm the SH8601/CO5300 resume sequence (display-on `0x29` +
  possibly re-assert brightness) restores cleanly — the vendor init already sends
  `0x29`/`0x51`, so reuse that path. Keep LVGL's pump task running or stop its
  tick while blanked to save more.

### 3. Persistent time via the RTC (survive reboot & phone-less operation)  ★
- **What:** Today `g_ws.epoch` is a **volatile snapshot from the phone**
  (`setTime`), lost on every reboot and never advanced by hardware. Back it with
  the PCF85063: on `setTime` write the RTC; on boot read the RTC so the clock is
  correct **before/without** a phone. Also feed the ESP32 system clock
  (`settimeofday`) so alarms/timers have a real timebase.
- **Feasibility:** High. PCF85063 is trivial (BCD time registers).
- **Effort:** S **+ the legacy-I2C shim**.
- **Depends on:** RTC via legacy I2C (shim).
- **Gotchas:** PCF85063 needs its own coin/backup rail to keep time across a full
  power-down — **verify whether this board wires one**; if not, the RTC only helps
  across warm reboots, not battery-dead. Timezone/DST still comes from the phone
  (`E.setTimeZone`), keep storing the offset.

### 4. Alarms / timers / stopwatch  ★
- **What:** On-watch stopwatch, countdown timer, and daily alarms. Fills an obvious
  smartwatch gap; the sleep-timer UI pattern already exists to crib from.
- **Feasibility:** High for stopwatch/timer (pure software over `millis()` / system
  time). Alarms want the RTC (#3) so they survive reboot and fire without the phone.
- **Effort:** M (UI is the bulk; three small pages + persistence).
- **Depends on:** RTC (#3) for robust alarms; none for stopwatch/timer.
- **Gotchas:** **No buzzer/haptic on this board** — an alarm can only *flash the
  screen* (full-bright pulse + big visual) unless a phone-side chime is used.
  Consider a `alarm`/`buzzPhone` message so the *phone* rings for you (see #6).
  Persist alarm defs to NVS/flash.

### 5. Find-my-phone / ring the phone  ★
- **What:** A button that makes the phone play a loud sound even on silent.
- **Feasibility:** Already ~90% there — firmware has `findPhone` and a "find phone"
  UI entry. Needs the **Android app** to actually ring on receipt (max-volume tone,
  ignore ringer mode). Mac equivalent = play a sound.
- **Effort:** S firmware (verify/extend the existing message) **+ S companion**.
- **Depends on:** Companion-app change (the phone side does the ringing).
- **Gotchas:** Route to the right source (phone preferred; Mac fallback per
  PROTOCOL_V2). Android needs the notification/DND override to override silent.

### 6. "Ring my phone for me" as the missing alarm actuator
- **What:** Because the watch has no buzzer, let watch alarms/timers (#4) trigger a
  **phone-side** sound via a new `buzzPhone`/`alarmFire` message.
- **Feasibility:** High; reuses the findPhone plumbing.
- **Effort:** S firmware **+ S companion**.
- **Depends on:** Companion change; only works when the phone is connected.
- **Gotchas:** Degrades to on-watch screen flash when disconnected.

---

## Tier 2 — High value, moderate effort

### 7. Step tracking & activity (pedometer)
- **What:** Daily step count, a ring/goal, maybe active-minutes. (A `FEATURE_STEPS.md`
  is referenced in planning but **does not yet exist in the repo** — create it as
  the detailed design when this is picked up.)
- **Feasibility:** Medium. QMI8658 has an **on-chip pedometer engine** (hardware
  step counter with configurable thresholds) — using it avoids running a DSP filter
  on-CPU. Otherwise, a classic peak-detection filter on accel magnitude at ~25–50 Hz.
- **Effort:** M **+ the legacy-I2C shim** (hardware pedometer) or **L** (software
  algorithm + tuning).
- **Depends on:** IMU via legacy I2C (shim). Optional companion sync to store
  history/graphs on the phone.
- **Gotchas:** Continuous accel sampling raises idle power — decide sample rate vs
  battery. Config-register programming for the QMI8658 pedometer is fiddly; the
  SensorLib source is a good reference to port to legacy-I2C calls. Persist the
  daily count across sleep; reset at local midnight (needs RTC/#3).

### 8. Always-on display (AOD) / low-power face
- **What:** A stripped, mostly-black face (time + a couple glyphs) shown at low
  brightness when idle, instead of fully blanking (#2). AMOLED's per-pixel emission
  makes this genuinely low-power if most pixels are black.
- **Feasibility:** Medium. AMOLED is the *right* panel for this. Draw a minimal
  layout, drop brightness (`0x51` low), refresh only once a minute.
- **Effort:** M. Firmware only.
- **Depends on:** Ideally RTC (#3) so the AOD clock is right without the phone; pairs
  with #1/#2.
- **Gotchas:** **Burn-in** — shift the layout a few px periodically (pixel-shift),
  keep it dim, keep it sparse. Real power savings only come if you also **stop the
  LVGL full-refresh cadence** and idle the CPU between minute ticks; a naive AOD
  that keeps rendering at full rate saves little. True deep/light-sleep is harder
  here because the vendor LVGL pump task + NimBLE run continuously (see #16).

### 9. WiFi: NTP time without the phone
- **What:** On boot (or on demand) connect to a saved WiFi AP, `configTime()` via
  NTP, set the clock. Makes the watch self-sufficient for time.
- **Feasibility:** Medium. ESP32-S3 WiFi is available (currently kept OFF for
  power/thermal). NTP is a few lines.
- **Effort:** M (WiFi lifecycle + credential storage + a settings/provisioning UI).
- **Depends on:** WiFi on; NVS for credentials; ideally RTC (#3) to hold the result.
- **Gotchas:** WiFi is a big power/heat draw — bring it up **briefly then drop it**,
  don't leave it associated. Credential entry on a 466px round touch screen is
  painful → provision via a BLE message from the companion app instead of on-watch
  typing. Coexistence of WiFi + BLE (NimBLE) shares the radio — test for hitches.

### 10. WiFi: direct weather without the phone
- **What:** Fetch Open-Meteo directly over WiFi (the companion already uses
  Open-Meteo), so weather works with no phone/Mac present.
- **Feasibility:** Medium. Same HTTP/JSON the apps do.
- **Effort:** M (HTTP client + JSON parse + reuse existing weather state fields).
- **Depends on:** WiFi (#9); a location (no GPS/geo on-watch → hard-set a lat/lon or
  use IP-geo like the Mac app does).
- **Gotchas:** No on-watch location source — needs a configured lat/lon or IP-geo.
  Keep the phone-preferred arbitration (PROTOCOL_V2) and treat direct-WiFi as
  another fallback tier below Mac. Same brief-connect power discipline as #9.

### 11. Watchface picker + custom faces
- **What:** A gallery to switch faces from the watch (analog/digital already exist);
  later, side-load custom faces/images.
- **Feasibility:** High for a picker of built-in faces (LVGL UI + persisted choice).
  Custom/side-loaded faces = Medium–Large (asset format, storage in the 9MB FAT
  partition, a loader).
- **Effort:** M (picker) / L (custom-face pipeline + companion uploader).
- **Depends on:** None for the picker. Custom faces need a companion "push image/face"
  message and flash storage (there's a FAT partition per the FQBN).
- **Gotchas:** 466×466 round assets are large in PSRAM/flash; define a constrained
  format. Keep the starfield background option. Persist selection to NVS.

### 12. Do-not-disturb (DND) / quiet hours
- **What:** Suppress the full-screen notification popups (still log them) during a
  schedule or on a manual toggle. Permission cards can still be exempt.
- **Feasibility:** High. The notification pipeline already has a suppression window
  (`notif_suppress_until`) to build on.
- **Effort:** S–M. Firmware only (optionally surface as a settings toggle).
- **Depends on:** RTC/time for scheduled quiet hours (have it already via `setTime`).
- **Gotchas:** Decide whether permission ("super key") cards override DND — they
  currently bypass suppression by design; keep that.

### 13. Quick-reply to notifications
- **What:** From a notification, tap a canned reply ("OK", "On my way", 👍) sent back
  to the phone to dispatch.
- **Feasibility:** Medium. Protocol/firmware **already send `notify` with
  `n:"REPLY"`** (watch→central) — the plumbing exists. Needs (a) a reply-picker UI on
  the watch and (b) the Android app to actually inject the reply via the
  notification's RemoteInput action.
- **Effort:** M firmware UI **+ M companion** (RemoteInput is the finicky part).
- **Depends on:** Companion (Android) change; per-app RemoteInput support.
- **Gotchas:** No keyboard → canned replies + emoji only (free-text on a round
  466px screen isn't worth it). Only apps exposing a reply action work.

### 14. Media album art
- **What:** Show cover art on the now-playing screen.
- **Feasibility:** Medium. Firmware can render an image; the constraint is getting
  the bytes over BLE.
- **Effort:** M firmware (decode + cache) **+ M companion** (downscale + chunk over
  the NUS characteristic).
- **Depends on:** Companion sends art; needs a chunked-transfer sub-protocol (BLE
  MTU is small; art must be resized to ~thumbnail and sent in frames).
- **Gotchas:** Bandwidth — send a small (e.g. 96–128px) JPEG/RGB565 thumbnail, not
  full art. Add a `musicart` message + reassembly. PSRAM is ample (8MB) for the
  buffer.

---

## Tier 3 — Nice-to-have / conditional

### 15. Battery health & charge state
- **What:** Show charging status, a charge history, maybe a health estimate.
- **Feasibility:** Limited. Battery is **ADC-only** (GPIO4, VBAT/3 divider); the
  build has **no PMIC (no AXP2101) and no confirmed charge/VBUS status line**.
  `battery.cpp` derives % from a voltage curve and has no charge detection.
- **Effort:** S for a "charging?" heuristic (voltage rising / pegged high while USB
  in) up to M if a status GPIO turns out to exist.
- **Depends on:** Verify on hardware whether any charge-status/VBUS pin is broken
  out. If not, this stays a rough voltage-based readout.
- **Gotchas:** Voltage-based % is coarse and noisy under load; no true SoC gauge.
  "Health" (capacity fade) is not reliably measurable without a fuel gauge — don't
  over-promise it.

### 16. True low-power sleep (light/deep sleep)
- **What:** Beyond blanking the screen (#2), actually drop CPU into light sleep
  between events to extend battery meaningfully.
- **Feasibility:** Hard on this architecture. The vendor **LVGL pump runs as a
  free-running FreeRTOS task** and **NimBLE runs its own task**; naive light-sleep
  fights both. Real gains need: stop/slow the LVGL tick when blanked, let BLE modem
  sleep, and gate wake on touch/IMU INT.
- **Effort:** L. Touches the vendor bring-up.
- **Depends on:** #2/#8; an IMU or touch INT wake source (INT lines may not be
  wired — see #1).
- **Gotchas:** Heat/idle draw on this board is dominated by the **always-on USB
  regulator** (per config.h notes), so sleep gains are smaller *while on USB* than
  on battery. Highest complexity-to-reward ratio on the list — do the cheap wins
  (#2, #8) first and measure before attempting this.

### 17. Orientation / level / activity gestures (IMU, NO compass)
- **What:** A bubble-level / tilt indicator, screen-rotation, double-tap-to-wake,
  shake-to-dismiss — cheap uses of the accel/gyro.
- **Feasibility:** Medium. All doable with the 6-axis IMU.
- **Effort:** S–M each **+ the legacy-I2C shim**.
- **Depends on:** IMU via legacy I2C (shim).
- **Gotchas:** **No magnetometer** — the QMI8658 is accel+gyro only, so a true
  heading **compass is not possible**. A gyro-integrated relative heading drifts
  within seconds and needs a known reference; do not sell it as a compass. Tilt/
  level/gesture uses are fine.

### 18. OTA firmware update
- **What:** Update firmware over WiFi (or over BLE) instead of the manual
  BOOT/RESET flash dance.
- **Feasibility:** Medium over WiFi (ESP32 ArduinoOTA / HTTP OTA); slow but possible
  over BLE.
- **Effort:** L. Partition/rollback handling + a delivery path + a safe-fallback.
- **Depends on:** WiFi (#9) or a BLE DFU sub-protocol; the FQBN's partition scheme
  is `app3M_fat9M_16MB` — **only ~3MB app, no second OTA app slot**, so a proper
  A/B OTA needs a **partition-table change** (and thus one more manual reflash to
  adopt it).
- **Gotchas:** Get rollback right or a bad OTA bricks the field-flash workflow.
  BLE OTA is very slow at NUS MTUs. Given the manual flash is well-documented and
  the device is developer-owned, this is low priority vs. the UX features.

### 19. Voice / AI on-watch
- **What:** Voice notes, dictation, on-wrist Claude.
- **Feasibility:** **Not feasible on-watch** — the board has **no microphone and no
  speaker** (verify against the schematic, but none is broken out and audio was
  removed from firmware). The only realistic path is **phone-mediated**: watch
  triggers a voice capture *on the phone*.
- **Effort:** N/A on-watch; M+ as a phone-side feature.
- **Depends on:** Entirely on the companion app (phone mic + phone-side AI).
- **Gotchas:** Don't attempt on-watch audio. The existing "super key" permission-card
  channel is already the good on-wrist AI touchpoint — extend *that* rather than
  adding audio.

### 20. Haptics
- **What:** Vibration for notifications/alarms.
- **Feasibility:** **Not feasible — no haptic motor on this board.** GPIO42 (the old
  1.69 board's buzzer pin) is **LCD_PWREN here — never drive it as a buzzer**
  (`WATCH_BUZZER_PIN = -1`). Sound/buzzer code was already removed from this build.
- **Effort:** N/A (would require add-on hardware).
- **Depends on:** External motor + driver wired to a free GPIO (hardware mod).
- **Gotchas:** Until/unless hardware is added, the alert actuator is the **screen
  flash** (#4) or a **phone-side ring** (#6). Flag as aspirational/hardware-blocked.

---

## Suggested sequencing

1. **Legacy-I2C sensor shim** (Tier-0 prereq) — unlocks IMU + RTC for everything below.
2. **#2 auto-dim/sleep** (no IMU needed) → immediate battery/UX win.
3. **#3 RTC-backed time** → correct clock without the phone; foundation for alarms.
4. **#1 raise-to-wake** → the signature gesture (needs shim).
5. **#5/#6 find-phone + phone-ring alarm actuator** → cheap, high perceived value.
6. **#4 alarms/timers/stopwatch**, **#12 DND**, **#11 face picker** → round out the
   "feels like a real watch" set.
7. Then the WiFi tier (#9/#10), **#7 steps**, **#8 AOD**, **#13 quick-reply**,
   **#14 art** as larger efforts.
8. Aspirational / blocked: **#16 deep sleep**, **#18 OTA**, **#19 voice**, **#20
   haptics**.

## Hardware reality-check (things this board CANNOT do without add-ons)
- **No compass** (no magnetometer; 6-axis IMU only).
- **No haptics, no buzzer, no speaker, no microphone.**
- **No true fuel-gauge / possibly no charge-status line** (ADC voltage only).
- **RTC backup rail unconfirmed** — persistence across full power-down may not hold.
- **IMU/RTC INT lines** likely not wired to free GPIOs → sensors are **polled**,
  which bounds how low idle power can go.
- **Single ~3MB app partition** (no A/B slot) → clean OTA needs a partition change.

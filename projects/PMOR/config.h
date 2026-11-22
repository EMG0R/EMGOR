// config.h — PMOR v3: every constant in one place.
#pragma once
#include <stdint.h>

#include "pmor_core/core_config.h"   // audio/sequencer/engine/fx/output/patch constants live with the core

// ---------------------------------------------------------------- pins (locked, see WIRING-v2.md)
#define PIN_ENC_A       0
#define PIN_ENC_B       1
#define PIN_ENC_SW      2
#define PIN_OLED_DC     3
#define PIN_OLED_RST    4
#define PIN_OLED_CS     7
#define PIN_OLED_SCK    8
#define PIN_OLED_MOSI   10
#define PIN_TBUS_SCL    11
#define PIN_TBUS_SDA    12
#define PIN_SBUS_SCL    13
#define PIN_SBUS_SDA    14
#define PIN_FSR         15   // adc ch0
#define PIN_JOY_X       16   // adc ch1
#define PIN_JOY_Y       17   // adc ch2
#define PIN_JOY_SW      24
#define PIN_SD_MISO     25
#define PIN_SD_CLK      26
#define PIN_SD_MOSI     27
#define PIN_SD_CS       28

// ---------------------------------------------------------------- calibration
#define FSR_IDLE_RAW    1600      // 12-bit-equivalent idle; FsrScale auto-learns from here
#define JOY_MAX_RAW     950       // usable deflection from centre (raw 0..1023 scale); edge catch beyond is fine
#define JOY_DEADZONE    0.06f     // normalised
// Joystick orientation. Logical axes everywhere in the firmware: +X = RIGHT, +Y = UP.
// Measured 2026-09-14: the module is mounted rotated — the wiring's "X" channel (adc1)
// is the physical vertical axis and "Y" (adc2) is horizontal. Flip a flag if a direction
// is backwards; nothing else in the code needs to change.
#define JOY_SWAP_XY     1         // 1: physical horizontal = adc2, physical vertical = adc1
#define JOY_INVERT_X    1         // logical +X must be physical RIGHT   (flipped 2026-09-14 after test 2)
#define ACCEL_SWAP_XY   0         // NO swap (EMGOR 2026-09-15): env macro on IMU X was right; filter on IMU Y, inverted below
#define ACCEL_INVERT_Y  1         // tilt direction for the synth PERF filter (accel Y): flipped 2026-09-15 per EMGOR (upright = open)
#define ACCEL_INVERT_X  1         // tilt direction for every accel-X mapping (env macro shorter/longer, LFO speeds): flipped 2026-09-14 per EMGOR
#define JOY_INVERT_Y    1         // logical +Y must be physical UP      (flipped 2026-09-14 after test 2)

// ---------------------------------------------------------------- nav feel
#define NAV_REPEAT_DELAY_MS  350
#define NAV_REPEAT_HZ        8
#define EDIT_STICK_FULL      0.55f   // stick deflection that already means "max" in the editors (the last 20 % is hard to hold)
#define DIVE_MS              540     // menu-switch tunnel/name duration
#define NAV_HOLD_DELETE_MS   5000
#define NAV_DOUBLE_WINDOW_MS 40      // both buttons within this = volume shortcut
// (2026-10-05) TAP_MAX_N / TAP_EXPIRE_MS are superseded: the tap window, expiry, bounce gate and fit
// now live in the shared analyzer (DEMIURGE_OS/src/demiurge-transport.h: DT_TAP_*). Kept only so nothing
// that still names them breaks; nothing in the firmware reads them.
#define TAP_MAX_N            5
#define TAP_EXPIRE_MS        2000
#define SAVER_IDLE_MS        25000
#define AUTOSAVE_DEBOUNCE_MS 2000

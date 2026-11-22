// hw.h — façade over the verbatim hardware ports. CONTROL plane unless stated.
// Implementations are lifted line-for-line from PMOR_V2 (see research/v2-audit.md §2).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "daisy_seed.h"

extern daisy::DaisySeed hw;

// ---- USB service pump (the one symbol every blocking driver needs)
extern "C" void pmor_usb_service(void);
void usbDelay(uint32_t ms);          // Delay that keeps USB alive

// ---- boot (order locked, see audit §2.12). Returns after audio is started and the trellis is up.
typedef void (*AudioCb)(float** in, float** out, size_t n);
void hwBoot(AudioCb cb);

// ---- trellis
bool trellisOk();
void trellisTick(uint32_t now_ms);                          // recovery/watchdog + LED push (80 ms cadence, frame-diff)
void trellisSetFrame(const uint8_t grb[48]);                // next frame to push
typedef void (*PadCb)(uint8_t pad, bool pressed);
void trellisPollKeys(uint32_t now_ms, PadCb cb);           // state machine, 2 ms cadence, call every pass (and between loop sections); applies rotation remap
void trellisSetRotation(uint8_t quarter_turns);            // 0..3, single remap table for LEDs and keys

// ---- OLED
namespace daisy { template<typename D> class OledDisplay; }
void oledInit();
void oledFlushBegin();                                     // frame drawn: mark the pieces that differ from the panel
bool oledFlushStep();                                      // send ONE piece (~0.35 ms); true = more waiting (main loop: once per pass)
bool oledFlushPending();
void oledFlush();                                          // display.Update() (dirty pages, pumps USB)
void oledSetContrast(uint8_t level);                       // GLOBAL setting; 1 = locked-safe minimum, clamps 1..32
// drawing primitives used by ui/render.cpp
void oledClear();
void oledPixel(int x, int y, bool on);
void oledLine(int x0, int y0, int x1, int y1, bool on);
void oledRect(int x0, int y0, int x1, int y1, bool on, bool fill);
void oledRoundRect(int x0, int y0, int x1, int y1, int r, bool on, bool fill);
void oledText(int x, int y, const char* s, bool on);       // Font_6x8
void oledTextBig(int x, int y, const char* s, bool on);    // Font_7x10

// ---- sensors
void  sensorsTick(uint32_t now_ms);                        // IMU 50 ms + watchdog, lidar, FSR median → SharedState g
bool  imuOk();
bool  lidarOk();
uint16_t lidarMm();
uint8_t  lidarStatus();

// ---- encoder + joystick (joystick ADC is polled from the AUDIO plane at 750 Hz)
int   encoderDelta();                                      // detents since last call (±)
bool  encoderPressed();                                    // raw level
bool  joyPressed();
void  joyPollFromIsr(uint32_t block_start_sample);         // AUDIO plane; writes g.joy_*; stamps encoder presses
// Encoder-press timestamps (2026-10-05). The click is SAMPLED in the audio ISR (750 Hz) and its
// absolute sample instant is queued here, so a tap is timed by the audio clock -- not by whenever
// the main loop got round to noticing it (OLED page blits, SD writes and I2C transactions stall
// the loop for up to tens of ms, and that used to land in the tempo and the grid phase as error).
// Pops the OLDEST queued press no older than `max_age_samples` before `now_sample`; presses older
// than that are stale (the main loop never turned them into an event) and are discarded.
// Returns false if nothing usable is queued (caller falls back to `now_sample`).
bool  encoderTakePressSample(uint32_t now_sample, uint32_t max_age_samples, uint32_t* out_sample);

// ---- SD
bool  sdOk();
void  sdSetFast(bool fast);   // drop to the slow SPI clock after a failed write verify
bool  sdIsFast();
extern char sd_path[];                                     // FatFS logical drive path

// ---- DFU
void  dfuCheck();                                          // CDC "dfu" line → bootloader

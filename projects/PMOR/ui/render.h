// render.h — OLED and trellis rendering (CONTROL plane). Draws from nav + engine state.
#pragma once
#include <stdint.h>
#include "nav.h"

// ---- OLED (128×64 landscape)
void oledRenderFrame(uint32_t now_ms);
void oledIntro();                          // boot tunnel with PMOR (called once from main after hwBoot)     // dispatch by nav.level / nav.menu; ends with saving-star + particles
// widgets (used by pages)
void wCards(const char* const* names, int n, int sel);                       // three rounded cards L→R
void wList(const ParamPage* page, int cursor, int top);                     // scrolling param list
void wSlider(const Param& p, float value);                                  // full-screen 1-D editor
void wXY(const Param& px, const Param& py, float vx, float vy);             // full-screen 2-D editor
void wTitle(const char* s);                                                 // top row title
void wSavingStar(bool busy, bool dirty, bool show);                         // spinner/solid VERBATIM from V2 L5446-5465; hollow (dirty) added TASK 3
void wParticles(float amp, uint32_t now_ms, bool saver);                    // outline particles / screensaver

// ---- Trellis
struct Rgb { uint8_t r, g, b; };
void trellisRenderFrame(uint32_t now_ms, uint8_t grb_out[48]);   // lava base + page overlay + brightness + rotation
// TASK 9 (2026-10): two variables, not one, so the perceptual gamma curve below can't
// compound itself through the QSPI persistence round-trip (store/patch.cpp's
// globalsSyncTick copies live state back into glob.* whenever it drifts from what's
// stored — if that compare used the gamma'd value, every boot would gamma it again).
//   g_trellis_brightness_ctrl — linear 0..1, the raw GLOBALS "BRIGHT" knob position.
//     This is the one persisted to QSPI (glob.trellis_brightness) and the one the GLOBALS
//     editor shows/edits. Defined in ui/render_trellis.cpp.
//   g_trellis_brightness      — gamma-corrected 0..1, written ONLY as trellisBrightnessGamma
//     (ctrl) by nav.cpp (live edit) and store/patch.cpp (boot apply). This is the single
//     value hw/trellis.cpp's trellisSetFrame() multiplies every pad by — the one and only
//     multiply in the whole codebase; nothing in ui/ multiplies by brightness a second time.
//     Defined in hw/trellis.cpp (not owned here).
extern float g_trellis_brightness;
extern float g_trellis_brightness_ctrl;
float trellisBrightnessGamma(float linear01);                     // shared curve, defined in ui/render_trellis.cpp

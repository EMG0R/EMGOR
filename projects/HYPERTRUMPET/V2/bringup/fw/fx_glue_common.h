// Shared bits for the per-effect glue files (fx_glue_reverb.h, fx_glue_drone.h, ...). One glue file per effect holds that effect's arena, defaults, sensor mapping,
// CPU gate and telemetry; main.cpp and lights.h only call into fx_glue_chain.h. Nothing under HYPER_CORE is edited.
#pragma once
#define HX_TARGET_SEED 1
#define HX_STFT_MAXN 4096          // drone FRAME up to 4096
#define HX_REVERB_LITE 1           // budget: 4-line reverb (see docs/FX_BUDGET.md)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"          // inside HYPER_CORE headers (read only here)
#ifdef HX_MIN_DRONE
#include "../../../HYPER_CORE/src/dsp/fx/fx_drone.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_chorus.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_reverb.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_grain.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_tape.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_freeze.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_looper.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_trace.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_wah.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_ringmod.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_modblock.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_harmony.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_glitch.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_blooper.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_moodii.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_dronemaker.h"
#else
#include "../../../HYPER_CORE/src/dsp/fx/fx_reverb.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_drone.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_harmonizer.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_looper.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_grain.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_delay.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_flanger.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_chorus.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_mood.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_freeze.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_blur.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_roar.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_ringmod.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_wah.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_autopan.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_harmony.h"
#include "../../../HYPER_CORE/src/dsp/fx/fx_pitchtrack.h"
#endif
#pragma GCC diagnostic pop
#include "stm32h7xx_hal.h"
enum { FXS_IDLE = 0, FXS_ACTIVE = 1, FXS_TAIL = 2 };
// Registry entry: the generic param mechanism (fx_glue_params.h) works only from the Fx descriptors (names, ranges, flags) plus these defaults and lock labels.
// mixUser: for pad-driven effects param 0 (MIX) is the user's wet level while the effect is on; the gate drives fx->v[0] itself, so the UI / persistence read and write mixUser instead.
struct FxGlue { const char* id; hx::Fx* fx; bool ok; float def[HX_MAXP]; const char* lock[HX_MAXP]; float* mixUser = nullptr; };
// Registry order: [0] reverb, [1] drone (its glue file hard-codes slot 1), [2..13] the generic template effects. The CHAIN order is a separate table (fx_glue_chain.h).
enum { GI_REVERB = 0, GI_DRONE = 1, GI_HARM, GI_LOOPER, GI_FREEZE, GI_BLUR, GI_FLANGE, GI_CHORUS, GI_MOOD, GI_GRAIN, GI_DELAY, GI_ROAR, GI_RING, GI_WAH, GI_AUTOPAN, GI_HARMONY, GLUE_N };
static FxGlue g_glue[GLUE_N];
static inline float glueGet(const FxGlue& g, int i) { return (i == 0 && g.mixUser) ? *g.mixUser : g.fx->v[i]; }
static void glueCaptureDefaults(FxGlue& g) { for(int i = 0; i < HX_MAXP; i++) { g.lock[i] = nullptr; g.def[i] = (g.fx && i < g.fx->np) ? g.fx->v[i] : 0.0f; } }
// The CPU gate shared by every effect: IDLE runs nothing; waking is the caller's job (clear + fade in); TAIL runs with a fast mix smoother until the wet gain is below 1e-4 (-80 dB) or 50 ms passed.
struct FxGate { volatile int state = FXS_IDLE; int tail = 0; float smA = 0.3f; };
static inline bool gateEnter(FxGate& g, hx::Fx* fx, float mixT) {      // returns true when the effect has to be woken (IDLE -> ACTIVE) this block
    int st = g.state; bool wake = false;
    if(st == FXS_IDLE) { if(mixT > 0.0005f) { st = FXS_ACTIVE; wake = true; } }
    else if(st == FXS_ACTIVE) { if(mixT <= 0.0005f) { st = FXS_TAIL; g.tail = 0; fx->smA = 0.5f; } }
    else if(st == FXS_TAIL) { if(mixT > 0.0005f) { st = FXS_ACTIVE; fx->smA = g.smA; } }
    g.state = st; return wake;
}
static inline void gateLeave(FxGate& g, hx::Fx* fx, size_t n) {          // after process(): finish a tail
    if(g.state != FXS_TAIL) return;
    g.tail++;
    if(fx->sm[0] < 1e-4f || (float)g.tail * (float)n * (1000.0f / 48000.0f) >= 50.0f) { g.state = FXS_IDLE; fx->smA = g.smA; }   // param 0 is always MIX (fx_common.h contract)
}

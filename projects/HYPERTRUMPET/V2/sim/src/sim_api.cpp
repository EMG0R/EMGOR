// sim_api.cpp -- flat extern "C" API over hx::FxEngine + hx::HxMapper + the synthetic trumpet, for WebAssembly
// (AudioWorklet) and for the node verify script. Everything audio-related is the SAME C++ the firmware compiles
// (HYPER_CORE/src/dsp/fx/**, mapping/fx_mapping.cpp); only this file and trumpet_synth.cpp are sim-only.
//
// THREADING: the module is single-threaded. On hardware FxEngine::setEffect must never run in the audio callback.
// In WASM it is called from the worklet's message handler BETWEEN process() calls, so it can never overlap audio.
//
// CONTROL PACK (sim_set_controls, float[SIM_NCTL]):
//  0..2 valve  3..5 fader  6..7 pot  8 fsr  9 joyX 10 joyY 11 joyClick 12..13 encDelta(accumulated, cleared by the
//  worklet after sim_set_controls consumed it) 14..15 encClick 16 tiltX 17 tiltY 18 shake 19 lidar 20 pads(bitmask)
#include <stdint.h>
#include <string.h>
#include <math.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define API extern "C" EMSCRIPTEN_KEEPALIVE
extern "C" double sim_js_now_ms(void) __attribute__((import_module("env"), import_name("now_ms")));
static uint64_t jsNowNs() { return (uint64_t)(sim_js_now_ms() * 1.0e6); }
#else
#define API extern "C"
static uint64_t jsNowNs() { return 0; }
#endif
#include "../../../HYPER_CORE/src/dsp/fx/mapping/fx_mapping.h"
#include "trumpet_synth.h"

#define SIM_NCTL 21
#define SIM_BLOCK 64          // firmware block size exercised by the sim
#define SIM_CTL_EVERY 4       // mapper update every 4 blocks = 256 samples = 187.5 Hz of audio time

alignas(16) static uint8_t g_arena[HX_NSLOTS * HX_SLOT_ARENA_BYTES];
static hx::FxEngine g_eng;
static hx::HxMapper g_map;
static sim::TrumpetSynth g_tr;
static hx::HxControls g_ctl;
static float g_sr = 48000.f;
static float g_trumpet[4] = {4, 0, 5.2f, 0.15f};
static float g_encAcc[2] = {0, 0};
static bool  g_inited = false;
static bool  g_mapperOn = true;   // test hook: verify.mjs can freeze the mapper to test effects in isolation
static int   g_blockCount = 0;
static float g_synthNs = 0.f, g_f0hint = 0.f;
static float g_inScratch[HX_MAXBLOCK];

API int sim_init(float sr) {
    g_sr = sr;
    hx::g_nowNs = jsNowNs;
    g_eng.init(sr, g_arena, sizeof(g_arena));
    g_map.init(&g_eng);
    g_tr.init(sr);
    g_ctl = hx::HxControls();
    g_inited = true; g_blockCount = 0;
    return 1;
}
API void sim_set_mapper_enabled(int on) { g_mapperOn = on != 0; }
API int sim_arena_bytes() { return (int)sizeof(g_arena); }
API int sim_block() { return SIM_BLOCK; }

// ---- audio ------------------------------------------------------------------------------------------------
// inPtr: mono float input (inputMode 1 = external audio: file or mic). inputMode 0: synthetic trumpet (inPtr ignored).
// n <= 128; split in SIM_BLOCK blocks. Outputs L/R float arrays of n.
API void sim_process(const float* inPtr, float* outL, float* outR, int n, int inputMode) {
    if (!g_inited) { memset(outL, 0, n * 4); memset(outR, 0, n * 4); return; }
    for (int off = 0; off < n; off += SIM_BLOCK) {
        int m = n - off < SIM_BLOCK ? n - off : SIM_BLOCK;
        const float* in;
        if (inputMode == 0 || !inPtr) {
            uint64_t t0 = jsNowNs();
            g_tr.valve[0] = g_ctl.valve[0]; g_tr.valve[1] = g_ctl.valve[1]; g_tr.valve[2] = g_ctl.valve[2];
            g_tr.partial = (int)(g_trumpet[0] + 0.5f); g_tr.breath = g_trumpet[1];
            g_tr.vibRate = g_trumpet[2]; g_tr.vibDepth = g_trumpet[3];
            g_tr.process(g_inScratch, m);
            g_f0hint = g_tr.f0();
            g_eng.setValveHint(g_f0hint);
            if (t0) g_synthNs += ((float)(jsNowNs() - t0) - g_synthNs) * 0.05f;
            in = g_inScratch;
        } else {
            g_f0hint = 0.f; g_eng.setValveHint(0.f);
            in = inPtr + off;
        }
        if (g_mapperOn && (g_blockCount % SIM_CTL_EVERY) == 0) {
            g_ctl.encDelta[0] = (int)g_encAcc[0]; g_ctl.encDelta[1] = (int)g_encAcc[1];
            g_encAcc[0] = g_encAcc[1] = 0;
            g_map.update(g_ctl, (float)(SIM_BLOCK * SIM_CTL_EVERY) / g_sr);
            g_ctl.encDelta[0] = g_ctl.encDelta[1] = 0;
        }
        g_blockCount++;
        g_eng.process(in, outL + off, outR + off, m);
    }
}

// ---- controls --------------------------------------------------------------------------------------------
API void sim_set_controls(const float* c) {
    for (int i = 0; i < 3; i++) { g_ctl.valve[i] = c[i]; g_ctl.fader[i] = c[3 + i]; }
    g_ctl.pot[0] = c[6]; g_ctl.pot[1] = c[7]; g_ctl.fsr = c[8];
    g_ctl.joyX = c[9]; g_ctl.joyY = c[10]; g_ctl.joyClick = c[11] > 0.5f;
    g_encAcc[0] += c[12]; g_encAcc[1] += c[13];       // deltas accumulate until the next mapper update
    g_ctl.encClick[0] = c[14] > 0.5f; g_ctl.encClick[1] = c[15] > 0.5f;
    g_ctl.tiltX = c[16]; g_ctl.tiltY = c[17]; g_ctl.shake = c[18]; g_ctl.lidar = c[19];
    g_ctl.pads = (uint16_t)(uint32_t)c[20];
}
API void sim_set_trumpet(float partial, float breath, float vibRate, float vibDepth) {
    g_trumpet[0] = partial; g_trumpet[1] = breath; g_trumpet[2] = vibRate; g_trumpet[3] = vibDepth;
}
API void sim_tongue() { g_tr.tongue(); }
API const uint8_t* sim_pad_leds() { return &g_map.leds().rgb[0][0]; }   // 16*3 bytes
API const char* sim_status_line() { return g_map.statusLine(); }
API int sim_mapper_page() { return g_map.page(); }
API int sim_selected_slot() { return g_map.selectedSlot(); }
API int sim_selected_param() { return g_map.selectedParam(); }
API void sim_select_slot(int s) { g_map.selectSlot(s); }
API void sim_load_default_patch() { g_map.loadDefaultPatch(); }

// ---- slot / effect introspection ------------------------------------------------------------------------
API int sim_effect_count() { return hx::fxTypeCount(); }
API const char* sim_effect_id(int i)   { const hx::FxTypeInfo* t = hx::fxType(i); return t ? t->id : ""; }
API const char* sim_effect_name(int i) { const hx::FxTypeInfo* t = hx::fxType(i); return t ? t->name : ""; }
API const char* sim_effect_v1(int i)   { const hx::FxTypeInfo* t = hx::fxType(i); return t ? t->v1 : ""; }
API int sim_slot_count() { return HX_NSLOTS; }
API int sim_slot_effect(int s) { return g_eng.effectType(s); }
// control-thread operation: call between process() calls only. Returns 1 if the effect built with enough arena memory.
API int sim_set_slot_effect(int s, int id) { return g_eng.setEffect(s, id) ? 1 : 0; }
API int sim_slot_ok(int s) { hx::Fx* f = g_eng.fx(s); return f ? (f->ok ? 1 : 0) : 0; }
API const char* sim_slot_name(int s) { hx::Fx* f = g_eng.fx(s); return f ? f->name : ""; }
API int sim_param_count(int s) { hx::Fx* f = g_eng.fx(s); return f ? f->np : 0; }
static const Param* P_(int s, int p) { hx::Fx* f = g_eng.fx(s); return (f && p >= 0 && p < f->np) ? &f->pt[p] : nullptr; }
API const char* sim_param_name(int s, int p) { const Param* q = P_(s, p); return q ? q->name : ""; }
API float sim_param_min(int s, int p)  { const Param* q = P_(s, p); return q ? q->min : 0.f; }
API float sim_param_max(int s, int p)  { const Param* q = P_(s, p); return q ? q->max : 1.f; }
API float sim_param_step(int s, int p) { const Param* q = P_(s, p); return q ? q->step : 0.f; }
API int   sim_param_flags(int s, int p){ const Param* q = P_(s, p); return q ? q->flags : 0; }
API float sim_param_value(int s, int p){ hx::Fx* f = g_eng.fx(s); return f ? f->getParam(p) : 0.f; }
API const char* sim_param_enum_name(int s, int p, int k) {
    const Param* q = P_(s, p); if (!q || !q->names) return "";
    if (k < 0 || k > (int)(q->max - q->min + 0.5f)) return "";
    return q->names[k] ? q->names[k] : "";
}
API int sim_param_role(int s, int p) {   // macro role driving this param (0..5) or -1
    hx::Fx* f = g_eng.fx(s); if (!f) return -1;
    for (int r = 0; r < hx::R_COUNT; r++) if (f->role[r] == p) return r;
    return -1;
}
API void sim_set_param(int s, int p, float v) { g_eng.setParam(s, p, v); }
API void sim_set_bypass(int s, int b) { g_eng.setBypass(s, b != 0); }
API int  sim_get_bypass(int s) { return g_eng.bypassed(s) ? 1 : 0; }
API void sim_action(int s, int id, float v) { g_eng.action(s, id, v); }

// ---- meters ---------------------------------------------------------------------------------------------
// out[0]=inPeak out[1]=outPeak (peak since last read) out[2]=engine ns/block (EMA) out[3]=f0 tracked out[4]=env
// out[5..8]=slot ns/block (EMA) out[9]=synth ns/block out[10]=f0 hint from valves
API void sim_read_meters(float* o) {
    o[0] = g_eng.readInPeak(); o[1] = g_eng.readOutPeak(); o[2] = g_eng.totalNs();
    o[3] = g_eng.f0(); o[4] = g_eng.env();
    for (int s = 0; s < HX_NSLOTS; s++) o[5 + s] = g_eng.slotNs(s);
    o[9] = g_synthNs; o[10] = g_f0hint;
}
// scratch buffers the JS side can use (in 128, outL 128, outR 128, ctl SIM_NCTL, meters 16)
static float g_io[128 * 3 + 64 + 16];
API float* sim_io() { return g_io; }
API int sim_ctl_count() { return SIM_NCTL; }

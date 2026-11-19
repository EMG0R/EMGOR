// TEST 12 (drone only). The ONLY DSP in this build is the drone (fx_drone.h + fx_fft.h and what they include): no template glue, no registry, no other effect header.
// Path: in (mic L) -> drBlock (dry + drone, additive send, gated: zero CPU at fader 0) -> mono sum (audioCb) -> output gain -> limiter -> out.
#pragma once
#define HX_TARGET_SEED 1
#define HX_STFT_MAXN 4096
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "../../../HYPER_CORE/src/dsp/fx/fx_drone.h"
#pragma GCC diagnostic pop
#include "stm32h7xx_hal.h"
enum { FXS_IDLE = 0, FXS_ACTIVE = 1, FXS_TAIL = 2 };
// CPU gate: IDLE runs nothing; ACTIVE; TAIL = fast fade to -80 dB or 50 ms, then IDLE.
struct FxGate { volatile int state = FXS_IDLE; int tail = 0; float smA = 0.3f; };
static inline bool gateEnter(FxGate& g, hx::Fx* fx, float mixT) {
    int st = g.state; bool wake = false;
    if(st == FXS_IDLE) { if(mixT > 0.0005f) { st = FXS_ACTIVE; wake = true; } }
    else if(st == FXS_ACTIVE) { if(mixT <= 0.0005f) { st = FXS_TAIL; g.tail = 0; fx->smA = 0.5f; } }
    else if(st == FXS_TAIL) { if(mixT > 0.0005f) { st = FXS_ACTIVE; fx->smA = g.smA; } }
    g.state = st; return wake;
}
static inline void gateLeave(FxGate& g, hx::Fx* fx, size_t n) {
    if(g.state != FXS_TAIL) return; g.tail++;
    if(fx->sm[0] < 1e-4f || (float)g.tail * (float)n * (1000.0f / 48000.0f) >= 50.0f) { g.state = FXS_IDLE; fx->smA = g.smA; }
}
// the drone's arrays: AXI SRAM (.axi_bss, cached, never zeroed by the startup code: the constructor zeroes what it allocates)
static uint8_t g_dr_mem[62 * 1024] __attribute__((section(".axi_bss"), aligned(32)));
static uint8_t g_dr_cold_mem[28 * 1024] __attribute__((section(".axi_bss"), aligned(32)));
static hx::Arena g_dr_cold, g_dr_arena; static hx::FxDrone* g_dr = nullptr; static bool g_dr_ok = false; static FxGate g_drg; static float g_dr_def[HX_MAXP];
static volatile float g_dr_mix_req = 0.0f, g_dr_wet = 0.0f, g_dr_pk = 0.0f, g_dr_cyc = 0.0f; static float g_dr_cps = 10000.0f;
// ---- inputs / state shared with main.cpp and lights.h (same names as the full rig)
static volatile float g_simFader[3] = {-1, -1, -1};          // `sim fader <1..3> <0..1000>` (-1 = hardware)
static volatile bool g_mute = false; static volatile float g_noiseAmp = 0.0f, g_toneHz = 440.0f, g_toneAmp = 0.0f;   // `sim noise <dbfs>`, `sim tone <hz> <dbfs>`
static volatile float g_olMin = 1e9f, g_olSum = 0.0f, g_olLvl = 0.0f; static volatile uint32_t g_olN = 0, g_olDrops = 0;   // output level before the mute: min / sum / count per frame, dropouts
static volatile uint32_t g_xrunOver = 0, g_xrunGap = 0, g_cbMax = 0; static volatile float g_cb_cpu = 0.0f, g_cb_cpu_pk = 0.0f, g_cpu_slowpk = 0.0f;
static volatile uint32_t g_secMax[9] = {0}, g_secWin[9] = {0};
#define SEC_T0(v) uint32_t v = DWT->CYCCNT
#define SEC_T1(i, v) { uint32_t d_ = DWT->CYCCNT - v; if(d_ > g_secMax[i]) g_secMax[i] = d_; if(d_ > g_secWin[i]) g_secWin[i] = d_; }
static float g_cyc_per_sample = 8000.0f; static volatile uint32_t g_ffault = 0;          // g_ffault: the fader ground-fault guard is holding every fader mix at 0 (fx_controls_d12.h)
static void drInit() {
    g_dr_arena.bind(g_dr_mem, sizeof(g_dr_mem)); g_dr_cold.bind(g_dr_cold_mem, sizeof(g_dr_cold_mem)); g_dr_arena.cold = &g_dr_cold;
    g_dr = static_cast<hx::FxDrone*>(hx::fx_create_drone(g_dr_arena, 48000.0f)); g_dr_ok = g_dr && g_dr->ok; if(!g_dr_ok) return;
    g_dr_cps = (float)HAL_RCC_GetSysClockFreq() / 48000.0f; g_dr->init(48000.0f, 48.0f); g_drg.smA = g_dr->smA;
    g_dr->setParam(0, 0.0f); g_dr->Fx::snap();                      // MIX is the fader's; every other param keeps the effect's own musical defaults
    for(int i = 0; i < HX_MAXP; i++) g_dr_def[i] = i < g_dr->np ? g_dr->v[i] : 0.0f; g_dr_def[0] = 1.0f;
}
static void fxGlueInit() {
    drInit();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_cyc_per_sample = (float)HAL_RCC_GetSysClockFreq() / 48000.0f;
}
static inline void drControls(float mix) { g_dr_mix_req = mix; }
static void drWake(float mixT) {
    g_dr->wake();                                                    // light reset: no big memsets in the audio thread
    g_dr->v[0] = mixT; g_dr->sm[0] = 0.0f; g_dr->mixCur = 0.0f; g_dr->primed = true; g_dr->smA = g_drg.smA;     // fade in from 0
}
// audio callback, never blocks: mono in -> stereo out (dry + drone). Zero work while the gate is IDLE.
static void fxChain(const float* in, float* oL, float* oR, size_t n) {
    for(size_t i = 0; i < n; i++) { oL[i] = in[i]; oR[i] = in[i]; }
    if(!g_dr_ok) return;
    const float mixT = g_dr_mix_req;
    if(g_drg.state == FXS_IDLE && mixT <= 0.0005f) { g_dr_wet = 0.0f; g_dr_cyc *= 0.98f; return; }
    const uint32_t c0 = DWT->CYCCNT;
    if(gateEnter(g_drg, g_dr, mixT)) drWake(mixT);
    if(g_drg.state == FXS_IDLE) { g_dr_wet = 0.0f; return; }
    g_dr->v[0] = (g_drg.state == FXS_TAIL) ? 0.0f : mixT;
    g_dr->smoothBlock(); hx::FxCtx ctx = {}; ctx.sr = 48000.0f;
    g_dr->process(ctx, oL, oR, (int)n, false); g_dr_wet = g_dr->sm[0]; gateLeave(g_drg, g_dr, n);
    float bad = 0.0f; for(size_t i = 0; i < n; i++) bad += oL[i] - oL[i] + oR[i] - oR[i];
    if(bad != 0.0f) { memcpy(oL, in, n * sizeof(float)); memcpy(oR, in, n * sizeof(float)); g_drg.state = FXS_IDLE; g_dr->reset(); }   // non-finite: dry, drone reset
    const float l = (float)(DWT->CYCCNT - c0) / ((float)n * g_dr_cps); g_dr_cyc += 0.02f * (l - g_dr_cyc); if(l > g_dr_pk) g_dr_pk = l;
}
static inline void fxCpu(float load) {
    g_cb_cpu = g_cb_cpu + 0.05f * (load - g_cb_cpu); if(load > g_cb_cpu_pk) g_cb_cpu_pk = load;
    g_cpu_slowpk = load > g_cpu_slowpk ? load : g_cpu_slowpk * 0.99995f;
}
static inline void fxClearPoll() {}
static inline void fxShedPoll(uint32_t) {}
// ---- output stage: soft limiter at the very end (ceiling -1 dBFS, look-ahead-free; g_target = (1 + r^4)^-0.25, instant attack, 100 ms release) + output meter
static volatile float g_moPk = 0.0f, g_moSq = 0.0f, g_moGr = 1.0f; static volatile uint32_t g_moN = 0;
static inline void limiterSample(float& L, float& R) {
    static float g = 1.0f; static const float ceil_ = 0.89125094f, rel = 1.0f - expf(-1.0f / (0.100f * 48000.0f));
    const float a = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R), r = a * (1.0f / ceil_), r2 = r * r;
    const float t = 1.0f / sqrtf(sqrtf(1.0f + r2 * r2)); if(t < g) g = t; else g += rel * (t - g);
    L *= g; R *= g; const float o = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R); if(o > g_moPk) g_moPk = o; if(g < g_moGr) g_moGr = g;
    g_moSq = g_moSq + L * L + R * R; g_moN = g_moN + 2;
}
static void moTelemetry() {
    auto db10 = [](float x) -> int32_t { return x > 1e-6f ? (int32_t)(200.0f * log10f(x)) : -1200; };
    const float pk = g_moPk, rms = g_moN ? sqrtf(g_moSq / (float)g_moN) : 0.0f, gr = g_moGr;
    puts_(",\"mo\":["); putd(db10(pk)); put(','); putd(db10(rms)); put(','); putd(-db10(gr)); put(']'); g_moPk = 0.0f; g_moSq = 0.0f; g_moN = 0; g_moGr = 1.0f;
}
// ---- output capture (test tool): `cap` records 8192 samples of the final output and dumps them as `cap` lines (host FFT / THD analysis)
static int16_t g_cap[8192]; static volatile int g_capN = 0; static volatile bool g_capArm = false, g_capDone = false; static int g_capSent = 0;
static inline void capSample(float x) { if(g_capArm) { g_cap[g_capN] = (int16_t)(x > 1.0f ? 32767 : (x < -1.0f ? -32767 : x * 32767.0f)); if(++g_capN >= 8192) { g_capArm = false; g_capDone = true; g_capSent = 0; } } }
// ---- telemetry (heavy frame): cpu, st{xrun,gap,cb}, ol[], dr{}, mo[]
static void fxGlueTelemetry() {
    float pk = g_cb_cpu_pk; g_cb_cpu_pk = 0;
    puts_(",\"cpu\":{\"a\":"); putu((uint32_t)(g_cb_cpu * 1000.0f)); puts_(",\"p\":"); putu((uint32_t)(pk * 1000.0f)); puts_(",\"s\":"); putu((uint32_t)(g_cpu_slowpk * 1000.0f));
    puts_("},\"st\":{\"xrun\":"); putu(g_xrunOver); puts_(",\"gap\":"); putu(g_xrunGap); puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_(",\"cb\":"); putu(g_cbMax); puts_(",\"b2a\":"); putu(g_b2a); puts_(",\"usbre\":"); putu(g_usbre); puts_(",\"txd\":"); putu(g_txDrop); puts_(",\"ffault\":"); putu(g_ffault); puts_("},\"ol\":[");
    { auto db10 = [](float x) -> int32_t { return x > 1e-6f ? (int32_t)(200.0f * log10f(x)) : -1200; }; const float mn = g_olMin, av = g_olN ? g_olSum / (float)g_olN : 0.0f;
      putd(db10(mn > 1e8f ? 0.0f : mn)); put(','); putd(db10(av)); put(','); putu(g_olDrops); put(','); putd(db10(g_olLvl)); g_olMin = 1e9f; g_olSum = 0.0f; g_olN = 0; }
    puts_("],\"dr\":{\"on\":"); putu(g_drg.state != FXS_IDLE ? 1 : 0); puts_(",\"st\":"); putu((uint32_t)g_drg.state); puts_(",\"mix\":"); putu((uint32_t)(g_dr_mix_req * 1000.0f));
    puts_(",\"wet\":"); putu((uint32_t)(g_dr_wet * 1000.0f)); puts_(",\"cyc\":"); putu((uint32_t)(g_dr_cyc * 1000.0f)); puts_(",\"pk\":"); putu((uint32_t)(g_dr_pk * 1000.0f)); g_dr_pk = 0.0f; puts_("}");
}
static void fxGlueHello() { puts_(",\"dri\":{\"ok\":"); put(g_dr_ok ? '1' : '0'); puts_("}"); }

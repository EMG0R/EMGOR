// The chain and everything the rest of the firmware calls (main.cpp, lights.h, fx_pads.h): init, per-block processing, controls, CPU numbers, refuse rule, telemetry.
// Chain order = the user's v1 patch (V1_SCRAPE 4 / FX_MAP 1): harmonizer, looper, freeze, drone, blur, flanger, chorus, MOOD, reverb, grain, delay, roar, ringmod; WAH (new in v2) last.
// Registry index (GI_*) and chain position are different things: the drone glue file hard-codes registry slot 1.
#pragma once
#include "fx_glue_reverb.h"
#include "fx_glue_drone.h"
#include "fx_glue_harm.h"
#include "fx_glue_looper.h"
#include "fx_glue_axslot.h"
#include "fx_glue_flange.h"
#include "fx_glue_chorus.h"
#include "fx_glue_mood.h"
#include "fx_glue_grain.h"
#include "fx_glue_delay.h"
#include "fx_glue_roar.h"
#include "fx_glue_ring.h"
#include "fx_glue_wah.h"
#include "fx_glue_autopan.h"
#include "fx_glue_harmony.h"
static const uint8_t kChain[GLUE_N] = { GI_HARM, GI_HARMONY, GI_LOOPER, GI_FREEZE, GI_DRONE, GI_BLUR, GI_FLANGE, GI_AUTOPAN, GI_CHORUS, GI_MOOD, GI_REVERB, GI_GRAIN, GI_DELAY, GI_ROAR, GI_RING, GI_WAH };
// ---- THE fader -> effect table (the only place). Current user setting: fader 1 reverb, fader 2 drone, fader 3 unassigned.
//      v1 (V1_MAP_AUDIT.md): slider 1 drone, slider 2 grain, slider 3 reverb  ->  { GI_DRONE, GI_GRAIN, GI_REVERB }
static const int8_t kFaderFx[3] = { GI_REVERB, GI_DRONE, GI_GRAIN };   // user 2026-11-17: fader 1 Supermassive, 2 drone, 3 granular dry/wet
// Admission cost of turning an effect on = max(its peak measured at run time, this device-measured peak table). Limit 70 % projected total peak (5 % margin under the 75 % design limit).
static const float kAdmitPeak[GLUE_N] = { 0.121f, 0.086f, 0.024f, 0.025f, 0.100f, 0.063f, 0.032f, 0.043f, 0.053f, 0.107f, 0.065f, 0.035f, 0.019f, 0.011f, 0.011f, 0.150f };   // registry order: reverb drone harm looper freeze blur flange chorus mood grain delay roar ring wah autopan harmony (harmony 15 % until 11_bench measures it)
static const float FX_PEAK_LIMIT = 0.65f;                    // refuse to turn the next effect on when the projected total peak would exceed this
static const float DRONE_EST = 0.12f;      // drone: 12 % avg at the reverb-derived factor (docs/FX_BUDGET.md)
static volatile float g_cb_cpu = 0.0f, g_cb_cpu_pk = 0.0f, g_cpu_slowpk = 0.0f;
// ---- output stage (HX_OUTSTAGE): soft limiter at the very end of the audio path (ceiling -1 dBFS, look-ahead-free; g_target = (1 + r^4)^-0.25, instant attack, 100 ms release)
static inline void limiterSample(float& L, float& R) {
    static float g = 1.0f; static const float ceil_ = 0.89125094f, rel = 1.0f - expf(-1.0f / (0.100f * 48000.0f));
    const float a = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R), r = a * (1.0f / ceil_), r2 = r * r;
    const float t = 1.0f / sqrtf(sqrtf(1.0f + r2 * r2)); if(t < g) g = t; else g += rel * (t - g); L *= g; R *= g;
}
static volatile float g_cpu4[4] = {0, 0, 0, 0}, g_cpu4pk[4] = {0, 0, 0, 0};   // 0 idle, 1 reverb only, 2 drone only, 3 both (other effects count as idle in this split)
// ---- simulation inputs (sim command): override the hardware until `sim off`; they feed the SAME mapping code (fader_cal.h, fx_controls.h, fx_pads.h)
static volatile bool g_simPadOn = false; static volatile uint16_t g_simKeys = 0;
static volatile float g_simFader[3] = {-1, -1, -1};        // 0..1000 or -1 = hardware
static volatile float g_simPot[2] = {-1, -1}, g_simFsr = -1, g_simLid = -1;   // 0..1000 (pot, fsr), mm (lidar); -1 = hardware
static volatile bool g_simAccel = false, g_simJoy = false, g_simEnc = false; static volatile float g_simAy = 0.0f; static volatile int g_simJx = 0, g_simJy = 0;
static volatile bool g_simJbtn = false; static volatile uint32_t g_simJpress = 0, g_simEncPress = 0; static volatile int g_simEncDet = 0;   // joystick / encoder presses and detents are ADDED to the hardware counts
static volatile bool g_mute = false;
static volatile float g_noiseAmp = 0.0f;                              // `sim noise <dbfs>`
static volatile float g_toneHz = 440.0f, g_toneAmp = 0.0f;           // `sim tone <hz> <dbfs>`: test tone instead of the mic at the start of the chain (0 = off)
static volatile float g_olMin = 1e9f, g_olSum = 0.0f, g_olLvl = 0.0f; static volatile uint32_t g_olN = 0, g_olDrops = 0;   // output level before the mute: min / sum / count since the last telemetry frame, dropout counter, running level
// ---- exclusivity (reverb / grain / drone, one at a time, last fader moved wins) and HARMONY page state; written by fx_controls.h / fx_pads.h
static volatile int g_exOwner = -1; static volatile uint8_t g_exBlocked = 0;     // owner = registry index of the active one of the three (-1 none); blocked: bit k = the fader k's effect was displaced and its fader has not been at 0 since
static volatile bool g_harmMode = false; static volatile int g_hvPad[3] = {-1, -1, -1}; static volatile uint32_t g_hvAge[3] = {0, 0, 0}; static uint32_t g_hvClock = 0;   // harmony page: pad index per voice slot, selection age
static const int8_t kHarmSemi[16] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 17, 18, 20, 21 };   // pad 1 top-left: 2 b3 3 4 / b5 5 #5 6 / b7 7 b9 9 / 11 #11 b13 13, semitones above the input
static volatile uint32_t g_simEnc2Press = 0;                                  // `mute on`: hard output mute, the chain keeps running
static volatile uint32_t g_xrunOver = 0, g_xrunGap = 0, g_cbMax = 0, g_nan[GLUE_N] = {0};   // callbacks longer than the block period, callbacks that started late (missed deadline), longest callback in cycles, non-finite blocks per effect
// ---- per-section main-loop timers (cycles): 0 joy+enc, 1 lightsPoll (strip + trellis + pads), 2 trellisPoll alone, 3 controls (faders, sensors, clear, shed), 4 imu/lidar/i2c, 5 hello dump, 6 sendFrame, 7 ogPoll (param commands, QSPI save, pd/state), 8 whole pass
static volatile uint32_t g_secMax[9] = {0}, g_secWin[9] = {0};
#define SEC_T0(v) uint32_t v = DWT->CYCCNT
#define SEC_T1(i, v) { uint32_t d_ = DWT->CYCCNT - v; if(d_ > g_secMax[i]) g_secMax[i] = d_; if(d_ > g_secWin[i]) g_secWin[i] = d_; }
static volatile uint32_t g_shed = 0; static volatile int g_shedLast = -1; static volatile bool g_shedReq = false; static uint32_t g_loadHist = 0;   // runtime guard: sheds so far, the last one shed (registry index), request from the audio thread, bit history of "callback > 75 %"
static volatile int g_blk_class = 0, g_focus = -1;
static volatile float g_cv[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // live control values for the UI: pot1, fsr, lidar mix, joy X, joy Y (0..1), freeze clear ramp, harmonizer interval / 13, joystick mode / 2
static int g_dmode = 0; static bool g_dsmooth = false;       // delay pads: 0 off, 1 pad 9 plain on, 2 pad 11 preset A, 3 pad 12 preset B (FSR drives the time); pad 10 smooth mode
static volatile uint32_t g_frzClearMs = 0;                   // pad 3: start time of the 2.3 s freeze-factor ramp (0 = idle)
static float g_cyc_per_sample = 8000.0f;
static hx::PitchTrack g_pt;
static void fxGlueInit() {
    rvGlueInit(); drGlueInit(GI_DRONE);
    fxgInit(GI_HARM, FXD_harm); fxgInit(GI_LOOPER, FXD_looper); fxgInit(GI_FREEZE, FXD_freeze); fxgInit(GI_BLUR, FXD_blur); fxgInit(GI_FLANGE, FXD_flange);
    fxgInit(GI_CHORUS, FXD_chorus); fxgInit(GI_MOOD, FXD_mood); fxgInit(GI_GRAIN, FXD_grain); fxgInit(GI_DELAY, FXD_delay); fxgInit(GI_ROAR, FXD_roar);
    fxgInit(GI_RING, FXD_ring); fxgInit(GI_WAH, FXD_wah); fxgInit(GI_AUTOPAN, FXD_autopan); fxgInit(GI_HARMONY, FXD_harmony);
    {   // sensor-owned params: locked in the UI and out of the persisted record. Labels from the fader table + the v1 control map.
        static const char* fname[3] = { "fader 1", "fader 2", "fader 3" };
        for(int k = 0; k < 3; k++) { int gi = kFaderFx[k]; if(gi >= 0 && gi != GI_DRONE && g_glue[gi].ok) g_glue[gi].lock[0] = fname[k]; }
        auto lk = [](int gi, const char* par, const char* lab) { FxGlue& g = g_glue[gi]; if(!g.ok) return; int i = par ? g.fx->findParam(par) : 0; if(i >= 0) g.lock[i] = lab; };
        lk(GI_FLANGE, "MIX", "lidar"); lk(GI_FREEZE, "FREEZE", "pad 3"); lk(GI_ROAR, "DRIVE", "pot 1"); lk(GI_DELAY, "TIME", "FSR");
        lk(GI_MOOD, "PITCH", "joystick X"); lk(GI_MOOD, "MOOD", "joystick Y"); lk(GI_HARMONY, nullptr, "encoder 1"); lk(GI_AUTOPAN, "DEPTH", "lidar"); lk(GI_AUTOPAN, "PERIOD", "lidar");
    }
    g_pt.init(48000.0f); g_ctx.sr = 48000.0f;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_cyc_per_sample = (float)HAL_RCC_GetSysClockFreq() / 48000.0f;
}
static inline bool fxIsOn(int gi) { return gi == GI_DRONE ? g_drg.state != FXS_IDLE : g_fxg[gi].gate.state != FXS_IDLE; }
static inline bool fxIsTail(int gi) { return gi == GI_DRONE ? g_drg.state == FXS_TAIL : g_fxg[gi].gate.state == FXS_TAIL; }
static inline float fxWet(int gi) { return gi == GI_DRONE ? g_dr_wet : (g_fxg[gi].fx ? g_fxg[gi].fx->sm[0] : 0.0f); }
// May effect gi be turned on? Projected total = the slow total peak now + this effect's own peak (measured when known, else the estimate).
static bool fxAllowOn(int gi) {
    FxG& g = g_fxg[gi]; if(gi != GI_DRONE && !g.ok) return false;
    if(g.shedUntil > (uint32_t)daisy::System::GetNow()) return false;                 // shed by the runtime guard a moment ago
    if(axAcquire(gi)) return false;                                                    // freeze / blur share one AXI slot: the other one is on
    float est = kAdmitPeak[gi]; if(g.pk > est) est = g.pk;      // admission: the device-measured peak table, or the effect's own peak measured at run time if that is higher (never a host estimate)
    return g_cpu_slowpk + est <= FX_PEAK_LIMIT;
}
static inline bool blockFinite(const float* L, const float* R, size_t n) { float bad = 0.0f; for(size_t i = 0; i < n; i++) { bad += L[i] - L[i]; if(R) bad += R[i] - R[i]; } return bad == 0.0f; }   // NaN or Inf anywhere makes the sum NaN
static float g_envY = 0.0f; static volatile float g_chainLoad = 0.0f, g_ovh = 0.0f;   // g_chainLoad: whole fxChain() as a fraction of the callback; g_ovh: what is left after the effects themselves (gate checks, copies, envelope)
// audio callback: never blocks. mono in -> stereo out.
static void fxChain(const float* in, float* oL, float* oR, size_t n) {
    static float mm[128], tL[128], tR[128]; const uint32_t c0 = DWT->CYCCNT; uint32_t stageCyc = 0;
    memcpy(oL, in, n * sizeof(float)); bool st = false;
    { float e = 0; for(size_t i = 0; i < n; i++) e += in[i] * in[i]; float rms = sqrtf(e / (float)n); g_envY += (rms - g_envY) * (rms > g_envY ? 0.5f : 0.08f); g_ctx.env = g_envY * 4.0f > 1.0f ? 1.0f : g_envY * 4.0f; }
    if(fxIsOn(GI_HARM) || fxIsOn(GI_HARMONY) || fxIsOn(GI_MOOD) || fxIsOn(GI_RING)) { g_pt.process(in, (int)n); g_ctx.f0 = g_pt.f0; g_ctx.conf = g_pt.conf; } else { g_ctx.f0 = 0.0f; g_ctx.conf = 0.0f; }   // shared pitch tracker only while a pitch-following effect is on
    bool rvRan = false, drRan = false;
    for(int k = 0; k < GLUE_N; k++) {
        const int gi = kChain[k]; FxG& g = g_fxg[gi]; const uint32_t a = DWT->CYCCNT; bool ran = false;
        if(gi == GI_DRONE) {
            if(g_drg.state != FXS_IDLE || g_dr_mix_req > 0.0005f) {                      // the drone glue is called only while it is wanted or fading out
                const float* src = oL; if(st) { for(size_t i = 0; i < n; i++) mm[i] = 0.5f * (oL[i] + oR[i]); src = mm; }
                ran = drGlueBlock(src, tL, tR, n);
                if(ran) { if(st) { for(size_t i = 0; i < n; i++) { oL[i] += tL[i] - mm[i]; oR[i] += tR[i] - mm[i]; } } else { memcpy(oL, tL, n * sizeof(float)); memcpy(oR, tR, n * sizeof(float)); } st = true; }
            }
            drRan = ran;
        } else { ran = fxgRun(g, oL, oR, st, n); if(gi == GI_REVERB) rvRan = ran; }
        if(ran && !blockFinite(oL, st ? oR : nullptr, n)) { memset(oL, 0, n * sizeof(float)); memset(oR, 0, n * sizeof(float)); g_nan[gi] = g_nan[gi] + 1; g.needReset = true; st = true; }   // zero the block, count it, the main loop resets the effect
        if(ran) { const uint32_t d = DWT->CYCCNT - a; stageCyc += d; if(d > g.maxcyc) g.maxcyc = d; float l = (float)d / ((float)n * g_cyc_per_sample); g.cyc += 0.05f * (l - g.cyc); g.avg += 0.002f * (l - g.avg); g.pk = l > g.pk ? l : g.pk * 0.9995f; } else g.cyc = 0.0f;
    }
    if(!st) memcpy(oR, oL, n * sizeof(float));
    g_blk_class = (drRan ? 2 : 0) + (rvRan ? 1 : 0);
    { const uint32_t tot = DWT->CYCCNT - c0; const float inv = 1.0f / ((float)n * g_cyc_per_sample); g_chainLoad += 0.02f * ((float)tot * inv - g_chainLoad); g_ovh += 0.02f * ((float)(tot - stageCyc) * inv - g_ovh); }
}
static inline void fxCpu(float load) {
    g_cb_cpu = g_cb_cpu + 0.05f * (load - g_cb_cpu); if(load > g_cb_cpu_pk) g_cb_cpu_pk = load;
    g_cpu_slowpk = load > g_cpu_slowpk ? load : g_cpu_slowpk * 0.99995f;                // ~20 s decay at 1 kHz callbacks: conservative, a recent peak keeps counting for a long time
    g_loadHist = (g_loadHist << 1) | (load > 0.75f ? 1u : 0u);                         // runtime guard: 8 of the last 32 blocks over 75 %, or one block over 90 %
    if(load > 0.90f || __builtin_popcount(g_loadHist) >= 8) g_shedReq = true;
    int c = g_blk_class; g_cpu4[c] = g_cpu4[c] + 0.05f * (load - g_cpu4[c]); if(load > g_cpu4pk[c]) g_cpu4pk[c] = load;
}
static void fxGlueTelemetry() {
    float pk = g_cb_cpu_pk; g_cb_cpu_pk = 0;
    puts_(",\"fx\":[");
    for(int gi = 0; gi < GLUE_N; gi++) {
        FxG& g = g_fxg[gi]; if(gi) put(',');
        put('['); put(fxIsOn(gi) ? (fxIsTail(gi) ? '2' : '1') : '0'); put(','); putu((uint32_t)(g.cyc * 1000.0f)); put(','); putu((uint32_t)(g.pk * 1000.0f)); put(','); putu((uint32_t)(fxWet(gi) * 1000.0f));
        put(','); putu((g.want() ? 1u : 0u) | (g.refuseMs > (uint32_t)daisy::System::GetNow() ? 2u : 0u)); put(']');
    }
    puts_("],\"cpu\":{\"a\":"); putu((uint32_t)(g_cb_cpu * 1000.0f)); puts_(",\"p\":"); putu((uint32_t)(pk * 1000.0f)); puts_(",\"s\":"); putu((uint32_t)(g_cpu_slowpk * 1000.0f)); puts_(",\"c4\":[");
    for(int i = 0; i < 4; i++) { if(i) put(','); putu((uint32_t)(g_cpu4[i] * 1000.0f)); } puts_("],\"p4\":[");
    for(int i = 0; i < 4; i++) { if(i) put(','); putu((uint32_t)(g_cpu4pk[i] * 1000.0f)); g_cpu4pk[i] = 0; } puts_("]},\"ov\":["); putu((uint32_t)(g_ovh * 1000.0f)); put(','); putu((uint32_t)(g_chainLoad * 1000.0f)); puts_("],\"cv\":["); for(int i = 0; i < 8; i++) { if(i) put(','); putu((uint32_t)(g_cv[i] * 1000.0f)); } puts_("],\"in\":["); putu((g_simPadOn ? 1u : 0u) | (g_simFader[0] >= 0 ? 2u : 0u) | (g_simFader[1] >= 0 ? 4u : 0u) | (g_simFader[2] >= 0 ? 8u : 0u) | (g_simPot[0] >= 0 ? 16u : 0u) | (g_simPot[1] >= 0 ? 32u : 0u) | (g_simFsr >= 0 ? 64u : 0u) | (g_simLid >= 0 ? 128u : 0u) | (g_simAccel ? 256u : 0u) | (g_simJoy ? 512u : 0u) | (g_simEnc ? 1024u : 0u));   // bit set = that input is simulated (pad, fader 1-3, pot 1, pot 2, fsr, lidar, accel, joystick, encoder)
    puts_("],\"st\":{\"b2a\":"); putu(g_b2a); puts_(",\"usbre\":"); putu(g_usbre); puts_(",\"txd\":"); putu(g_txDrop); puts_(",\"xrun\":"); putu(g_xrunOver); puts_(",\"gap\":"); putu(g_xrunGap); puts_(",\"rec\":"); putu(g_recov); puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_(",\"shed\":"); putu(g_shed); puts_(",\"shedlast\":"); putd(g_shedLast); puts_(",\"cb\":"); putu(g_cbMax); puts_(",\"nan\":[");
    for(int i = 0; i < GLUE_N; i++) { if(i) put(','); putu(g_nan[i]); } puts_("],\"maxcyc\":["); for(int i = 0; i < GLUE_N; i++) { if(i) put(','); putu(g_fxg[i].maxcyc); } puts_("]},\"sec\":["); for(int i = 0; i < 9; i++) { if(i) put(','); putu((uint32_t)((float)g_secWin[i] / (g_cyc_per_sample * 0.048f))); g_secWin[i] = 0; } puts_("],\"rm\":["); for(int i = 0; i < GLUE_N; i++) { if(i) put(','); const float r = g_fxg[i].rmin; putd(r > 1e8f ? 0 : (int32_t)(100.0f * log10f(r + 1e-9f))); g_fxg[i].rmin = 1e9f; } puts_("],\"secm\":["); for(int i = 0; i < 9; i++) { if(i) put(','); putu((uint32_t)((float)g_secMax[i] / (g_cyc_per_sample * 0.048f))); } puts_("],\"ol\":["); { auto db10 = [](float x) -> int32_t { return x > 1e-6f ? (int32_t)(200.0f * log10f(x)) : -1200; }; const float mn = g_olMin, av = g_olN ? g_olSum / (float)g_olN : 0.0f; putd(db10(mn > 1e8f ? 0.0f : mn)); put(','); putd(db10(av)); put(','); putu(g_olDrops); put(','); putd(db10(g_olLvl)); g_olMin = 1e9f; g_olSum = 0.0f; g_olN = 0; }
    puts_("],\"ex\":["); putd(g_exOwner); put(','); putu(g_exBlocked); puts_("],\"hm\":"); put(g_harmMode ? '1' : '0'); puts_(",\"foc\":"); putd(g_focus);
    puts_(",\"ay\":"); putd((int)(g_dly_ay * 1000.0f)); puts_(",\"warp\":"); putu((uint32_t)(g_rv_warp_t * 1000.0f));
    drGlueTelemetry(g_fxg[GI_DRONE].cyc);
}
// hello part: per effect (registry order) the pad colour, family, state and estimate; the chain order
static void fxGlueHello() {
    puts_(",\"fxm\":[");
    for(int gi = 0; gi < GLUE_N; gi++) {
        if(gi) put(','); const FxDef* d = g_fxg[gi].d; static const char* H = "0123456789abcdef";
        uint8_t c[3] = { 150, 110, 255 }; int fam = 0; if(gi == GI_DRONE) { c[0] = 120; c[1] = 70; c[2] = 255; fam = 0; } else if(d) { c[0] = d->rgb[0]; c[1] = d->rgb[1]; c[2] = d->rgb[2]; fam = d->fam; }
        put('"'); for(int k = 0; k < 3; k++) { put(H[c[k] >> 4]); put(H[c[k] & 15]); } put(','); put('0' + fam); put('"');
    }
    puts_("],\"ord\":["); for(int k = 0; k < GLUE_N; k++) { if(k) put(','); putu(kChain[k]); } put(']');
}

// ---- RUNTIME GUARD (main loop, the guarantee layer under the static admission rule). The audio thread raises g_shedReq when 8 of the last 32 callbacks went over 75 % or one went over 90 %.
// Victim: the most recently enabled NON-fader effect that is on (fade-out through the gate tail, refused for 5 s, its pad blinks red); only when none is left, the fader effect (reverb /
// drone / grain) that is active: its fader is blocked until it has been at 0. At most one shed per 150 ms, so the load can settle; repeats until the requests stop (load under about 65 %).
static void fxShedPoll(uint32_t now) {
    static uint32_t lastShed = 0;
    for(int gi = 0; gi < GLUE_N; gi++) { FxG& g = g_fxg[gi]; const bool w = g.want(); if(w && !g.wasWant) g.enabledAt = now; g.wasWant = w; }
    if(!g_shedReq) return;
    if(now - lastShed < 150) return;
    int victim = -1; uint32_t best = 0;
    for(int gi = 0; gi < GLUE_N; gi++) { if(gi == GI_REVERB || gi == GI_GRAIN || gi == GI_DRONE) continue; FxG& g = g_fxg[gi]; if(g.ok && g.want() && fxIsOn(gi) && g.enabledAt >= best) { best = g.enabledAt; victim = gi; } }
    if(victim >= 0) {
        FxG& g = g_fxg[victim]; g.latch = false; g.hold = false; g.shedUntil = now + 5000; g.refuseMs = now + 1500;
        if(victim == GI_FREEZE) g_frzClearMs = 0; if(victim == GI_DELAY) g_dmode = 0; if(victim == GI_HARMONY) for(int s = 0; s < 3; s++) g_hvPad[s] = -1;
    } else {                                                                              // last resort: the active fader effect
        for(int k = 0; k < 3; k++) { const int gi = kFaderFx[k]; if(gi < 0) continue; if(gi == g_exOwner || (gi == GI_DRONE ? g_dr_mix_req > 0.0005f : g_extmix[gi] > 0.0005f)) { g_exBlocked |= (uint8_t)(1u << k); g_fxg[gi].shedUntil = now + 5000; g_fxg[gi].refuseMs = now + 1500; victim = gi; break; } }
    }
    g_shedReq = false; lastShed = now; if(victim >= 0) { g_shed = g_shed + 1; g_shedLast = victim; }
}

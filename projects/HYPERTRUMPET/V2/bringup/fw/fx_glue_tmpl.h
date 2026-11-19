// The one generic glue template for every effect that is not the drone (the drone glue is owned by another agent and is adapted in fx_glue_chain.h).
// An effect is a FxDef (fx_glue_<name>.h, a few lines: id, create fn, arena bytes, CPU estimate, colour, optional hooks) plus one FxG runtime object.
//   * Zero CPU when off: not a single process() call while the gate is IDLE.
//   * ON  : the wake clears the effect (reset(), except the looper, which keeps its layers) and the wet fades in (15 ms blend + the effect's own mix smoother from 0).
//   * OFF : mix target 0, the effect keeps running with a fast smoother until its wet is below -80 dB or 50 ms have passed, then stops (FxGate in fx_glue_common.h).
//   * Param 0 (MIX) of a pad-driven effect is the user's wet level while on (FxG::mix, shown and persisted as MIX); the gate owns fx->v[0].
//   * Cost: DWT cycles around every process() call, per effect (avg, peak) -> telemetry and the refuse-on-overload rule.
#pragma once
#include "fx_glue_common.h"
struct FxDef {
    const char* id; hx::Fx* (*create)(hx::Arena&, float); uint32_t bytes;
    float est;                         // CPU estimate (fraction of the callback) until it has been measured: host microseconds x 40
    uint8_t fam; uint8_t rgb[3];       // family (0 pitch, 1 drive, 2 mod, 3 time, 4 spectral, 5 space) and the pad colour at full brightness
    void (*setup)(hx::Fx*);            // musical defaults after init (optional)
    void (*pre)(hx::Fx*);              // every block before smoothBlock(): sensor mapping (optional)
    void (*wake)(hx::Fx*);             // extra clearing on wake (optional)
    bool keepOnWake;                   // true: no reset() on wake (looper keeps its layers)
    volatile float* ext;               // fader-driven mix source (reverb); nullptr = pad-driven
    const char* lockMix;               // UI lock label for MIX when ext is set
};
struct FxG {
    const FxDef* d = nullptr; hx::Fx* fx = nullptr; hx::Arena arena; FxGate gate;
    volatile bool latch = false, hold = false;     // pad state: latched on, held on (momentary)
    float mix = 1.0f, fade = 1.0f, cyc = 0.0f, pk = 0.0f;
    volatile uint32_t refuseMs = 0; bool ok = false;
    uint32_t enabledAt = 0, shedUntil = 0; bool wasWant = false;        // runtime guard: when it was last switched on, refused until (ms) after a shed
    uint32_t maxcyc = 0; uint32_t nanCnt = 0; volatile bool needReset = false;                       // stability guards: max cycles of one process() call since boot, non-finite blocks, reset requested
    bool clean = true; volatile bool clearing = false; float avg = 0.0f;   // clean: state buffers are zero, so a wake needs no memset in the audio thread; clearing: the main loop is resetting the effect right now
    int noHoldIdx = -1;                                                  // param index whose value > 0.5 disables the dry hold (flanger THRUZ: its output already holds inverted dry)
    float rmin = 1e9f;                                                 // diagnostic: lowest block energy ratio (effect output incl. dry hold / dry input) since the last telemetry frame
    int fadeHold = 0; bool additive = false;                           // fadeHold: blocks the wake blend has been held back because the wet is not valid yet                                              // MIX is a send (harmony) or the effect keeps its own dry handling (looper): no dry hold
    volatile bool pendOn[4] = {false, false, false, false}; volatile float pendV[4] = {0, 0, 0, 0};   // actions queued by the pads, executed in the audio thread right before process()
    inline bool want() const { return latch || hold; }
};
static FxG g_fxg[GLUE_N];
static volatile uint32_t g_recov = 0;                    // effects reset by the main loop after a non-finite block
static volatile float g_extmix[GLUE_N];                  // sensor-driven mix targets (fader / lidar), see fx_controls.h; an effect with FxDef::ext runs while this OR its pad / web toggle wants it
static uint8_t HX_SDRAM g_pool[14 << 20] __attribute__((aligned(32)));     // all generic effect arenas, SDRAM (looper 9.2 MB, grain 1.1, delay 1.1, spectral ~0.5, rest small)
static hx::Arena g_poolA;
static hx::FxCtx g_ctx = {};
static void fxgInit(int gi, const FxDef& d) {
    if(!g_poolA.base) g_poolA.bind(g_pool, sizeof(g_pool));
    FxG& g = g_fxg[gi]; FxGlue& gl = g_glue[gi]; g.d = &d; gl.id = d.id; gl.ok = false; gl.fx = nullptr;
    void* m = g_poolA.allocBytes(d.bytes, 32); if(!m) return;
    g.arena.bind(m, d.bytes); g.fx = d.create(g.arena, 48000.0f); if(!g.fx || !g.fx->ok) return;
    g.fx->init(48000.0f, 48.0f); if(d.setup) d.setup(g.fx);
    g.mix = g.fx->v[0]; g.fx->setParam(0, 0.0f); g.fx->Fx::snap(); g.gate.smA = g.fx->smA;
    g.noHoldIdx = !strcmp(d.id, "flange") ? g.fx->findParam("THRUZ") : -1; if(g.noHoldIdx >= 0) g.fx->setParam(g.noHoldIdx, 0.0f);      // flanger: THRUZ default 0 (thru-zero puts polarity-inverted dry in the output, the hold would cancel it)
    g.additive = d.keepOnWake || !strcmp(d.id, "harmony");
    gl.fx = g.fx; gl.ok = true; g.ok = true; glueCaptureDefaults(gl); gl.def[0] = g.mix; gl.mixUser = &g.mix;      // locks (sensor-owned params) are set in fxGlueInit
}
static inline void fxgAct(int gi, int id, float v) { FxG& g = g_fxg[gi]; g.pendV[id] = v; g.pendOn[id] = true; }   // id = hx::Action
static void fxgWake(FxG& g, float mixT) {
    if(!g.d->keepOnWake && !g.clean) g.fx->reset();                  // fallback only: the main loop normally cleared it already when it went idle (fxClearPoll)
    g.clean = false;
    if(g.d->wake) g.d->wake(g.fx);
    g.fx->v[0] = mixT; g.fx->sm[0] = 0.0f; g.fx->smA = g.gate.smA; g.fade = 0.0f; g.fadeHold = 0;
}
// In place on L/R. st = "L and R differ" (stereo) on entry and exit. Returns true when the effect ran (false = not one instruction of the effect executed).
static bool fxgRun(FxG& g, float* L, float* R, bool& st, size_t n) {
    if(!g.ok || g.clearing) return false;
    const float ex = g.d->ext ? *g.d->ext : 0.0f, wn = g.want() ? g.mix : 0.0f; const float mixT = ex > wn ? ex : wn;
    if(gateEnter(g.gate, g.fx, mixT)) fxgWake(g, mixT);
    if(g.gate.state == FXS_IDLE) return false;
    for(int a = 0; a < 4; a++) if(g.pendOn[a]) { g.pendOn[a] = false; g.fx->action(a, g.pendV[a]); }       // pad actions land after the wake clear
    g.fx->v[0] = (g.gate.state == FXS_TAIL) ? 0.0f : mixT;
    if(g.d->pre) g.d->pre(g.fx);
    g.fx->smoothBlock();
    const bool was = st, trans = g.fade < 1.0f;
    static float dL[128], dR[128];
    if(trans) { memcpy(dL, L, n * sizeof(float)); memcpy(dR, was ? R : L, n * sizeof(float)); }
    if(!was) memcpy(R, L, n * sizeof(float));
    static float hL[128], hR[128]; const bool hold = !g.additive && !(g.noHoldIdx >= 0 && g.fx->v[g.noHoldIdx] > 0.5f); if(hold) { memcpy(hL, L, n * sizeof(float)); memcpy(hR, R, n * sizeof(float)); }   // dry copy for the dry hold
    const bool o = g.fx->process(g_ctx, L, R, (int)n, was);
    if(hold) { const float mm = g.fx->sm[0], k = mm > 0.5f ? mm - 0.5f : 0.0f; if(k > 0.0f) { for(size_t i = 0; i < n; i++) { L[i] += hL[i] * k; R[i] += hR[i] * k; } } st = true;
        float e1 = 0.0f, e0 = 0.0f; for(size_t i = 0; i < n; i++) { e1 += L[i] * L[i]; e0 += hL[i] * hL[i]; } if(e0 > 1e-12f) { const float r = e1 / e0; if(r < g.rmin) g.rmin = r; } }   // at full wet the dry never drops below -6 dB: a fresh delay / reverb / grain / freeze buffer is silent for a while and must not cut the audio
    if(trans) {
        float eY = 0.0f, eD = 0.0f; for(size_t i = 0; i < n; i++) { eY += L[i] * L[i] + R[i] * R[i]; eD += dL[i] * dL[i] + dR[i] * dR[i]; }
        if(eY < 0.1f * eD && g.fadeHold < 500) { g.fadeHold++; for(size_t i = 0; i < n; i++) { L[i] = dL[i]; R[i] = dR[i]; } st = true; gateLeave(g.gate, g.fx, n); return true; }   // the wet is not valid yet (empty buffer / still building): the dry stays, the blend waits (at most 500 ms)
        float g0 = g.fade, g1 = g0 + (float)n / (0.015f * 48000.0f); if(g1 > 1.0f) g1 = 1.0f; const float inc = (g1 - g0) / (float)n; float x = g0;
        for(size_t i = 0; i < n; i++) { float wl = L[i], wr = o ? R[i] : wl; L[i] = dL[i] + (wl - dL[i]) * x; R[i] = dR[i] + (wr - dR[i]) * x; x += inc; }
        g.fade = g1; st = true;
    } else st = o || was;
    gateLeave(g.gate, g.fx, n);
    return true;
}

// Main loop, 200 Hz: when an effect has gone idle (tail finished, nobody wants it) reset it HERE, so the next wake costs no memset in the audio callback (the reverb reset alone is a
// 330 KB memset = more than one callback, measured as a 135 % peak on the Seed). One effect per call. The audio thread skips an effect while `clearing` is set.
static void fxClearPoll() {
    static int rr = 0;
    for(int t = 0; t < GLUE_N; t++) {
        const int gi = (rr + t) % GLUE_N; FxG& g = g_fxg[gi];
        if(g.ok && g.needReset && gi != GI_DRONE) { g.clearing = true; g.gate.state = FXS_IDLE; g.fx->reset(); g.clean = true; g.needReset = false; g.clearing = false; g_recov++; rr = gi + 1; return; }   // non-finite output: recover
        if(!g.ok || !g.d || g.clean || g.d->keepOnWake || g.gate.state != FXS_IDLE || g.want() || (g.d->ext && *g.d->ext > 0.0005f)) continue;
        g.clearing = true;
        if(g.gate.state == FXS_IDLE && !g.want()) { g.fx->reset(); g.clean = true; }
        g.clearing = false; rr = gi + 1; return;
    }
}

// fx_looper.h -- HYPERTRUMPET V2 multi-layer looper (v1: RNBO 12-layer stereo 80 s; v2: HX_LOOP_LAYERS mono layers of
// HX_LOOP_SECONDS each, default 4 x 12 s = 9.2 MB of the 12 MB slot arena). Own work (Emory Smith / EMGOR).
// Gestures (actions): A_MAIN tap = record first layer / end it (sets loop length, starts playing) / start overdub of the next layer /
//   punch out. An overdub also punches out by itself after exactly one loop. A_ALT = play/stop toggle (stop fades, rewinds), A_HOLD =
//   stop, A_CLEAR = clear all (v1: hold >= 0.5 s on pad 13). Layers sum; sample-accurate loop; 10 ms fades at punch in/out, loop start
//   fade-in/end fade-out of the first layer. DRYDUCK: dry level = 1 - layers/N (as in v1). TRANSP -12..12 st and REVERSE act on the
//   playback head only (own head, Hermite weights computed once per sample and shared by all layers; direction change ramps via
//   zero rate like a tape). Loop sum goes through a transparent (< 0.9) soft limiter. Runtime-limited: layers/length shrink to the arena.
// Memory is cleared lazily in 4096-sample chunks after CLEAR (never a long memset in the audio thread); a new recording waits for its layer.
#pragma once
#include "fx_common.h"
#ifndef HX_LOOP_LAYERS
#define HX_LOOP_LAYERS 4
#endif
#ifndef HX_LOOP_SECONDS
#define HX_LOOP_SECONDS 12
#endif
namespace hx {
// Optional dedicated looper memory (firmware: the 46 MB SDRAM looper pool, see HYPERTRUMPET_V2/docs/FX_STATUS.md memory map).
// If bound (set once at boot, before any setEffect) the layers come from this arena (reset on every looper construction, so only
// ONE looper may exist at a time); otherwise from the slot arena (host tests, simulator).
inline Arena* g_loopPool = nullptr;
struct FxLooper : Fx {
    enum { P_MIX = 0, P_VOLUME, P_TRANSP, P_REVERSE, P_DRYDUCK };
    enum St { S_EMPTY = 0, S_REC1, S_PLAY, S_OVR, S_STOP, S_TRIM };   // S_TRIM: first loop closed, scanning back for the trailing silence (NEPTR state 6)
    float* lay[HX_LOOP_LAYERS] = {}; int nLay = 0; int cap = 0;          // layers actually allocated, samples per layer
    int dirty[HX_LOOP_LAYERS] = {}; int clrPos = 0;
    int len = 0, count = 0, st = S_EMPTY, wp = 0, recLayer = -1, recCount = 0;
    int trimPos = 0, rawLen = 0;
    float pp = 0.f, recG = 0.f, playG = 0.f, dirCur = 1.f, fadeInc = 0.f;
    bool recOn = false, wantRec = false, recClosing = false, playing = false, clearPend = false;
    volatile uint8_t pend = 0;   // bit0 main, bit1 alt(toggle), bit2 clear, bit3 stop

    FxLooper(Arena& a, float s) {
        name = "LOOPER"; sr = s;
        static const char* const onoff[] = { "OFF", "ON" };
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_VOLUME, "VOLUME", -18.f, 18.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_TRANSP, "TRANSP", -12.f, 12.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_REVERSE, "REVERSE", 0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT, onoff);
        P(P_DRYDUCK, "DRYDUCK", 0.f, 1.f, 1.f, 1.f, PF_ENUM | PF_QUANT, onoff);
        setRole(R_AMT, P_VOLUME); setRole(R_PITCH, P_TRANSP);
        fadeInc = 1.f / (0.010f * s);
        int want = (int)(HX_LOOP_SECONDS * s);
        // fit to the arena: shrink layer count first, then length (>= 1 s)
        Arena& src = g_loopPool ? *g_loopPool : a;
        if (g_loopPool) g_loopPool->reset();
        size_t room = src.left() > 64 ? src.left() - 64 : 0;
        int nl = HX_LOOP_LAYERS;
        while (nl > 1 && (size_t)nl * want * 4 > room) nl--;
        if ((size_t)nl * want * 4 > room) want = (int)(room / 4 / nl);
        if (want < (int)s) { ok = false; return; }
        for (int k = 0; k < nl; k++) { lay[k] = src.floats((size_t)want); if (!lay[k]) { ok = false; return; } }
        nLay = nl; cap = want;
    }
    void action(int id, float) override {
        if (id == A_MAIN) pend |= 1; else if (id == A_ALT) pend |= 2; else if (id == A_CLEAR) pend |= 4; else if (id == A_HOLD) pend |= 8;
    }
    void reset() override { pend |= 4; }
    int layers() const { return count; }
    int length() const { return len; }
    int state() const { return st; }

    static inline float limit(float x) {   // identity below 0.9, soft knee to 1.0
        float a = x < 0.f ? -x : x; if (a <= 0.9f) return x;
        float y = 0.9f + 0.1f * softclip((a - 0.9f) * 10.f); return x < 0.f ? -y : y;
    }
    void startRec(int layer, int from) {
        recLayer = layer; recOn = true; recClosing = false; recCount = 0; (void)from;
    }
    void clearAll() {
        for (int k = 0; k < nLay; k++) { int u = (k < count || k == recLayer) ? (len > 0 ? len : (wp > 0 ? wp : 0)) : 0; if (st == S_REC1 && k == 0) u = wp; if (u > dirty[k]) dirty[k] = u; }
        if (recLayer >= 0 && dirty[recLayer] < cap && st == S_REC1) dirty[recLayer] = cap;
        count = 0; len = 0; st = S_EMPTY; wp = 0; pp = 0.f; recOn = false; recClosing = false; recLayer = -1; wantRec = false; playing = false; playG = 0.f; recG = 0.f;
    }
    void handleMain() {
        if (st == S_EMPTY) { wantRec = true; }
        else if (st == S_REC1) { beginTrim(); }
        else if (st == S_PLAY && count < nLay && !recOn && !wantRec) { wantRec = true; }
        else if (st == S_OVR && recOn && !recClosing) { recClosing = true; }
    }
    // NEPTR looper rule (looper.orc state 6 / LP_QCLOSE, UNCLOCKED): the first close does not bake the press-timing silence into the loop: scan backward from the
    // end for the last sample above -60 dB and cut the loop there; if the whole take is silent keep it raw. Bounded work per block (see process()).
    void beginTrim() { recOn = false; recClosing = false; recLayer = -1; rawLen = wp; trimPos = wp; st = S_TRIM; }
    void finishFirst() {
        len = wp; if (len < 64) { clearAll(); return; }
        int f = (int)(0.010f * sr); if (f > len / 2) f = len / 2;
        for (int i = 0; i < f; i++) lay[0][len - 1 - i] *= (float)i / (float)f;       // end fade-out of layer 0
        count = 1; recLayer = -1; recOn = false; recClosing = false;
        st = S_PLAY; wp = 0; pp = 0.f; playing = true; playG = 1.f;
    }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        uint8_t p = pend; pend = 0;
        if (p & 4) {
            if (st == S_OVR && (recOn || wantRec) && count > 0) {                         // hold during an overdub pass = cancel just that pass (NEPTR spec 2026-06-22 sec. 2), the loop keeps playing
                if (recOn && recLayer >= 0) { if (len > dirty[recLayer]) dirty[recLayer] = len; recOn = false; recClosing = false; recLayer = -1; recG = 0.f; }
                wantRec = false; st = S_PLAY;
            }
            else if (count > 0 && playG > 0.f) { playing = false; clearPend = true; } else clearAll();   // otherwise delete all (v1 pad 13 hold)
        }
        if ((p & 1) && !clearPend) handleMain();
        if (p & 8) { if (st == S_PLAY || st == S_OVR) { playing = false; } }
        if (p & 2) { if (st == S_STOP) { st = S_PLAY; playing = true; pp = 0.f; wp = 0; } else if (st == S_PLAY || st == S_OVR) playing = false; }
        // lazy clear chunk
        for (int k = 0; k < nLay; k++) if (dirty[k] > 0) {
            int e = clrPos + 4096; if (e > dirty[k]) e = dirty[k];
            memset(lay[k] + clrPos, 0, (size_t)(e - clrPos) * 4); clrPos = e;
            if (clrPos >= dirty[k]) { dirty[k] = 0; clrPos = 0; }
            break;
        }
        // pending record waits for a clean layer
        if (wantRec) {
            int k = (st == S_EMPTY) ? 0 : count;
            if (k < nLay && dirty[k] == 0) {
                wantRec = false;
                if (st == S_EMPTY) { st = S_REC1; wp = 0; startRec(0, 0); }
                else { st = S_OVR; startRec(k, wp); }
            }
        }
        if (st == S_TRIM) {                                    // 16 x block samples per call: a typical 0.2 s tail takes 1 block, the worst case (all silent, 12 s) about 1.2 s
            int steps = 16 * n; const float* b0 = lay[0];
            while (steps > 0 && trimPos > 0) { steps--; if (fabsf(b0[--trimPos]) > 0.001f) { wp = trimPos + 1; finishFirst(); break; } }
            if (st == S_TRIM && trimPos <= 0) { wp = rawLen; finishFirst(); }          // nothing above -60 dB: keep the raw take
        }
        const float vol = dbToLin(sm[P_VOLUME]), mix = sm[P_MIX];
        const float ratio = semiToRatio(sm[P_TRANSP]);
        const float dirT = sm[P_REVERSE] > 0.5f ? -1.f : 1.f;
        const float dirInc = 2.f / (0.05f * sr);
        const float duck = (sm[P_DRYDUCK] > 0.5f && nLay > 0) ? (float)count / (float)nLay : 0.f;
        const float dryG = 1.f - mix * duck;
        const int nc = stereoIn ? 2 : 1; (void)nc;
        for (int i = 0; i < n; i++) {
            float x = L[i];
            // ---- record (loop-time head wp)
            if (recOn) {
                float g = recClosing ? recG - fadeInc : recG + fadeInc;
                recG = g < 0.f ? 0.f : (g > 1.f ? 1.f : g);
                lay[recLayer][wp] = x * recG;
                if (st == S_OVR) { if (++recCount >= len - (int)(0.010f * sr) && !recClosing) recClosing = true; }
                if (recClosing && recG <= 0.f) { recOn = false; recClosing = false; if (st == S_OVR) { count++; st = S_PLAY; } recLayer = -1; }
            }
            // ---- play (own head)
            float loop = 0.f;
            if (len > 0 && (count > 0)) {
                float pg = playing ? playG + fadeInc : playG - fadeInc; playG = pg < 0.f ? 0.f : (pg > 1.f ? 1.f : pg);
                if (playG > 0.f) {
                    int ip = (int)pp; float f = pp - (float)ip;
                    if (f == 0.f) { for (int k = 0; k < count; k++) loop += lay[k][ip]; }
                    else {
                        float f2 = f * f, f3 = f2 * f;
                        float w0 = -0.5f * f + f2 - 0.5f * f3, w1 = 1.f - 2.5f * f2 + 1.5f * f3, w2 = 0.5f * f + 2.f * f2 - 1.5f * f3, w3 = -0.5f * f2 + 0.5f * f3;
                        int i0 = ip - 1; if (i0 < 0) i0 += len; int i2 = ip + 1; if (i2 >= len) i2 -= len; int i3 = i2 + 1; if (i3 >= len) i3 -= len;
                        for (int k = 0; k < count; k++) { const float* b = lay[k]; loop += w0 * b[i0] + w1 * b[ip] + w2 * b[i2] + w3 * b[i3]; }
                    }
                    loop *= playG;
                }
                dirCur += dirCur < dirT ? dirInc : (dirCur > dirT ? -dirInc : 0.f);
                if ((dirCur - dirT) * (dirCur - dirT) < dirInc * dirInc) dirCur = dirT;
                pp += ratio * dirCur;
                if (pp >= (float)len) pp -= (float)len; else if (pp < 0.f) pp += (float)len;
                if (pp >= (float)len) pp = 0.f;
            }
            if (clearPend && playG <= 0.f) { clearAll(); clearPend = false; }
            // ---- advance loop-time head
            if (st == S_REC1) { if (++wp >= cap) { wp = cap; finishFirst(); } }
            else if (len > 0 && (playing || playG > 0.f || recOn)) { if (++wp >= len) wp = 0; }
            if (!playing && playG <= 0.f && (st == S_PLAY || st == S_OVR) && !recOn) { st = S_STOP; wp = 0; pp = 0.f; }
            float wet = limit(loop * vol) * mix;
            L[i] = x * dryG + wet;
            if (stereoIn) R[i] = R[i] * dryG + wet;
        }
        return stereoIn;
    }
};
inline Fx* fx_create_looper(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxLooper)); return m ? new (m) FxLooper(a, sr) : nullptr; }
}

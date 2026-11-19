// fx_blooper.h -- HYPERTRUMPET V2 BLOOPER: generative slice-thrower that MODIFIES THE LOOPER's loop (not the live input; MOOD does that). Own work (Emory Smith / EMGOR).
// Modelled on the user's Csound BLOOPER (NEPTR phase4 csound/effects/blooper.orc): it owns no buffer, records nothing, and punctuates the loop's normal playback with
// grain EVENTS: duty capped at 0.45, event length log-uniform x1/3..x3 around 0.10*6^SIZE s (max len/2), discrete per-event speeds {1x .40, .5x .20, 2x .20, -1x .20}
// times the looper's own rate, 8 ms trapezoid on every event edge, gap = len*(1-duty)/duty*rand(0.7,1.6), generative events LAUNCH ON THE LOOP'S TICK GRID (tick = len/2^m <=
// 0.375 s, anchored at loop start), event start = playhead + displacement, a pedal/expression push CUES an event. Csound REPLACES the looper's playback with its render (never a
// sum: loops are not doubled; MOD=0 is an exact null). Same here: REPLACE=1 (default) subtracts the looper's normal playback while a slice sounds, REPLACE=0 overlays.
//
// WIRING (glue, once per block, BEFORE the looper's process(), then run blooper.process() on the looper's OUTPUT buffers, i.e. Blooper sits after the looper in the chain):
//     bl->setSource(lp->lay, lp->count, lp->len, lp->pp, lp->ratioNow, lp->wetGain);   // see FxLooper accessors below / test glue
//   layers: array of per-layer float* (summed as played); n: layers audible; len: loop length (samples); playPos: looper play head at block start (float samples);
//   rate: signed samples-per-sample of that head (TRANSP ratio * reverse dir; 1 = normal); gain: the looper's wet gain (VOLUME*MIX*playG) so the Blooper's replace-subtraction cancels
//   the looper's own output exactly (exact at rate 1; at fractional rates the looper reads Hermite and so does the Blooper normal read).
//   No source this block (never called, len<64, no layers, gain~0, looper stopped) => dry pass-through, ZERO work (hasSource()==false: show the pad DIM).
//
// PARAMS: MIX (events level + replace depth), ON (toggle state), PROB (duty = 0.45*PROB; PROB=0 + joystick centred = output unchanged), SIZE, DIST (NEAR<->FAR = how far from the
//   playhead in the loop an event grabs from: displacement fraction 0.02+0.48*DIST of the loop, +-; also the space: far = darker LP 18k->2.5k, -4 dB, 2 allpasses blend 60%; DIST=0 is
//   transparent), SPEEDMUL (joystick -1..+1; deadzone +-0.08, then 2^x: +1 -> 2x, -1 -> 0.5x on every slice's read rate, ~5 ms one-pole slew = instant to the ear, click-free,
//   not block-smoothed; leaving the deadzone also cues an immediate event even at PROB=0), LEVEL, REPLACE.
// ACTIONS: A_MAIN toggle ON; A_HOLD >0.5 momentary on; A_ALT cue a slice now (immediate, off-grid); A_CLEAR panic: stop the current slice.
// No static/global state; wake() zeroes everything. Memory: two allpass buffers only (3,120 B) -- the old 11.5 MB private capture ring is gone.
#pragma once
#include "fx_common.h"
#ifndef HX_BLP_MAXL
#define HX_BLP_MAXL 8
#endif
namespace hx {
struct FxBlooper : Fx {
    enum { P_MIX = 0, P_ON, P_PROB, P_SIZE, P_DIST, P_SPEED, P_LEVEL, P_REPL };
    static const int AP1 = 223, AP2 = 557;
    float* ap1 = nullptr; float* ap2 = nullptr;
    struct Head { int i = 0; float f = 0.f; };
    // source (set each block by the glue)
    const float* lay[HX_BLP_MAXL] = {}; int nL = 0, len = 0; float srcRate = 1.f, srcGain = 0.f; Head ph; bool srcFresh = false, srcValid = false;
    int tickLen = 0, tkPrv = -1; float invTick = 0.f; int tickForLen = 0;
    // engine
    float onG = 0.f, gap = 0.f, mulCur = 1.f, lpY = 0.f;
    bool arm = false, joyActive = false, cue = false, stut = false;
    int evRem = 0, evLen = 0, sRem = 0, sLen = 0, a1i = 0, a2i = 0, idleN = 0, tAbs = 0, startI = 0;
    float evSpd = 1.f, startF = 0.f; Head eh;
    uint32_t rng = 0x1234567u;
    volatile uint8_t pend = 0;   // bit0 cue, bit1 panic
#ifdef HX_BLP_TELEMETRY
    double dbgWetSq = 0, dbgWetDiffSq = 0; float dbgWetPrev = 0;
#endif
    int dbgEvents = 0, dbgActiveSamples = 0, dbgLastFireT = 0, dbgLastStartPos = 0, dbgLastLen = 0, dbgLastStut = 0, dbgLastOnTick = 0, dbgLastCue = 0, dbgLastPh = 0; float dbgLastSpd = 1.f;

    FxBlooper(Arena& a, float s) {
        name = "BLOOPER"; sr = s;
        static const char* const onoff[] = { "OFF", "ON" };
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_ON, "ON", 0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT, onoff);
        P(P_PROB, "PROB", 0.f, 1.f, 0.45f, 0.f, PF_PERCENT);
        P(P_SIZE, "SIZE", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_DIST, "DIST", 0.f, 1.f, 0.35f, 0.f, PF_PERCENT);
        P(P_SPEED, "SPEEDMUL", -1.f, 1.f, 0.f, 0.f, PF_BIPOLAR); noSm[P_SPEED] = true;
        P(P_LEVEL, "LEVEL", -18.f, 12.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_REPL, "REPLACE", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        setRole(R_AMT, P_PROB); setRole(R_TIME, P_SIZE); setRole(R_TONE, P_DIST); setRole(R_MOD, P_SPEED);
        ap1 = a.floats(AP1); ap2 = a.floats(AP2);
        if (!ap1 || !ap2) { ok = false; return; }
        wake();
    }
    void wake() {   // zero ALL state (arena memory on the Seed is dirty)
        if (!ok) return;
        memset(ap1, 0, AP1 * 4); memset(ap2, 0, AP2 * 4);
        for (int k = 0; k < HX_BLP_MAXL; k++) lay[k] = nullptr;
        nL = 0; len = 0; srcRate = 1.f; srcGain = 0.f; ph = Head(); srcFresh = srcValid = false; tickLen = 0; tkPrv = -1; invTick = 0.f; tickForLen = 0;
        onG = 0.f; gap = 0.f; mulCur = 1.f; lpY = 0.f; arm = joyActive = cue = stut = false; evRem = evLen = sRem = sLen = a1i = a2i = idleN = tAbs = startI = 0;
        evSpd = 1.f; startF = 0.f; eh = Head(); rng = 0x1234567u; pend = 0;
        dbgEvents = dbgActiveSamples = dbgLastFireT = dbgLastStartPos = dbgLastLen = dbgLastStut = dbgLastOnTick = dbgLastCue = dbgLastPh = 0; dbgLastSpd = 1.f;
#ifdef HX_BLP_TELEMETRY
        dbgWetSq = dbgWetDiffSq = 0; dbgWetPrev = 0;
#endif
        v[P_ON] = pt[P_ON].min; sm[P_ON] = v[P_ON];
    }
    void reset() override { pend = 0; evRem = 0; arm = cue = false; srcFresh = srcValid = false; }
    void action(int id, float value) override {
        if (id == A_MAIN) v[P_ON] = v[P_ON] > 0.5f ? 0.f : 1.f;
        else if (id == A_HOLD) v[P_ON] = value > 0.5f ? 1.f : 0.f;
        else if (id == A_ALT) pend |= 1;
        else if (id == A_CLEAR) pend |= 2;
    }
    // glue API (control/audio thread, once per block, before process)
    void setSource(const float* const* layers, int nLayers, int loopLen, float playPos, float rate, float gain) {
        nL = nLayers > HX_BLP_MAXL ? HX_BLP_MAXL : (nLayers < 0 ? 0 : nLayers);
        for (int k = 0; k < nL; k++) lay[k] = layers[k];
        len = loopLen; srcRate = rate; srcGain = gain;
        srcValid = ok && nL > 0 && len >= 64 && gain > 1e-4f && playPos == playPos;
        srcFresh = true;
        if (srcValid) {
            float pp = playPos; if (pp < 0.f) pp = 0.f; int i = (int)pp; ph.f = pp - (float)i; ph.i = i >= len ? i % len : i;
            if (len != tickForLen) {   // tick grid = len / 2^m, <= 0.375 s (Csound chase_bliss_clock v4 band), >= 2 ticks for loops >= 0.1 s
                int m = 0; while (m < 8 && (float)len / (float)(1 << m) > 0.375f * sr) m++;
                if (m == 0 && (float)len > 0.1f * sr) m = 1;
                tickLen = len >> m; if (tickLen < 1) tickLen = 1; invTick = 1.f / (float)tickLen; tickForLen = len; tkPrv = -1;
            }
        }
    }
    bool hasSource() const { return srcValid; }                 // false -> pad dim, zero CPU
    bool isOn() const { return v[P_ON] > 0.5f; }
    bool active() const { return srcValid && isOn(); }          // pad bright
    float rate() const { return mulCur; }
    size_t bytes() const { return (size_t)(AP1 + AP2) * 4; }

    static inline float limit(float x) { float a = x < 0.f ? -x : x; if (a <= 0.9f) return x; float y = 0.9f + 0.1f * softclip((a - 0.9f) * 10.f); return x < 0.f ? -y : y; }
    static inline float joyToMul(float j) {
        float a = j < 0.f ? -j : j; if (a < 0.08f) return 1.f;
        a = (a - 0.08f) * (1.f / 0.92f); if (a > 1.f) a = 1.f; return exp2f(j < 0.f ? -a : a);
    }
    inline void adv(Head& h, float r) const {
        h.f += r;
        if (h.f >= 1.f || h.f < 0.f) { float fl = floorf(h.f); h.i += (int)fl; h.f -= fl; if (h.f >= 1.f) { h.f -= 1.f; h.i++; } }
        if (h.i >= len) { h.i -= len; if (h.i >= len) h.i %= len; } else if (h.i < 0) { h.i += len; if (h.i < 0) { h.i %= len; if (h.i < 0) h.i += len; } }
    }
    inline float readLin(const Head& h) const {
        int i1 = h.i + 1; if (i1 >= len) i1 = 0; float s = 0.f;
        if (h.f == 0.f) { for (int k = 0; k < nL; k++) s += lay[k][h.i]; }
        else { for (int k = 0; k < nL; k++) s += lay[k][h.i] + (lay[k][i1] - lay[k][h.i]) * h.f; }
        return s;
    }
    inline float readHerm(const Head& h) const {   // identical weights to FxLooper's playback read
        float s = 0.f;
        if (h.f == 0.f) { for (int k = 0; k < nL; k++) s += lay[k][h.i]; return s; }
        float f = h.f, f2 = f * f, f3 = f2 * f;
        float w0 = -0.5f * f + f2 - 0.5f * f3, w1 = 1.f - 2.5f * f2 + 1.5f * f3, w2 = 0.5f * f + 2.f * f2 - 1.5f * f3, w3 = -0.5f * f2 + 0.5f * f3;
        int ip = h.i, i0 = ip - 1; if (i0 < 0) i0 += len; int i2 = ip + 1; if (i2 >= len) i2 -= len; int i3 = i2 + 1; if (i3 >= len) i3 -= len;
        for (int k = 0; k < nL; k++) { const float* b = lay[k]; s += w0 * b[i0] + w1 * b[ip] + w2 * b[i2] + w3 * b[i3]; }
        return s;
    }
    void fire(float ctr, float dist, bool immediate) {
        float u = rnd01(rng), u2 = rnd01(rng), u3 = rnd01(rng), u4 = rnd01(rng), u5 = rnd01(rng);
        float NL = ctr * powf(3.f, u * 2.f - 1.f);
        float lo = 0.02f * sr, hi = (float)len * 0.5f; if (NL < lo) NL = lo; if (NL > hi) NL = hi; if (NL < 1.f) NL = 1.f;
        bool st = u4 < 0.2f; float spd;
        if (st) spd = 1.f; else spd = u2 < 0.40f ? 1.f : (u2 < 0.60f ? 0.5f : (u2 < 0.80f ? 2.f : -1.f));
        int ln = (int)NL, slice = ln;
        if (st) { slice = (int)(NL / (float)(2 + (int)(u5 * 3.999f))); int mn = (int)(0.015f * sr); if (slice < mn) slice = mn; if (slice > ln) slice = ln; }
        float disp = (0.02f + 0.48f * dist) * (float)len * (u3 * 2.f - 1.f);
        float p = (float)ph.i + ph.f + disp; p = floorf(p); p -= (float)len * floorf(p / (float)len); if (p >= (float)len || p < 0.f) p = 0.f;   // integer start: 1x slices are bit-exact copies
        eh.i = (int)p; eh.f = p - (float)eh.i; startI = eh.i; startF = eh.f;
        evLen = evRem = ln; evSpd = spd; stut = st; sLen = sRem = slice;
        dbgEvents++; dbgLastFireT = tAbs; dbgLastStartPos = startI; dbgLastLen = ln; dbgLastSpd = spd; dbgLastStut = st ? 1 : 0; dbgLastPh = ph.i; dbgLastOnTick = (ph.i % tickLen) == 0; dbgLastCue = immediate ? 1 : 0;
    }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        const bool have = srcFresh && srcValid; srcFresh = false;
        uint8_t pd = pend; pend = 0; if (pd & 2) { evRem = 0; arm = false; cue = false; }
        const bool on = v[P_ON] > 0.5f;
        const int idleMax = (int)(0.6f * sr);
        if (!have) {   // no loop: dry pass-through, no work (flush the tail state once)
            if (evRem > 0 || lpY != 0.f || idleN < idleMax) { evRem = 0; arm = false; cue = false; lpY = 0.f; memset(ap1, 0, AP1 * 4); memset(ap2, 0, AP2 * 4); idleN = idleMax; }
            onG = on ? onG : 0.f; tkPrv = -1; mulCur = joyToMul(v[P_SPEED]); joyActive = (mulCur != 1.f); tAbs += n; return stereoIn;
        }
        if (!on && onG <= 0.f && evRem <= 0 && idleN >= idleMax) { mulCur = joyToMul(v[P_SPEED]); joyActive = (mulCur != 1.f); cue = false; arm = false; tAbs += n; return stereoIn; }
        if (pd & 1) cue = true;
        const float mix = sm[P_MIX], lvl = dbToLin(sm[P_LEVEL]), prob = sm[P_PROB], dist = sm[P_DIST], size = sm[P_SIZE], repl = sm[P_REPL];
        const float duty = 0.45f * prob;
        const float ctr = 0.10f * powf(6.f, size) * sr;
        const float mulT = joyToMul(v[P_SPEED]); const bool joyNow = (mulT != 1.f);
        const float lpHz = 18000.f * powf(2500.f / 18000.f, dist);
        const float lpA = dist < 0.001f ? 1.f : 1.f - expf(-kTwoPi * lpHz / sr);
        const float gAP = 0.5f * dist, apBlend = 0.6f * dist, farG = 1.f - 0.3688f * dist;
        const float xfN = 0.008f * sr, invXf = 1.f / xfN, sXf = 0.003f * sr, invSXf = 1.f / sXf;
        const float mulA = 1.f - expf(-1.f / (0.005f * sr)), onInc = 1.f / (0.010f * sr);
        const float g = srcGain, rate = srcRate;
        if (on && onG <= 0.f && !arm && evRem <= 0) gap = 0.1f * sr;
        if (on && joyNow && !joyActive && evRem <= 0) cue = true;
        joyActive = joyNow;
        for (int i = 0; i < n; i++) {
            onG += on ? onInc : -onInc; onG = onG < 0.f ? 0.f : (onG > 1.f ? 1.f : onG);
            if (onG <= 0.f && !on) { evRem = 0; arm = false; cue = false; }
            mulCur += (mulT - mulCur) * mulA; if (mulCur - mulT < 1e-5f && mulT - mulCur < 1e-5f) mulCur = mulT;
            // ---- scheduler: gap countdown ARMS; generative events FIRE on the next loop-tick boundary of the normal playhead; cues fire at once
            int ti = (int)((float)ph.i * invTick); bool tickX = (tkPrv >= 0 && ti != tkPrv); tkPrv = ti;
            if (evRem <= 0 && on) {
                if (cue) { fire(ctr, dist, true); cue = false; arm = false; }
                else if (duty > 0.001f) {
                    if (!arm) { gap -= 1.f; if (gap <= 0.f) arm = true; }
                    if (arm && tickX) { arm = false; fire(ctr, dist, false); }
                } else arm = false;
            }
            float env = 0.f, ev = 0.f, nrm = 0.f; const bool wasAct = evRem > 0;
            if (evRem > 0) {
                float e = (float)(evLen - evRem) * invXf, e2 = (float)evRem * invXf; if (e2 < e) e = e2; if (e > 1.f) e = 1.f; if (e < 0.f) e = 0.f;
                if (stut) { float s1 = (float)(sLen - sRem) * invSXf, s2 = (float)sRem * invSXf; if (s2 < s1) s1 = s2; if (s1 > 1.f) s1 = 1.f; if (s1 < 0.f) s1 = 0.f; e *= s1; }
                env = e;
                ev = readLin(eh) * g;
                if (repl > 0.f && env > 0.f) nrm = readHerm(ph) * g;
                adv(eh, rate * evSpd * mulCur);
                evRem--; dbgActiveSamples++;
                if (stut && --sRem <= 0 && evRem > 0) { eh.i = startI; eh.f = startF; sRem = sLen; }
            }
            if (wasAct && evRem <= 0) { arm = false; gap = duty > 0.001f ? (float)evLen * (1.f - duty) / duty * (0.7f + 0.9f * rnd01(rng)) : (float)len; }
            // ---- space + level; REPLACE subtracts the looper's own playback under the slice
            float sg = ev * env * farG;
            lpY += (sg - lpY) * lpA; lpY = flush(lpY);
            float sp = lpY;
            if (gAP > 0.f) {
                float wd = ap1[a1i]; float w1 = sp + gAP * wd; ap1[a1i] = flush(w1); float y1 = wd - gAP * w1; if (++a1i >= AP1) a1i = 0;
                wd = ap2[a2i]; float w2 = y1 + gAP * wd; ap2[a2i] = flush(w2); float y2 = wd - gAP * w2; if (++a2i >= AP2) a2i = 0;
                sp += (y2 - sp) * apBlend;
            }
            float wet = limit(sp * lvl) * mix * onG;
            float sub = repl * mix * onG * env * nrm;
#ifdef HX_BLP_TELEMETRY
            { double d = wet - dbgWetPrev; dbgWetPrev = wet; dbgWetSq += (double)wet * wet; dbgWetDiffSq += d * d; }
#endif
            L[i] = finite(L[i]) + wet - sub;
            if (stereoIn) R[i] = finite(R[i]) + wet - sub;
            adv(ph, rate); tAbs++;
        }
        if (!on && onG <= 0.f && evRem <= 0) { if (idleN < idleMax) { idleN += n; if (idleN >= idleMax) { lpY = 0.f; memset(ap1, 0, AP1 * 4); memset(ap2, 0, AP2 * 4); } } }
        else idleN = 0;
        return stereoIn;
    }
};
inline Fx* fx_create_blooper(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxBlooper)); return m ? new (m) FxBlooper(a, sr) : nullptr; }
}

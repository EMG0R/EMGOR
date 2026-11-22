// fx_glitch.h -- HYPERTRUMPET V2 GLITCH: generative delay + bitcrush stutter machine. NEW 2026-11-17. Own work (Emory Smith / EMGOR).
// Design: ONE mono ring (2^17 samples = 2.73 s, 512 KB in the slot Arena) holds input + feedback. A seeded scheduler ticks on a musical grid
// (RATE ticks/s, scaled x0.5/1/2/4 by INTENS). Each tick may (a) fire an event with probability DENSITY*(.25+.75*INTENS), (b) start a crush burst.
// Events: STUTTER (freeze the last 1/2..1/16 grid step, loop it 2..REPMAX times, each loop optionally REVERSED and/or PITCHED +-12 st by read rate),
// JUMP (delay time jumps among a table of rhythmic lengths, the table widens with INTENS), DROP (wet gate). Crush bursts: bit depth 16 -> 3 and sample
// hold decimation 1 -> DECMAX, scaled to the input envelope (the mic signal is ~-45 dBFS, so the quantiser is level-relative, not absolute).
// Every voice change is a 2..5 ms smoothstep crossfade between up to 3 read voices (spawns are spaced >= one crossfade), duck/gate/crush mix ramp
// linearly over the same window: no clicks. Output wet = (live signal ducked during stutters) + voices, -> crush -> gate -> 2-pole TONE LP -> softclip.
// Feedback = the LP'd wet through an envelope-relative softclip back into the ring (glitches smear into echoes).
// Macro: INTENS (role AMT, FSR) = event speed + event probability + crush depth + jump/pitch/slice extremity. Output is mono (R = L).
// wake() zeroes ALL state incl. the ring (Seed SDRAM arenas are dirty); reset() = wake().
#pragma once
#include "fx_common.h"
namespace hx {
struct FxGlitch : Fx {
    enum { P_MIX = 0, P_INTENS, P_RATE, P_DENSITY, P_CRUSHMAX, P_DECMAX, P_REPMAX, P_REVPROB, P_PITPROB, P_DROPPROB,
           P_FEEDBACK, P_TONE, P_SEED, P_XFADE, P_DLYTIME, P_JUMPPROB };
    static const int NV = 3;
    struct Voice {
        bool on; bool tap; bool loop; int dir;       // dir +1 fading in / 0 steady / -1 fading out
        uint32_t anchor; float p, p0, rate, D, Dt, mult, lvl, t;   // t = fade position 0..1
        int left, len, repsLeft;
    };
    float* ring = nullptr; uint32_t mask = 0, w = 0;
    Voice vo[NV]; Voice pend; bool hasPend = false;
    uint32_t rng = 1, seedCur = 0xFFFFFFFFu, eventCount = 0, sinceStart = 0;
    float tickPh = 0.f, tapMult = 2.f, tapGridD = 0.f;
    int evTimer = 0, crushT = 0, dropT = 0, dcnt = 0;
    float dk = 0.f, dkT = 0.f, gt = 1.f, gtT = 1.f, cm = 0.f, held = 0.f, env = 0.f, fbPrev = 0.f, l1 = 0.f, l2 = 0.f;
    float crushLv = 2048.f; int crushDec = 1;
    // block-level params
    float pI = 0.f, tickInc = 0.1f, tickS = 1000.f, Tg = 12000.f, pEvent = 0.f, pCrushTick = 0.f, intr = 0.f, kmod = 1.f;
    float lpA0 = 0.3f, lpAc = 0.1f, fbG = 0.f, lpA = 0.3f, fbRef = 0.02f, fbInv = 50.f, k1 = 1.f, k2 = 1.f, xfInc = 0.01f, maxD = 100000.f;
    int xfN = 168;
    DryWet dw; float dryB[HX_MAXBLOCK], wetB[HX_MAXBLOCK];

    FxGlitch(Arena& a, float s) {
        name = "GLITCH"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_INTENS, "INTENS", 0.f, 1.f, 0.25f, 0.f, PF_PERCENT);       // macro (FSR)
        P(P_RATE, "RATE", 1.f, 16.f, 4.f, 0.f, PF_LOG);               // grid ticks/s (16th at 60 bpm = 4)
        P(P_DENSITY, "DENSITY", 0.f, 1.f, 0.7f, 0.f, PF_PERCENT);     // event probability ceiling
        P(P_CRUSHMAX, "CRUSHMAX", 0.f, 1.f, 0.8f, 0.f, PF_PERCENT);   // deepest crush at INTENS=1 (1 = 3 bit)
        P(P_DECMAX, "DECMAX", 1.f, 32.f, 16.f, 0.f, PF_LOG);          // max sample-rate decimation
        P(P_REPMAX, "REPMAX", 1.f, 16.f, 6.f, 1.f, PF_QUANT | PF_INT);// max stutter repeats
        P(P_REVPROB, "REVPROB", 0.f, 1.f, 0.25f, 0.f, PF_PERCENT);
        P(P_PITPROB, "PITPROB", 0.f, 1.f, 0.25f, 0.f, PF_PERCENT);
        P(P_DROPPROB, "DROPPROB", 0.f, 1.f, 0.2f, 0.f, PF_PERCENT);
        P(P_FEEDBACK, "FEEDBACK", 0.f, 0.92f, 0.45f, 0.f, PF_PERCENT);
        P(P_TONE, "TONE", 500.f, 16000.f, 6000.f, 0.f, PF_LOG);
        P(P_SEED, "SEED", 0.f, 9999.f, 1.f, 1.f, PF_QUANT | PF_INT);
        P(P_XFADE, "XFADE", 2.f, 5.f, 3.5f, 0.f);                      // ms
        P(P_DLYTIME, "DLYTIME", 0.5f, 8.f, 2.f, 0.f, PF_LOG);          // base delay, grid steps
        P(P_JUMPPROB, "JUMPPROB", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        setRole(R_AMT, P_INTENS); setRole(R_TIME, P_RATE); setRole(R_TONE, P_TONE); setRole(R_MOD, P_DENSITY); setRole(R_FB, P_FEEDBACK); setRole(R_PITCH, P_PITPROB);
        const uint32_t n = 131072u;
        ring = a.floats(n); mask = n - 1;
        if (!ring) { ok = false; return; }
        maxD = 2.4f * s; if (maxD > (float)(n - 4096)) maxD = (float)(n - 4096);
        wake();
    }
    void reset() override { wake(); }
    // zero ALL state (ring included); called by the ctor and by the firmware when the slot activates
    void wake() {
        if (!ring) return;
        memset(ring, 0, (size_t)(mask + 1) * sizeof(float));
        w = 0; hasPend = false; seedCur = 0xFFFFFFFFu; eventCount = 0; sinceStart = 0x7FFFFFFFu; tickPh = 0.f; tapMult = 2.f;
        evTimer = crushT = dropT = dcnt = 0; dk = dkT = 0.f; gt = gtT = 1.f; cm = held = env = fbPrev = l1 = l2 = 0.f; crushLv = 2048.f; crushDec = 1;
        memset(vo, 0, sizeof(vo)); memset(&pend, 0, sizeof(pend)); dw = DryWet();
        seedRng(); readParams();
        vo[0].on = true; vo[0].tap = true; vo[0].D = vo[0].Dt = tapGridD; vo[0].mult = tapMult; vo[0].lvl = 0.7f; vo[0].t = 1.f; vo[0].dir = 0;
    }
    void seedRng() { seedCur = (uint32_t)v[P_SEED]; rng = seedCur * 2654435761u + 0x9E3779B9u; if (!rng) rng = 1; for (int i = 0; i < 4; i++) rnd01(rng); }

    // ---- scheduler helpers (all control-rate, run only on ticks)
    void readParams() {
        pI = clampf(sm[P_INTENS], 0.f, 1.f);
        const float rate = clampf(sm[P_RATE], 1.f, 16.f);
        const float f = pI < 0.25f ? 0.5f : (pI < 0.5f ? 1.f : (pI < 0.75f ? 2.f : 4.f));
        tickInc = f * rate / sr; tickS = sr / (f * rate); Tg = sr / rate;
        pEvent = clampf(sm[P_DENSITY], 0.f, 1.f) * (0.25f + 0.75f * pI);
        pCrushTick = sm[P_CRUSHMAX] > 0.01f ? (0.08f + 0.55f * pI) : 0.f;
        intr = 0.5f * pI; kmod = 0.4f + 0.6f * pI;
        fbG = clampf(sm[P_FEEDBACK], 0.f, 0.92f);
        { const float fc = clampf(sm[P_TONE], 500.f, 0.45f * sr);   // while crushing, the LP also follows the decimation so the held steps are smoothed (lo-fi, not harsh)
          lpA0 = 1.f - expf(-kTwoPi * fc / sr);
          const float fcc = fminf(fc, fmaxf(1500.f, 0.6f * sr / (float)crushDec)); lpAc = 1.f - expf(-kTwoPi * fcc / sr); }
        const float xf = clampf(sm[P_XFADE], 2.f, 5.f);
        xfN = (int)(xf * 0.001f * sr); if (xfN < 8) xfN = 8; xfInc = 1.f / (float)xfN;
        tapGridD = clampf(tapMult * Tg, 4.f, maxD);
        for (int i = 0; i < NV; i++) if (vo[i].tap) vo[i].Dt = clampf(vo[i].mult * Tg, 4.f, maxD);   // each tap voice glides only toward ITS OWN grid length (RATE changes), never the new jump target
    }
    void commit(const Voice& nv) {
        for (int i = 0; i < NV; i++) if (vo[i].on && vo[i].dir >= 0) vo[i].dir = -1;
        int k = -1; for (int i = 0; i < NV; i++) if (!vo[i].on) { k = i; break; }
        if (k < 0) { k = 0; for (int i = 1; i < NV; i++) if (vo[i].t < vo[k].t) k = i; }
        vo[k] = nv; vo[k].on = true; vo[k].t = 0.f; vo[k].dir = 1; sinceStart = 0; hasPend = false;
        dkT = nv.tap ? 0.f : 1.f;
    }
    void startVoice(const Voice& nv) { if (sinceStart < (uint32_t)xfN) { pend = nv; hasPend = true; } else commit(nv); }
    Voice tapVoice() { Voice t; memset(&t, 0, sizeof(t)); t.tap = true; t.D = t.Dt = tapGridD; t.mult = tapMult; t.lvl = 0.7f; t.left = 0x7FFFFFFF; return t; }
    void fireEvent() {
        eventCount++;
        const float wS = 1.f, wJ = clampf(sm[P_JUMPPROB], 0.f, 1.f), wD = clampf(sm[P_DROPPROB], 0.f, 1.f) * kmod;
        float r = rnd01(rng) * (wS + wJ + wD);
        if (r < wS) {                                   // ---- STUTTER
            const int ki = (int)(rnd01(rng) * (2.f + 2.f * pI)); float S = Tg * ldexpf(0.5f, -ki);
            const bool rev = rnd01(rng) < clampf(sm[P_REVPROB], 0.f, 1.f) * kmod;
            const bool pit = rnd01(rng) < clampf(sm[P_PITPROB], 0.f, 1.f) * kmod;
            float rate = 1.f;
            if (pit) { static const float st[8] = { 7.f, -5.f, 12.f, -12.f, 5.f, -7.f, 3.f, -3.f };
                       int n = 2 + (int)(pI * 6.99f); rate = semiToRatio(st[(int)(rnd01(rng) * (float)n) % n]); }
            else rnd01(rng), rnd01(rng);               // keep rng consumption fixed
            const float Smin = 3.f * (float)xfN * rate + 8.f;
            if (S < Smin) S = Smin; if (S > 0.35f * sr) S = 0.35f * sr;
            const uint32_t Si = (uint32_t)S; const float Seff = (float)Si - 4.f, loopDur = Seff / rate;
            int reps = 1 + (int)((sm[P_REPMAX] - 1.f) * (0.25f + 0.75f * pI) * rnd01(rng) + 0.5f);
            int cap = (int)(2.f * sr / loopDur); if (cap < 1) cap = 1; if (reps > cap) reps = cap; if (reps < 1) reps = 1;
            Voice nv; memset(&nv, 0, sizeof(nv)); nv.loop = true; nv.anchor = w - Si; nv.rate = rev ? -rate : rate;
            nv.p = rev ? (float)Si - 3.f : 1.f; nv.p0 = nv.p; nv.left = nv.len = (int)loopDur; nv.repsLeft = reps - 1; nv.lvl = 1.f;
            evTimer = (int)((float)reps * loopDur); startVoice(nv);
        } else if (r < wS + wJ) {                       // ---- JUMP of delay time
            const float tb[10] = { sm[P_DLYTIME], 2.f, 1.f, 3.f, 0.5f, 4.f, 0.25f, 6.f, 8.f, 0.125f };
            tapMult = tb[(int)(rnd01(rng) * (float)(2 + (int)(pI * 7.99f))) % (2 + (int)(pI * 7.99f))];
            tapGridD = clampf(tapMult * Tg, 4.f, maxD);
            evTimer = (int)tickS; startVoice(tapVoice());
        } else {                                        // ---- DROP
            int d = (int)((1.f + 2.f * rnd01(rng)) * tickS); if (d < 3 * xfN) d = 3 * xfN;
            gtT = 0.f; dropT = d; evTimer = d;
        }
    }
    void onTick() {
        if (crushT <= 0 && rnd01(rng) < pCrushTick) {
            const float d = clampf(sm[P_CRUSHMAX], 0.f, 1.f) * (0.1f + 0.9f * pI) * (0.4f + 0.6f * rnd01(rng));
            crushLv = exp2f(15.f - 13.f * d); crushDec = (int)(powf(clampf(sm[P_DECMAX], 1.f, 32.f), d * sqrtf(d)) + 0.5f);
            if (crushDec < 1) crushDec = 1;
            crushT = (int)((1.f + 3.f * rnd01(rng)) * tickS); if (crushT < 2 * xfN) crushT = 2 * xfN;
        }
        if (evTimer > 0 && !(rnd01(rng) < intr)) return;
        if (rnd01(rng) >= pEvent) return;
        fireEvent();
    }
    static inline float herm(const float* b, uint32_t m, uint32_t i0, float f) {
        const float y0 = b[(i0 - 1) & m], y1 = b[i0 & m], y2 = b[(i0 + 1) & m], y3 = b[(i0 + 2) & m];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if ((uint32_t)v[P_SEED] != seedCur) seedRng();
        readParams();
        { const float sc = fmaxf(env, 0.004f); k1 = crushLv / sc; k2 = sc / crushLv; }
        fbRef = fmaxf(4.f * env, 0.02f); fbInv = 1.f / fbRef;
        const float sp = xfInc, cmA = xfInc;
        for (int i = 0; i < n; i++) {
            const float x = stereoIn ? 0.5f * (L[i] + R[i]) : L[i];
            dryB[i] = x;
            tickPh += tickInc; if (tickPh >= 1.f) { tickPh -= 1.f; onTick(); }
            if (evTimer > 0) --evTimer;
            if (sinceStart < 0x7FFFFFF0u) ++sinceStart;
            if (hasPend && sinceStart >= (uint32_t)xfN) commit(pend);
            // ---- voices
            float vs = 0.f; bool wantTap = false, wantLoop = false; Voice nextLoop;
            for (int k = 0; k < NV; k++) {
                Voice& s = vo[k]; if (!s.on) continue;
                float y;
                if (s.tap) {
                    if (s.D != s.Dt) { s.D += (s.Dt - s.D) * 0.0004f; if (fabsf(s.Dt - s.D) < 0.01f) s.D = s.Dt; }
                    const float fd = floorf(s.D); const uint32_t Di = (uint32_t)fd; const float fr = s.D - fd;
                    y = fr > 0.f ? herm(ring, mask, w - Di - 1, 1.f - fr) : ring[(w - Di) & mask];
                } else {
                    const float fp = floorf(s.p);
                    y = herm(ring, mask, s.anchor + (uint32_t)(int32_t)fp, s.p - fp);
                    s.p += s.rate;
                    if (s.dir >= 0 && --s.left == xfN) {
                        if (s.repsLeft > 0) { nextLoop = s; wantLoop = true; }
                        else wantTap = true;
                    } else if (s.dir < 0) --s.left;
                    if (s.left <= 0) { s.on = false; continue; }
                }
                const float g = s.t * s.t * (3.f - 2.f * s.t);
                vs += s.lvl * g * y;
                if (s.dir > 0) { s.t += sp; if (s.t >= 1.f) { s.t = 1.f; s.dir = 0; } }
                else if (s.dir < 0) { s.t -= sp; if (s.t <= 0.f) { s.t = 0.f; s.on = false; } }
            }
            if (wantLoop) { // restart the frozen segment from its start with one repeat fewer
                Voice nl = nextLoop; nl.p = nl.p0; nl.left = nl.len; nl.repsLeft = nextLoop.repsLeft - 1; startVoice(nl);
            } else if (wantTap) { evTimer = 0; startVoice(tapVoice()); }
            // ---- duck / gate / crush
            dk += clampf(dkT - dk, -sp, sp);
            if (dropT > 0) { if (--dropT == 0) gtT = 1.f; }
            gt += clampf(gtT - gt, -sp, sp);
            if (crushT > 0) --crushT;
            const float cT = crushT > 0 ? 1.f : 0.f; cm += clampf(cT - cm, -cmA, cmA);
            float sum = x * (1.f - dk) + vs;
            { const float a = fabsf(sum); env = a > env ? env + (a - env) * 0.05f : flush(env * 0.9999f); }
            if (cm > 0.f) {
                if (++dcnt >= crushDec) { dcnt = 0; held = sum; }
                const float q = held * k1; const float qi = (float)(int)(q + (q >= 0.f ? 0.5f : -0.5f));
                sum += cm * (qi * k2 - sum);
            } else dcnt = 0;
            sum *= gt;
            const float lpA = cm > 0.f ? lpAc : lpA0;
            l1 += (sum - l1) * lpA; l1 = flush(l1); l2 += (l1 - l2) * lpA; l2 = flush(l2);
            wetB[i] = softclip(l2);
            ring[w] = flush(x + fbG * fbPrev); w = (w + 1) & mask;
            fbPrev = fbRef * softclip(l2 * fbInv);
        }
        dw.apply(L, dryB, wetB, n, sm[P_MIX]);
        memcpy(R, L, (size_t)n * sizeof(float));
        return true;
    }
};
inline Fx* fx_create_glitch(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxGlitch)); return m ? new (m) FxGlitch(a, sr) : nullptr; }
}

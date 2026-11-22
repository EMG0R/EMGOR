// fx_reverb.h -- HYPERTRUMPET V2 "reverb" v2: SCATTER -> diffusion -> FDN. Lush, unpredictable, long. Own algorithm, no third-party code.
// Replaces v1's 3x ValhallaSupermassive; aims at Supermassive's chaotic modes (little echoes blooming into a huge shimmering tail).
//
// Topology (mono in, decorrelated stereo out, L+R sums fine to mono):
//   in -> low-cut -> ring (predelay + scatter source)
//   SCATTER : NT (8) taps read from the ring at randomised times 20..400 ms (drawn per SEED, scaled by SPREAD), each with a slowly
//             wandering time (smooth random walk, rate DRIFT, per-sample interpolated -> no zipper), random gain/polarity/pan.
//             Taps feed the FDN lines directly (each tap -> its own line) AND the output (the audible "little delays").
//   diffusion: 6 allpass cascade (g .55..65), every delay modulated by its own random walk (MODDEPTH) -> FDN input.
//   FDN     : N lines (default 6), Householder feedback with a wandering mix coefficient c (I - c*2/N*11^T, eigenvalues in [-1,1-2c]
//             => always contractive), per-line one-pole damping with a wandering cutoff, per-line independent modulation =
//             0.65 random walk + 0.35 sine (MODDEPTH / MODRATE). Reads use per-sample linearly-interpolated delay, so WARP (all line
//             lengths x(1+1.5 warp), 70 ms slew) glides the tail in pitch with no stair-steps/clicks.
//   BLOOM   : the FDN output gain is pulled down by input onsets and swells back (tau ~1.6 s) as the tail density builds.
//   CHAOS   : depth of the random variation of damping + matrix mix.
// Stability: all gains contractive (fb <= .995 on a symmetric matrix with |eig| <= 1; damping <= 1; interpolation gain <= 1; allpasses |g|<1).
// wake() (public) zeroes ALL state and buffers (the Seed's SDRAM arena is not zeroed). Compile -DHX_REVERB_LINES=4|6|8 (-DHX_REVERB_LITE = 4).
#pragma once
#include "fx_common.h"
namespace hx {
#ifdef HX_REVERB_LITE
  #define HX_REVERB_LINES 4
#endif
#ifndef HX_REVERB_LINES
  #define HX_REVERB_LINES 6
#endif
#ifndef HX_REVERB_AP
  #define HX_REVERB_AP 6
#endif
#ifndef HX_REVERB_TAPS
  #define HX_REVERB_TAPS 8
#endif
static inline float oneMinusExp(float x) { return x * (1.f - 0.5f * x + 0.1667f * x * x); }   // 1-exp(-x), x < ~0.1 (control-rate coefficients)
struct FxReverb : Fx {
    enum { N = HX_REVERB_LINES, NAP = HX_REVERB_AP, NT = HX_REVERB_TAPS, SB = 24 };
    enum { P_MIX, P_DECAY, P_SIZE, P_WARP, P_DAMP, P_MODD, P_MODR, P_LOWCUT, P_WIDTH, P_PRE,
           P_SCAT, P_SPREAD, P_DRIFT, P_SEED, P_BLOOM, P_CHAOS };
    // smooth random walk: random target every ~1/rate s (+-50%), two cascaded one-poles -> C1-continuous, output ~ +-0.7
    struct Wander {
        float t = 0.f, y1 = 0.f, y2 = 0.f; int cnt = 0; uint32_t s = 1;
        inline float step(int sb, float a, float ivl) {
            cnt -= sb;
            if (cnt <= 0) { t = rnd01(s) * 2.f - 1.f; cnt = (int)(ivl * (0.5f + rnd01(s))); if (cnt < sb) cnt = sb; }
            y1 += (t - y1) * a; y2 += (y1 - y2) * a; return y2; }
    };
    DelayLine dl[N]; DelayLine pre;                       // pre = predelay AND scatter source
    float lp[N], ph[N], rmul[N], baseLen[N], maxLen[N], sL[N], sR[N], inj[N];
    Wander wl[N], wd[N], wt[NT], wa[NAP]; Wander wm;
    // scatter state: targets (per SEED), slewed live values
    float tgtT[NT], rg[NT], rpl[NT], rpr[NT];             // target time (samples), raw feed gain, raw pan L/R
    float curT[NT], gF[NT], gL[NT], gR[NT], tprev[NT], tmul[NT]; uint8_t tline[NT];
    float* apBuf[NAP]; uint32_t apMask[NAP], apW[NAP]; float apBase[NAP], apG[NAP], apPrev[NAP];
    float adn, sc, hpz, bgS, bloomB, envF, envS, cmixPrev; float dprev[N];
    bool primed, fresh, seeded; int curSeed;
    DryWet dwL, dwR;
    float cSlow = 0.f, cGain = 0.f, cBg = 0.f, cBloom = 0.f, cEF = 0.f, cES = 0.f;
    float arate = 0.f;      // per-sample slew coefficient of the warp scale
    float memBytes = 0.f;

    FxReverb(Arena& a, float s) {
        name = "REVERB"; sr = s;
        P(P_MIX,   "MIX",      0.f, 1.f, 0.4f, 0.f, PF_PERCENT);
        P(P_DECAY, "DECAY",    0.f, 0.99f, 0.975f, 0.f, PF_PERCENT);
        P(P_SIZE,  "SIZE",     0.3f, 2.f, 1.f);
        P(P_WARP,  "WARP",     0.f, 1.f, 0.f, 0.f, PF_PERCENT);
        P(P_DAMP,  "DAMP",     500.f, 18000.f, 5200.f, 0.f, PF_LOG);
        P(P_MODD,  "MODDEPTH", 0.f, 1.f, 0.6f, 0.f, PF_PERCENT);
        P(P_MODR,  "MODRATE",  0.05f, 5.f, 0.4f, 0.f, PF_LOG);
        P(P_LOWCUT,"LOWCUT",   20.f, 1000.f, 90.f, 0.f, PF_LOG);
        P(P_WIDTH, "WIDTH",    0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_PRE,   "PREDELAY", 0.f, 200.f, 10.f, 0.f);
        P(P_SCAT,  "SCATTER",  0.f, 1.f, 0.65f, 0.f, PF_PERCENT);
        P(P_SPREAD,"SPREAD",   0.f, 1.f, 0.8f, 0.f, PF_PERCENT);
        P(P_DRIFT, "DRIFT",    0.02f, 2.f, 0.12f, 0.f, PF_LOG);
        P(P_SEED,  "SEED",     0.f, 99.f, 7.f, 1.f, PF_INT);
        P(P_BLOOM, "BLOOM",    0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_CHAOS, "CHAOS",    0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        setRole(R_AMT, P_MIX); setRole(R_TIME, P_SIZE); setRole(R_TONE, P_DAMP); setRole(R_MOD, P_WARP); setRole(R_FB, P_DECAY);

        static const float L8[8] = {1087, 1283, 1499, 1733, 1993, 2287, 2579, 2927};   // primes @48k
        static const float L6[6] = {1103, 1409, 1753, 2161, 2663, 3203};
        static const float L4[4] = {1283, 1733, 2287, 2927};
        static const uint16_t AP[6] = {73, 113, 181, 271, 383, 509};                // primes (~ 1.5 ms .. 10.6 ms)
        const float k = sr / 48000.f; bool good = true;
        for (int i = 0; i < N; i++) {
            baseLen[i] = (N == 8 ? L8[i] : (N == 6 ? L6[i] : L4[i])) * k;
            uint32_t mx = (uint32_t)(baseLen[i] * 5.f + 32.f);          // size 2 x warp 2.5 + mod
            good &= dl[i].init(a, mx);
            maxLen[i] = (float)dl[i].size() - 4.f;
            sL[i] = (i & 1) ? -1.f : 1.f; sR[i] = ((i >> 1) & 1) ? -1.f : 1.f; inj[i] = ((i >> 2) & 1) ? -1.f : 1.f;
            rmul[i] = 0.77f + 0.061f * i * (8.f / N);
        }
        good &= pre.init(a, (uint32_t)(0.2f * sr + 0.465f * sr) + 8);
        for (int j = 0; j < NAP; j++) {
            apBase[j] = AP[j] * k; apG[j] = (j & 1) ? 0.62f : 0.55f;
            uint32_t sz = nextPow2((uint32_t)(apBase[j] + 24.f)); apBuf[j] = a.floats(sz); apMask[j] = sz - 1; good &= apBuf[j] != nullptr; }
        ok = good;
        float tot = pre.size() * 4.f; for (int i = 0; i < N; i++) tot += dl[i].size() * 4.f;
        for (int j = 0; j < NAP; j++) tot += (apMask[j] + 1) * 4.f; memBytes = tot;
        arate = 1.f - expf(-1.f / (0.07f * sr));
        cSlow = 1.f - expf(-(float)SB / (0.5f * sr)); cGain = 1.f - expf(-(float)SB / (0.12f * sr)); cBg = 1.f - expf(-(float)SB / (0.04f * sr));
        cBloom = 1.f - expf(-(float)SB / (1.6f * sr)); cEF = 1.f - expf(-(float)SB / (0.02f * sr)); cES = 1.f - expf(-(float)SB / (0.5f * sr));
        for (int k2 = 0; k2 < NT; k2++) tmul[k2] = 0.7f + 0.6f * (float)((k2 * 5) % NT) / (float)NT;
        wakeState();
    }
    // all scalar/array state (buffers are zeroed separately). Also seeds the random walkers deterministically.
    void wakeState() {
        for (int i = 0; i < N; i++) { lp[i] = 0.f; ph[i] = 0.137f * i; dprev[i] = baseLen[i]; wl[i] = Wander(); wd[i] = Wander(); wl[i].s = 0x9E3779B9u * (i + 1); wd[i].s = 0x85EBCA6Bu * (i + 3); }
        for (int j = 0; j < NAP; j++) { apW[j] = 0; apPrev[j] = apBase[j]; wa[j] = Wander(); wa[j].s = 0xC2B2AE35u * (j + 5); }
        for (int k = 0; k < NT; k++) { tgtT[k] = curT[k] = tprev[k] = 4000.f; rg[k] = rpl[k] = rpr[k] = gF[k] = gL[k] = gR[k] = 0.f; tline[k] = (uint8_t)(k % N); wt[k] = Wander(); wt[k].s = 0x27D4EB2Fu * (k + 7); }
        wm = Wander(); wm.s = 0x165667B1u;
        adn = 1e-18f; sc = 1.f; hpz = 0.f; envAcc = 0.f; bgS = 1.f; bloomB = 1.f; envF = envS = 0.f; cmixPrev = 1.f;
        primed = false; fresh = true; seeded = false; curSeed = -1; dwL = DryWet(); dwR = DryWet();
    }
    // PUBLIC: zero everything (the arena is dirty SDRAM on the Seed) so the next block starts from silence. Immediate: call while idle.
    void wake() {
        if (!ok) return;
        for (int i = 0; i < N; i++) memset(dl[i].buf, 0, dl[i].size() * 4);
        memset(pre.buf, 0, pre.size() * 4);
        for (int j = 0; j < NAP; j++) memset(apBuf[j], 0, (apMask[j] + 1) * 4);
        for (int i = 0; i < N; i++) dl[i].w = 0; pre.w = 0;
        wakeState();
    }
    void reset() override { wake(); }

    // SEED -> tap table: times log-spread 20..400 ms with jitter, random magnitude/polarity/pan, later taps quieter
    void drawTaps(int seed) {
        uint32_t s = (uint32_t)seed * 2654435761u + 0x1234567u; for (int i = 0; i < 4; i++) rnd01(s);
        const int off = (int)(rnd01(s) * N);
        for (int k = 0; k < NT; k++) {
            float u = rnd01(s);
            tgtT[k] = 0.020f * powf(20.f, ((float)k + u) / (float)NT) * sr;
            float mag = (0.4f + 0.6f * rnd01(s)) * (1.f - 0.5f * (float)k / (float)NT);
            rg[k] = (rnd01(s) < 0.5f ? -1.f : 1.f) * mag;
            float pan = rnd01(s) * 1.5707963f; rpl[k] = cosf(pan); rpr[k] = sinf(pan);
            tline[k] = (uint8_t)((k + off) % N);
        }
        curSeed = seed; seeded = true;
    }

    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        const int seedI = (int)(sm[P_SEED] + 0.5f);
        if (!seeded || seedI != curSeed) drawTaps(seedI);
        const float fb = clampf(sm[P_DECAY], 0.f, 0.995f);
        const float gin = sqrtf(1.f - fb * fb) * 1.5f;                // input comp: keeps level ~constant vs DECAY
        const float scT = sm[P_SIZE] * (1.f + 1.5f * sm[P_WARP]);
        if (!primed) { sc = scT; primed = true; }
        const float damp = 1.f - expf(-kTwoPi * clampf(sm[P_DAMP], 200.f, 0.45f * sr) / sr);
        const float hpa = 1.f - expf(-kTwoPi * sm[P_LOWCUT] / sr);
        const float chaos = sm[P_CHAOS];
        const float depth = sm[P_MODD] * 20.f * (sr / 48000.f) * (0.5f + 0.5f * sm[P_SIZE]);
        const float mrate = sm[P_MODR];
        const float rinc = mrate / sr;
        const float pd = clampf(sm[P_PRE] * 0.001f * sr, 1.f, 0.2f * sr);
        const uint32_t pdi = (uint32_t)pd; const float pdf = pd - (float)pdi;
        const float width = sm[P_WIDTH], ts = 0.707f;
        const float h2 = 2.f / (float)N;
        const float amt = sm[P_SCAT], spreadMul = 0.25f + 0.9f * sm[P_SPREAD], drift = sm[P_DRIFT], bloom = sm[P_BLOOM];
        // random-walk smoothing coefficients (tau = interval/3), once per block
        float aL[N], aD, aM, aT[NT], aA[NAP], ivL[N];
        for (int i = 0; i < N; i++) { float r = mrate * rmul[i]; aL[i] = oneMinusExp(3.f * SB * r / sr); ivL[i] = sr / r; }
        aD = oneMinusExp(3.f * SB * 0.11f / sr); aM = oneMinusExp(3.f * SB * 0.07f / sr);
        for (int k = 0; k < NT; k++) aT[k] = oneMinusExp(3.f * SB * drift * tmul[k] / sr);
        for (int j = 0; j < NAP; j++) aA[j] = oneMinusExp(3.f * SB * (0.15f + 0.05f * j) * (0.5f + sm[P_MODR]) / sr);
        const float aSlow = cSlow, aGain = cGain, aBg = cBg, aBloom = cBloom, aEF = cEF, aES = cES;
        float mt = sm[P_MIX]; if (!dwL.primed) { dwL.cur = mt; dwL.primed = true; } if (!dwR.primed) { dwR.cur = mt; dwR.primed = true; }
        const float minc = (mt - dwL.cur) / (float)n; float mm = dwL.cur;
        float dcur[N], dd[N], dmp[N], apD[NAP], td[NT], tdd[NT];
        const float* hb[N]; for (int i = 0; i < N; i++) hb[i] = dl[i].buf;
        for (int s0 = 0; s0 < n; s0 += SB) {
            const int sb = (n - s0 < SB) ? (n - s0) : SB; const float isb = 1.f / (float)sb;
            sc += (scT - sc) * (sb * arate * (1.f - 0.5f * (float)(sb - 1) * arate));
            // ---- control: FDN lines
            const float wmv = wm.step(sb, aM, sr / 0.07f);
            const float cmix = 1.f - chaos * 0.4f * (0.5f + 0.7f * wmv);                 // 0.6..1 at chaos 1 (never above 1: contractive)
            const float sumK = clampf(cmix, 0.5f, 1.f) * h2;
            for (int i = 0; i < N; i++) {
                float wv = wl[i].step(sb, aL[i], ivL[i]);
                float lfo = sinPar(ph[i]); ph[i] += rinc * rmul[i] * (float)sb; if (ph[i] >= 1.f) ph[i] -= 1.f;
                float len = baseLen[i] * sc + depth * (0.9f * wv + 0.35f * lfo); len = clampf(len, 3.f, maxLen[i]);
                if (fresh) dprev[i] = len;
                dcur[i] = dprev[i]; dd[i] = (len - dprev[i]) * isb; dprev[i] = len;
                float dv = wd[i].step(sb, aD, sr / 0.11f);
                dmp[i] = clampf(damp * (1.f + chaos * 0.9f * dv), 0.004f, 0.98f);          // wandering damping cutoff
            }
            // ---- control: allpass delays
            for (int j = 0; j < NAP; j++) {
                float wv = wa[j].step(sb, aA[j], sr / (0.15f + 0.05f * j));
                float d = clampf(apBase[j] + wv * depth * 0.45f, 4.f, apBase[j] + 20.f);
                if (fresh) apPrev[j] = d; apD[j] = d; apPrev[j] = d;
            }
            // ---- control: scatter taps
            const float ringMax = (float)pre.size() - 4.f;
            for (int k = 0; k < NT; k++) {
                const float tgt = tgtT[k] * spreadMul;
                if (fresh) curT[k] = tgt; else curT[k] += (tgt - curT[k]) * aSlow;
                float wv = wt[k].step(sb, aT[k], sr / (drift * tmul[k]));
                float dep = 0.002f * sr + 0.012f * curT[k];                                   // +-(2 ms + 1.2 % of the time)
                float tot = clampf(pd + curT[k] + wv * 1.3f * dep, 2.f, ringMax);
                if (fresh) tprev[k] = tot;
                td[k] = tprev[k]; tdd[k] = (tot - tprev[k]) * isb; tprev[k] = tot;
                const float g = rg[k] * amt;
                const float tF = g * 0.8f, tL = g * rpl[k] * 1.3f, tR = g * rpr[k] * 1.3f;
                if (fresh) { gF[k] = tF; gL[k] = tL; gR[k] = tR; } else { gF[k] += (tF - gF[k]) * aGain; gL[k] += (tL - gL[k]) * aGain; gR[k] += (tR - gR[k]) * aGain; }
            }
            // ---- control: bloom (onset pulls the tail level down, density build swells it back)
            float bgT;
            { float e = envAcc * isb; envAcc = 0.f; envF += (e - envF) * (e > envF ? aEF : aEF * 0.25f); envS += (e - envS) * aES;
              float on = envF - 1.8f * envS - 2e-4f; if (on > 0.f) { on = on / (envF + 1e-9f); bloomB -= bloomB * on * 0.35f; }
              bloomB += (1.f - bloomB) * aBloom; envF += 1e-20f; envS += 1e-20f;
              bgT = 1.f - bloom * 0.65f * (1.f - bloomB); }
            float bg = bgS; bgS += (bgT - bgS) * (fresh ? 1.f : aBg); if (fresh) bg = bgS; const float bgInc = (bgS - bg) * isb;
            fresh = false;
            const float xScat = 1.f - 0.4f * amt;
            for (int s = s0; s < s0 + sb; s++) {
                const float dL = L[s], dR = stereoIn ? R[s] : dL;
                float x = 0.5f * (dL + dR) * gin + adn; adn = -adn;   // +-1e-18 keeps filter / line states out of the denormal range
                hpz += hpa * (x - hpz); x -= hpz;
                envAcc += fabsf(x);
                pre.write(x);
                { uint32_t p = pre.w - 1 - pdi; x = pre.buf[p & pre.mask] + pdf * (pre.buf[(p - 1) & pre.mask] - pre.buf[p & pre.mask]); }
                // scatter taps (per-sample interpolated, wandering times)
                float fi[N]; for (int i = 0; i < N; i++) fi[i] = 0.f;
                float oSL = 0.f, oSR = 0.f;
                { const float* pb = pre.buf; const uint32_t pm = pre.mask, pw = pre.w - 1;
                  for (int k = 0; k < NT; k++) {
                    const float dly = td[k]; td[k] = dly + tdd[k]; const uint32_t di = (uint32_t)dly; const float f = dly - (float)di; const uint32_t p = pw - di;
                    const float a0 = pb[p & pm]; const float v = a0 + f * (pb[(p - 1) & pm] - a0);
                    fi[tline[k]] += gF[k] * v; oSL += gL[k] * v; oSR += gR[k] * v; } }
                // modulated allpass cascade
                for (int j = 0; j < NAP; j++) {
                    float* b = apBuf[j]; const uint32_t m = apMask[j]; const float dly = apD[j]; const uint32_t di = (uint32_t)dly; const float f = dly - (float)di;
                    const uint32_t p = apW[j] - di; const float z0 = b[p & m]; const float z = z0 + f * (b[(p - 1) & m] - z0);
                    const float v = x - apG[j] * z; b[apW[j] & m] = v; apW[j]++; x = z + apG[j] * v; }
                // FDN
                float o[N], sum = 0.f, oL = 0.f, oR = 0.f;
                for (int i = 0; i < N; i++) {
                    const float* b = hb[i]; const uint32_t m = dl[i].mask; const float dly = dcur[i]; dcur[i] = dly + dd[i];
                    const uint32_t di = (uint32_t)dly; const float f = dly - (float)di; const uint32_t p = dl[i].w - 1 - di;
                    const float y0 = b[p & m]; float y = y0 + f * (b[(p - 1) & m] - y0);
                    lp[i] += (y - lp[i]) * dmp[i]; y = lp[i];
                    o[i] = y; sum += y; oL += sL[i] * y; oR += sR[i] * y;
                }
                sum *= sumK; const float xi = x * 1.75f * xScat;
                for (int i = 0; i < N; i++) dl[i].write(xi * inj[i] + fi[i] + fb * (o[i] - sum));
                oL = oL * bg + oSL; oR = oR * bg + oSR; bg += bgInc;
                float m = 0.5f * (oL + oR) * ts, sd = 0.5f * (oL - oR) * ts * width;
                float uL = 0.5f * (m + sd), uR = 0.5f * (m - sd);     // output limiter: cubic, unity small-signal gain, no division
                uL = clampf(uL, -1.f, 1.f); uR = clampf(uR, -1.f, 1.f);
                const float wl_ = 2.f * (uL - uL * uL * uL * (1.f / 3.f)), wr_ = 2.f * (uR - uR * uR * uR * (1.f / 3.f));
                L[s] = dL + (wl_ - dL) * mm; R[s] = dR + (wr_ - dR) * mm; mm += minc;
            }
        }
        dwL.cur = mt; dwR.cur = mt;
        return true;
    }
    float envAcc = 0.f;
};
inline Fx* fx_create_reverb(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxReverb)); return m ? new (m) FxReverb(a, sr) : nullptr; }
}

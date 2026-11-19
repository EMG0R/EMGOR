// fx_grain.h -- HYPERTRUMPET V2 grain delay.
// Re-implementation (clean C++, not a copy of the .csd) of the algorithm of Iain McCurdy's "DelayGrain.csd" (Csound / Cabbage,
// McCurdy 2013), licensed CC BY-NC-SA 4.0 (https://creativecommons.org/licenses/by-nc-sa/4.0/). Changes: fixed voice pool with a
// hard cap, density-compensated grain amplitude, grains start immediately at their delay offset instead of being scheduled
// later, linear-interpolated reads (as Csound tablei), one-pole per-grain filter instead of butlp, an added OCTUP stage.
// As a derivative this file is CC BY-NC-SA 4.0 too (non-commercial, share-alike, attribution to Iain McCurdy).
// Algorithm: stereo ring buffer; a metro-style scheduler fires grains at DENS/s; each grain draws a random size (GSIZE1..2),
// delay (19.4 ms..DELAY, exponential/linear/log distribution), reverse (REV probability), pan (PAN spread, linear L/R),
// amplitude (AMPSPRD, plus delay->amplitude decay DECAY), one-pole cutoff (FILT) and plays a window (Hann/half-sine/
// decay/decay-exp/gate) while reading the buffer at the transposition rate. When the rate > 1 the start delay is raised so the
// read head never overtakes the write head (as in the original).
// OCTUP: probability (0..1) that a grain is additionally transposed +12 st (v1 gated its octave-up AU 6 % of the time), done by
// per-grain playback rate. Actions: A_MAIN = next window shape, A_ALT = next delay distribution, A_CLEAR = clear buffer.
#pragma once
#include "fx_common.h"
#ifndef HX_GRAIN_VOICES
#define HX_GRAIN_VOICES 16     // budget: 16 voices (was 24); the density cap and the overlap-compensated gain follow it
#endif
namespace hx {
struct FxGrain : Fx {
    enum { P_MIX = 0, P_GSIZE1, P_GSIZE2, P_DENS, P_DELAY, P_TRANSP, P_PAN, P_AMPSPRD, P_REV, P_OCTUP, P_DECAY, P_FILT };
    static const int WT = 1024;                 // window table size (+1 guard)
    static constexpr float MIN_DLY_MS = 19.4f;  // v1 Dly1
    struct Voice { float d, dInc, wph, wInc, gL, gR, lpA, lpL, lpR; int left; int off; };   // off: first sample of the current block this voice sounds from (spawned mid-block)
    float* bufL = nullptr; float* bufR = nullptr; uint32_t mask = 0, w = 0;
    float win[5][WT + 1];
    Voice vc[HX_GRAIN_VOICES]; int nv = 0, maxSeen = 0;
    float prevL = 0.f, prevR = 0.f; float acc = 0.f; uint32_t rng = 0xBEEF123u;
    volatile uint8_t winSel = 0, dstSel = 0; volatile bool clearReq = false;
    DryWet dwL, dwR; float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK];

    FxGrain(Arena& a, float s) {
        name = "GRAIN"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_GSIZE1, "GSIZE1", 5.f, 500.f, 14.8f, 0.f, PF_LOG);    // ms
        P(P_GSIZE2, "GSIZE2", 5.f, 500.f, 84.8f, 0.f, PF_LOG);
        P(P_DENS, "DENS", 0.2f, 2000.f, 500.f, 0.f, PF_LOG);      // grains/s (voice cap limits the real rate)
        P(P_DELAY, "DELAY", 20.f, 2000.f, 1012.f, 0.f, PF_LOG);   // max delay ms
        P(P_TRANSP, "TRANSP", -12.f, 12.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_PAN, "PAN", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_AMPSPRD, "AMPSPRD", 0.f, 1.f, 0.8f, 0.f, PF_PERCENT);
        P(P_REV, "REV", 0.f, 1.f, 0.7f, 0.f, PF_PERCENT);
        P(P_OCTUP, "OCTUP", 0.f, 1.f, 0.06f, 0.f, PF_PERCENT);
        P(P_DECAY, "DECAY", 0.f, 1.f, 0.536f, 0.f, PF_PERCENT);
        P(P_FILT, "FILT", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        setRole(R_AMT, P_MIX); setRole(R_TIME, P_GSIZE2); setRole(R_TONE, P_FILT); setRole(R_MOD, P_DELAY); setRole(R_PITCH, P_TRANSP);
        uint32_t n = nextPow2((uint32_t)(2.0f * s) + 8);
        bufL = a.floats(n); mask = n - 1;                      // budget: ONE mono grain buffer (L+R summed); the stereo image comes from the per-grain pan
        if (!bufL) ok = false;
        buildWindows();
    }
    void buildWindows() {
        for (int i = 0; i <= WT; i++) {
            float x = (float)i / WT;
            win[0][i] = 0.5f - 0.5f * cosf(kTwoPi * x);                              // Hanning
            win[1][i] = sinf(kPi * x);                                               // half sine
            win[2][i] = x < 0.0234f ? x / 0.0234f : (1.f - x) / (1.f - 0.0234f);    // percussive, straight segments
            { float t = x < 0.0234f ? x / 0.0234f : 1.f;
              win[3][i] = x < 0.0234f ? t : expf(-6.f * (x - 0.0234f) / (1.f - 0.0234f)) * (1.f - x * 0.0f); }   // percussive, exponential
            win[4][i] = x < 0.027f ? x / 0.027f : (x > 0.973f ? (1.f - x) / 0.027f : 1.f);                       // gate with ramps
        }
        win[3][WT] = 0.f;
    }
    void reset() override { memset(bufL, 0, (mask + 1) * 4); nv = 0; prevL = prevR = 0.f; }   // immediate, main loop while idle
    void action(int id, float) override {
        if (id == A_MAIN) winSel = (uint8_t)((winSel + 1) % 5);
        else if (id == A_ALT) dstSel = (uint8_t)((dstSel + 1) % 3);
        else if (id == A_CLEAR) clearReq = true;
    }
    int activeVoices() const { return nv; }
    int maxVoicesSeen() const { return maxSeen; }

    inline float shapeDly(float x) {   // McCurdy expcurve/logcurve steepness 100
        if (dstSel == 0) return (exp2f(6.643856f * x) - 1.f) * (1.f / 99.f);
        if (dstSel == 2) return 1.f - (exp2f(6.643856f * (1.f - x)) - 1.f) * (1.f / 99.f);
        return x;
    }
    int spawnOff = 0; float rtoBlk = 1.f, octGate = 0.f, octTgt = 0.f, octTimer = 0.f;   // octGate: smoothed octave-up gate 0..1   // rtoBlk: transposition ratio, computed once per block
    void spawn(float sizeMs1, float sizeMs2, float dlyMaxMs, float transp, float pan, float ampSp, float rev, float octUp, float decay, float filt) {
        if (nv >= HX_GRAIN_VOICES) return;
        const float cap = (float)(mask - 8);
        float s1 = fminf(sizeMs1, sizeMs2), s2 = fmaxf(sizeMs1, sizeMs2);
        float N = (s1 + (s2 - s1) * rnd01(rng)) * 0.001f * sr; if (N < 16.f) N = 16.f;
        float dMin = MIN_DLY_MS * 0.001f * sr, dMax = fmaxf(dlyMaxMs * 0.001f * sr, dMin + 1.f);
        float x = shapeDly(rnd01(rng));
        float dly = dMin + (dMax - dMin) * x;
        float ratio = (dly - dMin) / (dMax - dMin);
        float amp = (1.f - ratio) * (1.f - ratio); amp = 1.f + (amp - 1.f) * decay;
        amp *= 1.f - ampSp * rnd01(rng);
        float rto = rtoBlk;
        if (rnd01(rng) < octGate) rto *= 2.f;                // v1 p autooct: a global octave-up GATE (metro 8n, roll <= 0.06, 100 ms line~), not an independent per-grain coin
        float dir = rnd01(rng) > rev ? 1.f : -1.f;
        float inc = 1.f - rto * dir;                       // delay change per sample
        float d0 = dly + (rto > 1.f && dir > 0.f ? (rto - 1.f) * N : 0.f);
        // keep the whole path inside [2, cap]
        float span = fabsf(inc) * N;
        if (span > cap - 2.f) { N = (cap - 2.f) / fabsf(inc); span = fabsf(inc) * N; }
        float lo = fminf(d0, d0 + inc * N), hi = fmaxf(d0, d0 + inc * N);
        if (lo < 2.f) { d0 += 2.f - lo; hi += 2.f - lo; }
        if (hi > cap) d0 -= hi - cap;
        Voice& v = vc[nv++];
        v.d = d0; v.dInc = inc; v.wph = 0.f; v.wInc = 1.f / N; v.left = (int)N; v.off = spawnOff;
        float pp = 0.5f + (rnd01(rng) - 0.5f) * pan;
        v.gL = pp * amp; v.gR = (1.f - pp) * amp;
        v.lpL = v.lpR = 0.f; v.lpA = 1.f;
        if (filt > 0.f) {   // cutoff uniform in octaves between 14 - 10*filt and 14 (McCurdy cpsoct): 16.7 kHz at the top
            float oct = 14.f - 10.f * filt * rnd01(rng);
            float hz = 440.f * exp2f(oct - 8.75f); if (hz > 0.45f * sr) hz = 0.45f * sr;
            v.lpA = 1.f - expf(-kTwoPi * hz / sr);
        }
        if (nv > maxSeen) maxSeen = nv;
    }

    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (clearReq) { clearReq = false; memset(bufL, 0, (mask + 1) * 4); nv = 0; }
        if (!stereoIn) memcpy(R, L, n * 4);
        memcpy(dryL, L, n * 4); memcpy(dryR, R, n * 4);
        const float g1 = sm[P_GSIZE1], g2 = sm[P_GSIZE2];
        const float meanN = 0.5f * (g1 + g2) * 0.001f;                 // s
        float dens = sm[P_DENS];
        const float densCap = 0.7f * HX_GRAIN_VOICES / fmaxf(meanN, 1e-4f);   // keep the pool from saturating
        if (dens > densCap) dens = densCap;
        const float norm = 3.f / sqrtf(fmaxf(1.f, dens * meanN * 0.4f));       // overlap-compensated amplitude (+9.5 dB make-up: pan/amp spread/filter/decay eat ~15 dB at the v1 preset)
        const float accInc = dens / sr;
        const float* wt = win[winSel];
        const float transp = sm[P_TRANSP], pan = sm[P_PAN], aSp = sm[P_AMPSPRD], rev = sm[P_REV], oct = sm[P_OCTUP], dec = sm[P_DECAY], flt = sm[P_FILT], dly = sm[P_DELAY];
        float* outL = L; float* outR = R; (void)outL; (void)outR;
        float wetL[HX_MAXBLOCK], wetR[HX_MAXBLOCK];
        const uint32_t mk = mask; rtoBlk = semiToRatio(transp);
        octTimer -= (float)n; if (octTimer <= 0.f) { octTimer += 0.25f * sr; octTgt = rnd01(rng) <= oct ? 1.f : 0.f; }   // metro 8n at 120 bpm = 250 ms
        octGate += (octTgt - octGate) * (1.f - expf(-(float)n / (0.1f * sr)));                                      // line~ 100 ms
        // budget: write the whole block into the buffer first (every grain reads >= 19 ms back, a block is 1 ms), spawn with a sample offset, then run the voices
        // voice-major (each voice's state lives in registers for the whole block, the window uses nearest-entry lookup in the 1024-point table).
        for (int i = 0; i < n; i++) { const uint32_t wn = w + (uint32_t)i; bufL[wn & mk] = 0.5f * (dryL[i] + dryR[i]); }
        // budget: the grain voices render at HALF rate (every second sample, window and delay advance by 2 per step, filter coefficient adjusted) into hL/hR; the output
        // is rebuilt with a linear interpolation (one half-step of delay). Grains are low-passed texture, the loss above ~12 kHz is inaudible in the cloud.
        const int m = (n + 1) >> 1; float hL[HX_MAXBLOCK / 2 + 1], hR[HX_MAXBLOCK / 2 + 1];
        for (int i = 0; i < m; i++) hL[i] = hR[i] = 0.f;
        for (int i = 0; i < n; i++) { acc += accInc; while (acc >= 1.f) { acc -= 1.f; spawnOff = i; spawn(g1, g2, dly, transp, pan, aSp, rev, oct, dec, flt); } }
        // voices that cover the whole block run four at a time in lockstep (independent filter chains overlap, one accumulate per sample); the rest run singly
        int full[HX_GRAIN_VOICES], nf = 0;
        for (int k = 0; k < nv; k++) { const Voice& v = vc[k]; if (v.off == 0 && v.left >= n) full[nf++] = k; else {
            Voice& u = vc[k]; float d = u.d, dInc = 2.f * u.dInc, wph = u.wph, lpL = u.lpL; const float a2 = u.lpA * (2.f - u.lpA); int i0 = u.off, cnt = n - i0; if (cnt > u.left) cnt = u.left; u.off = 0;
            int steps = 0;
            for (int i = i0; i < i0 + cnt; i += 2, steps++) {
                const int di = (int)d; const float fr = d - (float)di; const uint32_t p0 = (w + (uint32_t)i - (uint32_t)di) & mk, p1 = (p0 - 1) & mk;
                lpL += (bufL[p0] + (bufL[p1] - bufL[p0]) * fr - lpL) * a2; const float eg = wt[(int)(wph * (float)WT)] * lpL;
                hL[i >> 1] += eg * u.gL; hR[i >> 1] += eg * u.gR; d += dInc; wph += 2.f * u.wInc;
            }
            u.d = d - (steps * 2 > cnt ? u.dInc : 0.f); u.wph = wph - (steps * 2 > cnt ? u.wInc : 0.f); u.lpL = lpL; u.left -= cnt; } }
        int q = 0;
        for (; q + 4 <= nf; q += 4) {
            Voice* V[4] = { &vc[full[q]], &vc[full[q + 1]], &vc[full[q + 2]], &vc[full[q + 3]] };
            float d[4], dI[4], ph[4], pI[4], lp[4], a[4], gl[4], gr[4];
            for (int j = 0; j < 4; j++) { d[j] = V[j]->d; dI[j] = 2.f * V[j]->dInc; ph[j] = V[j]->wph; pI[j] = 2.f * V[j]->wInc; lp[j] = V[j]->lpL; a[j] = V[j]->lpA * (2.f - V[j]->lpA); gl[j] = V[j]->gL; gr[j] = V[j]->gR; }
            int steps = 0;
            for (int i = 0; i < n; i += 2, steps++) {
                float sl = 0.f, sr_ = 0.f;
                for (int j = 0; j < 4; j++) {
                    const int di = (int)d[j]; const float fr = d[j] - (float)di; const uint32_t p0 = (w + (uint32_t)i - (uint32_t)di) & mk, p1 = (p0 - 1) & mk;
                    lp[j] += (bufL[p0] + (bufL[p1] - bufL[p0]) * fr - lp[j]) * a[j]; const float eg = wt[(int)(ph[j] * (float)WT)] * lp[j];
                    sl += eg * gl[j]; sr_ += eg * gr[j]; d[j] += dI[j]; ph[j] += pI[j];
                }
                hL[i >> 1] += sl; hR[i >> 1] += sr_;
            }
            const bool odd = (steps * 2 > n);                                       // odd block: the last step ran one sample past the block, take it back
            for (int j = 0; j < 4; j++) { V[j]->d = d[j] - (odd ? dI[j] * 0.5f : 0.f); V[j]->wph = ph[j] - (odd ? pI[j] * 0.5f : 0.f); V[j]->lpL = lp[j]; V[j]->left -= n; }
        }
        for (; q < nf; q++) {
            Voice& u = vc[full[q]]; float d = u.d, dInc = 2.f * u.dInc, wph = u.wph, lpL = u.lpL; const float a2 = u.lpA * (2.f - u.lpA); int steps = 0;
            for (int i = 0; i < n; i += 2, steps++) {
                const int di = (int)d; const float fr = d - (float)di; const uint32_t p0 = (w + (uint32_t)i - (uint32_t)di) & mk, p1 = (p0 - 1) & mk;
                lpL += (bufL[p0] + (bufL[p1] - bufL[p0]) * fr - lpL) * a2; const float eg = wt[(int)(wph * (float)WT)] * lpL;
                hL[i >> 1] += eg * u.gL; hR[i >> 1] += eg * u.gR; d += dInc; wph += 2.f * u.wInc;
            }
            const bool odd = (steps * 2 > n);
            u.d = d - (odd ? u.dInc : 0.f); u.wph = wph - (odd ? u.wInc : 0.f); u.lpL = lpL; u.left -= n;
        }
        for (int i = 0; i < n; i++) {                                                // rebuild full rate: even samples = mean of the previous and current step, odd = current
            const int k = i >> 1; const float pl = k ? hL[k - 1] : prevL, pr = k ? hR[k - 1] : prevR;
            if (i & 1) { wetL[i] = hL[k]; wetR[i] = hR[k]; } else { wetL[i] = 0.5f * (pl + hL[k]); wetR[i] = 0.5f * (pr + hR[k]); }
        }
        prevL = hL[(n - 1) >> 1]; prevR = hR[(n - 1) >> 1];
        for (int k = 0; k < nv; k++) if (vc[k].left <= 0) { vc[k] = vc[--nv]; k--; }
        for (int i = 0; i < n; i++) { wetL[i] = flush(wetL[i] * norm); wetR[i] = flush(wetR[i] * norm); }
        w += (uint32_t)n;
        // denormal guard on one-pole states is implicit: states are overwritten when voices are re-spawned
        dwL.apply(L, dryL, wetL, n, sm[P_MIX]); dwR.apply(R, dryR, wetR, n, sm[P_MIX]);
        return true;
    }
};
inline Fx* fx_create_grain(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxGrain)); return m ? new (m) FxGrain(a, sr) : nullptr; }
}

// fx_fft.h -- portable real FFT + time-sliced STFT (hop = N/4, sqrt-Hann analysis/synthesis, overlap-add).
// HYPERTRUMPET V2 FX library. Plain float C++17, no std::complex, no recursion, no heap (Arena only).
//
// Design
//  * Real N-point FFT via one complex N/2-point radix-2 DIT FFT + real pack/unpack. N in {512,1024,2048} (size index 0..2),
//    all tables sized once for 2048 and shared (twiddles by stride, bit-reverse + window per size).
//  * The FFT is RESUMABLE: butterflies are executed in budgeted slices so the per-hop work (fwd FFT, spectral
//    callback, inverse FFT per channel) is spread across the blocks of one hop instead of spiking one 48-sample block.
//    `sliced=false` runs the whole frame at the hop boundary instead.
//  * Latency (input -> output, samples): sliced N + H - 1, unsliced N - 1   (H = N/4). Query latency().
//  * One analysis STFT feeds nOut (0, 1 or 2) synthesis channels; nOut = 0 is analysis-only (no inverse FFT).
//  * Spectral callback S (a template parameter, no virtuals) must provide:
//        void frameBegin(Stft&);                 // analysis spectrum st.inRe/inIm[0..nb) is complete
//        void bins(Stft&, int k0, int k1);       // called in order, slices of <= 64 bins; write st.oRe[c]/oIm[c][k0..k1)
//        void frameEnd(Stft&);
//  * Optional: `#define HX_USE_CMSIS` is NOT wired in this version; the default path is plain C++ loops
//    (butterfly kernel is isolated in Stft::butterflies() so arm_cfft_f32 can replace it later).
// Original code (standard textbook radix-2 + real-FFT packing). Emory 2026-11-15.
#pragma once
#include "fx_common.h"
#include "fx_halfband.h"

namespace hx {

// 2^x, |x| < 60, rel. error ~1e-4. Plain float, no table.
static inline float fastExp2(float x) {
    x = clampf(x, -60.f, 60.f);
    float fi = floorf(x), f = x - fi;
    float p = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    uint32_t u = (uint32_t)((int)fi + 127) << 23; float s; memcpy(&s, &u, 4);
    return p * s;
}

// radix-4 twiddles, shared by every Stft: per step half-size h = 2^lh (lh = 1..9) the triplet (W, W^2, W^3), W = exp(-2 pi i j / 4h), j < h, as 6 contiguous floats per j.
// Twiddles. Sizes up to 2048 (everything the effects use: 512..2048 points) live in fast RAM (DTCM, ~27 KB, zero wait states); only the optional 4096 set is in SDRAM.
static float fftTabA[6 * 510] __attribute__((aligned(32)));                 // radix-4 steps lh = 1..8
static float HX_SDRAM fftTabB[6 * 512] __attribute__((aligned(32)));        // lh = 9 (N = 4096 only)
static bool fftTabOk = false;
static inline const float* fftTabPtr(int lh) { return lh >= 9 ? fftTabB : fftTabA + 6 * ((1 << lh) - 2); }
static float fftUA[2 * 1795] __attribute__((aligned(32)));                  // pack/unpack twiddles exp(-2 pi i q / N), q = 0..N/2: N = 512, 1024, 2048 (re block then im block per size)
static float HX_SDRAM fftUB[2 * 2049] __attribute__((aligned(32)));         // N = 4096
static inline float* fftUPtr(int idx) { return idx == 0 ? fftUA : (idx == 1 ? fftUA + 2 * 257 : (idx == 2 ? fftUA + 2 * 770 : fftUB)); }
static inline void fftTabInit() {
    if (fftTabOk) return;
    const double TP = 6.283185307179586;
    for (int idx = 0; idx < 4; idx++) { const int N = 512 << idx, M = N / 2; float* t = fftUPtr(idx); for (int q = 0; q <= M; q++) { t[q] = (float)cos(TP * q / N); t[M + 1 + q] = (float)(-sin(TP * q / N)); } }
    for (int lh = 1; lh <= 9; lh++) { const int h = 1 << lh; float* t = (float*)fftTabPtr(lh);
        for (int j = 0; j < h; j++) for (int q = 1; q <= 3; q++) { const double a = -TP * (double)(q * j) / (double)(4 * h); t[6 * j + 2 * (q - 1)] = (float)cos(a); t[6 * j + 2 * (q - 1) + 1] = (float)sin(a); }
    }
    fftTabOk = true;
}

// One 2:1 resampling stage pair (decimate + interpolate) for effects that run their STFT at sr/2 (the memory-bound part of the CPU halves).
struct Resamp2 {
    HalfBand hb; HbDec d; HbInt u;
    void init(int len, float beta) { hb.design(len, beta); d.hb = &hb; u.hb = &hb; d.clear(); u.clear(); }
    void clear() { d.clear(); u.clear(); }
};

// Incremental zeroing: a wake must not memset 100+ KB of SDRAM in one 1 ms audio block. Arrays are registered, then run() clears a bounded chunk per call.
struct ClearList {
    float* p[20]; int n[20]; int cnt = 0, seg = 0, off = 0; bool active = false;
    void reset() { cnt = 0; seg = 0; off = 0; active = false; }
    void add(float* a, int len) { if (a && len > 0 && cnt < 20) { p[cnt] = a; n[cnt] = len; cnt++; } }
    void arm() { seg = 0; off = 0; active = cnt > 0; }
    void run(int maxFloats) {
        while (active && maxFloats > 0) {
            int c = n[seg] - off; if (c > maxFloats) c = maxFloats;
            memset(p[seg] + off, 0, sizeof(float) * (size_t)c); off += c; maxFloats -= c;
            if (off >= n[seg]) { seg++; off = 0; if (seg >= cnt) active = false; }
        }
    }
};

struct Stft {
#ifndef HX_STFT_MAXN
#define HX_STFT_MAXN 2048      // a target that wants frames up to 4096 defines HX_STFT_MAXN 4096 before including (drone FRAME 4096)
#endif
    static const int MAXN = HX_STFT_MAXN;
    static const int NSZ = (MAXN >= 4096) ? 4 : 3;   // sizes 512 << 0..NSZ-1
    static const int RS = 2 * MAXN;                  // overlap-add ring (power of two, >= N + H)
    static const int RSM = RS - 1;

    // geometry (current size)
    int N = 0, M = 0, H = 0, nb = 0, L = 0, stages = 0, sizeIdx = -1, nOut = 0;
    bool sliced = true, ok = false, resetFlag = false;
    int capN = 0, nsz = 0;                           // opt-in (set before init): largest frame this instance will ever use (512..MAXN); 0 = MAXN. Sizes all per-instance arrays, so small users stay small.
    bool slicedLoad = false;                         // opt-in (set before init): the windowed input copy of a frame is also time-sliced (input ring 2*MAXN instead of MAXN)
    int specN = 0;                                   // opt-in: spectral callback and forward unpack only cover bins [0, specN) (0 = all); bins above must stay zero in oRe/oIm
    uint32_t inMask = 0, rsm = 0;
    float spec_extra = 0.f;                          // a spectral callback may add work units here for data-dependent slow paths; step() deducts them from the block's budget
    ClearList clr;                                   // lazy clear (see setSizeLazy); process() outputs zeros and ignores its input until it has finished
    float specCost = 4.f; int hopDiv = 4;            // hop = N / hopDiv: 4 (default, 75% overlap) or 2 (50% overlap; sqrt-Hann stays exact, half the frames)

    // spectra visible to the callback
    float *inRe = nullptr, *inIm = nullptr;           // analysis spectrum, bins 0..N/2
    float *oRe[2] = {nullptr, nullptr}, *oIm[2] = {nullptr, nullptr};   // synthesis spectra per channel

    static int sizeOf(int idx) { return 512 << (idx < 0 ? 0 : (idx > NSZ - 1 ? NSZ - 1 : idx)); }
    int latency() const { return L; }
    bool consumeReset() { bool r = resetFlag; resetFlag = false; return r; }

    // ------------------------------------------------------------------ setup (control path; uses double for table accuracy)
    void setHop(int div) { hopDiv = (div == 2) ? 2 : 4; }   // call before init()/setSize()
    bool init(Arena& a0, int sizeIndex, int numOut, bool slicedMode, float perBinCost) {
        Arena& a = a0; Arena& ca = a0.cold ? *a0.cold : a0;
        fftTabInit(); ok = false; sliced = slicedMode; nOut = numOut < 0 ? 0 : (numOut > 2 ? 2 : numOut); specCost = perBinCost;
        const int cap = (capN >= 512 && capN < MAXN) ? capN : MAXN; nsz = 0; while ((512 << nsz) < cap) nsz++; nsz++;
        wre = a.floats(cap / 2); wim = a.floats(cap / 2);
        inRe = a.floats(cap / 2 + 1); inIm = a.floats(cap / 2 + 1);
        inRing = ca.floats(slicedLoad ? 2 * cap : MAXN);
        for (int c = 0; c < nOut; c++) { oRe[c] = a.floats(cap / 2 + 1); oIm[c] = a.floats(cap / 2 + 1); ola[c] = ca.floats(slicedLoad ? 2 * cap : RS); }
        for (int i = 0; i < nsz; i++) {
            int n = 512 << i, m = n / 2;
            win[i] = ca.floats(n); brv[i] = (uint16_t*)ca.allocBytes(sizeof(uint16_t) * m);
            if (!win[i] || !brv[i]) return false;
        }
        if (!wre || !wim || !inRe || !inIm || !inRing) return false;
        for (int c = 0; c < nOut; c++) if (!oRe[c] || !oIm[c] || !ola[c]) return false;
        const double TP = 6.283185307179586;
        for (int i = 0; i < nsz; i++) {
            int n = 512 << i, m = n / 2, bits = 0; while ((1 << bits) < m) bits++;
            for (int j = 0; j < n; j++) win[i][j] = (float)sqrt(0.5 - 0.5 * cos(TP * j / n));   // periodic sqrt-Hann
            for (int j = 0; j < m; j++) { int r = 0; for (int b = 0; b < bits; b++) if (j & (1 << b)) r |= 1 << (bits - 1 - b); brv[i][j] = (uint16_t)r; }
        }
        ok = true; setSize(sizeIndex); return true;
    }

    // change frame size (control path or between blocks). Clears all state; callers must reset their own per-bin state (consumeReset()).
    void setSize(int idx) {                           // immediate: geometry + clear everything now
        if (!setGeom(idx)) return;
        memset(inRing, 0, sizeof(float) * (slicedLoad ? (size_t)(rsm + 1) : (size_t)MAXN));
        for (int c = 0; c < nOut; c++) memset(ola[c], 0, sizeof(float) * (rsm + 1));
        memset(inRe, 0, sizeof(float) * nb); memset(inIm, 0, sizeof(float) * nb);
        for (int c = 0; c < nOut; c++) { memset(oRe[c], 0, sizeof(float) * nb); memset(oIm[c], 0, sizeof(float) * nb); }
        clr.reset();
    }
    // wake-friendly: geometry now, the clearing spread over the next blocks. The caller may add its own arrays to `clr` afterwards, then must call clr.arm().
    void setSizeLazy(int idx) {
        if (!setGeom(idx)) return;
        clr.reset(); clr.add(inRing, slicedLoad ? (int)(rsm + 1) : MAXN);
        for (int c = 0; c < nOut; c++) { clr.add(ola[c], (int)(rsm + 1)); clr.add(oRe[c], nb); clr.add(oIm[c], nb); }
        clr.add(inRe, nb); clr.add(inIm, nb);
    }
    bool setGeom(int idx) {
        if (!ok) return false;
        if (idx < 0) idx = 0; if (idx > nsz - 1) idx = nsz - 1;
        sizeIdx = idx; N = 512 << idx; M = N / 2; H = N / hopDiv; nb = M + 1; stages = 0; while ((1 << stages) < M) stages++;
        fph0 = stages & 1; fsteps = fph0 + stages / 2;
        twr = fftUPtr(idx); twi = twr + M + 1;                 // shared pack/unpack twiddles for this N
        inMask = slicedLoad ? (uint32_t)(2 * N - 1) : (uint32_t)(MAXN - 1); rsm = slicedLoad ? (uint32_t)(2 * N - 1) : (uint32_t)(RS - 1);
        L = sliced ? (N + H - 1) : (N - 1);
        ip = 0; hop = 0; nFrames = 0; phase = IDLE; acc = 0.f; pend = false; resetFlag = true;
        float units = (float)(slicedLoad ? M : 0) + (float)(M / 2 * fsteps) * (1 + nOut) + (float)(specN > 0 && specN + 8 < nb ? specN + 8 : M * 3 / 4) + (float)specLim() * specCost + (float)nOut * 1.75f * (float)M;
        unitsPerSample = units * 1.15f / (float)H;
        return true;
    }

    // ------------------------------------------------------------------ streaming interface
    // in: mono block (n <= any); outs[c] receive nOut delayed (by latency()) channels. S as documented above.
    template <class S> void process(const float* in, float* const* outs, int n, S& s) {
        if (!ok) return;
        if (clr.active) { clr.run(1024); if (clr.active) { for (int c = 0; c < nOut; c++) memset(outs[c], 0, sizeof(float) * (size_t)n); return; } }
        int i = 0;
        while (i < n) {
            int run = H - hop; const bool trig = run <= n - i; if (run > n - i) run = n - i;
            const int m = trig ? run - 1 : run;                  // samples before the trigger sample: plain ring write + overlap-add read
            for (int j = 0; j < m; j++) inRing[(ip + (uint32_t)j) & inMask] = in[i + j];
            for (int c = 0; c < nOut; c++) { float* o = ola[c]; float* d = outs[c] + i; for (int j = 0; j < m; j++) { uint32_t sl = (ip + (uint32_t)j - (uint32_t)L) & rsm; d[j] = flush(o[sl]); o[sl] = 0.f; } }
            hop += m; ip += (uint32_t)m; i += m;
            if (trig) {                                          // the sample that completes a hop: write, start the frame, then read
                inRing[ip & inMask] = in[i]; hop = 0; trigger(s);
                for (int c = 0; c < nOut; c++) { float* o = ola[c]; uint32_t sl = (ip - (uint32_t)L) & rsm; outs[c][i] = flush(o[sl]); o[sl] = 0.f; }
                ip++; i++;
            }
        }
        if (sliced && phase != IDLE) { acc += unitsPerSample * (float)n; int b = (int)acc; acc -= (float)b; lastBudget = b; step(b, s); }
    }

    // ------------------------------------------------------------------ blocking helpers (tests / offline)
    // forward real FFT of plain N samples x -> re/im [0..N/2]
    void rfft(const float* x, float* re, float* im) {
        const uint16_t* br = brv[sizeIdx];
        for (int n = 0; n < M; n++) { wre[br[n]] = x[2 * n]; wim[br[n]] = x[2 * n + 1]; }
        fftS = 0; fftB = 0; butterflies(1 << 30); unpack(0, nb);
        memcpy(re, inRe, sizeof(float) * nb); memcpy(im, inIm, sizeof(float) * nb);
    }
    // inverse real FFT (scaled so irfft(rfft(x)) == x). Uses synthesis channel 0 storage.
    void irfft(const float* re, const float* im, float* x) {
        memcpy(oRe[0], re, sizeof(float) * nb); memcpy(oIm[0], im, sizeof(float) * nb); oIm[0][0] = 0.f; oIm[0][M] = 0.f;
        pack(0, 0, M); fftS = 0; fftB = 0; butterflies(1 << 30);
        float sc = 1.f / (float)M;
        for (int n = 0; n < M; n++) { x[2 * n] = wim[n] * sc; x[2 * n + 1] = wre[n] * sc; }
    }

    uint32_t frameIndex() const { return nFrames; }

private:
    enum Phase { IDLE, LOAD, FWD, UNPACK, SPEC, PACK, INV, POST };
    int specLim() const { return (specN > 0 && specN < nb) ? specN : nb; }
    const float *twr = nullptr, *twi = nullptr; float *wre = nullptr, *wim = nullptr, *inRing = nullptr;
    float* win[4] = {nullptr, nullptr, nullptr, nullptr}; uint16_t* brv[4] = {nullptr, nullptr, nullptr, nullptr}; float* ola[2] = {nullptr, nullptr};
    int fsteps = 0, fph0 = 0; bool pend = false; uint32_t pendT = 0; int lastBudget = 0;
    uint32_t ip = 0, frameBase = 0, nFrames = 0; int hop = 0, fftS = 0, fftB = 0, k = 0, ch = 0, jStart = 0; Phase phase = IDLE;
    float acc = 0.f, unitsPerSample = 1.f;

    // resumable FFT on wre/wim (already bit-reversed): radix-4 steps (two radix-2 stages merged: 3 complex multiplies per 4 points, contiguous
    // per-step twiddles) with one trivial radix-2 step first when log2(M) is odd. Budget unit = one radix-2 butterfly (a radix-4 butterfly counts 2: it costs about half of four radix-2 ones).
    // State: fftS = step index, fftB = butterfly index inside the step. Returns leftover budget.
    int butterflies(int budget) {
        float* HX_RESTRICT re = wre; float* HX_RESTRICT im = wim;
        while (budget > 0 && fftS < fsteps) {
            if (fph0 && fftS == 0) {                            // radix-2, h = 1, twiddle 1
                const int cnt = M / 2; int todo = cnt - fftB; if (todo > budget) todo = budget;
                for (int b = fftB; b < fftB + todo; b++) {
                    const int i = 2 * b; float xr = re[i + 1], xi = im[i + 1];
                    re[i + 1] = re[i] - xr; im[i + 1] = im[i] - xi; re[i] += xr; im[i] += xi;
                }
                fftB += todo; budget -= todo;
                if (fftB >= cnt) { fftB = 0; fftS++; }
                continue;
            }
            const int lh = 2 * fftS - fph0, h = 1 << lh, cnt = M / 4;
            int todo = cnt - fftB, bq = (budget + 1) >> 1; if (todo > bq) todo = bq;
            int left = todo, b = fftB;
            if (lh == 0) {                                      // h = 1: all twiddles 1, plain 4-point butterflies on consecutive quads
                for (int q = b; q < b + todo; q++) {
                    float *pr = re + 4 * q, *pi = im + 4 * q;
                    const float x0r = pr[0], x0i = pi[0], x1r = pr[1], x1i = pi[1], x2r = pr[2], x2i = pi[2], x3r = pr[3], x3i = pi[3];
                    const float sr = x0r + x1r, si = x0i + x1i, dr = x0r - x1r, di = x0i - x1i, ur = x2r + x3r, ui = x2i + x3i, vr = x2i - x3i, vi = x3r - x2r;
                    pr[0] = sr + ur; pi[0] = si + ui; pr[2] = sr - ur; pi[2] = si - ui; pr[1] = dr + vr; pi[1] = di + vi; pr[3] = dr - vr; pi[3] = di - vi;
                }
                left = 0;
            }
            while (left > 0) {
                const int j = b & (h - 1), g = b >> lh; int run = h - j; if (run > left) run = left;
                const int i0 = (g << (lh + 2)) + j;
                float *p0r = re + i0, *p0i = im + i0, *p1r = p0r + h, *p1i = p0i + h, *p2r = p1r + h, *p2i = p1i + h, *p3r = p2r + h, *p3i = p2i + h;
                const float* tw = fftTabPtr(lh) + 6 * j;
                for (int r = 0; r < run; r++, tw += 6) {
                    const float w1r = tw[0], w1i = tw[1], w2r = tw[2], w2i = tw[3], w3r = tw[4], w3i = tw[5];
                    const float x0r = p0r[r], x0i = p0i[r];
                    const float t1r = p1r[r] * w2r - p1i[r] * w2i, t1i = p1r[r] * w2i + p1i[r] * w2r;   // W^2 x1
                    const float t2r = p2r[r] * w1r - p2i[r] * w1i, t2i = p2r[r] * w1i + p2i[r] * w1r;   // W   x2
                    const float t3r = p3r[r] * w3r - p3i[r] * w3i, t3i = p3r[r] * w3i + p3i[r] * w3r;   // W^3 x3
                    const float sr = x0r + t1r, si = x0i + t1i, dr = x0r - t1r, di = x0i - t1i, ur = t2r + t3r, ui = t2i + t3i, vr = t2i - t3i, vi = t3r - t2r;
                    p0r[r] = sr + ur; p0i[r] = si + ui; p2r[r] = sr - ur; p2i[r] = si - ui; p1r[r] = dr + vr; p1i[r] = di + vi; p3r[r] = dr - vr; p3i[r] = di - vi;
                }
                b += run; left -= run;
            }
            fftB += todo; budget -= 2 * todo;
            if (fftB >= cnt) { fftB = 0; fftS++; }
        }
        return budget;
    }
    // Z[k] (M-point FFT of even+i*odd) -> X[k], k = 0..M  (X[k] = Xe + w^k Xo)
    void unpack(int k0, int k1) {
        for (int q = k0; q < k1; q++) {
            int a = q & (M - 1), b = (M - q) & (M - 1);
            float ar = wre[a], ai = wim[a], br_ = wre[b], bi = -wim[b];
            float er = 0.5f * (ar + br_), ei = 0.5f * (ai + bi);
            float dr = 0.5f * (ar - br_), di = 0.5f * (ai - bi);
            float wr = twr[q], wi = twi[q];
            float xr = di, xi = -dr;                        // Xo = D / i
            inRe[q] = er + xr * wr - xi * wi; inIm[q] = ei + xr * wi + xi * wr;
        }
        inIm[0] = 0.f; inIm[M] = 0.f;
    }
    // same result as unpack(0, M+1), two bins per iteration (q and M-q share the loads): handles q in [k0, k1) of 0..M/2
    void unpackPair(int k0, int k1) {
        for (int q = k0; q < k1; q++) {
            const int b = (M - q) & (M - 1), a = q & (M - 1);
            const float ar = wre[a], ai = wim[a], br_ = wre[b], bi = -wim[b];
            const float er = 0.5f * (ar + br_), ei = 0.5f * (ai + bi), dr = 0.5f * (ar - br_), di = 0.5f * (ai - bi);
            const float wr = twr[q], wi = twi[q], xr = di, xi = -dr;
            const float tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
            inRe[q] = er + tr; inIm[q] = ei + ti; inRe[M - q] = er - tr; inIm[M - q] = ti - ei;
        }
        inIm[0] = 0.f; inIm[M] = 0.f;
    }
    // same result as pack(c, 0, M) times the OLA gain (hopDiv-dependent) / M, two outputs per iteration: handles q in [k0, k1) of 0..M/2
    void packPair(int c, int k0, int k1) {
        const uint16_t* br = brv[sizeIdx]; const float *xr_ = oRe[c], *xi_ = oIm[c]; const float hg = 0.5f * (hopDiv == 2 ? 1.0f : 0.5f) / (float)M;   // Hann OLA (sqrt-Hann squared) sums to 2 at hop N/4 and to 1 at hop N/2
        for (int q = k0; q < k1; q++) {
            const float xr = xr_[q], xi = xi_[q], yr = xr_[M - q], yi = xi_[M - q];
            const float aer = hg * (xr + yr), aei = hg * (xi - yi), dr = hg * (xr - yr), di = hg * (xi + yi);
            const float cr = twr[q], ci = -twi[q];
            const float bor = dr * cr - di * ci, boi = dr * ci + di * cr;
            wre[br[q]] = aei + bor; wim[br[q]] = aer - boi;
            if (q > 0 && q < M - q) { const int q2 = M - q; wre[br[q2]] = bor - aei; wim[br[q2]] = aer + boi; }
        }
    }
    // synthesis spectrum (channel c) -> Z, written swapped + bit-reversed into the work arrays (inverse via forward FFT)
    void pack(int c, int k0, int k1) {
        const uint16_t* br = brv[sizeIdx]; const float *xr_ = oRe[c], *xi_ = oIm[c];
        for (int q = k0; q < k1; q++) {
            float xr = xr_[q], xi = xi_[q], yr = xr_[M - q], yi = xi_[M - q];
            float aer = 0.5f * (xr + yr), aei = 0.5f * (xi - yi);
            float dr = 0.5f * (xr - yr), di = 0.5f * (xi + yi);
            float cr = twr[q], ci = -twi[q];      // conj(w)
            float bor = dr * cr - di * ci, boi = dr * ci + di * cr;
            float zr = aer - boi, zi = aei + bor;           // Z = Ae + i*Bo
            wre[br[q]] = zi; wim[br[q]] = zr;               // swap trick
        }
    }
    void post(int c, int k0, int k1) {                  // overlap-add of the windowed inverse-FFT output (the 1/M and overlap gain are already folded into packPair)
        const float* w = win[sizeIdx]; float* o = ola[c];
        if (jStart <= 2 * k0) {                          // common case: no samples before t=0 to skip; contiguous unless the ring wraps
            const uint32_t base = (frameBase + 2u * (uint32_t)k0) & rsm; const int cnt = 2 * (k1 - k0);
            if (base + (uint32_t)cnt <= (rsm + 1u)) {
                float* p = o + base; const float* ww = w + 2 * k0;
                for (int q = k0, j = 0; q < k1; q++, j += 2) { p[j] += wim[q] * ww[j]; p[j + 1] += wre[q] * ww[j + 1]; }
                return;
            }
        }
        for (int q = k0; q < k1; q++) {
            int s0 = 2 * q; uint32_t sl = (frameBase + (uint32_t)s0) & rsm;
            if (s0 >= jStart) o[sl] += wim[q] * w[s0];
            if (s0 + 1 >= jStart) o[(sl + 1) & rsm] += wre[q] * w[s0 + 1];
        }
    }
    void load(int q0, int q1) {                         // windowed + bit-reversed copy of input samples 2*q0 .. 2*q1-1 of the frame
        const float* w = win[sizeIdx]; const uint16_t* br = brv[sizeIdx];
        const uint32_t b0 = (frameBase + 2u * (uint32_t)q0) & inMask;
        if (b0 + 2u * (uint32_t)(q1 - q0) <= inMask + 1u) {
            const float* r = inRing + b0; const float* ww = w + 2 * q0;
            for (int n = q0, j = 0; n < q1; n++, j += 2) { wre[br[n]] = r[j] * ww[j]; wim[br[n]] = r[j + 1] * ww[j + 1]; }
            return;
        }
        for (int n = q0; n < q1; n++) {
            int j0 = 2 * n; float x0 = inRing[(frameBase + (uint32_t)j0) & inMask], x1 = inRing[(frameBase + (uint32_t)j0 + 1) & inMask];
            wre[br[n]] = x0 * w[j0]; wim[br[n]] = x1 * w[j0 + 1];
        }
    }
    // A hop boundary. If the previous frame is still running (slow target, data-dependent slow path) the new frame is not forced into one block (that would
    // be a deadline-missing spike): it starts as soon as the running one finishes, so overload degrades to a late frame, never to an xrun.
    template <class S> void trigger(S& s) {
        if (phase != IDLE) { step(2 * lastBudget + 32, s); if (phase != IDLE) { pend = true; pendT = ip + 1; return; } }   // finish a nearly done frame now (bounded: about two blocks of work), else defer
        startFrame(ip + 1, s);
    }
    template <class S> void startFrame(uint32_t T, S& s) {
        frameBase = T - (uint32_t)N;
        jStart = ((uint64_t)nFrames * (uint64_t)H + (uint64_t)H < (uint64_t)N) ? (int)(N - (nFrames * H + H)) : 0;   // skip samples before t=0
        { const int32_t late = (int32_t)((ip - (uint32_t)L) - frameBase); if (late > jStart) jStart = late; }   // a late frame: its first samples were already read out, skip them instead of leaving stale data in the ring
        nFrames++;
        if (slicedLoad && sliced) { phase = LOAD; k = 0; ch = 0; return; }
        load(0, M);
        fftS = 0; fftB = 0; phase = FWD; k = 0; ch = 0;
        if (!sliced) step(1 << 30, s);
    }
    template <class S> void step(int budget, S& s) {
        while (phase != IDLE && budget > 0) {
            switch (phase) {
            case FWD:
                budget = butterflies(budget);
                if (fftS >= fsteps) { phase = UNPACK; k = 0; }
                break;
            case LOAD: { int e = k + (budget < 16 ? 16 : (budget > 128 ? 128 : budget)); if (e > M) e = M; load(k, e); budget -= (e - k); k = e;
                if (k >= M) { phase = FWD; fftS = 0; fftB = 0; } break; }
            case UNPACK: { const bool lim = specN > 0 && specN + 8 < nb; const int nu = lim ? specN + 8 : M / 2 + 1; int e = k + (budget < 16 ? 16 : (budget > 128 ? 128 : budget)); if (e > nu) e = nu;
                if (lim) { unpack(k, e); budget -= (e - k); } else { unpackPair(k, e); budget -= (e - k) + ((e - k) >> 1); } k = e;
                if (k >= nu) { phase = SPEC; k = 0; s.frameBegin(*this); } break; }
            case SPEC: { const int ns = specLim(); int cb = (int)((float)budget / specCost); if (cb < 8) cb = 8; if (cb > 32) cb = 32; int e = k + cb; if (e > ns) e = ns; s.bins(*this, k, e); budget -= (int)((float)(e - k) * specCost + spec_extra) + 1; spec_extra = 0.f; k = e;
                if (k >= ns) { s.frameEnd(*this);
                    if (nOut > 0) { for (int c = 0; c < nOut; c++) { oIm[c][0] = 0.f; oIm[c][M] = 0.f; } phase = PACK; ch = 0; k = 0; } else phase = IDLE; } break; }
            case PACK: { int e = k + (budget < 16 ? 16 : (budget > 128 ? 128 : budget)); if (e > M / 2 + 1) e = M / 2 + 1; packPair(ch, k, e); budget -= (e - k) + ((e - k) >> 1); k = e;
                if (k >= M / 2 + 1) { phase = INV; fftS = 0; fftB = 0; } break; }
            case INV:
                budget = butterflies(budget);
                if (fftS >= fsteps) { phase = POST; k = 0; }
                break;
            case POST: { int e = k + (budget < 16 ? 16 : (budget > 128 ? 128 : budget)); if (e > M) e = M; post(ch, k, e); budget -= (e - k); k = e;
                if (k >= M) { ch++; k = 0; phase = (ch >= nOut) ? IDLE : PACK; } break; }
            default: phase = IDLE; break;
            }
        }
        if (pend && phase == IDLE) { pend = false; startFrame(pendT, s); }   // nominal window position; only the processing starts late
    }
};

} // namespace hx

// fx_blur.h -- BLUR: spectral blurring, after Michael Norris "Spectral Blurring" (Soundmagic Spectral, macOS AU, closed source).
// HYPERTRUMPET V2 FX library. Clean-room C++, own code. Rebuilt 2026-11-16 from the author's published description (the 2026-11-15 version was
// re-derived by name only: it had a LENGTH time constant, a triangular frequency smear and stereo width, none of which his docs mention).
//
// SOURCES (only these are public):
//   https://michaelnorris.info/software/soundmagic-spectral/guide/   (mirror: https://website2026-f4g.pages.dev/software/soundmagic-spectral/guide/)
//   https://michaelnorris.info/software/soundmagic-spectral          (overview; Blurring was the first effect ported to JUCE)
//
// WHAT HIS DOCS SAY: "Applies a low-pass filter to the spectral changes from one window to the next, progressively smearing the sound.
// Has similar spurious problems as Spectral Averaging." One effect param: Blur amount ("the amount of smearing; can be varied using the variance
// controls"). Shared params: Brightness, Lo bin cutoff, Hi bin cutoff, Randomize phases, FFT size, Gain, Feedback, and Parameter Variance (links a
// parameter to amplitude, inverse amplitude, an LFO, or random change). No ranges, units, defaults, filter order or phase handling are given.
// v1 raw state [44.9,0,0,0,0,39.78,1,1,0.0348]: only [0] = Blur amount (44.9 %) is mapped with any confidence; [1..4] are probably the variance
// controls (all off) and [5..8] common params, but the order is undocumented, so they are NOT mapped.
//
// ALGORITHM (ours): mono-in STFT (fx_fft.h, hop N/4, sqrt-Hann, one inverse FFT). Per bin the magnitude goes through a one-pole lowpass ACROSS
// FRAMES: T = c*T + (1-c)*|X|, c = exp(-hop/tau), tau = 4 s * (BLUR/100)^3 (BLUR=0 -> c=0 -> transparent). Output = T with the live analysis phase
// (so changes smear in time but the signal's phase structure is kept, which gives the "spurious" artefacts he mentions; bins whose live level is < 10% of the blurred level get random phase). Variance (VARTYPE/VARAMT/
// VARRATE) modulates the blur amount per bin: AMP = by the bin's amplitude relative to the loudest bin of the previous frame, INV = the inverse,
// LFO = global sine LFO at VARRATE Hz, RANDOM = per-bin random value re-drawn at VARRATE Hz. BRIGHT / LOCUT / HICUT / RNDPHASE / GAIN act on the output.
//
// GUESSED (docs silent): the BLUR->time-constant law, one-pole order, the variance law (bEff = BLUR + VARAMT*(2m-1), m in 0..1) and its rate
// semantics, BRIGHT as dB/octave tilt around 1 kHz, LOCUT/HICUT as hard Hz cutoffs, RNDPHASE as amount of random phase per bin per frame, mono wet
// copied to L and R, MIX crossfade (no dry/wet in his docs; MIX=0 = undelayed dry), output limiter above 0.8 (not his). NOT implemented: Feedback.
// REMOVED from the previous version: LENGTH, FSMEAR, PHASE (-> RNDPHASE), WIDTH. Pitch spread was never implemented.
// Cost (Seed, N=1024): 1 fwd + 1 inverse FFT per 256-sample hop (half the previous version) + per-bin ~25 flops (+ 1 expf/bin when variance is on).
// Wet latency N + N/4 - 1 (1279 @ FRAME 1024).
// Params: MIX BLUR VARTYPE VARAMT VARRATE BRIGHT LOCUT HICUT RNDPHASE GAIN FRAME.  Roles: AMT=BLUR TIME=VARRATE TONE=BRIGHT MOD=VARAMT.
#pragma once
#include "fx_common.h"
#include "fx_fft.h"
#include "fx_phasecloud.h"

namespace hx {

struct FxBlur : Fx {
    enum { P_MIX, P_BLUR, P_VARTYPE, P_VARAMT, P_VARRATE, P_BRIGHT, P_LOCUT, P_HICUT, P_RNDPH, P_GAIN, P_FRAME, P_DRIFT };
    Stft st;
    // The spectral work runs at sr/2 (24 kHz): half the samples, half the memory traffic. Wet band 0..~9 kHz (dry untouched). FRAME 0/1 (512/1024) = 512 points at 24 kHz (same 47 Hz bins as 1024 at 48 kHz), FRAME 2 = 1024 points.
    static const int CAPN = 2048, MBN = CAPN / 2 + 1;   // FRAME 3 (chill default) = 2048 points at 24 kHz (85 ms window)
    PhaseCloud pc; int dq = 15;
    float srI = 24000.f; Resamp2 rs; float mi[HX_MAXBLOCK / 2 + 2], wi[HX_MAXBLOCK / 2 + 2];
    static int frameToIdx(int fi) { return fi >= 3 ? 2 : (fi >= 2 ? 1 : 0); }
    float *T = nullptr, *rv = nullptr, *lg = nullptr;
    float mono[HX_MAXBLOCK], wl[HX_MAXBLOCK];
    float cBase = 0.f, b01 = 0.f, vamt = 0.f, rnd = 0.f, bright = 0.f, loHz = 20.f, hiHz = 24000.f, binHz = 46.875f, hopS = 0.005f;
    float lfoPh = 0.f, lfoV = 0.5f, maxCur = 0.f, maxPrev = 1.f, rvTimer = 0.f; int vtype = 0; bool redraw = true;
    float mixCur = 0.f; bool primed = false; int curFi = 0; uint32_t seed = 0xBEEF1234u;

    FxBlur(Arena& a, float s) {
        name = "BLUR"; sr = s; srI = s * 0.5f; rs.init(15, 5.0f);
        static const char* const frames[] = {"512", "1024", "2048", "4096"};
        static const char* const vtypes[] = {"None", "Amp", "InvAmp", "LFO", "Random"};
        P(P_MIX,    "MIX",     0.f, 1.f, 0.7f, 0.f, PF_PERCENT);
        P(P_BLUR,   "BLUR",    0.f, 100.f, 44.9f, 0.f, 0);                  // Blur amount, % (v1 raw [0])
        P(P_VARTYPE,"VARTYPE", 0.f, 4.f, 0.f, 1.f, PF_ENUM | PF_QUANT, vtypes);   // Parameter Variance source
        P(P_VARAMT, "VARAMT",  0.f, 1.f, 0.f, 0.f, PF_PERCENT);
        P(P_VARRATE,"VARRATE", 0.05f, 20.f, 1.f, 0.f, PF_LOG);              // Hz (LFO rate / random re-draw rate)
        P(P_BRIGHT, "BRIGHT",  -12.f, 12.f, 0.f, 0.f, PF_BIPOLAR);          // dB/octave tilt around 1 kHz
        P(P_LOCUT,  "LOCUT",   20.f, 20000.f, 20.f, 0.f, PF_LOG);
        P(P_HICUT,  "HICUT",   20.f, 24000.f, 24000.f, 0.f, PF_LOG);
        P(P_RNDPH,  "RNDPHASE",0.f, 1.f, 1.f, 0.f, PF_PERCENT);   // 1 = blurred magnitudes resynthesised with random phases (Paulstretch chill), fades in over BLUR 0..12 % so BLUR = 0 stays transparent
        P(P_GAIN,   "GAIN",    0.f, 2.f, 1.f, 0.f, 0);
        P(P_FRAME,  "FRAME",   0.f, 3.f, 3.f, 1.f, PF_ENUM | PF_QUANT, frames);   // FFT size (3 = longest window, chill default)
        P(P_DRIFT,  "DRIFT",   0.f, 1.f, 0.12f, 0.f, PF_PERCENT);                // slow drift of the random phases: 0 static shimmer, 0.1 slow smooth, 1 new random phase every frame
        setRole(R_AMT, P_BLUR); setRole(R_TIME, P_VARRATE); setRole(R_TONE, P_BRIGHT); setRole(R_MOD, P_VARAMT);
        const int MB = MBN;
        st.setHop(2); st.slicedLoad = true; st.capN = CAPN; curFi = 0; ok = st.init(a, 0, 1, true, 6.0f);      // budget: hop N/2 (half the frames per second, sqrt-Hann overlap-add stays exact)
        T = a.floats(MB); rv = a.floats(MB); lg = a.floats(MB);
        if (!T || !rv || !lg || !pc.alloc(a, MB)) ok = false;
        if (!ok) return;
        resetState();
    }
    int lgDone = 0; float lgBh = 46.875f;
    void resetState(bool lazy = false) {              // lazy (wake): the clearing is spread over the next blocks with the Stft's rings (st.clr)
        if (!ok) return;
        const int MB = lazy ? st.nb : MBN;
        if (lazy) st.clr.add(T, MB); else memset(T, 0, sizeof(float) * MB);
        for (int k = 0; k < MB; k++) rv[k] = 0.5f;
        pc.seedBins(MB);
        lgDone = 0; lgBh = srI / (float)Stft::sizeOf(curFi);               // BRIGHT tilt table: filled lazily, only when BRIGHT is used
        maxCur = 0.f; maxPrev = 1.f; redraw = true; rvTimer = 0.f;
        if (lazy) st.clr.arm();
    }
    void reset() override { if (!ok) return; st.setSizeLazy(curFi); st.consumeReset(); resetState(true); primed = false; }   // primed=false: the mix smoother restarts from the (glue-zeroed) target

    static inline float tauOf(float b) { return 4.f * b * b * b; }          // b 0..1 -> seconds
    void frameBegin(Stft& s) {
        hopS = (float)s.H / srI; b01 = clampf(sm[P_BLUR] * 0.01f, 0.f, 1.f); vamt = sm[P_VARAMT]; vtype = (int)(v[P_VARTYPE] + 0.5f);
        float t0 = tauOf(b01); cBase = t0 > 1e-4f ? expf(-hopS / t0) : 0.f;
        rnd = sm[P_RNDPH] * clampf(b01 * 8.f, 0.f, 1.f); dq = (int)(clampf(sm[P_DRIFT], 0.f, 1.f) * 128.f + 0.5f); bright = sm[P_BRIGHT]; loHz = sm[P_LOCUT]; hiHz = sm[P_HICUT]; binHz = srI / (float)s.N;
        const float rate = sm[P_VARRATE];
        lfoPh += hopS * rate; lfoPh -= (float)(int)lfoPh; lfoV = 0.5f + 0.5f * sinPar(lfoPh);
        rvTimer += hopS * rate; redraw = false; if (rvTimer >= 1.f) { rvTimer -= (float)(int)rvTimer; redraw = true; }
    }
    void bins(Stft& s, int k0, int k1) {
        float *oR = s.oRe[0], *oI = s.oIm[0]; const float bg = bright * (1.f / 6.0206f), invMax = 1.f / (maxPrev + 1e-9f);
        for (int i = 0; i < 8 && lgDone < s.nb; i++, lgDone++) lg[lgDone] = lgDone > 0 ? log2f((float)lgDone * lgBh * 0.001f) : -6.f;   // BRIGHT tilt table, a few bins per slice in the background
        const bool var = vtype != 0 && vamt > 0.001f;
        for (int k = k0; k < k1; k++) {
            const float xr = s.inRe[k], xi = s.inIm[k], m2 = xr * xr + xi * xi, iv0 = m2 > 1e-18f ? 1.f / sqrtf(m2) : 0.f, a = m2 * iv0;   // one sqrt + one division per bin, reused for the unit vector
            if (a > maxCur) maxCur = a;
            float c = cBase;
            if (var) {
                float m;
                switch (vtype) { case 1: m = clampf(a * invMax, 0.f, 1.f); break; case 2: m = 1.f - clampf(a * invMax, 0.f, 1.f); break; case 3: m = lfoV; break;
                                 default: if (redraw) rv[k] = rnd01(seed); m = rv[k]; }
                float be = clampf(b01 + vamt * (2.f * m - 1.f), 0.f, 1.f), t = tauOf(be); c = t > 1e-4f ? fastExp2(-1.442695f * hopS / t) : 0.f;
            }
            float t = flush(c * T[k] + (1.f - c) * a); T[k] = t;
            float ur, ui;
            if (a > 1e-9f) { ur = xr * iv0; ui = xi * iv0; } else { ur = 1.f; ui = 0.f; }
            const bool noPh = a < 0.1f * t;                   // live bin far below the blurred level: its phase is meaningless -> synthetic phase
            if (rnd > 0.001f || noPh) {
                pc.step(k, dq); const float cr = pc.zr[k], ci = pc.zi[k], w = noPh ? 1.f : rnd;
                if (w >= 0.999f) { ur = cr; ui = ci; }
                else { float nr = (1.f - w) * ur + w * cr, ni = (1.f - w) * ui + w * ci; const float m2 = nr * nr + ni * ni; if (m2 > 1e-12f) { const float iv = 1.f / sqrtf(m2); nr *= iv; ni *= iv; ur = nr; ui = ni; } else { ur = cr; ui = ci; } }
            }
            float g = 1.f; const float fHz = (float)k * binHz;
            if (fHz < loHz || fHz > hiHz) g = 0.f; else if (bg != 0.f) { g = k < lgDone ? fastExp2(bg * lg[k]) : 1.f; }
            oR[k] = t * g * ur; oI[k] = t * g * ui;
        }
    }
    void frameEnd(Stft&) { maxPrev = maxCur > 1e-9f ? maxCur : 1.f; maxCur = 0.f; }

    static inline float lim(float x) { float ax = x < 0.f ? -x : x; if (ax <= 0.8f) return x; float y = 0.8f + 0.2f * softclip((ax - 0.8f) * 5.f); return x < 0.f ? -y : y; }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        int fi = (int)(v[P_FRAME] + 0.5f); if (fi < 0) fi = 0; if (fi > 3) fi = 3; fi = frameToIdx(fi);
        if (fi != curFi) { curFi = fi; st.setSizeLazy(fi); st.consumeReset(); resetState(true); }   // deferred: the clearing is spread over the next blocks
        const float* inp = L;
        if (stereoIn) { for (int i = 0; i < n; i++) mono[i] = 0.5f * (L[i] + R[i]); inp = mono; }
        const int n2 = n & ~1; int mc = 0; float y;
        for (int i = 0; i < n2; i++) if (rs.d.push(inp[i], &y)) mi[mc++] = y;
        float* outs[1] = {wi}; st.process(mi, outs, mc, *this);
        if (st.clr.active) return true;                                          // waking / resizing: still clearing, the dry signal passes untouched
        for (int j = 0, o = 0; j < mc; j++) { float a0, a1; rs.u.push(wi[j], &a0, &a1); wl[o++] = a0; wl[o++] = a1; }
        for (int i = n2; i < n; i++) wl[i] = 0.f;
        float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; }
        float inc = (mt - mixCur) / (float)n, m = mixCur, g = sm[P_GAIN];
        if (!stereoIn) {
            for (int i = 0; i < n; i++) { const float d = L[i], a = lim(finite(wl[i]) * g), y = flush(d + (a - d) * m); L[i] = y; R[i] = y; m += inc; }
        } else {
            for (int i = 0; i < n; i++) { const float a = lim(finite(wl[i]) * g), d0 = L[i], d1 = R[i]; L[i] = flush(d0 + (a - d0) * m); R[i] = flush(d1 + (a - d1) * m); m += inc; }
        }
        mixCur = mt; return true;
    }
};
inline Fx* fx_create_blur(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxBlur)); return m ? new (m) FxBlur(a, sr) : nullptr; }
}

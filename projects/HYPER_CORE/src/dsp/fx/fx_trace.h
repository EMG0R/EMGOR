// fx_trace.h -- TRACE: spectral tracing, after Michael Norris "Spectral Tracing" (Soundmagic Spectral, macOS AU, closed source).
// HYPERTRUMPET V2 FX library. Clean-room C++, own code. NEW 2026-11-17.
//
// SOURCES (only these are public):
//   https://michaelnorris.info/software/soundmagic-spectral/guide/   (mirror: https://website2026-f4g.pages.dev/software/soundmagic-spectral/guide/)
//   https://michaelnorris.info/software/soundmagic-spectral          (overview; beta 5 of v1.5 added the retain/remove-loudest/softest variants)
//
// WHAT HIS DOCS SAY: Spectral Tracing keeps or removes spectral bins based on their loudness; the typical use is keeping only the n loudest bins
// and discarding the rest. Parameters: "Tracing type" with eight algorithms: retain the n loudest bins, retain the n softest, retain bins within
// a range, retain bins above a threshold, retain bins below a threshold, remove the n loudest, remove the n softest, remove bins within a range.
// "Number of bins 1" = the number of bins to retain/remove. "Number of bins 2" = the lower end of the range for the range types. "Threshold" =
// a level in dB for the threshold types. Shared suite params: Brightness, Lo bin cutoff, Hi bin cutoff, Randomize phases, FFT size, Gain,
// Feedback. He gives no ranges, units, defaults, window or overlap; the ones below are ours.
//
// ALGORITHM (ours): mono-in STFT (fx_fft.h, 50% overlap, periodic sqrt-Hann analysis + synthesis, so overlap-add is exact and window edges are
// smooth), spectral work at 24 kHz (half-band resampler pair from fx_halfband.h). Each frame: |X|^2 of the bins inside LOCUT..HICUT, the rank
// thresholds are found with a quickselect (O(bins), one pass, no sort), then each bin gets a keep/drop target. The per-bin gain follows its
// target through a one-pole across frames (tau ~15 ms) so a bin entering or leaving the set fades instead of switching (no frame-boundary
// clicks, less "musical noise"). Kept bins pass the live complex bin (true phase, partials stay coherent). The frame's bin loop is run in slices
// by the Stft helper, so the FFT/IFFT and per-bin work are spread over the blocks of a hop (no CPU spike); the only per-frame step done at once is
// the O(bins) quickselect (~257 bins at FRAME 512).
//
// GUESSED (docs silent): range types select by LOUDNESS RANK (rank BINS2..BINS1 counted from the loudest; the larger count is the lower end);
// THRESH units = dBFS of a sinusoid-equivalent partial amplitude (same as FREEZE); bins counted within the LOCUT..HICUT band only (DC excluded);
// BRIGHT as dB/octave tilt around 1 kHz; LOCUT/HICUT as hard Hz cutoffs; RNDPHASE as amount 0..1 of random phase rotation per bin per frame;
// mono wet copied to L and R; MIX crossfade (MIX=0 = undelayed dry); output limiter above 0.8; the ~15 ms bin-gain smoothing. NOT implemented: Feedback.
// Wet latency: 2*(N + N/2 - 1) samples at 48 kHz = 1534 (FRAME 512, 32 ms) or 3070 (FRAME 1024, 64 ms), plus the half-band delay.
// Cost (Seed): 1 fwd + 1 inverse FFT per N/2 hop at 24 kHz + ~15 flops/bin.
// Params: MIX TYPE BINS1 BINS2 THRESH BRIGHT LOCUT HICUT RNDPHASE GAIN FRAME.  Roles: AMT=BINS1 TIME=BINS2 TONE=BRIGHT MOD=RNDPHASE.
// Actions: A_HOLD value>0.5 = freeze the current bin selection (the keep mask stops updating); 0 = release.
#pragma once
#include "fx_common.h"
#include "fx_fft.h"

namespace hx {

struct FxTrace : Fx {
    enum { P_MIX, P_TYPE, P_BINS1, P_BINS2, P_THRESH, P_BRIGHT, P_LOCUT, P_HICUT, P_RNDPH, P_GAIN, P_FRAME };
    enum { T_KEEP_LOUD, T_KEEP_SOFT, T_KEEP_RANGE, T_KEEP_ABOVE, T_KEEP_BELOW, T_DROP_LOUD, T_DROP_SOFT, T_DROP_RANGE };
    Stft st;
    static const int CAPN = 1024, MBN = CAPN / 2 + 1;     // spectral work at 24 kHz: FRAME 0 = 512 points (47 Hz bins), 1 = 1024 points (23 Hz bins)
    float *pw = nullptr, *sc = nullptr, *gk = nullptr, *tgt = nullptr, *lg = nullptr;
    float srI = 24000.f; Resamp2 rs; float mi[HX_MAXBLOCK / 2 + 2], wi[HX_MAXBLOCK / 2 + 2];
    float mono[HX_MAXBLOCK], wl[HX_MAXBLOCK];
    int curFi = 0, lgDone = 0, kLo = 1, kHi = 0, cnt = 0; float lgBh = 46.875f;
    int type = 0; float tHi = 0.f, tLo = 0.f, thrPw = 0.f, gSm = 0.5f, bright = 0.f, rnd = 0.f;
    bool hold = false;
    float mixCur = 0.f; bool primed = false; uint32_t seed = 0x2468aceu;

    FxTrace(Arena& a, float s) {
        name = "TRACE"; sr = s; srI = s * 0.5f; rs.init(15, 5.0f);
        static const char* const types[] = {"KEEP LOUD", "KEEP SOFT", "KEEP RNG", "KEEP >THR", "KEEP <THR", "DROP LOUD", "DROP SOFT", "DROP RNG"};
        static const char* const frames[] = {"512", "1024"};
        P(P_MIX,    "MIX",     0.f, 1.f, 0.8f, 0.f, PF_PERCENT);
        P(P_TYPE,   "TYPE",    0.f, 7.f, 0.f, 1.f, PF_ENUM | PF_QUANT, types);   // Tracing type
        P(P_BINS1,  "BINS1",   1.f, 256.f, 10.f, 1.f, PF_INT | PF_QUANT);        // Number of bins 1
        P(P_BINS2,  "BINS2",   1.f, 256.f, 40.f, 1.f, PF_INT | PF_QUANT);        // Number of bins 2 (range types)
        P(P_THRESH, "THRESH",  -96.f, 0.f, -60.f, 0.f, 0);                       // Threshold, dBFS
        P(P_BRIGHT, "BRIGHT",  -12.f, 12.f, 0.f, 0.f, PF_BIPOLAR);               // Brightness, dB/octave around 1 kHz
        P(P_LOCUT,  "LOCUT",   20.f, 20000.f, 20.f, 0.f, PF_LOG);                // Lo bin cutoff, Hz
        P(P_HICUT,  "HICUT",   20.f, 12000.f, 12000.f, 0.f, PF_LOG);             // Hi bin cutoff, Hz (wet band ends at 12 kHz)
        P(P_RNDPH,  "RNDPHASE",0.f, 1.f, 0.f, 0.f, PF_PERCENT);                  // Randomize phases
        P(P_GAIN,   "GAIN",    0.f, 4.f, 1.f, 0.f, 0);
        P(P_FRAME,  "FRAME",   0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT, frames);  // FFT size
        setRole(R_AMT, P_BINS1); setRole(R_TIME, P_BINS2); setRole(R_TONE, P_BRIGHT); setRole(R_MOD, P_RNDPH);
        st.setHop(2); st.slicedLoad = true; st.capN = CAPN; curFi = 0; ok = st.init(a, 0, 1, true, 2.5f);
        pw = a.floats(MBN); sc = a.floats(MBN); gk = a.floats(MBN); tgt = a.floats(MBN); lg = a.floats(MBN);
        if (!(pw && sc && gk && tgt && lg)) ok = false;
        if (!ok) return;
        resetState(false);
    }
    void resetState(bool lazy) {
        if (!ok) return;
        if (lazy) { st.clr.add(gk, st.nb); st.clr.add(tgt, st.nb); } else { memset(gk, 0, sizeof(float) * MBN); memset(tgt, 0, sizeof(float) * MBN); }
        lgDone = 0; lgBh = srI / (float)Stft::sizeOf(curFi);
        if (lazy) st.clr.arm();
    }
    void reset() override { if (!ok) return; st.setSizeLazy(curFi); st.consumeReset(); resetState(true); rs.clear(); primed = false; hold = false; }
    void wake() { reset(); }
    void action(int id, float value) override { if (id == A_HOLD) hold = value > 0.5f; }

    // k-th largest (0-based) of a[0..n) by iterative quickselect; permutes a.
    static float kthLargest(float* a, int n, int k) {
        if (n <= 0) return 0.f; if (k < 0) k = 0; if (k > n - 1) k = n - 1;
        int lo = 0, hi = n - 1;
        while (lo < hi) {
            const float pv = a[(lo + hi) >> 1]; int i = lo, j = hi;
            while (i <= j) { while (a[i] > pv) i++; while (a[j] < pv) j--; if (i <= j) { float t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; } }
            if (k <= j) hi = j; else if (k >= i) lo = i; else break;
        }
        return a[k];
    }

    // ---- spectral callbacks
    void frameBegin(Stft& s) {
        const int nb = s.nb; const float binHz = srI / (float)s.N;
        type = (int)(v[P_TYPE] + 0.5f); bright = sm[P_BRIGHT]; rnd = sm[P_RNDPH];
        gSm = 1.f - expf(-((float)s.H / srI) / 0.015f);
        thrPw = powf(dbToLin(sm[P_THRESH]) * (0.3183f * (float)s.N), 2.f);
        kLo = (int)ceilf(sm[P_LOCUT] / binHz); if (kLo < 1) kLo = 1;
        kHi = (int)floorf(sm[P_HICUT] / binHz); if (kHi > nb - 1) kHi = nb - 1;
        cnt = kHi - kLo + 1; if (cnt < 0) cnt = 0;
        if (hold) return;                                       // selection frozen: tgt keeps its last keep/drop mask
        const float *xr = s.inRe, *xi = s.inIm;
        for (int k = kLo, j = 0; k <= kHi; k++, j++) { const float p = xr[k] * xr[k] + xi[k] * xi[k]; pw[k] = p; sc[j] = p; }
        int n1 = (int)(sm[P_BINS1] + 0.5f), n2 = (int)(sm[P_BINS2] + 0.5f);
        if (n1 < 1) n1 = 1; if (n2 < 1) n2 = 1; if (n1 > cnt) n1 = cnt; if (n2 > cnt) n2 = cnt;
        tHi = tLo = 0.f;
        switch (type) {
        case T_KEEP_LOUD: case T_DROP_LOUD: tHi = kthLargest(sc, cnt, n1 - 1); break;
        case T_KEEP_SOFT: case T_DROP_SOFT: tLo = kthLargest(sc, cnt, cnt - n1); break;
        case T_KEEP_RANGE: case T_DROP_RANGE: { const int a = n1 < n2 ? n1 : n2, b = n1 < n2 ? n2 : n1; tHi = kthLargest(sc, cnt, a - 1); tLo = kthLargest(sc, cnt, b - 1); } break;
        default: break;
        }
        for (int k = 0; k < nb; k++) tgt[k] = 0.f;
        for (int k = kLo; k <= kHi; k++) {
            const float p = pw[k]; bool keep;
            switch (type) {
            case T_KEEP_LOUD:  keep = p >= tHi; break;
            case T_KEEP_SOFT:  keep = p <= tLo; break;
            case T_KEEP_RANGE: keep = p <= tHi && p >= tLo; break;
            case T_KEEP_ABOVE: keep = p >= thrPw; break;
            case T_KEEP_BELOW: keep = p <= thrPw; break;
            case T_DROP_LOUD:  keep = p < tHi; break;
            case T_DROP_SOFT:  keep = p > tLo; break;
            default:           keep = !(p <= tHi && p >= tLo); break;
            }
            tgt[k] = keep ? 1.f : 0.f;
        }
        if (type == T_KEEP_LOUD || type == T_KEEP_SOFT || type == T_KEEP_RANGE) { for (int k = kLo; k <= kHi; k++) if (pw[k] <= 1e-18f) tgt[k] = 0.f; }   // silence is not a "loudest" bin
    }
    void bins(Stft& s, int k0, int k1) {
        const int nb = s.nb; float *oR = s.oRe[0], *oI = s.oIm[0];
        for (int i = 0; i < 8 && lgDone < nb; i++, lgDone++) lg[lgDone] = lgDone > 0 ? log2f((float)lgDone * lgBh * 0.001f) : -6.f;
        const float bg = bright * (1.f / 6.0206f);
        for (int k = k0; k < k1; k++) {
            float g = gk[k]; g += (tgt[k] - g) * gSm; if (g < 1e-5f) g = 0.f; gk[k] = g;
            if (g == 0.f) { oR[k] = 0.f; oI[k] = 0.f; continue; }
            float re = s.inRe[k] * g, im = s.inIm[k] * g;
            if (rnd > 0.001f) { const float ph = rnd * (rnd01(seed) - 0.5f), c = sinPar(ph + 0.25f), sn = sinPar(ph); const float t = re * c - im * sn; im = re * sn + im * c; re = t; }
            if (bg != 0.f && k < lgDone) { const float gn = fastExp2(bg * lg[k]); re *= gn; im *= gn; }
            oR[k] = re; oI[k] = im;
        }
    }
    void frameEnd(Stft&) {}

    static inline float lim(float x) { float ax = x < 0.f ? -x : x; if (ax <= 0.8f) return x; float y = 0.8f + 0.2f * softclip((ax - 0.8f) * 5.f); return x < 0.f ? -y : y; }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        int fi = (int)(v[P_FRAME] + 0.5f); if (fi < 0) fi = 0; if (fi > 1) fi = 1;
        if (fi != curFi) { curFi = fi; st.setSizeLazy(fi); st.consumeReset(); resetState(true); }
        const float* inp = L;
        if (stereoIn) { for (int i = 0; i < n; i++) mono[i] = 0.5f * (L[i] + R[i]); inp = mono; }
        const int n2 = n & ~1; int mc = 0; float y;
        for (int i = 0; i < n2; i++) if (rs.d.push(inp[i], &y)) mi[mc++] = y;
        float* outs[1] = {wi}; st.process(mi, outs, mc, *this);
        if (st.clr.active) return stereoIn;
        for (int j = 0, o = 0; j < mc; j++) { float a0, a1; rs.u.push(wi[j], &a0, &a1); wl[o++] = a0; wl[o++] = a1; }
        for (int i = n2; i < n; i++) wl[i] = 0.f;
        float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; }
        const float inc = (mt - mixCur) / (float)n; float m = mixCur; const float g = sm[P_GAIN];
        if (!stereoIn) {
            for (int i = 0; i < n; i++) { const float d = L[i], a = lim(finite(wl[i]) * g); const float o = flush(d + (a - d) * m); L[i] = o; R[i] = o; m += inc; }
        } else {
            for (int i = 0; i < n; i++) { const float a = lim(finite(wl[i]) * g), d0 = L[i], d1 = R[i]; L[i] = flush(d0 + (a - d0) * m); R[i] = flush(d1 + (a - d1) * m); m += inc; }
        }
        mixCur = mt; return true;
    }
};
inline Fx* fx_create_trace(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxTrace)); return m ? new (m) FxTrace(a, sr) : nullptr; }
}

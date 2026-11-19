// fx_freeze.h -- FREEZE: spectral freeze / accumulation, after Michael Norris "Spectral Freezing" (a.k.a. "Spectral Accumulation",
// Soundmagic Spectral, macOS AU, closed source). HYPERTRUMPET V2 FX library. Clean-room C++, own code. Rebuilt 2026-11-16 from the author's
// published description (the 2026-11-15 version was re-derived by name only and did not match: it was a crossfade freeze with tilt/smear/width).
//
// SOURCES (only these are public):
//   https://michaelnorris.info/software/soundmagic-spectral/guide/   (mirror: https://website2026-f4g.pages.dev/software/soundmagic-spectral/guide/)
//   https://michaelnorris.info/software/soundmagic-spectral          (version history: beta 6 "fixed incorrect operation in Spectral Freezing")
//
// WHAT HIS DOCS SAY: "Each bin is watched until its amplitude reaches a peak. It is then held at that peak until it is exceeded by another peak
// in that bin." Params: Freeze Factor ("once a bin is frozen, it can then be decayed"; at 100% no decay, only superseded by a new peak),
// Use threshold param (checkbox: a freeze only takes place if the new peak is above the threshold), Threshold, Zero bins under threshold
// (checkbox: unfrozen bins under the threshold are zeroed). Common params shared by the suite: Brightness (boost/cut of higher frequencies),
// Lo bin cutoff, Hi bin cutoff, Randomize phases, FFT size, Gain, Feedback. He gives no ranges, units, defaults or overlap/window/phase detail,
// and the v1 raw state of Freezing was not recorded (nothing to map), so defaults below are ours.
//
// ALGORITHM (ours): mono-in STFT (fx_fft.h, hop N/4, sqrt-Hann, one inverse FFT). Per bin a time-peak detector over the magnitude
// history (a2,a1,a0): a1 is a peak if a1 >= a2 and a1 >= a0. A peak replaces the held value when it exceeds the (decayed) hold and, if USETHR,
// is above THRESH. Frozen bins are output at the held magnitude with a running unit phasor advanced per hop by the analysis phase increment of
// the bin's spectral-region peak (X_t*conj(X_t-1), kept from the update moment): a frozen sinusoid stays a true steady sinusoid and neighbouring
// bins stay phase-locked. Bins that are not frozen pass the live spectrum (or are zeroed when ZEROBIN and below threshold). BRIGHT, LOCUT, HICUT,
// RNDPHASE, GAIN act on the output spectrum.
//
// GUESSED (docs silent): FREEZE decay law (per-frame gain = (FREEZE/100)^(hop seconds * 20), i.e. FREEZE is the gain per 50 ms); THRESH units (dBFS,
// sinusoid-equivalent partial amplitude); BRIGHT as a spectral tilt in dB/octave around 1 kHz; LOCUT/HICUT as hard Hz cutoffs; RNDPHASE as an
// amount 0..1 of random phase rotation per bin per frame (0 = coherent, 1 = fully random); that unfrozen bins pass live audio; the peak definition
// (local maximum of the bin magnitude over three consecutive frames, one frame of delay); stereo (wet is the mono mix, copied to L and R);
// output limiter above 0.8 (not in his docs; protects the Seed output); MIX crossfade (his docs mention no dry/wet; MIX=0 = undelayed dry). NOT implemented: Feedback.
// REMOVED from the previous version: RELEASE, TILT (-> BRIGHT), SMEAR, PHASE (-> RNDPHASE), WIDTH. FREEZE changed meaning: was a 0..1 crossfade
// amount (default 0), now Freeze Factor 0..100 % (default 100 = no decay).
// Actions: A_MAIN = input gate (value 1 = new peaks accepted, 0 = hold what is frozen; default open); A_ALT toggles latched hold; A_HOLD value>0.5 =
// latch on; A_CLEAR = drop all frozen bins and start accumulating again.
// Cost (Seed, N=1024): 1 fwd + 1 inverse FFT per 256-sample hop + per-bin ~50 flops (peak test, hill-climb rotation, phasor step) = ~half of the
// previous version (2 inverse FFTs). Wet latency N + N/4 - 1 (1279 @ FRAME 1024).
// Params: MIX FREEZE USETHR THRESH ZEROBIN BRIGHT LOCUT HICUT RNDPHASE GAIN FRAME.  Roles: AMT=FREEZE TIME=THRESH TONE=BRIGHT MOD=RNDPHASE.
#pragma once
#include "fx_common.h"
#include "fx_fft.h"
#include "fx_phasecloud.h"

namespace hx {

struct FxFreeze : Fx {
    enum { P_MIX, P_FREEZE, P_USETHR, P_THRESH, P_ZEROBIN, P_BRIGHT, P_LOCUT, P_HICUT, P_RNDPH, P_GAIN, P_FRAME, P_DRIFT };
    Stft st;
    float *Hd = nullptr, *a1 = nullptr, *a2 = nullptr, *Pr = nullptr, *Pi = nullptr, *qr = nullptr, *qi = nullptr,
          *prRe = nullptr, *prIm = nullptr, *nxRe = nullptr, *nxIm = nullptr, *lg = nullptr, *pwA = nullptr;
    // The spectral work runs at sr/2 (24 kHz): half the samples, half the memory traffic. Wet band is 0..~9 kHz (dry is untouched). FRAME 0/1 (512/1024) = 512 points at 24 kHz (same 47 Hz bins as 1024 at 48 kHz), FRAME 2 = 1024 points.
    static const int CAPN = 2048, MBN = CAPN / 2 + 1;   // FRAME 3 (the chill default) = 2048 points at 24 kHz (85 ms window, 11.7 Hz bins)
    PhaseCloud pc; int dq = 15;
    float srI = 24000.f; Resamp2 rs; float mi[HX_MAXBLOCK / 2 + 2], wi[HX_MAXBLOCK / 2 + 2];
    static int frameToIdx(int fi) { return fi >= 3 ? 2 : (fi >= 2 ? 1 : 0); }
    int pwDone = 0, lastP = -1; float lcr = 0.f, lci = 0.f, lm2 = 0.f;
    float mono[HX_MAXBLOCK], wl[HX_MAXBLOCK];
    bool gateOpen = true, latch = false;
    float gDec = 1.f, thrAmp = 0.f, bright = 0.f, rnd = 0.f, loHz = 20.f, hiHz = 24000.f, binHz = 46.875f; bool useThr = false, zeroB = false, coh = false;
    float mixCur = 0.f; bool primed = false; uint32_t seed = 0x1234567u; int curFi = 0;

    FxFreeze(Arena& a, float s) {
        name = "FREEZE"; sr = s; srI = s * 0.5f; rs.init(15, 5.0f);
        static const char* const frames[] = {"512", "1024", "2048", "4096"};
        P(P_MIX,    "MIX",     0.f, 1.f, 0.65f, 0.f, PF_PERCENT);
        P(P_FREEZE, "FREEZE",  0.f, 100.f, 100.f, 0.f, 0);                  // Freeze Factor, % (100 = no decay)
        P(P_USETHR, "USETHR",  0.f, 1.f, 0.f, 1.f, PF_INT | PF_QUANT);      // Use threshold param
        P(P_THRESH, "THRESH",  -96.f, 0.f, -60.f, 0.f, 0);                  // Threshold, dBFS
        P(P_ZEROBIN,"ZEROBIN", 0.f, 1.f, 0.f, 1.f, PF_INT | PF_QUANT);      // Zero bins under threshold
        P(P_BRIGHT, "BRIGHT",  -12.f, 12.f, 0.f, 0.f, PF_BIPOLAR);          // Brightness, dB/octave tilt around 1 kHz
        P(P_LOCUT,  "LOCUT",   20.f, 20000.f, 20.f, 0.f, PF_LOG);           // Lo bin cutoff, Hz
        P(P_HICUT,  "HICUT",   20.f, 24000.f, 24000.f, 0.f, PF_LOG);        // Hi bin cutoff, Hz
        P(P_RNDPH,  "RNDPHASE",0.f, 1.f, 1.f, 0.f, PF_PERCENT);             // Randomize phases: 1 = every frozen bin resynthesised with a random phase (Paulstretch chill), 0 = coherent (phase-locked) freeze
        P(P_GAIN,   "GAIN",    0.f, 2.f, 1.f, 0.f, 0);
        P(P_FRAME,  "FRAME",   0.f, 3.f, 3.f, 1.f, PF_ENUM | PF_QUANT, frames);   // FFT size (3 = longest window, chill default)
        P(P_DRIFT,  "DRIFT",   0.f, 1.f, 0.12f, 0.f, PF_PERCENT);                // slow phase drift of the random phases: 0 = static shimmer, 0.1 = slow smooth walk, 1 = new random phase every frame (noisy)
        setRole(R_AMT, P_FREEZE); setRole(R_TIME, P_THRESH); setRole(R_TONE, P_BRIGHT); setRole(R_MOD, P_RNDPH);
        const int MB = MBN;
        st.setHop(2); st.slicedLoad = true; st.capN = CAPN; curFi = 0; ok = st.init(a, 0, 1, true, 3.5f);      // budget: hop N/2 (half the frames per second, sqrt-Hann overlap-add stays exact)
        if (!pc.alloc(a, MB)) ok = false;
        Hd = a.floats(MB); a1 = a.floats(MB); a2 = a.floats(MB); Pr = a.floats(MB); Pi = a.floats(MB); qr = a.floats(MB); qi = a.floats(MB);
        prRe = a.floats(MB); prIm = a.floats(MB); nxRe = a.floats(MB); nxIm = a.floats(MB); lg = a.floats(MB); pwA = a.floats(MB);
        if (!(Hd && a1 && a2 && Pr && Pi && qr && qi && prRe && prIm && nxRe && nxIm && lg && pwA)) ok = false;
        if (!ok) return;
        resetState();
    }
    int lgDone = 0; float lgBh = 46.875f;
    void resetState(bool lazy = false) {              // lazy (wake): clear the per-bin arrays over the next blocks, with the Stft's own rings (st.clr)
        if (!ok) return;
        const int MB = lazy ? st.nb : MBN;
        if (lazy) { st.clr.add(Hd, MB); st.clr.add(a1, MB); st.clr.add(a2, MB); st.clr.add(prRe, MB); st.clr.add(prIm, MB); st.clr.add(nxRe, MB); st.clr.add(nxIm, MB); }
        else { memset(Hd, 0, sizeof(float) * MB); memset(a1, 0, sizeof(float) * MB); memset(a2, 0, sizeof(float) * MB);
               memset(prRe, 0, sizeof(float) * MB); memset(prIm, 0, sizeof(float) * MB); memset(nxRe, 0, sizeof(float) * MB); memset(nxIm, 0, sizeof(float) * MB); }
        for (int k = 0; k < MB; k++) { static const float cc[4] = {1.f, 0.f, -1.f, 0.f}, ss[4] = {0.f, 1.f, 0.f, -1.f}; Pr[k] = 1.f; Pi[k] = 0.f; qr[k] = cc[k & 3]; qi[k] = ss[k & 3]; }
        pc.seedBins(MB);
        lgDone = 0; lgBh = srI / (float)Stft::sizeOf(curFi);               // BRIGHT tilt table (log2 of bin Hz / 1 kHz): filled lazily, only when BRIGHT is used
        if (lazy) st.clr.arm();
    }
    void reset() override { if (!ok) return; st.setSizeLazy(curFi); st.consumeReset(); resetState(true); latch = false; primed = false; }   // primed=false: the mix smoother restarts from the (glue-zeroed) target instead of ramping from the old value

    void action(int id, float value) override {
        switch (id) {
        case A_MAIN: gateOpen = value > 0.5f; break;
        case A_ALT:  latch = !latch; break;
        case A_CLEAR: if (value > 0.5f) { memset(Hd, 0, sizeof(float) * MBN); latch = false; } break;
        case A_HOLD: latch = value > 0.5f; break;
        }
    }

    // ---- spectral callback (called by Stft, sliced across blocks)
    void frameBegin(Stft& s) {
        const float hopS = (float)s.H / srI, f = clampf(sm[P_FREEZE] * 0.01f, 0.f, 1.f);
        gDec = f >= 0.9999f ? 1.f : (f <= 0.f ? 0.f : powf(f, hopS * 20.f));
        useThr = v[P_USETHR] > 0.5f; zeroB = v[P_ZEROBIN] > 0.5f;
        thrAmp = dbToLin(sm[P_THRESH]) * (0.3183f * (float)s.N);          // threshold in |X| units
        bright = sm[P_BRIGHT]; rnd = sm[P_RNDPH]; dq = (int)(clampf(sm[P_DRIFT], 0.f, 1.f) * 128.f + 0.5f); coh = rnd < 0.999f; loHz = sm[P_LOCUT]; hiHz = sm[P_HICUT]; binHz = srI / (float)s.N; pwDone = 0; lastP = -1;
    }
    inline float pw(const Stft& s, int p) const { return s.inRe[p] * s.inRe[p] + s.inIm[p] * s.inIm[p]; }
    void bins(Stft& s, int k0, int k1) {
        const int nb = s.nb; float *oR = s.oRe[0], *oI = s.oIm[0];
        { int hi = k1 + 5; if (hi > nb) hi = nb; for (int j = pwDone; j < hi; j++) pwA[j] = s.inRe[j] * s.inRe[j] + s.inIm[j] * s.inIm[j]; if (hi > pwDone) pwDone = hi; }   // |X|^2 once per bin, a few bins ahead for the hill climb
        for (int i = 0; i < 8 && lgDone < nb; i++, lgDone++) lg[lgDone] = lgDone > 0 ? log2f((float)lgDone * lgBh * 0.001f) : -6.f;   // BRIGHT tilt table, a few bins per slice in the background
        const bool accept = gateOpen && !latch; const float bg = bright * (1.f / 6.0206f);
        for (int k = k0; k < k1; k++) {
            const float xr = s.inRe[k], xi = s.inIm[k], a0 = sqrtf(xr * xr + xi * xi);
            nxRe[k] = xr; nxIm[k] = xi;
            // held value: decay, then time-peak test (a1 was a local maximum)
            float h = Hd[k] * gDec; if (h < 1e-9f) h = 0.f;
            const float peak = a1[k];
            // budget: the region-peak phase analysis (hill climb, normalisation) is only needed when this bin may take a new peak: same result, most bins skip it
            if (accept && peak >= a2[k] && peak >= a0 && peak > h && peak > 1e-9f && (!useThr || peak >= thrAmp)) {
                if (!coh) h = peak;                                              // chill (RNDPHASE = 1): the held phase is synthetic, no region-peak phase analysis needed (cheaper)
                else {
                s.spec_extra += 24.f;                                             // this path costs ~4x a skipped bin
                int p = k;
                for (int q = 0; q < 4; q++) {
                    const float c0 = pwA[p], cl = p > 0 ? pwA[p - 1] : 0.f, cr = p < nb - 1 ? pwA[p + 1] : 0.f;
                    if (cl > c0 && cl >= cr) p--; else if (cr > c0) p++; else break;
                }
                if (p != lastP) { lastP = p; lcr = s.inRe[p] * prRe[p] + s.inIm[p] * prIm[p]; lci = s.inIm[p] * prRe[p] - s.inRe[p] * prIm[p]; lm2 = lcr * lcr + lci * lci; }   // same peak as the previous bin: reuse
                const float cr_ = lcr, ci_ = lci, m2 = lm2;
                if (m2 > 1e-12f) {
                    float iv = 1.f / sqrtf(m2), cq = cr_ * iv, sq = ci_ * iv;
                    h = peak; Pr[k] = a0 > 1e-9f ? xr / a0 : 1.f; Pi[k] = a0 > 1e-9f ? xi / a0 : 0.f; qr[k] = cq; qi[k] = sq;
                }
                }
            }
            Hd[k] = h; a2[k] = a1[k]; a1[k] = a0;
            float re, im;
            if (h > 0.f) {
                float nr, ni;
                if (rnd > 0.001f) {                                              // chill: random-phase resynthesis (PhaseCloud), blended with the coherent phase below RNDPHASE = 1
                    pc.step(k, dq); nr = pc.zr[k]; ni = pc.zi[k];
                }
                if (coh) {
                    float cr2 = Pr[k] * qr[k] - Pi[k] * qi[k], ci2 = Pr[k] * qi[k] + Pi[k] * qr[k];
                    float g = 1.5f - 0.5f * (cr2 * cr2 + ci2 * ci2); Pr[k] = cr2 * g; Pi[k] = ci2 * g;
                    if (rnd > 0.001f) { nr = (1.f - rnd) * Pr[k] + rnd * nr; ni = (1.f - rnd) * Pi[k] + rnd * ni; const float m2 = nr * nr + ni * ni; if (m2 > 1e-12f) { const float iv = 1.f / sqrtf(m2); nr *= iv; ni *= iv; } else { nr = 1.f; ni = 0.f; } }
                    else { nr = Pr[k]; ni = Pi[k]; }
                }
                re = h * nr; im = h * ni;
            } else if (zeroB && useThr && a0 < thrAmp) { re = 0.f; im = 0.f; }
            else { re = xr; im = xi; }
            const float fHz = (float)k * binHz;
            if (fHz < loHz || fHz > hiHz) { re = 0.f; im = 0.f; }
            else if (bg != 0.f) { if (k < lgDone) { const float gn = fastExp2(bg * lg[k]); re *= gn; im *= gn; } }
            oR[k] = re; oI[k] = im;
        }
    }
    void frameEnd(Stft&) { float* t = prRe; prRe = nxRe; nxRe = t; t = prIm; prIm = nxIm; nxIm = t; }

    // output safety: transparent below 0.8, smooth saturation to 1.0 (accumulating many bins can sum far past full scale; his docs warn it clips)
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
inline Fx* fx_create_freeze(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxFreeze)); return m ? new (m) FxFreeze(a, sr) : nullptr; }
}

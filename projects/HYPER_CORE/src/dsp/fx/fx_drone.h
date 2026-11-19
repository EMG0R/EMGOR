// fx_drone.h -- DRONE: spectral drone generator, after Michael Norris "Spectral DroneMaker" (Soundmagic Spectral, macOS AU, closed source).
// HYPERTRUMPET V2 FX library. Clean-room C++, own code. REBUILT FROM SCRATCH 2026-11-17 (v3): v2 "sounded primitive, super clicky, like a bad freeze".
//
// SOURCES (everything public about it):
//   https://michaelnorris.info/software/soundmagic-spectral/guide/   (effect + parameter descriptions)
//   https://michaelnorris.info/software/soundmagic-spectral          (overview: "large FFT sizes give smooth, ethereal drones")
// His words: DroneMaker turns any sound into a slowly-changing drone. Unlike Mammut (one huge FFT) it uses NORMAL-sized FFTs and creates the drone
// "by gradually interpolating between frequency components sampled at a distance". Extras: a comb filterbank BEFORE the interpolation (subtle or
// strong pitched/harmonising effect; level, fundamental, scale type, number of octaves, resonance), a spectral gate (as in Spectral Tracing),
// a flanger on the output. His guide warns of ~0.5 s latency. Ranges/units are not published (guessed, see below).
//
// WHY v2 CLICKED (diagnosed in the v2 code, not measured on the device): (1) STFT hop N/2 with free-running per-bin phasors: a frame is a frozen
// copy of the input's own phase pattern, repeated at the frame rate -> amplitude ripple / buzz at the hop rate; (2) bins hard-gated per snapshot and a
// level normaliser that jumped per frame; (3) bins of one partial given different phase histories (live phase for new bins, free-running for old).
//
// ALGORITHM (v3):
//  * Spectral work at 12 kHz (two half-band stages each way, ~5 kHz bandwidth). STFT N=512 (= 2048 at 48 kHz, 43 ms), hop N/4 (75 % overlap),
//    sqrt-Hann (sine) analysis + synthesis (fx_fft.h; sliced over the blocks, so no frame-boundary CPU spike). One inverse FFT (mono pad).
//  * SNAPSHOTS: every INTERP seconds (+-INTVAR %) the live frame is sampled into per-bin magnitudes B (PEAKAMP: the per-bin maximum since the last
//    snapshot). The previous B becomes A. Output magnitude = A + (B-A)*s, s = raised-cosine of the segment position: zero slope at both ends, so there
//    is never a kink at a snapshot. A silent input (below GATE) is not sampled: the pad holds its last state and keeps sounding (drone).
//  * COMB (before the interpolation, on the sampled values): each spectral peak is weighted by a resonance mask tuned to SCALE notes across OCTAVES
//    from the fundamental (peak width from RESON). The mask is evaluated at the peak's true frequency, so all bins of one partial get the same weight.
//  * PHASE (the part that makes it smooth): every bin keeps ONE phasor that rotates by the TRUE per-hop phase advance of the spectral peak that owns
//    it (peak found by hill climb; advance = X_t * conj(X_t-1) of the peak, normalised, no atan2). All bins of a partial rotate together, so a
//    partial is an exact stationary sinusoid at its true frequency (not a bin-centre frequency). A bin that starts sounding takes the phase of its
//    peak's field (if already sounding) or the live phase, times the sine-window lobe sign (-1)^(k-p): it is phase-locked to the partial, never a
//    pulse. The rotation is cross-faded A->B together with the magnitudes, so a drifting partial glides instead of jumping or beating.
//    Phase never re-syncs to the live input while a bin sounds (no reset pops, no frame-rate re-sync).
//  * LEVEL (smooth auto-level, -20 dBFS RMS before MIX, max +50 dB): feed-forward gain from the pad's own RMS, which is known every frame from the
//    snapshot mix (Parseval; smoothed 0.1 s; the mix is itself a raised-cosine, so it is smooth), times a slow correction (2 s) from the MEASURED pad
//    power (the lobe crossfade of a drifting partial is not exactly power-preserving, up to ~3 dB). A purely slow AGC lags the 10+ dB loudness swings
//    between snapshots (a note's attack is 13 dB quieter than its body) by +-5 dB, hence feed-forward. Gain is applied per sample in the time domain;
//    a new pad fades in over 0.4 s. MIX is the fader.
//  * STEREO: delayed-copy mid/side (mono sum = the pad). OUTPUT: flanger (single tap, linear read, depth/rate), soft limiter.
//
// GUESSED (docs silent): units/ranges (INTERP s, INTVAR %, GATE dBFS of a sinusoid-equivalent partial, COMBLVL/RESON %, FLRATE Hz, FLDEPTH ms),
//   the scale list and order, comb bank realised as a spectral resonance mask (his is time-domain), flanger topology, dry/wet law (additive send),
//   his "Randomize phases" is not exposed (always phase-locked). Defaults are chosen for a quiet dynamic-mic trumpet, not copied from his plug-in.
//
// Params: MIX INTERP INTVAR PEAKAMP GATE COMBLVL COMBFUND SCALE OCTAVES RESON FLAMT FLRATE FLDEPTH GAIN.
// Roles: AMT=MIX TIME=INTERP TONE=COMBLVL MOD=FLAMT PITCH=COMBFUND.
// Memory: ~14 KB per-bin pool + 12 KB lines (hot arena), Stft ~7 KB hot + ~12 KB cold.
// Glue-visible members kept: tI, mixCur, primed, lastRms, floorLin, gOut, refMag, wake().
#pragma once
#include "fx_common.h"
#include "fx_fft.h"

namespace hx {

struct FxDrone : Fx {
    enum { P_MIX, P_INTERP, P_INTVAR, P_PEAK, P_GATE, P_COMBLVL, P_COMBFUND, P_SCALE, P_NOCT, P_RESON, P_FLAMT, P_FLRATE, P_FLDEPTH, P_GAIN, P_COUNT };
    static const int DEC = 4;                  // 48 kHz -> 12 kHz (two half-band stages)
    static const int NF = 512, NFI = 0;        // STFT size / size index (512 << 0): 43 ms window = 2048 points at 48 kHz, bin spacing 23.4 Hz
    static const int SPECN = 214;              // bins [0, 214) = 0..5.0 kHz
    static const int KS = 224;                 // per-bin array length (>= SPECN + 8 for the hill-climb look-ahead)
    static const int NARR = 16;
    static const int HSDLY = 713, HSMAX = 1024; static constexpr float HSG = 0.3f, HSN = 0.95f;   // stereo spread delay
    static const int FLMAX = 1024;             // flanger line (samples @48k), >= 20 ms
    static const int NSCALE = 10;

    Stft st;
    float* pool = nullptr;
    float *M[2], *Qr[2], *Qi[2], *Pr[2], *Pi[2], *prRe, *prIm, *nxRe, *nxIm, *pk, *pw;
    float *fdL = nullptr, *fdR = nullptr, *hs = nullptr;
    HalfBand hb1, hb2; HbDec d1, d2; HbInt u1, u2;
    float mi[HX_MAXBLOCK / 4 + 4], wi[HX_MAXBLOCK / 4 + 4], mono[HX_MAXBLOCK], wl[HX_MAXBLOCK], fq[HX_MAXBLOCK + 16]; int fqn = 0;
    int sel = 0, cp = 0, pwDone = 0, lastP = -1, hsw = 0, fw = 0, validRun = 0, bCnt = 0, hcK = -9, hcP = 0;
    float srI = 12000.f;
    // timing / state
    float segPos = 0.f, segLen = 2.f, lastI = 2.f, sCur = 0.f; bool snap = false, first = false, fresh = true, peakMode = false;
    uint32_t warm = 0, seed = 0x2468ACEu;
    float pkP1 = 0.f, pkP2 = 0.f, pAcc = 0.f, eAcc = 0.f, sumM2 = 0.f, fade = 0.f, padP = 0.f, gFeed = 1.f, corr = 1.f, gateSq = 0.f; int padWarm = 0;
    // glue/telemetry-visible
    float tI = 0.f, mixCur = 0.f, lastRms = 0.f, floorLin = 0.f, gOut = 1.f, refMag = 0.f; bool primed = false;
    // comb
    float combLvl = 0.f, combW = 30.f, fund = 300.f, fundHi = 1e9f; int scaleIx = 7;
    // cached peak (bins of one partial share it)
    float lqr = 1.f, lqi = 0.f, lpr = 1.f, lpi = 0.f, lWgt = 1.f;
    float flPh = 0.f, faCur = 0.f, gCur = 1.f, dCur = 96.f;

    static uint16_t scaleMask(int s) {         // bitmask over 12 pitch classes; index 7 = overtone series (special case)
        static const uint16_t m[NSCALE] = {0xFFF, 0xAB5, 0x5AD, 0x9AD, 0x555, 0x295, 0x4A9, 0x001, 0x6AD, 0xAD5};
        return m[s < 0 ? 0 : (s >= NSCALE ? NSCALE - 1 : s)];
    }

    FxDrone(Arena& a, float s) {
        name = "DRONE"; sr = s; srI = s / (float)DEC; hb1.design(11, 4.5f); hb2.design(31, 5.5f); d1.hb = &hb1; u1.hb = &hb1; d2.hb = &hb2; u2.hb = &hb2;
        static const char* const scales[] = {"Chromatic", "Major", "Minor", "Harm.minor", "Whole tone", "Pent.major", "Pent.minor", "Overtone", "Dorian", "Lydian"};
        P(P_MIX,      "MIX",      0.f, 1.f, 0.5f, 0.f, PF_PERCENT);               // additive send: out = dry + MIX * drone
        P(P_INTERP,   "INTERP",   0.1f, 30.f, 2.06f, 0.f, PF_LOG);               // Interpolation length, s
        P(P_INTVAR,   "INTVAR",   0.f, 100.f, 3.38f, 0.f, 0);                    // Interpolation length variance, % (+-)
        P(P_PEAK,     "PEAKAMP",  0.f, 1.f, 0.f, 1.f, PF_INT | PF_QUANT);        // Use peak amplitudes
        P(P_GATE,     "GATE",     -96.f, -20.f, -72.f, 0.f, 0);                  // Gate level, dBFS of a sinusoid-equivalent partial
        P(P_COMBLVL,  "COMBLVL",  0.f, 100.f, 40.f, 0.f, 0);                     // Comb filterbank level, %
        P(P_COMBFUND, "COMBFUND", 20.f, 2000.f, 303.04f, 0.f, PF_LOG);           // Comb filter fundamental, Hz
        P(P_SCALE,    "SCALE",    0.f, (float)(NSCALE - 1), 7.f, 1.f, PF_ENUM | PF_QUANT, scales);
        P(P_NOCT,     "OCTAVES",  1.f, 8.f, 6.f, 1.f, PF_INT | PF_QUANT);        // Number of octaves
        P(P_RESON,    "RESON",    0.f, 100.f, 70.f, 0.f, 0);                     // Filter resonance, %
        P(P_FLAMT,    "FLAMT",    0.f, 1.f, 0.f, 0.f, PF_PERCENT);               // Flanger amount
        P(P_FLRATE,   "FLRATE",   0.02f, 10.f, 0.1f, 0.f, PF_LOG);               // Flanger rate, Hz
        P(P_FLDEPTH,  "FLDEPTH",  0.1f, 20.f, 2.f, 0.f, 0);                      // Flanger depth, ms (max delay)
        P(P_GAIN,     "GAIN",     0.f, 2.f, 1.f, 0.f, 0);
        setRole(R_AMT, P_MIX); setRole(R_TIME, P_INTERP); setRole(R_TONE, P_COMBLVL); setRole(R_MOD, P_FLAMT); setRole(R_PITCH, P_COMBFUND);
        st.setHop(4); st.slicedLoad = true; st.capN = NF; st.specN = SPECN;
        ok = st.init(a, NFI, 1, true, 6.f);
        pool = a.floats((size_t)NARR * KS); fdL = a.floats(FLMAX); fdR = a.floats(FLMAX); hs = a.floats(HSMAX);
        if (!(ok && pool && fdL && fdR && hs)) { ok = false; return; }
        { float** all[NARR] = {&M[0], &M[1], &Qr[0], &Qr[1], &Qi[0], &Qi[1], &Pr[0], &Pr[1], &Pi[0], &Pi[1], &prRe, &prIm, &nxRe, &nxIm, &pk, &pw};
          for (int i = 0; i < NARR; i++) *all[i] = pool + (size_t)i * KS; }
        reset();
    }

    void stateReset() {
        d1.clear(); d2.clear(); u1.clear(); u2.clear(); fqn = 3; fq[0] = fq[1] = fq[2] = 0.f; hsw = 0; fw = 0; sel = 0; cp = 0; pwDone = 0; lastP = -1; validRun = 0;
        segPos = 0.f; segLen = sm[P_INTERP]; lastI = sm[P_INTERP]; sCur = 0.f; tI = 0.f; snap = first = false; fresh = true;
        warm = 0; pkP1 = pkP2 = pAcc = eAcc = sumM2 = 0.f; fade = 0.f; padP = 0.f; gFeed = 1.f; corr = 1.f; padWarm = 0; lastRms = refMag = 0.f; gOut = 1.f;
        mixCur = sm[P_MIX]; primed = false;
    }
    void reset() override {
        if (!ok) return;
        memset(pool, 0, sizeof(float) * (size_t)NARR * KS); memset(fdL, 0, sizeof(float) * FLMAX); memset(fdR, 0, sizeof(float) * FLMAX); memset(hs, 0, sizeof(float) * HSMAX);
        stateReset(); st.setSize(NFI); st.consumeReset();
    }
    // Light wake from IDLE: everything is cleared a bounded chunk per block (the Stft outputs zeros and ignores input meanwhile).
    void wake() {
        if (!ok) return;
        stateReset(); st.setSizeLazy(NFI); st.consumeReset();
        memset(fdL, 0, sizeof(float) * FLMAX); memset(fdR, 0, sizeof(float) * FLMAX); memset(hs, 0, sizeof(float) * HSMAX);   // 12 KB: no stale audio leaks into the stereo/flanger lines
        st.clr.add(pool, NARR * KS); st.clr.arm();
    }

    // ---- comb resonance weight (1 = no change) for a partial at frequency f
    inline float combGain(float f) const {
        float mask = 0.f;
        if (scaleIx == 7) {
            const float n = f / fund;
            if (n >= 0.97f && f <= fundHi) { float r = floorf(n + 0.5f); if (r < 1.f) r = 1.f; const float dev = 1200.f * log2f(n / r), x = dev / combW; mask = expf(-x * x); }
        } else if (f <= fundHi && f > 10.f) {
            float semi = log2f(f / fund) * 12.f; semi -= floorf(semi / 12.f) * 12.f;
            const uint16_t mk = scaleMask(scaleIx); float best = 99.f;
            for (int d = 0; d < 12; d++) if (mk & (1u << d)) { float e = semi - (float)d; if (e > 6.f) e -= 12.f; if (e < -6.f) e += 12.f; e = fabsf(e); if (e < best) best = e; }
            const float x = best * 100.f / combW; mask = expf(-x * x);
        }
        return (1.f - combLvl) + combLvl * (0.03f + 0.97f * mask);
    }

    // ---- spectral callbacks
    void frameBegin(Stft& s) {
        const float hopS = (float)s.H / srI;
        floorLin = dbToLin(sm[P_GATE]); const float gateBin = floorLin * 0.3183f * (float)s.N;
        snap = false; first = false; peakMode = sm[P_PEAK] > 0.5f; s.specCost = 6.f;
        { const float rel = 0.01f * pkP1; const float g = gateBin > rel ? gateBin : rel; gateSq = g * g; }
        if (!fresh) segPos += hopS;
        { const float ni = sm[P_INTERP]; if (lastI > 0.f) segLen *= ni / lastI; lastI = ni; }                // INTERP acts live
        if (fresh || segPos >= segLen) {
            // a snapshot is due: only if the previous frame had sound (this frame is checked at frameEnd; an empty sample is rolled back there).
            // The phase advance needs two frames of sound, the very first pad waits ~85 ms (not the attack frame).
            if (warm >= (uint32_t)(s.N + s.H) && pkP1 >= gateBin && (!fresh || validRun >= 8)) {
                snap = true; refMag = pkP1; bCnt = 0; s.specCost = 9.f;   // a snapshot frame costs ~2x per bin: let the slicer spread it (9 measured best of 6..40 for the worst block)
                if (fresh) { fresh = false; first = true; segPos = 0.f; fade = 0.f; padWarm = s.L; }
                else { sel ^= 1; segPos -= segLen; if (segPos > hopS || segPos < 0.f) segPos = 0.f; }
                const float v = sm[P_INTVAR] * 0.01f; segLen = sm[P_INTERP] * (1.f + v * (rnd01(seed) * 2.f - 1.f)); if (segLen < 0.05f) segLen = 0.05f;
            } else if (!fresh) segPos = segLen;                                                            // silence: hold the end state, retry next frame
        }
        tI = fresh ? 0.f : clampf(segPos / segLen, 0.f, 1.f);
        sCur = 0.5f - 0.5f * cosf(3.14159265f * tI);
        combLvl = sm[P_COMBLVL] * 0.01f; fund = sm[P_COMBFUND]; scaleIx = (int)(sm[P_SCALE] + 0.5f); fundHi = fund * exp2f(sm[P_NOCT]) * 1.03f;
        { const float res = sm[P_RESON] * 0.01f; combW = 5.f + (1.f - res) * (1.f - res) * 295.f; }        // peak half-width in cents: 300 .. 5
        pwDone = 0; lastP = -1; hcK = -9; sumM2 = 0.f; eAcc = 0.f; pAcc = 0.f;
    }

    void bins(Stft& s, int k0, int k1) {
        float* HX_RESTRICT oR = s.oRe[0]; float* HX_RESTRICT oI = s.oIm[0];
        const float* xr_ = s.inRe; const float* xi_ = s.inIm;
        { int hi = k1 + 8; if (hi > SPECN + 8) hi = SPECN + 8; for (int j = pwDone; j < hi; j++) pw[j] = xr_[j] * xr_[j] + xi_[j] * xi_[j]; if (hi > pwDone) pwDone = hi; }
        float *Mf = M[sel], *Mt = M[sel ^ 1], *Qfr = Qr[sel], *Qfi = Qi[sel], *Qtr = Qr[sel ^ 1], *Qti = Qi[sel ^ 1];
        const float *Pcr = Pr[cp], *Pci = Pi[cp]; float *Pnr = Pr[cp ^ 1], *Pni = Pi[cp ^ 1];
        const float sC = sCur, wA = 1.f - sC, binHz = srI / (float)s.N;
        for (int k = k0; k < k1; k++) {
            const float xr = xr_[k], xi = xi_[k], pwk = pw[k];
            if (k > 0) { eAcc += pwk; if (pwk > pAcc) pAcc = pwk; }
            if (peakMode && pwk > pk[k]) pk[k] = pwk;
            nxRe[k] = xr; nxIm[k] = xi;
            float mf, mt, qfr, qfi, qtr, qti, cr = 0.f, ci = 0.f; bool np = false;
            if (snap) {
                const float src2 = peakMode ? pk[k] : pwk; pk[k] = 0.f;
                mf = first ? 0.f : Mf[k]; qfr = Qfr[k]; qfi = Qfi[k];
                bool take = false; int p = k;
                if (k >= 1 && src2 >= gateSq * 6e-6f) {                              // cheap reject: below -52 dB of the gate can never pass
                    {   // hill-climb to the local spectral peak (<= 6 bins: a sine-window lobe falls monotonically); neighbours on the same slope reuse the last result
                        const float cl = pw[k - 1], c0 = pw[k], cr2 = pw[k + 1];
                        const int d = (cl > c0 && cl >= cr2 && k > 1) ? -1 : (cr2 > c0 ? 1 : 0);
                        if (k == hcK + 1 && ((d < 0 && hcP < k && k - hcP <= 6) || (d > 0 && hcP > k && hcP - k <= 5))) p = hcP;
                        else {
                            p = k;
                            for (int q = 0; q < 6; q++) {
                                const float a0 = pw[p], al = pw[p - 1], ar2 = pw[p + 1];
                                if (al > a0 && al >= ar2 && p > 1) p--; else if (ar2 > a0) p++; else break;
                            }
                        }
                        hcK = k; hcP = p;
                    }
                    { const float refP = pw[p] > src2 ? pw[p] : src2; take = refP >= gateSq && src2 >= 6e-6f * refP; }   // GATE judges the PARTIAL (its peak), not each skirt bin: a partial is never cut into a truncated lobe
                    if (take && p != lastP) {                                          // new peak: phase advance, live phase, comb weight (once per partial)
                        lastP = p;
                        const float ar = xr_[p] * prRe[p] + xi_[p] * prIm[p], ai = xi_[p] * prRe[p] - xr_[p] * prIm[p], m2 = ar * ar + ai * ai;
                        if (m2 > 1e-30f) { const float iv = 1.f / sqrtf(m2); lqr = ar * iv; lqi = ai * iv; }
                        else { static const float cc[4] = {1.f, 0.f, -1.f, 0.f}, ss[4] = {0.f, 1.f, 0.f, -1.f}; lqr = cc[p & 3]; lqi = ss[p & 3]; }
                        if (pw[p] > 0.f) { const float iv = 1.f / sqrtf(pw[p]); lpr = xr_[p] * iv; lpi = xi_[p] * iv; } else { lpr = 1.f; lpi = 0.f; }
                        lWgt = 1.f;
                        if (combLvl > 0.001f) {                                        // true frequency = bin p + wrapped phase deviation / (pi/2)
                            static const float off[4] = {0.f, 1.5707963f, 3.1415927f, -1.5707963f};
                            float dph = atan2f(lqi, lqr) - off[p & 3]; if (dph > 3.1415927f) dph -= 6.2831853f; if (dph < -3.1415927f) dph += 6.2831853f;
                            lWgt = combGain(((float)p + dph * 0.63661977f) * binHz);
                        }
                    }
                }
                if (take) {
                    bCnt++; mt = sqrtf(src2) * lWgt; qtr = lqr; qti = lqi;
                    if (first || mf <= 0.f) {                                          // a bin that starts to sound: lock to its partial
                        np = true; qfr = qtr; qfi = qti;
                        float br, bi;
                        if (!first && Mf[p] > 0.f) { br = Pcr[p]; bi = Pci[p]; } else { br = lpr; bi = lpi; }
                        if (k != p) { br = -br; bi = -bi; }              // sine-window lobe: W(nu) ~ 1/(1-4nu^2): same phase for every bin, sign - outside |nu| < 0.5
                        cr = br; ci = bi;
                    }
                } else { mt = 0.f; qtr = qfr; qti = qfi; }
                Mt[k] = mt; Qtr[k] = qtr; Qti[k] = qti;
                if (first) { mf = mt; Mf[k] = mt; qfr = qtr; qfi = qti; Qfr[k] = qfr; Qfi[k] = qfi; }
                else if (np) { Qfr[k] = qfr; Qfi[k] = qfi; }
            } else { mf = Mf[k]; mt = Mt[k]; }
            if (mf <= 0.f && mt <= 0.f) { oR[k] = 0.f; oI[k] = 0.f; continue; }       // dead bin
            if (!snap) { qfr = Qfr[k]; qfi = Qfi[k]; qtr = Qtr[k]; qti = Qti[k]; }
            if (!np) { cr = Pcr[k]; ci = Pci[k]; }
            const float mu = wA * mf + sC * mt; sumM2 += mu * mu;
            oR[k] = mu * cr; oI[k] = mu * ci;
            // rotate by the cross-faded per-hop advance and keep the phasor on the unit circle
            float nr = wA * qfr + sC * qtr, ni = wA * qfi + sC * qti; const float n2 = nr * nr + ni * ni;
            if (n2 > 1e-4f) { const float iv = 1.f / sqrtf(n2); nr *= iv; ni *= iv; } else { nr = qfr; ni = qfi; }
            const float pr = cr * nr - ci * ni, pi = cr * ni + ci * nr, g = 1.5f - 0.5f * (pr * pr + pi * pi);
            Pnr[k] = pr * g; Pni[k] = pi * g;
        }
    }

    void frameEnd(Stft& s) {
        if (snap && bCnt == 0) {   // the sampled frame was empty (note cut / release): undo, keep the pad and try again next frame
            if (first) { fresh = true; fade = 0.f; padWarm = 0; }
            else { const size_t nb = sizeof(float) * (SPECN + 8); memcpy(M[sel ^ 1], M[sel], nb); memcpy(Qr[sel ^ 1], Qr[sel], nb); memcpy(Qi[sel ^ 1], Qi[sel], nb); segPos = segLen; }
            first = false;
        }
        pkP2 = pkP1; pkP1 = sqrtf(pAcc); validRun = pkP1 >= floorLin * 0.3183f * (float)s.N ? validRun + 1 : 0; lastRms = 2.f * sqrtf(eAcc) / (float)s.N;
        // feed-forward level: the pad's RMS at unit gain from the snapshot mix (Parseval); process() adds the slow measured correction
        { const float rms1 = 2.f * sqrtf(sumM2) / (float)s.N;
          if (rms1 > 1e-7f) { const float tgt = clampf(0.1f / rms1, 0.f, 300.f); if (first) { gFeed = tgt; corr = 1.f; padP = rms1 * rms1; } else gFeed += (tgt - gFeed) * (1.f - expf(-(float)s.H / srI / 0.1f)); } }
        cp ^= 1; float* t = prRe; prRe = nxRe; nxRe = t; t = prIm; prIm = nxIm; nxIm = t;
    }

    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        const float* inp = L;
        if (stereoIn) { for (int i = 0; i < n; i++) mono[i] = 0.5f * (L[i] + R[i]); inp = mono; }
        {   // 48k -> 12k, STFT, 12k -> 48k through a small FIFO (any block size)
            int m = 0; for (int i = 0; i < n; i++) { float x1, y; if (d1.push(inp[i], &x1) && d2.push(x1, &y)) mi[m++] = y; }
            float* outs[1] = {wi}; const bool wasClr = st.clr.active; st.process(mi, outs, m, *this); if (!wasClr && warm < (1u << 30)) warm += (uint32_t)m;
            if (fresh) { for (int j = 0; j < m; j++) wi[j] = 0.f; }
            else if (m > 0) {   // level + fade, per sample (linear ramps over the block)
                const float dt = (float)m / srI;
                if (padWarm > 0) padWarm -= m;                                                     // pad audio has not reached the output yet (STFT latency)
                else {   // measured unit-gain pad power (80 ms window) -> slow multiplicative correction of the feed-forward gain
                    float e = 0.f; for (int j = 0; j < m; j++) e += wi[j] * wi[j]; e /= (float)m;
                    padP += (e - padP) * (dt / (0.08f + dt));
                    const float rr = clampf(0.1f / (gFeed * sqrtf(padP) + 1e-9f), 0.25f, 4.f); corr += (rr - corr) * (dt / (2.0f + dt));
                }
                const float g0 = gOut, g1 = clampf(gFeed * corr, 0.f, 400.f);
                const float f0 = fade, f1 = clampf(fade + dt * (1.f / 0.4f), 0.f, 1.f);
                const float h0 = g0 * f0 * f0 * (3.f - 2.f * f0), h1 = g1 * f1 * f1 * (3.f - 2.f * f1), hs_ = (h1 - h0) / (float)m; float h = h0;
                for (int j = 0; j < m; j++) { h += hs_; wi[j] *= h; }
                gOut = g1; fade = f1;
            }
            for (int j = 0; j < m; j++) { float a0, a1, b0, b1; u2.push(wi[j], &a0, &a1); u1.push(a0, &b0, &b1); fq[fqn++] = b0; fq[fqn++] = b1; u1.push(a1, &b0, &b1); fq[fqn++] = b0; fq[fqn++] = b1; }
            for (int i = 0; i < n; i++) wl[i] = i < fqn ? fq[i] : 0.f;
            const int used = n < fqn ? n : fqn; for (int i = used; i < fqn; i++) fq[i - used] = fq[i]; fqn -= used;
        }
        const float faT = sm[P_FLAMT], gT = sm[P_GAIN], phInc = sm[P_FLRATE] / sr;
        const float dMaxT = clampf(sm[P_FLDEPTH] * 0.001f * sr, 1.f, (float)(FLMAX - 2));
        if (!primed) { faCur = faT; gCur = gT; dCur = dMaxT; }
        const float faInc = (faT - faCur) / (float)n, gInc = (gT - gCur) / (float)n, dInc = (dMaxT - dCur) / (float)n; float fa = faCur, g = gCur, dMax = dCur;
        const float nrm1 = 1.f / (1.f + 0.5f * faT), nrmInc = (nrm1 - 1.f / (1.f + 0.5f * faCur)) / (float)n; float nrm = 1.f / (1.f + 0.5f * faCur);   // flanger level compensation, ramped   // per-sample ramps: no zipper clicks when a knob moves
        const float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; }
        const float inc = (mt - mixCur) / (float)n; float m = mixCur;
        for (int i = 0; i < n; i++) {
            float a = finite(wl[i]), b;
            hs[hsw] = a; b = hs[(hsw - HSDLY) & (HSMAX - 1)]; hsw = (hsw + 1) & (HSMAX - 1);
            { const float mid = a; a = (mid + HSG * b) * HSN; b = (mid - HSG * b) * HSN; }   // delayed-copy mid/side: mono sum stays the pad
            if (fa > 0.001f || faT > 0.001f) {
                fdL[fw] = a; fdR[fw] = b;
                const float lfoL = 0.5f + 0.5f * sinPar(flPh), lfoR = 1.f - lfoL;   // L and R sweep in anti-phase
                const float dL = 1.f + lfoL * dMax, dR = 1.f + lfoR * dMax;
                float pL = (float)fw - dL; if (pL < 0.f) pL += FLMAX; int iL = (int)pL; const float fL = pL - (float)iL; if (iL >= FLMAX) iL -= FLMAX; const int jL = iL + 1 >= FLMAX ? 0 : iL + 1;   // (pL + FLMAX can round up to exactly FLMAX)
                float pR = (float)fw - dR; if (pR < 0.f) pR += FLMAX; int iR = (int)pR; const float fR = pR - (float)iR; if (iR >= FLMAX) iR -= FLMAX; const int jR = iR + 1 >= FLMAX ? 0 : iR + 1;
                const float yl = fdL[iL] + (fdL[jL] - fdL[iL]) * fL, yr = fdR[iR] + (fdR[jR] - fdR[iR]) * fR;
                a = (a + fa * yl) * nrm; b = (b + fa * yr) * nrm;
                if (++fw >= FLMAX) fw = 0; flPh += phInc; if (flPh >= 1.f) flPh -= 1.f;
            }
            {   // safety limiter 2*softclip(x/2) on both channels with one division (x(27+x^2)/(27+9x^2)), ~1 % compression at 0.4
                const float u = clampf(0.5f * a, -3.f, 3.f), v = clampf(0.5f * b, -3.f, 3.f), u2 = u * u, v2 = v * v, du = 27.f + 9.f * u2, dv = 27.f + 9.f * v2, ri = 2.f / (du * dv) * (g * m);
                const float d0 = L[i], d1 = stereoIn ? R[i] : d0;
                L[i] = flush(d0 + u * (27.f + u2) * dv * ri); R[i] = flush(d1 + v * (27.f + v2) * du * ri); m += inc; fa += faInc; g += gInc; dMax += dInc; nrm += nrmInc;
            }
        }
        mixCur = mt; faCur = faT; gCur = gT; dCur = dMaxT; return true;
    }
};
inline Fx* fx_create_drone(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxDrone)); return m ? new (m) FxDrone(a, sr) : nullptr; }
}

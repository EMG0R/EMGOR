// fx_dronemaker.h -- DRONEMAKER: faithful C++ port of the NEPTR SM_DroneMaker kernel (the Csound re-implementation of Michael Norris's
// "Spectral DroneMaker", SoundMagic Spectral AU "SDmk"). NEW 2026-11-17. Class FxDroneMaker, factory fx_create_dronemaker.
// fx_drone.h is a DIFFERENT effect (peak-tracking oscillator bank, kept and renamed "Loose Follow"); nothing here shares code with it.
//
// WHAT NEPTR ACTUALLY RUNS (read-only audit, DEMIURGE_OS/neptr/csound/spectral/, NOT the legacy/NEPTR phase4 tree, which has no DroneMaker):
//   sm_dronemaker.orc:23-190   SM_DroneMaker UDO (per-bin A->B magnitude+frequency interpolation, comb bank, flanger, gate)
//   sm_core.orc:118-173        SM_Common, run after the kernel: lo/hi cutoff, brightness tilt, RANDOMIZE PHASES, gain
//   sm_rack.orc:213-234        pvsanal(iN, hop iN/4, win iN, fmt 1, Hann) -> pvs2tab -> kernels -> tab2pvs -> pvsynth -> dcblock -> limit +-0.99
//   sm_rack.orc:49-54          kernel then SM_Common; params default from sm_params.json (fx 2 "DRONE"); FFT Size default idx 2 = 2048 @ 48 kHz
//   signal_chain.orc:391-407   rack hand-off: the effect is 100 % WET while on (no mix control); bypass = bit-exact dry.
//   It is a pvs (phase-vocoder) effect, NOT the AU hosted: no pvsmooth/pvsfreeze/pvsmorph/pvsblur/partikkel/resonators; tables are only the param store.
// Measured on Csound 6.18 (sm_core.orc:1-9): pvsanal's frame is amp + TRUE FREQUENCY (Hz) and pvsynth accumulates phase from it.
//
// PORT DESIGN
//  * STFT through fx_fft.h Stft (current API, untouched) at 24 kHz via Resamp2 half-bands: N = 1024 @ 24 kHz has the SAME 23.4375 Hz bins and the SAME
//    hop (256 samples = 10.667 ms, N/4) as NEPTR's 2048 @ 48 kHz. Spectrum covers 0..12 kHz (everything above is dropped: trumpet + dynamic mic).
//    Deviation (forced by the API): Stft uses sqrt-Hann analysis AND synthesis (net Hann) instead of pvsanal's Hann + pvsynth window. Bin magnitudes are
//    normalised by sum(w)/2 so a full-scale bin-centred sine reads 1.0, exactly the Csound amplitude scale (the gate threshold is in that scale).
//  * Kernel = sm_dronemaker.orc line for line: per bin A,B,peak, phase p (0..1, random start), length T +-variance (random per segment, floor 0.05 s),
//    out = (1-p)A + pB, at p>=1 A=B and B = live frame (or the segment's peak in PEAKAMP mode), gate, spectral flanger |cos(pi f d)| (d swept by an LFO),
//    comb bank (max over notes of 1/sqrt(1+d^2) around every harmonic; built incrementally, double buffered, restarts on any change of
//    fund/scale/octaves/resonance). Per-bin work that only matters at a resample (sqrt, frequency estimate) is done lazily: in the default mode the per-frame
//    cost per bin is one phase step, one lerp, one random phase and two table reads.
//  * RANDOMIZE PHASES = 1 (default, as in NEPTR): sm_core.orc:166 overwrites every bin's frequency with (k + U(-2,2)) bins, i.e. a phase increment that is
//    uniform over a full circle per hop, which IS a fresh uniform random phase per bin per frame. So the true-frequency estimate (atan2) is dead code in the
//    default mode and is not computed; phase = random table index. RANDOMIZE = 0: pvsanal's phase-vocoder estimate (phase difference minus k*pi/2, wrapped,
//    bin freq = (k + 2*dev/pi) * binHz; fast atan2, only evaluated where the kernel reads it) and pvsynth's accumulated phase per bin (inverse FFT frame
//    from the per-bin phase accumulator, phase += 2*pi*Fo*hop/sr, starting at 0) are reproduced.
//  * Output: dcblock (g 0.99 at 48 kHz, 0.9801 at 24 kHz = same corner), limit as in the rack, MIX = linear dry/wet crossfade (the law every NEPTR send/return
//    uses: dry*(1-m) + wet*m, ramped per block). NEPTR has no MIX on DroneMaker (100 % wet), so default MIX = 1; MIX = 0 is the undelayed dry.
//  * Latency of the wet path: N + H - 1 = 1279 samples @ 24 kHz (53 ms), plus the half-band FIRs.
//  * Dropped params (16-param limit): Lo/Hi bin cutoff (fixed 0..100 %), FFT Size (fixed 2048-equivalent). Everything else of the 18 NEPTR params is here.
//  * Comb bank above 12 kHz is not built (no spectrum there). Peak-mode segment peak is only tracked while PEAKAMP is on (NEPTR tracks it always).
//
// Params: MIX INTERP INTVAR PEAKAMP BRIGHT COMBLVL COMBFUND SCALE OCTAVES RESON FLAMT FLDEPTH FLRATE GATE RNDPHASE GAIN.
// Roles: AMT=MIX TIME=INTERP TONE=BRIGHT MOD=INTVAR PITCH=COMBFUND.  Actions: A_CLEAR = restart every bin segment from the live frame.
// State: all in instance members + the arena; no statics except const tables. wake() zeroes everything (lazily over the next blocks, the wet is muted
// meanwhile); wakeNow() zeroes at once (host / construction).
#pragma once
#include "fx_common.h"
#include "fx_fft.h"

namespace hx {

struct FxDroneMaker : Fx {
    enum { P_MIX, P_INTERP, P_INTVAR, P_PEAK, P_BRIGHT, P_COMBLVL, P_COMBFUND, P_SCALE, P_OCTAVES, P_RESON, P_FLAMT, P_FLDEPTH, P_FLRATE, P_GATE, P_RNDPH, P_GAIN };

    static const int CAPN = 1024, NB = CAPN / 2 + 1, NS = 13, TN = 2048;   // STFT size @24 kHz, bins, per-bin arrays, trig table size
    static constexpr float WET_TRIM = 0.92f;                              // calibrated against the Csound render (see tests)
    Stft st; Resamp2 rs;
    float *A = nullptr, *B = nullptr, *Pk = nullptr, *Ph = nullptr, *Inc = nullptr, *G = nullptr, *Gb = nullptr,
          *FA = nullptr, *FB = nullptr, *FPk = nullptr, *PRe = nullptr, *PIm = nullptr, *Sp = nullptr, *slab = nullptr, *sinT = nullptr;
    const float* cosT = nullptr;
    float srI = 24000.f, binHz = 23.4375f, dt = 256.f / 24000.f;

    // per-frame constants (frameBegin)
    bool peakOn = false, rnd = true, useComb = false, gValid = false, first = true, prValid = false, wetOn = true;
    float lenLo = 3.f, lenHi = 5.f, cl = 0.f, flA = 0.f, flU = 0.f, gateA = 0.f, tiltStep = 1.f, wetScale = 1.f, flPh = 0.f;
    // comb bank builder
    float cS0 = -1.f, cS1 = -1.f, cS2 = -1.f, cS3 = -1.f, cFl = 0.f, cHm = 0.f, cF = 0.f, cInvHw = 0.f;
    int cNote = 0, cTot = 0, cSpB = 0, cJ = -1, cJE = -1; bool cPend = false, cActive = false, swapPend = false;
    uint32_t seed = 0x9E3779B9u, rs32 = 0x1234567u;
    DcBlock dcb;
    float mixCur = 0.f; bool primed = false;
    float mono[HX_MAXBLOCK], mi[HX_MAXBLOCK / 2 + 2], wi[HX_MAXBLOCK / 2 + 2], fifo[2 * HX_MAXBLOCK + 4]; int fifoN = 0;

    static constexpr int SCL[10][7] = {{0,-1,-1,-1,-1,-1,-1},{0,-1,-1,-1,-1,-1,-1},{0,7,-1,-1,-1,-1,-1},{0,4,7,-1,-1,-1,-1},{0,3,7,-1,-1,-1,-1},{0,2,4,5,7,9,11},{0,2,3,5,7,9,10},{0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,2,3,5,7,8,10}};   // sm_dronemaker.orc:50, 7 slots per row
    static constexpr int CNT[10] = {1, 1, 2, 3, 3, 7, 7, 7, 7, 7};                                                                                                          // sm_dronemaker.orc:51

    FxDroneMaker(Arena& a, float s) {
        name = "DRONEMKR"; sr = s; srI = s * 0.5f; rs.init(15, 5.0f); dcb.R = 0.9801f;
        static const char* const scales[] = {"None", "Octaves", "Fifths", "Maj triad", "Min triad", "Major", "Dorian", "Lydian", "Mixolyd", "Aeolian"};
        P(P_MIX,     "MIX",      0.f, 1.f, 1.f, 0.f, PF_PERCENT);                       // NEPTR: 100 % wet while on
        P(P_INTERP,  "INTERP",   0.1f, 30.f, 4.f, 0.f, PF_LOG);                          // Interpolation length, s
        P(P_INTVAR,  "INTVAR",   0.f, 10.f, 2.f, 0.f, 0);                                // Interp length variance, s
        P(P_PEAK,    "PEAKAMP",  0.f, 1.f, 0.f, 1.f, PF_INT | PF_QUANT);                 // Use peak amplitudes
        P(P_BRIGHT,  "BRIGHT",   -100.f, 100.f, 0.f, 0.f, PF_BIPOLAR);                   // Brightness, % (+-24 dB tilt over the full band)
        P(P_COMBLVL, "COMBLVL",  0.f, 100.f, 0.f, 0.f, PF_PERCENT);                      // Comb filterbank level
        P(P_COMBFUND,"COMBFUND", 20.f, 2000.f, 200.f, 0.f, PF_LOG);                      // Comb filter fundamental, Hz
        P(P_SCALE,   "SCALE",    0.f, 9.f, 6.f, 1.f, PF_ENUM | PF_QUANT, scales);        // Scale type (default Dorian)
        P(P_OCTAVES, "OCTAVES",  1.f, 10.f, 2.f, 1.f, PF_INT | PF_QUANT);                // Number of octaves
        P(P_RESON,   "RESON",    1.f, 99.f, 99.f, 0.f, 0);                               // Filter resonance, %
        P(P_FLAMT,   "FLAMT",    0.f, 100.f, 0.f, 0.f, PF_PERCENT);                      // Flanger amount
        P(P_FLDEPTH, "FLDEPTH",  0.1f, 15.f, 2.f, 0.f, 0);                               // Flanger depth, ms
        P(P_FLRATE,  "FLRATE",   0.01f, 5.f, 0.1f, 0.f, PF_LOG);                         // Flanger rate, Hz
        P(P_GATE,    "GATE",     -60.f, 0.f, -60.f, 0.f, 0);                             // Gate level, dB (-60 = off)
        P(P_RNDPH,   "RNDPHASE", 0.f, 1.f, 1.f, 1.f, PF_INT | PF_QUANT);                 // Randomize phases
        P(P_GAIN,    "GAIN",     -40.f, 40.f, 0.f, 0.f, PF_BIPOLAR);                     // Gain, dB
        setRole(R_AMT, P_MIX); setRole(R_TIME, P_INTERP); setRole(R_TONE, P_BRIGHT); setRole(R_MOD, P_INTVAR); setRole(R_PITCH, P_COMBFUND);
        st.setHop(4); st.slicedLoad = true; st.capN = CAPN;
        ok = st.init(a, 1, 1, true, 9.f);                                                // size idx 1 = 1024 points, one inverse FFT, sliced
        slab = a.floats((size_t)NS * NB); sinT = a.floats(TN + TN / 4 + 1);
        if (!slab || !sinT) ok = false;
        if (!ok) return;
        A = slab; B = A + NB; Pk = B + NB; Ph = Pk + NB; Inc = Ph + NB; G = Inc + NB; Gb = G + NB; FA = Gb + NB; FB = FA + NB; FPk = FB + NB; PRe = FPk + NB; PIm = PRe + NB; Sp = PIm + NB;
        for (int i = 0; i < TN + TN / 4 + 1; i++) sinT[i] = (float)sin(6.283185307179586 * (double)i / (double)TN);
        cosT = sinT + TN / 4;                                                            // cos(x) = sin(x + pi/2)
        binHz = srI / (float)CAPN; dt = (float)(CAPN / 4) / srI;
        wakeNow();
    }

    // ---------------------------------------------------------------- state clear
    void clearScalars() {
        first = true; prValid = false; gValid = false; cPend = false; cActive = false; swapPend = false; cJ = -1; cNote = 0; cTot = 0;
        cS0 = cS1 = cS2 = cS3 = -1.f; flPh = 0.37f; seed = 0x9E3779B9u; rs32 = 0x1234567u; dcb.x1 = dcb.y1 = 0.f;
        mixCur = 0.f; primed = false; fifoN = 1; fifo[0] = 0.f; wetOn = false;               // fifo primed with one sample: odd block sizes stay in step
        rs.clear();
    }
    void wakeNow() { if (!ok) return; st.setSize(1); st.consumeReset(); memset(slab, 0, sizeof(float) * (size_t)NS * NB); clearScalars(); wetOn = true; }
    void wake() { if (!ok) return; st.setSizeLazy(1); st.consumeReset(); st.clr.add(slab, NS * NB); st.clr.arm(); clearScalars(); wetOn = true; }   // lazy: ~15 blocks, wet muted meanwhile
    void reset() override { wake(); }
    void action(int id, float value) override { if (id == A_CLEAR && value > 0.5f) first = true; }

    inline uint32_t rnd32() { rs32 ^= rs32 << 13; rs32 ^= rs32 >> 17; rs32 ^= rs32 << 5; return rs32; }
    inline float rnd01f() { return (float)(rnd32() >> 8) * (1.f / 16777216.f); }

    // ---------------------------------------------------------------- comb bank (sm_dronemaker.orc:68-124), work-budgeted, resumable
    static inline float rsq(float x) { union { float f; uint32_t i; } u; u.f = x; u.i = 0x5f3759dfu - (u.i >> 1); float y = u.f; y = y * (1.5f - 0.5f * x * y * y); return y; }   // 1/sqrt(x), rel. error < 0.2 %
    void combStep(int budget) {
        const float fund = v[P_COMBFUND], res = clampf(v[P_RESON], 1.f, 99.f) * 0.01f;
        const int scl = (int)clampf(floorf(v[P_SCALE] + 0.5f), 0.f, 9.f), oct = (int)clampf(floorf(v[P_OCTAVES] + 0.5f), 1.f, 10.f);
        useComb = (v[P_COMBLVL] > 0.f && scl > 0);
        if (!useComb) return;
        if (fabsf(fund - cS0) > 0.0005f * fund || (float)scl != cS1 || (float)oct != cS2 || fabsf(res - cS3) > 0.0005f) {
            cS0 = fund; cS1 = (float)scl; cS2 = (float)oct; cS3 = res; cNote = 0; cPend = true; cActive = false; cJ = -1; cTot = oct * CNT[scl];
            cFl = (1.f - res) / (1.f + res); for (int q = 0; q < NB; q++) Gb[q] = cFl;
        }
        const float nyq = srI * 0.5f;
        while (cPend && budget > 0) {
            if (!cActive) {
                if (cNote >= cTot) { cPend = false; swapPend = true; break; }
                const int no = cNote / CNT[scl], ns = cNote - no * CNT[scl];
                const float F = fund * exp2f((float)no + (float)SCL[scl][ns] * (1.f / 12.f));
                if (!(F < nyq && F > 1.f)) { cNote++; continue; }
                float hw = F * (1.f - res) / 6.2832f; if (hw < 0.8f * binHz) hw = 0.8f * binHz;
                float span = 8.f * hw; if (span > F * 0.5f) span = F * 0.5f; if (span > 64.f * binHz) span = 64.f * binHz;
                cSpB = (int)(span / binHz) + 1; cF = F; cInvHw = 1.f / hw; cHm = F; cJ = -1; cActive = true;
            }
            if (cJ < 0) {
                if (cHm >= nyq) { cActive = false; cNote++; continue; }
                const int c = (int)(cHm / binHz + 0.5f); cJ = c - cSpB < 0 ? 0 : c - cSpB; cJE = c + cSpB > NB - 1 ? NB - 1 : c + cSpB;
            }
            while (cJ <= cJE && budget > 0) {
                const float d = ((float)cJ * binHz - cHm) * cInvHw, g = rsq(1.f + d * d);
                if (g > Gb[cJ]) Gb[cJ] = g;
                cJ++; budget--;
            }
            if (cJ > cJE) { cHm += cF; cJ = -1; }
        }
    }

    // ---------------------------------------------------------------- spectral callback (Stft, sliced across blocks)
    float tiltBr = 0.f, gainLin = 1.f, flDepthS = 0.002f, gateDb = -60.f;
    void frameBegin(Stft& s) {
        const float T = v[P_INTERP], V = v[P_INTVAR];
        lenLo = T - V; if (lenLo < 0.05f) lenLo = 0.05f; lenHi = T + V; if (lenHi < lenLo) lenHi = lenLo;
        const bool pk = v[P_PEAK] >= 0.5f; if (pk && !peakOn) { for (int k = 0; k < NB; k++) Pk[k] = 0.f; } peakOn = pk;
        const bool r = v[P_RNDPH] >= 0.5f; if (!r && rnd) prValid = false; rnd = r;
        if (swapPend) { memcpy(G, Gb, sizeof(float) * NB); swapPend = false; gValid = true; }
        cl = clampf(v[P_COMBLVL], 0.f, 100.f) * 0.01f;
        const int scl = (int)clampf(floorf(v[P_SCALE] + 0.5f), 0.f, 9.f); useComb = (cl > 0.f && scl > 0 && gValid);
        flA = clampf(sm[P_FLAMT], 0.f, 100.f) * 0.01f;
        const float fd = (sm[P_FLDEPTH] < 0.01f ? 0.01f : sm[P_FLDEPTH]) * 0.001f;
        flPh += dt * sm[P_FLRATE]; flPh -= (float)(int)flPh;
        const float d = fd * (0.5f + 0.5f * sinT[(int)(flPh * (float)TN) & (TN - 1)]) + 0.00001f;
        flU = binHz * d * 0.5f;                                                          // |cos(pi k binHz d)| = |cos(2 pi k flU)|, flU in turns per bin
        gateDb = sm[P_GATE]; gateA = gateDb > -59.9f ? dbToLin(gateDb) : 0.f;
        const float br = sm[P_BRIGHT]; tiltBr = br; tiltStep = br != 0.f ? powf(10.f, br * 0.012f / 1025.f) : 1.f;
        gainLin = dbToLin(sm[P_GAIN]);
        wetScale = gainLin * (float)CAPN * (1.f / 3.14159265f) * WET_TRIM;               // amplitude units -> Stft bin units (N / pi)
        (void)s;
    }
    static inline float fastAtan2(float y, float x) {                                    // |err| < 1e-4 rad
        const float ax = fabsf(x), ay = fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
        if (mx < 1e-30f) return 0.f;
        const float a = mn / mx, q = a * a;
        float r = ((-0.0464964749f * q + 0.15931422f) * q - 0.327622764f) * q * a + a;
        if (ay > ax) r = 1.57079633f - r;
        if (x < 0.f) r = 3.14159265f - r;
        return y < 0.f ? -r : r;
    }
    // Hann-equivalent analysis. NEPTR analyses with a Hann window; Stft hands out sqrt-Hann spectra X_s of the same frames. Since Hann = sqrt-Hann * sqrt-Hann,
    // X_h[k] = (1/N) sum_j C[j] X_s[k-j] with C = DFT of the sqrt-Hann window = (2N/pi) / (1-4 j^2): an EXACT convolution (truncated at |j| <= 4, tail < -40 dB),
    // so the bin amplitudes AND phases are those pvsanal would have produced (a bin-centred sine reads .5/1/.5 across three bins, not the sqrt-Hann sidelobe
    // forest). Scaled to the Csound amplitude unit (full-scale bin-centred sine = 1.0): 4/N * 2/pi * sum.
    static constexpr int JH = 4;
    inline void hannBin(const Stft& s, int k, float& hr, float& hi) const {
        const float* R = s.inRe; const float* I = s.inIm;
        static constexpr float g1 = -1.f / 3.f, g2 = -1.f / 15.f, g3 = -1.f / 35.f, g4 = -1.f / 63.f;
        float ar, ai;
        if (k >= JH && k <= NB - 1 - JH) {
            ar = R[k] + g1 * (R[k - 1] + R[k + 1]) + g2 * (R[k - 2] + R[k + 2]) + g3 * (R[k - 3] + R[k + 3]) + g4 * (R[k - 4] + R[k + 4]);
            ai = I[k] + g1 * (I[k - 1] + I[k + 1]) + g2 * (I[k - 2] + I[k + 2]) + g3 * (I[k - 3] + I[k + 3]) + g4 * (I[k - 4] + I[k + 4]);
        } else {
            static constexpr float gg[JH + 1] = {1.f, g1, g2, g3, g4}; ar = R[k]; ai = I[k];
            for (int j = 1; j <= JH; j++) {
                int a = k - j, b = k + j; float ra, ia, rb, ib;
                if (a < 0) { ra = R[-a]; ia = -I[-a]; } else { ra = R[a]; ia = I[a]; }                 // X[-m] = conj X[m]
                if (b > NB - 1) { rb = R[2 * (NB - 1) - b]; ib = -I[2 * (NB - 1) - b]; } else { rb = R[b]; ib = I[b]; }   // X[M+m] = conj X[M-m]
                ar += gg[j] * (ra + rb); ai += gg[j] * (ia + ib);
            }
        }
        const float hk = 8.f / (3.14159265f * (float)CAPN); hr = ar * hk; hi = ai * hk;
    }
    inline float estFreq(int k, float re, float im) const {                              // pvsanal true frequency of bin k (Hz); bin centre until a previous frame exists
        if (!prValid) return (float)k * binHz;
        const float pr = PRe[k], pi = PIm[k];
        float dev = fastAtan2(im * pr - re * pi, re * pr + im * pi) - (float)(k & 3) * 1.57079633f;
        if (dev < -3.14159265f) dev += 6.2831853f; if (dev < -3.14159265f) dev += 6.2831853f; if (dev > 3.14159265f) dev -= 6.2831853f;
        return ((float)k + dev * 0.63661977f) * binHz;
    }
    void bins(Stft& s, int k0, int k1) {
        float* oR = s.oRe[0]; float* oI = s.oIm[0];
        const bool useC = useComb, pkOn = peakOn, rn = rnd, gate = gateA > 0.f, fl = flA > 0.f, tilt = tiltBr != 0.f, fst = first;
        const float cl1 = 1.f - cl, fA1 = 1.f - flA, fAm = flA, ws = wetScale;
        float acc = 1.f, fu = (float)k0 * flU; fu -= (float)(int)fu;
        if (tilt) acc = fastExp2((float)k0 * log2f(tiltStep));
        for (int k = k0; k < k1; k++) {
            float hr = 0.f, hi = 0.f, M = 0.f; bool haveH = false, haveM = false;
            if (!rn || fst || pkOn) { hannBin(s, k, hr, hi); haveH = true; }                  // coherent mode needs the complex bin every frame (previous-frame phase)
            if (fst || pkOn) { M = sqrtf(hr * hr + hi * hi); if (useC) M *= cl1 + cl * G[k]; haveM = true; }
            if (fst) {
                const float f0 = rn ? 0.f : estFreq(k, hr, hi);
                A[k] = B[k] = Pk[k] = M; FA[k] = FB[k] = FPk[k] = f0;
                Ph[k] = rnd01f(); Inc[k] = dt / (lenLo + (lenHi - lenLo) * rnd01f());
            } else {
                if (pkOn && M >= Pk[k]) { Pk[k] = M; if (!rn) FPk[k] = estFreq(k, hr, hi); }
                float p = Ph[k] + Inc[k];
                if (p >= 1.f) {
                    if (!haveM) { if (!haveH) hannBin(s, k, hr, hi); M = sqrtf(hr * hr + hi * hi); if (useC) M *= cl1 + cl * G[k]; }
                    const float f0 = rn ? 0.f : estFreq(k, hr, hi);
                    A[k] = B[k]; FA[k] = FB[k];
                    if (pkOn) { B[k] = Pk[k]; FB[k] = FPk[k]; } else { B[k] = M; FB[k] = f0; }
                    Pk[k] = M; FPk[k] = f0; p = 0.f;
                    Inc[k] = dt / (lenLo + (lenHi - lenLo) * rnd01f());
                }
                Ph[k] = p;
            }
            const float p = Ph[k], wa = (1.f - p) * A[k], wb = p * B[k]; float O = wa + wb;
            float Fo = 0.f;
            if (!rn) Fo = O > 0.f ? (wa * FA[k] + wb * FB[k]) / O : (float)k * binHz;
            if (gate && O < gateA) O = 0.f;
            if (fl) { O *= fA1 + fAm * fabsf(cosT[(int)(fu * (float)TN) & (TN - 1)]); fu += flU; fu -= (float)(int)fu; }
            if (tilt) { O *= acc; acc *= tiltStep; }
            O *= ws;
            if (rn) {
                const uint32_t ix = rnd32() >> 21;                                       // 11 bits = TN
                oR[k] = O * cosT[ix]; oI[k] = O * sinT[ix];
            } else {
                float sp = Sp[k] + Fo * (1.f / (4.f * binHz)); sp -= (float)(int)sp; Sp[k] = sp;   // turns per hop = Fo * hop / sr = (Fo / binHz) / 4
                // Lobe signs. pvsynth's phases are window-centred, where a steady partial's Hann lobes are all in phase; Stft's are frame-start (offset pi*k): odd bins get +pi.
                // Hann lobes W(d) are positive for |d| < 2 and then alternate (-,+,...): bins with |d| in (2,3), (4,5) ... get another +pi, d = k - f/binHz.
                const float d = fabsf((float)k - Fo * (1.f / binHz)); const int neg = d < 2.f ? 0 : ((int)(d - 1.f) & 1);
                const int ix = ((int)(sp * (float)TN) + (((k & 1) ^ neg) << 10)) & (TN - 1);
                oR[k] = O * cosT[ix]; oI[k] = O * sinT[ix];
                PRe[k] = hr; PIm[k] = hi;
            }
        }
    }
    void frameEnd(Stft&) { first = false; if (!rnd) prValid = true; }

    static inline float lim(float x) { float ax = x < 0.f ? -x : x; if (ax <= 0.8f) return x; float y = 0.8f + 0.2f * softclip((ax - 0.8f) * 5.f); return x < 0.f ? -y : y; }   // rack limiter is +-0.99; this is a soft one
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        const float* inp = L;
        if (stereoIn) { for (int i = 0; i < n; i++) mono[i] = 0.5f * (L[i] + R[i]); inp = mono; }
        int mc = 0; float y;
        for (int i = 0; i < n; i++) if (rs.d.push(inp[i], &y)) mi[mc++] = y;
        combStep(150);
        float* outs[1] = {wi}; st.process(mi, outs, mc, *this);
        const bool waking = st.clr.active;
        for (int j = 0; j < mc; j++) {
            float w = waking ? 0.f : dcb.tick(wi[j]); float a0, a1; rs.u.push(w, &a0, &a1); fifo[fifoN++] = a0; fifo[fifoN++] = a1;
        }
        for (int i = 0; i < n; i++) wl[i] = fifo[i];
        fifoN -= n; for (int i = 0; i < fifoN; i++) fifo[i] = fifo[i + n];
        float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; }
        const float inc = (mt - mixCur) / (float)n; float m = mixCur;
        if (!stereoIn) {
            for (int i = 0; i < n; i++) { const float d = L[i], a = lim(finite(wl[i])), o = flush(d + (a - d) * m); L[i] = o; R[i] = o; m += inc; }
        } else {
            for (int i = 0; i < n; i++) { const float a = lim(finite(wl[i])), d0 = L[i], d1 = R[i]; L[i] = flush(d0 + (a - d0) * m); R[i] = flush(d1 + (a - d1) * m); m += inc; }
        }
        mixCur = mt; return true;
    }
    float wl[HX_MAXBLOCK];
};
inline Fx* fx_create_dronemaker(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxDroneMaker)); return m ? new (m) FxDroneMaker(a, sr) : nullptr; }
}

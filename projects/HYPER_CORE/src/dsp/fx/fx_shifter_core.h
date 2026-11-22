// fx_shifter_core.h -- shared pitch-synchronous time-domain pitch shifter for HYPERTRUMPET V2 (harm + mood). NEW 2026-11-15.
// Method: one shared input delay line feeds N voices. Each voice = two crossfading Hermite read heads whose delay drifts
// at (1 - ratio) samples/sample inside a grain window [DMIN, DMIN+G]; at the window edge (weight 0) the head jumps by G.
// G = 2*M*period (period from the tracker, ctx.f0) so the two heads are an integer number of periods apart and the jump
// is waveform-coherent (SOLAD / dual-delay-line shifting); unvoiced fallback G = 24 ms. Window = smoothstep triangle
// (complementary: w(p)+w(p+1/2)=1), normalised by the actual weight sum when grain lengths differ during pitch moves.
// Written from scratch for this library. The old hyperGuitar SOLAD/PSOLA code (HYPER_CORE/src/dsp/hg/shifter.cpp) is
// tightly coupled to Bela globals; it was read for ideas only, nothing is copied.
// Latency: mean read delay = DMIN + G/2 ~ 10..13 ms. Range: ratio 0.25..4 (+-24 st).
// Ops/sample per active voice (hand count): 2 Hermite reads (~22 flops each) + 2 windows/wrap tests (~14) + norm (~6) + lp (2) ~ 70.
#pragma once
#include "fx_common.h"
namespace hx {

struct ShifterCore {
    static constexpr float DMIN = 4.f;
    static constexpr float GMIN = 256.f, GMAX = 1400.f, GFALL = 1152.f;
    DelayLine dl;
    float Gt = GFALL, Gs = GFALL;      // target grain (samples), slow-smoothed copy

    bool init(Arena& a) { return dl.init(a, 1600); }
    inline void write(float x) { dl.write(x); }

    // per-block: derive grain length from the tracker (period-synchronous). Call once per block.
    // Period source (EMGOR decision 2026-11-15): tracked f0 when the tracker is confident, else the valve pitch hint (3 Hall valves + partial
    // estimate, ctx.valvePitchHz), else the 24 ms unvoiced fallback. `src` records which one was taken (tests, meters).
    int src = 0;                       // 0 fallback, 1 tracked, 2 valve hint
    float Pper = 0.f;                  // period in samples when src != 0, else 0 (read by SolaVoice / FxHarmony)
    void setPitch(float f0, float conf, float sr, float valveHz = 0.f) {
        float g = GFALL * (sr / 48000.f);
        src = 0; Pper = 0.f;
        if (!(f0 > 70.f && f0 < 1600.f && conf > 0.4f) && valveHz > 70.f && valveHz < 1600.f) { f0 = valveHz; conf = 1.f; src = 2; }
        else if (f0 > 70.f && f0 < 1600.f && conf > 0.4f) src = 1;
        if (src) {
            float P = sr / f0; Pper = P; float M = floorf(0.010f * sr / P + 0.5f); if (M < 1.f) M = 1.f;
            g = 2.f * M * P;
        }
        g = clampf(g, GMIN, GMAX);
        Gs += (g - Gs) * 0.5f; Gt = Gs;                                  // light smoothing of tracker jitter
    }
};

struct ShVoice {
    float d[2] = {0.f, 0.f}, G[2] = {ShifterCore::GFALL, ShifterCore::GFALL};
    float lp = 0.f;
    void reset(float g = ShifterCore::GFALL) { G[0] = G[1] = g; d[0] = ShifterCore::DMIN + 0.25f * g; d[1] = ShifterCore::DMIN + 0.75f * g; lp = 0.f; }

    static inline float win(float p) { float q = p < 0.5f ? 2.f * p : 2.f - 2.f * p; q = q < 0.f ? 0.f : (q > 1.f ? 1.f : q); return q * q * (3.f - 2.f * q); }

    // one output sample at pitch ratio r (read AFTER the input sample was written to the core)
    inline float tick(const ShifterCore& c, float r) {
        const float step = 1.f - r, dmin = ShifterCore::DMIN;
        float y[2], w[2];
        for (int k = 0; k < 2; k++) {
            float dd = d[k] + step;
            if (dd > dmin + G[k]) { dd -= G[k]; G[k] = c.Gt; }
            else if (dd < dmin)   { G[k] = c.Gt; dd += G[k]; }
            d[k] = dd;
            float p = (dd - dmin) / G[k];
            w[k] = win(p);
            y[k] = c.dl.read(dd);
        }
        float s = w[0] + w[1]; if (s < 0.3f) s = 0.3f;
        return (w[0] * y[0] + w[1] * y[1]) / s;
    }
};


// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// SolaVoice -- correlation-aligned dual-head shifter (HARMONY, 2026-11-17). Same two crossfading heads as ShVoice, but the splice is NOT trusted to the tracker:
// when a head wraps (weight 0) its read position is re-placed by an integer offset o in [0, Omax] that maximises the normalised cross-correlation between the
// waveform ahead of the new position and the waveform the other (active) head is playing right now (SOLA / waveform-similarity splice). The window uses the
// nominal base delay b (exactly complementary), the read position is b + o. The tracker only chooses the grain G and the search range Omax (>= 1 period); with
// no tracker (quiet input, unvoiced) Omax = 330 (covers 145 Hz and up) and G = 512. Quality no longer depends on tracker accuracy or vibrato.
// Cost per wrap: coarse search (stride 3 offsets x stride 2 samples x 128-sample window) + +-2 refine, ~7k MAC, a wrap every ~250..1100 samples per voice.
struct SolaVoice {
    static constexpr int WC = 128;          // correlation window (samples; stage 1 reads it at stride 4, stage 2 at stride 2)
    static constexpr int WMAX = 420;        // largest search range (omax is clamped to this)
    float b[2] = {0.f, 0.f}, o[2] = {0.f, 0.f}, G = 512.f;
    void reset(const ShifterCore& c, float g, float omax) { G = g; b[0] = ShifterCore::DMIN + 0.25f * g; b[1] = ShifterCore::DMIN + 0.75f * g; o[0] = 0.f; o[1] = align(c, b[0], b[1] + o[1], omax); }
    // best offset in [0,omax] for a head about to sit at nominal delay bn so that it matches the signal read at delay dref.
    // Stage 1: the candidate region (omax + WC samples) is copied once into a linear array; offsets at stride 4 are scored on a stride-4 decimated window (WC/4 taps).
    // Stage 2: +-3 offsets around the winner scored on a stride-2 window (WC/2 taps). Score = sign(dot) dot^2 / energy (normalised cross-correlation, amplitude independent).
    static inline float align(const ShifterCore& c, float bn, float dref, float omax) {
        const int dr = (int)(dref + 0.5f), base = (int)(bn + 0.5f); int om = (int)omax; if (om > WMAX) om = WMAX;
        float tmp[WMAX + WC + 8]; const int Ln = om + WC + 4;
        { const float* HX_RESTRICT bf = c.dl.buf; const uint32_t mk = c.dl.mask; const uint32_t p0 = c.dl.w - 1 - (uint32_t)base; for (int m = 0; m < Ln; m++) tmp[m] = bf[(p0 - (uint32_t)m) & mk]; }
        float ref[WC / 2]; { const float* HX_RESTRICT bf = c.dl.buf; const uint32_t mk = c.dl.mask; const uint32_t p0 = c.dl.w - 1 - (uint32_t)dr; for (int j = 0; j < WC / 2; j++) ref[j] = bf[(p0 - (uint32_t)(2 * j)) & mk]; }
        float best = -1e30f; int bo = 0;
        for (int off = 0; off <= om; off += 4) {
            float d0 = 0.f, d1 = 0.f, e0 = 1e-12f, e1 = 0.f; const float* x = tmp + off;
            for (int j = 0; j < WC / 4; j += 2) { float a0 = x[4 * j], a1 = x[4 * j + 4]; d0 += ref[2 * j] * a0; d1 += ref[2 * j + 2] * a1; e0 += a0 * a0; e1 += a1 * a1; }
            float dot = d0 + d1, e = e0 + e1; float sc = dot > 0.f ? dot * dot / e : -dot * dot / e - 1.f; if (sc > best) { best = sc; bo = off; }
        }
        const int lo = bo - 3 < 0 ? 0 : bo - 3, hi = bo + 3 > om ? om : bo + 3; best = -1e30f; int fo = bo;
        for (int off = lo; off <= hi; off++) {
            float d0 = 0.f, d1 = 0.f, e0 = 1e-12f, e1 = 0.f; const float* x = tmp + off;
            for (int j = 0; j < WC / 2; j += 2) { float a0 = x[2 * j], a1 = x[2 * j + 2]; d0 += ref[j] * a0; d1 += ref[j + 1] * a1; e0 += a0 * a0; e1 += a1 * a1; }
            float dot = d0 + d1, e = e0 + e1; float sc = dot > 0.f ? dot * dot / e : -dot * dot / e - 1.f; if (sc > best) { best = sc; fo = off; }
        }
        return (float)fo;
    }
    inline float tick(const ShifterCore& c, float r, float g, float omax) {
        const float step = 1.f - r, dmin = ShifterCore::DMIN; float y[2], w[2];
        G = g;
        for (int k = 0; k < 2; k++) {
            float bb = b[k] + step; bool wr = false;
            if (bb > dmin + G) { bb -= G; wr = true; } else if (bb < dmin) { bb += G; wr = true; }
            if (bb < dmin) bb = dmin; else if (bb > dmin + G) bb = dmin + G;
            b[k] = bb;
            if (wr) o[k] = align(c, bb, b[1 - k] + o[1 - k], omax);
            w[k] = ShVoice::win((bb - dmin) / G); y[k] = c.dl.read(bb + o[k]);
        }
        float s = w[0] + w[1]; if (s < 0.3f) s = 0.3f;
        return (w[0] * y[0] + w[1] * y[1]) / s;
    }
};

} // namespace hx

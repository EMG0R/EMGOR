// synth_algos.cpp — the 8 SynAlgo voices. Character/ratio setup on note-on,
// per-sample raw oscillator render (pre-filter) after. AUDIO plane: no
// malloc, no double, no libm calls inside the per-sample render path (noteOn
// runs once per note so a handful of cheap float ops there is fine, but no
// libm trig/pow is used even there — see engine_util.h's fast helpers).
//
// V2 reference (audit-line-numbered): Thief voiceCharacter L1219-1241, render
// L2286-2428; wavetables L638-665; Karplus L2348-2364; CZ L2365-2376;
// wavefolder L2331-2347; FM L2303-2311; supersaw detune table L616.
// Thief's plain-saw branch (V2 L1229, V.type==0, 55% of non-FM notes) is
// REMOVED here: Thief chooses only between a triangle stack and an FM pluck,
// 45/55, per the module spec.
#include "synth_private.h"

float SDRAM_BSS synWtbl[WT_N][WT_LEN];
float SDRAM_BSS synKpBuf[SYN_VOICES][KP_LEN];

// detune "shape" reused (scaled) by THIEF's tri stack and SUPER's saw stack.
static const float UNIT5[5] = { -2.f, -1.f, 0.f, 1.f, 2.f };
static const float UNIT3[3] = { -1.f, 0.f, 1.f };

// ---------------------------------------------------------------- wavetables
// Ported from V2 buildWavetables() (L642-665): additive, harmonic-capped so
// each table stays clean across the playable range, normalised to unity peak.
void synBuildWavetables() {
    const float TWO_PI = 6.2831853072f;
    for (int t = 0; t < WT_N; t++) {
        for (int i = 0; i < WT_LEN; i++) {
            float ph = (float)i / (float)WT_LEN, s = 0.f;
            switch (t) {
                case 0: s = sinf(TWO_PI * ph); break;                                  // pure sine
                case 1: for (int h = 1; h <= 15; h += 2) {                              // triangle
                            float sg = ((h >> 1) & 1) ? -1.f : 1.f;
                            s += sg * sinf(TWO_PI * h * ph) / (float)(h * h);
                        } break;
                case 2: for (int h = 1; h <= 8;  h++) s += sinf(TWO_PI * h * ph) / (float)h; break;  // soft saw
                case 3: for (int h = 1; h <= 24; h++) s += sinf(TWO_PI * h * ph) / (float)h; break;  // bright saw
                case 4: for (int h = 1; h <= 23; h += 2) s += sinf(TWO_PI * h * ph) / (float)h; break; // square
                case 5: { const float a[4] = {1.f, .5f, .32f, .22f}; const int hh[4] = {1, 2, 4, 8}; // organ
                          for (int j = 0; j < 4; j++) s += a[j] * sinf(TWO_PI * hh[j] * ph); } break;
                case 6: for (int h = 1; h <= 12; h++) {                                 // formant
                            float d = (float)(h - 4);
                            s += expf(-0.35f * d * d) * sinf(TWO_PI * h * ph);
                        } break;
                case 7: for (int h = 1; h <= 16; h++) s += sinf(TWO_PI * h * ph) * (0.55f + 0.45f / (float)h); break; // bright/harsh
            }
            synWtbl[t][i] = s;
        }
        float pk = 0.0001f;
        for (int i = 0; i < WT_LEN; i++) { float a = fabsf(synWtbl[t][i]); if (a > pk) pk = a; }
        float gn = 1.f / pk;
        for (int i = 0; i < WT_LEN; i++) synWtbl[t][i] *= gn;
    }
}

// ADD's spectral-tilt profile endpoints (bright/flat vs dark, precomputed so
// the per-sample render never calls pow/log). Blended per-note by TILT.
static const float ADD_BRIGHT[8] = { 1.0f, 0.85f, 0.70f, 0.55f, 0.42f, 0.32f, 0.24f, 0.18f };
static const float ADD_DARK[8]   = { 1.0f, 0.45f, 0.18f, 0.08f, 0.035f, 0.016f, 0.008f, 0.004f };

// ---------------------------------------------------------------- note-on: character/ratios
void synAlgoNoteOn(Voice& v, const SynPreset& pr, uint32_t& rng) {
    v.osc_type = 0;
    for (int k = 0; k < 5; k++) { v.phase[k] = 0.f; v.ratio[k] = 1.f; }
    for (int k = 0; k < 6; k++) v.aux[k] = 0.f;
    for (int k = 0; k < 3; k++) v.iaux[k] = 0;
    v.pitchGlideEnable = true;

    switch (v.algo) {
    case ALG_THIEF: {
        // === Mid-Air Thief — PMOR_V2 voiceCharacter() L1219-1241, VERBATIM. ===
        // Every note is its own patch: 40 % harmonic 2-op FM pluck, 60 % detuned tri stack
        // (the v2 plain/dry SAW stack is REMOVED per EMGOR); per-note VCF snap envelope with always-stable random
        // resonance; 15 % swells; some longer attacks; 10 % glide notes; krell drift.
        // Layout in the Voice scratch: iaux[0] = type (0 saw stack, 1 tri stack, 2 FM);
        // aux[0] fmi, aux[1] fmiT, aux[2] fc, aux[3] fcT, aux[4] fcR, aux[5] q;
        // apart[0] drift, apart[1] driftT, apart[2] atk coef, apart[3] env, apart[4] envTarget,
        // apart[5] relCoef. The preset ADSR / filter pages do NOT apply to Thief.
        static const float DETUNE[5] = {-0.012f, -0.006f, 0.f, +0.006f, +0.012f};
        bool generative = (v.pad == 0xFF);
        v.apart[0] = 0.f;
        v.apart[1] = ((float)(lcg(rng) % 200u) - 100.f) * 0.00008f;          // krell drift
        v.aux[5]   = 0.70f + (float)(lcg(rng) % 35u) * 0.01f;                // q: rings, never unstable
        uint32_t r = lcg(rng) % 100u;
        if (r < 40u) {
            v.iaux[0] = 2;
            static const float FMR[6] = {0.5f, 1.f, 1.003f, 2.f, 2.005f, 4.f};
            v.ratio[1] = FMR[lcg(rng) % 6u];
            v.aux[0] = 0.10f + (float)(lcg(rng) % 32u) * 0.01f;
            v.aux[1] = 0.02f;
            v.aux[2] = 0.40f; v.aux[3] = 0.15f; v.aux[4] = 0.00008f;
        } else {
            v.iaux[0] = 1;                                                     // tri stack only — no dry saw
            for (int k = 0; k < 5; k++) v.ratio[k] = 1.f + DETUNE[k];          // ONE pitch, pure detune
            v.aux[2] = 0.22f + (float)(lcg(rng) % 21u) * 0.01f;
            v.aux[3] = 0.025f + (float)(lcg(rng) % 8u) * 0.01f;
            v.aux[4] = 0.00004f + (float)(lcg(rng) % 9u) * 0.00001f;
        }
        uint32_t ar = generative ? (lcg(rng) % 100u) : 100u;
        bool swell = (ar < 15u);
        v.apart[2] = swell ? 0.0025f : ((ar < 40u) ? 0.012f : 0.08f);
        if (swell) { v.aux[2] = 0.04f; v.aux[3] = 0.36f; v.aux[4] = 0.00003f; }
        if ((lcg(rng) % 100u) < 10u)                                            // glide notes
            v.apart[1] = ((lcg(rng) & 1u) ? 1.f : -1.f) * (0.06f + (float)(lcg(rng) % 10u) * 0.012f);
        // filterTrack (V2 L1655): key-track the VCF around A3
        {
            float trk = v.note_hz / 220.f;
            if (trk < 0.5f) trk = 0.5f; if (trk > 2.6f) trk = 2.6f;
            v.aux[2] *= trk; v.aux[3] *= trk;
            if (v.aux[2] > 0.9f) v.aux[2] = 0.9f;
            if (v.aux[3] > 0.9f) v.aux[3] = 0.9f;
        }
        v.apart[3] = 0.f;                                                       // env
        v.apart[4] = generative ? (0.6f + (float)(lcg(rng) % 35u) * 0.01f) : 0.9f;
        v.apart[5] = generative ? 0.000048f * fastPow2(4.9069f * ((float)(lcg(rng) % 64u) / 63.f)) : 0.0004f;
        v.pitchGlideEnable = false;                                             // Thief has its own motion
        (void)pr;
        break;
    }
    case ALG_FM: {
        v.ratio[1] = clampf(pr.p[0], 0.1f, 16.f);
        v.aux[0] = clampf(pr.p[1], 0.f, 1.f) * 8.f;         // full-scale index
        v.aux[1] = v.aux[0];                                 // current index
        float d = pr.p[2];
        v.aux[2] = (d > 1e-4f) ? (1.f - fastExp(-3.f / (d * SR))) : 1.f;   // decay coef
        v.aux[3] = 0.f;                                      // feedback history
        break;
    }
    case ALG_WAVE: {
        float dt = clampf(pr.p[2], 0.f, 1.f) * 0.02f;
        for (int k = 0; k < 3; k++) v.ratio[k] = 1.f + UNIT3[k] * dt;
        break;
    }
    case ALG_PLUCK: {
        // handled lazily on first render sample (needs phaseInc); iaux[2] flags "not yet seeded".
        v.iaux[2] = 0;
        break;
    }
    case ALG_SUPER: {
        float spread = 0.004f + clampf(pr.p[1], 0.f, 1.f) * 0.016f;
        float amt    = 0.3f + clampf(pr.p[0], 0.f, 1.f) * 1.4f;
        for (int k = 0; k < 5; k++) v.ratio[k] = 1.f + UNIT5[k] * spread * amt;
        v.aux[0] = lcg01(rng);                                // sub-osc phase, randomised start
        break;
    }
    case ALG_CZ: {
        for (int k = 0; k < 3; k++) v.ratio[k] = 1.f + UNIT3[k] * 0.006f;
        break;
    }
    case ALG_FOLD: {
        for (int k = 0; k < 3; k++) v.ratio[k] = 1.f + UNIT3[k] * 0.004f;
        break;
    }
    case ALG_ADD: {
        float t01 = (clampf(pr.p[0], -1.f, 1.f) + 1.f) * 0.5f;
        float oe  = clampf(pr.p[1], 0.f, 1.f);
        float driftAmt = clampf(pr.p[3], 0.f, 1.f);
        for (int h = 0; h < 8; h++) {
            int hn = h + 1;
            bool isOdd = (hn & 1) != 0;
            float w = isOdd ? (0.3f + 0.7f * oe) : (0.3f + 0.7f * (1.f - oe));
            float base = lerpf(ADD_DARK[h], ADD_BRIGHT[h], t01);
            v.pgain[h]  = base * w;
            v.apart[h]  = 0.f;
            v.adrift[h] = (lcg01(rng) * 2.f - 1.f) * driftAmt * 0.01f;
        }
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------- per-sample render
float synAlgoRender(int vidx, Voice& v, const SynPreset& pr, float phaseInc, float envNorm, uint32_t& rng) {
    float raw = 0.f;
    switch (v.algo) {

    case ALG_THIEF: {
        // V2 render L2303-2311 / L2405-2409: naive saw/tri stacks (that grit IS the sound), FM pluck.
        float inc = phaseInc * (1.f + v.apart[0]);
        if (v.iaux[0] == 2) {
            v.aux[0] += (v.aux[1] - v.aux[0]) * 0.00015f;
            v.phase[1] += inc * v.ratio[1]; synWrap01(v.phase[1]);
            v.phase[0] += inc;              synWrap01(v.phase[0]);
            float p = v.phase[0] + synSin(v.phase[1]) * v.aux[0];
            synWrap01(p);
            raw = synSin(p) * 3.2f;
        } else {
            for (int k = 0; k < 5; k++) {
                v.phase[k] += inc * v.ratio[k]; synWrap01(v.phase[k]);
                float ph = v.phase[k];
                raw += (v.iaux[0] == 1) ? (4.f * fabsf(ph - 0.5f) - 1.f) : (ph + ph - 1.f);
            }
        }
        v.apart[0] += (v.apart[1] - v.apart[0]) * 0.00002f;                     // drift
        (void)envNorm;
        break;
    }
    case ALG_FM: {
        float fb = clampf(pr.p[3], 0.f, 1.f);
        v.aux[1] += ((v.aux[0] * 0.25f) - v.aux[1]) * v.aux[2];
        v.phase[1] += phaseInc * v.ratio[1]; synWrap01(v.phase[1]);
        float modOut = synSin(v.phase[1] + v.aux[3] * fb * 0.25f);
        v.aux[3] = modOut;
        v.phase[0] += phaseInc; synWrap01(v.phase[0]);
        float p = v.phase[0] + modOut * v.aux[1];
        synWrap01(p);
        raw = synSin(p) * 3.0f;
        break;
    }

    case ALG_WAVE: {
        float pos = clampf(pr.p[0], 0.f, (float)(WT_N - 1)) + clampf(pr.p[1], 0.f, 1.f);
        pos += envNorm * clampf(pr.p[3], 0.f, 1.f) * 2.0f;
        if (pos > (float)(WT_N - 1)) pos = (float)(WT_N - 1);
        if (pos < 0.f) pos = 0.f;
        int t0 = (int)pos, t1 = t0 + 1; if (t1 >= WT_N) t1 = WT_N - 1;
        float tf = pos - (float)t0;
        for (int k = 0; k < 3; k++) {
            v.phase[k] += phaseInc * v.ratio[k]; synWrap01(v.phase[k]);
            float idx = v.phase[k] * (float)WT_LEN;
            int i0 = (int)idx & (WT_LEN - 1);
            float f = idx - (float)(int)idx;
            int i1 = (i0 + 1) & (WT_LEN - 1);
            float a = synWtbl[t0][i0] + (synWtbl[t0][i1] - synWtbl[t0][i0]) * f;
            float b = synWtbl[t1][i0] + (synWtbl[t1][i1] - synWtbl[t1][i0]) * f;
            raw += a + (b - a) * tf;
        }
        raw *= 1.5f;
        break;
    }

    case ALG_PLUCK: {
        if (v.iaux[2] == 0) {
            // seed the string on the first render sample of the note (needs phaseInc).
            int L = (int)(1.f / (phaseInc > 1e-6f ? phaseInc : 1e-6f));
            if (L < 2) L = 2;
            if (L > KP_LEN - 2) L = KP_LEN - 2;
            v.iaux[0] = L; v.iaux[1] = 0;
            float bright = clampf(pr.p[1], 0.f, 1.f);
            float lpCoef = 0.15f + (1.f - bright) * 0.55f;
            float lp = 0.f;
            for (int j = 0; j < L; j++) {
                float noise = lcg01(rng) * 2.f - 1.f;
                lp += (noise - lp) * lpCoef;
                synKpBuf[vidx][j] = lerpf(lp, noise, bright);
            }
            int pp = (int)(clampf(pr.p[2], 0.f, 1.f) * (float)L * 0.5f);
            if (pp > 0 && pp < L) {
                for (int j = L - 1; j >= 0; j--)
                    synKpBuf[vidx][j] -= 0.5f * synKpBuf[vidx][(j - pp + L) % L];
            }
            v.aux[0] = 0.f;   // damping one-pole state
            v.iaux[2] = 1;
        }
        int L = v.iaux[0], p = v.iaux[1], pn = p + 1; if (pn >= L) pn = 0;
        float damp = 0.984f + clampf(pr.p[3], 0.f, 1.f) * 0.0148f;   // DECAY -> sustain length
        float outk = synKpBuf[vidx][p];
        float avg  = (synKpBuf[vidx][p] + synKpBuf[vidx][pn]) * 0.5f;
        float dampAmt = clampf(pr.p[0], 0.f, 1.f);
        v.aux[0] += (avg - v.aux[0]) * (0.2f + dampAmt * 0.6f);
        float filtered = lerpf(avg, v.aux[0], dampAmt * 0.6f);
        synKpBuf[vidx][p] = flushDenorm(filtered * damp);
        v.iaux[1] = pn;
        raw = outk * 2.6f;
        break;
    }

    case ALG_SUPER: {
        float centerAcc = 0.f, sideAcc = 0.f;
        for (int k = 0; k < 5; k++) {
            float dt = phaseInc * v.ratio[k];
            if (dt < 0.f) dt = -dt;
            if (dt > 0.45f) dt = 0.45f;
            v.phase[k] += phaseInc * v.ratio[k]; synWrap01(v.phase[k]);
            float ph = v.phase[k];
            float os = ph + ph - 1.f - synPolyBlep(ph, dt);
            if (k == 2) centerAcc += os; else sideAcc += os;
        }
        float wall = centerAcc + sideAcc;
        float center = centerAcc * 5.f;
        raw = lerpf(center, wall, clampf(pr.p[2], 0.f, 1.f)) * 0.85f;
        v.aux[0] += phaseInc * 0.5f; synWrap01(v.aux[0]);       // sub, one octave down
        raw += synSin(v.aux[0]) * clampf(pr.p[3], 0.f, 1.f) * 1.2f;
        break;
    }

    case ALG_CZ: {
        float reso = 1.f + (clampf(pr.p[0], 0.f, 1.f) + envNorm * clampf(pr.p[2], 0.f, 1.f)) * (1.f + clampf(pr.p[1], 0.f, 1.f) * 6.f);
        float shape = clampf(pr.p[3], 0.f, 1.f);
        for (int k = 0; k < 3; k++) {
            v.phase[k] += phaseInc * v.ratio[k]; synWrap01(v.phase[k]);
            float ph = v.phase[k];
            float win  = 1.f - ph;
            float win2 = win * win;
            float w = lerpf(win, win2, shape);
            float rp = ph * reso; rp -= (float)(int)rp;
            raw += w * synSin(rp) * 1.4f;
        }
        break;
    }

    case ALG_FOLD: {
        float drive  = 0.6f + clampf(pr.p[3], 0.f, 1.f) * 2.4f + (clampf(pr.p[0], 0.f, 1.f) + envNorm * clampf(pr.p[2], 0.f, 1.f)) * 3.0f;
        float offset = (clampf(pr.p[1], 0.f, 1.f) - 0.5f) * 1.2f;
        for (int k = 0; k < 3; k++) {
            v.phase[k] += phaseInc * v.ratio[k]; synWrap01(v.phase[k]);
            float s = (synSin(v.phase[k]) + offset) * drive;
            for (int it = 0; it < 6; it++) {
                if (s > 1.f) s = 2.f - s;
                else if (s < -1.f) s = -2.f - s;
                else break;
            }
            raw += s;
        }
        raw *= 1.1f;
        break;
    }

    case ALG_ADD: {
        float hiAmt = clampf(pr.p[2], 0.f, 1.f);
        for (int h = 0; h < 8; h++) {
            float hn = (float)(h + 1);
            v.apart[h] += phaseInc * hn * (1.f + v.adrift[h]); synWrap01(v.apart[h]);
            float atten = 1.f - hiAmt * (1.f - envNorm) * (hn * 0.125f);
            if (atten < 0.f) atten = 0.f;
            raw += synSin(v.apart[h]) * v.pgain[h] * atten;
        }
        raw *= 0.9f;
        break;
    }

    default: break;
    }
    return raw;
}

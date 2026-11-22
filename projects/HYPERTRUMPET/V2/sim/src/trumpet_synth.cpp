// trumpet_synth.cpp -- see trumpet_synth.h
#include "trumpet_synth.h"
namespace sim {

static inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

void TrumpetSynth::init(float sampleRate) {
    sr_ = sampleRate; phase_ = vibPh_ = 0.f; amp_ = gate_ = duck_ = 0.f;
    for (int h = 0; h < NH; h++) ampH_[h] = 0.f;
    lp1_ = nbp1_ = nbp2_ = shelf_ = 0.f; rng_ = 22222u; freq_ = target_ = 233.f;
}

void TrumpetSynth::tongue() { duck_ = 0.024f; }

// effective "pressed-ness" of a valve from Hall depth: half-valve zone below 0.5 bends partially, >=0.5 is pressed.
float TrumpetSynth::eff(float d) const {
    d = clamp01(d);
    if (d < 0.5f) { float t = d / 0.5f; return 0.45f * t * t; }
    float t = clamp01((d - 0.5f) / 0.1f); t = t * t * (3.f - 2.f * t);
    return 0.45f + 0.55f * t;
}

float TrumpetSynth::semis() const {
    float e1 = eff(valve[0]), e2 = eff(valve[1]), e3 = eff(valve[2]);
    float s = e1 * 2.f + e2 * 1.f + e3 * 3.f;
    if (quirks) {   // real long-tube combos run sharp; players lip them down. a mild version of that
        float full = (e1 > 0.9f ? 1.f : 0.f) + (e3 > 0.9f ? 1.f : 0.f);
        if (full > 1.5f) s -= 0.18f;                  // 1+3 : tube a touch short -> sharp
        if (e1 > 0.9f && e2 > 0.9f && e3 > 0.9f) s -= 0.25f;
    }
    return s;
}

void TrumpetSynth::process(float* out, int n) {
    const float base = 116.54f;                         // Bb2
    int p = partial < 2 ? 2 : (partial > 9 ? 9 : partial);
    float pf = (float)p;
    if (p == 7) pf = 6.80f;                             // flat natural 7th
    target_ = base * pf * exp2f(-semis() / 12.f);

    // half-valve airiness: peak where valves sit between the two states
    float air = 0.f;
    for (int i = 0; i < 3; i++) { float e = eff(valve[i]); float a = 4.f * e * (1.f - e); if (valve[i] > 0.12f && a > air) air = a; }

    // glide 5..30 ms: longer for bigger jumps
    float jump = fabsf(log2f(target_ / freq_)) * 12.f;
    float glideMs = 6.f + 2.f * (jump > 6.f ? 6.f : jump);
    float gc = 1.f - expf(-1.f / (0.001f * glideMs * sr_));

    float brTarget = breath;
    const float aAtk = 1.f - expf(-1.f / (0.008f * sr_)), aRel = 1.f - expf(-1.f / (0.06f * sr_));
    const float aGate = 1.f - expf(-1.f / (0.004f * sr_));
    const float nyq = 0.45f * sr_;

    // per-block target harmonic amplitudes (brass: brighter when louder, formant bump ~1.3 kHz)
    float tgt[NH];
    float loud = amp_ > brTarget ? amp_ : brTarget; loud = clamp01(loud);
    float tilt = 3.2f - 2.3f * powf(loud, 0.6f);
    float f1 = freq_;
    for (int h = 0; h < NH; h++) {
        float fh = f1 * (float)(h + 1);
        float a = powf((float)(h + 1), -tilt);
        float x = (fh - 1300.f) / 700.f; a *= 1.f + 1.6f * expf(-x * x) * (0.4f + loud);
        if (fh > nyq) a = 0.f; else if (fh > nyq * 0.8f) a *= (nyq - fh) / (nyq * 0.2f);
        tgt[h] = a;
    }
    float norm = 0.f; for (int h = 0; h < NH; h++) norm += tgt[h] * tgt[h];
    norm = 1.f / sqrtf(norm + 1e-6f) * 0.55f;
    float inc[NH];
    for (int h = 0; h < NH; h++) { tgt[h] *= norm; inc[h] = (tgt[h] - ampH_[h]) / (float)n; }

    float duckStep = 1.f / sr_;
    for (int i = 0; i < n; i++) {
        float g = brTarget > amp_ ? aAtk : aRel;
        amp_ += (brTarget - amp_) * g;
        float gt = (duck_ > 0.f) ? 0.f : 1.f; duck_ -= duckStep; if (duck_ < 0.f) duck_ = 0.f;
        gate_ += (gt - gate_) * (gt > gate_ ? aAtk : aGate);   // tongue closes fast, opens ~8 ms
        freq_ += (target_ - freq_) * gc;
        float vib = 1.f + (vibDepth * 0.5f) * sinf(6.2831853f * vibPh_) * 0.0594f;   // +-0.5 st at depth 1 (0.0297 ratio)
        vibPh_ += vibRate / sr_; if (vibPh_ >= 1.f) vibPh_ -= 1.f;
        // half-valve bend is flat-ish: shave pitch a few cents by airiness
        float fi = freq_ * vib * (1.f - 0.012f * air);
        phase_ += fi / sr_; if (phase_ >= 1.f) phase_ -= 1.f;
        float th = 6.2831853f * phase_;
        float s1 = sinf(th), c1 = cosf(th), c2 = 2.f * c1;
        float sPrev = 0.f, sCur = s1, y = 0.f;
        for (int h = 0; h < NH; h++) {
            ampH_[h] += inc[h];
            y += ampH_[h] * sCur;
            float sNext = c2 * sCur - sPrev; sPrev = sCur; sCur = sNext;
        }
        // breath noise (bandpassed white), louder with air and breath
        rng_ = rng_ * 1664525u + 1013904223u;
        float w = (float)(int32_t)rng_ * (1.f / 2147483648.f);
        lp1_ += (w - lp1_) * 0.35f;
        float nz = lp1_ - nbp1_; nbp1_ += (lp1_ - nbp1_) * 0.04f;
        float noiseAmt = 0.025f * amp_ + 0.18f * air * amp_ + 0.01f;
        float level = amp_ * gate_;
        float v = (y * (0.25f + 0.75f * amp_) + nz * noiseAmt * 2.f) * level * (level > 0.001f ? 1.f : 0.f);
        // bell-ish low shelf (+3 dB below ~250 Hz)
        shelf_ += (v - shelf_) * 0.032f;
        v = v + 0.4f * shelf_;
        out[i] = v * 0.9f;
    }
    (void)nbp2_;
}

} // namespace sim

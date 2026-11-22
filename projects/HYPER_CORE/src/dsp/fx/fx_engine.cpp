// fx_engine.cpp -- see fx_engine.h
#include "fx_engine.h"
namespace hx {
uint64_t (*g_nowNs)() = nullptr;

void FxEngine::init(float sampleRate, void* sdram, size_t bytes) {
    sr_ = sampleRate;
    size_t per = (bytes / HX_NSLOTS) & ~(size_t)15;
    for (int i = 0; i < HX_NSLOTS; i++) { slots_[i].arena.bind((uint8_t*)sdram + i * per, per); slots_[i].fx = nullptr; slots_[i].type = -1; slots_[i].ready = false; }
    pt_.init(sr_);
    ctx_ = FxCtx{}; ctx_.sr = sr_;
}

bool FxEngine::setEffect(int slot, int typeId) {
    if (slot < 0 || slot >= HX_NSLOTS) return false;
    Slot& s = slots_[slot];
    s.ready = false;
    if (s.fx) { s.fx->~Fx(); s.fx = nullptr; }
    s.arena.reset(); s.type = typeId; s.ns = 0.f;
    if (typeId < 0) return true;
    // arena memory may be dirty (SDRAM is not zeroed at reset): arena.floats() zeroes what it hands out.
    s.fx = fxCreate(typeId, s.arena, sr_);
    if (!s.fx) { s.type = -1; return false; }
    s.fx->init(sr_);
    s.fx->snap();
    s.g = s.bypassTarget ? 0.f : 1.f;
    s.ready = true;
    return s.fx->ok;
}

void FxEngine::process(const float* in, float* outL, float* outR, int n) {
    if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
    uint64_t t0 = g_nowNs ? g_nowNs() : 0;
    float pk = 0.f, e = 0.f;
    for (int i = 0; i < n; i++) { float a = in[i] < 0 ? -in[i] : in[i]; if (a > pk) pk = a; e += in[i] * in[i]; outL[i] = in[i]; }
    if (pk > inPeak_) inPeak_ = pk;
    float rms = sqrtf(e / (float)n);
    float c = rms > envY_ ? 0.5f : 0.08f;                    // fast attack, slower release (block rate)
    envY_ += (rms - envY_) * c;
    pt_.process(in, n);
    ctx_.f0 = pt_.f0; ctx_.conf = pt_.conf; ctx_.env = clampf(envY_ * 4.f, 0.f, 1.f); ctx_.valvePitchHz = valveHz_; ctx_.blockIndex++;
    bool stereo = false;
    float* L = outL; float* R = outR;
    float stepG = (float)n / (0.015f * sr_);                 // 15 ms bypass crossfade
    for (int k = 0; k < HX_NSLOTS; k++) {
        Slot& s = slots_[k];
        if (!s.ready || !s.fx) continue;
        float tg = s.bypassTarget ? 0.f : 1.f;
        if (s.g == tg && tg == 0.f) continue;                // fully bypassed: no CPU
        bool trans = (s.g != tg);
        if (trans) { memcpy(dryL_, L, n * sizeof(float)); if (stereo) memcpy(dryR_, R, n * sizeof(float)); }
        bool wasStereo = stereo;
        if (!stereo) memcpy(R, L, n * sizeof(float));        // fx may read R only if stereoIn
        uint64_t a = g_nowNs ? g_nowNs() : 0;
        s.fx->smoothBlock();
        bool st = s.fx->process(ctx_, L, R, n, stereo);
        if (g_nowNs) { float d = (float)(g_nowNs() - a); s.ns += (d - s.ns) * 0.05f; }
        if (trans) {
            float g0 = s.g; float g1 = g0 + (tg > g0 ? stepG : -stepG); if ((tg > g0 && g1 > tg) || (tg < g0 && g1 < tg)) g1 = tg;
            float inc = (g1 - g0) / (float)n, g = g0;
            const float* dr = dryL_; const float* dr2 = wasStereo ? dryR_ : dryL_;
            for (int i = 0; i < n; i++) { float wl = L[i], wr = st ? R[i] : wl; L[i] = dr[i] + (wl - dr[i]) * g; R[i] = dr2[i] + (wr - dr2[i]) * g; g += inc; }
            s.g = g1; stereo = true;
        } else {
            stereo = st || wasStereo;
        }
    }
    if (!stereo) memcpy(R, L, n * sizeof(float));
    float op = 0.f;
    for (int i = 0; i < n; i++) { L[i] = finite(L[i]); R[i] = finite(R[i]); float a = fabsf(L[i]), b = fabsf(R[i]); if (a > op) op = a; if (b > op) op = b; }
    if (op > outPeak_) outPeak_ = op;
    if (g_nowNs) { float d = (float)(g_nowNs() - t0); totalNs_ += (d - totalNs_) * 0.05f; }
}

} // namespace hx

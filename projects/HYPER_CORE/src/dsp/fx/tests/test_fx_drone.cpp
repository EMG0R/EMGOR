#include "../fx_drone.h"
#include "fx_testutil.h"
#include <algorithm>
#include <functional>
using namespace hx;
static double tone(const std::vector<float>& x, double from, double to, double hz) {
    long a = (long)(from * hxt::SR), b = (long)(to * hxt::SR); double sr = 0, si = 0;
    for (long i = a; i < b; i++) { double w = 0.5 - 0.5 * cos(6.283185307179586 * (i - a) / (double)(b - a)); double ph = 6.283185307179586 * hz * i / hxt::SR; sr += w * x[i] * cos(ph); si += w * x[i] * sin(ph); }
    return 2.0 * sqrt(sr * sr + si * si) / (0.5 * (b - a));
}
static double band(const std::vector<float>& x, double from, double to, double lo, double hi) { double m = 0; for (double f = lo; f <= hi; f += 1.0) m = fmax(m, tone(x, from, to, f)); return m; }
static std::vector<float> runCfg(const char* const* nm, const float* val, int cnt, double secs, std::function<void(float*, int, long)> gen) {
    hxt::Host h; h.make(hx::fx_create_drone); Fx* fx = h.fx; fx->setParam(0, 1.f);
    for (int i = 0; i < cnt; i++) { int p = fx->findParam(nm[i]); HX_CHECK(p >= 0, "missing param %s", nm[i]); fx->setParam(p, val[i]); }
    fx->snap(); fx->reset(); std::vector<float> oL; hxt::run(fx, secs, 48, false, gen, hxt::defCtx(), &oL);
    std::vector<float> dry(oL.size()); for (size_t i = 0; i < dry.size(); i += 48) gen(&dry[i], (int)std::min<size_t>(48, dry.size() - i), (long)i);
    for (size_t i = 0; i < oL.size(); i++) oL[i] -= dry[i];   // isolate the wet send (MIX is additive)
    return oL;
}
int main() {
    hxt::Opts o; hxt::stdSuite("drone", hx::fx_create_drone, o);
    { hxt::Host h; h.make(hx::fx_create_drone); Fx* f = h.fx;   // v1 raw defaults (his order), published names
      static const char* const nm[] = {"MIX","INTERP","INTVAR","PEAKAMP","GATE","COMBLVL","COMBFUND","SCALE","OCTAVES","RESON","FLAMT","FLRATE"};
      static const float dv[] = {0.5f, 2.06f, 3.38f, 0, -72.f, 40.f, 303.04f, 7, 6, 70, 0, 0.1f};   // v3 defaults (2026-11-17 rebuild)
      for (int i = 0; i < 12; i++) { int p = f->findParam(nm[i]); HX_CHECK(p == i, "param %s index %d", nm[i], p); if (p >= 0) HX_CHECK(fabsf(f->getParam(p) - dv[i]) < 1e-4f, "default %s = %f", nm[i], f->getParam(p)); } }
    // 440 + 660 for 1.5 s then silence, short INTERP: drone present during/after input, gone once snapshots of silence arrive
    {
        static const char* const nm[] = {"INTERP","INTVAR","GATE","COMBLVL"}; static const float vv[] = {0.4f, 0.f, -60.f, 0.f};
        auto gen = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)hxt::SR; b[i] = t < 1.5 ? 0.3f * sinf(6.2831853f * 468.75f * (float)t) + 0.2f * sinf(6.2831853f * 656.25f * (float)t) : 0.f; } };
        auto oL = runCfg(nm, vv, 4, 6.0, gen);
        double during = band(oL, 1.0, 1.4, 464, 474), a660 = band(oL, 1.0, 1.4, 651, 661), off = band(oL, 1.0, 1.4, 540, 600), late = band(oL, 4.5, 5.5, 464, 474);
        printf("  drone: 440 during %.3f, 660 %.3f, off-band %.4f, 3 s after stop %.5f\n", during, a660, off, late);
        HX_CHECK(during > 0.05, "no drone at 440 (%.4f)", during); HX_CHECK(a660 > 0.02, "no 660 partial (%.4f)", a660);
        HX_CHECK(during > 5 * off, "energy not at the partial"); HX_CHECK(late > 0.2 * during, "pad does not sustain through silence (it holds its last snapshot)");
    }
    // PEAKAMP: input stops right after 440 burst; with peak amps the held peak keeps feeding the next snapshot, without it silence is sampled
    {
        static const char* const n0[] = {"INTERP","INTVAR","GATE","COMBLVL","PEAKAMP"}; float v0[] = {1.0f, 0.f, -80.f, 0.f, 0.f}, v1[] = {1.0f, 0.f, -80.f, 0.f, 1.f};
        auto gen = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)hxt::SR; b[i] = (t > 0.1 && t < 0.2) ? 0.3f * sinf(6.2831853f * 468.75f * (float)t) : 0.f; } };
        auto a = runCfg(n0, v0, 5, 3.5, gen), b = runCfg(n0, v1, 5, 3.5, gen);
        double ea = band(a, 1.0, 2.0, 464, 474), eb = band(b, 1.0, 2.0, 464, 474); printf("  drone: PEAKAMP off %.4f on %.4f\n", ea, eb);
        HX_CHECK(eb > 0.5 * ea && ea > 0.005, "PEAKAMP run broke the sustained pad (silence is no longer sampled: both settings hold)");
    }
    // GATE: a -50 dBFS partial passes at -80 dB, is removed at -30 dB
    {
        static const char* const n0[] = {"INTERP","INTVAR","GATE","COMBLVL"}; float v0[] = {0.4f, 0.f, -80.f, 0.f}, v1[] = {0.4f, 0.f, -30.f, 0.f};
        auto g = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) b[i] = 0.003f * sinf(6.2831853f * 468.75f * (float)((i0 + i) / (double)hxt::SR)); };
        auto a = runCfg(n0, v0, 4, 3.0, g), b = runCfg(n0, v1, 4, 3.0, g); double ea = band(a, 2.0, 2.8, 464, 474), eb = band(b, 2.0, 2.8, 464, 474);
        printf("  drone: gate -80 %.5f, gate -30 %.5f\n", ea, eb); HX_CHECK(ea > 5e-4 && eb < 0.05 * ea, "gate not working");
    }
    // COMB: noise through overtone comb, fundamental 220 Hz, RESON 100: 440 (harmonic) >> 330 (between harmonics)
    {
        static const char* const n0[] = {"INTERP","INTVAR","GATE","COMBLVL","COMBFUND","SCALE","OCTAVES","RESON"}; float v0[] = {0.3f, 0.f, -90.f, 100.f, 234.375f, 7.f, 5.f, 100.f};
        auto gen = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) { uint32_t q = (uint32_t)(i0 + i) * 2654435761u; q ^= q >> 15; q *= 2246822519u; q ^= q >> 13; b[i] = 0.2f * ((float)(q >> 8) * (1.f / 8388608.f) - 1.f); } };
        auto a = runCfg(n0, v0, 8, 4.0, gen);
        double h = 0, bt = 0; for (double f : {468.75, 703.125, 937.5, 1171.875}) h += band(a, 2.0, 3.5, f - 4, f + 4); for (double f : {585.9375, 820.3125, 1054.6875, 1289.0625}) bt += band(a, 2.0, 3.5, f - 4, f + 4);
        printf("  drone: comb harmonic %.4f vs between %.4f\n", h, bt); HX_CHECK(h > 3 * bt, "comb bank not harmonising");
    }
    // pitch accuracy: a partial between bin centres is resynthesised at its true frequency (v3: phase advance of the spectral peak).
    {
        static const char* const nm[] = {"INTERP","INTVAR","GATE","COMBLVL"}; static const float vv[] = {0.4f, 0.f, -60.f, 0.f};
        hxt::SineGen g{440.f, 0.3f}; auto oL = runCfg(nm, vv, 4, 3.0, g);
        double at = band(oL, 2.0, 2.8, 438, 442), nb = band(oL, 2.0, 2.8, 400, 430) + band(oL, 2.0, 2.8, 450, 480);
        printf("  drone: pitch accuracy 440 Hz: at 440 %.4f, neighbours %.4f\n", at, nb); HX_CHECK(at > nb, "partial off-pitch (bin-centre resynthesis)");
    }
    hxt::benchNsPerSample(hx::fx_create_drone, "drone", false); hxt::benchNsPerSample(hx::fx_create_drone, "drone_st", true);
    printf("[drone-extra] %s\n", hxt::failCount() ? "FAIL" : "PASS"); return hxt::failCount() ? 1 : 0;
}

#include "../fx_mood.h"
#include "fx_testutil.h"
using namespace hx; using namespace hxt;
struct Horn2 { float hz; void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) { float t = (float)(i0 + i) / SR, y = 0; for (int h = 1; h <= 6; h++) y += sinf(6.2831853f * hz * h * t) / h; b[i] = 0.3f * y; } } };

int main() {
    stdSuite("mood", fx_create_mood, Opts{});
    // wet probability statistics: RATE 30 ms, 3000 steps
    for (float p : {0.f, 0.25f, 0.5f, 0.8f, 1.f}) {
        Host h; h.make(fx_create_mood); FxMood* e = (FxMood*)h.fx; e->setParam(2, p); e->setParam(3, 30.f); e->snap();
        run(e, 90.0, 48, false, Horn2{330.f}, defCtx(330.f));
        float frac = (float)e->onSteps / (float)e->steps;
        printf("  MOOD %.2f: %u/%u steps wet = %.3f\n", p, e->onSteps, e->steps, frac);
        HX_CHECK(e->steps > 2900 && e->steps < 3100, "step count %u", e->steps);
        HX_CHECK(fabsf(frac - p) < 0.03f, "prob %.3f vs %.2f", frac, p);
    }
    // RATE accuracy: 125 ms -> 8 steps/s
    { Host h; h.make(fx_create_mood); FxMood* e = (FxMood*)h.fx; run(e, 10.0, 48, false, Horn2{330.f}, defCtx(330.f)); HX_CHECK(e->steps >= 79 && e->steps <= 81, "125ms steps %u", e->steps); }
    // AUTO=1: all 9 ladder indices appear, roughly uniform; AUTO=0: only PITCH idx
    { Host h; h.make(fx_create_mood); FxMood* e = (FxMood*)h.fx; e->setParam(2, 1.f); e->setParam(3, 30.f); e->setParam(4, 1.f); e->snap();
      run(e, 60.0, 48, false, Horn2{330.f}, defCtx(330.f)); uint32_t tot = 0; for (int i = 0; i < 9; i++) tot += e->idxCount[i];
      for (int i = 0; i < 9; i++) { printf("  auto idx %d: %u\n", i, e->idxCount[i]); HX_CHECK(e->idxCount[i] > tot / 9 * 0.7 && e->idxCount[i] < tot / 9 * 1.3, "idx %d count %u/%u", i, e->idxCount[i], tot); } }
    { Host h; h.make(fx_create_mood); FxMood* e = (FxMood*)h.fx; e->setParam(2, 1.f); e->setParam(3, 30.f); e->setParam(1, 3.f); e->snap();
      run(e, 10.0, 48, false, Horn2{330.f}, defCtx(330.f)); HX_CHECK(e->idxCount[3] == e->onSteps && e->onSteps > 0, "AUTO=0 idx"); }
    // determinism + seed sensitivity
    { std::vector<float> a, b, c2; for (int k = 0; k < 3; k++) { Host h; h.make(fx_create_mood); h.fx->setParam(6, k == 2 ? 7.f : 1.f); h.fx->setParam(3, 40.f); h.fx->setParam(4, 0.5f); h.fx->snap(); run(h.fx, 3.0, 48, false, Horn2{330.f}, defCtx(330.f), k == 0 ? &a : (k == 1 ? &b : &c2)); }
      HX_CHECK(a == b, "same seed must reproduce"); HX_CHECK(a != c2, "different seed must differ"); }
    // MIX=0 dry exact; gate-off output == dry exactly (MOOD=0, MIX=1)
    { Host h; h.make(fx_create_mood); h.fx->setParam(2, 0.f); h.fx->snap(); std::vector<float> o, in(48000); Horn2{330.f}(in.data(), 48000, 0); run(h.fx, 1.0, 48, false, Horn2{330.f}, defCtx(330.f), &o);
      float err = 0; for (size_t i = 0; i < o.size(); i++) err = fmaxf(err, fabsf(o[i] - in[i])); HX_CHECK(err < 1e-6f, "MOOD=0 not dry %.2e", err); }
    // wet step: with gate forced on, +12 -> dominant 660 Hz at the end of wet (MOOD=1, MIX=1, PITCH 6): output contains energy at 660 and no discontinuity spike on ramp
    { Host h; h.make(fx_create_mood); h.fx->setParam(2, 1.f); h.fx->setParam(0, 1.f); h.fx->snap(); std::vector<float> o; run(h.fx, 2.0, 48, false, Horn2{330.f}, defCtx(330.f), &o);
      
      // correlate with 660 Hz fundamental vs 330
      double c660 = 0, s660 = 0, c330 = 0, s330 = 0; for (size_t i = 48000; i < o.size(); i++) { float t = i / SR; c660 += o[i] * cosf(6.2831853f * 660 * t); s660 += o[i] * sinf(6.2831853f * 660 * t); c330 += o[i] * cosf(6.2831853f * 330 * t); s330 += o[i] * sinf(6.2831853f * 330 * t); }
      double m660 = sqrt(c660 * c660 + s660 * s660), m330 = sqrt(c330 * c330 + s330 * s330); printf("  wet +12: |660|=%.1f |330|=%.1f\n", m660, m330);
      HX_CHECK(m660 > 4 * m330, "wet voice not +12"); float mx = 0; for (size_t i = 48001; i < o.size(); i++) mx = fmaxf(mx, fabsf(o[i] - o[i - 1])); HX_CHECK(mx < 0.5f, "step spike %.3f", mx); }
    benchNsPerSample(fx_create_mood, "mood");
    printf("[mood] %s (%d failures)\n", failCount() ? "FAIL" : "PASS", failCount());
    return failCount() ? 1 : 0;
}

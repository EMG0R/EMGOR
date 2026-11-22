#include "../fx_chorus.h"
#include "fx_testutil.h"
int main() {
    hxt::stdSuite("chorus", hx::fx_create_chorus, hxt::Opts{});
    using namespace hxt;
    { Host h; h.make(hx::fx_create_chorus); Fx* e = h.fx; e->setParam(0, 1.f); e->setParam(4, 0.5f); e->setParam(2, 1.f); e->snap();
      std::vector<float> oL, oR; Stats s = run(e, 3.0, 48, false, SineGen{440.f, 0.5f}, defCtx(), &oL, &oR);
      double eW = 0, dd = 0; for (size_t i = 48000; i < oL.size(); i++) { eW += (double)oL[i] * oL[i]; dd += (double)(oL[i] - oR[i]) * (oL[i] - oR[i]); }
      printf("  wet energy %.1f, L-R difference energy %.1f (stereo spread), peak %.3f\n", eW, dd, s.peak);
      HX_CHECK(!s.bad && eW > 1.0 && s.peak < 2.f, "wet energy / bounded"); HX_CHECK(dd > 0.01 * eW, "stereo spread"); }
    // static delay check: with DEPTH 0 and a 1 kHz click train the middle tap arrives at DELAY ms
    { Host h; h.make(hx::fx_create_chorus); Fx* e = h.fx; e->setParam(0, 1.f); e->setParam(2, 0.f); e->setParam(5, 1.f); e->setParam(7, 20.f); e->setParam(8, 20000.f); e->snap();
      struct Imp { void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) b[i] = (i0 + i == 0) ? 1.f : 0.f; } };
      std::vector<float> o; run(e, 0.1, 48, false, Imp(), defCtx(), &o);
      long exp0 = (long)(0.015f * SR); float pk = 0; long pi = 0; for (long i = exp0 - 20; i < exp0 + 20; i++) { float a = fabsf(o[i]); if (a > pk) { pk = a; pi = i; } }
      printf("  centre tap near %ld (exp %ld), amp %.3f\n", pi, exp0, pk); HX_CHECK(pk > 0.1f, "centre tap present"); }
    benchNsPerSample(hx::fx_create_chorus, "chorus", false);
    return failCount() ? 1 : 0;
}

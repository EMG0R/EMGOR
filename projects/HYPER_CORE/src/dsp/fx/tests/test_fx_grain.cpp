#include "../fx_grain.h"
#include "fx_testutil.h"
int main() {
    hxt::stdSuite("grain", hx::fx_create_grain, hxt::Opts{});
    using namespace hxt;
    { Host h; h.make(hx::fx_create_grain); hx::FxGrain* g = (hx::FxGrain*)h.fx;
      g->setParam(0, 1.f); g->setParam(3, 2000.f); g->setParam(2, 500.f); g->setParam(1, 300.f); g->setParam(9, 0.5f); g->snap();
      int maxV = 0; Stats s; std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f);
      for (int b = 0; b < 4000; b++) {
          for (int i = 0; i < 48; i++) L[i] = 0.5f * sinf(6.2831853f * 330.f * (float)(b * 48 + i) / SR); R = L;
          g->smoothBlock(); g->process(c, L.data(), R.data(), 48, false); scan(s, L.data(), 48); scan(s, R.data(), 48);
          if (g->activeVoices() > maxV) maxV = g->activeVoices(); }
      printf("  max density: max voices %d (cap %d), peak %.3f, energy %.1f\n", maxV, HX_GRAIN_VOICES, s.peak, s.energy);
      HX_CHECK(!s.bad && s.energy > 1.0, "output energy/NaN"); HX_CHECK(maxV <= HX_GRAIN_VOICES && g->maxVoicesSeen() <= HX_GRAIN_VOICES, "voice cap");
      HX_CHECK(s.peak < 4.f, "peak %.2f", s.peak); }
    { Host h; h.make(hx::fx_create_grain); Fx* g = h.fx; g->setParam(11, 0.f); g->setParam(5, 12.f); g->setParam(8, 0.f); g->snap();   // TRANSP +12, forward
      Stats s = run(g, 3.0, 48, true, SineGen{200.f, 0.4f}, defCtx()); HX_CHECK(!s.bad && s.energy > 1.0, "transposed grains"); }
    benchNsPerSample(hx::fx_create_grain, "grain", false);
    return failCount() ? 1 : 0;
}

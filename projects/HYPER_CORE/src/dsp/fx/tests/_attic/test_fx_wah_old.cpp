#include "../fx_wah.h"
#include "fx_testutil.h"
using namespace hxt;
static double gz(const std::vector<float>& x, size_t from, size_t n, double hz) {
    double w = 2 * M_PI * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { double s = x[from + i] + c * s1 - s2; s2 = s1; s1 = s; }
    return sqrt((s1 * s1 + s2 * s2 - c * s1 * s2)) * 2.0 / n;
}
static double peakHz(Fx* e, bool useEnv = false, float env = 0.5f) {
    std::vector<float> out; NoiseGen ng; ng.amp = 0.5f; FxCtx c = defCtx(); c.env = env;
    run(e, 3.0, 48, false, ng, c, &out);
    double best = 0, bf = 0; for (double f = 200; f < 6000; f *= 1.01) { double a = gz(out, 48000, 96000, f); if (a > best) { best = a; bf = f; } } return bf;
}
int main() {
    hxt::stdSuite("wah", hx::fx_create_wah, hxt::Opts{});
    // BP peak tracks FREQ within 10%
    for (float fr : {350.f, 500.f, 800.f, 1200.f, 1800.f, 2500.f}) {
        Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(1, fr); e->setParam(2, 5.f); e->snap();
        double pk = peakHz(e); printf("  FREQ %.0f -> peak %.0f Hz (%.1f%%)\n", fr, pk, 100 * (pk / fr - 1));
        HX_CHECK(fabs(pk / fr - 1) < 0.10, "BP peak %.0f vs FREQ %.0f", pk, fr);
    }
    // ENV opens the filter: higher ctx.env -> higher peak
    { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(1, 500.f); e->setParam(2, 5.f); e->setParam(4, 1.f); e->setParam(5, 1.f); e->snap();
      for (int w = 0; w < 400; w++) { e->smoothBlock(); } double lo = peakHz(e, true, 0.0f);
      Host h2; h2.make(hx::fx_create_wah); Fx* e2 = h2.fx; e2->setParam(1, 500.f); e2->setParam(2, 5.f); e2->setParam(4, 1.f); e2->setParam(5, 1.f); e2->snap(); double hi = peakHz(e2, true, 0.8f);
      printf("  ENV 0 -> %.0f Hz, ENV 0.8 -> %.0f Hz\n", lo, hi); HX_CHECK(hi > lo * 1.5, "ENV does not open filter"); }
    // LP mode: lowpass (HF attenuated); NOTCH mode: dip at FREQ
    { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(1, 800.f); e->setParam(3, 1.f); e->snap(); std::vector<float> o; NoiseGen ng; ng.amp = .5f; run(e, 2.0, 48, false, ng, defCtx(), &o);
      HX_CHECK(gz(o, 48000, 48000, 5000) < 0.2 * gz(o, 48000, 48000, 300), "LP not lowpass"); }
    { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(1, 800.f); e->setParam(3, 2.f); e->setParam(2, 3.f); e->snap(); std::vector<float> o; NoiseGen ng; ng.amp = .5f; run(e, 2.0, 48, false, ng, defCtx(), &o);
      HX_CHECK(gz(o, 48000, 48000, 800) < 0.3 * gz(o, 48000, 48000, 400), "notch missing"); }
    // loudness comp: BP/LP vs raw noise rms
    for (int m = 0; m < 3; m++) { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(3, (float)m); e->snap(); NoiseGen ng; ng.amp = .5f; Stats s = run(e, 1.0, 48, false, ng, defCtx());
      NoiseGen ng2; ng2.amp = .5f; Host h0; h0.make(hx::fx_create_wah); h0.fx->setParam(0, 0.f); h0.fx->snap(); Stats s0 = run(h0.fx, 1.0, 48, false, ng2, defCtx());
      printf("  mode %d level vs dry: %.1f dB\n", m, 10 * log10(s.energy / s0.energy)); }
    hxt::benchNsPerSample(hx::fx_create_wah, "wah");
    hxt::benchNsPerSample(hx::fx_create_wah, "wah(stereo)", true);
    return failCount() ? 1 : 0;
}

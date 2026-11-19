#include "../fx_ringmod.h"
#include "fx_testutil.h"
using namespace hxt;
static double gz(const std::vector<float>& x, size_t from, size_t n, double hz) {
    double w = 2 * M_PI * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { double s = x[from + i] + c * s1 - s2; s2 = s1; s1 = s; }
    return sqrt((s1 * s1 + s2 * s2 - c * s1 * s2)) * 2.0 / n;
}
int main() {
    Opts o; hxt::stdSuite("ring", hx::fx_create_ringmod, o);
    // sum/difference: 1000 Hz sine x 300 Hz carrier -> 700 + 1300, input and carrier suppressed
    for (int shape = 0; shape < 3; shape++) {
        Host h; h.make(hx::fx_create_ringmod); Fx* e = h.fx; e->setParam(1, 300.f); e->setParam(2, 1.f); e->setParam(4, (float)shape); e->snap();
        std::vector<float> out; run(e, 1.0, 48, false, SineGen{1000.f, 0.5f}, defCtx(), &out);
        size_t f = 24000, n = 24000; double a7 = gz(out, f, n, 700), a13 = gz(out, f, n, 1300), a10 = gz(out, f, n, 1000), a3 = gz(out, f, n, 300);
        printf("  shape %d: 700 %.3f  1300 %.3f  | 1000 %.4f  300 %.4f\n", shape, a7, a13, a10, a3);
        HX_CHECK(a7 > 0.1 && a13 > 0.1, "sidebands weak shape %d", shape);
        HX_CHECK(a10 < 0.01 * a7, "input leak %.4f shape %d", a10, shape);
        HX_CHECK(a3 < 0.01, "carrier leak %.4f", a3);
    }
    // TRACK mode: carrier = f0*RATIO. f0=400, ratio 0.5 -> 200 Hz carrier on 1000 Hz sine
    { Host h; h.make(hx::fx_create_ringmod); Fx* e = h.fx; e->setParam(5, 1.f); e->setParam(6, 0.5f); e->setParam(2, 1.f); e->snap();
      std::vector<float> out; run(e, 1.0, 48, false, SineGen{1000.f, 0.5f}, defCtx(400.f), &out);
      double a8 = gz(out, 24000, 24000, 800), a12 = gz(out, 24000, 24000, 1200); printf("  track: 800 %.3f 1200 %.3f\n", a8, a12); HX_CHECK(a8 > 0.2 && a12 > 0.2, "track sidebands"); }
    // glide: after FREQ step, carrier moves smoothly (no instant 300 -> 800)
    { Host h; h.make(hx::fx_create_ringmod); Fx* e = h.fx; e->setParam(1, 100.f); e->snap(); std::vector<float> out; run(e, 0.2, 48, false, SineGen{1000.f, 0.5f}, defCtx(), &out);
      e->setParam(1, 800.f); std::vector<float> o2; run(e, 0.05, 48, false, SineGen{1000.f, 0.5f}, defCtx(), &o2);
      double a900 = gz(o2, 0, 2400, 1800), a200 = gz(o2, 0, 2400, 1100); printf("  glide first 50ms: 1100Hz %.3f, 1800Hz %.3f\n", a200, a900); HX_CHECK(a900 < a200 + 0.2, "no glide"); }
    // A_HOLD gate: ramps in 10 ms, out on release
    { Host h; h.make(hx::fx_create_ringmod); Fx* e = h.fx; e->action(hx::A_HOLD, 0.f); std::vector<float> a, b, c;
      run(e, 0.1, 48, false, SineGen{1000.f, 0.5f}, defCtx(), &a);
      double rel = 0; for (size_t i = 2400; i < a.size(); i++) rel = fmax(rel, fabs(a[i] - 0.5f * sinf(6.2831853f * 1000.f * (float)i / SR))); HX_CHECK(rel < 2e-3, "released not dry %.4f", rel);
      e->action(hx::A_HOLD, 1.f); run(e, 0.1, 48, false, SineGen{1000.f, 0.5f}, defCtx(), &b);
      double d = 0; for (size_t i = 2400; i < b.size(); i++) d = fmax(d, fabs(b[i] - 0.5f * sinf(6.2831853f * 1000.f * (float)(i + 4800) / SR))); HX_CHECK(d > 0.2, "held not wet %.3f", d);
      printf("  hold: released dry err %.5f, engaged diff %.2f\n", rel, d); }
    hxt::benchNsPerSample(hx::fx_create_ringmod, "ring");
    return failCount() ? 1 : 0;
}

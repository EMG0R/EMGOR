#include "../fx_delay.h"
#include "fx_testutil.h"
int main() {
    hxt::stdSuite("delay", hx::fx_create_delay, hxt::Opts{});
    using namespace hxt;
    // echo position + decay ratio
    { Host h; h.make(hx::fx_create_delay); Fx* e = h.fx;
      e->setParam(0, 1.f); e->setParam(1, 100.f); e->setParam(2, 0.5f); e->setParam(3, 20000.f); e->setParam(4, 0.f); e->snap();
      struct Imp { void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) b[i] = (i0 + i == 0) ? 0.05f : 0.f; } };
      std::vector<float> o; run(e, 1.0, 48, false, Imp(), defCtx(), &o);
      auto peak = [&](long a, long b) { long pi = a; for (long i = a; i < b; i++) if (fabsf(o[i]) > fabsf(o[pi])) pi = i; return pi; };
      long p1 = peak(1000, 8000), p2 = peak(p1 + 1000, p1 + 8000);   // time 100 ms = 4800 samples
      printf("  echo1 at %ld (exp 4800) echo2 at %ld (exp 9600) ratio %.3f (fb .5)\n", p1, p2, fabsf(o[p2]) / fabsf(o[p1]));
      HX_CHECK(labs(p1 - 4800) <= 1, "echo1 at %ld", p1); HX_CHECK(labs(p2 - 9600) <= 1, "echo2 at %ld", p2);
      float r = fabsf(o[p2]) / fabsf(o[p1]); HX_CHECK(r > 0.40f && r < 0.52f, "decay ratio %.3f", r); }
    // 0.95 feedback with loud input never explodes; stochastic mode produces output; clear works
    { Host h; h.make(hx::fx_create_delay); Fx* e = h.fx; e->setParam(2, 0.95f); e->setParam(0, 1.f); e->setParam(7, 1.f); e->setParam(6, 60.f); e->snap();
      Stats s = run(e, 8.0, 48, false, NoiseGen{0, 0.9f}, defCtx()); HX_CHECK(!s.bad && s.peak < 2.f, "fb .95 peak %.2f", s.peak);
      e->action(hx::A_CLEAR, 1.f); e->setParam(7, 0.f);
      Stats z = run(e, 0.3, 48, false, SilenceGen(), defCtx()); HX_CHECK(z.peak < 1e-4f, "clear residual %.5f", z.peak); }
    benchNsPerSample(hx::fx_create_delay, "delay", false);
    return failCount() ? 1 : 0;
}

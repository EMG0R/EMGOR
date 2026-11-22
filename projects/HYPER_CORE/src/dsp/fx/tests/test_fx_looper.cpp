#include "../fx_looper.h"
#include "fx_testutil.h"
int main() {
    hxt::stdSuite("looper", hx::fx_create_looper, hxt::Opts{});
    using namespace hxt;
    const int N = 48000;
    // record 1 s of noise (layer 0), then play back with silent input: identical to the recording within 1e-3 outside the 10 ms end fades
    { Host h; h.make(hx::fx_create_looper); hx::FxLooper* lp = (hx::FxLooper*)h.fx; lp->setParam(0, 1.f); lp->snap();
      std::vector<float> rec, a, b, c;
      lp->action(hx::A_MAIN, 1.f);
      struct Nz { mutable uint32_t s = 99; void operator()(float* x, int m, long) const { for (int i = 0; i < m; i++) x[i] = 0.5f * (rnd01(s) * 2.f - 1.f); } };
      run(lp, 0.0, 48, false, Nz(), defCtx());   // process pending action state
      // recorded signal collected from a mirror generator
      Nz mirror; { std::vector<float> t(N); long done = 0; while (done < N) { int m = 48; mirror(t.data() + done, m, done); done += m; } rec = t; }
      Nz g; run(lp, 1.0, 48, false, g, defCtx(), &a);   // layer 0 records while input = noise (dry heard too)
      HX_CHECK(lp->state() == hx::FxLooper::S_REC1, "state rec1 (%d)", lp->state());
      lp->action(hx::A_MAIN, 1.f);
      run(lp, 1.0, 48, false, SilenceGen(), defCtx(), &b);
      HX_CHECK(lp->length() == N && lp->layers() == 1, "len %d layers %d", lp->length(), lp->layers());
      double err = 0; int f = 480 + 480; for (int i = f; i < N - f; i++) err = fmax(err, fabs(b[i] - rec[i] * 0.75f * 0 - 0.f * rec[i]) * 0 + fabs(b[i] - rec[i]));
      printf("  playback vs recording (mix 1, silent in) max err %.6f\n", err);
      HX_CHECK(err < 1e-3, "playback error %.6f", err);
      // overdub layer 2 with a 300 Hz sine, one loop long
      lp->action(hx::A_MAIN, 1.f);
      run(lp, 1.0 + 0.1, 48, false, SineGen{300.f, 0.2f}, defCtx(), &c);
      HX_CHECK(lp->layers() == 2, "layers after overdub %d", lp->layers());
      std::vector<float> d; run(lp, 1.0, 48, false, SilenceGen(), defCtx(), &d);
      // the overdub began at an arbitrary loop phase; layer-2 content must have raised the energy over layer 0 alone
      double e0 = 0, e1 = 0; for (int i = 0; i < N; i++) { e0 += (double)b[i] * b[i]; e1 += (double)d[i] * d[i]; }
      printf("  overdub energy: 1 layer %.1f -> 2 layers %.1f\n", e0, e1); HX_CHECK(e1 > e0 * 1.05, "overdub adds");
      // dry duck: with layers=2 of N=%d dry*(1-2/N)
      // clear + stop/play
      lp->action(hx::A_CLEAR, 1.f); std::vector<float> z; run(lp, 0.5, 48, false, SilenceGen(), defCtx(), &z);
      Stats s0; scan(s0, z.data() + 4800, (int)z.size() - 4800); HX_CHECK(s0.peak < 1e-6f && lp->layers() == 0, "clear: peak %.6f layers %d", s0.peak, lp->layers()); }
    // transpose / reverse / stop-play: bounded, finite, nonzero
    { Host h; h.make(hx::fx_create_looper); Fx* e = h.fx; hx::FxLooper* lp = (hx::FxLooper*)e;
      lp->action(hx::A_MAIN, 1.f); run(e, 0.5, 48, false, SineGen{220.f, 0.3f}, defCtx()); lp->action(hx::A_MAIN, 1.f);
      e->setParam(2, 7.f); e->setParam(3, 1.f); e->snap(); Stats s = run(e, 2.0, 48, true, SilenceGen(), defCtx());
      HX_CHECK(!s.bad && s.energy > 1.0, "transp+reverse output"); lp->action(hx::A_ALT, 1.f); Stats t = run(e, 0.5, 48, false, SilenceGen(), defCtx());
      HX_CHECK(!t.bad, "stop"); lp->action(hx::A_ALT, 1.f); Stats u = run(e, 0.5, 48, false, SilenceGen(), defCtx()); HX_CHECK(!u.bad && u.energy > 0.1, "replay energy %.2f", u.energy); }
    printf("  layers x seconds: %d x %d, arena bytes %lu\n", HX_LOOP_LAYERS, HX_LOOP_SECONDS, (unsigned long)(HX_LOOP_LAYERS * HX_LOOP_SECONDS * 48000UL * 4));
    benchNsPerSample(hx::fx_create_looper, "looper", false);
    return failCount() ? 1 : 0;
}

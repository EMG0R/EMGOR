// Flanger test. Default has THRUZ=1 (v1 default): at MIX=0 the output is the polarity-INVERTED dry (1 sample late), as in v1 / McCurdy's
// Flanger.csd (ntrpol(-dry, flange, mix)), so the stdSuite "MIX=0 == dry" check is relaxed (mixZeroIsDry=false) and re-done below
// explicitly: THRUZ=0 -> exact dry, THRUZ=1 -> exact inverted dry.
#include "../fx_flanger.h"
#include "fx_testutil.h"
static double rmsTail(const std::vector<float>& o) { double e = 0; size_t f = o.size() / 2; for (size_t i = f; i < o.size(); i++) e += (double)o[i] * o[i]; return sqrt(e / (o.size() - f)); }
int main() {
    hxt::Opts op; op.mixZeroIsDry = false;
    hxt::stdSuite("flange", hx::fx_create_flanger, op);
    using namespace hxt;
    auto mk = [&](Host& h, float depth, float offMs, float fb, float tz, float mix) { h.make(hx::fx_create_flanger); Fx* e = h.fx;
        e->setParam(0, mix); e->setParam(2, depth); e->setParam(3, offMs); e->setParam(4, fb); e->setParam(6, tz); e->snap(); return e; };
    // static comb: offset 2 ms, no feedback, no depth, THRUZ off: 250 Hz is a notch, 500 Hz a peak (0.5 + 0.5 z^-D)
    { double r[2]; float fr[2] = { 250.f, 500.f };
      for (int k = 0; k < 2; k++) { Host h; Fx* e = mk(h, 0.f, 2.f, 0.f, 0.f, 1.f); std::vector<float> o; run(e, 1.0, 48, false, SineGen{fr[k], 0.5f}, defCtx(), &o); r[k] = rmsTail(o); }
      printf("  comb: rms@250Hz %.4f (notch) rms@500Hz %.4f (peak, in %.4f)\n", r[0], r[1], 0.5 / sqrt(2.0));
      HX_CHECK(r[0] < 0.04 && r[1] > 0.30, "comb notch/peak (notch depth limited by the DC blocker phase lead, as in v1)"); }
    // sweeping: sine 1 kHz through depth 6 ms @ 2 Hz, fb .8: amplitude fluctuates strongly
    { Host h; Fx* e = mk(h, 6.f, 0.5f, 0.8f, 0.f, 1.f); e->setParam(1, 2.f); e->snap(); std::vector<float> o; run(e, 2.0, 48, false, SineGen{1000.f, 0.3f}, defCtx(), &o);
      double mn = 1e9, mx = 0; for (size_t i = 48000; i + 480 < o.size(); i += 480) { double s = 0; for (int j = 0; j < 480; j++) s += (double)o[i + j] * o[i + j]; s = sqrt(s / 480); mn = fmin(mn, s); mx = fmax(mx, s); }
      printf("  sweep: window rms min %.4f max %.4f\n", mn, mx); HX_CHECK(mx > 2.5 * mn, "no sweeping comb"); }
    // MIX=0 behaviours
    for (int tz = 0; tz < 2; tz++) { Host h; Fx* e = mk(h, 7.8f, 0.02f, 0.9f, (float)tz, 0.f); std::vector<float> o; run(e, 0.5, 48, false, SineGen{440.f, 0.5f}, defCtx(), &o);
      double err = 0; for (size_t i = 24000; i < o.size(); i++) { float ref = 0.5f * sinf(6.2831853f * 440.f * (float)(tz ? i - 1 : i) / SR) * (tz ? -1.f : 1.f); err = fmax(err, fabs(o[i] - ref)); }
      HX_CHECK(err < 2e-3, "MIX=0 THRUZ=%d err %.5f", tz, err); }
    // all LFO shapes, stereo, fb extremes: bounded
    for (int sh = 0; sh < 5; sh++) for (float fb : { -1.f, 1.f }) { Host h; Fx* e = mk(h, 10.f, 1.f, fb, 1.f, 1.f); e->setParam(5, (float)sh); e->setParam(1, 7.f); e->snap();
      Stats s = run(e, 2.0, 48, true, NoiseGen{5, 0.3f}, defCtx()); HX_CHECK(!s.bad && s.peak < 16.f, "shape %d fb %.0f peak %.2f", sh, fb, s.peak); }
    benchNsPerSample(hx::fx_create_flanger, "flange", false);
    return failCount() ? 1 : 0;
}

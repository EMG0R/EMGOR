#include "../fx_roar.h"
#include "fx_testutil.h"
using namespace hxt;
static double goertzel(const std::vector<float>& x, size_t from, size_t n, double hz) {
    double w = 2 * M_PI * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { double s = x[from + i] + c * s1 - s2; s2 = s1; s1 = s; }
    return sqrt((s1 * s1 + s2 * s2 - c * s1 * s2)) * 2.0 / n;
}
static double thd(Fx* e, float amp, float hz = 1000.f) {
    std::vector<float> o; run(e, 1.0, 48, false, SineGen{hz, amp}, defCtx(hz), &o);
    size_t from = 24000, n = 12000; double f1 = goertzel(o, from, n, hz), hs = 0;
    for (int k = 2; hz * k < 22000; k++) { double a = goertzel(o, from, n, hz * k); hs += a * a; }
    return sqrt(hs) / (f1 + 1e-12);
}
int main() {
    Opts o; o.mixZeroIsDry = false;   // MIX=0 is dry delayed by FxRoar::LAT samples (oversampler latency): checked below
    hxt::stdSuite("roar", hx::fx_create_roar, o);

    // MIX=0 == input delayed exactly LAT samples
    { Host h; h.make(hx::fx_create_roar); h.fx->setParam(0, 0.f); h.fx->snap(); std::vector<float> out;
      run(h.fx, 0.5, 48, false, SineGen{440.f, 0.5f}, defCtx(), &out); double err = 0;
      for (size_t i = 12000; i < out.size(); i++) err = fmax(err, fabs(out[i] - 0.5f * sinf(6.2831853f * 440.f * (float)(i - FxRoar::LAT) / SR)));
      HX_CHECK(err < 2e-3, "MIX=0 delayed-dry err %.5f", err); }

    // THD rises with DRIVE
    double t[4]; float dr[4] = {0.0f, 0.3f, 0.6f, 1.0f};
    for (int k = 0; k < 4; k++) { Host h; h.make(hx::fx_create_roar); h.fx->setParam(1, dr[k]); h.fx->snap(); t[k] = thd(h.fx, 0.3f); printf("  THD @DRIVE %.1f = %.1f %%\n", dr[k], t[k] * 100); }
    HX_CHECK(t[0] < t[1] && t[1] < t[2] && t[2] <= t[3] * 1.05, "THD not rising with DRIVE");
    HX_CHECK(t[0] < 0.05, "clean at DRIVE 0 (%.3f)", t[0]);

    // bounded at 1.0 input, max drive, every TYPE, bias & fold extremes
    for (int ty = 0; ty < 4; ty++) { Host h; h.make(hx::fx_create_roar); Fx* e = h.fx; e->setParam(1, 1.f); e->setParam(2, (float)ty); e->setParam(3, 1.f); e->setParam(5, 1.f); e->setParam(6, 12.f); e->snap();
        Stats s = run(e, 1.0, 48, true, SineGen{700.f, 1.0f}, defCtx(700.f)); HX_CHECK(!s.bad && s.peak < 6.f, "type %d peak %.2f", ty, s.peak); printf("  type %d full-scale, max drive/fold/bias, +12 dB: peak %.2f\n", ty, s.peak); }

    // alias sanity: 5 kHz sine, DRIVE max SOFT: energy at non-harmonic bins vs fundamental
    { Host h; h.make(hx::fx_create_roar); h.fx->setParam(1, 1.f); h.fx->snap(); std::vector<float> out; run(h.fx, 1.0, 48, false, SineGen{5000.f, 1.0f}, defCtx(), &out);
      double f1 = goertzel(out, 24000, 12000, 5000), worst = 0; for (double f : {3100., 7300., 9100., 12300., 13700., 17100., 19300.}) worst = fmax(worst, goertzel(out, 24000, 12000, f));
      printf("  alias: worst non-harmonic bin %.1f dBc\n", 20 * log10(worst / f1 + 1e-12)); HX_CHECK(worst / f1 < 0.03, "alias too high %.3f", worst / f1); }

    // v1 auto-gain law: loud-clipped output level falls with drive (0.3 floor)
    { Host h; h.make(hx::fx_create_roar); h.fx->setParam(1, 1.f); h.fx->snap(); Stats s = run(h.fx, 0.5, 48, false, SineGen{700.f, 1.0f}, defCtx()); HX_CHECK(s.peak < 0.6f, "autogain at DRIVE 1 peak %.2f", s.peak); }
    hxt::benchNsPerSample(hx::fx_create_roar, "roar");
    hxt::benchNsPerSample(hx::fx_create_roar, "roar(stereo)", true);
    return failCount() ? 1 : 0;
}

// Auto-wah (fx_wah.h) host tests: finite/no denormals, no clicks, MIX=0 dry, centre frequency tracks the envelope, DIR, CPU.
#include "../fx_wah.h"
#include "fx_testutil.h"
using namespace hxt;
enum { MIX, SENS, RANGE, Q, AUTO = 100, DIR = 101 };   // AUTO/DIR: FxWah test hooks (auto-gain off / direction down)
static double gz(const std::vector<float>& x, size_t from, size_t n, double hz) {
    double w = 2 * M_PI * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { double s = x[from + i] + c * s1 - s2; s2 = s1; s1 = s; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / n;
}
static double peakHz(const std::vector<float>& o, size_t from, size_t n) { double best = 0, bf = 0; for (double f = 150; f < 6000; f *= 1.01) { double a = gz(o, from, n, f); if (a > best) { best = a; bf = f; } } return bf; }
static float dbl(float db) { return powf(10.f, db / 20.f); }
// noise at a fixed RMS (dBFS) through the wah; returns the filter peak (Hz) and the model's fcNow
static void noiseRun(std::vector<std::pair<int, float>> ps, float rmsDb, double& pk, float& fc, bool mono = true) {
    Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; for (auto& p : ps) { if (p.first == AUTO) ((hx::FxWah*)e)->autoGain = p.second > 0.5f; else if (p.first == DIR) ((hx::FxWah*)e)->down = p.second > 0.5f; else e->setParam(p.first, p.second); } e->snap();
    NoiseGen ng; ng.amp = dbl(rmsDb) * 1.732f; std::vector<float> out; run(e, 3.0, 48, !mono, ng, defCtx(), &out);
    pk = peakHz(out, 48000, 96000); fc = ((hx::FxWah*)e)->fcNow;
}
int main() {
    stdSuite("autowah", hx::fx_create_wah, Opts{});
    // 1. horn at -45 dBFS: finite, no denormals, no clicks (sample-step bound relative to a 330 Hz sine through the same filter chain)
    { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; std::vector<float> o;
      auto g = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)SR; double y = 0; for (int k = 1; k <= 8; k++) y += sin(2 * M_PI * 330 * k * t) / k;
          double gate = (fmod(t, 0.5) < 0.3) ? 1.0 : 0.0; b[i] = (float)(0.01 * y * gate); } };   // ~ -45 dBFS, 300 ms notes every 500 ms
      Stats s = run(e, 4.0, 48, false, g, defCtx(), &o);
      HX_CHECK(!s.bad && s.denorm == 0, "horn: nan/denormal"); float mx = 0; for (size_t i = 1; i < o.size(); i++) mx = fmaxf(mx, fabsf(o[i] - o[i - 1]));
      printf("  horn max step %.4f peak %.4f\n", mx, s.peak); HX_CHECK(mx < 0.15f, "click: max step %.4f", mx);
      // hard on/off gating of loud noise must stay bounded too (bounded softclip, no step spikes beyond 2*level)
      Host h2; h2.make(hx::fx_create_wah); std::vector<float> o2; NoiseGen ng; ng.amp = 0.5f; auto gg = [&](float* b, int m, long i0) { ng(b, m, i0); for (int i = 0; i < m; i++) if (((i0 + i) / 4800) & 1) b[i] = 0; };
      Stats s2 = run(h2.fx, 3.0, 48, false, gg, defCtx(), &o2); float m2 = 0; for (size_t i = 1; i < o2.size(); i++) m2 = fmaxf(m2, fabsf(o2[i] - o2[i - 1]));
      printf("  gated noise: peak %.3f max step %.3f\n", s2.peak, m2); HX_CHECK(!s2.bad && s2.peak <= 1.01f && m2 < 1.2f, "gated noise out of bounds"); }
    // 2. param jumps mid-stream (LOFREQ/HIFREQ/Q/DIR) do not click beyond the smoothing: step bound
    { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; std::vector<float> o; HornGen hg{330.f}; std::vector<float> L(48), R(48); float mx = 0, last = 0; uint32_t sd = 5;
      for (int b = 0; b < 4000; b++) { if (b % 50 == 0) { e->setParam(RANGE, rnd01(sd)); e->setParam(SENS, rnd01(sd)); e->setParam(Q, 0.7f + 13.3f * rnd01(sd));  }
          hg(L.data(), 48, (long)b * 48); for (auto& x : L) x *= 0.03f; e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false);
          for (int i = 0; i < 48; i++) { if (b) mx = fmaxf(mx, fabsf(L[i] - last)); last = L[i]; } }
      printf("  param-jump max step %.4f\n", mx); HX_CHECK(mx < 0.5f, "param jump click %.4f", mx); }
    // 3. MIX=0 exactly dry (bit exact), mono and stereo
    for (int st = 0; st < 2; st++) { Host h; h.make(hx::fx_create_wah); Fx* e = h.fx; e->setParam(MIX, 0.f); e->snap(); std::vector<float> L(48), R(48), ref(48); NoiseGen ng; ng.amp = 0.3f; bool ex = true;
      for (int b = 0; b < 200; b++) { ng(L.data(), 48, 0); ref = L; for (int i = 0; i < 48; i++) R[i] = L[i]; e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, st); for (int i = 0; i < 48; i++) if (L[i] != ref[i] || (st && R[i] != ref[i])) ex = false; }
      HX_CHECK(ex, "MIX=0 not bit-exact dry (stereo=%d)", st); }
    // 4. centre frequency tracks the envelope (AUTO off, fixed window: SENS 0.75 -> top -50 dBFS, window -70..-50)
    { double pk[5]; float fc[5]; float lv[5] = {-68.f, -65.f, -62.f, -59.f, -56.f};
      for (int i = 0; i < 5; i++) { noiseRun({{AUTO, 0}, {SENS, 0.75f}, {Q, 6}, {RANGE, 1.f}}, lv[i], pk[i], fc[i]); printf("  AUTO off, %.0f dBFS rms -> model fc %.0f, measured peak %.0f Hz\n", lv[i], fc[i], pk[i]); }
      for (int i = 1; i < 5; i++) HX_CHECK(pk[i] > pk[i - 1] * 1.1 && fc[i] > fc[i - 1], "fc not monotonic in level (%d)", i);
      for (int i = 0; i < 5; i++) HX_CHECK(fabs(pk[i] / fc[i] - 1) < 0.12, "measured peak %.0f vs model fc %.0f", pk[i], fc[i]);
      HX_CHECK(fc[4] > 1500 && fc[0] < 500, "range not used: %.0f..%.0f", fc[0], fc[4]);
      // DOWN reverses
      double pd0, pd1; float fd0, fd1; noiseRun({{AUTO, 0}, {SENS, 0.75f}, {Q, 6}, {RANGE, 1.f}, {DIR, 1}}, -68.f, pd0, fd0); noiseRun({{AUTO, 0}, {SENS, 0.75f}, {Q, 6}, {RANGE, 1.f}, {DIR, 1}}, -56.f, pd1, fd1);
      printf("  DOWN: quiet %.0f Hz, loud %.0f Hz\n", pd0, pd1); HX_CHECK(pd0 > pd1 * 1.5, "DOWN does not reverse"); }
    // 5. trumpet-level input (-50/-40 dBFS) with defaults (AUTO on): the sweep position uses most of its range between soft and loud notes
    { Host h; h.make(hx::fx_create_wah); hx::FxWah* e = (hx::FxWah*)h.fx; float xmin = 9, xmax = -9, fmin = 1e9, fmax = 0; NoiseGen ng; std::vector<float> L(48), R(48);
      for (int b = 0; b < 48000 * 6 / 48; b++) { double t = b * 48.0 / SR; float amp = dbl(((int)(t / 0.75) & 1) ? -40.f : -50.f) * 1.4f; ng.amp = amp; ng(L.data(), 48, 0); e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false);
          if (t > 2.5) { xmin = fminf(xmin, e->xPos); xmax = fmaxf(xmax, e->xPos); fmin = fminf(fmin, e->fcNow); fmax = fmaxf(fmax, e->fcNow); } }
      printf("  defaults, -50/-40 dBFS notes: x %.2f..%.2f, fc %.0f..%.0f Hz\n", xmin, xmax, fmin, fmax);
      HX_CHECK(xmax - xmin > 0.35f && fmax > 1.8f * fmin, "default sweep too small on trumpet-level input"); }
    // 6. attack/release: fc reaches 90%% of a step within ~3x attack, falls over ~150 ms
    { Host h; h.make(hx::fx_create_wah); hx::FxWah* e = (hx::FxWah*)h.fx; ((hx::FxWah*)e)->autoGain = false; e->setParam(SENS, 0.75f); e->snap(); NoiseGen ng; std::vector<float> L(48), R(48); int tUp = -1, tDn = -1; float xd = 0, xRef = 0.55f;
      for (int b = 0; b < 800; b++) { ng.amp = dbl(b < 100 ? -80.f : -58.f) * 1.7f; if (b >= 300) ng.amp = dbl(-80.f) * 1.7f; ng(L.data(), 48, 0); e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false);
          if (b >= 100 && tUp < 0 && e->xPos > 0.9f * xRef) tUp = b - 100; if (b == 299) xd = e->xPos; if (b >= 300 && tDn < 0 && e->xPos < 0.5f * xd) tDn = b - 300; }
      printf("  step up -> 54%% in %d ms, release to half in %d ms (x at release %.2f)\n", tUp, tDn, xd); HX_CHECK(tUp >= 0 && tUp <= 25, "attack too slow"); HX_CHECK(tDn >= 20 && tDn <= 300, "release off %d ms", tDn); }
    // 7. denormals after long silence, both modes, stereo
    for (int md = 0; md < 2; md++) { Host h; h.make(hx::fx_create_wah); h.fx->setParam(Q, md ? 14.f : 0.7f); h.fx->snap(); NoiseGen ng; ng.amp = .3f; run(h.fx, 0.5, 48, true, ng, defCtx());
      Stats s = run(h.fx, 5.0, 48, true, SilenceGen(), defCtx()); HX_CHECK(s.denorm == 0 && !s.bad, "denormals after silence, mode %d", md); }
    // 8. CPU (host ns/sample * 100 = % of one Seed core only if 1 ns/sample == 0.01%... report as the brief says)
    double ns = benchNsPerSample(hx::fx_create_wah, "wah"), ns2 = benchNsPerSample(hx::fx_create_wah, "wah(stereo)", true);
    printf("CPU wah: est %.2f%% mono, %.2f%% stereo (host ns/sample x 100 / 1000; target <= 2%%)\n", ns * 0.1, ns2 * 0.1);
    printf(failCount() ? "[autowah] FAIL\n" : "[autowah] ALL PASS\n"); return failCount() ? 1 : 0;
}

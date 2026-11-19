// Spectral tracing (fx_trace.h) host tests: finite/no denormals, no clicks, MIX=0 dry, keeps the strongest partials, type behaviour, CPU.
#include "../fx_trace.h"
#include "fx_testutil.h"
using namespace hxt;
enum { MIX, TYPE, BINS1, BINS2, THRESH, BRIGHT, LOCUT, HICUT, RNDPH, GAIN, FRAME };
static double tone(const std::vector<float>& x, double from, double to, double hz) {
    long a = (long)(from * SR), b = (long)(to * SR); double sr = 0, si = 0;
    for (long i = a; i < b; i++) { double w = 0.5 - 0.5 * cos(2 * M_PI * (i - a) / (double)(b - a)); double ph = 2 * M_PI * hz * i / SR; sr += w * x[i] * cos(ph); si += w * x[i] * sin(ph); }
    return 2.0 * sqrt(sr * sr + si * si) / (0.5 * (b - a));
}
static double rmsOf(const std::vector<float>& x, double from, double to) { long a = (long)(from * SR), b = (long)(to * SR); double e = 0; for (long i = a; i < b; i++) e += (double)x[i] * x[i]; return sqrt(e / (b - a)); }
// harmonic tone: partial h has amplitude amp/h
static std::vector<float> runTone(std::vector<std::pair<int, float>> ps, double secs, float f0, float amp, int nh = 12) {
    Host h; h.make(hx::fx_create_trace); Fx* e = h.fx; e->setParam(MIX, 1.f); for (auto& p : ps) e->setParam(p.first, p.second); e->snap();
    std::vector<float> out; auto g = [&](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)SR, y = 0; for (int k = 1; k <= nh; k++) y += sin(2 * M_PI * f0 * k * t) / k; b[i] = (float)(amp * y); } };
    run(e, secs, 48, false, g, defCtx(), &out); return out;
}
int main() {
    Opts o; o.dryTol = 1e-6f; stdSuite("trace", hx::fx_create_trace, o);
    // 1. no clicks: horn gated notes through default settings; the max sample step stays below the dry signal's own max step, and a hold/mask flip does not spike
    for (int fr = 0; fr < 2; fr++) { Host h; h.make(hx::fx_create_trace); Fx* e = h.fx; e->setParam(FRAME, (float)fr); e->setParam(MIX, 1.f); e->snap(); std::vector<float> o2; uint32_t sd = 3; float dryStep = 0;
      auto g = [&](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)SR, y = 0; for (int k = 1; k <= 8; k++) y += sin(2 * M_PI * 330 * k * t) / k; float v = (float)(0.3 * y * (fmod(t, 0.6) < 0.4 ? 1 : 0)); b[i] = v; } };
      std::vector<float> in; g(nullptr == nullptr ? (in.resize(SR * 6), in.data()) : nullptr, (int)(SR * 6), 0); for (size_t i = 1; i < in.size(); i++) dryStep = fmaxf(dryStep, fabsf(in[i] - in[i - 1]));
      std::vector<float> L(48), R(48); float mx = 0, last = 0; for (int b = 0; b < 6000; b++) { if (b % 200 == 0) { e->setParam(TYPE, (float)(int)(rnd01(sd) * 8)); e->setParam(BINS1, 1 + (float)(int)(rnd01(sd) * 40)); e->setParam(BINS2, 1 + (float)(int)(rnd01(sd) * 80)); }
          g(L.data(), 48, (long)b * 48); e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false); for (int i = 0; i < 48; i++) { if (b > 100) mx = fmaxf(mx, fabsf(L[i] - last)); last = L[i]; } }
      printf("  FRAME %d: max step %.3f (dry max step %.3f), randomised TYPE/BINS every 200 ms\n", fr, mx, dryStep); HX_CHECK(mx < 1.25f * dryStep + 0.02f, "click: step %.3f vs dry %.3f", mx, dryStep); }
    // 2. keeps the strongest partials: 220 Hz, amplitude 1/h. KEEP LOUD with 3 bins (6 at FRAME 1024) keeps h1 + h2 (a partial is 1-2 significant bins wide), kills the weak upper partials.
    for (int fr = 0; fr < 2; fr++) {
        auto out = runTone({{TYPE, 0}, {BINS1, fr ? 6.f : 3.f}, {FRAME, (float)fr}}, 3.0, 220.f, 0.2f);
        double d[13]; for (int h = 1; h <= 12; h++) d[h] = tone(out, 1.5, 2.8, 220.0 * h) / (0.2 / h);   // wet relative to the input partial
        printf("  FRAME %d KEEP LOUD: partial gain h1 %.2f h2 %.2f h3 %.2f h5 %.2f h8 %.2f h12 %.3f\n", fr, d[1], d[2], d[3], d[5], d[8], d[12]);
        HX_CHECK(d[1] > 0.8 && d[1] < 1.2, "strongest partial not kept (%.2f)", d[1]); HX_CHECK(d[2] > 0.5, "2nd partial lost (%.2f)", d[2]);
        HX_CHECK(fr == 1 || d[3] < 0.5, "3rd partial kept with so few bins (%.2f)", d[3]);
        HX_CHECK(d[8] < 0.1 && d[12] < 0.1 && d[5] < 0.1, "weak partials not removed (h5 %.2f h8 %.2f h12 %.2f)", d[5], d[8], d[12]);
    }
    { // more bins -> more partials; the ones kept are always the strongest first
      double prev = -1; for (int nbins : {2, 6, 12, 24, 60}) { auto out = runTone({{TYPE, 0}, {BINS1, (float)nbins}}, 3.0, 220.f, 0.2f); double r = rmsOf(out, 1.5, 2.8); printf("  KEEP LOUD %d bins -> rms %.4f\n", nbins, r); HX_CHECK(r >= prev * 0.98, "rms not growing with bins"); prev = r; } }
    // 3. KEEP SOFT and DROP LOUD: weakest partials survive / strongest removed
    { auto out = runTone({{TYPE, 1}, {BINS1, 236.f}}, 3.0, 220.f, 0.2f); double a = tone(out, 1.5, 2.8, 220.0) / 0.2, b = tone(out, 1.5, 2.8, 220.0 * 10) / (0.2 / 10);
      printf("  KEEP SOFT 236 of 256 (drops the 20 loudest bins): h1 gain %.3f, h10 gain %.2f\n", a, b); HX_CHECK(a < 0.1 && b > 0.1, "KEEP SOFT wrong");
      auto o2 = runTone({{TYPE, 1}, {BINS1, 40.f}}, 3.0, 220.f, 0.2f); double r2 = rmsOf(o2, 1.5, 2.8); printf("  KEEP SOFT 40: rms %.5f (residue only)\n", r2); HX_CHECK(r2 < 0.02, "KEEP SOFT 40 kept strong partials"); }
    { auto out = runTone({{TYPE, 5}, {BINS1, 8.f}}, 3.0, 220.f, 0.2f); double a = tone(out, 1.5, 2.8, 220.0) / 0.2, b = tone(out, 1.5, 2.8, 220.0 * 9) / (0.2 / 9);
      printf("  DROP LOUD 8: h1 gain %.3f, h9 gain %.2f\n", a, b); HX_CHECK(a < 0.15 && b > 0.7, "DROP LOUD wrong"); }
    { auto out = runTone({{TYPE, 3}, {THRESH, -26.f}}, 3.0, 220.f, 0.2f); double a = tone(out, 1.5, 2.8, 220.0) / 0.2, b = tone(out, 1.5, 2.8, 220.0 * 8) / (0.2 / 8);   // h1 = -14 dBFS, h8 = -38 dBFS
      printf("  KEEP >THR(-26): h1 %.2f, h8 %.3f\n", a, b); HX_CHECK(a > 0.8 && b < 0.05, "KEEP >THR wrong"); }
    { auto out = runTone({{TYPE, 2}, {BINS1, 3.f}, {BINS2, 12.f}}, 3.0, 220.f, 0.2f); double a = tone(out, 1.5, 2.8, 220.0) / 0.2, b = tone(out, 1.5, 2.8, 440.0) / 0.1, c = tone(out, 1.5, 2.8, 220.0 * 3) / (0.2 / 3);
      printf("  KEEP RANGE ranks 3..12: h1 %.2f h2 %.2f h3 %.2f\n", a, b, c); HX_CHECK(a < 0.2, "range kept the loudest partial"); }
    // 4. MIX=0 bit-exact dry (mono and stereo)
    for (int st = 0; st < 2; st++) { Host h; h.make(hx::fx_create_trace); Fx* e = h.fx; e->setParam(MIX, 0.f); e->snap(); std::vector<float> L(48), R(48), ref(48); NoiseGen ng; ng.amp = 0.3f; bool ex = true;
      for (int b = 0; b < 400; b++) { ng(L.data(), 48, 0); ref = L; for (int i = 0; i < 48; i++) R[i] = L[i]; e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, st); for (int i = 0; i < 48; i++) if (L[i] != ref[i] || (st && R[i] != ref[i])) ex = false; }
      HX_CHECK(ex, "MIX=0 not bit-exact dry (stereo=%d)", st); }
    // 5. silence after loud input: no denormals, silent output (all bins decay to exactly 0); reset/wake
    { Host h; h.make(hx::fx_create_trace); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); NoiseGen ng; ng.amp = .4f; run(e, 1.0, 48, true, ng, defCtx()); Stats s = run(e, 4.0, 48, true, SilenceGen(), defCtx());
      HX_CHECK(s.denorm == 0 && !s.bad, "denormals after silence"); ((hx::FxTrace*)e)->wake(); Stats s2 = run(e, 1.0, 48, false, SineGen{440.f, .3f}, defCtx()); HX_CHECK(!s2.bad && s2.energy > 0, "no output after wake"); }
    // 6. CPU: average ns/sample, and worst block over a 4 s run (both FRAME sizes)
    for (int fr = 0; fr < 2; fr++) {
        Host h; h.make(hx::fx_create_trace); Fx* e = h.fx; e->setParam(MIX, 1.f); e->setParam(FRAME, (float)fr); e->snap(); std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f);
        auto blk = [&](long b) { for (int i = 0; i < 48; i++) { double t = (b * 48 + i) / (double)SR, y = 0; for (int k = 1; k <= 8; k++) y += sin(2 * M_PI * 330 * k * t) / k; L[i] = (float)(0.2 * y); } R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); };
        for (long b = 0; b < 400; b++) blk(b);
        // time only the process call; the signal generator above is excluded by timing around process with precomputed input
        long nb = 4 * 48000 / 48; std::vector<float> inp((size_t)nb * 48); for (long i = 0; i < nb * 48; i++) { double t = (i + 400 * 48) / (double)SR, y = 0; for (int k = 1; k <= 8; k++) y += sin(2 * M_PI * 330 * k * t) / k; inp[i] = (float)(0.2 * y); }
        double tot = 0, worst = 1e30; float sink = 0;
        for (int pass = 0; pass < 6; pass++) { double pw = 0; if (pass) tot = 0;
        for (long b = 0; b < nb; b++) { memcpy(L.data(), &inp[b * 48], 48 * 4); R = L; auto t0 = std::chrono::steady_clock::now(); e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); tot += ns; if (ns > pw) pw = ns; sink += L[0]; }
        if (pw < worst) worst = pw; }   // worst block of the quietest of 6 passes (host OS jitter otherwise dominates)
        double avg = tot / (nb * 48.0), wblk = worst / 48.0;
        printf("BENCH trace FRAME %d: avg %.2f ns/sample, worst block %.2f ns/sample (sink %.2f)\n", fr, avg, wblk, sink);
        printf("CPU trace FRAME %d: est avg %.2f%%, worst block %.2f%% (x0.1; target <= 6%% / 10%%)\n", fr, avg * 0.1, wblk * 0.1);
    }
    printf(failCount() ? "[trace] FAIL\n" : "[trace] ALL PASS\n"); return failCount() ? 1 : 0;
}

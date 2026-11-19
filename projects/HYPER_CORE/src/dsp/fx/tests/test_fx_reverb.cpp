// fx_reverb v2 host tests: std suite, RT60 (Schroeder EDC), echo density growth, WARP click test, MIX=0 exact dry,
// max-setting level stability, wake() on a dirty arena, CPU + memory. Build: make out/test_reverb && ./out/test_reverb
#include "../fx_reverb.h"
#include "fx_testutil.h"
#include <functional>
#include <algorithm>
using namespace hxt;
typedef hx::FxReverb Rv;
enum { MIX, DECAY, SIZE, WARP, DAMP, MODD, MODR, LOWCUT, WIDTH, PRE, SCAT, SPREAD, DRIFT, SEED, BLOOM, CHAOS };
static double winDb(const std::vector<float>& x, long a, long b) { double e = 0; for (long i = a; i < b && i < (long)x.size(); i++) e += (double)x[i] * x[i]; return 10 * log10(e / (double)(b - a) + 1e-30); }
// RT60 from the Schroeder backward-integrated energy decay curve: extrapolate the -5..-35 dB slope (T30) to 60 dB
static double rt60(const std::vector<float>& x) {
    std::vector<double> e(x.size()); double acc = 0; for (long i = (long)x.size() - 1; i >= 0; i--) { acc += (double)x[i] * x[i]; e[i] = acc; }
    double t5 = -1, t35 = -1;
    for (size_t i = 0; i < e.size(); i++) { double d = 10 * log10(e[i] / e[0] + 1e-30); if (t5 < 0 && d <= -5) t5 = (double)i / SR; if (t35 < 0 && d <= -35) { t35 = (double)i / SR; break; } }
    return t35 < 0 ? 99.0 : 2.0 * (t35 - t5);
}
// normalised echo density (Abel & Huang): fraction of samples beyond 1 sigma in a window, / erfc(1/sqrt2) -> 1.0 = Gaussian-dense
static double ned(const std::vector<float>& x, double t, int w = 2048) {
    long a = (long)(t * SR) - w / 2; if (a < 0) a = 0; long b = a + w; if (b > (long)x.size()) b = (long)x.size();
    double e = 0; for (long i = a; i < b; i++) e += (double)x[i] * x[i]; double sd = sqrt(e / (double)(b - a)) + 1e-30;
    long c = 0; for (long i = a; i < b; i++) if (fabs(x[i]) > sd) c++;
    return (double)c / (double)(b - a) / 0.3173;
}
static std::vector<float> impulseResp(std::function<void(Fx*)> cfg, double secs) {
    Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); if (cfg) cfg(e); e->snap();
    std::vector<float> a; run(e, secs, 48, false, ImpulseGen(), defCtx(), &a); return a;
}
static double maxStep(const std::vector<float>& x, size_t a) { double m = 0; for (size_t i = a + 1; i < x.size(); i++) m = fmax(m, fabs(x[i] - x[i - 1])); return m; }

int main() {
    Opts o; o.stereoOut = true; o.tailSecs = 20.0;
    hxt::stdSuite("reverb", hx::fx_create_reverb, o);

    // 0. params the firmware maps stay where they were
    { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx;
      HX_CHECK(!strcmp(e->pt[MIX].name, "MIX") && !strcmp(e->pt[WARP].name, "WARP") && !strcmp(e->pt[MODD].name, "MODDEPTH"), "param names");
      HX_CHECK(e->findParam("SCATTER") == SCAT && e->findParam("SPREAD") == SPREAD && e->findParam("DRIFT") == DRIFT, "new params");
      HX_CHECK(e->role[hx::R_MOD] == WARP && e->role[hx::R_AMT] == MIX, "roles"); HX_CHECK(e->np <= HX_MAXP, "np"); }

    // 1. RT60 at defaults (impulse, MIX=1), plus RT follows DECAY
    { auto a = impulseResp(nullptr, 40.0); double r = rt60(a);
      printf("  RT60 at defaults: %.2f s\n", r); HX_CHECK(r > 6.0 && r < 10.5, "default RT60 %.2f not 6-10 s", r); }
    { double rt[3]; float decs[3] = {0.6f, 0.85f, 0.97f};
      for (int k = 0; k < 3; k++) { auto a = impulseResp([&](Fx* e) { e->setParam(DECAY, decs[k]); }, 30.0); rt[k] = rt60(a); printf("  DECAY %.2f -> RT60 %.2f s\n", decs[k], rt[k]); }
      HX_CHECK(rt[0] < rt[1] && rt[1] < rt[2], "RT not increasing with DECAY"); }

    // 2. echo density grows over time (scattered echoes -> dense wash)
    { auto a = impulseResp(nullptr, 4.0); const double ts[] = {0.05, 0.15, 0.3, 0.6, 1.0, 2.0}; double d[6];
      printf("  echo density NED:"); for (int i = 0; i < 6; i++) { d[i] = ned(a, ts[i]); printf(" %.2f@%.2fs", d[i], ts[i]); } printf("\n");
      HX_CHECK(d[1] > d[0] || d[2] > d[0], "density not building early"); HX_CHECK(d[4] > d[1] + 0.05, "density not increasing 0.15->1.0 s (%.2f -> %.2f)", d[1], d[4]);
      HX_CHECK(d[5] > 0.85, "late tail not dense (%.2f)", d[5]); HX_CHECK(d[0] < 0.8 * d[5], "start not sparse enough to hear echoes (%.2f vs %.2f)", d[0], d[5]);
      auto b = impulseResp([&](Fx* e) { e->setParam(SCAT, 0.f); }, 4.0); printf("  (SCATTER=0 NED@0.15s %.2f, @0.3s %.2f)\n", ned(b, 0.15), ned(b, 0.3));
      // the scattered cloud: count distinct echo peaks above -30 dB (of the max) in the first 450 ms
      long pk = 0; float mx = 0; for (long i = 0; i < 21600; i++) mx = fmaxf(mx, fabsf(a[i])); long last = -1000;
      for (long i = 1; i < 21600; i++) if (fabsf(a[i]) > mx * 0.03f && fabsf(a[i]) >= fabsf(a[i - 1]) && fabsf(a[i]) >= fabsf(a[i + 1]) && i - last > 240) { pk++; last = i; }
      printf("  distinct echoes >-30 dB in first 450 ms: %ld\n", pk); }

    // 3. decorrelated stereo
    { auto a = impulseResp(nullptr, 3.0); Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); std::vector<float> l, r; run(e, 3.0, 48, false, ImpulseGen(), defCtx(), &l, &r);
      double sl = 0, sr_ = 0, sx = 0; for (size_t i = 4800; i < l.size(); i++) { sl += l[i] * l[i]; sr_ += r[i] * r[i]; sx += l[i] * r[i]; } double c = sx / sqrt(sl * sr_ + 1e-30);
      printf("  L/R correlation %.3f\n", c); HX_CHECK(fabs(c) < 0.5, "stereo not decorrelated %.3f", c); }

    // 4. MIX=0 exactly dry (bit-exact), signal active in the engine first
    { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 0.f); e->snap(); std::vector<float> L(48), R(48); uint32_t sd = 5; double maxe = 0;
      for (int b = 0; b < 2000; b++) { std::vector<float> in(48); for (int i = 0; i < 48; i++) in[i] = 0.5f * (rnd01(sd) * 2 - 1); L = in; R = in; e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false);
          for (int i = 0; i < 48; i++) maxe = fmax(maxe, fmax(fabs(L[i] - in[i]), fabs(R[i] - in[i]))); }
      printf("  MIX=0 max |out-in| = %g\n", maxe); HX_CHECK(maxe == 0.0, "MIX=0 not exactly dry"); }

    // 5. WARP swept quickly: no clicks. Metric: max |sample step| of the wet tail vs the same input with WARP fixed.
    { auto go = [&](bool sweep, float rate) { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); std::vector<float> out, L(48), R(48); FxCtx c = defCtx(220.f);
          for (int b = 0; b < 48000 * 8 / 48; b++) { float t = b * 48 / SR; if (sweep && t > 3.f) e->setParam(WARP, 0.5f + 0.5f * sinf(kTwoPi * rate * (t - 3.f)));
              for (int i = 0; i < 48; i++) L[i] = 0.3f * sinf(kTwoPi * 220.f * (float)(b * 48 + i) / SR) * (t < 5.f ? 1.f : 0.f); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); out.insert(out.end(), L.begin(), L.end()); }
          return out; };
      auto base = go(false, 0), sw = go(true, 4.f), sw2 = go(true, 12.f); size_t a0 = 3 * 48000;
      double m0 = maxStep(std::vector<float>(base.begin() + a0, base.end()), 0), m1 = maxStep(std::vector<float>(sw.begin() + a0, sw.end()), 0), m2 = maxStep(std::vector<float>(sw2.begin() + a0, sw2.end()), 0);
      auto rmsOf = [&](const std::vector<float>& x) { double e = 0; for (size_t i = a0; i < x.size(); i++) e += (double)x[i] * x[i]; return sqrt(e / (double)(x.size() - a0)); };
      double r0 = rmsOf(base), r1 = rmsOf(sw), r2 = rmsOf(sw2);
      printf("  max |step|/rms: steady %.3f | WARP sweep 4 Hz %.3f | 12 Hz %.3f  (a click-free signal below ~2 kHz stays < 0.37)\n", m0 / r0, m1 / r1, m2 / r2);
      HX_CHECK(m1 < 0.37 * r1 && m2 < 0.37 * r2, "clicks while warping (%.3f / %.3f of rms)", m1 / r1, m2 / r2);
      Stats st; scan(st, sw2.data(), (int)sw2.size()); HX_CHECK(!st.bad && st.denorm == 0, "warp sweep NaN/denorm"); }

    // 6. max settings: noise at full scale, everything cranked, WARP sweeping -> bounded, steady, decays when input stops
    { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); e->setParam(DECAY, 0.99f); e->setParam(SIZE, 2.f); e->setParam(DAMP, 18000.f); e->setParam(MODD, 1.f); e->setParam(MODR, 5.f);
      e->setParam(SCAT, 1.f); e->setParam(SPREAD, 1.f); e->setParam(DRIFT, 2.f); e->setParam(BLOOM, 1.f); e->setParam(CHAOS, 1.f); e->setParam(WIDTH, 1.f); e->snap();
      std::vector<float> L(48), R(48), out; Stats st; uint32_t sd = 3; FxCtx c = defCtx();
      for (int b = 0; b < 48000 * 70 / 48; b++) { float t = b * 48 / SR; e->setParam(WARP, 0.5f + 0.5f * sinf(0.7f * t)); for (int i = 0; i < 48; i++) L[i] = t < 35.f ? 0.9f * (rnd01(sd) * 2 - 1) : 0.f; R = L;
          e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); scan(st, L.data(), 48); scan(st, R.data(), 48); out.insert(out.end(), L.begin(), L.end()); }
      double w1 = winDb(out, 10 * 48000L, 15 * 48000L), w2 = winDb(out, 25 * 48000L, 30 * 48000L), t1 = winDb(out, 36 * 48000L, 38 * 48000L), t2 = winDb(out, 66 * 48000L, 68 * 48000L);
      printf("  max settings: peak %.3f, rms %.1f dB @10-15 s, %.1f dB @25-30 s, tail %.1f dB -> %.1f dB (30 s later)\n", st.peak, w1, w2, t1, t2);
      HX_CHECK(!st.bad && st.denorm == 0, "NaN/denorm at max"); HX_CHECK(st.peak < 1.4f, "peak %.2f above limiter bound", st.peak); HX_CHECK(fabs(w2 - w1) < 3.0, "level drifting %.1f vs %.1f", w1, w2); HX_CHECK(t2 < t1 - 6.0, "tail does not decay (%.1f -> %.1f)", t1, t2); }
    // also: defaults, white noise, 60 s -> stays put
    { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); std::vector<float> out; NoiseGen ng; ng.amp = 0.3f; run(e, 40.0, 48, false, ng, defCtx(), &out);
      double a = winDb(out, 8 * 48000L, 12 * 48000L), b = winDb(out, 36 * 48000L, 40 * 48000L); printf("  defaults, 0.3 noise: wet %.1f dB (in -15.3) @8-12 s, %.1f dB @36-40 s\n", a, b); HX_CHECK(fabs(a - b) < 2.0, "default level drift"); }

    // 7. wake() on a dirty arena: bit-identical to a fresh instance, no garbage
    { Host d; d.make(hx::fx_create_reverb); hx::FxReverb* e = (hx::FxReverb*)d.fx; e->setParam(MIX, 1.f); e->snap();
      std::vector<float> sink; run(e, 1.5, 48, false, NoiseGen{777, 0.8f}, defCtx(), &sink);                    // leave state everywhere
      size_t off = (sizeof(hx::FxReverb) + 15) & ~(size_t)15; memset(d.mem.data() + off, 0xCD, d.arena.used - off); // simulate dirty SDRAM (buffers hold ~ -4e8 garbage)
      e->wake(); e->setParam(MIX, 1.f); e->snap();
      Host f; f.make(hx::fx_create_reverb); f.fx->setParam(MIX, 1.f); f.fx->snap();
      std::vector<float> a, b; Stats sa = run(e, 4.0, 48, false, ImpulseGen(), defCtx(), &a); run(f.fx, 4.0, 48, false, ImpulseGen(), defCtx(), &b);
      size_t diff = 0; for (size_t i = 0; i < a.size(); i++) if (a[i] != b[i]) diff++;
      printf("  wake() on dirty arena: peak %.3f, %zu samples differ from a fresh instance\n", sa.peak, diff); HX_CHECK(!sa.bad && sa.peak < 2.f && diff == 0, "garbage after wake (peak %.2f diff %zu)", sa.peak, diff); }

    // 8. SEED changes the pattern, gliding without clicks
    { auto a = impulseResp([&](Fx* e) { e->setParam(SEED, 3.f); }, 1.0), b = impulseResp([&](Fx* e) { e->setParam(SEED, 11.f); }, 1.0); double dd = 0, ee = 0; for (size_t i = 0; i < 24000; i++) { dd += (a[i] - b[i]) * (a[i] - b[i]); ee += a[i] * a[i]; }
      HX_CHECK(dd > 0.3 * ee, "SEED does not change the echo pattern"); }

    // 9. CPU + memory
    double ns = hxt::benchNsPerSample(hx::fx_create_reverb, "reverb");
    { Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(MIX, 1.f); e->setParam(DECAY, 0.99f); e->setParam(SIZE, 2.f); e->setParam(SCAT, 1.f); e->setParam(SPREAD, 1.f); e->setParam(MODD, 1.f); e->setParam(DRIFT, 2.f); e->setParam(CHAOS, 1.f); e->snap();
      std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f); double worst = 0, tot = 0; long nb = 20000; std::vector<double> bt;
      for (long b = 0; b < nb + 300; b++) { e->setParam(WARP, 0.5f + 0.5f * sinf(b * 0.02f)); for (int i = 0; i < 48; i++) L[i] = 0.3f * sinf(kTwoPi * 330.f * (float)(b * 48 + i) / SR); R = L; e->smoothBlock();
          auto t0 = std::chrono::steady_clock::now(); e->process(c, L.data(), R.data(), 48, false); double d = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / 48.0; if (b >= 300) bt.push_back(d); }
      std::sort(bt.begin(), bt.end()); for (double d : bt) tot += d; worst = bt[(size_t)(bt.size() * 0.999)];
      printf("CPU worst-case params (WARP sweeping, all max): avg %.2f ns/sample, p99.9 block %.2f ns/sample\n", tot / bt.size(), worst);
      printf("CPU est. device: default %.1f %% | worst-case params avg %.1f %%, p99.9 %.1f %%  (x0.48)\n", ns * 0.48, tot / bt.size() * 0.48, worst * 0.48); }
    { Host h; h.make(hx::fx_create_reverb); printf("memory: %.0f bytes of buffers (%u lines, %u scatter taps, %u allpass) + %zu arena used incl. object\n", ((hx::FxReverb*)h.fx)->memBytes, (unsigned)Rv::N, (unsigned)Rv::NT, (unsigned)Rv::NAP, h.arena.used); }
    return failCount() ? 1 : 0;
}

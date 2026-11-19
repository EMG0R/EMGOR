#include "../fx_blur.h"
#include "fx_testutil.h"
#include <algorithm>
#include <functional>
using namespace hx;

static double rms(const std::vector<float>& x, double from, double to) {
    long a = (long)(from * hxt::SR), b = (long)(to * hxt::SR); double e = 0; for (long i = a; i < b; i++) e += (double)x[i] * x[i]; return sqrt(e / (b - a));
}
static double spread(const std::vector<float>& x, double from, double to) {   // fraction of 10 ms windows above 1e-3 of the loudest
    int w = (int)(0.01 * hxt::SR); long a = (long)(from * hxt::SR), b = (long)(to * hxt::SR); std::vector<double> e; double mx = 0;
    for (long i = a; i + w <= b; i += w) { double s = 0; for (int j = 0; j < w; j++) s += (double)x[i + j] * x[i + j]; e.push_back(s); mx = fmax(mx, s); }
    int c = 0; for (double s : e) if (s > 1e-3 * mx) c++; return e.empty() ? 0 : (double)c / e.size();
}
typedef std::function<float(double)> Sig;
static std::vector<float> go(std::vector<std::pair<const char*, float>> ps, double total, Sig sig) {
    hxt::Host h; h.make(hx::fx_create_blur); Fx* fx = h.fx; fx->setParam(0, 1.f);
    for (auto& p : ps) { int i = fx->findParam(p.first); HX_CHECK(i >= 0, "missing param %s", p.first); fx->setParam(i, p.second); }
    fx->snap(); std::vector<float> L(48), R(48), out; long N = (long)(total * hxt::SR), idx = 0; FxCtx c = hxt::defCtx(440.f);
    while (idx < N) { int m = (int)std::min<long>(48, N - idx); for (int i = 0; i < m; i++) { L[i] = sig((idx + i) / (double)hxt::SR); R[i] = L[i]; }
        fx->smoothBlock(); fx->process(c, L.data(), R.data(), m, false); out.insert(out.end(), L.begin(), L.begin() + m); idx += m; }
    return out;
}
int main() {
    hxt::stdSuite("blur", hx::fx_create_blur, hxt::Opts{});
    { hxt::Host h; h.make(hx::fx_create_blur); Fx* f = h.fx;
      static const char* const nm[] = {"MIX","BLUR","VARTYPE","VARAMT","VARRATE","BRIGHT","LOCUT","HICUT","RNDPHASE","GAIN","FRAME"};
      for (int i = 0; i < 11; i++) HX_CHECK(f->findParam(nm[i]) == i, "param %s at %d", nm[i], f->findParam(nm[i]));
      HX_CHECK(fabsf(f->getParam(1) - 44.9f) < 1e-4f, "BLUR default 44.9"); }
    Sig imp = [](double t) { return (((long)(t * hxt::SR)) % 24000) == 100 ? 1.f : 0.f; };
    // BLUR=0 is transparent (steady sine level preserved, short impulses stay short); BLUR=100 smears
    {
        auto s0 = go({{"BLUR", 0.f}}, 3.0, [](double t) { return (float)(0.5 * sin(6.283185307179586 * 440 * t)); });
        double r = rms(s0, 2.0, 3.0); printf("  blur: BLUR=0 steady sine rms %.3f (input 0.354)\n", r); HX_CHECK(fabs(r - 0.354) < 0.03, "BLUR=0 not transparent");
        auto a = go({{"BLUR", 0.f}}, 5.0, imp), b = go({{"BLUR", 100.f}}, 5.0, imp);
        double sa = spread(a, 1.0, 5.0), sb = spread(b, 1.0, 5.0); printf("  blur: impulse-train energy spread BLUR=0 %.3f, BLUR=100 %.3f\n", sa, sb);
        HX_CHECK(sb > 0.5 && sb > 4 * sa, "impulse train not smeared");
    }
    // time smear: tone stops, output decays slower with more BLUR
    {
        Sig s = [](double t) { return t < 1.0 ? (float)(0.5 * sin(6.283185307179586 * 440 * t)) : 0.f; };
        auto a = go({{"BLUR", 20.f}}, 3.0, s), b = go({{"BLUR", 60.f}}, 3.0, s);
        double ra = rms(a, 1.6, 1.8), rb = rms(b, 1.6, 1.8); printf("  blur: 0.6 s after stop: BLUR 20 -> %.5f, BLUR 60 -> %.5f\n", ra, rb); HX_CHECK(rb > 20 * ra && rb > 0.02, "BLUR amount does not lengthen smear");
    }
    // variance: LFO at 1 Hz with full depth modulates the smear time -> output envelope differs from no-variance
    {
        Sig s = [](double t) { return t < 1.0 ? (float)(0.5 * sin(6.283185307179586 * 440 * t)) : 0.f; };
        auto a = go({{"BLUR", 44.9f}}, 3.0, s), b = go({{"BLUR", 44.9f}, {"VARTYPE", 3.f}, {"VARAMT", 1.f}, {"VARRATE", 1.f}}, 3.0, s);
        double d = 0; for (size_t i = 48000; i < 144000; i++) d += fabs(a[i] - b[i]); printf("  blur: LFO variance changes the output (sum |diff| %.1f)\n", d); HX_CHECK(d > 50.0, "variance has no effect");
        auto c = go({{"BLUR", 44.9f}, {"VARTYPE", 1.f}, {"VARAMT", 1.f}}, 3.0, s), e = go({{"BLUR", 44.9f}, {"VARTYPE", 4.f}, {"VARAMT", 1.f}}, 3.0, s);
        HX_CHECK(rms(c, 1.2, 1.3) != rms(a, 1.2, 1.3) && rms(e, 1.2, 1.3) != rms(a, 1.2, 1.3), "amp/random variance inert");
    }
    // LOCUT / HICUT
    {
        auto a = go({{"LOCUT", 1000.f}}, 2.0, [](double t) { return (float)(0.5 * sin(6.283185307179586 * 440 * t)); }), b = go({{"HICUT", 300.f}}, 2.0, [](double t) { return (float)(0.5 * sin(6.283185307179586 * 440 * t)); });
        printf("  blur: LOCUT 1k on 440 -> %.5f, HICUT 300 on 440 -> %.5f\n", rms(a, 1.0, 2.0), rms(b, 1.0, 2.0)); HX_CHECK(rms(a, 1.0, 2.0) < 0.01 && rms(b, 1.0, 2.0) < 0.01, "cutoffs");
    }
    hxt::benchNsPerSample(hx::fx_create_blur, "blur", false); hxt::benchNsPerSample(hx::fx_create_blur, "blur_st", true);
    {   // worst case: random variance on, wideband input
        hxt::Host h; h.make(hx::fx_create_blur); Fx* fx = h.fx; fx->setParam(fx->findParam("VARTYPE"), 4.f); fx->setParam(fx->findParam("VARAMT"), 1.f); fx->snap();
        std::vector<float> L(48), R(48); FxCtx c = hxt::defCtx(); std::vector<double> ts; long nb = 20000; uint32_t sd = 3;
        for (long b = 0; b < nb; b++) { for (int i = 0; i < 48; i++) L[i] = 0.3f * (rnd01(sd) * 2.f - 1.f); R = L;
            auto t0 = std::chrono::steady_clock::now(); fx->smoothBlock(); fx->process(c, L.data(), R.data(), 48, false);
            ts.push_back(std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count()); }
        ts.erase(ts.begin(), ts.begin() + 100); double tot = 0, mx = 0; for (double t : ts) { tot += t; mx = fmax(mx, t); } std::vector<double> s2 = ts; std::sort(s2.begin(), s2.end());
        printf("  BLOCKTIME blur(random variance, noise) avg %.0f (%.1f ns/sample) p99 %.0f max %.0f ns/block, arena %zu B\n", tot / ts.size(), tot / ts.size() / 48, s2[(size_t)(s2.size() * 0.99)], mx, h.arena.used);
    }
    printf("[blur] %s\n", hxt::failCount() ? "FAIL" : "PASS"); return hxt::failCount() ? 1 : 0;
}

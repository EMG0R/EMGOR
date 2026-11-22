#include "../fx_freeze.h"
#include "fx_testutil.h"
#include <algorithm>
#include <functional>
using namespace hx;

static double tone(const std::vector<float>& x, double from, double to, double hz) {
    long a = (long)(from * hxt::SR), b = (long)(to * hxt::SR); double sr = 0, si = 0;
    for (long i = a; i < b; i++) { double w = 0.5 - 0.5 * cos(6.283185307179586 * (i - a) / (double)(b - a)); double ph = 6.283185307179586 * hz * i / hxt::SR; sr += w * x[i] * cos(ph); si += w * x[i] * sin(ph); }
    return 2.0 * sqrt(sr * sr + si * si) / (0.5 * (b - a));
}
static double rms(const std::vector<float>& x, double from, double to) {
    long a = (long)(from * hxt::SR), b = (long)(to * hxt::SR); double e = 0; for (long i = a; i < b; i++) e += (double)x[i] * x[i]; return sqrt(e / (b - a));
}
typedef std::function<float(double)> Sig;                       // input signal by time
typedef std::function<void(double, Fx*)> Ev;                    // events (called per block)
// MIX=1 run; params by name
static std::vector<float> go(std::vector<std::pair<const char*, float>> ps, double total, Sig sig, Ev ev = nullptr) {
    hxt::Host h; h.make(hx::fx_create_freeze); Fx* fx = h.fx; fx->setParam(0, 1.f);
    fx->setParam(fx->findParam("RNDPHASE"), 0.f); fx->setParam(fx->findParam("FRAME"), 1.f);   // these tests exercise the coherent freeze (the pre-chill defaults); chill is covered by test_fx_chill.cpp
    for (auto& p : ps) { int i = fx->findParam(p.first); HX_CHECK(i >= 0, "missing param %s", p.first); fx->setParam(i, p.second); }
    fx->snap(); std::vector<float> L(48), R(48), out; long N = (long)(total * hxt::SR), idx = 0; FxCtx c = hxt::defCtx(440.f);
    while (idx < N) {
        int m = (int)std::min<long>(48, N - idx); if (ev) ev(idx / (double)hxt::SR, fx);
        for (int i = 0; i < m; i++) { L[i] = sig((idx + i) / (double)hxt::SR); R[i] = L[i]; }
        fx->smoothBlock(); fx->process(c, L.data(), R.data(), m, false); out.insert(out.end(), L.begin(), L.begin() + m); idx += m;
    }
    return out;
}
static Sig burst(double hz, double amp, double t0, double t1) { return [=](double t) { return (t >= t0 && t < t1) ? (float)(amp * sin(6.283185307179586 * hz * t)) : 0.f; }; }

int main() {
    hxt::Opts o; o.expectOutput = true; hxt::stdSuite("freeze", hx::fx_create_freeze, o);
    { hxt::Host h; h.make(hx::fx_create_freeze); Fx* f = h.fx;
      static const char* const nm[] = {"MIX","FREEZE","USETHR","THRESH","ZEROBIN","BRIGHT","LOCUT","HICUT","RNDPHASE","GAIN","FRAME"};
      for (int i = 0; i < 11; i++) HX_CHECK(f->findParam(nm[i]) == i, "param %s at %d", nm[i], f->findParam(nm[i]));
      HX_CHECK(f->getParam(1) == 100.f, "FREEZE default 100"); }
    // 1. freeze a true 440 Hz tone (1.2 s burst), then silence: sustained, stable, at 440, narrowband
    {
        auto y = go({}, 6.0, burst(440, 0.5, 0.0, 1.2));
        double a1 = tone(y, 2.0, 2.5, 440), a3 = tone(y, 5.4, 5.9, 440), off = tone(y, 5.4, 5.9, 700), near = std::max(tone(y, 5.4, 5.9, 400), tone(y, 5.4, 5.9, 480));
        printf("  freeze: 440 amp %.3f -> %.3f, off(700) %.4f, neighbours(400/480) %.4f\n", a1, a3, off, near);
        HX_CHECK(a1 > 0.1 && a3 > 0.1, "frozen 440 missing"); HX_CHECK(std::max(a1, a3) / std::min(a1, a3) < 1.3, "amplitude drifts");
        HX_CHECK(a3 > 10 * off && a3 > 5 * near, "not narrowband / off pitch");
    }
    // 2. FREEZE factor < 100 decays the held bins; 100 does not
    {
        auto a = go({{"FREEZE", 100.f}}, 5.0, burst(440, 0.5, 0, 1.0)), b = go({{"FREEZE", 80.f}}, 5.0, burst(440, 0.5, 0, 1.0));
        double ra = rms(a, 3.5, 4.0), rb = rms(b, 3.5, 4.0); printf("  freeze factor: 100%% -> %.4f, 80%% -> %.5f\n", ra, rb);
        HX_CHECK(ra > 0.1 && rb < 0.1 * ra, "FREEZE factor decay wrong");
    }
    // 3. held until exceeded by a LOUDER peak: 0.2 -> 0.5 raises it, then 0.1 leaves it at 0.5
    {
        Sig s = [](double t) { double a = t < 1.0 ? 0.2 : (t < 2.0 ? 0.5 : (t < 3.0 ? 0.1 : 0.0)); return (float)(a * sin(6.283185307179586 * 440 * t)); };
        auto y = go({}, 5.0, s);
        double h1 = rms(y, 4.0, 4.5), mid = rms(y, 0.5, 0.9);
        printf("  freeze peak-replace: input 0.2 -> %.3f rms, final held (after 0.5 then 0.1) %.3f rms (0.5 tone = 0.354)\n", mid, h1);
        HX_CHECK(h1 > 0.25 && h1 < 0.42, "held level not the loudest peak");
    }
    // 4. threshold: tone at -40 dBFS, THRESH -20, USETHR on -> not frozen (gone after stop); USETHR off -> frozen
    {
        auto a = go({{"USETHR", 1.f}, {"THRESH", -20.f}}, 4.0, burst(440, 0.01, 0, 1.0)), b = go({{"USETHR", 0.f}}, 4.0, burst(440, 0.01, 0, 1.0));
        double ra = rms(a, 3.0, 3.5), rb = rms(b, 3.0, 3.5); printf("  freeze threshold: below-thr tone after stop: USETHR on %.5f, off %.5f\n", ra, rb);
        HX_CHECK(rb > 0.003 && ra < 0.05 * rb, "threshold does not gate freezing");
    }
    // 5. ZEROBIN: unfrozen bins under threshold are zeroed while the (quiet) input is present
    {
        auto a = go({{"USETHR", 1.f}, {"THRESH", -20.f}, {"ZEROBIN", 1.f}}, 2.0, burst(440, 0.01, 0, 2.0)), b = go({{"USETHR", 1.f}, {"THRESH", -20.f}, {"ZEROBIN", 0.f}}, 2.0, burst(440, 0.01, 0, 2.0));
        double ra = rms(a, 1.2, 1.8), rb = rms(b, 1.2, 1.8); printf("  freeze zero-bins: on %.5f, off %.5f\n", ra, rb);
        HX_CHECK(rb > 0.003 && ra < 0.05 * rb, "ZEROBIN does not zero");
    }
    // 6. A_CLEAR drops frozen bins; A_MAIN 0 holds (new louder tone ignored)
    {
        auto y = go({}, 6.0, burst(440, 0.5, 0, 1.0), [](double t, Fx* f) { static bool d = false; if (t < 0.01) d = false; if (t >= 3.0 && !d) { d = true; f->action(A_CLEAR, 1.f); } });
        double before = rms(y, 2.0, 2.5), after = rms(y, 4.0, 4.5); printf("  freeze clear: %.4f -> %.5f\n", before, after);
        HX_CHECK(before > 0.1 && after < 0.02, "A_CLEAR did not clear");
        Sig s = [](double t) { double a = t < 1.0 ? 0.2 : (t > 2.0 && t < 3.0 ? 0.5 : 0.0); return (float)(a * sin(6.283185307179586 * 660 * t)); };
        auto g = go({}, 5.0, s, [](double t, Fx* f) { static bool d = false; if (t < 0.01) d = false; if (t >= 1.5 && !d) { d = true; f->action(A_MAIN, 0.f); } });
        double r = rms(g, 4.0, 4.5); printf("  freeze hold gate: level after closed-gate loud burst %.4f (0.2 tone = 0.141)\n", r);
        HX_CHECK(r > 0.1 && r < 0.2, "A_MAIN=0 did not hold");
    }
    // 7. LOCUT removes 440; RNDPHASE=1 keeps energy but spreads it
    {
        auto a = go({{"LOCUT", 1000.f}}, 3.0, burst(440, 0.5, 0, 3.0)), r = go({{"RNDPHASE", 1.f}}, 5.0, burst(440, 0.5, 0, 1.0));
        printf("  freeze locut: 440 amp %.5f; rndphase rms %.3f, 440 coherence %.3f\n", tone(a, 1.5, 2.5, 440), rms(r, 3.0, 3.5), tone(r, 3.0, 3.5, 440));
        HX_CHECK(tone(a, 1.5, 2.5, 440) < 0.01, "LOCUT"); HX_CHECK(rms(r, 3.0, 3.5) > 0.05, "RNDPHASE killed sound");
    }
    hxt::benchNsPerSample(hx::fx_create_freeze, "freeze", false); hxt::benchNsPerSample(hx::fx_create_freeze, "freeze_st", true);
    printf("[freeze] %s\n", hxt::failCount() ? "FAIL" : "PASS"); return hxt::failCount() ? 1 : 0;
}

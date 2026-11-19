// Chill (random-phase, Paulstretch style) FREEZE and BLUR: fx_freeze.h / fx_blur.h with RNDPHASE = 1 (default), DRIFT, FRAME 4096 (default).
#include "../fx_freeze.h"
#include "../fx_blur.h"
#include "fx_testutil.h"
#include <algorithm>
using namespace hxt;
typedef std::vector<std::pair<const char*, float>> PL;
static std::vector<float> horn(double secs, double t1) {            // 220 Hz harmonic tone (1/h) for t < t1, then silence
    std::vector<float> y((size_t)(secs * SR)); for (size_t i = 0; i < y.size(); i++) { double t = i / (double)SR; if (t >= t1) break; double s = 0; for (int h = 1; h <= 8; h++) s += sin(2 * M_PI * 220.0 * h * t) / h; y[i] = (float)(0.2 * s); } return y; }
static std::vector<float> render(Factory f, PL ps, const std::vector<float>& in) {
    Host h; h.make(f); Fx* e = h.fx; e->setParam(0, 1.f); for (auto& p : ps) { int i = e->findParam(p.first); HX_CHECK(i >= 0, "missing %s", p.first); e->setParam(i, p.second); } e->snap();
    std::vector<float> L(48), R(48), out; size_t pos = 0; FxCtx c = defCtx(220.f); while (pos + 48 <= in.size()) { for (int i = 0; i < 48; i++) L[i] = in[pos + i]; R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); out.insert(out.end(), L.begin(), L.end()); pos += 48; }
    return out; }
static double rmsW(const std::vector<float>& x, double a, double b) { size_t i0 = (size_t)(a * SR), i1 = (size_t)(b * SR); double e = 0; for (size_t i = i0; i < i1 && i < x.size(); i++) e += (double)x[i] * x[i]; return sqrt(e / (i1 - i0)); }
static double maxStep(const std::vector<float>& x, double a, double b) { double m = 0; for (size_t i = (size_t)(a * SR) + 1; i < (size_t)(b * SR) && i < x.size(); i++) m = fmax(m, fabs(x[i] - x[i - 1])); return m; }
// level fluctuation: std/mean of 50 ms window rms over [a,b]
static double fluct(const std::vector<float>& x, double a, double b, double* mn = nullptr, double* mx = nullptr) { std::vector<double> r; for (double t = a; t + 0.05 <= b; t += 0.05) r.push_back(rmsW(x, t, t + 0.05)); double m = 0, s = 0; for (double v : r) m += v; m /= r.size(); for (double v : r) s += (v - m) * (v - m); if (mn) *mn = *std::min_element(r.begin(), r.end()); if (mx) *mx = *std::max_element(r.begin(), r.end()); return sqrt(s / r.size()) / m; }
static double corr(const std::vector<float>& a, const std::vector<float>& b, double t0, double t1) { double ab = 0, aa = 0, bb = 0; for (size_t i = (size_t)(t0 * SR); i < (size_t)(t1 * SR); i++) { ab += (double)a[i] * b[i]; aa += (double)a[i] * a[i]; bb += (double)b[i] * b[i]; } return ab / sqrt(aa * bb + 1e-30); }
static double tonePow(const std::vector<float>& x, double a, double b, double hz) { size_t i0 = (size_t)(a * SR), i1 = (size_t)(b * SR); double sr = 0, si = 0; for (size_t i = i0; i < i1; i++) { double w = 0.5 - 0.5 * cos(2 * M_PI * (i - i0) / (double)(i1 - i0)); double ph = 2 * M_PI * hz * i / SR; sr += w * x[i] * cos(ph); si += w * x[i] * sin(ph); } return (sr * sr + si * si); }

int main() {
    stdSuite("freeze-chill", hx::fx_create_freeze, Opts{}); stdSuite("blur-chill", hx::fx_create_blur, Opts{});
    { Host h; h.make(hx::fx_create_freeze); HX_CHECK(h.fx->v[h.fx->findParam("RNDPHASE")] == 1.f, "freeze RNDPHASE default 1"); HX_CHECK(h.fx->findParam("DRIFT") >= 0, "freeze DRIFT"); HX_CHECK(h.fx->v[h.fx->findParam("FRAME")] == 3.f, "freeze FRAME default 3 (longest)"); }
    { Host h; h.make(hx::fx_create_blur); HX_CHECK(h.fx->v[h.fx->findParam("RNDPHASE")] == 1.f, "blur RNDPHASE default 1"); HX_CHECK(h.fx->findParam("DRIFT") >= 0, "blur DRIFT"); HX_CHECK(h.fx->v[h.fx->findParam("FRAME")] == 3.f, "blur FRAME default 3"); }
    auto in = horn(8.0, 1.0);
    // ---- FREEZE: hold a 1 s tone for 7 s
    auto fc = render(hx::fx_create_freeze, {}, in), f0 = render(hx::fx_create_freeze, {{"RNDPHASE", 0.f}, {"FRAME", 1.f}}, in);
    { double mn, mx, fl = fluct(fc, 3.0, 8.0, &mn, &mx); double rc = rmsW(fc, 3, 8), r0 = rmsW(f0, 3, 8); double c = corr(fc, f0, 3, 8);
      printf("  FREEZE chill: held level %.4f rms (coherent freeze %.4f, %+.1f dB), 50 ms-window range %.4f..%.4f, fluctuation %.2f, corr with coherent %.2f\n", rc, r0, 20 * log10(rc / r0), mn, mx, fl, c);
      HX_CHECK(rc > 0.03 && fabs(20 * log10(rc / r0)) < 4.5, "freeze chill level"); HX_CHECK(mn > 0.3 * mx, "freeze chill level not steady (%.4f..%.4f)", mn, mx); HX_CHECK(fabs(c) < 0.4, "phases not randomised (corr %.2f)", c);
      double inb = 0, tot = 0; for (int h = 1; h <= 8; h++) inb += tonePow(fc, 3, 8, 220.0 * h) + tonePow(fc, 3, 8, 220.0 * h + 6) + tonePow(fc, 3, 8, 220.0 * h - 6); for (double f = 100; f < 2400; f += 12) tot += tonePow(fc, 3, 8, f);
      printf("  FREEZE chill spectral energy kept near the 8 partials (+-6 Hz probes): %.0f%% of 100..2400 Hz\n", 100 * inb / tot); HX_CHECK(inb / tot > 0.5, "partials lost"); }
    // DRIFT effect: static shimmer (0) vs slow (0.12) vs full random (1): all steady and bounded
    for (float d : {0.f, 0.12f, 1.f}) { auto o = render(hx::fx_create_freeze, {{"DRIFT", d}}, in); double mn, mx, fl = fluct(o, 3.0, 8.0, &mn, &mx); printf("  FREEZE DRIFT %.2f: fluctuation %.2f (range %.4f..%.4f), peak %.3f\n", d, fl, mn, mx, maxStep(o, 3, 8)); HX_CHECK(rmsW(o, 3, 8) > 0.02 && mn > 0.2 * mx, "DRIFT %.2f unsteady", d); }
    // clicks: freeze engaged while the tone plays (input continuous 8 s): output step bound
    { auto cont = horn(8.0, 99.0); auto o = render(hx::fx_create_freeze, {{"MIX", 1.f}}, cont); double ms = maxStep(o, 2.0, 8.0), is = maxStep(cont, 2.0, 8.0); printf("  FREEZE clicks: max step %.4f vs input's %.4f\n", ms, is); HX_CHECK(ms < 1.6 * is, "freeze click: %.4f vs %.4f", ms, is); }
    // ---- BLUR: tone burst smeared into a long pad; BLUR=0 untouched
    auto bi = horn(10.0, 1.0);
    auto bc = render(hx::fx_create_blur, {{"BLUR", 80.f}}, bi), b0 = render(hx::fx_create_blur, {{"BLUR", 80.f}, {"RNDPHASE", 0.f}, {"FRAME", 1.f}}, bi);
    { double mn, mx, fl = fluct(bc, 2.0, 5.0, &mn, &mx); printf("  BLUR chill: tail rms 2..5 s %.4f (coherent %.4f), range %.4f..%.4f, fluctuation %.2f, corr with coherent %.2f\n", rmsW(bc, 2, 5), rmsW(b0, 2, 5), mn, mx, fl, corr(bc, b0, 1.5, 5));
      HX_CHECK(rmsW(bc, 1.5, 3) > 0.01, "blur chill tail missing"); HX_CHECK(fabs(corr(bc, b0, 1.5, 5)) < 0.4, "blur phases not randomised"); }
    { auto o = render(hx::fx_create_blur, {{"BLUR", 0.f}, {"MIX", 1.f}}, horn(4.0, 99.0)); auto ref = horn(4.0, 99.0); double r = rmsW(o, 1, 3), ri = rmsW(ref, 1, 3); printf("  BLUR=0 with RNDPHASE 1: rms %.4f vs input %.4f\n", r, ri); HX_CHECK(fabs(20 * log10(r / ri)) < 1.0, "BLUR=0 not transparent"); }
    { auto cont = horn(8.0, 99.0); auto o = render(hx::fx_create_blur, {{"BLUR", 70.f}, {"MIX", 1.f}}, cont); double ms = maxStep(o, 2.0, 8.0), is = maxStep(cont, 2.0, 8.0); printf("  BLUR clicks: max step %.4f vs input's %.4f\n", ms, is); HX_CHECK(ms < 1.6 * is, "blur click"); }
    // ---- CPU: chill defaults vs the previous configuration (RNDPHASE 0, FRAME 1)
    auto bench = [&](Factory f, PL ps, const char* nm) { Host h; h.make(f); Fx* e = h.fx; e->setParam(0, 1.f); for (auto& p : ps) e->setParam(e->findParam(p.first), p.second); e->snap(); std::vector<float> L(48), R(48); FxCtx c = defCtx(220.f);
        long nb = 4 * 48000 / 48; std::vector<float> inp((size_t)nb * 48); for (long i = 0; i < nb * 48; i++) { double t = i / (double)SR, s = 0; for (int k = 1; k <= 8; k++) s += sin(2 * M_PI * 220.0 * k * t) / k; inp[i] = (float)(0.2 * s); }
        for (long b = 0; b < 600; b++) { memcpy(L.data(), &inp[b * 48], 192); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); }
        double best = 1e30, bw = 1e30; float sink = 0; for (int pass = 0; pass < 5; pass++) { double tot = 0, pw = 0; for (long b = 0; b < nb; b++) { memcpy(L.data(), &inp[b * 48], 192); R = L; auto t0 = std::chrono::steady_clock::now(); e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); tot += ns; if (ns > pw) pw = ns; sink += L[0]; } best = fmin(best, tot / (nb * 48.0)); bw = fmin(bw, pw / 48.0); }
        printf("CPU %-22s avg %.2f ns/sample, worst block %.2f ns/sample (device est x0.48: %.2f%% / %.2f%%) sink %.2f\n", nm, best, bw, best * 0.48, bw * 0.48, sink); return best; };
    double a1 = bench(hx::fx_create_freeze, {}, "freeze chill"), a0 = bench(hx::fx_create_freeze, {{"RNDPHASE", 0.f}, {"FRAME", 1.f}}, "freeze previous");
    double b1 = bench(hx::fx_create_blur, {}, "blur chill"), b0c = bench(hx::fx_create_blur, {{"RNDPHASE", 0.f}, {"FRAME", 1.f}}, "blur previous");
    HX_CHECK(a1 < 1.5 * a0 + 1.0 && b1 < 1.5 * b0c + 1.0, "chill CPU regression: freeze %.2f vs %.2f, blur %.2f vs %.2f", a1, a0, b1, b0c);
    printf(failCount() ? "[chill] FAIL\n" : "[chill] ALL PASS\n"); return failCount() ? 1 : 0;
}

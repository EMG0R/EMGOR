#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/HYPER_CORE/src/dsp/fx/fx_modblock.h"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/HYPER_CORE/src/dsp/fx/tests/fx_testutil.h"
#include <complex>
#include <algorithm>
using namespace hxt; typedef std::complex<double> C;
static void fft(std::vector<C>& a) { size_t n = a.size(); for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
  for (size_t len = 2; len <= n; len <<= 1) { double ang = -2 * M_PI / len; C wl(cos(ang), sin(ang)); for (size_t i = 0; i < n; i += len) { C w(1); for (size_t j = 0; j < len / 2; j++) { C u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } } } }
struct Res { double thdn, harm, spur; };
// spectrum of N=2^18 (5.46 s) after 2 s settle; band = +-bw around f0 (AM/PM sidebands of the fundamental), harmonics 2..9 +-bw, "spur" = everything else (noise, zipper, aliasing)
static Res analyse(const std::vector<float>& x, double f0, double bw) {
    const size_t N = 1 << 18, a = 96000; std::vector<C> v(N); double wsum = 0; for (size_t i = 0; i < N; i++) { double w = 0.5 - 0.5 * cos(2 * M_PI * i / N); v[i] = x[a + i] * w; wsum += w; } fft(v);
    double df = SR / N, band = 0, harm = 0, rest = 0; for (size_t k = 1; k < N / 2; k++) { double f = k * df, p = std::norm(v[k]); bool inb = fabs(f - f0) <= bw; bool inh = false; for (int h = 2; h <= 9; h++) if (fabs(f - h * f0) <= bw) inh = true;
        if (inb) band += p; else if (inh) harm += p; else rest += p; }
    if (getenv("DBG")) { std::vector<std::pair<double,double>> t; for (size_t k = 1; k < N / 2; k++) { double f = k * df; if (fabs(f - f0) <= 60) continue; t.push_back({std::norm(v[k]), f}); } std::sort(t.rbegin(), t.rend()); for (int i = 0; i < 8; i++) printf("    spur %.1f Hz  %.1f dB\n", t[i].second, 10 * log10(t[i].first / band)); }
    return {sqrt((harm + rest) / band), sqrt(harm / band), sqrt(rest / band)};
}
int main(int argc, char** argv) {
    int variant = argc > 1 ? atoi(argv[1]) : 0;
    for (double f0 : {100.0, 500.0, 1000.0, 2000.0}) for (double db : {-30.0, -20.0, -10.0}) {
        Host h; h.make(hx::fx_create_modblock); hx::FxModBlock* m = (hx::FxModBlock*)h.fx; m->setParam(0, 1.f);
        if (variant == 1) m->ch->satFb = false; if (variant == 3) m->setParam(3, 0.f); if (variant == 5) m->setParam(3, 0.f), m->setParam(5, 0.f); m->snap();
        std::vector<float> oL, oR; auto gen = [&](float* b, int mm, long i0) { for (int i = 0; i < mm; i++) b[i] = (float)(pow(10, db / 20) * sin(2 * M_PI * f0 * (double)(i0 + i) / SR)); }; run(m, 9.0, 48, false, gen, defCtx(), &oL, &oR);
        Res r = analyse(oL, f0, 6.0); printf("f0 %5.0f %4.0f dBFS: THD+N %7.1f dB  (harm %7.1f, other %7.1f)\n", f0, db, 20 * log10(r.thdn), 20 * log10(r.harm + 1e-12), 20 * log10(r.spur));
    }
}

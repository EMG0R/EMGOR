#include "../src/dsp/fx/fx_drone.h"
#include "../src/dsp/fx/tests/fx_testutil.h"
using namespace hx;
int main() {
    hxt::Host h; h.make(hx::fx_create_drone); Fx* fx = h.fx;
    printf("np=%d\n", fx->np);
    auto gen = [](float* b, int m, long i0) { for (int i = 0; i < m; i++) { double t = (i0 + i) / (double)hxt::SR; b[i] = t < 3.0 ? 0.3f * sinf(6.2831853f * 440.f * (float)t) : 0.f; } };
    fx->setParam(0, 0.f); fx->snap();
    std::vector<float> oL, oR; auto s = hxt::run(fx, 1.0, 48, false, gen, hxt::defCtx(), &oL, &oR);
    double d = 0; for (long i = 0; i < (long)oL.size(); i++) d = fmax(d, fabs(oL[i] - 0.3f * sinf(6.2831853f * 440.f * (float)(i / 48000.0)))); printf("mix0 max dry diff %.2e bad=%d\n", d, (int)s.bad);
    fx->setParam(0, 1.f); fx->setParam(fx->findParam("GATE"), -60.f); fx->setParam(fx->findParam("COMBLVL"), 0.f); fx->snap();
    auto s2 = hxt::run(fx, 12.0, 48, false, gen, hxt::defCtx(), &oL, &oR);
    auto rms = [&](double a, double b) { double e = 0; long i0 = a * 48000, i1 = b * 48000; for (long i = i0; i < i1; i++) e += oL[i] * oL[i]; return sqrt(e / (i1 - i0)); };
    printf("peak %.3f bad=%d denorm=%ld rms 1-2s %.4f 4-5s %.4f 8-9s %.4f 11-12s %.4f\n", s2.peak, (int)s2.bad, s2.denorm, rms(1,2), rms(4,5), rms(8,9), rms(11,12));
    return 0;
}

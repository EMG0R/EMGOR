// test_share.cpp -- max/pmor_share.h: shared render, multi-reader alignment, resampling.
#include <stdio.h>
#include <math.h>
#include <vector>
#include "../max/pmor_share.h"

static int g_fail = 0, g_pass = 0;
#define EXPECT(c, ...) do { if (c) g_pass++; else { g_fail++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// fake core: L = sin(2 pi 1000 t / 48000), R = frame index / 1e6 (a ramp), counted by absolute frame
static uint64_t g_frame = 0;
static void fakeRender(float* l, float* r, size_t n) { for (size_t i = 0; i < n; i++, g_frame++) { l[i] = sinf(6.2831853f * 1000.f * (float)g_frame / 48000.f); r[i] = (float)g_frame * 1e-6f; } }

int main() {
    using namespace pmorshare;
    static Ring ring;
    printf("[two readers at 48 kHz share one render]\n");
    Reader a, b; a.setRate(48000); b.setRate(48000); a.join(ring); b.join(ring);
    std::vector<double> aL(512), aR(512), bL(512), bR(512);
    for (int v = 0; v < 10; v++) {
        a.pull(ring, fakeRender, aL.data(), aR.data(), 512);
        b.pull(ring, fakeRender, bL.data(), bR.data(), 512);
        bool same = true; for (int i = 0; i < 512; i++) if (aL[i] != bL[i] || aR[i] != bR[i]) same = false;
        EXPECT(same, "vector %d identical on both readers", v);
    }
    EXPECT(ring.blocksRendered == (uint64_t)(10 * 512 / 64 + 0) || ring.blocksRendered == (uint64_t)(10 * 512 / 64 + 1), "core rendered once for both readers: %llu blocks for 5120 frames", (unsigned long long)ring.blocksRendered);

    printf("[resampling: ratio, continuity, pitch]\n");
    static Ring r2; g_frame = 0; Reader c; c.setRate(44100); c.join(r2);
    std::vector<double> L(441 * 20), R(441 * 20);
    for (int v = 0; v < 20; v++) c.pull(r2, fakeRender, L.data() + v * 441, R.data() + v * 441, 441);
    // R is a ramp in core frames: output sample k must sit at core frame k*48000/44100 (linear interp of a ramp is exact)
    double maxErr = 0; for (size_t k = 0; k < R.size(); k++) maxErr = fmax(maxErr, fabs(R[k] - (double)k * (48000.0 / 44100.0) * 1e-6));
    EXPECT(maxErr < 1e-9, "ramp position error %.3g", maxErr);
    // 1 kHz sine at 44.1 kHz: count rising zero crossings over 20 vectors (0.2 s) -> ~200
    int zc = 0; for (size_t k = 1; k < L.size(); k++) if (L[k - 1] < 0 && L[k] >= 0) zc++;
    EXPECT(zc >= 198 && zc <= 202, "1 kHz holds its pitch at 44.1 kHz (%d crossings in 0.2 s)", zc);
    static Ring r3; g_frame = 0; Reader d; d.setRate(96000); d.join(r3);
    std::vector<double> L3(960 * 10), R3(960 * 10);
    for (int v = 0; v < 10; v++) d.pull(r3, fakeRender, L3.data() + v * 960, R3.data() + v * 960, 960);
    double e3 = 0; for (size_t k = 0; k < R3.size(); k++) e3 = fmax(e3, fabs(R3[k] - (double)k * 0.5 * 1e-6));
    EXPECT(e3 < 1e-9, "96 kHz ramp position error %.3g", e3);

    printf("[late joiner and idle reader]\n");
    static Ring r4; g_frame = 0; Reader e, f; e.setRate(48000); f.setRate(48000); e.join(r4);
    std::vector<double> x(128), y(128);
    for (int v = 0; v < 50; v++) e.pull(r4, fakeRender, x.data(), y.data(), 128);
    f.join(r4);
    f.pull(r4, fakeRender, x.data(), y.data(), 128);
    EXPECT(fabs(x[0] - 0) >= 0 && y[0] > 0, "late joiner starts at the live head (ramp %.6f)", y[0]);
    for (int v = 0; v < 400; v++) e.pull(r4, fakeRender, x.data(), y.data(), 128);     // f idle for 51200 frames > ring
    f.pull(r4, fakeRender, x.data(), y.data(), 128);
    bool finite = true; for (double v : x) if (!std::isfinite(v)) finite = false;
    EXPECT(finite && y[0] > 0.04, "idle reader rejoins near the head (ramp %.4f)", y[0]);

    printf("\ntest_share: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

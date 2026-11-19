// test_fx_pitchtrack.cpp -- PitchTrack accuracy / robustness (no stdSuite: it is not an Fx).
#include "../fx_pitchtrack.h"
#include "fx_testutil.h"
using namespace hx; using namespace hxt;

static float horn(float hz, long i, int nh = 8) {
    float t = (float)i / 48000.f, y = 0; for (int h = 1; h <= nh; h++) y += sinf(6.2831853f * hz * h * t) / (float)h; return 0.35f * y; }

int main() {
    PitchTrack pt; pt.init(48000.f);
    // 1. steady horn tones + sine, block 48 and 17 (arbitrary n)
    for (int pass = 0; pass < 2; pass++) {
        int bs = pass ? 17 : 48; int tot = 0, good = 0;
        for (float hz = 165.f; hz <= 1050.f; hz *= 1.0595f) {
            pt.init(48000.f); float buf[128]; long idx = 0; int tb = 0, gb = 0;
            for (int b = 0; b < 48000 / bs; b++) {
                for (int i = 0; i < bs; i++, idx++) buf[i] = (pass == 0) ? horn(hz, idx) : 0.4f * sinf(6.2831853f * hz * (float)idx / 48000.f);
                pt.process(buf, bs);
                if (idx > 4800) { tb++; if (pt.f0 > 0 && fabsf(pt.f0 / hz - 1.f) < 0.01f) gb++; }
            }
            tot += tb; good += gb;
            if (gb < tb * 0.95) printf("  note %.1f Hz: %d/%d ok (last f0 %.2f conf %.2f)\n", hz, gb, tb, pt.f0, pt.conf);
        }
        float frac = (float)good / (float)tot;
        printf("steady %s bs=%d: %.2f%% within 1%%\n", pass ? "sine" : "horn", bs, 100 * frac);
        HX_CHECK(frac > 0.95f, "steady accuracy %.3f", frac);
    }
    // 2. slow horn sweep 165 -> 1000 Hz over 3 s, compare to true f at t-10 ms
    {
        pt.init(48000.f); float ph = 0; int tot = 0, good = 0; float buf[48]; long idx = 0; const int N = 3 * 48000;
        std::vector<float> truth(N);
        for (int b = 0; b < N / 48; b++) {
            for (int i = 0; i < 48; i++, idx++) {
                float f = 165.f * powf(1000.f / 165.f, (float)idx / N); truth[idx] = f; ph += f / 48000.f; if (ph >= 1) ph -= 1;
                float y = 0; for (int h = 1; h <= 8; h++) y += sinf(6.2831853f * h * ph) / h; buf[i] = 0.35f * y;
            }
            pt.process(buf, 48);
            if (idx > 4800 && pt.f0 > 0) { tot++; if (fabsf(pt.f0 / truth[idx - 480] - 1.f) < 0.02f) good++; }
            else if (idx > 4800) tot++;
        }
        printf("sweep: %.2f%% within 2%%\n", 100.f * good / tot);
        HX_CHECK(good > 0.95 * tot, "sweep %d/%d", good, tot);
    }
    // 3. octave-jump rarity on a held note with vibrato + noise
    {
        pt.init(48000.f); float buf[48]; long idx = 0; float ph = 0; uint32_t s = 99; int jumps = 0, blocks = 0;
        for (int b = 0; b < 4 * 1000; b++) {
            for (int i = 0; i < 48; i++, idx++) {
                float f = 440.f * (1.f + 0.02f * sinf(6.2831853f * 5.f * (float)idx / 48000.f)); ph += f / 48000.f; if (ph >= 1) ph -= 1;
                float y = 0; for (int h = 1; h <= 6; h++) y += sinf(6.2831853f * h * ph) / h; buf[i] = 0.3f * y + 0.02f * (rnd01(s) * 2 - 1);
            }
            pt.process(buf, 48);
            if (idx > 9600) { blocks++; if (pt.f0 > 0 && fabsf(pt.f0 / 440.f - 1.f) > 0.15f) jumps++; }
        }
        printf("vibrato: %d/%d blocks off by >15%%\n", jumps, blocks);
        HX_CHECK(jumps < blocks / 100, "octave/jump rate");
    }
    // 4. noise / silence -> unvoiced or low conf
    {
        pt.init(48000.f); NoiseGen ng; ng.amp = 0.4f; float buf[48]; int voiced = 0, tot = 0;
        for (int b = 0; b < 2000; b++) { ng(buf, 48, 0); pt.process(buf, 48); if (b > 50) { tot++; if (pt.f0 > 0 && pt.conf > 0.5f) voiced++; } }
        printf("noise: %d/%d blocks voiced&conf>0.5\n", voiced, tot);
        HX_CHECK(voiced < tot / 20, "noise falsely voiced");
        for (int b = 0; b < 200; b++) { SilenceGen()(buf, 48, 0); pt.process(buf, 48); }
        HX_CHECK(pt.f0 == 0.f, "silence f0 = %f", pt.f0);
    }
    // 5. bench ns/sample
    {
        pt.init(48000.f); float buf[48]; long idx = 0; float sink = 0; static float pre[4800]; for (int i = 0; i < 4800; i++) pre[i] = horn(330.f, i);
        auto t0 = std::chrono::steady_clock::now(); int nb = 48000 * 4 / 48;
        for (int b = 0; b < nb; b++) { for (int i = 0; i < 48; i++, idx++) buf[i] = pre[idx % 4800]; pt.process(buf, 48); sink += pt.f0; }
        double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48);
        printf("BENCH pitchtrack %.2f ns/sample (incl. horn gen; sink %.1f)\n", ns, sink);
    }
    printf("[pitchtrack] %s (%d failures)\n", hxt::failCount() ? "FAIL" : "PASS", hxt::failCount());
    return hxt::failCount() ? 1 : 0;
}

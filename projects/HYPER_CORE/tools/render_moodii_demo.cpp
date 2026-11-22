// Renders moodii_demo.wav: dry horn 3 s, then MOOD II REVERB, DELAY, SLIP (GEN .6), loop ENV, loop TAPE (reverse), loop STRETCH, 3 s each (mono, 16-bit 48 kHz). Build:
//   c++ -O2 -std=c++17 render_moodii_demo.cpp -o /tmp/render_md && /tmp/render_md <out_dir>
#include "../src/dsp/fx/fx_moodii.h"
#include "../src/dsp/fx/tests/fx_testutil.h"
using namespace hxt;
static std::vector<float> horn(double secs, float peakDb) {   // trumpet-ish phrase: tongued notes, vibrato, brass-like harmonic series
    static const float notes[] = {293.66f, 369.99f, 440.f, 587.33f, 554.37f, 440.f, 369.99f, 329.63f};
    std::vector<float> y((size_t)(secs * SR)); double ph[16] = {0}; uint32_t sd = 9; float lp = 0;
    for (size_t i = 0; i < y.size(); i++) { double t = i / (double)SR; int ni = (int)(t / 0.5) % 8; double tn = fmod(t, 0.5);
        float env = tn < 0.012f ? tn / 0.012f : (tn < 0.4f ? 1.f - 0.25f * (float)(tn - 0.012) / 0.4f : expf(-(float)(tn - 0.4) * 30.f)); float vib = 1.f + 0.008f * sinf(6.f * 6.2831853f * (float)t);
        float f = notes[ni] * vib, s = 0; for (int h = 1; h <= 14; h++) { ph[h] += 6.2831853 * f * h / SR; s += sinf((float)ph[h]) * (1.f / h) * (h < 6 ? 1.f : 0.8f); }
        lp += ((rnd01(sd) * 2 - 1) - lp) * 0.2f; y[i] = (s * 0.3f + lp * 0.01f) * env; }
    float pk = 0; for (float x : y) pk = fmaxf(pk, fabsf(x)); float g = powf(10.f, peakDb / 20.f) / pk; for (auto& x : y) x *= g; return y;
}
static void wav(const char* path, const std::vector<float>& x) {
    FILE* f = fopen(path, "wb"); uint32_t n = (uint32_t)x.size() * 2, sr = 48000, br = sr * 2, d = 36 + n; uint16_t one = 1, ch = 1, ba = 2, bits = 16; uint32_t fmt = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&d, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); for (float v : x) { int s = (int)lrintf(fmaxf(-1.f, fminf(1.f, v)) * 32767.f); int16_t q = (int16_t)s; fwrite(&q, 2, 1, f); } fclose(f); }

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "."; auto in = horn(4.6 + 3.0, -12.f);
    std::vector<float> out;
    struct Sec { const char* name; float wm, lm; float mix, time, mod, clock, gen, loop, llen, lmod; float route; bool tap; };
    // 3 wet modes, then 3 loop modes (loop heard through a short, dry-feedback delay so only the loop channel colours it)
    const Sec secs[] = {
        { "REVERB",          0, 1, 0.55f, 0.60f, 0.80f, 0.5f,  0.f, 0.f, 1.f, 0.7143f, 0, false },
        { "DELAY",           1, 1, 0.50f, 0.45f, 0.55f, 0.5f,  0.f, 0.f, 1.f, 0.7143f, 0, false },
        { "SLIP (GEN .6)",   2, 1, 0.70f, 0.30f, 0.50f, 0.5f,  0.6f, 0.f, 1.f, 0.7143f, 0, false },
        { "LOOP ENV",        1, 0, 0.60f, 0.00f, 0.00f, 0.5f,  0.f, 1.f, 0.35f, 0.62f, 0, true },
        { "LOOP TAPE (rev)", 1, 1, 0.60f, 0.00f, 0.00f, 0.5f,  0.f, 1.f, 1.f,  0.2857f, 0, true },
        { "LOOP STRETCH",    1, 2, 0.60f, 0.00f, 0.00f, 0.5f,  0.f, 1.f, 0.55f, 0.62f, 0, true },
    };
    // dry reference first (3 s)
    for (size_t i = 0; i < (size_t)(3.0 * SR); i++) out.push_back(in[i]);
    for (const Sec& s : secs) {
        Host h; h.make(hx::fx_create_moodii); Fx* e = h.fx;
        e->setParam(0, s.mix); e->setParam(1, s.wm); e->setParam(2, s.lm); e->setParam(3, s.clock); e->setParam(4, s.time); e->setParam(5, s.mod); e->setParam(6, s.loop);
        e->setParam(7, s.llen); e->setParam(8, s.lmod); e->setParam(9, s.gen); e->setParam(10, s.route); e->snap();
        const size_t pre = s.tap ? (size_t)(1.6 * SR) : 0, n = (size_t)(3.0 * SR); size_t pos = s.tap ? (size_t)(0.0 * SR) : 0; std::vector<float> L(48), R(48); FxCtx c = defCtx();
        // pre-roll (not recorded) lets the looper listen; tap lands at its end
        size_t total = pre + n; size_t tapAt = pre;
        for (size_t b = 0; b * 48 < total; b++) {
            if (b * 48 == (tapAt / 48) * 48 && s.tap) e->action(A_MAIN, 1);
            for (int i = 0; i < 48; i++) L[i] = in[(pos + b * 48 + i) % in.size()]; R = L;
            e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false);
            if (b * 48 >= pre) for (int i = 0; i < 48; i++) out.push_back(L[i]);
        }
        printf("  section %-16s peak ", s.name); float pk = 0; for (size_t i = out.size() - n; i < out.size(); i++) pk = fmaxf(pk, fabsf(out[i])); printf("%.3f\n", pk);
    }
    float pk = 0; for (float x : out) pk = fmaxf(pk, fabsf(x)); printf("  total %.1f s, peak %.3f\n", out.size() / (double)SR, pk);
    if (pk > 0.98f) { float g = 0.98f / pk; for (auto& x : out) x *= g; }
    wav((dir + "/moodii_demo.wav").c_str(), out); return 0; }

// Renders freeze_blur_chill_demo.wav: dry phrase (3 s), then FREEZE chill (phrase 3 s, held 5 s), then BLUR chill (phrase 3 s, smeared 5 s). Mono 16-bit 48 kHz, wet only (MIX 1). Build:
//   c++ -O2 -std=c++17 render_freeze_blur_chill_demo.cpp -o /tmp/render_chill && /tmp/render_chill <out_dir>
#include "../src/dsp/fx/fx_freeze.h"
#include "../src/dsp/fx/fx_blur.h"
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

static std::vector<float> render(Factory fac, std::vector<std::pair<int, float>> ps, const std::vector<float>& in) {
    Host h; h.make(fac); for (auto& p : ps) h.fx->setParam(p.first, p.second); h.fx->snap(); std::vector<float> out; size_t pos = 0;
    auto g = [&](float* b, int m, long) { for (int i = 0; i < m; i++) b[i] = in[pos++]; }; run(h.fx, in.size() / (double)SR, 48, false, g, defCtx(), &out); return out; }
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "."; auto ph = horn(3.0, -12.f); std::vector<float> in8 = ph; in8.resize((size_t)(8 * SR), 0.f); std::vector<float> out = ph;
    auto fr = render(hx::fx_create_freeze, {{0, 1.f}}, in8); out.insert(out.end(), fr.begin(), fr.end());          // defaults: RNDPHASE 1, DRIFT 0.12, FRAME 4096
    auto bl = render(hx::fx_create_blur, {{0, 1.f}, {1, 85.f}}, in8); out.insert(out.end(), bl.begin(), bl.end());   // BLUR 85 %
    float pk = 0; for (float x : out) pk = fmaxf(pk, fabsf(x)); float g = 0.8f / fmaxf(pk, 1e-6f); for (auto& x : out) x *= g; printf("peak gain %.1f dB\n", 20 * log10(g));
    wav((dir + "/freeze_blur_chill_demo.wav").c_str(), out); return 0; }

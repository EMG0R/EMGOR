// Renders trace_demo.wav and autowah_demo.wav (dry 4 s, then wet 4 s; mono 16-bit 48 kHz). Build:
//   c++ -O2 -std=c++17 render_trace_autowah_demo.cpp -o /tmp/render_demo && /tmp/render_demo <out_dir>
#include "../src/dsp/fx/fx_trace.h"
#include "../src/dsp/fx/fx_wah.h"
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
static std::vector<float> fx(Factory fac, std::vector<std::pair<int, float>> ps, const std::vector<float>& in) {
    Host h; h.make(fac); for (auto& p : ps) h.fx->setParam(p.first, p.second); h.fx->snap(); std::vector<float> out; size_t pos = 0;
    auto g = [&](float* b, int m, long) { for (int i = 0; i < m; i++) b[i] = in[pos++]; }; run(h.fx, in.size() / (double)SR, 48, false, g, defCtx(), &out); return out; }
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "."; std::vector<float> out;
    { // TRACE: -12 dBFS horn, KEEP LOUD 6 bins, MIX 1 (wet only), then dry
      auto in = horn(4.0, -12.f); auto w = fx(hx::fx_create_trace, {{0, 1.f}, {1, 0}, {2, 6.f}, {10, 1.f}, {9, 2.f}}, in); out = in; out.insert(out.end(), w.begin(), w.end()); wav((dir + "/trace_demo.wav").c_str(), out); }
    { // AUTO-WAH: input played at -45 dBFS peak (quiet dynamic mic); dry and wet share one make-up gain (+30 dB) so the level the wah sees is the real one
      auto in = horn(4.0, -45.f); auto w = fx(hx::fx_create_wah, {{0, 1.f}}, in); float pk = 0; for (float x : w) pk = fmaxf(pk, fabsf(x)); float gain = 0.7f / fmaxf(pk, 1e-6f); out.clear();
      for (float x : in) out.push_back(x * gain); for (float x : w) out.push_back(x * gain); wav((dir + "/autowah_demo.wav").c_str(), out); printf("autowah demo gain %.1f dB\n", 20 * log10(gain)); }
    return 0; }

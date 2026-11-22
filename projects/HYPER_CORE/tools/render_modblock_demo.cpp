// Renders modblock_demo.wav: dry, CHORUS, PHASER, FLANGER, 3 s each (mono = L+R sum, the way the output is summed later; 16-bit 48 kHz). Build:
//   c++ -O2 -std=c++17 render_modblock_demo.cpp -o /tmp/render_mb && /tmp/render_mb <out_dir>
#include "../src/dsp/fx/fx_modblock.h"
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
    std::string dir = argc > 1 ? argv[1] : "."; auto in = horn(3.0, -12.f); std::vector<float> out = in;
    for (int md = 0; md < 3; md++) {
        Host h; h.make(hx::fx_create_modblock); h.fx->setParam(1, (float)md); h.fx->setParam(0, 1.f); h.fx->snap(); std::vector<float> oL, oR; size_t pos = 0;
        auto g = [&](float* b, int m, long) { for (int i = 0; i < m; i++) b[i] = in[pos++]; }; run(h.fx, in.size() / (double)SR, 48, false, g, defCtx(), &oL, &oR);
        for (size_t i = 0; i < oL.size(); i++) out.push_back(0.5f * (oL[i] + oR[i]));
    }
    wav((dir + "/modblock_demo.wav").c_str(), out); return 0; }

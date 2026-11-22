// Renders reverb_lush_demo.wav: [dry trumpet-like phrase] [same phrase through the reverb, defaults, MIX .5, + tail] [same phrase, WARP swept, + tail].
// mono = L+R sum (as the output is summed later), 16-bit 48 kHz, 1 s of silence between sections. Build:
//   c++ -O2 -std=c++17 render_reverb_lush_demo.cpp -o /tmp/render_rv && /tmp/render_rv <out_dir>
#include "../src/dsp/fx/fx_reverb.h"
#include "../src/dsp/fx/tests/fx_testutil.h"
using namespace hxt;
static std::vector<float> horn(double secs, float peakDb) {   // sparse phrase: tongued notes with gaps so the scattered echoes and the tail are audible
    static const float notes[] = {293.66f, 369.99f, 440.f, 587.33f, 554.37f, 440.f, 369.99f, 329.63f};
    static const float len[] = {0.55f, 0.35f, 0.9f, 1.4f, 0.4f, 0.6f, 0.5f, 1.3f}, gap[] = {0.15f, 0.1f, 0.5f, 0.6f, 0.1f, 0.2f, 0.2f, 0.f};
    std::vector<float> y((size_t)(secs * SR), 0.f); double ph[16] = {0}; uint32_t sd = 9; float lp = 0; double t0 = 0.2;
    for (int n = 0; n < 8; n++) {
        for (size_t i = (size_t)(t0 * SR); i < (size_t)((t0 + len[n]) * SR) && i < y.size(); i++) { double tn = i / (double)SR - t0;
            float env = tn < 0.015 ? (float)(tn / 0.015) : (tn < len[n] - 0.08 ? 1.f - 0.2f * (float)(tn / len[n]) : (float)((len[n] - tn) / 0.08)); float vib = 1.f + 0.007f * sinf(5.5f * 6.2831853f * (float)tn) * fminf(1.f, (float)tn * 3.f);
            float f = notes[n] * vib, s = 0; for (int h = 1; h <= 14; h++) { ph[h] += 6.2831853 * f * h / SR; s += sinf((float)ph[h]) * (1.f / h) * (h < 6 ? 1.f : 0.8f); }
            lp += ((rnd01(sd) * 2 - 1) - lp) * 0.2f; y[i] = (s * 0.3f + lp * 0.01f) * env; }
        t0 += len[n] + gap[n]; }
    float pk = 0; for (float x : y) pk = fmaxf(pk, fabsf(x)); float g = powf(10.f, peakDb / 20.f) / pk; for (auto& x : y) x *= g; return y;
}
static void wav(const char* path, const std::vector<float>& x) {
    FILE* f = fopen(path, "wb"); uint32_t n = (uint32_t)x.size() * 2, sr = 48000, br = sr * 2, d = 36 + n; uint16_t one = 1, ch = 1, ba = 2, bits = 16; uint32_t fmt = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&d, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); for (float v : x) { int s = (int)lrintf(fmaxf(-1.f, fminf(1.f, v)) * 32767.f); int16_t q = (int16_t)s; fwrite(&q, 2, 1, f); } fclose(f); }
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "."; const double phrase = 7.5, tail = 9.0; auto dry = horn(phrase, -12.f);
    std::vector<float> out = dry; out.insert(out.end(), (size_t)SR, 0.f);
    for (int warp = 0; warp < 2; warp++) {
        Host h; h.make(hx::fx_create_reverb); Fx* e = h.fx; e->setParam(0, 0.5f); e->snap(); std::vector<float> L(48), R(48); FxCtx c = defCtx();
        for (size_t b = 0; b * 48 < (size_t)((phrase + tail) * SR); b++) {
            float t = (float)(b * 48) / SR; if (warp) e->setParam(3, t < 1.5f || t > 7.5f ? 0.f : 0.5f - 0.5f * cosf(kTwoPi * (t - 1.5f) / 2.f));   // 3 swells 0 -> 1 -> 0, 2 s each
            for (int i = 0; i < 48; i++) { size_t k = b * 48 + i; L[i] = k < dry.size() ? dry[k] : 0.f; } R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false);
            for (int i = 0; i < 48; i++) out.push_back(0.5f * (L[i] + R[i])); }
        out.insert(out.end(), (size_t)SR, 0.f); }
    float pk = 0; for (float x : out) pk = fmaxf(pk, fabsf(x)); if (pk > 0.95f) for (auto& x : out) x *= 0.95f / pk;
    wav((dir + "/reverb_lush_demo.wav").c_str(), out); printf("peak %.3f, %.1f s\n", pk, out.size() / (double)SR); return 0; }

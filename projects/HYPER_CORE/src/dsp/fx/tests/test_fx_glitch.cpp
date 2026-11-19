#include "../fx_glitch.h"
#include "fx_testutil.h"
using namespace hxt;
enum { MIX = 0, INTENS, RATE, DENSITY, CRUSHMAX, DECMAX, REPMAX, REVPROB, PITPROB, DROPPROB, FEEDBACK, TONE, SEED, XFADE, DLYTIME, JUMPPROB };

// trumpet-like phrase: pentatonic notes, attack/release breath envelope, vibrato, 1/h harmonic roll-off, a little breath noise. amp = peak scale.
struct Trumpet {
    float amp; bool soft = false; mutable uint32_t ns = 99;
    void operator()(float* b, int m, long i0) const {
        static const float notes[8] = { 392.f, 466.2f, 523.3f, 587.3f, 523.3f, 466.2f, 392.f, 349.2f };
        for (int i = 0; i < m; i++) {
            long k = i0 + i; float t = (float)k / SR; int ni = (int)(t / 0.3f) % 8; float tn = fmodf(t, 0.3f);
            float env = tn < 0.02f ? tn / 0.02f : (tn > 0.24f ? fmaxf(0.f, (0.3f - tn) / 0.06f) : 1.f);
            float f = notes[ni] * (1.f + 0.004f * sinf(6.2831853f * 5.5f * t));
            static thread_local double ph = 0; ph += f / SR; if (ph > 1e6) ph -= 1e6;
            if (k == 0) ph = 0;
            float y = 0; for (int h = 1; h <= (soft ? 3 : 7); h++) y += sinf(6.2831853f * (float)(ph * h)) / (soft ? (float)(h * h * h) : (float)h);
            b[i] = amp * (0.4f * y * env + 0.004f * (rnd01(ns) * 2.f - 1.f));
        }
    }
};
static void setP(Fx* e, float I, uint32_t seed = 1) { e->setParam(MIX, 1.f); e->setParam(INTENS, I); e->setParam(SEED, (float)seed); e->snap(); }
static double rmsOf(const std::vector<float>& o, size_t a = 0) { double s = 0; for (size_t i = a; i < o.size(); i++) s += (double)o[i] * o[i]; return sqrt(s / (double)(o.size() - a)); }
// click metric: max |second difference| / RMS. A smooth waveform (even a fast pitched one) has tiny d2; a click/discontinuity of step s gives 2s.
static double maxStep(const std::vector<float>& o, size_t a = 0) { double m = 0; for (size_t i = a + 2; i < o.size(); i++) m = fmax(m, fabs((double)o[i] - 2.0 * o[i - 1] + o[i - 2])); return m; }

int main() {
    Opts o; o.tailSecs = 6.0; stdSuite("glitch", hx::fx_create_glitch, o);

    // ---- MIX = 0 exactly dry (even at INTENS 1, events running)
    { Host h; h.make(hx::fx_create_glitch); setP(h.fx, 1.f); h.fx->setParam(MIX, 0.f); h.fx->snap();
      std::vector<float> out, in; run(h.fx, 3.0, 48, false, Trumpet{0.5f}, defCtx(), &out);
      in.resize(out.size()); Trumpet{0.5f}(in.data(), (int)in.size(), 0);
      double err = 0; for (size_t i = 0; i < out.size(); i++) err = fmax(err, fabs(out[i] - in[i]));
      printf("  MIX=0 max err %.3g\n", err); HX_CHECK(err == 0.0, "MIX=0 not exactly dry: %g", err); }

    // ---- determinism: same SEED bit-identical, other SEED differs; dirty vs zeroed arena identical (wake zeroes all state)
    { std::vector<float> a, b, c, z;
      { Host h; h.make(hx::fx_create_glitch); setP(h.fx, 0.8f, 42); run(h.fx, 3.0, 48, false, Trumpet{0.4f}, defCtx(), &a); }
      { Host h; h.make(hx::fx_create_glitch); setP(h.fx, 0.8f, 42); run(h.fx, 3.0, 48, false, Trumpet{0.4f}, defCtx(), &b); }
      { Host h; h.make(hx::fx_create_glitch); setP(h.fx, 0.8f, 43); run(h.fx, 3.0, 48, false, Trumpet{0.4f}, defCtx(), &c); }
      { std::vector<uint8_t> mem(HX_SLOT_ARENA_BYTES, 0); Arena ar; ar.bind(mem.data(), mem.size()); Fx* e = hx::fx_create_glitch(ar, SR); e->init(SR, 48.f); setP(e, 0.8f, 42);
        run(e, 3.0, 48, false, Trumpet{0.4f}, defCtx(), &z); e->~Fx(); }
      HX_CHECK(a.size() == b.size() && !memcmp(a.data(), b.data(), a.size() * 4), "same seed not bit-identical");
      HX_CHECK(!memcmp(a.data(), z.data(), a.size() * 4), "dirty arena != zeroed arena (wake leaves state)");
      double d = 0; for (size_t i = 0; i < a.size(); i++) d = fmax(d, fabs(a[i] - c[i]));
      HX_CHECK(d > 0.01, "different seed gave same output"); printf("  determinism ok, seed diff %.3f\n", d); }

    // ---- event rate scales with INTENS (events per second, mean of 3 seeds, 20 s)
    { double r[4]; float Is[4] = { 0.f, 0.1f, 0.5f, 1.f };
      for (int k = 0; k < 4; k++) { double sum = 0;
          for (uint32_t sd = 1; sd <= 3; sd++) { Host h; h.make(hx::fx_create_glitch); setP(h.fx, Is[k], sd); run(h.fx, 20.0, 48, false, Trumpet{0.4f}, defCtx());
              sum += ((hx::FxGlitch*)h.fx)->eventCount / 20.0; }
          r[k] = sum / 3; }
      printf("  events/s: I=0 %.2f  I=.1 %.2f  I=.5 %.2f  I=1 %.2f\n", r[0], r[1], r[2], r[3]);
      HX_CHECK(r[0] > 0.05 && r[0] < 1.5, "I=0 should be occasional (%.2f/s)", r[0]);
      HX_CHECK(r[1] < r[2] && r[2] < r[3], "events not monotone in INTENS");
      HX_CHECK(r[3] > 3.0 * r[1], "I=1 rate %.2f not >3x I=.1 rate %.2f", r[3], r[1]); }

    // ---- no clicks: max second-difference vs RMS. (a) no crush, all slice/jump/drop events at I=1. (b) full crush. (c) -45 dBFS horn level-awareness.
    { auto clk = [&](const char* tag, float I, float crush, float amp, double bound, bool fb) {
          double worst = 0; double rmsMin = 1e9;
          for (uint32_t sd = 1; sd <= 4; sd++) { Host h; h.make(hx::fx_create_glitch); setP(h.fx, I, sd); h.fx->setParam(CRUSHMAX, crush); h.fx->setParam(DECMAX, crush > 0 ? 32.f : 1.f);
              h.fx->setParam(FEEDBACK, fb ? 0.9f : 0.3f); h.fx->setParam(PITPROB, 0.8f); h.fx->setParam(REVPROB, 0.6f); h.fx->snap();
              std::vector<float> out; run(h.fx, 8.0, 48, false, Trumpet{amp, true}, defCtx(), &out);
              size_t a = 4800; double rm = rmsOf(out, a); worst = fmax(worst, maxStep(out, a) / rm); rmsMin = fmin(rmsMin, rm); }
          printf("  clicks[%s]: worst d2/RMS %.3f (bound %.2f), min rms %.4g\n", tag, worst, bound, rmsMin);
          HX_CHECK(worst < bound, "%s d2/RMS %.3f >= %.2f", tag, worst, bound); return rmsMin; };
      // reference: the dry input's own d2/RMS, and a deliberate click for validity of the metric
      std::vector<float> in(48000 * 4); Trumpet{0.4f, true}(in.data(), (int)in.size(), 0);
      double ref = maxStep(in) / rmsOf(in); printf("  input d2/RMS %.3f\n", ref);
      std::vector<float> cl = in; for (size_t i = 100000; i < 100000 + 400; i++) cl[i] += 0.3f;   // 0.3 offset step (a click)
      double refc = maxStep(cl) / rmsOf(cl); printf("  hard-cut d2/RMS %.3f (metric must flag it)\n", refc);
      clk("noCrush I=1", 1.f, 0.f, 0.4f, 0.45, true);
      clk("crush I=1", 1.f, 1.f, 0.4f, 0.8, true);
      clk("crush I=.5", 0.5f, 1.f, 0.4f, 0.45, false);
      double q = clk("-45dBFS I=1", 1.f, 1.f, 0.0056f * 2.5f, 0.8, true);
      HX_CHECK(q > 0.0056 * 0.1, "level-relative crush zeroed the quiet input (rms %.5g)", q);
      HX_CHECK(refc > 0.45, "metric would not catch a hard click"); }

    // ---- CPU: host ns/sample, device % ~ ns*0.48 (limit 5%)
    { double ns[3]; float Is[3] = { 0.f, 0.5f, 1.f };
      for (int k = 0; k < 3; k++) { Host h; h.make(hx::fx_create_glitch); setP(h.fx, Is[k], 7); h.fx->setParam(PITPROB, 0.6f); h.fx->snap();
          std::vector<float> L(48), R(48); FxCtx c = defCtx(); Trumpet g{0.4f}; long nb = (long)(8.0 * SR / 48); float sink = 0;
          for (long b = 0; b < 500; b++) { g(L.data(), 48, b * 48); h.fx->smoothBlock(); h.fx->process(c, L.data(), R.data(), 48, false); }
          double best = 1e9;
          for (int rep = 0; rep < 3; rep++) { auto t0 = std::chrono::steady_clock::now();
              for (long b = 0; b < nb; b++) { g(L.data(), 48, (b + 500) * 48); h.fx->smoothBlock(); h.fx->process(c, L.data(), R.data(), 48, false); sink += L[3]; }
              double t = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48); best = fmin(best, t); }
          ns[k] = best; (void)sink; }
      // subtract input generator cost
      { std::vector<float> L(48); Trumpet g{0.4f}; long nb = (long)(8.0 * SR / 48); auto t0 = std::chrono::steady_clock::now(); float s = 0;
        for (long b = 0; b < nb; b++) { g(L.data(), 48, b * 48); s += L[5]; }
        double gen = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48);
        for (int k = 0; k < 3; k++) ns[k] -= gen; printf("  (generator %.1f ns/sample subtracted, sink %.2f)\n", gen, s); }
      for (int k = 0; k < 3; k++) printf("BENCH glitch I=%.1f %.2f ns/sample -> device ~%.2f %%\n", Is[k], ns[k], ns[k] * 0.48);
      HX_CHECK(fmax(ns[0], fmax(ns[1], ns[2])) * 0.48 <= 5.0, "CPU over 5%% device"); }

    // ---- memory
    { Host h; h.make(hx::fx_create_glitch); printf("  arena used: %zu bytes (ring %u floats = %.2f s)\n", h.arena.used, 131072u, 131072.0 / SR); }

    // ---- demo wav: dry, I=0.1, 0.5, 1.0, 4 s each (mono 16-bit 48 kHz) -> HYPER_CORE/tools/glitch_demo.wav
    { std::vector<float> all; float Is[4] = { -1.f, 0.1f, 0.5f, 1.f };
      for (int k = 0; k < 4; k++) { std::vector<float> seg;
          if (k == 0) { seg.resize(4 * 48000); Trumpet{0.4f}(seg.data(), (int)seg.size(), 0); }
          else { Host h; h.make(hx::fx_create_glitch); setP(h.fx, Is[k], 5); h.fx->setParam(MIX, 0.8f); h.fx->snap(); run(h.fx, 4.0, 48, false, Trumpet{0.4f}, defCtx(), &seg); }
          all.insert(all.end(), seg.begin(), seg.end()); }
      float pk = 0; for (float x : all) pk = fmaxf(pk, fabsf(x)); float g = pk > 0.9f ? 0.9f / pk : 1.f;
      const char* path = "../../../tools/glitch_demo.wav"; FILE* f = fopen(path, "wb");
      if (f) { uint32_t n = (uint32_t)all.size() * 2, sr = 48000, br = 96000, riff = 36 + n, fmtLen = 16; uint16_t pcm = 1, ch = 1, ba = 2, bits = 16;
          fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
          fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
          for (float x : all) { int16_t s = (int16_t)lrintf(fmaxf(-1.f, fminf(1.f, x * g)) * 32767.f); fwrite(&s, 2, 1, f); }
          fclose(f); printf("  wrote %s (%.0f s, gain %.2f)\n", path, all.size() / 48000.0, g); }
      else printf("  could not write demo wav (run from src/dsp/fx)\n"); }
    printf(failCount() ? "[glitch] FAILED\n" : "[glitch] ALL PASS\n");
    return failCount() ? 1 : 0;
}

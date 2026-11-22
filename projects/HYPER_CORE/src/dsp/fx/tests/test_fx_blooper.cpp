// test_fx_blooper.cpp -- host tests for FxBlooper (modifies the LOOPER's loop). `./out/test_blooper` runs; `./out/test_blooper --wav <path>` also renders the demo WAV.
#define HX_BLP_TELEMETRY 1
#include "../fx_blooper.h"
#include "../fx_looper.h"
#include "fx_testutil.h"
using namespace hxt;
typedef hx::FxBlooper B; typedef hx::FxLooper LP;
static const float TWO_PI = 6.2831853f;

// real FxLooper + FxBlooper chained exactly as the firmware glue should do it
struct Rig {
    Host hl, hb; LP* lp = nullptr; B* bl = nullptr; long t = 0; bool useBlooper = true;
    Rig(bool withBlooper = true) : useBlooper(withBlooper) {
        hl.make(hx::fx_create_looper); lp = (LP*)hl.fx; lp->setParam(0, 1.f); lp->snap();
        if (withBlooper) { hb.make(hx::fx_create_blooper); bl = (B*)hb.fx; }
    }
    void block(float* L, float* R, int n, bool st) {
        FxCtx c = defCtx();
        if (bl) { float ratio = hx::semiToRatio(lp->sm[LP::P_TRANSP]) * lp->dirCur, gain = hx::dbToLin(lp->sm[LP::P_VOLUME]) * lp->sm[LP::P_MIX] * lp->playG;
                  bl->setSource(lp->lay, lp->count, lp->len, lp->pp, ratio, gain); }
        lp->smoothBlock(); lp->process(c, L, R, n, st);
        if (bl) { bl->smoothBlock(); bl->process(c, L, R, n, st); }
    }
    template <class G> void feed(double secs, G g, std::vector<float>* out = nullptr) {
        std::vector<float> L(48), R(48); long total = (long)(secs * SR);
        for (long k = 0; k < total; k += 48) { int m = (int)((total - k) < 48 ? (total - k) : 48); g(L.data(), m, t); for (int i = 0; i < m; i++) R[i] = L[i];
            block(L.data(), R.data(), m, false); if (out) out->insert(out->end(), L.begin(), L.begin() + m); t += m; }
    }
    void press() { lp->action(hx::A_MAIN, 1.f); }
};
struct Nz { mutable uint32_t s = 4242; float a; void operator()(float* b, int m, long) const { for (int i = 0; i < m; i++) b[i] = a * (hx::rnd01(s) * 2.f - 1.f); } };
// record a loop of `secs` of g on the looper (first layer), then it plays
template <class G> static void recordLoop(Rig& r, double secs, G g) { r.press(); r.feed(secs, g); r.press(); r.feed(0.05, SilenceGen()); }
static double goertzel(const float* x, int n, double hz) {
    double w = 2 * 3.14159265358979 * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (int i = 0; i < n; i++) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; } return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n * 2;
}
static float maxStep(const std::vector<float>& o) { float m = 0; for (size_t i = 1; i < o.size(); i++) m = fmaxf(m, fabsf(o[i] - o[i - 1])); return m; }
struct Phrase { void operator()(float* b, int m, long i0) const {
    static const float notes[] = { 392.f, 494.f, 587.f, 523.f, 440.f, 392.f, 330.f, 392.f };
    for (int i = 0; i < m; i++) { float t = (float)(i0 + i) / SR; int ni = ((int)(t * 2.f)) & 7; float f = notes[ni]; float lt = fmodf(t * 2.f, 1.f);
        float env = lt < 0.05f ? lt / 0.05f : (lt > 0.8f ? (1.f - lt) / 0.2f : 1.f); float y = 0; for (int h = 1; h <= 6; h++) y += sinf(TWO_PI * f * h * t) / (float)h;
        b[i] = 0.25f * y * env * (lt < 0.9f ? 1.f : 0.f); } } };
static void wavWrite(const char* path, const std::vector<float>& x) {
    FILE* f = fopen(path, "wb"); if (!f) { printf("  cannot write %s\n", path); return; }
    uint32_t n = (uint32_t)x.size(), db = n * 2, rate = 48000, br = rate * 2; uint16_t fmt = 1, ch = 1, ba = 2, bits = 16; uint32_t sixteen = 16, riff = 36 + db;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&db, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) { float v = x[i] * 0.9f; v = v > 1.f ? 1.f : (v < -1.f ? -1.f : v); int16_t s = (int16_t)lrintf(v * 32767.f); fwrite(&s, 2, 1, f); }
    fclose(f); printf("  wrote %s (%.1f s)\n", path, x.size() / 48000.0);
}

int main(int argc, char** argv) {
    stdSuite("blooper", hx::fx_create_blooper, Opts{});
    { Host h; h.make(hx::fx_create_blooper); B* b = (B*)h.fx;
      printf("  memory: %lu B arena (2 allpass buffers) + object %lu B; the old private 60 s capture ring (11,520,000 B) is gone\n", (unsigned long)b->bytes(), (unsigned long)sizeof(B));
      HX_CHECK(b->bytes() < 4096, "memory"); HX_CHECK(!b->hasSource() && !b->active(), "no source -> dim"); }
    // ---- NO LOOP: bit-exact pass-through, zero work (ON + PROB 1 + joystick pushed, never given a source)
    { Host h; h.make(hx::fx_create_blooper); B* b = (B*)h.fx; b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_SPEED, 1.f); b->snap();
      std::vector<float> o; long t = 0; { std::vector<float> L(48), R(48); Nz g{0, 0.5f}; double err = 0; for (int k = 0; k < 1000; k++) { g(L.data(), 48, t); std::vector<float> ref = L; b->smoothBlock(); b->process(defCtx(), L.data(), R.data(), 48, false); for (int i = 0; i < 48; i++) err = fmax(err, fabs(L[i] - ref[i])); }
        HX_CHECK(err == 0.0, "no-source pass-through err %.3g", err); HX_CHECK(b->dbgEvents == 0, "events without a loop"); }
      // CPU with no source
      std::vector<float> L(48, 0.1f), R(48, 0.1f); auto t0 = std::chrono::steady_clock::now(); for (int k = 0; k < 200000; k++) { b->smoothBlock(); b->process(defCtx(), L.data(), R.data(), 48, false); }
      double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (200000.0 * 48); printf("  no-source cost: %.3f ns/sample (%.2f %% device)\n", ns, ns * 0.48); HX_CHECK(ns * 0.48 < 0.3, "no-source not ~free"); }
    // ---- null vs the looper alone: ON prob 0 joystick centred, and OFF prob 1: bit-exact
    for (int mode = 0; mode < 2; mode++) {
        Rig a(false), b(true); b.bl->setParam(B::P_ON, mode == 0 ? 1.f : 0.f); b.bl->setParam(B::P_PROB, mode == 0 ? 0.f : 1.f); b.bl->setParam(B::P_SPEED, 0.05f); b.bl->snap();
        std::vector<float> oa, ob; Nz g{0, 0.5f};
        for (Rig* r : { &a, &b }) { std::vector<float>& o = (r == &a) ? oa : ob; recordLoop(*r, 2.0, g); r->feed(6.0, Nz{77, 0.2f}, &o); }
        double err = 0; for (size_t i = 0; i < oa.size(); i++) err = fmax(err, fabs(oa[i] - ob[i]));
        printf("  null vs looper alone (%s): max err %.3g, source seen %d\n", mode == 0 ? "ON prob0" : "OFF prob1", err, (int)b.bl->hasSource()); HX_CHECK(err == 0.0 && b.bl->hasSource(), "null not bit-exact %.3g", err);
    }
    // ---- slice exactness REPLACE / OVERLAY, layers summed as played. Silent live input => output is the loop (+/- slices)
    for (int mode = 0; mode < 2; mode++) {
        Rig r; B* b = r.bl; Nz g{0, 0.3f}; recordLoop(r, 2.0, g);
        r.lp->action(hx::A_MAIN, 1.f); r.feed(0.1, SineGen{300.f, 0.2f});   // start overdub layer 2 (one loop long, auto punch-out)
        r.feed((double)r.lp->len / SR + 0.1, SineGen{300.f, 0.2f}); HX_CHECK(r.lp->count == 2, "layers %d", r.lp->count);
        b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 0.f); b->setParam(B::P_DIST, 0.f); b->setParam(B::P_SIZE, 0.7f); b->setParam(B::P_REPL, mode == 0 ? 1.f : 0.f); b->snap();
        bool got = false; int tries = 0;
        while (!got && tries++ < 60) {
            int ev0 = b->dbgEvents; b->action(hx::A_ALT, 1.f); std::vector<float> o; long t0 = r.t; r.feed(0.01, SilenceGen(), &o);
            if (b->dbgEvents == ev0) continue;
            int fireT = b->dbgLastFireT, sp = b->dbgLastStartPos, len = b->dbgLastLen; float spd = b->dbgLastSpd; int st = b->dbgLastStut; int ph0 = b->dbgLastPh;
            std::vector<float> rest; r.feed(3.5, SilenceGen(), &rest); o.insert(o.end(), rest.begin(), rest.end());
            if (spd != 1.f || st) continue;
            int L0 = r.lp->len; double err = 0; int cnt = 0;
            for (int j = (int)(0.02 * SR); j < len - (int)(0.02 * SR); j += 5) {
                long k = (long)fireT + j - t0; if (k < 0 || k >= (long)o.size()) continue;
                int gi = (sp + j) % L0, ni = (ph0 + j) % L0; float grain = r.lp->lay[0][gi] + r.lp->lay[1][gi], normal = r.lp->lay[0][ni] + r.lp->lay[1][ni];
                float want = mode == 0 ? grain : normal + grain; err = fmax(err, fabs(o[k] - want)); cnt++; }
            printf("  %s: slice len %d checked %d max err %.3g\n", mode == 0 ? "REPLACE (out == loop at slice pos, layers summed)" : "OVERLAY (out == loop + slice)", len, cnt, err);
            HX_CHECK(cnt > 100 && err < 1e-5, "slice mismatch err %.3g", err); got = true;
        }
        HX_CHECK(got, "no plain 1x slice in 60 cues");
    }
    // ---- joystick / speeds on a sine loop (silent live input)
    for (int joy = -1; joy <= 1; joy++) {
        Rig r; B* b = r.bl; recordLoop(r, 3.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_DIST, 0.f); b->setParam(B::P_SIZE, 0.6f); b->setParam(B::P_SPEED, (float)joy); b->snap();
        r.feed(2.0, SilenceGen()); std::vector<float> o; r.feed(20.0, SilenceGen(), &o);
        float mul = b->rate(), want = joy == 0 ? 1.f : (joy > 0 ? 2.f : 0.5f); HX_CHECK(fabsf(mul - want) < 1e-3f, "joystick %d: mul %.4f", joy, mul);
        double fa = joy == 0 ? 150.0 : (joy > 0 ? 600.0 : 150.0), fb = joy == 0 ? 600.0 : (joy > 0 ? 1200.0 : 75.0);
        double ea = goertzel(o.data(), (int)o.size(), fa), eb = goertzel(o.data(), (int)o.size(), fb), ex = goertzel(o.data(), (int)o.size(), 1000.0);
        printf("  joy %+d: mul %.3f  slice tones %.0f Hz %.4f, %.0f Hz %.4f | 1 kHz %.5f\n", joy, mul, fa, ea, fb, eb, ex); HX_CHECK(ea > 0.003 && eb > 0.003 && ex < 0.002, "speed set wrong");
    }
    { Rig r; B* b = r.bl; recordLoop(r, 2.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 0.f); r.feed(0.5, SilenceGen());
      b->setParam(B::P_SPEED, 0.08f); r.feed(0.1, SilenceGen()); HX_CHECK(b->rate() == 1.f, "deadzone edge %.4f", b->rate());
      int ev0 = b->dbgEvents; b->setParam(B::P_SPEED, 1.f); r.feed(0.2, SilenceGen()); HX_CHECK(b->dbgEvents == ev0 + 1, "joystick cue (%d)", b->dbgEvents - ev0); HX_CHECK(b->dbgLastCue == 1, "cue is immediate"); }
    // ---- duty cap, grid lock, SIZE, DIST (displacement), HF
    { double duty[4]; float probs[4] = { 0.f, 0.3f, 0.6f, 1.f };
      for (int k = 0; k < 4; k++) { Rig r; B* b = r.bl; recordLoop(r, 2.0, Nz{0, 0.3f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, probs[k]); b->snap(); r.feed(1.0, SilenceGen()); int a0 = b->dbgActiveSamples; r.feed(60.0, SilenceGen()); duty[k] = (double)(b->dbgActiveSamples - a0) / (60.0 * SR); }
      printf("  duty PROB 0/.3/.6/1: %.3f %.3f %.3f %.3f (cap 0.45)\n", duty[0], duty[1], duty[2], duty[3]);
      HX_CHECK(duty[0] == 0.0 && duty[1] > 0.04 && duty[1] < duty[2] && duty[2] < duty[3] && duty[3] < 0.52 && duty[3] > 0.25, "duty not monotone/capped"); }
    { Rig r; B* b = r.bl; recordLoop(r, 2.0, Nz{0, 0.3f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->snap(); int last = 0, on = 0, tot = 0;
      for (int k = 0; k < 30000; k++) { r.feed(0.001, SilenceGen()); if (b->dbgEvents != last) { last = b->dbgEvents; tot++; on += b->dbgLastOnTick; } }
      printf("  grid: %d/%d generative events launched on a loop tick (tick %d samples of loop %d)\n", on, tot, b->tickLen, r.lp->len); HX_CHECK(tot > 20 && on == tot, "events off-grid"); }
    { double mLen[2], mDisp[2]; float sz[2] = { 0.f, 1.f };
      for (int k = 0; k < 2; k++) { Rig r; B* b = r.bl; recordLoop(r, 4.0, Nz{0, 0.3f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_SIZE, sz[k]); b->setParam(B::P_DIST, sz[k]); b->snap();
        int last = 0, cnt = 0; double sl = 0, sd = 0; int L0 = r.lp->len;
        for (int i = 0; i < 60000; i++) { r.feed(0.001, SilenceGen()); if (b->dbgEvents != last) { last = b->dbgEvents; int d = abs(b->dbgLastStartPos - b->dbgLastPh); if (d > L0 / 2) d = L0 - d; sl += b->dbgLastLen; sd += (double)d / L0; cnt++; } }
        mLen[k] = sl / cnt / SR; mDisp[k] = sd / cnt; printf("  SIZE/DIST %.0f: %d events, mean len %.3f s, mean displacement %.3f of the loop\n", sz[k], cnt, mLen[k], mDisp[k]); }
      HX_CHECK(mLen[1] > 2.5 * mLen[0], "SIZE"); HX_CHECK(mDisp[0] < 0.06 && mDisp[1] > 0.1 && mDisp[1] > 3 * mDisp[0], "DIST near->far"); }
    { double hf[2]; for (int k = 0; k < 2; k++) { Rig r; B* b = r.bl; recordLoop(r, 4.0, Nz{0, 0.3f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_DIST, (float)k); b->setParam(B::P_SIZE, 1.f); b->snap();
        b->dbgWetSq = b->dbgWetDiffSq = 0; r.feed(12.0, SilenceGen()); hf[k] = b->dbgWetDiffSq / (b->dbgWetSq + 1e-12); }
      printf("  wet bus HF ratio (white = 2): near %.3f far %.3f\n", hf[0], hf[1]); HX_CHECK(hf[0] > 0.8 && hf[1] < 0.5 * hf[0], "far not darker"); }
    // ---- follows looper varispeed/reverse and stop; looper cleared mid-event; looper stopped
    { Rig r; B* b = r.bl; recordLoop(r, 2.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); r.lp->setParam(LP::P_TRANSP, 7.f); r.lp->setParam(LP::P_REVERSE, 1.f); r.lp->snap();
      std::vector<float> o; r.feed(8.0, SilenceGen(), &o); Stats s; scan(s, o.data(), (int)o.size()); HX_CHECK(!s.bad && s.peak < 2.f && b->dbgEvents > 5, "varispeed/reverse run (events %d)", b->dbgEvents);
      int guard = 0; while (b->dbgActiveSamples == 0 && guard++ < 100) r.feed(0.05, SilenceGen());
      for (int k = 0; k < 400 && b->evRem <= 0; k++) r.feed(0.005, SilenceGen()); HX_CHECK(b->evRem > 0, "event active before clear");
      r.lp->action(hx::A_CLEAR, 1.f); std::vector<float> o2; r.feed(0.5, SilenceGen(), &o2); Stats s2; scan(s2, o2.data() + 2400, (int)o2.size() - 2400); Stats s2a; scan(s2a, o2.data(), (int)o2.size()); s2.bad = s2a.bad;
      HX_CHECK(!s2.bad && !b->hasSource(), "clear mid-event: bad=%d source=%d", (int)s2.bad, (int)b->hasSource()); HX_CHECK(s2.peak < 1e-4f, "clear mid-event leaves sound %.5f", s2.peak); }
    { Rig r; B* b = r.bl; recordLoop(r, 2.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); r.feed(2.0, SilenceGen()); HX_CHECK(b->hasSource(), "playing -> source");
      r.lp->action(hx::A_ALT, 1.f); r.feed(0.5, SilenceGen()); HX_CHECK(!b->hasSource(), "looper stopped -> no source (dim)"); }
    // ---- clicks: real looper loop of a phrase, everything moving; step bound vs the loop's own step
    { Rig r; B* b = r.bl; recordLoop(r, 4.0, Phrase()); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_SIZE, 0.2f); b->snap(); std::vector<float> o; uint32_t s = 99;
      for (int blk = 0; blk < 60 * 1000; blk++) { if (blk % 700 == 0) b->action(hx::A_MAIN, 1.f); if (blk % 1300 == 0) b->action(hx::A_ALT, 1.f);
        if (blk % 250 == 0) b->setParam(B::P_SPEED, hx::rnd01(s) < 0.3f ? 0.f : hx::rnd01(s) * 2.f - 1.f); if (blk % 400 == 0) b->setParam(B::P_DIST, hx::rnd01(s)); if (blk % 5000 == 0) b->setParam(B::P_REPL, (float)(blk / 5000 & 1));
        r.feed(0.001, SilenceGen(), &o); }
      std::vector<float> loop(r.lp->len); for (int i = 0; i < r.lp->len; i++) loop[i] = r.lp->lay[0][i]; float ml = maxStep(loop), mo = maxStep(o);
      printf("  click: loop's own max step %.4f, output %.4f\n", ml, mo); HX_CHECK(mo < 3.f * ml + 0.02f, "click %.4f vs %.4f", mo, ml); }
    // ---- joystick step click on a long slice
    { Rig r; B* b = r.bl; recordLoop(r, 3.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 0.f); b->setParam(B::P_DIST, 0.f); b->setParam(B::P_SIZE, 1.f); b->snap(); r.feed(1.0, SilenceGen());
      std::vector<float> o; b->action(hx::A_ALT, 1.f); for (int k = 0; k < 6; k++) { b->setParam(B::P_SPEED, k == 1 ? 1.f : (k == 3 ? -1.f : 0.f)); r.feed(0.1, SilenceGen(), &o); }
      float step = maxStep(o); printf("  joystick step: max |dx| %.4f (2x sine bound %.4f)\n", step, 0.4f * TWO_PI * 600.f / 48000.f); HX_CHECK(step < 0.4f * TWO_PI * 700.f / 48000.f, "joystick click %.4f", step); }
    // ---- robustness: NaN/inf on the live input through looper+blooper, tails
    { Rig r; B* b = r.bl; recordLoop(r, 2.0, SineGen{300.f, 0.4f}); b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); std::vector<float> o;
      struct Bad { void operator()(float* x, int m, long i0) const { for (int i = 0; i < m; i++) { long k = i0 + i; x[i] = (k % 977 == 0) ? NAN : ((k % 1301 == 0) ? INFINITY : 0.1f); } } };
      r.feed(6.0, Bad(), &o); Stats s; scan(s, o.data(), (int)o.size()); HX_CHECK(!s.bad, "NaN escaped"); std::vector<float> o2; r.feed(8.0, SilenceGen(), &o2); Stats s2; scan(s2, o2.data(), (int)o2.size()); HX_CHECK(!s2.bad && s2.denorm == 0, "tail"); }
    // ---- wake with dirty state: identical to a fresh instance on a synthetic source
    { std::vector<float> loop(96000); { uint32_t s = 5; for (auto& x : loop) x = 0.3f * (hx::rnd01(s) * 2.f - 1.f); }
      auto scenario = [&](B* e, std::vector<float>& o) { const float* L1[1] = { loop.data() }; e->setParam(B::P_ON, 1.f); e->setParam(B::P_PROB, 0.8f); e->setParam(B::P_SPEED, 0.f); e->snap();
        float pos = 0; std::vector<float> L(48), R(48); for (int k = 0; k < 1500; k++) { if (k == 700) e->setParam(B::P_SPEED, 1.f); for (int i = 0; i < 48; i++) L[i] = 0.f;
          e->setSource(L1, 1, 96000, pos, 1.f, 1.f); e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false); pos = fmodf(pos + 48.f, 96000.f); o.insert(o.end(), L.begin(), L.end()); } };
      Host a; a.make(hx::fx_create_blooper); std::vector<float> ref; scenario((B*)a.fx, ref);
      Host b; b.make(hx::fx_create_blooper); B* bp = (B*)b.fx; std::vector<float> junk; scenario(bp, junk);
      memset(bp->ap1, 0xCD, B::AP1 * 4); memset(bp->ap2, 0xCD, B::AP2 * 4); bp->lpY = 1e9f; bp->mulCur = 7.f; bp->evRem = 999; bp->onG = 1.f; bp->rng = 5; bp->tickLen = 3; bp->tkPrv = 9; bp->eh.i = 77777; bp->idleN = 5; bp->wake();
      bp->setParam(B::P_SPEED, 0.f); bp->snap(); std::vector<float> o; scenario(bp, o);
      bool same = o.size() == ref.size() && memcmp(o.data(), ref.data(), o.size() * 4) == 0; HX_CHECK(same, "wake() != fresh"); printf("  wake on dirty state: bit-identical to fresh = %d\n", (int)same); }
    // ---- actions
    { Host h; h.make(hx::fx_create_blooper); Fx* e = h.fx; B* b = (B*)e; HX_CHECK(!b->isOn(), "default off"); e->action(hx::A_MAIN, 1.f); HX_CHECK(b->isOn(), "toggle on"); e->action(hx::A_MAIN, 1.f); HX_CHECK(!b->isOn(), "toggle off");
      e->action(hx::A_HOLD, 1.f); HX_CHECK(b->isOn(), "hold on"); e->action(hx::A_HOLD, 0.f); HX_CHECK(!b->isOn(), "hold off"); }
    // ---- CPU: blooper only, worst case: 4 layers x 60 s source, PROB 1, SIZE 1, DIST far, joystick pushed, fractional rate (Hermite normal read + linear slice)
    { const int LEN = 2880000; std::vector<float> l0(LEN), l1(LEN), l2(LEN), l3(LEN); { uint32_t s = 3; for (int i = 0; i < LEN; i++) { l0[i] = 0.1f * (hx::rnd01(s) - 0.5f); l1[i] = l0[i] * 0.5f; l2[i] = l0[i] * 0.3f; l3[i] = l0[i] * 0.2f; } }
      const float* ls[4] = { l0.data(), l1.data(), l2.data(), l3.data() };
      Host h; h.make(hx::fx_create_blooper); B* b = (B*)h.fx; b->setParam(B::P_ON, 1.f); b->setParam(B::P_PROB, 1.f); b->setParam(B::P_SIZE, 1.f); b->setParam(B::P_DIST, 1.f); b->setParam(B::P_SPEED, 0.7f); b->snap();
      std::vector<float> L(48, 0.1f), R(48, 0.1f); float pos = 0; FxCtx c = defCtx(); const float rate = 0.97f;
      auto run = [&](long nb) { for (long k = 0; k < nb; k++) { b->setSource(ls, 4, LEN, pos, rate, 1.f); pos = fmodf(pos + 48 * rate, (float)LEN); b->smoothBlock(); b->process(c, L.data(), R.data(), 48, false); } };
      run(3000); auto t0 = std::chrono::steady_clock::now(); long nb = 48000L * 8 / 48; run(nb);
      double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48);
      printf("BENCH blooper worst-case (4 layers, fractional rate) %.2f ns/sample -> device ~%.2f %% (limit 6 %%); duty %.2f\n", ns, ns * 0.48, (double)b->dbgActiveSamples / (48000.0 * 8 + 3000 * 48)); HX_CHECK(ns * 0.48 <= 6.0, "CPU %.2f %%", ns * 0.48); }
    // ---- demo wav: real looper loop of a phrase + overdubbed layer, blooper over it, silent live input
    for (int a = 1; a + 1 < argc; a++) if (!strcmp(argv[a], "--wav")) {
        Rig r; B* b = r.bl; std::vector<float> o; b->setParam(B::P_LEVEL, 0.f); b->snap();
        struct Pad { void operator()(float* x, int m, long i0) const { for (int i = 0; i < m; i++) { float t = (float)(i0 + i) / SR; x[i] = 0.12f * sinf(TWO_PI * 196.f * t) * (0.6f + 0.4f * sinf(TWO_PI * 0.25f * t)); } } };
        r.press(); r.feed(8.0, Phrase()); r.press(); r.feed(0.05, SilenceGen(), &o);                       // 8 s loop
        r.press(); r.feed((double)r.lp->len / SR + 0.1, Pad(), &o); printf("   looper: %d layers, %d samples\n", r.lp->count, r.lp->len);
        auto sec = [&](const char* label, double s, float on, float prob, float size, float dist, float joy, float repl) { printf("   [%5.1fs] %s\n", r.t / 48000.0, label);
            b->setParam(B::P_ON, on); b->setParam(B::P_PROB, prob); b->setParam(B::P_SIZE, size); b->setParam(B::P_DIST, dist); b->setParam(B::P_SPEED, joy); b->setParam(B::P_REPL, repl); r.feed(s, SilenceGen(), &o); };
        sec("OFF: plain looper", 8, 0, .5f, .5f, .3f, 0, 1);
        sec("ON prob 0.25 size .4 near", 8, 1, .25f, .4f, 0, 0, 1);
        sec("prob 0.6 near", 8, 1, .6f, .4f, 0, 0, 1);
        sec("prob 1.0 small slices", 8, 1, 1, 0, .2f, 0, 1);
        sec("prob 0.6 big slices", 8, 1, .6f, 1, .2f, 0, 1);
        sec("FAR: grabs from anywhere in the loop, darker", 8, 1, .7f, .5f, 1, 0, 1);
        sec("overlay (REPLACE 0)", 8, 1, .7f, .5f, .5f, 0, 0);
        sec("joystick UP (2x)", 4, 1, .8f, .5f, .4f, 1, 1);
        sec("joystick DOWN (0.5x)", 4, 1, .8f, .5f, .4f, -1, 1);
        sec("joystick neutral", 4, 1, .8f, .5f, .4f, 0, 1);
        wavWrite(argv[a + 1], o); }
    printf("%s\n", failCount() ? "blooper: FAIL" : "blooper: PASS");
    return failCount() ? 1 : 0;
}

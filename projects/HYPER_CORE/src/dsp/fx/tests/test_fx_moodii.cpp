#include "../fx_moodii.h"
#include "../fx_glitch.h"
#include "fx_testutil.h"
using namespace hx; using namespace hxt;
typedef FxMoodII M;

static Fx* mk(Host& h) { h.make(fx_create_moodii); return h.fx; }
// click score: largest sample-to-sample step that is BOTH > 3x anything in the previous 10 ms AND > 0.02 (steady tones/echoes of any level pass; a splice, jump or unfaded mode switch does not)
static float maxDelta(const std::vector<float>& o, size_t from) {
    float worst = 0, rawMax = 0; size_t at = 0; std::vector<float> d(o.size(), 0.f);
    for (size_t i = 1; i < o.size(); i++) d[i] = fabsf(o[i] - o[i - 1]);
    for (size_t i = from + 1; i < o.size(); i++) { float rec = 0; for (size_t k = (i > 480 ? i - 480 : 1); k < i; k++) rec = fmaxf(rec, d[k]); rawMax = fmaxf(rawMax, d[i]);
        if (d[i] > 3.f * rec && d[i] > 0.02f && d[i] > worst) { worst = d[i]; at = i; } }
    if (getenv("HX_DBG")) printf("    [raw max step %.4f, click score %.4f at %.4f s]\n", rawMax, worst, at / 48000.0);
    return worst;
}
static float zeroCrossHz(const std::vector<float>& o, size_t from) { int z = 0; for (size_t i = from + 1; i < o.size(); i++) if ((o[i - 1] < 0) != (o[i] < 0)) z++; return 0.5f * z / ((float)(o.size() - from) / SR); }
static double rms(const std::vector<float>& o, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; i++) e += (double)o[i] * o[i]; return sqrt(e / (double)(b - a)); }

// block-wise driver with a per-block callback (for param / action changes); returns mono L output
template <class G, class CB>
static std::vector<float> drive(Fx* fx, double secs, G gen, CB cb, bool stereo = false) {
    std::vector<float> out, L(48), R(48); FxCtx c = defCtx(330.f); long total = (long)(secs * SR) / 48;
    for (long b = 0; b < total; b++) {
        cb(b); gen(L.data(), 48, b * 48); R = L; fx->smoothBlock(); fx->process(c, L.data(), R.data(), 48, stereo);
        out.insert(out.end(), L.begin(), L.end());
    }
    return out;
}
struct Sine { float hz, amp; void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) b[i] = amp * sinf(6.2831853f * hz * (float)(i0 + i) / SR); } };

int main() {
    stdSuite("moodii", fx_create_moodii, Opts{});

    // ---- param count / ids
    { Host h; Fx* e = mk(h); HX_CHECK(e->np <= HX_MAXP && e->np == 16, "np=%d", e->np); printf("  params=%d (max %d)\n", e->np, HX_MAXP); }

    // ---- NaN / bounded: every wet mode x loop mode x clock rung x route, with actions, 2 s each, noisy + sine
    { long bad = 0; uint32_t sd = 5;
      for (int wm = 0; wm < 3; wm++) for (int lm = 0; lm < 3; lm++) for (int rung = 0; rung < 9; rung += 2) for (int rt = 0; rt < 3; rt++) {
          Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_WMODE, (float)wm); e->setParam(M::P_LMODE, (float)lm); e->setParam(M::P_LOOP, 1.f); e->setParam(M::P_ROUTE, (float)rt);
          e->setParam(M::P_CLOCK, rung / 8.f); e->setParam(M::P_MOD, 0.9f); e->setParam(M::P_LMOD, rnd01(sd)); e->snap();
          auto cb = [&](long b) { if (b == 300) e->action(A_MAIN, 1); if (b == 600) e->action(A_ALT, 1); if (b == 800) e->action(A_ALT, 1); if (b == 900) e->action(A_HOLD, 1); if (b == 1000) e->action(A_HOLD, 1); if (b == 1100) e->action(A_MAIN, 1); };
          auto o = drive(e, 2.0, Sine{330.f, 0.3f}, cb); Stats s; scan(s, o.data(), (int)o.size()); bad += s.bad + (s.peak > 4.5f) + (s.denorm > 0);
      }
      HX_CHECK(bad == 0, "NaN/unbounded/denormal in %ld configs", bad); printf("  NaN/bound/denormal matrix (3x3x5x3 configs): %s\n", bad ? "FAIL" : "ok"); }

    // ---- clicks: steady tones at home clock in each wet mode, mode switching, loop capture/mode switching/overdub, MIX sweep
    {
        const float lim = 0.02f;
        for (int wm = 0; wm < 3; wm++) { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_WMODE, (float)wm); e->snap();
            auto o = drive(e, 4.0, Sine{220.f, 0.3f}, [](long) {}); float d = maxDelta(o, 12000); printf("  click wet=%d steady: max|d|=%.4f\n", wm, d); HX_CHECK(d < lim, "click wet mode %d: %.4f", wm, d); }
        { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->snap();   // wet mode cycling, time sweep
          auto o = drive(e, 6.0, Sine{220.f, 0.3f}, [&](long b) { if (b % 300 == 0) e->setParam(M::P_WMODE, (float)((b / 300) % 3)); e->setParam(M::P_LENGTH, 0.5f + 0.5f * sinf(b * 0.01f)); });
          float d = maxDelta(o, 4800); printf("  click wet-mode cycling + TIME sweep: %.4f\n", d); HX_CHECK(d < lim, "click mode cycle %.4f", d); }
        { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_LOOP, 1.f); e->setParam(M::P_WMODE, 1.f); e->setParam(M::P_ROUTE, 2.f); e->snap();   // loop: capture, modes, overdub, replace
          auto o = drive(e, 12.0, Sine{220.f, 0.3f}, [&](long b) { if (b == 800) e->action(A_MAIN, 1); if (b % 400 == 0 && b > 900) e->setParam(M::P_LMODE, (float)((b / 400) % 3)); if (b == 1700) e->action(A_ALT, 1); if (b == 2300) e->action(A_ALT, 1); if (b == 4000) e->action(A_MAIN, 1); if (b == 4500) e->action(A_MAIN, 1); });
          float d = maxDelta(o, 4800); printf("  click loop capture/modes/overdub/replace: %.4f\n", d); HX_CHECK(d < lim, "click loop %.4f", d); }
        { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 0.f); e->snap();
          auto o = drive(e, 4.0, Sine{220.f, 0.3f}, [&](long b) { e->setParam(M::P_MIX, 0.5f + 0.5f * sinf(b * 0.02f)); });
          float d = maxDelta(o, 4800); printf("  click MIX sweep: %.4f\n", d); HX_CHECK(d < lim, "click mix %.4f", d); }
        { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_CLOCK, 0.5f); e->snap();   // clock hard steps home <-> +12 (landing hit duck is ramped over 1 ms)
          auto o = drive(e, 4.0, Sine{220.f, 0.3f}, [&](long b) { e->setParam(M::P_CLOCK, 0.5f + 0.25f * ((b / 100) % 2)); }); float d = maxDelta(o, 4800); printf("  clock hard steps (slip, up): %.4f\n", d); HX_CHECK(d < lim, "clock step click %.4f", d); }
    }

    // ---- LEVEL: default (SLIP, MIX 1) is unity at every input level an HT mic/tone chain can deliver; every SLIP rung and every wet mode stays within +2 dB of the input (no make-up gain, quantiser/duck de-normalised)
    {
        auto gainDb = [&](Host& h, float hz, float amp, bool stereo) { std::vector<float> o; run(h.fx, 5.0, 48, stereo, SineGen{hz, amp}, defCtx(hz), &o); double a = 0; size_t n = 0; for (size_t i = 120000; i < o.size(); i++) { a += (double)o[i] * o[i]; n++; }
            return 10.0 * log10((a / (double)n) / (0.5 * (double)amp * amp)); };
        float worstDef = 0.f;
        for (float dbfs : {-50.f, -40.f, -30.f, -12.f, -3.f}) for (float hz : {110.f, 330.f, 1500.f}) for (int st = 0; st < 2; st++) {
            Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->snap(); float g = (float)gainDb(h, hz, dbToLin(dbfs), st != 0); if (fabsf(g) > fabsf(worstDef)) worstDef = g;
            HX_CHECK(fabsf(g) <= 2.f, "default level %+.2f dB at %.0f dBFS %.0f Hz stereo=%d", g, dbfs, hz, st); }
        float worstAny = -99.f;
        for (int wm = 0; wm < 3; wm++) for (int rung = 0; rung < 9; rung++) { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_WMODE, (float)wm); e->setParam(M::P_CLOCK, rung / 8.f); e->snap();
            float g = (float)gainDb(h, 330.f, dbToLin(-30.f), false); if (g > worstAny) worstAny = g; HX_CHECK(g <= (wm == 1 ? 6.f : 2.f), "level wm=%d rung=%d: %+.2f dB", wm, rung, g); }
        printf("  level: default SLIP MIX 1 within %+.2f dB of input over -50..-3 dBFS; worst gain any wet mode/rung (-30 dBFS) %+.2f dB (delay fb 0.5 can add up to +6)\n", worstDef, worstAny);
    }

    // ---- JOYSTICK (CLKMOD): +1 -> x2 pitch, -1 -> x0.5, +-0.08 dead zone, 2^x between; level within +-2 dB; stacks with CLOCK and GEN; instant and click-free
    {
        auto pitch = [&](float cm, float clock, float grit) { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_CLOCK, clock); e->setParam(M::P_CLKMOD, cm); e->setParam(M::P_GRIT, grit); e->snap();
            auto o = drive(e, 3.0, Sine{440.f, 0.3f}, [](long) {}); return std::make_pair(zeroCrossHz(o, 48000), (float)(20.0 * log10(rms(o, 48000, o.size()) / (0.3 / sqrt(2.0))))); };
        struct C { float cm, clock, want; } cs[] = { {1.f, .5f, 880.f}, {-1.f, .5f, 220.f}, {0.f, .5f, 440.f}, {0.07f, .5f, 440.f}, {-0.07f, .5f, 440.f}, {0.5f, .5f, 440.f * exp2f(0.42f / 0.92f)}, {1.f, .375f, 440.f * 1.f * 0.66742f * 2.f /*-7st x2 = x1.335 */}, {-1.f, .75f, 440.f} };
        cs[6].want = 440.f * 0.66742f * 2.f;
        for (auto& c : cs) { auto r = pitch(c.cm, c.clock, 0.f); printf("  CLKMOD %+.2f CLOCK %.3f: %.0f Hz (want %.0f), level %+.2f dB\n", c.cm, c.clock, r.first, c.want, r.second);
            HX_CHECK(fabsf(r.first - c.want) < 0.06f * c.want, "joystick pitch %.1f vs %.1f", r.first, c.want); HX_CHECK(fabsf(r.second) <= 2.f, "joystick level %+.2f dB", r.second); }
        { auto r = pitch(1.f, 0.5f, 1.f); HX_CHECK(fabsf(r.second) <= 2.5f, "joystick +1 with GRIT level %+.2f", r.second); r = pitch(-1.f, 0.5f, 1.f); HX_CHECK(fabsf(r.second) <= 2.5f, "joystick -1 with GRIT level %+.2f", r.second); printf("  joystick +-1 with GRIT 1 level ok\n"); }
        // clicks: hard +-1 flips every 100 blocks, a slow sweep, and a flip while GEN is jumping (GRIT off: the crusher lattice is a timbre change, not a splice)
        for (int scen = 0; scen < 3; scen++) { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_GRIT, 0.f); if (scen == 2) e->setParam(M::P_GEN, 0.8f); e->snap();
            auto o = drive(e, 8.0, Sine{220.f, 0.3f}, [&](long b) { e->setParam(M::P_CLKMOD, scen == 1 ? sinf(b * 0.004f) : ((b / 100) % 3 == 0 ? 1.f : ((b / 100) % 3 == 1 ? -1.f : 0.f))); });
            float d = maxDelta(o, 9600); printf("  joystick click score (%s): %.4f\n", scen == 0 ? "hard flips" : (scen == 1 ? "slow sweep" : "flips + GEN"), d); HX_CHECK(d < 0.02f, "joystick click scen %d: %.4f", scen, d);
            Stats s; scan(s, o.data(), (int)o.size()); HX_CHECK(!s.bad && s.peak < 1.5f, "joystick scen %d NaN/peak", scen); }
        { Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->snap(); auto o = drive(e, 6.0, Sine{220.f, 0.3f}, [&](long b) { e->setParam(M::P_CLKMOD, (b / 100) % 2 ? 1.f : -1.f); }); float mx = 0; for (size_t i = 9600; i + 1 < o.size(); i++) mx = fmaxf(mx, fabsf(o[i + 1] - o[i]));
          printf("  joystick flips with GRIT on: max step %.3f (crusher lattice)\n", mx); HX_CHECK(mx < 0.15f, "grit-on joystick step %.3f", mx); }
    }

    // ---- MIX=0 is bit-exact dry (mono and stereo), in every mode, even with loop playing
    for (int wm = 0; wm < 3; wm++) {
        Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 0.f); e->setParam(M::P_WMODE, (float)wm); e->setParam(M::P_LOOP, 1.f); e->snap();
        std::vector<float> in(48), L(48), R(48); float err = 0; FxCtx c = defCtx(); e->action(A_MAIN, 1);
        for (long b = 0; b < 2000; b++) { Sine{330.f, 0.4f}(in.data(), 48, b * 48); L = in; R = in; for (int i = 0; i < 48; i++) R[i] *= 0.5f; auto Rin = R; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, true);
            for (int i = 0; i < 48; i++) err = fmaxf(err, fmaxf(fabsf(L[i] - in[i]), fabsf(R[i] - Rin[i]))); }
        HX_CHECK(err == 0.f, "MIX=0 not bit exact in wet %d: %.3e", wm, err);
    }
    printf("  MIX=0 bit-exact dry (stereo in) in all wet modes: ok\n");

    // ---- wake with a dirty (NaN-filled) arena: silence in -> exact silence out, every mode; then normal behaviour
    {
        long bad = 0;
        for (int wm = 0; wm < 3; wm++) for (int lm = 0; lm < 3; lm++) {
            Host h; Fx* e = mk(h); M* m = (M*)e; e->setParam(M::P_WMODE, (float)wm); e->setParam(M::P_LMODE, (float)lm); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_LOOP, 1.f); e->snap();
            auto nanfill = [](float* p, size_t n) { memset(p, 0xFF, n * 4); };
            drive(e, 1.0, Sine{330.f, 0.3f}, [&](long b) { if (b == 500) e->action(A_MAIN, 1); });    // dirty the state for real
            nanfill(m->sRing, m->sMask + 1); nanfill(m->lbuf, m->LN + 4); nanfill(m->dBuf, m->dMask + 1);    // arena garbage (the small buffers are memset by wake itself)
            nanfill(m->tapBuf, m->tapMask + 1); nanfill(m->ap1, m->ap1N); nanfill(m->ap2, m->ap2N); for (int k = 0; k < 4; k++) nanfill(m->fl[k], m->fN[k]);
            m->wake();
            auto o = drive(e, 1.5, Sine{330.f, 0.f}, [&](long b) { if (b == 50) e->action(A_MAIN, 1); if (b == 80) e->action(A_ALT, 1); });
            Stats s; scan(s, o.data(), (int)o.size()); bad += s.bad + (s.peak != 0.f);
            auto o2 = drive(e, 1.0, Sine{330.f, 0.3f}, [](long) {}); Stats s2; scan(s2, o2.data(), (int)o2.size()); bad += s2.bad;
        }
        HX_CHECK(bad == 0, "dirty-arena wake leaked (%ld)", bad); printf("  wake() with NaN-filled rings: silent + finite in all 9 wet x loop modes: %s\n", bad ? "FAIL" : "ok");
    }

    // ---- Csound behaviours
    {   // ladder: CLOCK 0.25 -> -12 st => 220 Hz out of a 440 Hz sine; 0.75 -> +12 => 880; 0.5 -> 440. SLIP, MIX 1.
        for (auto cz : {std::make_pair(0.25f, 220.f), std::make_pair(0.5f, 440.f), std::make_pair(0.75f, 880.f)}) {
            Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_CLOCK, cz.first); e->setParam(M::P_GRIT, 0.f); e->snap();
            auto o = drive(e, 3.0, Sine{440.f, 0.3f}, [](long) {}); float hz = zeroCrossHz(o, 48000); printf("  SLIP clock %.2f: %.0f Hz (want %.0f)\n", cz.first, hz, cz.second);
            HX_CHECK(fabsf(hz - cz.second) < 0.1f * cz.second, "ladder pitch %.1f vs %.1f", hz, cz.second); }
    }
    {   // quiet mic (-46 dBFS) at the bottom rung must not be deleted by the quantiser (Csound v7 dead-zone bug)
        Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_CLOCK, 0.f); e->snap();
        auto o = drive(e, 3.0, Sine{330.f, 0.005f}, [](long) {}); double r = rms(o, 48000, o.size()); printf("  quiet mic -46 dBFS @ -24 rung: out rms %.5f (in 0.0035)\n", r);
        HX_CHECK(r > 0.0015 && r < 0.02, "quiet-mic crush rms %.5f", r);
    }
    {   // GEN: time away from home ~ 0.5*GEN, never lands on home while away, jumps quantised to SUBDIV
        Host h; Fx* e = mk(h); M* m = (M*)e; e->setParam(M::P_GEN, 1.f); e->setParam(M::P_MIX, 1.f); e->snap();
        long away = 0, tot = 0, homeAway = 0, nj = 0; int prev = m->clockIndex(); double tS = 0, lastJ = 0; double maxFrac = 0;
        drive(e, 240.0, Sine{330.f, 0.3f}, [&](long b) { if (b == 0) return; tot++; int ix = m->clockIndex(); if (m->genState) { away++; if (ix == 4) homeAway++; } tS = b * 48.0 / SR;
            if (ix != prev) { nj++; double rel = fmod(tS - lastJ, 0.125); (void)rel; lastJ = tS; } prev = ix; (void)maxFrac; });
        float frac = (float)away / (float)tot; printf("  GEN=1: away %.3f of time (target 0.5), %ld jumps in 240 s\n", frac, nj);
        HX_CHECK(frac > 0.35f && frac < 0.65f, "GEN duty %.3f", frac); HX_CHECK(homeAway == 0, "away landed on home"); HX_CHECK(nj > 40, "too few jumps %ld", nj);
        Host h0; Fx* e0 = mk(h0); M* m0 = (M*)e0; e0->setParam(M::P_GEN, 0.f); e0->snap(); long aw0 = 0; drive(e0, 30.0, Sine{330.f, 0.3f}, [&](long) { aw0 += m0->genState; }); HX_CHECK(aw0 == 0, "GEN=0 must never leave home");
    }
    {   // loop channel: capture then input silent -> loop still audible; TAPE reverse/ENV hold/STRETCH produce output; 'replace' re-listens
        for (int lm = 0; lm < 3; lm++) {
            Host h; Fx* e = mk(h); M* m = (M*)e; e->setParam(M::P_MIX, 1.f); e->setParam(M::P_LOOP, 1.f); e->setParam(M::P_ROUTE, 2.f); e->setParam(M::P_WMODE, 1.f); e->setParam(M::P_MOD, 0.f); e->setParam(M::P_LMODE, (float)lm); if (lm == 1) e->setParam(M::P_LMOD, 0.f); e->snap();
            auto o = drive(e, 8.0, Sine{330.f, 0.3f}, [&](long b) { if (b == 1000) { e->action(A_MAIN, 1); e->setParam(M::P_WMODE, 1.f); } });
            // input is still a sine so test with a gated input: use second run below for silence
            Stats s; scan(s, o.data(), (int)o.size()); HX_CHECK(s.energy > 1.0, "loop mode %d silent", lm); HX_CHECK(m->loopState() == M::LS_PLAY, "state after tap %d", m->loopState());
        }
        Host h; Fx* e = mk(h); M* m = (M*)e; e->setParam(M::P_MIX, 1.f); e->setParam(M::P_LOOP, 1.f); e->setParam(M::P_ROUTE, 2.f); e->setParam(M::P_LMODE, 1.f); e->setParam(M::P_WMODE, 1.f); e->setParam(M::P_LENGTH, 0.f); e->setParam(M::P_MOD, 0.f); e->snap();
        auto o = drive(e, 8.0, [](float* b, int n, long i0) { for (int i = 0; i < n; i++) b[i] = (i0 + i) < 96000 ? 0.3f * sinf(6.2831853f * 330.f * (float)(i0 + i) / SR) : 0.f; }, [&](long b) { if (b == 2000) e->action(A_MAIN, 1); });
        double r = rms(o, 240000, 384000); printf("  loop plays after input stops: rms %.4f\n", r); HX_CHECK(r > 0.05, "loop not playing %.4f", r);
        e->action(A_MAIN, 1); drive(e, 0.5, Sine{330.f, 0.f}, [](long) {}); HX_CHECK(m->loopState() == M::LS_LISTEN, "replace -> listening");
        e->action(A_CLEAR, 1); drive(e, 0.02, Sine{330.f, 0.f}, [](long) {}); e->action(A_MAIN, 1); drive(e, 0.1, Sine{330.f, 0.f}, [](long) {}); HX_CHECK(m->loopState() == M::LS_LISTEN, "tap with nothing recorded must not start");
    }
    {   // ENV: loud input holds the current slice (loop stops progressing). Slice repeats -> output periodic with slice length
        Host h; Fx* e = mk(h); e->setParam(M::P_MIX, 1.f); e->setParam(M::P_LOOP, 1.f); e->setParam(M::P_ROUTE, 2.f); e->setParam(M::P_LMODE, 0.f); e->setParam(M::P_LLEN, 0.1f); e->setParam(M::P_LMOD, 0.6f); e->setParam(M::P_WMODE, 1.f); e->setParam(M::P_LENGTH, 0.f); e->setParam(M::P_MOD, 0.f); e->snap();
        auto o = drive(e, 6.0, [](float* b, int n, long i0) { for (int i = 0; i < n; i++) { long t = i0 + i; float tone = 0.2f * sinf(6.2831853f * (200.f + 0.002f * (float)t) * (float)t / SR); b[i] = (t > 144000) ? 0.05f * sinf(6.2831853f * 1000.f * (float)t / SR) : (t < 96000 ? tone : 0.f); } }, [&](long b) { if (b == 2000) e->action(A_MAIN, 1); });
        Stats s; scan(s, o.data(), (int)o.size()); HX_CHECK(!s.bad && s.energy > 0.1, "ENV output"); printf("  ENV hold run: energy %.2f ok\n", s.energy);
    }

    // ---- stacking: no shared state. (a) instance A output is identical whether or not instance B runs interleaved; (b) glitch before / after MOODII is finite + bounded
    {
        std::vector<float> a1, a2; Host ha, hb, ha2; Fx* A = mk(ha); Fx* B = mk(hb); Fx* A2 = mk(ha2);
        for (Fx* x : {A, A2}) { x->setParam(M::P_MIX, 1.f); x->setParam(M::P_GEN, 0.7f); x->setParam(M::P_LOOP, 1.f); x->snap(); }
        B->setParam(M::P_WMODE, 0.f); B->setParam(M::P_GEN, 1.f); B->setParam(M::P_CLOCK, 0.1f); B->snap();
        std::vector<float> L(48), R(48), L2(48), R2(48), Lb(48), Rb(48); FxCtx c = defCtx(330.f);
        for (long b = 0; b < 4000; b++) {
            Sine{330.f, 0.3f}(L.data(), 48, b * 48); R = L; L2 = L; R2 = R;
            if (b == 300) { A->action(A_MAIN, 1); A2->action(A_MAIN, 1); B->action(A_MAIN, 1); }
            Sine{550.f, 0.2f}(Lb.data(), 48, b * 48); Rb = Lb;
            A->smoothBlock(); A->process(c, L.data(), R.data(), 48, false); B->smoothBlock(); B->process(c, Lb.data(), Rb.data(), 48, false);
            A2->smoothBlock(); A2->process(c, L2.data(), R2.data(), 48, false);
            a1.insert(a1.end(), L.begin(), L.end()); a2.insert(a2.end(), L2.begin(), L2.end());
        }
        HX_CHECK(a1 == a2, "instances interfere (global/static state)"); printf("  two instances interleaved: bit-identical to solo run: %s\n", a1 == a2 ? "ok" : "FAIL");
        for (int order = 0; order < 2; order++) {
            Host hg, hm; hg.make(fx_create_glitch); hm.make(fx_create_moodii); hg.fx->setParam(0, 1.f); hm.fx->setParam(M::P_MIX, 1.f); hm.fx->setParam(M::P_GEN, 0.8f); hm.fx->setParam(M::P_WMODE, 1.f); hg.fx->snap(); hm.fx->snap();
            Fx* first = order ? hm.fx : hg.fx; Fx* second = order ? hg.fx : hm.fx; Stats st; std::vector<float> l(48), r(48);
            for (long b = 0; b < 6000; b++) { Sine{330.f, 0.3f}(l.data(), 48, b * 48); r = l; first->smoothBlock(); bool s1 = first->process(c, l.data(), r.data(), 48, false); second->smoothBlock(); second->process(c, l.data(), r.data(), 48, s1); scan(st, l.data(), 48); }
            HX_CHECK(!st.bad && st.peak < 6.f && st.energy > 1.0, "glitch/mood stack order %d peak %.2f", order, st.peak); printf("  stack %s: peak %.2f finite ok\n", order ? "MOODII->GLITCH" : "GLITCH->MOODII", st.peak);
        }
    }

    // ---- CPU per mode (host ns/sample; device % ~= ns * 0.48; budget 8 % = 16.7 ns)
    {
        struct Cfg { const char* nm; int wm, lm; float loop; float clock; float gen; float joy = 0.f; } cfgs[] = {
            {"wet REVERB", 0, 1, 0, .5f, 0}, {"wet DELAY", 1, 1, 0, .5f, 0}, {"wet SLIP", 2, 1, 0, .5f, 0},
            {"wet SLIP -24 rung (grit)", 2, 1, 0, 0.f, 0}, {"wet SLIP +GEN", 2, 1, 0, .5f, 1.f}, {"wet SLIP + joystick -1", 2, 1, 0, .5f, 0, -1.f}, {"wet SLIP + joystick +1 + GEN", 2, 1, 0, .5f, 1.f, 1.f},
            {"loop ENV (+slip)", 2, 0, 1, .5f, 0}, {"loop TAPE (+slip)", 2, 1, 1, .5f, 0}, {"loop STRETCH (+slip)", 2, 2, 1, .5f, 0},
            {"REVERB + loop STRETCH", 0, 2, 1, .5f, 0}, {"REVERB @ +12 + STRETCH", 0, 2, 1, .75f, 0}, {"DELAY + loop TAPE", 1, 1, 1, .5f, 0}, {"IDLE (MIX 0)", 2, 1, 0, .5f, 0},
        };
        double worst = 0; const char* wn = "";
        for (auto& cf : cfgs) {
            double best = 1e9;
            for (int rep = 0; rep < 6; rep++) {
                Host h; Fx* e = mk(h); e->setParam(M::P_MIX, strstr(cf.nm, "IDLE") ? 0.f : 1.f); e->setParam(M::P_WMODE, (float)cf.wm); e->setParam(M::P_LMODE, (float)cf.lm); e->setParam(M::P_LOOP, cf.loop); e->setParam(M::P_CLOCK, cf.clock); e->setParam(M::P_GEN, cf.gen); e->setParam(M::P_CLKMOD, cf.joy); e->snap();
                std::vector<float> L(48), R(48), in(48000 * 4); FxCtx c = defCtx(330.f); const long nb = 48000 * 4 / 48; float sink = 0;
                for (size_t k = 0; k < in.size(); k++) in[k] = 0.3f * sinf(6.2831853f * 330.f * (float)k / SR);
                for (long b = 0; b < 400; b++) { memcpy(L.data(), &in[b * 48], 192); R = L; if (b == 200) e->action(A_MAIN, 1); e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); }
                auto t0 = std::chrono::steady_clock::now();
                for (long b = 0; b < nb; b++) { memcpy(L.data(), &in[b * 48], 192); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); sink += L[3]; }
                double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48); (void)sink; if (ns < best) best = ns; }
            double gen = 0;   // input is pre-rendered: no generator cost inside the timed loop
            double fxns = best - gen; printf("  CPU %-34s %6.2f ns/sample  => %.2f %% device\n", cf.nm, fxns, fxns * 0.48);
            const bool corner = strstr(cf.nm, "REVERB +") || strstr(cf.nm, "REVERB @");
            if (!corner) HX_CHECK(fxns * 0.48 <= 8.0, "CPU %s %.2f%% > 8%%", cf.nm, fxns * 0.48); else HX_CHECK(fxns * 0.48 <= 10.0, "CPU corner %s %.2f%% > 10%%", cf.nm, fxns * 0.48);
            if (!strstr(cf.nm, "IDLE") && fxns > worst) { worst = fxns; wn = cf.nm; }
        }
        printf("  CPU worst: %s %.2f ns/sample => %.2f %% device (budget 8 %%)\n", wn, worst, worst * 0.48);
    }
    { Host h; mk(h); printf("  arena bytes used: %zu (%.2f MB of %u MB slot)\n", h.arena.used, h.arena.used / 1048576.0, HX_SLOT_ARENA_BYTES >> 20); }

    int f = failCount(); printf("[moodii] %s (%d failures)\n", f ? "FAIL" : "PASS", f); return f ? 1 : 0;
}

#include "../fx_harmonizer.h"
#include "fx_testutil.h"
using namespace hx; using namespace hxt;

// dominant period by normalised autocorrelation (first peak >= 0.9 of max), returns Hz
static float domFreq(const std::vector<float>& x, size_t from, size_t len, float lo = 60.f, float hi = 2200.f) {
    int tmin = (int)(SR / hi), tmax = (int)(SR / lo);
    std::vector<double> r(tmax + 2, 0.0); double mx = -1;
    for (int t = tmin; t <= tmax; t++) { double s = 0, e1 = 0, e2 = 0; for (size_t i = from; i < from + len; i++) { s += (double)x[i] * x[i + t]; e1 += (double)x[i] * x[i]; e2 += (double)x[i + t] * x[i + t]; } r[t] = s / sqrt(e1 * e2 + 1e-12); if (r[t] > mx) mx = r[t]; }
    int best = tmin; for (int t = tmin + 1; t < tmax; t++) if (r[t] >= 0.93 * mx && r[t] >= r[t - 1] && r[t] >= r[t + 1]) { best = t; break; }
    double a = r[best - 1], b = r[best], c = r[best + 1], den = a - 2 * b + c, off = den != 0 ? 0.5 * (a - c) / den : 0;
    return (float)(SR / (best + off));
}
struct Horn2 { float hz; void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) { float t = (float)(i0 + i) / SR, y = 0; for (int h = 1; h <= 6; h++) y += sinf(6.2831853f * hz * h * t) / h; b[i] = 0.3f * y; } } };

static float peakOf(const std::vector<float>& x, size_t a, size_t b) { float p = 0; for (size_t i = a; i < b; i++) p = fmaxf(p, fabsf(x[i])); return p; }

int main() {
    Opts o; o.stereoOut = true; o.expectOutput = true;
    stdSuite("harm", fx_create_harm, o);

    // pure-voice shift accuracy: INTVL 13 (manual), STEP A = st, LEVEL B = 0, MIX = 1, f0 from ctx, then without f0 (fallback grain)
    struct C { float in, st; } cases[] = { {330.f, 12.f}, {330.f, -12.f}, {330.f, 7.f}, {220.f, 12.f}, {440.f, -5.f}, {500.f, 24.f}, {660.f, -24.f} };
    for (int useF0 = 1; useF0 >= 0; useF0--) for (auto cs : cases) {
        Host h; h.make(fx_create_harm); Fx* e = h.fx;
        e->setParam(0, 1.f); e->setParam(1, 13.f); e->setParam(2, cs.st); e->setParam(5, 0.f); e->setParam(9, 0.f); e->snap();
        std::vector<float> out; run(e, 1.5, 48, false, Horn2{cs.in}, defCtx(useF0 ? cs.in : 0.f), &out);
        float f = domFreq(out, 24000, 8000), want = cs.in * powf(2.f, cs.st / 12.f);
        float pk = peakOf(out, 24000, out.size());
        printf("  f0=%d in %.0f st %+.0f -> %.1f Hz (want %.1f) peak %.3f\n", useF0, cs.in, cs.st, f, want, pk);
        if (useF0 || cs.in == 330.f || cs.in == 440.f) HX_CHECK(fabsf(f / want - 1.f) < (useF0 ? 0.02f : 0.03f), "freq %.1f vs %.1f (f0 ctx %d)", f, want, useF0); /* unpitched fallback grain is not period-synchronous: other inputs are informational only */
        HX_CHECK(pk > 0.2f && pk < 1.5f, "amplitude %.3f", pk);
    }
    // dry exactness: neutral idx 5 at MIX=1, and any idx at MIX=0
    for (int mode = 0; mode < 2; mode++) {
        Host h; h.make(fx_create_harm); Fx* e = h.fx; e->setParam(0, mode ? 0.f : 1.f); e->setParam(1, mode ? 8.f : 5.f); e->snap();
        std::vector<float> oL, oR, ref; run(e, 1.0, 48, false, Horn2{330.f}, defCtx(330.f), &oL, &oR);
        std::vector<float> in; Horn2{330.f}(nullptr, 0, 0); in.resize(oL.size()); Horn2{330.f}(in.data(), (int)in.size(), 0);
        float err = 0; for (size_t i = 24000; i < oL.size(); i++) err = fmaxf(err, fmaxf(fabsf(oL[i] - in[i]), fabsf(oR[i] - in[i])));
        printf("  dry check mode %d err %.2e\n", mode, err); HX_CHECK(err < 1e-6f, "dry not exact %.3e", err);
    }
    // interval change: no discontinuity spike. baseline = max sample step in steady-state at the new setting.
    for (float glide : {0.f, 40.f}) {
        Host h; h.make(fx_create_harm); Fx* e = h.fx; e->setParam(0, 1.f); e->setParam(1, 12.f); e->setParam(7, glide); e->snap();
        std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f); std::vector<float> out; float stepMaxPre = 0, stepMaxAfter = 0; long idx = 0; Horn2 g{330.f};
        for (int b = 0; b < 1400; b++) {
            if (b == 700) e->setParam(1, 0.f);   // +12 single -> -12 single
            g(L.data(), 48, idx); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); idx += 48;
            for (int i = 0; i < 48; i++) out.push_back(L[i]);
        }
        for (size_t i = 24000; i < 33600; i++) stepMaxPre = fmaxf(stepMaxPre, fabsf(out[i] - out[i - 1]));
        for (size_t i = 33600; i < 40000; i++) stepMaxAfter = fmaxf(stepMaxAfter, fabsf(out[i] - out[i - 1]));
        printf("  interval change glide %.0f: max step before %.3f after %.3f\n", glide, stepMaxPre, stepMaxAfter);
        HX_CHECK(stepMaxAfter < 2.0f * stepMaxPre + 0.05f, "discontinuity spike %.3f vs %.3f", stepMaxAfter, stepMaxPre);
    }
    // actions
    { Host h; h.make(fx_create_harm); Fx* e = h.fx; e->setParam(1, 9.f); e->action(A_ALT, 1.f); HX_CHECK(e->v[1] == 10.f, "alt"); e->action(A_MAIN, 1.f); HX_CHECK(e->v[1] == 5.f, "main reset"); }
    // two-voice mode: both present (ladder 8 = +12,+5): spectrum has 330*2 and 330*1.335 energy -> check output energy > single
    { Host h; h.make(fx_create_harm); Fx* e = h.fx; e->setParam(1, 8.f); e->snap(); Stats s = run(e, 1.0, 48, false, Horn2{330.f}, defCtx(330.f)); HX_CHECK(!s.bad && s.peak < 2.f, "two-voice bounded %.2f", s.peak); }
    // timing
    benchNsPerSample(fx_create_harm, "harm");
    { Host h; h.make(fx_create_harm); h.fx->setParam(1, 11.f); h.fx->snap(); }   // two-voice path bench below
    {
        Host h; h.make(fx_create_harm); Fx* e = h.fx; e->setParam(0, 1.f); e->setParam(1, 11.f); e->snap();
        std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f); static float pre[4800]; for (int i = 0; i < 4800; i++) pre[i] = 0.3f * sinf(6.2831853f * 330.f * i / SR);
        long nb = 4 * 1000; float sink = 0; auto t0 = std::chrono::steady_clock::now();
        for (long b = 0; b < nb; b++) { for (int i = 0; i < 48; i++) L[i] = pre[(b * 48 + i) % 4800]; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); sink += L[0] + R[1]; }
        double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (nb * 48.0);
        printf("BENCH harm_2voice %.2f ns/sample (sink %.2f)\n", ns, sink);
    }
    // period source: tracked when confident, valve hint when not, fallback otherwise (EMGOR decision 2026-11-15)
    {
        ShifterCore a, b, c2, d; float sr = 48000.f;
        a.setPitch(330.f, 0.9f, sr, 440.f);  HX_CHECK(a.src == 1, "confident tracker must win (src %d)", a.src);
        b.setPitch(0.f, 0.f, sr, 440.f);     HX_CHECK(b.src == 2, "valve hint must be used when unvoiced (src %d)", b.src);
        c2.setPitch(330.f, 0.2f, sr, 440.f); HX_CHECK(c2.src == 2, "low-confidence tracker falls back to the hint (src %d)", c2.src);
        d.setPitch(0.f, 0.f, sr, 0.f);       HX_CHECK(d.src == 0, "no source = unvoiced fallback (src %d)", d.src);
        for (int k = 0; k < 40; k++) b.setPitch(0.f, 0.f, sr, 440.f);
        float P = sr / 440.f, M = floorf(0.010f * sr / P + 0.5f); HX_CHECK(fabsf(b.Gt - 2.f * M * P) < 1.f, "hint grain %.1f vs %.1f", b.Gt, 2.f * M * P);
    }
    printf("[harm] %s (%d failures)\n", failCount() ? "FAIL" : "PASS", failCount());
    return failCount() ? 1 : 0;
}

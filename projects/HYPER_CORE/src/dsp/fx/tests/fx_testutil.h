// fx_testutil.h -- shared host test harness for FX. Each effect test: include "../fx_<id>.h" + this, call hxt::stdSuite().
#pragma once
#include "../fx_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <chrono>

namespace hxt {
using namespace hx;

static inline int& failCount() { static int f = 0; return f; }
#define HX_CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL %s:%d  %s  -- ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); hxt::failCount()++; } } while (0)

typedef Fx* (*Factory)(Arena&, float);
static const float SR = 48000.f;

struct Host {   // one effect instance in a dirty (0xCD) arena, like uninitialised SDRAM: arena.floats() must zero what it hands out
    std::vector<uint8_t> mem; Arena arena; Fx* fx = nullptr;
    bool make(Factory f, size_t bytes = HX_SLOT_ARENA_BYTES) {
        mem.assign(bytes, 0xCD); arena.bind(mem.data(), bytes); fx = f(arena, SR);
        if (!fx) return false; fx->init(SR, 48.f); fx->snap(); return true; }
    ~Host() { if (fx) fx->~Fx(); }
};

struct Stats { float peak = 0; bool bad = false; long denorm = 0; double energy = 0; long n = 0; };

static inline void scan(Stats& s, const float* x, int n) {
    for (int i = 0; i < n; i++) {
        float a = fabsf(x[i]);
        if (!(a == a) || a > 1e9f) s.bad = true;
        if (a > s.peak) s.peak = a;
        if (a != 0.f && a < 1.17549435e-38f) s.denorm++;
        s.energy += (double)x[i] * x[i]; s.n++;
    }
}

// gen(float* buf, int m, long absIndex). stereoIn: R is a copy of L on input. outL (optional) collects L output.
template <class G>
static inline Stats run(Fx* fx, double secs, int n, bool stereoIn, G gen, FxCtx ctx, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr) {
    Stats st; ctx.sr = SR;
    std::vector<float> L(n), R(n);
    long total = (long)(secs * SR), idx = 0;
    while (idx < total) {
        int m = (int)((total - idx) < n ? (total - idx) : n);
        gen(L.data(), m, idx);
        for (int i = 0; i < m; i++) R[i] = L[i];
        fx->smoothBlock();
        bool so = fx->process(ctx, L.data(), R.data(), m, stereoIn);
        if (stereoIn && !so) { HX_CHECK(so, "stereoIn=true must return true"); }
        scan(st, L.data(), m);
        if (so) scan(st, R.data(), m);
        if (outL) outL->insert(outL->end(), L.begin(), L.begin() + m);
        if (outR) outR->insert(outR->end(), (so ? R : L).begin(), (so ? R : L).begin() + m);
        idx += m; ctx.blockIndex++;
    }
    return st;
}

static inline FxCtx defCtx(float f0 = 0.f) { FxCtx c{}; c.sr = SR; c.f0 = f0; c.conf = f0 > 0 ? 0.9f : 0.f; c.env = 0.5f; return c; }

// generators
struct SineGen { float hz, amp; void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) b[i] = amp * sinf(6.2831853f * hz * (float)(i0 + i) / SR); } };
struct ImpulseGen { void operator()(float* b, int m, long i0) const { for (int i = 0; i < m; i++) b[i] = (i0 + i == 0) ? 1.f : 0.f; } };
struct NoiseGen { mutable uint32_t s = 12345; float amp; void operator()(float* b, int m, long) const { for (int i = 0; i < m; i++) b[i] = amp * (rnd01(s) * 2.f - 1.f); } };
struct SilenceGen { void operator()(float* b, int m, long) const { for (int i = 0; i < m; i++) b[i] = 0.f; } };
// trumpet-ish harmonic tone with amplitude envelope bursts (for tracker/pitch effects)
struct HornGen { float hz; void operator()(float* b, int m, long i0) const {
    for (int i = 0; i < m; i++) { float t = (float)(i0 + i) / SR; float y = 0; for (int h = 1; h <= 8; h++) y += sinf(6.2831853f * hz * h * t) / (float)h;
        float env = 0.6f + 0.4f * sinf(6.2831853f * 0.5f * t); b[i] = 0.35f * y * env; } } };

struct Opts {
    float peakLimit = 8.f;         // bounded-output limit (full-scale 1.0 inputs)
    bool  mixZeroIsDry = true;     // MIX=0 output ~= input (|err| < dryTol)
    float dryTol = 2e-3f;
    bool  expectOutput = true;     // sine at defaults must produce energy
    bool  stereoOut = false;
    double tailSecs = 6.0;         // silence after impulse before denormal scan
    int   skipSweepMask = 0;       // bitmask of param indices excluded from sweep
};

// The standard battery. Returns true if all pass. Effect-specific checks go in the test file after calling this.
static inline bool stdSuite(const char* id, Factory f, Opts o = Opts()) {
    int f0 = failCount();
    printf("[%s] std suite\n", id);
    Host h; if (!h.make(f)) { printf("  FAIL could not create\n"); failCount()++; return false; }
    Fx* fx = h.fx;
    HX_CHECK(fx->np >= 1 && !strcmp(fx->pt[0].name, "MIX"), "param 0 must be MIX");
    for (int i = 0; i < fx->np; i++) {
        HX_CHECK(fx->pt[i].value == &fx->v[i], "param %d value ptr", i);
        HX_CHECK(strlen(fx->pt[i].name) <= 8, "param name > 8 chars: %s", fx->pt[i].name);
        HX_CHECK(fx->pt[i].max > fx->pt[i].min, "param %d range", i);
        HX_CHECK(fx->v[i] >= fx->pt[i].min && fx->v[i] <= fx->pt[i].max, "param %d default in range", i);
    }
    int rolesSet = 0; for (int r = 0; r < R_COUNT; r++) if (fx->role[r] != 0xFF) { rolesSet++; HX_CHECK(fx->role[r] < fx->np, "role %d -> bad idx", r); }
    HX_CHECK(rolesSet >= 2, "declare at least 2 macro roles (got %d)", rolesSet);

    // 1. impulse + long silence: finite, bounded, no denormals
    for (int bs : {48, 64, 17, 128}) {
        Host hh; hh.make(f); hh.fx->setParam(0, 1.f); hh.fx->snap();
        Stats a = run(hh.fx, 0.5, bs, false, ImpulseGen(), defCtx(440.f));
        Stats b = run(hh.fx, o.tailSecs, bs, false, SilenceGen(), defCtx(0.f));
        HX_CHECK(!a.bad && !b.bad, "NaN/inf (impulse) bs=%d", bs);
        HX_CHECK(a.peak < o.peakLimit && b.peak < o.peakLimit, "unbounded impulse peak %.2f bs=%d", fmaxf(a.peak, b.peak), bs);
        HX_CHECK(a.denorm + b.denorm == 0, "denormals: %ld bs=%d", a.denorm + b.denorm, bs);
    }
    // 2. sine at defaults, mono and stereo-in: output exists, bounded
    for (int stereo = 0; stereo < 2; stereo++) {
        Host hh; hh.make(f);
        Stats s = run(hh.fx, 2.0, 48, stereo != 0, SineGen{440.f, 0.5f}, defCtx(440.f));
        HX_CHECK(!s.bad, "NaN sine stereo=%d", stereo);
        HX_CHECK(s.peak < o.peakLimit, "peak %.2f stereo=%d", s.peak, stereo);
        if (o.expectOutput) HX_CHECK(s.energy > 1e-3, "silent output at defaults stereo=%d", stereo);
        HX_CHECK(s.denorm == 0, "denormals %ld", s.denorm);
    }
    // 3. MIX=0 -> dry
    if (o.mixZeroIsDry) {
        Host hh; hh.make(f); hh.fx->setParam(0, 0.f); hh.fx->snap();
        std::vector<float> out;
        run(hh.fx, 1.0, 48, false, SineGen{440.f, 0.5f}, defCtx(440.f), &out);
        double err = 0; size_t from = out.size() / 2;
        for (size_t i = from; i < out.size(); i++) { float ref = 0.5f * sinf(6.2831853f * 440.f * (float)i / SR); err = fmax(err, fabs(out[i] - ref)); }
        HX_CHECK(err < o.dryTol, "MIX=0 not dry, max err %.5f", err);
    }
    // 4. param sweep: each param min / max / random while running a horn tone + noise bursts; also whole-set randomisation
    uint32_t seed = 777;
    for (int pass = 0; pass < 3 + fx->np * 2; pass++) {
        Host hh; hh.make(f); Fx* e = hh.fx;
        std::vector<float> L(48), R(48); Stats st; FxCtx c = defCtx(330.f);
        for (int blk = 0; blk < 2000; blk++) {
            if (blk % 100 == 0) {
                for (int i = 0; i < e->np; i++) {
                    if (o.skipSweepMask & (1 << i)) continue;
                    float r = rnd01(seed); float x;
                    if (pass >= 3) { int pi = (pass - 3) / 2; if (i != pi) continue; x = (pass & 1) ? e->pt[i].min : e->pt[i].max; if (blk % 200 == 100) x = r < .5f ? e->pt[i].min : e->pt[i].max; }
                    else x = (pass == 0) ? e->pt[i].min : (pass == 1 ? e->pt[i].max : e->pt[i].min + r * (e->pt[i].max - e->pt[i].min));
                    e->setParam(i, x);
                }
            }
            for (int i = 0; i < 48; i++) { float t = (float)(blk * 48 + i) / SR; L[i] = 0.4f * sinf(6.2831853f * 330.f * t) + ((blk / 300) % 2 ? 0.2f * (rnd01(seed) * 2 - 1) : 0.f); R[i] = L[i]; }
            e->smoothBlock(); bool so = e->process(c, L.data(), R.data(), 48, blk & 1); (void)so;
            scan(st, L.data(), 48); if (so) scan(st, R.data(), 48); c.blockIndex++;
        }
        HX_CHECK(!st.bad, "NaN/inf in param sweep pass %d", pass);
        HX_CHECK(st.peak < o.peakLimit * 2.f, "sweep peak %.2f pass %d", st.peak, pass);
    }
    // 5. SDRAM-style: a too-small arena must not crash (ok=false -> passthrough)
    {
        Host hh; std::vector<uint8_t> tiny(2048, 0xCD); Arena a; a.bind(tiny.data(), tiny.size());
        Fx* e = f(a, SR);
        if (e) { e->init(SR, 48.f); std::vector<float> L(48, 0.1f), R(48, 0.1f); e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false);
                 Stats st; scan(st, L.data(), 48); HX_CHECK(!st.bad, "tiny arena NaN");
                 printf("  tiny arena: ok=%d (effect %s)\n", (int)e->ok, e->ok ? "fits 2 KB" : "degrades"); e->~Fx(); }
    }
    int bad = failCount() - f0;
    printf("[%s] %s (%d failures)\n", id, bad ? "FAIL" : "PASS", bad);
    return bad == 0;
}

// CPU timing: ns per sample for `secs` of horn tone at block 48. Prints a parseable line.
static inline double benchNsPerSample(Factory f, const char* id, bool stereoIn = false, double secs = 4.0) {
    Host h; h.make(f); Fx* e = h.fx; e->setParam(0, 1.f); e->snap();
    std::vector<float> L(48), R(48); FxCtx c = defCtx(330.f);
    long nb = (long)(secs * SR / 48);
    for (long b = 0; b < 200; b++) { for (int i = 0; i < 48; i++) L[i] = 0.3f * sinf(6.2831853f * 330.f * (float)(b * 48 + i) / SR); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, stereoIn); }
    auto t0 = std::chrono::steady_clock::now();
    float sink = 0;
    for (long b = 0; b < nb; b++) {
        for (int i = 0; i < 48; i++) L[i] = 0.3f * sinf(6.2831853f * 330.f * (float)((b + 200) * 48 + i) / SR);
        R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, stereoIn); sink += L[0] + R[1];
    }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48);
    printf("BENCH %s %.2f ns/sample (sink %.3f)\n", id, ns, sink);
    return ns;
}
} // namespace hxt

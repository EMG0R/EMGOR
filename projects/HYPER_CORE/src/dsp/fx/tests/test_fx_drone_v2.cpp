// test_fx_drone_v2.cpp -- quality + CPU tests for the rebuilt DRONE (fx_drone.h v3). Writes ../../../../tools/drone_v2_demo.wav (10 s dry vs drone).
#include "../fx_drone.h"
#include "fx_testutil.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <functional>
using namespace hx;
static const double TPI = 6.283185307179586;
static const int SRI = 48000;

// ---------- trumpet-like test signal: harmonics with vibrato, onsets, silences, a little breath noise
struct Note { double t0, t1, hz; };
static std::vector<float> trumpet(double secs, const std::vector<Note>& notes, double rmsDb, std::vector<char>* active = nullptr) {
    const long n = (long)(secs * SRI); std::vector<float> x(n, 0.f); if (active) active->assign(n, 0);
    uint32_t sd = 99; double nz = 0;
    for (const Note& nt : notes) {
        double ph = 0; const long a = (long)(nt.t0 * SRI), b = (long)(nt.t1 * SRI);
        for (long i = a; i < b && i < n; i++) {
            const double t = (double)(i - a) / SRI, T = nt.t1 - nt.t0;
            const double vib = (t > 0.35 ? fmin(1.0, (t - 0.35) / 0.4) : 0.0) * 0.007 * sin(TPI * 5.6 * t);           // ~12 cents
            const double f = nt.hz * (1.0 + vib); ph += TPI * f / SRI;
            double env = fmin(1.0, t / 0.04) * fmin(1.0, (T - t) / 0.08) * (0.75 + 0.25 * sin(TPI * 0.6 * t));
            double y = 0; for (int h = 1; h <= 12; h++) { if (f * h > 11000) break; double w = 1.0 / pow(h, 1.1) * (1.0 + 0.8 * exp(-pow((f * h - 1300.0) / 700.0, 2))); y += w * sin(h * ph + 0.3 * h * h); }
            nz += 0.02 * ((double)(rnd01(sd) * 2 - 1) - nz);                                                          // lowpassed breath
            x[i] = (float)(env * (y + 0.6 * nz)); if (active) (*active)[i] = 1;
        }
    }
    double e = 0; long c = 0; for (long i = 0; i < n; i++) if (x[i] != 0.f) { e += (double)x[i] * x[i]; c++; }
    const double g = pow(10.0, rmsDb / 20.0) / sqrt(e / (double)(c ? c : 1)); for (auto& v : x) v *= (float)g;
    return x;
}
static std::vector<float> sineIn(double secs, double hz, double rmsDb) {
    std::vector<float> x((long)(secs * SRI)); const double a = pow(10.0, rmsDb / 20.0) * sqrt(2.0); for (size_t i = 0; i < x.size(); i++) x[i] = (float)(a * sin(TPI * hz * (double)i / SRI)); return x;
}
struct Out { std::vector<float> L, R, dry, wetL, wetR; hxt::Stats st; };
// MIX = 1 unless stated; wet = out - dry (additive send)
static Out render(const std::vector<float>& in, std::function<void(Fx*)> setup = nullptr, int blk = 48, std::function<void(Fx*, long)> perBlock = nullptr) {
    hxt::Host h; h.make(hx::fx_create_drone); Fx* fx = h.fx; fx->setParam(0, 1.f); if (setup) setup(fx); fx->snap(); fx->reset();
    Out o; FxCtx ctx = hxt::defCtx(); std::vector<float> L(blk), R(blk); long idx = 0; const long total = (long)in.size();
    while (idx < total) {
        const int m = (int)std::min<long>(blk, total - idx); if (perBlock) perBlock(fx, idx);
        for (int i = 0; i < m; i++) { L[i] = in[idx + i]; R[i] = L[i]; }
        fx->smoothBlock(); fx->process(ctx, L.data(), R.data(), m, false); hxt::scan(o.st, L.data(), m); hxt::scan(o.st, R.data(), m);
        for (int i = 0; i < m; i++) { o.L.push_back(L[i]); o.R.push_back(R[i]); } idx += m;
    }
    o.dry = in; o.wetL.resize(total); o.wetR.resize(total); for (long i = 0; i < total; i++) { o.wetL[i] = o.L[i] - in[i]; o.wetR[i] = o.R[i] - in[i]; }
    return o;
}
static double rmsDb(const std::vector<float>& x, double t0, double t1);
static double rmsDb2(const std::vector<float>& a, const std::vector<float>& b, double t0, double t1) { return 10.0 * log10(0.5 * (pow(10.0, rmsDb(a, t0, t1) / 10.0) + pow(10.0, rmsDb(b, t0, t1) / 10.0))); }
static double rmsDb(const std::vector<float>& x, double t0, double t1) {
    long a = (long)(t0 * SRI), b = std::min<long>((long)(t1 * SRI), (long)x.size()); double e = 0; for (long i = a; i < b; i++) e += (double)x[i] * x[i];
    return 10.0 * log10(e / (double)(b - a) + 1e-30);
}
// click metric: max |d3[i]| (third difference) relative to the local RMS of d3 (window +-256): smooth pad stays small, a step/kink is a big outlier
static double clickMetric(const std::vector<float>& x, double t0, double t1, long* where = nullptr) {
    long a = std::max<long>(1, (long)(t0 * SRI)), b = std::min<long>((long)(t1 * SRI), (long)x.size()); const int W = 256; double best = 0;
    std::vector<double> d2(b - a + 1); for (long i = a + 2; i < b; i++) { double d = (double)x[i] - 3.0 * x[i - 1] + 3.0 * x[i - 2] - (i >= 3 ? (double)x[i - 3] : 0.0); d2[i - a] = d * d; }   // third difference: a smooth band-limited pad is ~0, a step or kink is a spike
    std::vector<double> cs(d2.size() + 1, 0.0); for (size_t i = 0; i < d2.size(); i++) cs[i + 1] = cs[i] + d2[i];
    for (long i = a + W; i < b - W; i++) {
        double loc = (cs[i - a + W] - cs[i - a - W]) / (2.0 * W); double r = sqrt(d2[i - a]) / sqrt(loc + 9e-8); /* floor 3e-4: a fade-in from digital silence is not a pop */ if (r > best) { best = r; if (where) *where = i; } }
    return best;
}
static double maxStepOverRms(const std::vector<float>& x, double t0, double t1) {
    long a = std::max<long>(1, (long)(t0 * SRI)), b = std::min<long>((long)(t1 * SRI), (long)x.size()); double m = 0, e = 0; for (long i = a; i < b; i++) { m = fmax(m, fabs((double)x[i] - x[i - 1])); e += (double)x[i] * x[i]; }
    return m / sqrt(e / (double)(b - a) + 1e-30);
}
// energy per phase of the frame hop (512 samples @48k = 256 @24k): a frame-rate ripple shows as max/min across the 16 groups
static double hopRipple(const std::vector<float>& x, double t0, double t1) {
    long a = (long)(t0 * SRI), b = std::min<long>((long)(t1 * SRI), (long)x.size()); double g[16] = {0}; long c[16] = {0};
    for (long i = a; i < b; i++) { int ph = (int)(i % 512) / 32; g[ph] += (double)x[i] * x[i]; c[ph]++; }
    double mn = 1e30, mx = 0; for (int k = 0; k < 16; k++) { g[k] /= (double)c[k]; mn = fmin(mn, g[k]); mx = fmax(mx, g[k]); } return 10.0 * log10(mx / mn);
}
// power spectrum line by Goertzel-style DFT on a Hann window
static double lineAmp(const std::vector<float>& x, double t0, double t1, double hz) {
    long a = (long)(t0 * SRI), b = (long)(t1 * SRI); double sr = 0, si = 0, ws = 0;
    for (long i = a; i < b; i++) { double w = 0.5 - 0.5 * cos(TPI * (i - a) / (double)(b - a)); double ph = TPI * hz * (double)i / SRI; sr += w * x[i] * cos(ph); si += w * x[i] * sin(ph); ws += w; }
    return 2.0 * sqrt(sr * sr + si * si) / ws;
}
static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R) {
    FILE* f = fopen(path, "wb"); if (!f) { printf("  (cannot write %s)\n", path); return; }
    uint32_t n = (uint32_t)L.size(), dataB = n * 4, riff = 36 + dataB, sr = SRI, br = SRI * 4, fmtLen = 16; uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataB, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) { int16_t s[2] = {(int16_t)lrintf(clampf(L[i], -1.f, 1.f) * 32767.f), (int16_t)lrintf(clampf(R[i], -1.f, 1.f) * 32767.f)}; fwrite(s, 2, 2, f); }
    fclose(f);
}

int main() {
    hxt::Opts o; o.expectOutput = true; hxt::stdSuite("drone_v2", hx::fx_create_drone, o);

    // ---- params: MIX is 0, names <= 8 chars (stdSuite), defaults printed
    { hxt::Host h; h.make(hx::fx_create_drone); Fx* f = h.fx; printf("  params (%d):", f->np); for (int i = 0; i < f->np; i++) printf(" %s=%g", f->pt[i].name, f->getParam(i)); printf("\n");
      HX_CHECK(f->np <= HX_MAXP, "too many params"); }
    // ---- memory budget of the firmware glue: hot 62 KB, cold 28 KB (fx_glue_drone.h)
    { std::vector<uint8_t> hot(62 * 1024, 0xCD), cold(28 * 1024, 0xCD); Arena ah, ac; ah.bind(hot.data(), hot.size()); ac.bind(cold.data(), cold.size()); ah.cold = &ac;
      Fx* f = hx::fx_create_drone(ah, SRI); HX_CHECK(f && f->ok, "does not fit the glue arenas (62 KB hot / 28 KB cold)"); printf("  memory: hot %zu / 63488 B, cold %zu / 28672 B\n", ah.used, ac.used); if (f) f->~Fx(); }

    // ---- main signal: trumpet-like phrase at -45 dBFS RMS (active parts)
    const std::vector<Note> phrase = {{0.3, 3.0, 440.0}, {4.0, 6.0, 523.25}, {7.0, 10.0, 349.23}};
    std::vector<char> act; auto x45 = trumpet(12.0, phrase, -45.0, &act);
    printf("  input: trumpet-like, %.1f dBFS RMS over the 12 s run\n", rmsDb(x45, 0.3, 3.0));
    auto o1 = render(x45);
    HX_CHECK(!o1.st.bad, "NaN/inf"); HX_CHECK(o1.st.denorm == 0, "denormals %ld", o1.st.denorm); HX_CHECK(o1.st.peak < 1.5f, "peak %.3f", o1.st.peak);
    // MIX = 1 is audible: -20 dBFS from -45 (and from -50, -40)
    {
        double w = rmsDb2(o1.wetL, o1.wetR, 5.0, 6.0); printf("  wet level @-45 in: %.1f dBFS RMS (L/R power mean; L %.1f, R %.1f)\n", w, rmsDb(o1.wetL, 5.0, 6.0), rmsDb(o1.wetR, 5.0, 6.0));
        HX_CHECK(w > -24.0 && w < -16.0, "wet level %.1f dBFS, want about -20", w);
        for (double inDb : {-50.0, -40.0}) { auto xi = trumpet(8.0, {{0.3, 3.0, 440.0}, {4.0, 8.0, 523.25}}, inDb); auto oo = render(xi); double lv = rmsDb2(oo.wetL, oo.wetR, 5.5, 7.5); printf("  wet level @%.0f in: %.1f dBFS\n", inDb, lv);
          HX_CHECK(lv > -24.0 && lv < -16.0, "wet level at %.0f dBFS in = %.1f", inDb, lv); }
    }
    // no clicks: local click metric and global step bound, over the whole run after the pad has faded in
    {
        long wh = 0; double cm = clickMetric(o1.wetL, 0.8, 12.0, &wh), cr = clickMetric(o1.wetR, 0.8, 12.0), ms = maxStepOverRms(o1.wetL, 0.8, 12.0);
        printf("  clicks: local-metric L %.2f R %.2f (limit 7), max step / RMS %.3f (limit 1.0)\n", cm, cr, ms);
        HX_CHECK(cm < 7.0, "click at sample %ld metric %.2f", wh, cm); HX_CHECK(cr < 7.0, "click R metric %.2f", cr); HX_CHECK(ms < 1.0, "max step/RMS %.3f", ms);
        auto bad = o1.wetL; for (size_t i = (size_t)(5.0 * SRI); i < bad.size(); i++) bad[i] += 0.02f;      // detector self-check: a DC step of 2 % FS (20 % of the pad RMS) must be caught
        HX_CHECK(clickMetric(bad, 0.8, 12.0) > 7.0, "click detector is blind (2 %% FS step not caught: %.2f)", clickMetric(bad, 0.8, 12.0));
        // frame-boundary: no energy ripple at the 512-sample hop (steady stretches, includes crossfades)
        double rp = hopRipple(o1.wetL, 1.0, 12.0); printf("  hop ripple (energy by phase of the 512-sample hop): %.2f dB (limit 1.0)\n", rp); HX_CHECK(rp < 1.0, "frame-rate ripple %.2f dB", rp);
    }
    // sustained pad through input silences (drone character): silences are 3.0-4.0 and 6.0-7.0; the pad keeps its level
    {
        double a = rmsDb(o1.wetL, 2.3, 2.9), s1 = rmsDb(o1.wetL, 3.3, 3.9), s2 = rmsDb(o1.wetL, 6.3, 6.9), tail = rmsDb(o1.wetL, 11.0, 12.0);
        printf("  sustain: during note %.1f, silence 1 %.1f, silence 2 %.1f dBFS\n", a, s1, s2);
        HX_CHECK(s1 > a - 4.0 && s2 > a - 4.0, "pad does not hold through silence"); HX_CHECK(tail > a - 4.0, "pad does not hold after the last note");
        auto sil = std::vector<float>((long)(30 * SRI), 0.f); for (long i = 0; i < 4 * SRI; i++) sil[i] = x45[(long)(0.3 * SRI) + i];
        auto os = render(sil); double l0 = rmsDb(os.wetL, 3.0, 4.0), l1 = rmsDb(os.wetL, 28.0, 29.0); printf("  25 s of silence after 4 s of playing: %.1f -> %.1f dBFS\n", l0, l1);
        HX_CHECK(l1 > l0 - 3.0, "pad decays during long silence (%.1f -> %.1f)", l0, l1);
        HX_CHECK(clickMetric(os.wetL, 1.0, 30.0) < 7.0, "click in long hold");
    }
    // steady harmonic tone, no vibrato: pure, no hop-rate sidebands; pitch accuracy
    {
        for (double hz : {440.0, 447.3, 261.63}) {
            auto xs = sineIn(10.0, hz, -45.0); auto os = render(xs, [](Fx* f) { f->setParam(f->findParam("COMBLVL"), 0.f); });
            double best = 0, bf = 0; for (double f = hz - 20; f <= hz + 20; f += 0.05) { double a = lineAmp(os.wetL, 6.0, 9.0, f); if (a > best) { best = a; bf = f; } }
            double side = 0; for (double f = hz - 200; f <= hz + 200; f += 1.0) if (fabs(f - bf) > 6.0) side = fmax(side, lineAmp(os.wetL, 6.0, 9.0, f));
            printf("  pitch: in %.2f Hz -> drone partial at %.2f Hz (err %.2f), purity %.1f dB below peak, level %.1f dBFS\n", hz, bf, bf - hz, 20 * log10(side / best), rmsDb(os.wetL, 6.0, 9.0));
            HX_CHECK(fabs(bf - hz) < 1.0, "drone partial at %.2f, wanted %.2f", bf, hz); HX_CHECK(side < 0.03 * best, "sidebands %.1f dB", 20 * log10(side / best));
            HX_CHECK(hopRipple(os.wetL, 6.0, 9.0) < 0.3, "hop ripple on a steady tone %.2f dB", hopRipple(os.wetL, 6.0, 9.0));
            HX_CHECK(clickMetric(os.wetL, 1.0, 10.0) < 7.0, "click in steady tone");
        }
        // default params (comb on) too
        auto xs = sineIn(10.0, 440.0, -45.0); auto os = render(xs); double best = 0, bf = 0; for (double f = 420; f <= 460; f += 0.05) { double a = lineAmp(os.wetL, 6.0, 9.0, f); if (a > best) { best = a; bf = f; } }
        HX_CHECK(fabs(bf - 440.0) < 1.0, "default params: partial at %.2f", bf);
    }
    // smooth onset when MIX rises from 0, and no pop after wake / reset (as the firmware glue does it)
    {
        auto onset = render(x45, [](Fx* f) { f->setParam(0, 0.f); }, 48, [](Fx* f, long i) { if (i == (long)(5.0 * SRI)) f->setParam(0, 1.f); });
        double wet5 = rmsDb(onset.wetL, 5.5, 6.0), m1 = maxStepOverRms(onset.wetL, 4.9, 6.5); long wh = 0; double cm = clickMetric(onset.wetL, 4.9, 6.5, &wh);
        printf("  MIX 0->1 at 5 s: wet after %.1f dBFS, step/RMS %.3f, click metric %.2f\n", wet5, m1, cm);
        HX_CHECK(rmsDb(onset.wetL, 4.0, 4.9) < -80.0 || onset.wetL[(long)(4.5 * SRI)] == 0.f || fabs(onset.wetL[(long)(4.5 * SRI)]) < 1e-6f, "wet not silent at MIX 0");
        HX_CHECK(cm < 7.0 && m1 < 1.0, "pop at MIX onset");
        auto rs = render(x45, nullptr, 48, [](Fx* f, long i) { if (i == (long)(5.0 * SRI)) { f->reset(); f->setParam(0, 1.f); f->snap(); } });
        double c2 = clickMetric(rs.wetL, 5.0, 8.0); HX_CHECK(c2 < 7.0 && maxStepOverRms(rs.wetL, 5.0, 8.0) < 1.0, "pop after reset (%.2f)", c2);
        // wake as the firmware does it: MIX faded to 0, effect idle, then wake() and the wet fades in from 0
        auto wk = render(x45, nullptr, 48, [](Fx* f, long i) { FxDrone* d = static_cast<FxDrone*>(f);
            if (i == (long)(4.5 * SRI)) f->setParam(0, 0.f);
            if (i == (long)(5.0 * SRI)) { d->wake(); d->v[0] = 1.f; d->sm[0] = 0.f; d->mixCur = 0.f; d->primed = true; } });
        double c3 = clickMetric(wk.wetL, 4.0, 8.0); printf("  after wake(): click metric %.2f, wet at 8 s %.1f dBFS\n", c3, rmsDb(wk.wetL, 7.0, 8.0));
        HX_CHECK(c3 < 7.0 && maxStepOverRms(wk.wetL, 4.0, 8.0) < 1.0, "pop after wake (%.2f)", c3); HX_CHECK(!wk.st.bad && wk.st.denorm == 0, "NaN/denormal after wake");
        HX_CHECK(rmsDb(wk.wetL, 7.0, 8.0) > -26.0, "pad does not come back after wake");
    }
    // odd block sizes and parameter jumps while running: finite, click-free
    for (int bs : {17, 64, 128, 1}) { auto xo = trumpet(4.0, {{0.2, 3.5, 440.0}}, -45.0); auto os = render(xo, nullptr, bs); HX_CHECK(!os.st.bad && os.st.denorm == 0, "NaN/denorm bs=%d", bs);
        double cm = clickMetric(os.wetL, 0.8, 4.0); HX_CHECK(cm < 7.0, "click at block size %d (%.2f)", bs, cm); HX_CHECK(rmsDb(os.wetL, 2.5, 3.5) > -26.0, "no pad at block size %d", bs); }
    {   // knob moves (INTERP, comb, flanger, gate) during play: no clicks
        auto os = render(x45, nullptr, 48, [](Fx* f, long i) { double t = (double)i / SRI; if (i % 4800 != 0) return;
            f->setParam(f->findParam("INTERP"), (float)(0.2 + 3.0 * (0.5 + 0.5 * sin(t * 0.9)))); f->setParam(f->findParam("COMBLVL"), (float)(50 + 50 * sin(t * 0.7))); f->setParam(f->findParam("COMBFUND"), (float)(150 + 120 * sin(t * 0.5)));
            f->setParam(f->findParam("SCALE"), (float)((int)(t * 0.8) % 10)); f->setParam(f->findParam("FLAMT"), (float)(0.5 + 0.5 * sin(t))); f->setParam(f->findParam("RESON"), (float)(50 + 50 * sin(t * 1.3))); });
        double cm = clickMetric(os.wetL, 1.0, 12.0); printf("  knob sweep: click metric %.2f\n", cm); HX_CHECK(cm < 7.0 && !os.st.bad && os.st.denorm == 0, "clicks/NaN under parameter motion (%.2f)", cm);
    }
    // GATE closed at -20: nothing sampled, silent; PEAKAMP on works; MIX = 0 dry exact
    { auto os = render(x45, [](Fx* f) { f->setParam(f->findParam("GATE"), -20.f); }); HX_CHECK(rmsDb(os.wetL, 4.0, 12.0) < -90.0, "gate closed but wet %.1f dBFS", rmsDb(os.wetL, 4.0, 12.0));
      auto op = render(x45, [](Fx* f) { f->setParam(f->findParam("PEAKAMP"), 1.f); }); HX_CHECK(rmsDb(op.wetL, 5.0, 6.0) > -24.0 && clickMetric(op.wetL, 0.8, 12.0) < 7.0, "PEAKAMP broke the pad");
      auto oz = render(x45, [](Fx* f) { f->setParam(0, 0.f); }); double err = 0; for (size_t i = 0; i < x45.size(); i++) err = fmax(err, fabs(oz.L[i] - x45[i])); HX_CHECK(err == 0.0, "MIX=0 not exactly dry (%.3g)", err); }

    // ---- CPU
    {
        // worst block: per-block minimum over repeated runs (removes OS jitter), then the maximum over blocks; average over all blocks
        const int RUNS = 5, BLK = 48; const long nb = (long)(x45.size() / BLK); std::vector<double> best(nb, 1e30);
        for (int r = 0; r < RUNS; r++) {
            hxt::Host h; h.make(hx::fx_create_drone); Fx* fx = h.fx; fx->setParam(0, 1.f); fx->snap(); fx->reset(); FxCtx ctx = hxt::defCtx(); float L[BLK], R[BLK];
            for (long b = 0; b < nb; b++) { for (int i = 0; i < BLK; i++) { L[i] = x45[b * BLK + i]; R[i] = L[i]; }
                fx->smoothBlock(); auto t0 = std::chrono::steady_clock::now(); fx->process(ctx, L, R, BLK, false); double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); if (ns < best[b]) best[b] = ns; }
        }
        double sum = 0, mx = 0; long at = 0; for (long b = 0; b < nb; b++) { sum += best[b]; if (best[b] > mx) { mx = best[b]; at = b; } }
        std::vector<double> sorted = best; std::sort(sorted.begin(), sorted.end());
        const double avg = sum / (double)(nb * BLK), worst = mx / BLK, p99 = sorted[(size_t)(nb * 0.99)] / BLK;
        printf("CPU drone_v2 (trumpet phrase, block 48, host, per-block min of 5 runs): avg %.1f ns/sample, p99 block %.1f, worst block %.1f ns/sample (block %ld = %.2f s)\n", avg, p99, worst, at, at * BLK / (double)SRI);
        // host -> device: device ns/sample = host ns/sample x 100 (measured factor; the previous 12 kHz version: ~8 ns host = ~4 % on the Seed). 1 block = 1e9/48000 = 20833 ns/sample.
        const double pct = 100.0 / 20833.0 * 100.0 / 100.0;   // % of a 48-sample block per host ns/sample (x100 factor folded in) = 0.48
        printf("CPU drone_v2 est. device (host ns/sample x 100 / 20833): avg %.1f %%, p99 block %.1f %%, worst block %.1f %%\n", avg * 100.0 / 20833.0 * 100.0, p99 * 100.0 / 20833.0 * 100.0, worst * 100.0 / 20833.0 * 100.0); (void)pct;
        HX_CHECK(avg * 100.0 / 20833.0 * 100.0 <= 8.0, "avg CPU %.1f%% > 8%%", avg * 100.0 / 20833.0 * 100.0); HX_CHECK(worst * 100.0 / 20833.0 * 100.0 <= 12.0, "worst block %.1f%% > 12%%", worst * 100.0 / 20833.0 * 100.0);
        {   // stress: flanger on, chromatic comb at 100 %, peak amps, INTERP 0.1 s, broadband noise + harmonics (extreme settings, flanger alone is ~+2.6 %; limits avg 9 %, worst 16 %)
            std::vector<float> xs(12 * SRI); uint32_t sd = 5; double ph = 0;
            for (size_t i = 0; i < xs.size(); i++) { ph += TPI * 440.0 / SRI; double y = 0; for (int h = 1; h <= 20; h++) y += sin(h * ph) / h; xs[i] = (float)(0.02 * y + 0.01 * (rnd01(sd) * 2 - 1)); }
            std::vector<double> bs(nb, 1e30);
            for (int r = 0; r < RUNS; r++) { hxt::Host h; h.make(hx::fx_create_drone); Fx* fx = h.fx; fx->setParam(0, 1.f); fx->setParam(fx->findParam("FLAMT"), 1.f); fx->setParam(fx->findParam("SCALE"), 0.f); fx->setParam(fx->findParam("COMBLVL"), 100.f);
                fx->setParam(fx->findParam("PEAKAMP"), 1.f); fx->setParam(fx->findParam("INTERP"), 0.1f); fx->snap(); fx->reset(); FxCtx ctx = hxt::defCtx(); float L[BLK], R[BLK];
                for (long b = 0; b < nb; b++) { for (int i = 0; i < BLK; i++) { L[i] = xs[b * BLK + i]; R[i] = L[i]; } fx->smoothBlock(); auto t0 = std::chrono::steady_clock::now(); fx->process(ctx, L, R, BLK, false); double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); if (ns < bs[b]) bs[b] = ns; } }
            double s2 = 0, m2 = 0; for (double v : bs) { s2 += v; m2 = fmax(m2, v); } printf("CPU drone_v2 stress (flanger, comb 100 %%, peak, INTERP 0.1 s): host avg %.1f / worst %.1f ns/sample = device %.1f %% / %.1f %%\n", s2 / (nb * BLK), m2 / BLK, s2 / (nb * BLK) * 100.0 / 208.33, m2 / BLK * 100.0 / 208.33);
            HX_CHECK(s2 / (nb * BLK) * 100.0 / 208.33 <= 9.0 && m2 / BLK * 100.0 / 208.33 <= 16.0, "stress CPU over budget");
        }
        hxt::benchNsPerSample(hx::fx_create_drone, "drone_v2_sine330", false);
    }

    // ---- demo WAV: 10 s, 0-3.3 s dry, 3.3-6.6 s drone only, 6.6-10 s dry + drone (one continuous run of the effect; dry gained to -20 dBFS like a mic preamp)
    {
        const std::vector<Note> ph = {{0.3, 2.6, 440.0}, {2.9, 4.8, 523.25}, {5.0, 6.5, 329.63}, {6.8, 8.6, 587.33}, {8.8, 9.9, 220.0}};
        auto xd = trumpet(10.0, ph, -45.0); auto od = render(xd); const double dg = pow(10.0, 25.0 / 20.0); const long n = (long)xd.size(); std::vector<float> L(n), R(n);
        auto ramp = [](double t, double a, double b) { return t <= a ? 0.0 : (t >= b ? 1.0 : (t - a) / (b - a)); };
        for (long i = 0; i < n; i++) { const double t = (double)i / SRI;
            double gd = 1.0 - ramp(t, 3.25, 3.35) + ramp(t, 6.55, 6.65), gw = ramp(t, 3.25, 3.35); gd = fmin(gd, 1.0);
            L[i] = (float)(0.6 * (gd * dg * xd[i] + gw * od.wetL[i])); R[i] = (float)(0.6 * (gd * dg * xd[i] + gw * od.wetR[i])); }
        { std::string p = __FILE__; p = p.substr(0, p.find_last_of("/\\") + 1) + "../../../../tools/drone_v2_demo.wav"; writeWav(p.c_str(), L, R); }
        printf("  demo wav: 10 s, drone RMS %.1f dBFS\n", rmsDb(od.wetL, 4.0, 6.0));
    }
    printf("[drone_v2-extra] %s\n", hxt::failCount() ? "FAIL" : "PASS"); return hxt::failCount() ? 1 : 0;
}

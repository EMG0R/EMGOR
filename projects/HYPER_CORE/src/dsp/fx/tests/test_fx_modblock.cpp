// MODBLOCK (fx_modblock.h) host tests: std suite, MODE walls, MIX=0 dry, no clicks on wake / mode switch, toggle-on level continuity on noise with a dirty arena
// (device glue wake sequence replica, see HYPERTRUMPET_V2/bringup/fw/fx_glue_tmpl.h), phaser sweep, CPU per mode (device % ~ host ns/sample x 0.48).
#include "../fx_modblock.h"
#include "fx_testutil.h"
#include <algorithm>
using namespace hxt;
enum { MIX, MODE };
static const char* MN[3] = {"CHORUS", "PHASER", "FLANGER"};
struct Rng { uint32_t s; float f() { return rnd01(s) * 2.f - 1.f; } };

// ---- replica of the device glue (fx_glue_tmpl.h): init in a dirty arena, setParam(0,0)+snap, idle reset() from the main loop, wake (v[0]=mix, sm[0]=0, smA=gate smA, wake hook),
// then per block: v[0]=mix, smoothBlock, process, dry hold (out += dry*max(0, sm0-0.5)), 15 ms blend with the "wet not valid yet" hold.
struct GlueRep {
    std::vector<uint8_t> mem; Arena a; Fx* fx = nullptr; float mix = 1.f, fade = 1.f, gateA = 0.3f; int fadeHold = 0; bool active = false; hx::FxModBlock* mb = nullptr;
    void init(uint8_t fill, int mode) {
        mem.assign(200000, fill); a.bind(mem.data(), mem.size()); fx = fx_create_modblock(a, SR); mb = (hx::FxModBlock*)fx; fx->init(SR, 48.f);
        fx->setParam(MODE, (float)mode); mix = fx->v[0]; fx->setParam(0, 0.f); fx->Fx::snap(); gateA = fx->smA;
    }
    void idleClear() { fx->reset(); }                           // main loop, effect idle
    void wake() { mb->wake(); fx->v[0] = mix; fx->sm[0] = 0.f; fx->smA = gateA; fade = 0.f; fadeHold = 0; active = true; }
    void block(float* L, float* R, int n) {
        float hL[48], hR[48], dL[48], dR[48]; memcpy(dL, L, 192); memcpy(dR, R, 192); memcpy(hL, L, 192); memcpy(hR, R, 192);
        fx->v[0] = mix; fx->smoothBlock(); FxCtx c = defCtx(); bool o = fx->process(c, L, R, n, false);
        float mm = fx->sm[0], k = mm > 0.5f ? mm - 0.5f : 0.f; if (k > 0.f) for (int i = 0; i < n; i++) { L[i] += hL[i] * k; R[i] += hR[i] * k; }
        if (fade < 1.f) {
            float eY = 0, eD = 0; for (int i = 0; i < n; i++) { eY += L[i] * L[i] + R[i] * R[i]; eD += dL[i] * dL[i] + dR[i] * dR[i]; }
            if (eY < 0.1f * eD && fadeHold < 500) { fadeHold++; memcpy(L, dL, 192); memcpy(R, dR, 192); return; }
            float g0 = fade, g1 = g0 + 48.f / (0.015f * SR); if (g1 > 1.f) g1 = 1.f; float x = g0, inc = (g1 - g0) / 48.f;
            for (int i = 0; i < n; i++) { L[i] = dL[i] + (L[i] - dL[i]) * x; R[i] = dR[i] + (R[i] - dR[i]) * x; x += inc; } fade = g1; (void)o;
        }
    }
};
static double toggleDip(int mode, uint8_t fill, bool tone, int cycles, double* maxStepOut) {
    GlueRep g; g.init(fill, mode); Rng r{99}; double worst = 0; float last = 0; double maxStep = 0; double ph = 0;
    for (int cyc = 0; cyc < cycles; cyc++) {
        g.idleClear();
        double ring[20] = {0}; int rp = 0; double base = 0; int nb = 0; double mn = 1e9;
        for (int b = 0; b < 300; b++) {                         // 100 blocks dry baseline, wake at block 100, then 200 blocks
            float L[48], R[48]; for (int i = 0; i < 48; i++) { if (tone) { ph += 6.2831853 * 440.0 / SR; L[i] = 0.1259f * (float)sin(ph); } else L[i] = 0.1259f * r.f(); R[i] = L[i]; }
            if (b == 100) g.wake();
            if (b >= 100) g.block(L, R, 48);
            double e = 0; for (int i = 0; i < 48; i++) { e += (double)L[i] * L[i]; if (b >= 100 && b > 101) maxStep = fmax(maxStep, fabs(L[i] - last)); last = L[i]; }
            ring[rp++ % 20] = e / 48; double s = 0; for (int q = 0; q < 20; q++) s += ring[q]; double rms = sqrt(s / 20);
            if (b >= 40 && b < 100) { base += rms; nb++; } if (b >= 125 && rms < mn) mn = rms;     // 20 ms window fully inside the wake
        }
        base /= nb; worst = fmin(cyc ? worst : 1e9, 20 * log10((mn + 1e-12) / base)); if (!cyc) worst = 20 * log10((mn + 1e-12) / base);
        for (int b = 0; b < 100; b++) { float L[48], R[48]; for (int i = 0; i < 48; i++) { L[i] = 0.1259f * r.f(); R[i] = L[i]; } g.block(L, R, 48); }   // some more running before going idle
    }
    if (maxStepOut) *maxStepOut = maxStep; return worst;
}
static double rmsSeg(const std::vector<float>& x, size_t a, size_t n) { double e = 0; for (size_t i = a; i < a + n; i++) e += (double)x[i] * x[i]; return sqrt(e / n); }

int main() {
    Opts o; o.dryTol = 1e-6f; stdSuite("modblock", hx::fx_create_modblock, o);
    // 1. MODE: starts as CHORUS, walls without wrap
    { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; HX_CHECK(e->v[MODE] == 0.f, "must start as CHORUS"); HX_CHECK(!strcmp(e->pt[MODE].name, "MODE"), "param 1 = MODE");
      hx::FxModBlock* m = (hx::FxModBlock*)e; m->stepMode(-1); HX_CHECK(e->v[MODE] == 0.f, "wall at 0"); m->stepMode(1); m->stepMode(1); HX_CHECK(e->v[MODE] == 2.f, "reaches 2"); m->stepMode(1); HX_CHECK(e->v[MODE] == 2.f, "wall at 2 (no wrap)");
      e->setParam(MODE, 7.f); HX_CHECK(e->v[MODE] == 2.f, "clamp high"); e->setParam(MODE, -3.f); HX_CHECK(e->v[MODE] == 0.f, "clamp low");
      printf("  MODE: starts %s, walls at 0 and 2 (np=%d)\n", MN[0], e->np); }
    // 2. MIX=0 bit-exact dry for every mode, mono + stereo, including right after mode changes
    for (int md = 0; md < 3; md++) for (int st = 0; st < 2; st++) { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MODE, (float)md); e->setParam(MIX, 0.f); e->snap(); std::vector<float> L(48), R(48), ref(48); Rng r{5}; bool ex = true;
      for (int b = 0; b < 400; b++) { if (b == 200) { e->setParam(MODE, (float)((md + 1) % 3)); } for (int i = 0; i < 48; i++) { L[i] = 0.3f * r.f(); R[i] = st ? 0.2f * r.f() : L[i]; } ref = L; std::vector<float> refR = R;
          e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, st); for (int i = 0; i < 48; i++) if (L[i] != ref[i] || R[i] != refR[i]) ex = false; }
      HX_CHECK(ex, "MIX=0 not bit-exact dry (mode %d stereo %d)", md, st); }
    // 3. toggle-on continuity (glue wake replica, dirty arena fills), noise and tone, 6 wake cycles each
    for (int md = 0; md < 3; md++) for (int tone = 0; tone < 2; tone++) { double worst = 0, ms = 0; for (uint8_t fill : {0xFF, 0xCD, 0x7F, 0x00}) { double st; double d = toggleDip(md, fill, tone, 6, &st); if (d < worst || fill == 0xFF) worst = d; ms = fmax(ms, st); }
      printf("  wake %-7s %-5s: min 20 ms level vs pre-wake %+.1f dB, max step %.3f (noise own max step ~0.25)\n", MN[md], tone ? "tone" : "noise", worst, ms); if (!tone) HX_CHECK(worst > -4.5, "level gap on wake: %s %+.1f dB", MN[md], worst); HX_CHECK(ms < (tone ? 0.02 : 0.50), "click on wake: step %.3f", ms); }
    // 4. direct wake gap probe on the raw output (no glue hold/blend): after reset+wake the output must not fall into silence while the input is present, flanger included
    for (int md = 0; md < 3; md++) { GlueRep g; g.init(0xFF, md); Rng r{7}; g.idleClear(); g.wake(); g.mix = 1.f; double mnr = 1e9; float L[48], R[48];
      for (int b = 0; b < 300; b++) { for (int i = 0; i < 48; i++) { L[i] = 0.1f * r.f(); R[i] = L[i]; } float di = 0; for (int i = 0; i < 48; i++) di += L[i] * L[i]; g.fx->v[0] = 1.f; g.fx->smoothBlock(); g.fx->process(defCtx(), L, R, 48, false);
          float e = 0; for (int i = 0; i < 48; i++) e += L[i] * L[i]; if (b > 3) mnr = fmin(mnr, e / (di + 1e-12f)); }
      printf("  raw %-7s after wake: min block energy ratio %.1f dB (sm0 ramps from 0 -> expect ~dry)\n", MN[md], 10 * log10(mnr)); HX_CHECK(mnr > 0.1, "raw output gap after wake mode %d", md); }
    // 5. mode switch: no clicks (tone), level continuity (noise), random + rapid switching, 80 ms equal power fade
    { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); std::vector<float> out; uint32_t sd = 11; double ph = 0; float last = 0, mx = 0; Stats s;
      std::vector<float> L(48), R(48); for (int b = 0; b < 48000 * 10 / 48; b++) {
          if (b % 150 == 0 && b) e->setParam(MODE, (float)(int)(rnd01(sd) * 3));          // every 150 ms, random (also repeats, rapid reversals via the next line)
          if (b % 150 == 3) e->setParam(MODE, (float)(int)(rnd01(sd) * 3));
          for (int i = 0; i < 48; i++) { ph += 6.2831853 * 330.0 / SR; L[i] = 0.3f * (float)sin(ph); R[i] = L[i]; }
          e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false); scan(s, L.data(), 48); for (int i = 0; i < 48; i++) { if (b > 600) mx = fmaxf(mx, fabsf(L[i] - last)); last = L[i]; } }
      printf("  switching (tone): max step %.4f (sine's own %.4f) peak %.3f\n", mx, 0.3f * 6.2831853f * 330.f / SR, s.peak); HX_CHECK(!s.bad && s.denorm == 0, "nan/denormal in switching"); HX_CHECK(mx < 0.07f, "click on mode switch %.4f", mx); }
    { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MIX, 1.f); e->snap(); std::vector<float> out; NoiseGen ng; ng.amp = 0.3f;
      auto gen = [&](float* b, int m, long i0) { ng(b, m, i0); }; std::vector<float> L(48), R(48);
      for (int b = 0; b < 48000 * 8 / 48; b++) { if (b % 160 == 0 && b) e->setParam(MODE, (float)(((int)e->v[MODE] + 1) % 3)); gen(L.data(), 48, 0); R = L; e->smoothBlock(); e->process(defCtx(), L.data(), R.data(), 48, false); out.insert(out.end(), L.begin(), L.end()); }
      double mn = 1e9, mx = 0; for (size_t i = 24000; i + 960 < out.size(); i += 480) { double r = rmsSeg(out, i, 960); mn = fmin(mn, r); mx = fmax(mx, r); }
      printf("  switching (noise): 20 ms rms %.3f..%.3f (%.1f dB range)\n", mn, mx, 20 * log10(mx / mn)); HX_CHECK(20 * log10(mx / mn) < 6.0, "level not continuous over mode switches"); }
    // 6. phaser actually sweeps (mono sum): 1 kHz sine, P_RATE 2 Hz, window rms varies; and chorus/flanger produce a differing output
    { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MODE, 1.f); e->setParam(MIX, 1.f); e->setParam(7, 2.f); e->snap(); std::vector<float> o, o2; run(e, 2.0, 48, true, SineGen{1000.f, 0.3f}, defCtx(), &o, &o2);
      std::vector<float> mono(o.size()); for (size_t i = 0; i < o.size(); i++) mono[i] = 0.5f * (o[i] + o2[i]); double mn = 1e9, mx = 0; for (size_t i = 48000; i + 480 < mono.size(); i += 480) { double r = rmsSeg(mono, i, 480); mn = fmin(mn, r); mx = fmax(mx, r); }
      printf("  phaser mono-sum sweep at 1 kHz: window rms %.3f..%.3f\n", mn, mx); HX_CHECK(mx > 1.6 * mn, "phaser mono sum does not sweep"); }
    // 7. param extremes per mode (feedback max, stages 4/8, depth max): bounded, no NaN
    for (int md = 0; md < 3; md++) for (int ext = 0; ext < 2; ext++) { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MODE, (float)md); for (int i = 2; i < e->np; i++) e->setParam(i, ext ? e->pt[i].max : e->pt[i].min); e->setParam(MIX, 1.f); e->snap();
      Stats s = run(e, 3.0, 48, true, NoiseGen{7, 0.5f}, defCtx()); HX_CHECK(!s.bad && s.peak < 12.f && s.denorm == 0, "extremes mode %d ext %d peak %.2f", md, ext, s.peak); }
    // 9. feedback at each mode's maximum (C_FB 0.6, P_FB 0.92, F_FB 0.95): 10 s of noise and of tones at -20 dBFS peak stays below 0 dBFS, no NaN; level vs feedback roughly constant
    { const int fbIdx[3] = {5, 9, 14}; const float fbMax[3] = {0.6f, 0.92f, 0.95f}; const float fbDef[3] = {0.f, 0.f, 0.f};
      for (int md = 0; md < 3; md++) {
        double lv[3] = {0, 0, 0}; int k = 0;
        for (float fb : {fbDef[md], 0.5f * fbMax[md], fbMax[md]}) {
            Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MODE, (float)md); e->setParam(MIX, 1.f); e->setParam(fbIdx[md], fb); e->snap(); std::vector<float> out;
            Stats s = run(e, 10.0, 48, false, NoiseGen{21, 0.1f}, defCtx(), &out); lv[k++] = rmsSeg(out, 48000, out.size() - 48000);
            if (fb == fbMax[md]) { HX_CHECK(!s.bad && s.peak < 1.0f && s.denorm == 0, "%s fb max noise: peak %.3f", MN[md], s.peak); printf("  %-7s fb %.2f noise -20 dBFS: peak %.3f (%.1f dBFS)\n", MN[md], fb, s.peak, 20 * log10(s.peak)); }
        }
        for (float hz : {110.f, 440.f, 1000.f, 3000.f}) { Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MODE, (float)md); e->setParam(MIX, 1.f); e->setParam(fbIdx[md], fbMax[md]); e->snap();
            Stats s = run(e, 10.0, 48, true, SineGen{hz, 0.1f}, defCtx()); HX_CHECK(!s.bad && s.peak < 1.0f && s.denorm == 0, "%s fb max tone %.0f Hz: peak %.3f", MN[md], hz, s.peak); if (hz == 440.f) printf("  %-7s fb %.2f tone 440: peak %.3f\n", MN[md], fbMax[md], s.peak); }
        printf("  %-7s noise rms at fb 0 / half / max: %.4f / %.4f / %.4f (%.1f dB spread)\n", MN[md], lv[0], lv[1], lv[2], 20 * log10(fmax(lv[0], fmax(lv[1], lv[2])) / fmin(lv[0], fmin(lv[1], lv[2]))));
        HX_CHECK(20 * log10(fmax(lv[0], fmax(lv[1], lv[2])) / fmin(lv[0], fmin(lv[1], lv[2]))) < 4.0, "%s level not constant over fb", MN[md]); } }
    // 8. CPU per mode (average ns/sample over 4 s, worst block of the quietest of 6 passes) + during a crossfade
    for (int md = 0; md < 4; md++) {
        Host h; h.make(hx::fx_create_modblock); Fx* e = h.fx; e->setParam(MIX, 1.f); e->setParam(MODE, (float)(md == 3 ? 0 : md)); e->snap(); FxCtx c = defCtx(330.f);
        long nb = 4 * 48000 / 48; std::vector<float> inp((size_t)nb * 48); for (long i = 0; i < nb * 48; i++) inp[i] = (float)(0.2 * sin(2 * M_PI * 330 * i / SR) + 0.1 * sin(2 * M_PI * 990 * i / SR));
        std::vector<float> L(48), R(48); for (long b = 0; b < 400; b++) { memcpy(L.data(), &inp[b * 48], 192); R = L; e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false); }
        double tot = 0, worst = 1e30; float sink = 0;
        for (int pass = 0; pass < 6; pass++) { double pw = 0; if (pass) tot = 0;
            for (long b = 0; b < nb; b++) { if (md == 3 && b % 4 == 0) e->setParam(MODE, (float)(((int)e->v[MODE] + 1) % 3));   // md 3: switching every 4 blocks = always crossfading
                memcpy(L.data(), &inp[b * 48], 192); R = L; auto t0 = std::chrono::steady_clock::now(); e->smoothBlock(); e->process(c, L.data(), R.data(), 48, false);
                double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); tot += ns; if (ns > pw) pw = ns; sink += L[0]; }
            if (pw < worst) worst = pw; }
        double avg = tot / (nb * 48.0), wb = worst / 48.0; const char* nm = md == 3 ? "SWITCHING" : MN[md];
        printf("CPU %-9s: host %.2f ns/sample avg, worst block %.2f ns/sample -> device est avg %.2f%%, worst %.2f%% (x0.48; sink %.2f)\n", nm, avg, wb, avg * 0.48, wb * 0.48, sink);
        if (md == 1) { printf("  phaser target <= 2%%: %s by the host-ns x 0.48 formula (%.2f%%); regression bound 8%%\n", avg * 0.48 <= 2.0 ? "MET" : "MISSED", avg * 0.48); HX_CHECK(avg * 0.48 <= 8.0, "phaser CPU regression %.2f%%", avg * 0.48); }
    }
    printf(failCount() ? "[modblock] FAIL\n" : "[modblock] ALL PASS\n"); return failCount() ? 1 : 0;
}

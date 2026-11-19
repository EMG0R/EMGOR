// fx_harmony.h -- HARMONY page voices for HYPERTRUMPET V2: up to 3 pitch-shifted voices on top of the dry signal. Rewritten 2026-11-17 (audit: tests/test_fx_harmony.cpp).
// Voices: SolaVoice (fx_shifter_core.h): dual crossfading heads whose splices are aligned by waveform cross-correlation, so shifting quality/level no longer depends on the
// tracker or on the vibrato. The shared tracker (ctx.f0/conf, or the valve hint) only picks the grain G = 2*M*P (M so that G ~ 7 ms) and the search range (>= 1 period).
// Intervals: semitones relative to the input, -24..+24, 0 is "no voice". The firmware's table maps pads to (old interval - 12): -10..+9.
// API (momentary friendly, no latching inside): voiceOn(semis, level) / voiceOff(semis) / voiceIsOn(semis); the 4th simultaneous voiceOn steals the oldest. setSlot(k, semis) is kept
// for the old pad code (slot k = semis, 0 = off). setVoiceLevel(semis, lin) = per-interval trim (default 1). Params: MIX (send to L and R, encoder), VOICE (level of every voice), TONE.
// Each voice fades in / out in 18 ms; changing a slot's interval fades it out, switches the ratio at zero gain, fades it in (no click). Dry stays at unity.
// Latency (wet path): DMIN + mean search offset + G/2 ~ 6..9 ms (measured: tests/test_fx_harmony.cpp). Memory: ONE 2048-float delay line (8 KB) + ~1 KB state.
#pragma once
#include "fx_common.h"
#include "fx_shifter_core.h"
namespace hx {
struct FxHarmony : Fx {
    enum { P_MIX, P_VOICE, P_TONE };
    static const int NV = 3;
    ShifterCore core; SolaVoice vc[NV];
    volatile int tgt[NV] = {0, 0, 0};                 // requested interval per slot (semitones, 0 = off)
    volatile uint32_t age[NV] = {0, 0, 0}; uint32_t clk = 0;
    float trim[49];                                   // per-interval level trim, index = semis + 24
    int cur[NV] = {0, 0, 0}; float g[NV] = {0.f, 0.f, 0.f}, lp[NV] = {0.f, 0.f, 0.f}; float mixCur = 0.f; bool primed = false;
    float Gv = 512.f, Om = 330.f;
    FxHarmony(Arena& a, float s) {
        name = "HARMONY"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 0.7f, 0.f, PF_PERCENT);          // harmony mix (encoder in harmony mode)
        P(P_VOICE, "VOICE", 0.f, 1.f, 0.6f, 0.f, PF_PERCENT);      // level of each voice
        P(P_TONE, "TONE", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);         // 0 = dark (one-pole at 2 kHz), 1 = open
        setRole(R_AMT, P_MIX); setRole(R_TONE, P_TONE);
        for (int i = 0; i < 49; i++) trim[i] = 1.f;
        ok = core.init(a); for (int k = 0; k < NV; k++) vc[k].reset(core, Gv, Om);
    }
    static inline int clampSemi(int s) { return s < -24 ? -24 : (s > 24 ? 24 : s); }
    // ---- control API (call from the control loop; each call touches single ints, safe against the audio ISR)
    void voiceOn(int semis, float level = -1.f) {
        semis = clampSemi(semis); if (semis == 0) return;
        if (level >= 0.f) trim[semis + 24] = level;
        for (int k = 0; k < NV; k++) if (tgt[k] == semis) { age[k] = ++clk; return; }        // already on (or fading out): keep / revive
        int slot = -1; for (int k = 0; k < NV; k++) if (tgt[k] == 0 && cur[k] == 0) { slot = k; break; }
        if (slot < 0) for (int k = 0; k < NV; k++) if (tgt[k] == 0) { slot = k; break; }
        if (slot < 0) { uint32_t o = 0xFFFFFFFFu; for (int k = 0; k < NV; k++) if (age[k] < o) { o = age[k]; slot = k; } }   // steal the oldest
        age[slot] = ++clk; tgt[slot] = semis;
    }
    void voiceOff(int semis) { semis = clampSemi(semis); for (int k = 0; k < NV; k++) if (tgt[k] == semis) tgt[k] = 0; }
    bool voiceIsOn(int semis) const { for (int k = 0; k < NV; k++) if (tgt[k] == semis && semis != 0) return true; return false; }
    void setVoiceLevel(int semis, float lin) { trim[clampSemi(semis) + 24] = lin < 0.f ? 0.f : (lin > 2.f ? 2.f : lin); }
    void setSlot(int k, int semis) { if (k >= 0 && k < NV) { semis = clampSemi(semis); if (tgt[k] != semis) age[k] = ++clk; tgt[k] = semis; } }
    void allOff() { for (int k = 0; k < NV; k++) tgt[k] = 0; }
    void reset() override { for (int k = 0; k < NV; k++) { vc[k].reset(core, Gv, Om); g[k] = 0.f; lp[k] = 0.f; cur[k] = 0; tgt[k] = 0; } }

    bool process(const FxCtx& c, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (!stereoIn) memcpy(R, L, (size_t)n * sizeof(float));
        core.setPitch(c.f0, c.conf, c.sr > 0.f ? c.sr : sr, c.valvePitchHz);
        { float srr = c.sr > 0.f ? c.sr : sr, gg = 512.f, om = 330.f * srr / 48000.f;
          if (core.Pper > 0.f) { const float P = core.Pper; float M = floorf(0.007f * srr / P); if (M < 1.f) M = 1.f; gg = 2.f * M * P; om = P + 2.f; if (om > 400.f) om = 400.f; }
          gg = clampf(gg, 256.f, 1000.f); Gv += (gg - Gv) * 0.5f; Om = om; }
        const float fstep = 1.f / (0.018f * sr), voiceG = sm[P_VOICE], tone = sm[P_TONE], la = 1.f - expf(-kTwoPi * (2000.f + 14000.f * tone) / sr);
        const float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; }
        const float minc = (mt - mixCur) / (float)n; float m = mixCur;
        float ratio[NV], lv[NV];
        for (int k = 0; k < NV; k++) { ratio[k] = exp2f((float)cur[k] * (1.f / 12.f)); lv[k] = trim[cur[k] + 24]; }
        for (int i = 0; i < n; i++) {
            core.write(0.5f * (L[i] + R[i])); float w = 0.f;
            for (int k = 0; k < NV; k++) {
                const int t = tgt[k];
                if (t != cur[k]) { g[k] -= fstep; if (g[k] <= 0.f) { g[k] = 0.f; cur[k] = t; ratio[k] = exp2f((float)t * (1.f / 12.f)); lv[k] = trim[t + 24]; vc[k].reset(core, Gv, Om); lp[k] = 0.f; } }   // fade out, switch at zero
                else if (t != 0 && g[k] < 1.f) { g[k] += fstep; if (g[k] > 1.f) g[k] = 1.f; }
                if (cur[k] != 0 && g[k] > 0.f) { float y = vc[k].tick(core, ratio[k], Gv, Om); lp[k] += (y - lp[k]) * la; w += lp[k] * g[k] * lv[k]; }
            }
            const float o = w * voiceG * m; L[i] += o; R[i] += o; m += minc;
        }
        mixCur = mt; return true;
    }
};
inline Fx* fx_create_harmony(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxHarmony)); return m ? new (m) FxHarmony(a, sr) : nullptr; }
}

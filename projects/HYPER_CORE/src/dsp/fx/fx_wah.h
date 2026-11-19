// fx_wah.h -- HYPERTRUMPET V2 "wah": envelope-following auto-wah for trumpet (reworked 2026-11-17; was a lidar/ctx.env hand-wah). Own algorithm.
// Signal path: input -> TPT state-variable filter (hx::Svf, bandpass, resonance Q 0.7..14) -> level -> soft clip -> MIX crossfade.
// Control path: the filter centre is swept by an envelope follower on the input (mono sum, rectified, one-pole with separate ATTACK ~5 ms /
// RELEASE ~150 ms). The follower is read in dB and mapped to a position x in 0..1 over a 14 dB window; fc = LOFREQ * (HIFREQ/LOFREQ)^x (exponential,
// so equal level change = equal musical interval); RANGE scales the sweep span (0..3.8 octaves above 250 Hz); direction is UP.
// Input sensitivity / auto-gain (the trumpet into a dynamic mic plays at about -50..-40 dBFS, far below what a normal auto-wah expects):
//   AUTO-gain (always on): the top of the window is a slow peak tracker of the input (rises with ~30 ms, falls back toward the -62 dBFS floor with a ~4 s time constant),
//             so the sweep spans the player's own dynamics at any mic gain; SENS shifts the window top by -5..+5 dB around 0.5 (more = opens sooner).
//   autoGain=false (test hook): fixed window, top = -20 - 40*SENS dBFS (SENS 0 = hot line signal, 1 = -60 dBFS quiet mic), floor 14 dB below.
//   The -62 dBFS floor keeps mic hiss from being normalised up into a sweep.
// Coefficients are updated every 16 samples (one log2f, one exp2f, one tanf per 16 samples). Wet level: BP is normalised to unity at the peak then
// lifted by 1+0.5*sqrt(Q) (LP: 1/sqrt(Q)), then LEVEL dB, then softclip (bounded +-1).
// GUESSED (no external reference): the 14 dB window, the 4 s tracker fall, the Q/level compensation, defaults (dramatic per spec: Q 7, 250 Hz..3.5 kHz, attack 4 ms, release 120 ms).
// Params (exactly four): MIX SENS RANGE Q.  Roles: AMT=RANGE TIME=SENS MOD=Q.
// Stability at Q max: TPT SVF is unconditionally stable; the peak-normalised BP output is soft-clipped (bounded +-1) so resonance cannot blow up the level.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxWah : Fx {
    enum { P_MIX, P_SENS, P_RANGE, P_Q };
    // Fixed (not exposed): ATTACK 4 ms, RELEASE 120 ms, sweep floor 250 Hz, full sweep top 3.5 kHz, direction UP, BP mode, AUTO-gain on, +3 dB level.
    static constexpr float kAttackMs = 4.f, kReleaseMs = 120.f, kLoHz = 250.f, kHiHz = 3500.f, kLevelDb = 3.f;
    Svf f[2]; DryWet dw[2];
    float env = 0.f, pkDb = -62.f, xPos = 0.f, fcNow = 250.f;   // fcNow / xPos: last centre frequency and sweep position (read by tests and UI)
    bool autoGain = true, down = false;                          // test hooks (defaults: auto-gain on, direction up)
    FxWah(Arena&, float s) {
        name = "WAH"; sr = s;
        P(P_MIX,   "MIX",   0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_SENS,  "SENS",  0.f, 1.f, 0.6f, 0.f, PF_PERCENT);   // input sensitivity: window top moves -5..+5 dB around 0.5 (auto) / fixed top -20-40*SENS dBFS (autoGain off)
        P(P_RANGE, "RANGE", 0.f, 1.f, 0.9f, 0.f, PF_PERCENT);   // movement amount: sweep span 0..log2(3500/250)=3.8 octaves above 250 Hz
        P(P_Q,     "Q",     0.7f, 14.f, 7.f, 0.f, PF_LOG);
        setRole(R_AMT, P_RANGE); setRole(R_TIME, P_SENS); setRole(R_MOD, P_Q);
    }
    void reset() override { f[0] = Svf(); f[1] = Svf(); env = 0.f; pkDb = -62.f; dw[0] = DryWet(); dw[1] = DryWet(); }
    void wake() { reset(); }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        const bool lp = false, autoG = autoGain;
        const float aA = 1.f - expf(-1000.f / (kAttackMs * sr)), aR = 1.f - expf(-1000.f / (kReleaseMs * sr));
        const float lo = kLoHz, pos = 0.f;
        const float oct = log2f(kHiHz / kLoHz) * sm[P_RANGE], sens = sm[P_SENS]; float q = sm[P_Q]; q = q < 0.7f ? 0.7f : (q > 14.f ? 14.f : q);
        const float lvl = dbToLin(kLevelDb) * (lp ? 1.f / sqrtf(q > 1.f ? q : 1.f) : 1.f + 0.5f * sqrtf(q));
        const float fixedTop = -20.f - 40.f * sens, kWin = 1.f / 14.f;
        const float riseA = 1.f - expf(-1000.f / (30.f * sr)), fallA = 1.f - expf(-1.f / (4.f * sr));
        float dry0[HX_MAXBLOCK], dry1[HX_MAXBLOCK], wet0[HX_MAXBLOCK], wet1[HX_MAXBLOCK];
        const int nch = stereoIn ? 2 : 1;
        for (int i = 0; i < n; i++) { dry0[i] = L[i]; dry1[i] = stereoIn ? R[i] : 0.f; }
        for (int i0 = 0; i0 < n; i0 += 16) {
            const int m = n - i0 < 16 ? n - i0 : 16;
            float e = env;
            for (int i = i0; i < i0 + m; i++) { const float x = stereoIn ? 0.5f * (dry0[i] + dry1[i]) : dry0[i], a = x < 0.f ? -x : x; e += (a - e) * (a > e ? aA : aR); }
            env = flush(e);
            const float edb = 6.0206f * log2f(e * 1.5708f + 1e-7f);            // rectified-mean -> peak-equivalent amplitude, in dB
            float top;
            if (autoG) {
                if (edb > pkDb) pkDb += (edb - pkDb) * riseA * (float)m; else pkDb += (-62.f - pkDb) * fallA * (float)m;
                if (pkDb < -62.f) pkDb = -62.f;
                top = pkDb - (sens - 0.5f) * 10.f;                              // SENS up -> window top lower -> opens sooner (+-5 dB)
            } else top = fixedTop;
            float x = (edb - (top - 14.f)) * kWin + pos; x = x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
            xPos = x;
            fcNow = lo * exp2f(oct * (down ? 1.f - x : x));
            for (int c = 0; c < nch; c++) {
                Svf& s = f[c]; s.set(fcNow, q, sr); const float* in = c ? dry1 : dry0; float* w = c ? wet1 : wet0;
                for (int i = i0; i < i0 + m; i++) { float l, b, h; s.tick(in[i], l, b, h); w[i] = softclip(finite(lp ? l : s.k * b) * lvl); }
                s.ic1 = flush(s.ic1); s.ic2 = flush(s.ic2);
            }
        }
        float* io[2] = {L, R}; const float* dr[2] = {dry0, dry1}; float* we[2] = {wet0, wet1};
        for (int c = 0; c < nch; c++) dw[c].apply(io[c], dr[c], we[c], n, sm[P_MIX]);
        return stereoIn;
    }
};
inline Fx* fx_create_wah(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxWah)); return m ? new (m) FxWah(a, sr) : nullptr; }
}

// fx_modblock.h -- MODBLOCK: one pad, three modulation effects behind a MODE param (HYPERTRUMPET V2). NEW 2026-11-17. Own code.
//   MODE 0 = CHORUS (reuses fx_chorus.h: slow, deep, wide 3-voice, a little feedback), 1 = PHASER (new, below), 2 = FLANGER (reuses fx_flanger.h, THRUZ = 0).
//   MODE is a quantised enum: setParam() clamps, so an encoder stepping it hits walls at 0 and 2 (no wrap). stepMode(+-1) does the same. Starts as CHORUS.
// Structure: the input is summed to mono (the dry path stays stereo); each mode renders  out_m = dry + wet-part  (chorus 50/50 with its ensemble, flanger 0.5 x + 0.5 comb,
//   phaser 0.5 x + 0.5 allpass chain). The final stage is  out = dry + MIX * sum_m g_m * (out_m - dry).  MIX = 0 is bit-exact dry (the wet term is multiplied by exactly 0).
// Mode switch: each mode has an amplitude a_m (linear 80 ms ramp, the active mode up, all others down, sum stays 1) and the gain applied is g_m = sin(a_m * pi/2):
//   a two-way fade is the equal-power pair sin/cos. Crossfading only the wet parts (not the dry they share) avoids the +3 dB bump correlated signals give. Only modes with a_m > 0
//   are processed (the active one, plus the outgoing one(s) for 80 ms); a mode that has faded out is cleared in 1024-float slices per block (its old audio must not play when it is re-selected; if it is re-selected before the slices finish, the rest is cleared at once).
// PHASER: N = 4/6/8 (P_STAGES) first-order allpass stages, a = (tan(pi f/sr) - 1)/(tan(pi f/sr) + 1), f swept exponentially 200 Hz .. 4 kHz by a sine LFO
//   (f = 200 * 20^(0.5 + 0.5*DEPTH*lfo)), coefficients computed once per block per chain and ramped linearly per sample. Feedback (P_FB +-0.92) taps the last stage through a soft clip INSIDE the loop (hx::cubicClip: clamp +-1.5, x - 4/27 x^3, no division; linear for small signals, bounded +-1), so high feedback sings instead of running away; a state reset if |state| > 60 or NaN.
//   FEEDBACK (all modes): soft clip inside each loop (chorus C_FB <= 0.6, phaser P_FB <= 0.92, flanger F_FB <= +-0.95; opt-in satFb members added to FxChorus/FxFlanger, default off) and the wet part is scaled by (1-|fb|)^0.5 to keep the level roughly constant as fb rises.
//   Two chains (L, R) with the LFO offset by P_SPREAD cycles; default 0.12 (43 deg) because the output is summed to mono later: a large offset would fill in the mono notches
//   (L + R notches at different frequencies), a small one keeps them deep and still gives some stereo movement. Wet = 0.5 dry + 0.5 chain out (classic notch comb).
// WAKE / state (the flanger shows a ~20 ms gap at -62 dB ~50 ms after wake on the Seed; the host never reproduced it): every member is initialised in the constructor
//   (arena memory is not zeroed on the Seed; Arena::floats() zeroes buffers, objects are placement-new'd with default initialisers) and reset() (call from the main loop when idle)
//   clears ALL state of all three modes eagerly, including the parts FxFlanger::reset() leaves stale (it only queues a ring memset; dcur, primed, mixCur, ph, prevR/nextR, x1 are kept,
//   so the delay slew and mix ramp started from the previous session's values). wake() zeroes the small state and the rings only if they were used since the last clear.
// Params (16 = HX_MAXP): MIX MODE C_RATE C_DEPTH C_DELAY C_FB C_WIDTH P_RATE P_DEPTH P_FB P_STAGES P_SPREAD F_RATE F_DEPTH F_FB F_OFFSET.  Roles: AMT=MIX TIME=P_RATE TONE=C_WIDTH MOD=P_DEPTH FB=P_FB.
#pragma once
#include "fx_common.h"
#include "fx_chorus.h"
#include "fx_flanger.h"
namespace hx {

struct FxModBlock : Fx {
    enum { K_MIX, K_MODE, K_CRATE, K_CDEPTH, K_CDELAY, K_CFB, K_CWIDTH, K_PRATE, K_PDEPTH, K_PFB, K_PSTAGES, K_PSPREAD, K_FRATE, K_FDEPTH, K_FFB, K_FOFFSET };
    enum { M_CHORUS = 0, M_PHASER = 1, M_FLANGER = 2, M_COUNT = 3 };
    FxChorus* ch = nullptr; FxFlanger* fl = nullptr;
    // phaser state
    float apS[2][8] = {}, apFb[2] = {0.f, 0.f}, phPh = 0.f;
    // mode crossfade amplitudes (linear), current mode, dirty flags
    float amp[M_COUNT] = {1.f, 0.f, 0.f}; int mode = M_CHORUS; bool used[M_COUNT] = {false, false, false}; bool dirty = false;
    float mixCur = 0.f; bool primed = false; bool pend[M_COUNT] = {false, false, false}; int pendPos[M_COUNT] = {0, 0, 0};   // pend: faded-out mode whose ring is being cleared in slices
    float dL[HX_MAXBLOCK] = {}, dR[HX_MAXBLOCK] = {}, mono_[HX_MAXBLOCK] = {}, aL[HX_MAXBLOCK] = {}, aR[HX_MAXBLOCK] = {}, accL[HX_MAXBLOCK] = {}, accR[HX_MAXBLOCK] = {};

    FxModBlock(Arena& a, float s) {
        name = "MODBLOCK"; sr = s;
        static const char* const modes[] = {"CHORUS", "PHASER", "FLANGER"};
        P(K_MIX,     "MIX",     0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(K_MODE,    "MODE",    0.f, 2.f, 0.f, 1.f, PF_ENUM | PF_QUANT, modes);
        P(K_CRATE,   "C_RATE",  0.02f, 5.f, 0.12f, 0.f, PF_LOG);
        P(K_CDEPTH,  "C_DEPTH", 0.f, 1.f, 0.85f, 0.f, PF_PERCENT);
        P(K_CDELAY,  "C_DELAY", 8.f, 30.f, 18.f, 0.f);
        P(K_CFB,     "C_FB",    0.f, 0.6f, 0.15f, 0.f, PF_PERCENT);
        P(K_CWIDTH,  "C_WIDTH", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(K_PRATE,   "P_RATE",  0.02f, 8.f, 0.3f, 0.f, PF_LOG);
        P(K_PDEPTH,  "P_DEPTH", 0.f, 1.f, 0.9f, 0.f, PF_PERCENT);
        P(K_PFB,     "P_FB",    -0.92f, 0.92f, 0.6f, 0.f, PF_BIPOLAR);
        P(K_PSTAGES, "P_STAGES",4.f, 8.f, 6.f, 2.f, PF_INT | PF_QUANT);
        P(K_PSPREAD, "P_SPREAD",0.f, 0.5f, 0.12f, 0.f, 0);
        P(K_FRATE,   "F_RATE",  0.01f, 10.f, 0.2f, 0.f, PF_LOG);
        P(K_FDEPTH,  "F_DEPTH", 0.f, 10.f, 4.5f, 0.f);
        P(K_FFB,     "F_FB",    -0.95f, 0.95f, 0.7f, 0.f, PF_BIPOLAR);
        P(K_FOFFSET, "F_OFFSET",0.1f, 20.f, 0.6f, 0.f, PF_LOG);
        setRole(R_AMT, K_MIX); setRole(R_TIME, K_PRATE); setRole(R_TONE, K_CWIDTH); setRole(R_MOD, K_PDEPTH); setRole(R_FB, K_PFB);
        ch = static_cast<FxChorus*>(fx_create_chorus(a, s)); fl = static_cast<FxFlanger*>(fx_create_flanger(a, s));
        if (!ch || !fl || !ch->ok || !fl->ok) { ok = false; return; }
        ch->init(s, 48.f); fl->init(s, 48.f); ch->satFb = true; fl->satFb = true;
        clearAll();
    }
    ~FxModBlock() override { if (ch) ch->~FxChorus(); if (fl) fl->~FxFlanger(); }
    void stepMode(int d) { setParam(K_MODE, v[K_MODE] + (float)d); }    // clamps at 0 and 2: walls, no wrap

    // ---- state clearing
    void clearPhaser() { memset(apS, 0, sizeof apS); apFb[0] = apFb[1] = 0.f; phPh = 0.f; apInit = false; }
    void clearChorus() { if (!ch) return; memset(ch->dl.buf, 0, ch->dl.size() * 4); ch->dl.w = 0; ch->ph = 0.f; ch->hpL = ch->hpR = ch->lpL = ch->lpR = ch->fbS = 0.f; ch->clearReq = false; ch->dwL = DryWet(); ch->dwR = DryWet(); }
    void clearFlanger() {
        if (!fl) return;
        for (int c = 0; c < 2; c++) { memset(fl->dl[c].buf, 0, fl->dl[c].size() * 4); fl->dl[c].w = 0; fl->dc[c] = DcBlock(); fl->x1[c] = 0.f; fl->dcur[c] = 0.f; fl->prevR[c] = 0.f; fl->nextR[c] = 0.f; }
        fl->ph = 0.f; fl->mixCur = 0.f; fl->primed = false; fl->clearReq = false; fl->rng[0] = 0x2468ACEu; fl->rng[1] = 0x13579BDu;
    }
    // progressive clear of a faded-out mode's ring (1024 floats per block): a 64 KB flanger memset in one audio block would be a CPU spike
    void clearSlice(int m, int maxF) {
        if (m == M_PHASER) { clearPhaser(); pend[m] = false; return; }
        float* b0 = m == M_CHORUS ? ch->dl.buf : fl->dl[0].buf; float* b1 = m == M_CHORUS ? nullptr : fl->dl[1].buf; const int sz = (int)(m == M_CHORUS ? ch->dl.size() : fl->dl[0].size());
        const int total = b1 ? 2 * sz : sz; int n = total - pendPos[m]; if (n > maxF) n = maxF;
        for (int k = 0; k < n; k++) { const int p = pendPos[m] + k; (p < sz ? b0 : b1)[p < sz ? p : p - sz] = 0.f; }
        pendPos[m] += n; if (pendPos[m] >= total) { pend[m] = false; if (m == M_CHORUS) clearChorus(); else clearFlanger(); }
    }
    void clearAll() {
        for (int m = 0; m < M_COUNT; m++) pend[m] = false;
        clearPhaser(); clearChorus(); clearFlanger(); dirty = false; for (int m = 0; m < M_COUNT; m++) used[m] = false;
        for (int m = 0; m < M_COUNT; m++) amp[m] = (m == mode) ? 1.f : 0.f; mixCur = 0.f; primed = false;
    }
    void reset() override { if (!ok) return; clearAll(); }                         // eager and complete; call from the main loop (idle), not the audio callback
    void wake() { if (!ok) return; if (dirty) clearAll(); else { clearPhaser(); fl->dcur[0] = fl->dcur[1] = 0.f; fl->primed = false; fl->mixCur = 0.f; mixCur = 0.f; primed = false; for (int m = 0; m < M_COUNT; m++) amp[m] = (m == mode) ? 1.f : 0.f; } }
    void action(int id, float value) override { if (id == A_CLEAR && value > 0.5f && ok) clearAll(); }

    // ---- phaser
    float apA[2] = {-0.5f, -0.5f}; bool apInit = false;                       // allpass coefficient per chain, ramped linearly across each block (one tanf + exp2f per chain per block)
    static inline float fbComp(float fb) { fb = fb < 0.f ? -fb : fb; return sqrtf(1.f - (fb > 0.97f ? 0.97f : fb)); }   // wet-part level compensation (1-|fb|)^0.5: the feedback comb builds up ~1/sqrt(1-fb^2) in rms, this keeps the level roughly constant as fb rises
    static inline float outClip(float y) { float u = 0.5f * y; u = u > 1.5f ? 1.5f : (u < -1.5f ? -1.5f : u); return 2.f * (u - 0.148148f * u * u * u); }   // cubic soft clip of the wet (2% compression at 0.75, bounded +-2); off the feedback path
    // both chains in one loop (independent dependency chains interleave); the LFO-spread L/R chains differ only in their coefficient
    template <int N> static inline void cascade2(float (*S)[8], float* a, const float* da, float fb, float g, float* fs, const float* in, float* oL, float* oR, int n) {
        float aL = a[0], aR = a[1], fL = fs[0], fR = fs[1]; const float dL = da[0], dR = da[1];
        for (int i = 0; i < n; i++) {
            const float x = in[i]; float yl = x + fb * cubicClip(fL), yr = x + fb * cubicClip(fR);
            for (int s = 0; s < N; s++) {
                const float ol = aL * yl + S[0][s], orr = aR * yr + S[1][s]; S[0][s] = yl - aL * ol; S[1][s] = yr - aR * orr; yl = ol; yr = orr;
            }
            fL = yl; fR = yr; oL[i] = 0.5f * x + 0.5f * g * yl; oR[i] = 0.5f * x + 0.5f * g * yr; aL += dL; aR += dR;
        }
        fs[0] = fL; fs[1] = fR;
    }
    inline void phaserBlock(const float* in, float* outL, float* outR, int n) {
        const int ns = ((int)(sm[K_PSTAGES] + 0.5f)) & ~1; const int N = ns < 4 ? 4 : (ns > 8 ? 8 : ns);
        const float dph = sm[K_PRATE] / sr * (float)n, dep = sm[K_PDEPTH], fb = clampf(sm[K_PFB], -0.92f, 0.92f), spr = sm[K_PSPREAD];
        const float k0 = kPi / sr, l2 = 4.321928f;     // log2(4000/200)
        float a1[2];
        for (int c = 0; c < 2; c++) {                  // coefficient at the END of this block (LFO phase advanced by n samples)
            const float x = 0.5f + 0.5f * dep * sinPar(phPh + dph + (c ? spr : 0.f));
            const float f = 200.f * exp2f(l2 * x), t = tanf(k0 * f); a1[c] = (t - 1.f) / (t + 1.f);
            if (!apInit) apA[c] = a1[c];
        }
        apInit = true;
        const float da[2] = {(a1[0] - apA[0]) / (float)n, (a1[1] - apA[1]) / (float)n};
        switch (N) { case 4: cascade2<4>(apS, apA, da, fb, fbComp(fb), apFb, in, outL, outR, n); break; case 8: cascade2<8>(apS, apA, da, fb, fbComp(fb), apFb, in, outL, outR, n); break; default: cascade2<6>(apS, apA, da, fb, fbComp(fb), apFb, in, outL, outR, n); }
        for (int c = 0; c < 2; c++) { apA[c] = a1[c]; apFb[c] = flush(apFb[c]); }
        if (!(fabsf(apFb[0]) < 60.f) || !(fabsf(apFb[1]) < 60.f)) { memset(apS, 0, sizeof apS); apFb[0] = apFb[1] = 0.f; }   // safety net (NaN / runaway): never reached with |fb| <= 0.95, the cascade has unit gain
        phPh += dph; phPh -= (float)(int)phPh;
        for (int c = 0; c < 2; c++) for (int s = 0; s < 8; s++) apS[c][s] = flush(apS[c][s]);
    }

    bool process(const FxCtx& cx, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        if (!stereoIn) memcpy(R, L, (size_t)n * 4);
        memcpy(dL, L, (size_t)n * 4); memcpy(dR, R, (size_t)n * 4);
        const float* mono = dL; if (stereoIn) { for (int i = 0; i < n; i++) mono_[i] = 0.5f * (dL[i] + dR[i]); mono = mono_; }
        // mode request (quantised enum: sm == v)
        int want = (int)(v[K_MODE] + 0.5f); if (want < 0) want = 0; if (want > M_COUNT - 1) want = M_COUNT - 1; mode = want;
        const float step = (float)n / (0.08f * sr);
        for (int m = 0; m < M_COUNT; m++) { const float t = (m == mode) ? 1.f : 0.f; if (amp[m] < t) { amp[m] += step; if (amp[m] > t) amp[m] = t; } else if (amp[m] > t) { amp[m] -= step; if (amp[m] < t) amp[m] = t; } }
        bool haveAcc = false, pendRes = false; float pg = 0.f;   // pendRes: the only result so far is still in aL/aR (gain pg); it is folded into acc only if a second mode is active (crossfade)
        for (int m = 0; m < M_COUNT; m++) {
            if (amp[m] <= 0.f) { if (used[m]) { used[m] = false; pend[m] = true; pendPos[m] = 0; } if (pend[m]) clearSlice(m, 1024); continue; }   // faded out: clear its state in slices over the next blocks
            if (pend[m]) { pend[m] = false; if (m == M_PHASER) clearPhaser(); else if (m == M_CHORUS) clearChorus(); else clearFlanger(); }   // re-selected before the slices finished: clear the rest now
            used[m] = true; dirty = true;
            if (pendRes) { for (int i = 0; i < n; i++) { accL[i] = pg * finite(aL[i] - dL[i]); accR[i] = pg * finite(aR[i] - dR[i]); } pendRes = false; haveAcc = true; }
            if (m == M_CHORUS) {
                ch->sm[0] = ch->v[0] = 1.f; ch->sm[1] = sm[K_CRATE]; ch->sm[2] = sm[K_CDEPTH]; ch->sm[3] = sm[K_CDELAY]; ch->sm[4] = sm[K_CFB]; ch->sm[5] = sm[K_CWIDTH]; ch->sm[6] = 3.f; ch->sm[7] = 80.f; ch->sm[8] = 12000.f;
                memcpy(aL, mono, (size_t)n * 4); memcpy(aR, mono, (size_t)n * 4); ch->process(cx, aL, aR, n, false);
                const float cg = fbComp(sm[K_CFB]); for (int i = 0; i < n; i++) { aL[i] = 0.5f * mono[i] + 0.5f * cg * aL[i]; aR[i] = 0.5f * mono[i] + 0.5f * cg * aR[i]; }
            } else if (m == M_PHASER) {
                phaserBlock(mono, aL, aR, n);
            } else {
                fl->sm[0] = fl->v[0] = 1.f; fl->sm[1] = sm[K_FRATE]; fl->sm[2] = sm[K_FDEPTH]; fl->sm[3] = sm[K_FOFFSET]; fl->sm[4] = sm[K_FFB]; fl->sm[5] = 1.f; fl->sm[6] = 0.f; fl->sm[7] = 1.f;   // SINE, THRUZ = 0, LEVEL 1
                memcpy(aL, mono, (size_t)n * 4); fl->process(cx, aL, aR, n, false); const float fg = fbComp(sm[K_FFB]); for (int i = 0; i < n; i++) aL[i] = 0.5f * mono[i] + fg * (aL[i] - 0.5f * mono[i]); memcpy(aR, aL, (size_t)n * 4);
            }
            const float g = sinf(amp[m] * 1.5707963f);        // equal power across a two-way fade: sin / cos
            if (!haveAcc) { pendRes = true; pg = g; }
            else for (int i = 0; i < n; i++) { accL[i] += g * finite(aL[i] - dL[i]); accR[i] += g * finite(aR[i] - dR[i]); }
        }
        const float mt = clampf(sm[K_MIX], 0.f, 1.f); if (!primed) { mixCur = mt; primed = true; }
        const float inc = (mt - mixCur) / (float)n; float mm = mixCur;
        if (pendRes) { for (int i = 0; i < n; i++) { const float k = mm * pg; L[i] = flush(dL[i] + k * finite(aL[i] - dL[i])); R[i] = flush(dR[i] + k * finite(aR[i] - dR[i])); mm += inc; } }
        else if (!haveAcc) { for (int i = 0; i < n; i++) { L[i] = dL[i]; R[i] = dR[i]; } }
        else for (int i = 0; i < n; i++) { L[i] = flush(dL[i] + mm * accL[i]); R[i] = flush(dR[i] + mm * accR[i]); mm += inc; }
        mixCur = mt; return true;
    }
};
inline Fx* fx_create_modblock(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxModBlock)); return m ? new (m) FxModBlock(a, sr) : nullptr; }
}

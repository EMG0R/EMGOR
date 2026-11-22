// fx_moodii.h -- HYPERTRUMPET V2 "MOOD II": Chase Bliss MOOD MKII style two-channel sampler/ambience. Clean-room, own work (Emory Smith / EMGOR). NEW 2026-11-17.
// Behaviour source = the user's Csound MOOD II (NEPTR phase4 csound/effects/mood2.orc, v9 SLIP engine) wherever it differs from the pedal:
//   * ONE shared CLOCK on a 9-rung tonal ladder [-24 -19 -12 -7 0 +7 +12 +19 +24] st (home = rung 4, knob 0.5; piecewise 0..1 map, mood2.orc:701-707).
//     rate = 2^(st/12). HARD steps (no glide) + a 'landing hit' duck. GEN = renewal process that jumps to a random other rung and back, the jump/return
//     times quantised to SUBDIV ms (Csound: gkChaseBlissSubdivSec = 0.125), duty = 0.5*GEN (mood2.orc:725-760).
//   * Down rungs get a bit-crush whose step is RELATIVE to the signal's own slow running peak (pow2 buckets, 60 ms atk / 600 ms rel) so quiet input is crushed, not deleted
//     (mood2.orc:803-828, 897-904). Up rungs are interpolated and clean. Csound quantises Slip even at home (1.1 % floor step): kept, scaled by GRIT (0 = off).
//   * SLIP (mood2.orc:931-1040): continuous ring, window TIME (10 ms..1 s, jitter +-3 % length / anchor), read rate = clock rate, ZOH when down, lerp when up, 2 ms edge dips.
//   * Csound deleted Reverb/Delay and the micro-looper from MOOD II and routes nothing from the looper. This file keeps the pedal's full structure (so every mode can be demoed)
//     but the DEFAULTS are the Csound behaviour: WMODE = SLIP, LOOP level 0, ROUTE = IN. Reverb/Delay/Loop modes are the pedal's (manual pp.10-41), driven by the same CLOCK.
// PLAYER REFINEMENT 2026-11-17: SLIP is the live mode (WMODE/LMODE/MOD/LOOP/LLEN/LMOD/ROUTE/FREEZE are PF_HIDDEN; loop off by default). The CLOCK is a sample rate: SLIP reads the ring by
//   nearest-neighbour in BOTH directions (sample-and-hold down, point-sampling up, no interpolation, no anti-alias) so pitch and fidelity move together. CLKMOD (joystick -1..+1, dead zone +-0.08,
//   2^x, +1 = x2, -1 = x0.5) multiplies the ladder rate (and GEN's jumps); total clamped 0.25..4 (the 2.2 MB arena ring). Faders: MIX, LENGTH, GEN.
// WET channel modes: REVERB (TIME = size+decay, MOD = smear: 0 multi-tap .. 1 reverb), DELAY (TIME = delay, MOD = feedback, time changes crossfade (no pitch bend)), SLIP.
//   Reverb/Delay run in a CLOCK-domain: the engine is ticked at rate r (<=2) per host sample, so time scales by 1/r and the ZOH hold is the sample-rate aliasing.
// LOOP channel (always-listening micro-looper, mono, 4 s ring): A_MAIN = capture (length = 1 s / clock rate, clamped 0.25..4 s) / back to listening (replace),
//   A_ALT = overdub toggle, A_CLEAR = clear, A_HOLD = freeze wet (pedal: hold bypass). LMODE: ENV (slices; input above threshold repeats the current slice; LMOD = sensitivity),
//   TAPE (LMOD = speed/direction -4,-2,-1,-.5,.5,1,2,4; LLEN = loop length), STRETCH (2 Hann grains; LMOD: left of noon reverse, noon frozen, right forward, max = none; LLEN = slice size).
// ROUTE: 0 IN (wet eats input, loop heard direct), 1 IN+LOOP (both into wet), 2 LOOP (only loop into wet, dry input stays clean). Output mono (L = R written).
// Clicks: every mode change (wet, loop) is a 10 ms dip-switch, loop/slice/slip seams have edge dips, delay time changes crossfade, MIX ramps per block.
// State: ALL instance members, no statics except const tables. wake() zeroes all state; big rings (SDRAM) are gated by "valid sample" counters instead of a multi-MB memset
//   (a dirty arena can never leak: reads past the valid count return 0). reset() = wake().
// Memory (48 kHz): slip ring 1 MB + loop 768 KB + delay 256 KB + reverb ~100 KB = ~2.1 MB of the 12 MB slot arena. Params: 15 (<=16).
#pragma once
#include "fx_common.h"
namespace hx {

struct FxMoodII : Fx {
    enum { P_MIX = 0, P_WMODE, P_LMODE, P_CLOCK, P_LENGTH, P_MOD, P_LOOP, P_LLEN, P_LMOD, P_GEN, P_ROUTE, P_GRIT, P_SUBDIV, P_TONE, P_FREEZE, P_CLKMOD };
    enum { W_REVERB = 0, W_DELAY, W_SLIP };
    enum { L_ENV = 0, L_TAPE, L_STRETCH };
    enum { LS_LISTEN = 0, LS_PLAY, LS_OVR };

    // ------------------------------------------------------------------ memory
    float *sRing = nullptr, *lbuf = nullptr, *dBuf = nullptr, *tapBuf = nullptr, *ap1 = nullptr, *ap2 = nullptr, *fl[4] = {};
    uint32_t sMask = 0, dMask = 0, tapMask = 0; int LN = 0; int ap1N = 0, ap2N = 0, fN[4] = {};
    float ks = 1.f;   // sr / 48000 (Csound constants are in 48 kHz samples)

    // ------------------------------------------------------------------ control / clock
    uint32_t rng = 1u; double tNow = 0.0, genDead = 0.0; int genState = 0, awayIdx = 4, prevIdx = 4, activeIdx = 4, baseIdx = 4; float genEvLen = 0.f;
    float cRate = 1.f, cTickRate = 1.f, cSemi = 0.f; bool cInterp = true; float joyF = 1.f, lastGritRate = -1.f;
    float hit = 1.f; int hitCnt = 0; float hitRec = 0.01f; int gritIdx = -1; float gritCurve = 0.f; int cN = -1; float cPort = 0.f, lastTone = -1.f, lastTm = -1.f, lastRate = -1.f, rt = 1.f, szT = 1.f, rgBase = 0.f, Dt0 = 100.f;
    float mixCur = 0.f; bool primed = false;
    volatile uint8_t pend = 0; bool frzAct = false;
    float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK], mono[HX_MAXBLOCK];

    // ------------------------------------------------------------------ grit (clock-down bit crusher), one per path
    struct Grit {
        float lev = 0.01f, bucket = 1.f, bucketSm = 1.f, blkMax = 0.f, step = 0.f, inv = 0.f, qfrac = 0.f;
        void wake() { lev = 0.01f; bucket = bucketSm = 1.f; blkMax = 0.f; step = inv = 0.f; }
        float cBkt = 0.f, cAtk = 0.f, cRel = 0.f; int cN = -1;
        void coef(int n, float sr) { if (n == cN) return; cN = n; cBkt = 1.f - expf(-(float)n / (0.03f * sr)); cAtk = expf(-(float)n / (0.06f * sr)); cRel = expf(-(float)n / (0.6f * sr)); }
        void begin(int n, float sr, float qf, float floorLev) {     // once per block, from the previous block's follower
            coef(n, sr); qfrac = qf; if (qf <= 0.f) { step = 0.f; return; }
            float l = lev < floorLev ? floorLev : (lev > 1.f ? 1.f : lev);
            if (l > bucket * 2.f) bucket *= 2.f; else if (l < bucket * 0.5f) bucket *= 0.5f;
            bucket = clampf(bucket, floorLev, 1.f);
            bucketSm += (bucket - bucketSm) * cBkt;
            step = qf * bucketSm; inv = 1.f / step;
        }
        inline void peak(float a) { if (a > blkMax) blkMax = a; }
        void end(int n, float sr) {
            (void)sr; coef(n, sr);
            lev = blkMax > lev ? blkMax + (lev - blkMax) * cAtk : blkMax + (lev - blkMax) * cRel; lev = flush(lev); blkMax = 0.f;
        }
        inline float q(float y) const { if (step <= 0.f) return y; const float t = y * inv; return step * (float)(int)(t + (t >= 0.f ? 0.5f : -0.5f)); }
    } gW, gL;

    // ------------------------------------------------------------------ wet channel
    int wmCur = W_SLIP, wmPend = W_SLIP; float wmG = 1.f, fadeStep = 0.f;
    float acc = 0.f, held = 0.f, tLp = 0.f, tA = 1.f; float frzG = 1.f;   // frzG: 1 = input open / normal feedback
    // slip
    uint32_t sW = 0, sWritten = 0; float sPh = 0.f, sWT = 0.f, sWTCur = 0.f; int sAoff = 0;
    // delay
    uint32_t dW = 0, dValid = 0; float dA = 0.f, dB = 0.f, dXf = 0.f, dHf = 0.f; bool dFading = false;
    // reverb
    uint32_t tapW = 0, tapD[5] = {1, 1, 1, 1, 1}; float tapDf[5] = {1, 1, 1, 1, 1}, tapInc[5] = {}; bool tapMoving = false; int ap1i = 0, ap2i = 0, fW[4] = {}; float fLp[4] = {}, szCur = 1.f, rvIn = 1.f, rvFb = 1.f;

    // ------------------------------------------------------------------ loop channel
    int lst = LS_LISTEN, lstart = 0, llen = 0, lmPend = L_TAPE; float tapeSpd = 1.f, tapeLw = 1.f;
    struct LoopSt {   // hot loop-channel scalars: copied to a local per block (registers, no store->load chains through `this`)
        float lg = 0.f, lmG = 1.f, ovG = 0.f, lph = 0.f, lhd = 0.f, envv = 0.f, hb = 0.f, holdStart = 0.f, spos = 0.f, gidx[2] = {}, gage[2] = {};
        int lw = 0, lvalid = 0, lwi = -1, lmCur = L_TAPE, gcnt = 0, gi = 0; bool gate = false, holding = false;
        void enter() { lwi = -1; lph = lhd = 0.f; holding = false; gate = false; hb = 0.f; envv = 0.f; spos = 0.f; gcnt = 0; gi = 0; gage[0] = gage[1] = 1e9f; }
    } ls;
    float loBuf[HX_MAXBLOCK];

    FxMoodII(Arena& a, float s) {
        name = "MOODII"; sr = s; ks = s / 48000.f;
        static const char* const wm[] = { "REVERB", "DELAY", "SLIP" };
        static const char* const lm[] = { "ENV", "TAPE", "STRETCH" };
        static const char* const rt[] = { "IN", "IN+LOOP", "LOOP" };
        static const char* const ft[] = { "OFF", "ON" };
        P(P_MIX, "MIX", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_WMODE, "WMODE", 0.f, 2.f, 2.f, 1.f, PF_ENUM | PF_QUANT | PF_HIDDEN, wm);       // default SLIP (Csound)
        P(P_LMODE, "LMODE", 0.f, 2.f, 1.f, 1.f, PF_ENUM | PF_QUANT | PF_HIDDEN, lm);
        P(P_CLOCK, "CLOCK", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);                  // 0.5 = home, 9-rung ladder
        P(P_LENGTH, "LENGTH", 0.f, 1.f, 0.35f, 0.f, PF_PERCENT);                 // SLIP window 10 ms..1 s (Csound LENGTH 0.35); in the hidden modes: reverb size/decay, delay time
        P(P_MOD, "MOD", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT | PF_HIDDEN);                      // wet: reverb smear, delay feedback (unused in SLIP, as in Csound)
        P(P_LOOP, "LOOP", 0.f, 1.f, 0.f, 0.f, PF_PERCENT | PF_HIDDEN);                     // loop level, 0 = Csound (no loop in MOOD)
        P(P_LLEN, "LLEN", 0.f, 1.f, 1.f, 0.f, PF_PERCENT | PF_HIDDEN);
        P(P_LMOD, "LMOD", 0.f, 1.f, 0.7143f, 0.f, PF_PERCENT | PF_HIDDEN);                 // 0.7143 = tape 1x / env sens mid-high
        P(P_GEN, "GEN", 0.f, 1.f, 0.f, 0.f, PF_PERCENT);                       // generative clock, Csound P4
        P(P_ROUTE, "ROUTE", 0.f, 2.f, 0.f, 1.f, PF_ENUM | PF_QUANT | PF_HIDDEN, rt);
        P(P_GRIT, "GRIT", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_SUBDIV, "SUBDIV", 60.f, 500.f, 125.f, 0.f, PF_LOG);                // ms, Csound gkChaseBlissSubdivSec 0.125
        P(P_TONE, "TONE", 1000.f, 20000.f, 20000.f, 0.f, PF_LOG);              // wet hi-cut (pedal DARKER); 20 k = off
        P(P_FREEZE, "FREEZE", 0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT | PF_HIDDEN, ft);
        P(P_CLKMOD, "CLKMOD", -1.f, 1.f, 0.f, 0.f, PF_BIPOLAR);              // joystick: +1 = clock x2 (+1 oct), -1 = x0.5, +-0.08 dead zone, 2^x between; stacks with CLOCK and GEN
        setRole(R_PITCH, P_CLKMOD); setRole(R_TIME, P_LENGTH); setRole(R_FB, P_CLOCK); setRole(R_TONE, P_TONE); setRole(R_MOD, P_GEN); setRole(R_AMT, P_LOOP);
        const uint32_t sN = 1u << 18, dN = nextPow2((uint32_t)(1.05f * s) + 8), tN = nextPow2((uint32_t)(0.2f * s));
        LN = (int)(4.0f * s);
        sRing = a.floats(sN); lbuf = a.floats((size_t)LN + 4); dBuf = a.floats(dN); tapBuf = a.floats(tN);
        sMask = sN - 1; dMask = dN - 1; tapMask = tN - 1;
        ap1N = (int)(142 * ks) + 1; ap2N = (int)(107 * ks) + 1; ap1 = a.floats(ap1N); ap2 = a.floats(ap2N);
        static const int base[4] = { 1087, 1283, 1549, 1823 };
        bool good = sRing && lbuf && dBuf && tapBuf && ap1 && ap2;
        for (int k = 0; k < 4; k++) { fN[k] = (int)(base[k] * ks) | 1; fl[k] = a.floats((size_t)fN[k]); good = good && fl[k]; }
        if (!good) { ok = false; return; }
        wake();
    }
    void reset() override { wake(); }
    // zero ALL state. Small buffers are memset; the 3 big rings are made unreadable via their valid counters (no multi-MB memset on the audio thread).
    void wake() {
        if (!ok) return;
        rng = 0x2545F491u; tNow = 0.0; genDead = 0.0; genState = 0; awayIdx = prevIdx = activeIdx = baseIdx = 4; genEvLen = 0.f;
        cRate = cTickRate = 1.f; cSemi = 0.f; cInterp = true; hit = 1.f; hitCnt = 0; mixCur = 0.f; primed = false; pend = 0; frzAct = false;
        gW.wake(); gL.wake(); fadeStep = 1.f / (0.010f * sr);
        wmCur = wmPend = (int)v[P_WMODE]; wmG = 1.f; acc = 0.f; held = 0.f; tLp = 0.f; frzG = 1.f;
        enterWet(wmCur);
        lst = LS_LISTEN; ls = LoopSt(); lstart = 0; llen = 0; ls.lmCur = lmPend = (int)v[P_LMODE]; ls.enter();
        for (int i = 0; i < HX_MAXBLOCK; i++) dryL[i] = dryR[i] = mono[i] = 0.f;
        sWT = sWTCur = (480.f + v[P_LENGTH] * 47040.f) * ks;
    }
    void enterWet(int m) {
        acc = 0.f; held = 0.f;
        sW = 0; sWritten = 0; sPh = 0.f; sAoff = 0; sWTCur = sWT;
        dW = 0; dValid = 0; dA = dB = 0.f; dXf = 0.f; dHf = 0.f; dFading = false;
        tapW = 0; ap1i = ap2i = 0; szCur = szT; rvIn = 0.f; rvFb = 1.f;   // rvIn 0: the input ramps in, so tap/FDN echoes start smoothly (no abrupt first echo)
        memset(tapBuf, 0, ((size_t)tapMask + 1) * 4); memset(ap1, 0, (size_t)ap1N * 4); memset(ap2, 0, (size_t)ap2N * 4);
        for (int k = 0; k < 4; k++) { memset(fl[k], 0, (size_t)fN[k] * 4); fW[k] = 0; fLp[k] = 0.f; }
        (void)m;
    }
    void enterLoopMode() { ls.enter(); }

    void action(int id, float) override {
        if (id == A_MAIN) pend |= 1; else if (id == A_ALT) pend |= 2; else if (id == A_CLEAR) pend |= 4; else if (id == A_HOLD) pend |= 8;
    }
    int loopState() const { return lst; }
    int clockIndex() const { return activeIdx; }

    static float ladderRate(int i) {
        static const float R[9] = { 0.25f, 0.33484f, 0.5f, 0.66742f, 1.f, 1.49831f, 2.f, 2.99661f, 4.f };
        return R[i < 0 ? 0 : (i > 8 ? 8 : i)];
    }
    static float ladderSemi(int i) { static const float S[9] = { -24, -19, -12, -7, 0, 7, 12, 19, 24 }; return S[i < 0 ? 0 : (i > 8 ? 8 : i)]; }
    inline float rnd() { return rnd01(rng) * 2.f - 1.f; }   // -1..1

    // ------------------------------------------------------------------ control (once per block)
    void control(int n) {
        const float dt = (float)n / sr; tNow += dt;
        // actions
        uint8_t p = pend; pend = 0;
        if (p & 4) { lst = LS_LISTEN; ls.lvalid = 0; ls.lw = 0; }
        if (p & 8) frzAct = !frzAct;
        if (p & 1) {
            if (lst == LS_LISTEN) {
                int want = (int)(sr / ladderRate(activeIdx)); if (want > LN) want = LN; if (want < (int)(0.25f * sr)) want = (int)(0.25f * sr);
                if (want > ls.lvalid) want = ls.lvalid;
                if (want >= (int)(0.05f * sr)) { llen = want; lstart = ls.lw - llen; if (lstart < 0) lstart += LN; lst = LS_PLAY; enterLoopMode(); }
            } else { lst = LS_LISTEN; ls.lvalid = 0; }
        }
        if (p & 2) { if (lst == LS_PLAY) lst = LS_OVR; else if (lst == LS_OVR) lst = LS_PLAY; }
        // clock: hard-step ladder + GEN
        const float clk = clampf(v[P_CLOCK], 0.f, 1.f), gen = clampf(v[P_GEN], 0.f, 1.f);
        const float bf = clk <= 0.5f ? clk * 8.f : 4.f + (clk - 0.5f) * 8.f;
        baseIdx = (int)floorf(bf + 0.5f); if (baseIdx > 8) baseIdx = 8;
        const float sub = v[P_SUBDIV] * 0.001f;
        if (tNow >= genDead) {
            if (genState == 0) {
                const float duty = 0.5f * gen;
                if (duty > 0.0005f) {
                    float evl = 0.35f + 1.85f * (rnd() * 0.5f + 0.5f); evl = floorf(evl / sub + 0.5f) * sub; if (evl < sub) evl = sub; genEvLen = evl;
                    int k = (int)((rnd() * 0.5f + 0.5f) * 7.999f); if (k >= baseIdx) k++; if (k > 8) k = 8; awayIdx = k;
                    genState = 1; genDead = tNow + genEvLen;
                } else genDead = tNow + 0.25;
            } else {
                genState = 0; const float duty = 0.5f * gen; float gap = 999999.f;
                if (duty > 0.0005f) { float r = 0.7f + 0.9f * (rnd() * 0.5f + 0.5f); gap = genEvLen * (1.f - duty) / duty * r; gap = floorf(gap / sub + 0.5f) * sub; if (gap < sub) gap = sub; }
                genDead = tNow + gap;
            }
        }
        activeIdx = genState ? awayIdx : baseIdx;
        if (activeIdx != prevIdx) { hitCnt = (int)(0.001f * sr); hitRec = 1.f - expf(-1.f / (0.006f * sr)); }
        prevIdx = activeIdx;
        // joystick: dead zone +-0.08 then 2^x (renormalised so +-1 = x2 / x0.5); smoothed over ~2 ms per block step so the factor is instant but never a staircase
        { const float j = clampf(v[P_CLKMOD], -1.f, 1.f), aj = fabsf(j); const float xj = aj <= 0.08f ? 0.f : (j < 0.f ? -1.f : 1.f) * (aj - 0.08f) * (1.f / 0.92f);
          const float tgt = xj == 0.f ? 1.f : exp2f(xj); joyF += (tgt - joyF) * 0.5f; if (fabsf(tgt - joyF) < 1e-4f) joyF = tgt; }
        cRate = clampf(ladderRate(activeIdx) * joyF, 0.25f, 4.f);                     // ring holds window/rate: total clamp 0.25..4 (ladder x joystick x GEN stack, then clamp)
        cSemi = (cRate > 0.9999f && cRate < 1.0001f) ? 0.f : 12.f * log2f(cRate); cTickRate = cRate > 2.f ? 2.f : cRate; cInterp = cSemi >= -0.01f;
        // grit
        const float g = clampf(v[P_GRIT], 0.f, 1.f); const bool down = cSemi < -0.01f;
        if (cRate != lastGritRate) { lastGritRate = cRate; gritCurve = down ? 0.18f + 0.82f * powf(clampf(-cSemi / 24.f, 0.f, 1.f), 0.55f) : 0.f; }
        const float qf = (0.0025f + gritCurve * 0.05f) / 0.22f * g;
        gW.begin(n, sr, (wmCur == W_SLIP || down) ? qf : 0.f, 0.0008f);
        gL.begin(n, sr, down ? qf : 0.f, 0.0008f);
        // slip window smoothing (Csound: port 0.12 s)
        if (n != cN) { cN = n; cPort = 1.f - expf(-(float)n / (0.12f * sr)); }
        sWT += ((480.f + clampf(sm[P_LENGTH], 0.f, 1.f) * 47040.f) * ks - sWT) * cPort;
        // tone
        const float tone = clampf(sm[P_TONE], 1000.f, 20000.f); if (tone != lastTone) { lastTone = tone; tA = tone >= 19000.f ? 1.f : 1.f - expf(-kTwoPi * tone / sr); }
        // modes
        wmPend = (int)clampf(v[P_WMODE] + 0.5f, 0.f, 2.f); lmPend = (int)clampf(v[P_LMODE] + 0.5f, 0.f, 2.f);
        if (lst == LS_LISTEN && ls.lg <= 0.f && lmPend != ls.lmCur) { ls.lmCur = lmPend; enterLoopMode(); }
        // overdub ramp is per sample in loopTick
    }

    // ------------------------------------------------------------------ SLIP
    inline float slipTick(float x, bool frz) {
        if (!frz) { sRing[sW] = x; sW = (sW + 1) & sMask; if (sWritten <= sMask) sWritten++; sAoff++; }
        sPh += cRate;
        if (sPh >= sWTCur) {
            sPh -= sWTCur; if (sPh >= sWTCur) sPh = 0.f;
            if (!frz) { const float ja = fminf(0.015f * sWTCur, 400.f * ks); sAoff = -(int)(rnd() * ja); }
            sWTCur = clampf(sWT + rnd() * 0.03f * sWT, 480.f * ks, 47520.f * ks);
        }
        if (sWritten < 4) return 0.f;
        const float dip = 96.f * ks, dw = fminf(sPh, sWTCur - sPh), env = dw < dip ? dw * (1.f / dip) : 1.f;
        float behind = (float)sAoff + sWTCur - sPh;
        const float hi = fminf((float)sWritten - 3.f, (float)sMask - 4.f);
        behind = behind < 2.f ? 2.f : (behind > hi ? hi : behind);
        const int ib = (int)behind; const float fr = behind - (float)ib;   // read pos = sW - behind; floor = sW - ib - 1
        const uint32_t i0 = (sW - (uint32_t)ib - 1u) & sMask;
        (void)fr; const float y = sRing[i0];   // sample-and-hold / point sampling in BOTH directions (the clock is a sample rate: no interpolation, no anti-alias, by design)
        gW.peak(fabsf(y));
        return gW.q(y) * env;
    }

    // ------------------------------------------------------------------ DELAY (clock domain)
    inline float dRead(float d) const {
        const float room = ((float)dValid - 2.f - d) * (1.f / 96.f); if (room <= 0.f) return 0.f;   // echo onset fades in over 96 samples (no click when the buffer first reaches the tap)
        const float gg = room > 1.f ? 1.f : room;
        const uint32_t i = (uint32_t)d; const float f = d - (float)i; const uint32_t p = dW - 1 - i;
        const float a = dBuf[p & dMask], b = dBuf[(p - 1) & dMask]; return (a + (b - a) * f) * gg;
    }
    inline float delayTick(float x, float Dt, float fb, float inG) {
        if (!dFading && fabsf(Dt - dA) > 0.5f) { if (dValid == 0 || dA < 2.f) dA = Dt; else { dB = Dt; dFading = true; dXf = 0.f; } }
        float y;
        if (dFading) {
            y = dRead(dA) * (1.f - dXf) + dRead(dB) * dXf; dXf += 1.f / (0.020f * sr);
            if (dXf >= 1.f) { dA = dB; dXf = 0.f; dFading = false; }
        } else y = dRead(dA);
        dHf += (y - dHf) * 0.55f; dHf = flush(dHf);
        dBuf[dW] = flush(cubicClip(x * inG + dHf * fb)); dW = (dW + 1) & dMask; if (dValid < dMask) dValid++;
        return y;
    }

    // ------------------------------------------------------------------ REVERB (clock domain): multi-tap cluster + 4-line FDN, smear blends them
    inline float reverbTick(float x, float szT, float smear, float g0, float dA_, float inG) {
        rvIn += (inG - rvIn) * 0.002f;
        const float in = x * rvIn;
        // diffusion
        float d = ap1[ap1i], w = in + 0.6f * d; ap1[ap1i] = w; float u = d - 0.6f * w; if (++ap1i >= ap1N) ap1i = 0;
        d = ap2[ap2i]; w = u + 0.6f * d; ap2[ap2i] = w; u = d - 0.6f * w; if (++ap2i >= ap2N) ap2i = 0;
        // multi-tap on the raw input
        tapBuf[tapW & tapMask] = in;
        static const float TG[5] = { 0.8f, -0.65f, 0.55f, -0.45f, 0.35f };
        float taps = 0.f;
        if (!tapMoving) { for (int k = 0; k < 5; k++) taps += TG[k] * tapBuf[(tapW - tapD[k]) & tapMask]; }   // still: integer taps
        else for (int k = 0; k < 5; k++) {                                                                      // TIME moving: delays glide per sample, linear interpolation (no block zipper)
            const float dd = tapDf[k]; tapDf[k] = dd + tapInc[k]; const uint32_t i = (uint32_t)dd; const float f = dd - (float)i; const uint32_t p = tapW - i;
            const float a = tapBuf[p & tapMask], b = tapBuf[(p - 1) & tapMask]; taps += TG[k] * (a + (b - a) * f);
        }
        tapW++;
        // FDN
        float o[4];
        for (int k = 0; k < 4; k++) { float y = fl[k][fW[k]]; fLp[k] += (y - fLp[k]) * 0.45f; fLp[k] = flush(fLp[k]); o[k] = fLp[k]; }
        const float s = 0.5f * (o[0] + o[1] + o[2] + o[3]), fin = u * (0.35f + 0.65f * smear);
        for (int k = 0; k < 4; k++) { float m = o[k] - s; fl[k][fW[k]] = flush(fin + g0 * m); if (++fW[k] >= fN[k]) fW[k] = 0; }
        const float fo = 0.5f * (o[0] - o[1] + o[2] - o[3]);
        return (1.f - smear) * taps * 0.6f + smear * (fo * 0.9f + taps * 0.2f);
    }

    // ------------------------------------------------------------------ LOOP channel
    inline float rdLoop(float idx, bool interp) const {
        const int bi = (int)idx; int p = lstart + bi; if (p >= LN) p -= LN;
        if (!interp) return lbuf[p];
        int p1 = p + 1; if (p1 >= LN) p1 -= LN; if (bi + 1 >= llen) p1 = p;   // never read past the captured end (that sample is stale / not yet written)
        return lbuf[p] + (lbuf[p1] - lbuf[p]) * (idx - (float)bi);
    }
    inline void ovWrite(int idx, float xo) { if (idx < 0 || idx >= llen) return; int p = lstart + idx; if (p >= LN) p -= LN; lbuf[p] = flush(cubicClip(lbuf[p] + xo)); }
    // overdub write (cold path, kept out of line so the playback loop stays in registers): once per loop index the head crosses, gaps up to 8 filled, so a slow/fast head does not build a stair-step into the loop
    __attribute__((noinline)) int ovStep(int lwi, int ip, float xo) {
        if (lwi < 0) ovWrite(ip, xo);
        else if (ip != lwi) { const int d = ip - lwi, ad = d < 0 ? -d : d, sg = d < 0 ? -1 : 1; if (ad > 8) ovWrite(ip, xo); else for (int k = 1; k <= ad; k++) ovWrite(lwi + sg * k, xo); }
        return ip;
    }
    inline float loopTick(LoopSt& q, float x, float llenK, float lmodK, float thrO, float thrC) {
        const float step = fadeStep;
        if (lst == LS_LISTEN) { lbuf[q.lw] = x; if (++q.lw >= LN) q.lw = 0; if (q.lvalid < LN) q.lvalid++; }
        const float lgT = lst != LS_LISTEN ? 1.f : 0.f;
        if (q.lg < lgT) { q.lg += step; if (q.lg > lgT) q.lg = lgT; } else if (q.lg > lgT) { q.lg -= step; if (q.lg < lgT) q.lg = lgT; }
        if (q.lg <= 0.f) return 0.f;
        if (q.lmCur != lmPend) { q.lmG -= step; if (q.lmG <= 0.f) { q.lmG = 0.f; q.lmCur = lmPend; q.enter(); } }
        else if (q.lmG < 1.f) { q.lmG += step; if (q.lmG > 1.f) q.lmG = 1.f; }
        const float dip = 96.f * ks, len = (float)llen, rate = cRate; const bool interp = cInterp; (void)lmodK;
        float y = 0.f, hd = 0.f;
        if (q.lmCur == L_TAPE) {
            const float inc = tapeSpd * rate, Lw = tapeLw;
            q.lph += inc; if (q.lph >= Lw) q.lph = fmodf(q.lph, Lw); if (q.lph < 0.f) { q.lph = fmodf(q.lph, Lw); if (q.lph < 0.f) q.lph += Lw; }
            const float dd = fminf(q.lph, Lw - q.lph);
            y = rdLoop(q.lph, interp && (tapeSpd * rate != 1.f)) * (dd < dip ? dd * (1.f / dip) : 1.f); hd = q.lph;
        } else if (q.lmCur == L_ENV) {
            float S = (480.f + llenK * llenK * 0.5f * sr) * ks; if (S > len) S = len;
            const float a = fabsf(x); q.envv += (a - q.envv) * (a > q.envv ? 0.02f : 0.0007f); q.envv = flush(q.envv);
            q.gate = q.envv > thrO || (q.gate && q.envv > thrC);
            if (q.gate && !q.holding) { q.holding = true; q.holdStart = floorf(q.lph / S) * S; }
            else if (!q.gate && q.holding) q.holding = false;
            q.lph += rate;
            if (q.holding) { if (q.lph < q.holdStart) q.lph = q.holdStart; if (q.lph >= q.holdStart + S) q.lph = q.holdStart + fmodf(q.lph - q.holdStart, S); }
            if (q.lph >= len) q.lph = fmodf(q.lph, len);
            const float hbT = q.holding ? 1.f : 0.f; q.hb += (hbT - q.hb) * 0.01f;
            const float d1 = fminf(q.lph, len - q.lph), seamG = d1 < dip ? d1 / dip : 1.f;
            float sliceG = 1.f; if (q.holding || q.hb > 0.001f) { const float d2 = fminf(q.lph - q.holdStart, q.holdStart + S - q.lph); sliceG = d2 < dip ? (d2 < 0.f ? 0.f : d2 / dip) : 1.f; }
            y = rdLoop(q.lph, interp) * (seamG + (sliceG - seamG) * q.hb); hd = q.lph;
        } else {   // STRETCH
            float G = (960.f + llenK * 18240.f) * ks; if (G > len) G = len;
            float s; if (lmodK >= 0.98f) s = 1.f; else if (lmodK >= 0.5f) s = (lmodK - 0.5f) / 0.48f; else s = -(0.5f - lmodK) / 0.5f;
            q.spos += s; if (q.spos >= len) q.spos -= len; else if (q.spos < 0.f) q.spos += len;
            if (--q.gcnt <= 0) { q.gcnt += (int)(G * 0.5f); q.gi ^= 1; q.gidx[q.gi] = q.spos; q.gage[q.gi] = 0.f; }
            const float dinc = (s < 0.f ? -1.f : 1.f) * rate, iG = 1.f / G;
            for (int k = 0; k < 2; k++) {
                if (q.gage[k] >= G) continue;
                const float t = q.gage[k] * iG, x2 = 2.f * (t < 0.5f ? t : 1.f - t), w = x2 * x2 * (3.f - 2.f * x2);   // smoothstep; two grains 50 % apart sum to exactly 1
                float idx = q.gidx[k]; if (idx >= len) idx -= len; else if (idx < 0.f) idx += len; if (idx >= len || idx < 0.f) idx = 0.f;
                const float dd = fminf(idx, len - idx);
                y += rdLoop(idx, interp && rate != 1.f) * w * (dd < dip ? dd * (1.f / dip) : 1.f); q.gage[k] += 1.f; q.gidx[k] = idx + dinc;   // rate 1: grain read positions are integer-aligned, no lerp needed
            }
            hd = q.spos;
        }
        const float ovT = lst == LS_OVR ? 1.f : 0.f; if (q.ovG != ovT) { q.ovG += (ovT - q.ovG) * 0.003f; if (ovT == 0.f && q.ovG < 0.002f) q.ovG = 0.f; }
        if (q.ovG > 0.002f) {   // overdub: write once per loop index the head crosses (gaps up to 8 filled), so a slow/fast head does not build a stair-step into the loop
            q.lwi = ovStep(q.lwi, (int)hd, x * q.ovG);
        } else q.lwi = -1;
        gL.peak(fabsf(y));
        return gL.q(y) * q.lg * q.lmG;
    }

    void loopBlock(int n, float llenK, float lmodK, float thrO, float thrC) {
        LoopSt q = ls;
        for (int i = 0; i < n; i++) loBuf[i] = loopTick(q, mono[i], llenK, lmodK, thrO, thrC);
        ls = q;
    }

    // ------------------------------------------------------------------ process
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        for (int i = 0; i < n; i++) { dryL[i] = L[i]; dryR[i] = stereoIn ? R[i] : L[i]; mono[i] = stereoIn ? 0.5f * (L[i] + R[i]) : L[i]; }
        control(n);
        const float mixT = sm[P_MIX];
        if (!primed) { mixCur = mixT; primed = true; }
        const float minc = (mixT - mixCur) / (float)n; float m = mixCur;
        const float loopLvl = clampf(sm[P_LOOP], 0.f, 1.f), llenK = clampf(sm[P_LLEN], 0.f, 1.f), lmodK = clampf(v[P_LMOD], 0.f, 1.f);
        const int route = (int)(v[P_ROUTE] + 0.5f);
        const bool frz = frzAct || v[P_FREEZE] > 0.5f; const float frzT = frz ? 1.f : 0.f;
        { static const float SP[8] = { -4.f, -2.f, -1.f, -0.5f, 0.5f, 1.f, 2.f, 4.f }; tapeSpd = SP[(int)(lmodK * 7.f + 0.5f)];
          float Lw = (float)llen * (0.0625f + 0.9375f * llenK * llenK); const float mn = 480.f * ks; if (Lw < mn) Lw = mn; if (Lw > (float)llen) Lw = (float)llen; tapeLw = Lw > 1.f ? Lw : 1.f; }
        const float thrO = dbToLin(-30.f - 48.f * lmodK), thrC = 0.7f * thrO;
        const float tm = clampf(sm[P_LENGTH], 0.f, 1.f), md = clampf(sm[P_MOD], 0.f, 1.f);
        // reverb / delay block params
        if (wmCur != W_SLIP && (tm != lastTm || cRate != lastRate)) {   // reverb/delay block params, recomputed only when TIME or the clock rung moves
            lastTm = tm; lastRate = cRate;
            // reverb ticks at <= 1x (CPU): an up-rung is rendered as a smaller, shorter-decaying room (size and RT60 divided by the rate) instead of a 2nd tick
            const float ru = cRate > 1.f ? cRate : 1.f;
            const float rt = 0.25f * powf(32.f, tm) / ru; szT = (0.4f + 1.2f * tm) / ru;
            rgBase = powf(10.f, -3.f * (0.5f * (float)fN[0] + 0.5f * (float)fN[3]) / (rt * sr));
            Dt0 = clampf(30.f * powf(1000.f / 30.f, tm) * 0.001f * sr, 4.f, (float)dMask - 8.f);
        }
        const float rg = frz ? 0.99995f : rgBase, Dt = Dt0;
        if (wmCur == W_REVERB) {
            static const float TT[5] = { 13.f, 29.f, 47.f, 71.f, 103.f };
            szCur += (szT - szCur) * 0.15f; bool mv = false; const float lo = 2.f, hi = (float)tapMask - 4.f;
            for (int k = 0; k < 5; k++) {
                float tg = TT[k] * szCur * sr * 0.001f; tg = tg < lo ? lo : (tg > hi ? hi : tg);
                if (!tapMoving) tapDf[k] = (float)tapD[k] + 0.f;                       // still -> start of a glide from the integer position
                const float diff = tg - tapDf[k]; tapInc[k] = diff * (1.f / (float)n);
                if (fabsf(diff) > 0.01f) mv = true;
            }
            if (mv) { tapMoving = true; }
            else { tapMoving = false; for (int k = 0; k < 5; k++) { float tg = TT[k] * szCur * sr * 0.001f; tg = tg < lo ? lo : (tg > hi ? hi : tg); tapD[k] = (uint32_t)(tg + 0.5f); tapDf[k] = (float)tapD[k]; } }
        }
        float dfb = md; if (frz) dfb = 1.f;
        if (lst == LS_LISTEN || (loopLvl > 0.0005f)) loopBlock(n, llenK, lmodK, thrO, thrC); else for (int i = 0; i < n; i++) loBuf[i] = 0.f;   // loop level 0 while playing: playback is not computed (state holds)
        for (int i = 0; i < n; i++) {
            const float x = mono[i];
            if (frzG != frzT) { frzG += (frzT - frzG) * 0.002f; if (fabsf(frzT - frzG) < 1e-4f) frzG = frzT; }
            const float inG = 1.f - frzG;
            const float lo = loBuf[i] * loopLvl;
            const float wi = route == 0 ? x : (route == 1 ? x + lo : lo);
            // wet engine (with 10 ms dip-switch on mode change)
            if (wmCur != wmPend) { wmG -= fadeStep; if (wmG <= 0.f) { wmG = 0.f; wmCur = wmPend; enterWet(wmCur); } }
            else if (wmG < 1.f) { wmG += fadeStep; if (wmG > 1.f) wmG = 1.f; }
            float wet;
            if (wmCur == W_SLIP) {
                wet = slipTick(wi, frz);
                if (hitCnt > 0) { hit -= (hit - 0.2f) / (float)hitCnt; hitCnt--; wet *= hit; } else if (hit < 0.9999f) { hit += (1.f - hit) * hitRec; wet *= hit; } else hit = 1.f;
            } else {
                acc += (wmCur == W_REVERB ? (cRate < 1.f ? cRate : 1.f) : cTickRate); float sum = 0.f; int nt = 0;
                while (acc >= 1.f) {
                    acc -= 1.f; nt++;
                    sum += (wmCur == W_REVERB) ? reverbTick(wi, szT, md, rg, 0.f, inG) : delayTick(wi, Dt, dfb, inG);
                }
                if (nt) held = sum / (float)nt;
                gW.peak(fabsf(held));
                wet = gW.q(held);
            }
            if (tA < 1.f) { tLp += (wet - tLp) * tA; tLp = flush(tLp); wet = tLp; }
            float total = wmG < 1.f ? wet * wmG : wet; if (route == 0) total += lo;
            total = clampf(total, -4.f, 4.f);
            L[i] = dryL[i] + (total - dryL[i]) * m; R[i] = dryR[i] + (total - dryR[i]) * m; m += minc;
        }
        gW.end(n, sr); gL.end(n, sr); mixCur = mixT; held = flush(held);
        if (!(L[n - 1] * 0.f == 0.f) || !(R[n - 1] * 0.f == 0.f)) {   // last-resort NaN/inf guard: should be unreachable (all reads valid-gated); emit dry and cold-restart
            for (int i = 0; i < n; i++) { L[i] = dryL[i]; R[i] = dryR[i]; } wake();
        }
        return true;
    }
};
inline Fx* fx_create_moodii(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxMoodII)); return m ? new (m) FxMoodII(a, sr) : nullptr; }
}

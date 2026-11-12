// fx.cpp — the FX HOST for the v2 punch-in banks (fx_v2.cpp holds the verbatim v2 DSP).
// Here: bank masks (live | automation), the v2 gridTick-side effects for bank 0 (ST4/8/16/32,
// BZZ retrigger with the v2 LIVE LATCH, HALF/DBL/STOP shadow clock, WASH), the FX-page accel
// tilt filter (v2 L4993-5002, mirrored per EMGOR: negative = closed, positive = open, frozen
// off the page), automation record/playback, the distortion set + crossfade, and the
// drum/sampler performance throws.
#include <string.h>
#include <math.h>
#include "fx.h"
#include "fx_v2.h"
#include "fx_internal.h"
#include "fx_host.h"
#include "sequencer.h"
#include "shared_state.h"
#include "clock.h"
#include "render.h"
#include "engine_util.h"
#include "synth.h"
#include "output.h"

FxState fx;

FxHostStut fx_host_stut;
FxHostVs   fx_host_vs;

// ---------------------------------------------------------------- v2 bank-0 pad numbers
enum { B0_ST4 = 0, B0_ST8, B0_ST16, B0_ST32, B0_REV, B0_CHOR, B0_PAN, B0_CRUSH,
       B0_HALF, B0_DBL, B0_BZZ, B0_MUTE, B0_FLT, B0_WASH, B0_DIST, B0_STOP };

// ---------------------------------------------------------------- stutter/BZZ retrigger (v2 gridTick L1905-2137)
static uint16_t   last_masks[ENG_COUNT];    // rt_dr / rt_notes: the content registers
static volatile uint16_t s_live_masks[ENG_COUNT];   // v2 live_dr / live_nt: manual hits since the last step
static FxRetrigCb s_retrig_cb = nullptr;
static uint32_t   s_abs_sample = 0;
static uint32_t   s_step_rw = 0;             // capture-ring head at the last 16th boundary (REV anchor)

void fxNoteLive(uint8_t engine, uint8_t pad) {
    if (engine < ENG_COUNT && pad < 16) s_live_masks[engine] |= (uint16_t)(1u << pad);
}

static uint32_t s_bzz_rng = 0xC0FFEEu;
void fxReseed(uint32_t s) { s_bzz_rng = 0xC0FFEEu; if (s) s_bzz_rng ^= s * 2654435761u; fxV2Reseed(s); }
static void fxRetrigTick(float step_samples, uint32_t gstep, float fsr, size_t n) {
    static uint32_t s_period     = 0;
    static uint32_t s_next       = 0;
    static uint32_t s_last_gstep = 0xFFFFFFFFu;


    bool active = (fx_host_stut.rate != STUT_NONE) || fx_host_stut.bzz;
    if (g_render_notes_only) { s_period = 0; s_last_gstep = 0xFFFFFFFFu; return; }   // host: PMOR's own retrigger notes arrive over MIDI; retriggering them here too doubled every stutter
    if (!active) { s_period = 0; s_last_gstep = 0xFFFFFFFFu; return; }

    if (s_last_gstep != gstep) {
        s_last_gstep = gstep;
        uint32_t period;
        if (fx_host_stut.bzz) {
            static const uint8_t DIVS[5] = {2, 3, 4, 6, 8};
            uint32_t r  = lcg(s_bzz_rng);
            int      di = (int)(r % 3u) + (int)(fsr * 2.49f);
            if (di > 4) di = 4;
            if (di < 0) di = 0;
            period = (uint32_t)(step_samples / (float)DIVS[di]);
        } else {
            float mul = (fx_host_stut.rate == STUT_4)  ? 4.f
                      : (fx_host_stut.rate == STUT_8)  ? 2.f
                      : (fx_host_stut.rate == STUT_16) ? 1.f : 0.5f;
            period = (uint32_t)(step_samples * mul);
        }
        if (period < 128u) period = 128u;   // ~2.7 ms floor
        s_period = period;
        s_next   = s_abs_sample;            // fire right at this new grid boundary
    }
    int guard = 8;
    while (s_period && s_next < s_abs_sample + n && guard-- > 0) {
        if (s_retrig_cb) s_retrig_cb(last_masks);
        s_next += s_period;
    }
}

// ---------------------------------------------------------------- distortion crossfade
static uint8_t s_dist_prev       = DIST_TANH_ASYM;
static uint8_t s_dist_cur        = DIST_TANH_ASYM;
static float   s_dist_xfade      = 1.f;   // 1 = fully switched to cur
static float   s_dist_xfade_step = 0.f;

float fxDistXfade(float x, float drive) {
    if (fx.dist_algo != s_dist_cur) {
        s_dist_prev       = s_dist_cur;
        s_dist_cur        = fx.dist_algo;
        s_dist_xfade       = 0.f;
        s_dist_xfade_step = 1.f / (DIST_XFADE_MS * 0.001f * SR);
    }
    float outp = fxDistSample((DistAlgo)s_dist_cur, x, drive);
    if (s_dist_xfade < 1.f) {
        float outo = fxDistSample((DistAlgo)s_dist_prev, x, drive);
        outp = outo + (outp - outo) * s_dist_xfade;
        s_dist_xfade += s_dist_xfade_step;
        if (s_dist_xfade > 1.f) s_dist_xfade = 1.f;
    }
    return outp;
}


// ---------------------------------------------------------------- distortion set (was fx_down.cpp)
// DIST_FL needs a soft-knee clipper whose knee position tracks drive; a 32-row
// table (built once, lazily) avoids log10f/powf in the audio-rate call.
static FlClip s_flTable[32];
static bool   s_flTableReady = false;
static void ensureFlTable() {
    if (s_flTableReady) return;
    s_flTableReady = true;
    for (int i = 0; i < 32; i++) {
        float drive = 1.f + (19.f * (float)i) / 31.f;                 // 1..20
        float thresh_db = -4.4f - 20.f * log10f(drive);
        thresh_db = clampf(thresh_db, -40.f, 0.f);
        flclipSet(s_flTable[i], thresh_db, 0.f);
    }
}
static inline int flTableIndex(float drive) {
    int idx = (int)((drive - 1.f) * (31.f / 19.f) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > 31) idx = 31;
    return idx;
}

float fxDistSample(DistAlgo a, float x, float drive) {
    switch (a) {
        case DIST_TANH_ASYM: {                                        // V2 L3328-3331
            float dc = fxTanh(0.08f * drive) * 0.62f;
            return fxTanh((x + 0.08f) * drive) * 0.62f - dc;
        }
        case DIST_FOLD:                                               // V2 L2642-2645
            return sinf(x * drive * 1.5707963f) * 0.8f;
        case DIST_FUZZ: {                                              // V2 L2965-2970
            float y = x * drive + 0.15f;
            y = (y > 0.f) ? (1.f - fastExp(-y)) : (-1.f + fastExp(y));
            return y * 0.6f;
        }
        case DIST_WARM:                                                // softer-knee tanh
            return fxTanh(x * drive * 0.6f) * 0.9f + 0.1f * x;
        case DIST_FL: {
            ensureFlTable();
            return flclipSample(s_flTable[flTableIndex(drive)], x);
        }
        case DIST_CRUSH: {                                             // bit quantise only
            float bits = clampf(16.f - (drive - 1.f) * (14.f / 19.f), 2.f, 16.f);
            float q = fastPow2(bits);
            return floorf(x * q + 0.5f) / q;
        }
        default: return x;
    }
}
const char* const DIST_NAMES[DIST_COUNT] = { "TANH", "FOLD", "FUZZ", "WARM", "FL", "CRUSH" };

// ---------------------------------------------------------------- init
void fxInit() {
    memset(&fx, 0, sizeof(fx));
    fx.varispeed_req = 1.f;
    fx.perf_engine    = 0xFF;
    fx.dist_algo      = DIST_TANH_ASYM;
    for (int b = 0; b < FX_BANKS; b++) fx.bank_mix[b] = 1.f;
    fx.tilt_t         = 1.f;                // open (bypassed) until the FX page is tilted; then frozen where left
    memset(last_masks, 0, sizeof(last_masks));
    for (int e = 0; e < ENG_COUNT; e++) s_live_masks[e] = 0;
    s_retrig_cb = nullptr;
    fx_host_stut.rate = STUT_NONE; fx_host_stut.bzz = false;
    fx_host_vs.half = fx_host_vs.dbl = fx_host_vs.stop = false;
    s_dist_prev = s_dist_cur = fx.dist_algo;
    s_dist_xfade = 1.f; s_dist_xfade_step = 0.f;
    fxV2Init();
}

// effective bank masks = physically held | external (MIDI-in) | automation for the current step (unless paused by an outside edit)
static void fxEffective(uint16_t* out, float* eff_fsr, float fsr) {
    uint32_t loop_len = clk.loop_len ? clk.loop_len : 1u;
    uint32_t idx      = clk.gstep % loop_len;
    bool any_auto = false;
    for (int b = 0; b < 8; b++) {
        uint16_t a = (!g_render_notes_only && fx.auto_enabled && idx < MAXSTEPS) ? fx.auto_mask[idx][b] : 0;   // host (notes-only): FX loops live on PMOR, never replayed here
        a = (uint16_t)(a & ~fx.ovr[b]);
        if (a) any_auto = true;
        out[b] = (uint16_t)(fx.held[b] | fx.ext[b] | a);
    }
    float f = clampf(fsr, 0.f, 1.f);
    if (any_auto) { float af = fx.auto_fsr[idx] * (1.f / 255.f); if (af > f) f = af; }
    if (fx.ext_fsr > f) f = fx.ext_fsr;
    *eff_fsr = f;
}

void fxEffectiveAll(uint16_t out[8], float* fsr) { fxEffective(out, fsr, g.fsr); }
uint16_t fxEffectiveBank(uint8_t bank) { uint16_t eff[8]; float f; fxEffective(eff, &f, 0.f); return bank < 8 ? eff[bank] : 0; }
float    fxEffectiveFsr() { uint16_t eff[8]; float f; fxEffective(eff, &f, g.fsr); return f; }

// ---------------------------------------------------------------- AUDIO plane: per 16th
void fxOnStep(uint32_t gstep, uint16_t* fire_masks) {
    (void)gstep;
    s_step_rw = fxV2RingHead();                       // v2 L2029: ring position at the step boundary
    uint16_t eff[8]; float ef; fxEffective(eff, &ef, g.fsr);
    uint16_t b0 = eff[0];
    // bank-0 host effects follow the EFFECTIVE mask (live | automation), like v2's fx_eff
    fx_host_stut.rate = (b0 & (1u << B0_ST32)) ? STUT_32 : (b0 & (1u << B0_ST16)) ? STUT_16
                      : (b0 & (1u << B0_ST8))  ? STUT_8  : (b0 & (1u << B0_ST4))  ? STUT_4 : STUT_NONE;
    fx_host_stut.bzz = (b0 & (1u << B0_BZZ)) != 0;
    fx_host_vs.half  = (b0 & (1u << B0_HALF)) != 0;
    fx_host_vs.dbl   = (b0 & (1u << B0_DBL))  != 0;
    fx_host_vs.stop  = (b0 & (1u << B0_STOP)) != 0;
    bool stut_held = (fx_host_stut.rate != STUT_NONE) || fx_host_stut.bzz;

    // v2 L1999-2011: registers = pattern ∪ live latch, sticky while a retrig FX is held
    for (int e = 0; e < ENG_COUNT; e++) {
        uint16_t live = s_live_masks[e]; s_live_masks[e] = 0;
        uint16_t nm = (uint16_t)(fire_masks[e] | live);
        if (nm || !stut_held) last_masks[e] = nm;
    }
    // while a stutter/BZZ governs retriggering the audible hits come only from the retrig
    // timer — suppress the plain per-step firing (v2: fire path replaced by stutFire)
    if (stut_held) for (int e = 0; e < ENG_COUNT; e++) fire_masks[e] = 0;

    // automation recording: snapshot the physically-held banks + FSR into this step's slot
    {
        uint32_t loop_len = clk.loop_len ? clk.loop_len : 1u;
        uint32_t idx = clk.gstep % loop_len;
        if (fx.auto_rec && !g_render_notes_only && idx < MAXSTEPS) {   // host never records an FX loop either
            bool any = false;
            for (int b = 0; b < 8; b++) { fx.auto_mask[idx][b] = fx.held[b]; if (fx.held[b]) any = true; }
            fx.auto_fsr[idx] = (uint8_t)(clampf(g.fsr, 0.f, 1.f) * 255.f);
            if (any) fx.auto_enabled = true;
        }
    }
}

// ---------------------------------------------------------------- AUDIO plane: per block
void fxProcess(float* L, float* R, size_t n, float fsr, float lidar, float accel_x, bool on_fx_page) {
    uint16_t eff[8]; float eff_fsr; fxEffective(eff, &eff_fsr, fsr);
    (void)eff_fsr;

    // WASH (v2 L1880-1886): force long blurry tails on every synth voice while held
    syn.wash_coef = (eff[0] & (1u << B0_WASH)) ? (0.00012f - eff_fsr * 0.00008f) : 0.f;

    // FX-page accel tilt filter (v2 L4993-5002, MIRRORED per EMGOR 2026-09-15: negative accel X =
    // closed, positive = open; 500 Hz..5 kHz log sweep). Only moves while ON the FX page — elsewhere
    // it freezes where it was, so tilting during normal play never touches it.
    if (on_fx_page) {
        static float axf = 0.f;
        axf += (accel_x - axf) * 0.05f;                   // block rate: same ~150 ms settle as v2's 3 reads @ 50 ms
        float t = (0.9f + axf) / 1.1f;                    // ax=-0.9 -> 0 (closed) ... +0.2 -> 1 (open); rest ~0.82
        if (t > 1.f) t = 1.f; if (t < 0.f) t = 0.f;
        fx.tilt_t = t;
    }
    {
        static float tf = 0.f, lpL = 0.f, bpL = 0.f, lpR = 0.f, bpR = 0.f;
        float fc = 300.f * fastPow2(fx.tilt_t * 5.315f);    // 300 * 10^(1.6 t): 300 Hz .. 12 kHz, rest (t~0.82) ~6 kHz
        float f  = 2.f * sinf(3.14159265f * clampf(fc, 20.f, 0.45f * SR) / SR);
        tf += (f - tf) * 0.2f;
        const float q = 0.62f;                            // v2 Q1 (synth SVF Q)
        if (fx.tilt_t < 0.98f) {
            for (size_t i = 0; i < n; i++) {
                lpL += tf * bpL; float hL = L[i] - lpL - q * bpL; bpL += tf * hL;
                lpR += tf * bpR; float hR = R[i] - lpR - q * bpR; bpR += tf * hR;
                L[i] = lpL; R[i] = lpR;
            }
            lpL = clampf(flushBad(lpL), -6.f, 6.f); bpL = clampf(flushBad(bpL), -6.f, 6.f);
            lpR = clampf(flushBad(lpR), -6.f, 6.f); bpR = clampf(flushBad(bpR), -6.f, 6.f);
        } else { lpL = bpL = lpR = bpR = 0.f; }
    }

    FxV2Blk b;
    for (int k = 0; k < 8; k++) { b.bank[k] = eff[k]; b.amt[k] = fx.bank_amt[k]; b.lidf[k] = fx.bank_lid[k]; b.mix[k] = fx.bank_mix[k]; }
    b.fsr     = eff_fsr;
    b.lid     = clampf(lidar, 0.f, 1.f);
    b.gstep   = clk.gstep;
    b.step_ms = (uint32_t)(clk.step_samples * (1000.f / SR) + 0.5f);
    if (b.step_ms < 1u) b.step_ms = 1u;
    b.step_rw = s_step_rw;
    fxV2Block(b, L, R, n);

    fxRetrigTick(clk.step_samples, clk.gstep, eff_fsr, n);

    if (fx_host_vs.stop)      fx.varispeed_req = 0.06f;
    else if (fx_host_vs.dbl)  fx.varispeed_req = 2.f;
    else if (fx_host_vs.half) fx.varispeed_req = 0.5f;
    else                      fx.varispeed_req = 1.f;

    s_abs_sample += (uint32_t)n;
}

// ---------------------------------------------------------------- performance throws (§4.4)
// Applied to the drum/sampler perf bus in place. UP/RIGHT are additive sends (their own input
// stage is gated by a 5 ms envelope of the amount, so releasing cuts new material in while any
// existing tail keeps ringing on its own feedback); DOWN/LEFT are inserts crossfaded dry->wet
// by the same 5 ms envelope.
#define SPRING_AP_N 3
static const uint16_t SPRING_AP_LEN[SPRING_AP_N] = {225, 341, 441};
static float    spring_apL[SPRING_AP_N][512];
static float    spring_apR[SPRING_AP_N][512];
static uint16_t spring_ap_w[SPRING_AP_N];

#define SPRING_DISP_N 1536u
static float    spring_dispL[SPRING_DISP_N];
static float    spring_dispR[SPRING_DISP_N];
static uint16_t spring_disp_w = 0;
static float    spring_disp_lfo = 0.f;
static float    spring_disp_lpL = 0.f, spring_disp_lpR = 0.f;

#define SPRING_COMB_NL 4800u
#define SPRING_COMB_NR 5300u
static float SDRAM_BSS spring_combL[SPRING_COMB_NL];
static float SDRAM_BSS spring_combR[SPRING_COMB_NR];
static uint16_t spring_comb_wL = 0, spring_comb_wR = 0;

#define PERF_DLY_N 48000u   // 1 s safety cap
static float SDRAM_BSS perf_dlyL[PERF_DLY_N];
static float SDRAM_BSS perf_dlyR[PERF_DLY_N];
static uint32_t perf_dly_w = 0;

static bool    s_perf_init = false;
static OnePole s_gUp, s_gRight, s_gDown, s_gLeft;
static uint32_t s_bc_cnt   = 0;
static float    s_bc_holdL = 0.f, s_bc_holdR = 0.f;

void fxProcessPerf(float* L, float* R, size_t n) {
    if (!s_perf_init) {
        s_perf_init = true;
        memset(spring_apL, 0, sizeof(spring_apL));
        memset(spring_apR, 0, sizeof(spring_apR));
        memset(spring_ap_w, 0, sizeof(spring_ap_w));
        memset(spring_dispL, 0, sizeof(spring_dispL));
        memset(spring_dispR, 0, sizeof(spring_dispR));
        memset(spring_combL, 0, sizeof(spring_combL));
        memset(spring_combR, 0, sizeof(spring_combR));
        memset(perf_dlyL, 0, sizeof(perf_dlyL));
        memset(perf_dlyR, 0, sizeof(perf_dlyR));
        s_gUp.setMs(5.f, BLOCK_HZ); s_gRight.setMs(5.f, BLOCK_HZ);
        s_gDown.setMs(5.f, BLOCK_HZ); s_gLeft.setMs(5.f, BLOCK_HZ);
    }

    float upAmt    = s_gUp.process(clampf(fx.perf_up, 0.f, 1.f));
    float rightAmt = s_gRight.process(clampf(fx.perf_right, 0.f, 1.f));
    float downAmt  = s_gDown.process(clampf(fx.perf_down, 0.f, 1.f));
    float leftAmt  = s_gLeft.process(clampf(fx.perf_left, 0.f, 1.f));

    float delaySamp = clk.step_samples * 2.f;   // 1/8-note (2 sixteenths)
    if (delaySamp < 4.f) delaySamp = 4.f;
    if (delaySamp > (float)(PERF_DLY_N - 4u)) delaySamp = (float)(PERF_DLY_N - 4u);

    uint32_t bc_hold    = 1u + (uint32_t)(leftAmt * 30.f);
    float    bc_bits    = 16.f - leftAmt * 12.f;              // 16 (clean) .. 4 (crushed)
    float    bc_q       = fastPow2(bc_bits - 1.f);
    float    bc_invq    = 1.f / bc_q;
    float    dist_drive = 1.f + downAmt * 3.f;                // 1..4

    for (size_t i = 0; i < n; i++) {
        float inL = L[i], inR = R[i];
        float outL = inL, outR = inR;

        // ---- UP: spring reverb — 3 short allpass chains -> modulated ~30 ms dispersive
        // delay feeding back through a low-pass -> ~1.2 s comb tail
        {
            float apL = inL * upAmt, apR = inR * upAmt;
            for (int s = 0; s < SPRING_AP_N; s++) {
                uint16_t wi  = spring_ap_w[s];
                uint16_t len = SPRING_AP_LEN[s];
                float    dL  = spring_apL[s][wi];
                float    dR  = spring_apR[s][wi];
                const float ag = 0.5f;
                float yL = -ag * apL + dL; spring_apL[s][wi] = apL + ag * yL;
                float yR = -ag * apR + dR; spring_apR[s][wi] = apR + ag * yR;
                wi++; if (wi >= len) wi = 0;
                spring_ap_w[s] = wi;
                apL = yL; apR = yR;
            }
            spring_disp_lfo += 1.4f * (1.f / SR); if (spring_disp_lfo >= 1.f) spring_disp_lfo -= 1.f;
            float mod   = 6.f * sinf(spring_disp_lfo * 6.2831853f);
            float dtime = 1200.f + mod; if (dtime < 4.f) dtime = 4.f;
            float rdL = fxRingRead(spring_dispL, SPRING_DISP_N, spring_disp_w, dtime);
            float rdR = fxRingRead(spring_dispR, SPRING_DISP_N, spring_disp_w, dtime);
            spring_disp_lpL += 0.35f * (rdL - spring_disp_lpL);
            spring_disp_lpR += 0.35f * (rdR - spring_disp_lpR);
            spring_dispL[spring_disp_w] = flushDenorm(apL + spring_disp_lpL * 0.55f);
            spring_dispR[spring_disp_w] = flushDenorm(apR + spring_disp_lpR * 0.55f);
            spring_disp_w++; if (spring_disp_w >= SPRING_DISP_N) spring_disp_w = 0;

            float cbL = spring_combL[spring_comb_wL];
            float cbR = spring_combR[spring_comb_wR];
            spring_combL[spring_comb_wL] = flushDenorm(spring_disp_lpL + cbL * 0.5624f);
            spring_combR[spring_comb_wR] = flushDenorm(spring_disp_lpR + cbR * 0.5624f);
            spring_comb_wL++; if (spring_comb_wL >= SPRING_COMB_NL) spring_comb_wL = 0;
            spring_comb_wR++; if (spring_comb_wR >= SPRING_COMB_NR) spring_comb_wR = 0;

            outL += (spring_disp_lpL * 0.6f + cbL * 0.8f) * upAmt;
            outR += (spring_disp_lpR * 0.6f + cbR * 0.8f) * upAmt;
        }

        // ---- RIGHT: 1/8-note stereo delay throw, feedback 0.45 (tanh-saturated), 50/50 mix
        {
            float rdL = fxRingRead(perf_dlyL, PERF_DLY_N, perf_dly_w, delaySamp);
            float rdR = fxRingRead(perf_dlyR, PERF_DLY_N, perf_dly_w, delaySamp);
            perf_dlyL[perf_dly_w] = flushDenorm(inL * rightAmt + fxTanh(rdL * 0.45f));
            perf_dlyR[perf_dly_w] = flushDenorm(inR * rightAmt + fxTanh(rdR * 0.45f));
            perf_dly_w++; if (perf_dly_w >= PERF_DLY_N) perf_dly_w = 0;
            outL += rdL * 0.5f; outR += rdR * 0.5f;
        }

        // ---- DOWN: distortion (insert, dry/wet crossfade by amount)
        {
            float dL = fxDistXfade(inL, dist_drive);
            float dR = fxDistXfade(inR, dist_drive);
            outL += (dL - inL) * downAmt;
            outR += (dR - inR) * downAmt;
        }

        // ---- LEFT: bitcrush, rate + bits from the amount (insert, dry/wet crossfade)
        {
            if (s_bc_cnt == 0) { s_bc_holdL = inL; s_bc_holdR = inR; }
            s_bc_cnt++; if (s_bc_cnt >= bc_hold) s_bc_cnt = 0;
            float bcL = floorf(s_bc_holdL * bc_q) * bc_invq;
            float bcR = floorf(s_bc_holdR * bc_q) * bc_invq;
            outL += (bcL - inL) * leftAmt;
            outR += (bcR - inR) * leftAmt;
        }

        L[i] = flushBad(outL);
        R[i] = flushBad(outR);
    }
}

void fxSetRetrigCallback(FxRetrigCb cb) { s_retrig_cb = cb; }
bool fxStutterActive() { return fx_host_stut.rate != STUT_NONE || fx_host_stut.bzz; }

// ---------------------------------------------------------------- CONTROL plane
void fxPress(uint8_t id)   { if (id < FX_TOTAL) { uint16_t m = (uint16_t)(1u << (id & 15)); fx.held[id >> 4] |= m; fx.ovr[id >> 4] &= (uint16_t)~m; fx.ext[id >> 4] &= (uint16_t)~m; } }   // a physical hold takes the FX back from MIDI-in
void fxRelease(uint8_t id) { if (id < FX_TOTAL) fx.held[id >> 4] &= (uint16_t)~(1u << (id & 15)); }
void fxExtSet(uint8_t id, bool on) { if (id >= FX_TOTAL) return; uint16_t m = (uint16_t)(1u << (id & 15)); fx.ovr[id >> 4] |= m; if (on) fx.ext[id >> 4] |= m; else fx.ext[id >> 4] &= (uint16_t)~m; }
void fxReleaseAll()        { for (int b = 0; b < 8; b++) fx.held[b] = 0; }
void fxSetView(uint8_t bank) { fx.view = bank < FX_BANKS ? bank : 0; }
bool fxAnyHeld()           { for (int b = 0; b < 8; b++) if (fx.held[b]) return true; return false; }
bool fxIsHeld(uint8_t id)  { return id < FX_TOTAL && ((fx.held[id >> 4] >> (id & 15)) & 1u); }
const char* fxName(uint8_t id) { return id < FX_TOTAL ? FX_V2_NAMES[id >> 4][id & 15] : "?"; }

// TEST 12 (HX_MIN_DRONE): the only audio-path glue: dry mono in -> DroneMaker (gated) -> stereo out (audioCb sums to mono). Same symbol names as fx_glue_chain.h where main.cpp / lights.h use them.
#pragma once
#include "fx_glue_tmpl.h"
#include "fx_glue_drone_pfx.h"
#include "fx_glue_looper.h"
#include "fx_glue_harmony.h"
#include "fx_glue_reverb.h"                         // rvSetup / rvWake (the reverb's own musical defaults); its fader / accel hooks are NOT used here                           // generic gated glue (FxG / fxgRun): the chorus is the first template effect
static volatile float g_cb_cpu = 0.0f, g_cb_cpu_pk = 0.0f, g_cpu_slowpk = 0.0f, g_cpu_pk2 = 0.0f; static volatile bool g_shedReq = false; static uint32_t g_loadHist = 0;
static volatile float g_simFader[3] = {-1, -1, -1};        // 0..1000 or -1 = hardware (`sim fader`)
static volatile bool g_mute = false;
static volatile float g_noiseAmp = 0.0f;                   // `sim noise <dbfs>`
static volatile float g_toneHz = 440.0f, g_toneAmp = 0.0f; // `sim tone <hz> <dbfs>`
static volatile float g_olMin = 1e9f, g_olSum = 0.0f, g_olLvl = 0.0f; static volatile uint32_t g_olN = 0, g_olDrops = 0;   // output level before the mute (min / sum / count per frame, dropouts)
static volatile uint32_t g_xrunOver = 0, g_xrunGap = 0, g_cbMax = 0;
static volatile uint32_t g_secMax[9] = {0}, g_secWin[9] = {0};
#define SEC_T0(v) uint32_t v = DWT->CYCCNT
#define SEC_T1(i, v) { uint32_t d_ = DWT->CYCCNT - v; if(d_ > g_secMax[i]) g_secMax[i] = d_; if(d_ > g_secWin[i]) g_secWin[i] = d_; }
static volatile int g_focus = -1;
static void padTelemetry(); static void padHello();
static float g_cyc_per_sample = 8000.0f; static float g_dr_cyc_avg = 0.0f;   // drone load while it runs, fraction of a block (one-pole)
// CHORUS defaults (user 2026-11-17: slow and dramatic, no sensor mapping): RATE 0.12 Hz (very slow sweep), DEPTH 85 %, DELAY 22 ms (3 voices at 0.8 / 1.0 / 1.2 x), FB 20 % (of a 50 % max: a little),
// WIDTH 100 % (voices fully spread L / R), GAIN 0 dB (no level jump on engaging), HP 80 Hz / LP 12 kHz (effect defaults), MIX 50 %.
static void chSetup(hx::Fx* f) {
    auto S = [&](const char* n, float v) { int i = f->findParam(n); if(i >= 0) f->setParam(i, v); };
    S("RATE", 0.12f); S("DEPTH", 0.85f); S("DELAY", 22.0f); S("FB", 0.2f); S("WIDTH", 1.0f); S("GAIN", 0.0f); S("MIX", 0.5f);
}
static const FxDef FXD_chorusT = { "chorus", hx::fx_create_chorus, 24000, 0.038f, 2, { 255, 111, 176 }, chSetup, nullptr, nullptr, false, nullptr, nullptr };
// Template effects of this build, in chain order after the drone. No sensor mapping: ext = nullptr, no pre hook; every param is a free UI param.
static const FxDef FXD_reverbT = { "reverb", hx::fx_create_reverb, 480000, 0.073f, 5, { 155, 133, 255 }, rvSetup, rvPre, rvWake, false, nullptr, nullptr };   // rvPre: accel Y -> WARP and MODDEPTH (fx_controls_min.h)
static const FxDef FXD_grainT  = { "grain", hx::fx_create_grain, 1100000, 0.061f, 3, { 30, 230, 170 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
static volatile float g_fsr_p = 0.0f;                          // FSR pressure 0..1 (fx_controls_min.h); the tape delay shortens its time with it, the ringmod adds rate
static volatile float g_tapeLive = 375.0f, g_ringLive = 0.0f; static volatile int g_simEncDet = 0;
static void tapePre(hx::Fx* f) {
    hx::FxTape* t = static_cast<hx::FxTape*>(f); t->press = g_fsr_p; const float tm = f->v[hx::FxTape::P_TIME], p = g_fsr_p / 0.6f > 1.0f ? 1.0f : g_fsr_p / 0.6f;
    g_tapeLive = tm > 2.0f ? tm * powf(2.0f / tm, p) : tm;
}
// RINGMOD: the FREQ param is the BASE rate (encoder 1 steps it; FREQ == 1.0 means "0", off). The live rate = base + 800 Hz x pressure^2 (the FSR adds, releasing returns to the base), smoothed (~40 ms).
// The live rate is written into FREQ only for the duration of one process() call (pre sets it, ringPost restores the base), so the persisted / displayed value is always the base.
static float g_ringSm = 0.0f, g_ringSaved = -1.0f; static int g_ringFi = -2;
static void ringSetup(hx::Fx* f) { const int i = f->findParam("FREQ"); if(i >= 0) f->setParam(i, 1.0f); }
static void ringPre(hx::Fx* f) {
    if(g_ringFi == -2) g_ringFi = f->findParam("FREQ"); if(g_ringFi < 0) return;
    const float base = f->v[g_ringFi]; g_ringSaved = base; const float p = g_fsr_p, live = (base <= 1.001f ? 0.0f : base) + 800.0f * p * p;
    g_ringSm += 0.06f * (live - g_ringSm); g_ringLive = g_ringSm; f->v[g_ringFi] = g_ringSm < 1.0f ? 1.0f : (g_ringSm > 2000.0f ? 2000.0f : g_ringSm);
}
static void ringPost(hx::Fx* f) { if(g_ringSaved > 0.0f && g_ringFi >= 0) { f->v[g_ringFi] = g_ringSaved; g_ringSaved = -1.0f; } }
static const FxDef FXD_delayT  = { "tape", hx::fx_create_tape, 300000, 0.03f, 3, { 79, 192, 234 }, nullptr, tapePre, nullptr, false, nullptr, nullptr };
// spectral tracing lives in registry slot GI_BLUR (unused in this build), the auto-wah in GI_WAH. Trace: static AXI SRAM arena like the freeze (SDRAM is memory-bound on the device).
// ---- ONE shared AXI SRAM slot for the spectral effects that cannot run together (freeze, tracing). Their param OBJECTS stay resident (SDRAM), so params / pd / persistence work while they sleep;
// only the arrays live in the slot, and the large cold buffers (Stft ring, overlap-add, windows) go to SDRAM through Arena::cold. Waking the effect that does not own the slot rebuilds it in the
// main loop (axAcquire: save params, reset the slot, placement-new at the same address, init, restore params); the audio thread never allocates. If the other one is on, the wake is REFUSED.
// The drone keeps its own static AXI arrays (its glue belongs to the drone agent).
// AX_SLOT: the phase-cloud freeze needs ~95 KB HOT (host measurement: ok=0 at 40 and 64 KB, ok=1 at 96 KB; 50.7 KB more goes to the cold SDRAM arena); trace needs 27 KB. That was the freeze bug on padfx-4.8: a 40 KB slot made
// the freeze constructor fail (ok = 0), so the pad refused it ("effect unavailable"). 96 KB, paid for by trimming the drone arenas (fx_glue_drone_pfx.h).
static const size_t AX_SLOT = 96 * 1024, AX_COLD = 320 * 1024;
static uint8_t g_ax_mem[AX_SLOT] __attribute__((section(".axi_bss"), aligned(32)));
static uint8_t HX_SDRAM g_ax_cold_mem[AX_COLD] __attribute__((aligned(32)));
static uint8_t HX_SDRAM g_ax_obj[2][((sizeof(hx::FxFreeze) > sizeof(hx::FxTrace) ? sizeof(hx::FxFreeze) : sizeof(hx::FxTrace)) + 63) & ~(size_t)31] __attribute__((aligned(32)));
static hx::Arena g_ax_arena, g_ax_cold; static int g_ax_owner = -1; static size_t g_ax_used[2] = {0, 0};
static hx::Fx* axConstruct(int which, float sr) {
    g_ax_arena.bind(g_ax_mem, sizeof(g_ax_mem)); g_ax_cold.bind(g_ax_cold_mem, sizeof(g_ax_cold_mem)); g_ax_arena.cold = &g_ax_cold;
    hx::Fx* f = which == 0 ? static_cast<hx::Fx*>(new (g_ax_obj[0]) hx::FxFreeze(g_ax_arena, sr)) : static_cast<hx::Fx*>(new (g_ax_obj[1]) hx::FxTrace(g_ax_arena, sr));
    g_ax_owner = which; g_ax_used[which] = g_ax_arena.used; return f;
}
static hx::Fx* freezeCreate(hx::Arena&, float sr) { return axConstruct(0, sr); }
static hx::Fx* traceCreate(hx::Arena&, float sr) { return axConstruct(1, sr); }
// The phase-cloud freeze is about 7 dB quieter than the old one (host: a -30 dBFS tone frozen reads -38.6 dBFS rms at GAIN 1): GAIN default 2.0 (+6 dB, the param maximum) puts the frozen wet back above -30 dB peak.
static void freezeSetup(hx::Fx* f) { const int i = f->findParam("GAIN"); if(i >= 0) f->setParam(i, 2.0f); f->setParam(0, 0.9f); }   // + MIX 0.9 (was 0.65)
static const FxDef FXD_freeze = { "freeze", freezeCreate, 64, 0.05f, 4, { 63, 207, 156 }, freezeSetup, nullptr, nullptr, false, nullptr, nullptr };
static const FxDef FXD_traceT = { "trace", traceCreate, 64, 0.04f, 4, { 63, 207, 156 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
static const FxDef FXD_wahT = { "wah", hx::fx_create_wah, 4096, 0.02f, 2, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
// Main loop only. nullptr = the effect owns the slot (or needed none); otherwise the reason it cannot wake.
static const char* axAcquire(int gi) {
    if(gi != GI_FREEZE && gi != GI_BLUR) return nullptr;
    const int w = gi == GI_FREEZE ? 0 : 1; if(g_ax_owner == w) return nullptr;
    FxG& o = g_fxg[w ? GI_FREEZE : GI_BLUR]; if(o.latch || o.gate.state != FXS_IDLE) return w ? "no AXI memory: freeze is using it" : "no AXI memory: tracing is using it";
    FxG& g = g_fxg[gi]; if(!g.ok) return "effect unavailable";
    float vs[HX_MAXP]; const int np = g.fx->np; for(int i = 0; i < np && i < HX_MAXP; i++) vs[i] = g.fx->v[i];
    g.clearing = true; hx::Fx* f = axConstruct(w, 48000.0f);
    if(!f || !f->ok) { g.clearing = false; return "no AXI memory"; }
    f->init(48000.0f, 48.0f); for(int i = 0; i < np && i < HX_MAXP; i++) f->setParam(i, vs[i]); f->Fx::snap(); f->reset();
    g.clean = true; g.gate.state = FXS_IDLE; g.clearing = false; o.clean = true; return nullptr;   // the sleeping one is never reset: its arrays are no longer valid
}
// MOD BLOCK (chorus / phaser / flanger, MODE 0..2) lives in registry slot GI_FLANGE (id "mod"). wake() is non-virtual: called through the glue hook.
static void modWake(hx::Fx* f) { static_cast<hx::FxModBlock*>(f)->wake(); }
// JOYSTICK -> MOD BLOCK feedback. Neutral = "chill" (the block's own C_FB / P_FB / F_FB params, set low by modSetup). Pushing UP raises the active mode's feedback toward its MAX (params of the
// small "modjoy" panel, registry slot GI_MOOD) along u^CURVE (steep near the top). Down and neutral are the same chill value. Deadzone + ~30 ms smoothing. Active whenever the block is ON.
// JOY_UP_SIGN: +1 means a POSITIVE g_jn_out[1] is "up" (away from the wires). VERIFY on the rig: push the stick away from the wire side and watch Dash > Joystick y (or telemetry j[1]): it must go
// positive; if it goes negative set JOY_UP_SIGN to -1. `sim joy <y permille>` (test only) feeds the same path.
static const int JOY_UP_SIGN = +1;
static volatile float g_joyUp = 0.0f, g_joyFb = 0.0f, g_simJoyY = -2.0f; static float g_modSaved = 0.0f; static int g_modSavedIdx = -1;
struct FxModJoy : hx::Fx {
    enum { P_CMAX, P_PMAX, P_FMAX, P_CURVE, P_DEAD, P_SMOOTH };
    FxModJoy(hx::Arena&, float s) { name = "MODJOY"; sr = s;
        P(P_CMAX, "C_MAX", 0.f, 0.5f, 0.5f, 0.f, PF_PERCENT);            // chorus feedback at full push (the chorus feedback range ends at 0.5)
        P(P_PMAX, "P_MAX", 0.f, 0.95f, 0.92f, 0.f, PF_PERCENT);          // phaser
        P(P_FMAX, "F_MAX", 0.f, 0.95f, 0.95f, 0.f, PF_PERCENT);          // flanger
        P(P_CURVE, "CURVE", 1.f, 4.f, 2.5f, 0.f);                        // feedback = chill + (max - chill) x push^CURVE
        P(P_DEAD, "DEADZONE", 0.f, 0.3f, 0.08f, 0.f, PF_PERCENT);
        P(P_SMOOTH, "SMOOTH", 5.f, 200.f, 30.f, 0.f, PF_LOG); }          // ms
    bool process(const hx::FxCtx&, float*, float*, int, bool st) override { return st; }
    void reset() override {}
};
static uint8_t HX_SDRAM g_mj_obj[sizeof(FxModJoy) + 64] __attribute__((aligned(32)));
static void modSetup(hx::Fx* f) {                                          // CHILL defaults (user): feedback low, gentle depth
    auto S = [&](const char* n, float v) { const int i = f->findParam(n); if(i >= 0) f->setParam(i, v); };
    S("C_FB", 0.1f); S("P_FB", 0.3f); S("F_FB", 0.3f); S("C_DEPTH", 0.5f); S("P_DEPTH", 0.6f); S("F_DEPTH", 2.5f);
}
static void modPre(hx::Fx* f) {
    const int md = (int)(f->v[hx::FxModBlock::K_MODE] + 0.5f); const int idx = md == 0 ? hx::FxModBlock::K_CFB : (md == 1 ? hx::FxModBlock::K_PFB : hx::FxModBlock::K_FFB);
    hx::Fx* mj = g_glue[GI_MOOD].ok ? g_glue[GI_MOOD].fx : nullptr; if(!mj) return;
    const float mx = mj->v[md == 0 ? FxModJoy::P_CMAX : (md == 1 ? FxModJoy::P_PMAX : FxModJoy::P_FMAX)], chill = f->v[idx];
    float fb = chill + (mx > chill ? (mx - chill) : 0.0f) * g_joyUp; g_modSaved = chill; g_modSavedIdx = idx; g_joyFb = fb; f->v[idx] = fb;
}
static void modPost(hx::Fx* f) { if(g_modSavedIdx >= 0) { f->v[g_modSavedIdx] = g_modSaved; g_modSavedIdx = -1; } }
static const FxDef FXD_modT = { "mod", hx::fx_create_modblock, 200000, 0.10f, 2, { 255, 111, 176 }, modSetup, modPre, modWake, false, nullptr, nullptr };
// GLITCH (registry slot GI_ROAR, id "glitch"): the FSR drives INTENS while the glitch is the SELECTED effect: live = base + (1 - base) x pressure, written only for one process() call (pre / post restore,
// so the persisted / displayed INTENS is the base). g_glitchSel is set by the controls tick.
static volatile bool g_glitchSel = false; static volatile float g_glitchLive = 0.0f; static float g_glSaved = -1.0f; static int g_glFi = -2;
static void glitchWake(hx::Fx* f) { static_cast<hx::FxGlitch*>(f)->wake(); }
static void glitchPre(hx::Fx* f) {
    if(g_glFi == -2) g_glFi = f->findParam("INTENS"); if(g_glFi < 0) return;
    const float base = f->v[g_glFi]; g_glSaved = base; const float live = g_glitchSel ? base + (1.0f - base) * g_fsr_p : base; g_glitchLive = live; f->v[g_glFi] = live;
}
static void glitchPost(hx::Fx* f) { if(g_glSaved >= 0.0f && g_glFi >= 0) { f->v[g_glFi] = g_glSaved; g_glSaved = -1.0f; } }
static const FxDef FXD_glitchT = { "glitch", hx::fx_create_glitch, 560000, 0.04f, 3, { 200, 255, 60 }, nullptr, glitchPre, nullptr, false, nullptr, nullptr };   // no wake hook: reset() = wake() is a 512 KB memset (3.4 ms): fxClearPoll does it in the main loop while idle
// HARMONY level: device measurement 2026-11-17: the voice came out -10.4 dB re dry at MIX 0.7 / VOICE 0.6 (0.42 gain + the splice / low-pass loss), the host -4.5 dB. (re-measured with the correct dry RMS: VOICE 0.6 = -7.4 dB, 1.0 = -3.0 dB) VOICE 0.7 = -6.1 dB re dry at MIX 0.7.
static void harmSetup(hx::Fx* f) { const int i = f->findParam("VOICE"); if(i >= 0) f->setParam(i, 0.7f); }
static const FxDef FXD_harmonyT = { "harmony", hx::fx_create_harmony, 20480, 0.15f, 0, { 255, 190, 60 }, harmSetup, nullptr, nullptr, false, nullptr, nullptr };
// BLOOPER (registry slot GI_AUTOPAN, id "blooper"): a 60 s capture ring (11.5 MB, its own SDRAM block: the shared pool is smaller). It captures the input continuously, ON or OFF, so it is NOT run through the
// CPU gate: fxChain calls its process() every block (while off it is only the ring write), and the ON state is the effect's own param (A_MAIN toggles). Joystick Y -> SPEEDMUL while it is on.
static const FxDef FXD_blooperT = { "blooper", hx::fx_create_blooper, 8192, 0.01f, 3, { 255, 170, 40 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
static volatile float g_blSpeed = 0.0f, g_blCyc = 0.0f, g_blPk = 0.0f; static volatile int g_blOn = 0; static volatile bool g_blHas = false;
// MOOD II lives in registry slot GI_HARM (the old harmonizer slot, unused in this build): GI_MOODII is the alias, id "moodii".
static const int GI_MOODII = GI_HARM;
// MOOD II (registry slot GI_MOODII, id "moodii"): 2.07 MB in the SDRAM pool. No wake hook: its wake() clears ~2 MB, which would be a multi-ms spike in the audio thread; fxClearPoll clears it in the main loop while idle.
// MOOD II: the joystick drives CLKMOD (-1..+1: x0.5 .. x2 clock) while it is on; the base CLKMOD stays the stored value (written for one process() call only).
static volatile float g_moodJoy = 0.0f; static float g_moodSaved = 0.0f; static int g_moodFi = -2, g_moodSavedIdx = -1;
static void moodPre(hx::Fx* f) { if(g_moodFi == -2) g_moodFi = f->findParam("CLKMOD"); if(g_moodFi < 0) return; g_moodSaved = f->v[g_moodFi]; g_moodSavedIdx = g_moodFi; f->v[g_moodFi] = g_moodJoy; }
static void moodPost(hx::Fx* f) { if(g_moodSavedIdx >= 0) { f->v[g_moodSavedIdx] = g_moodSaved; g_moodSavedIdx = -1; } }
static const FxDef FXD_moodT = { "moodii", hx::fx_create_moodii, 2200000, 0.095f, 3, { 220, 60, 200 }, nullptr, moodPre, nullptr, false, nullptr, nullptr };
// DRONEMAKER (registry slot GI_CHORUS, id "dronemaker"): the faithful NEPTR port, 78.9 KB arena + 5.5 KB object, in the SDRAM pool. wake() is lazy (about 15 blocks, wet muted meanwhile): safe as the wake hook.
// GAIN default +6 dB above NEPTR's 0 dB: its wet sits about 9 dB under the dry at defaults, and the mic is quiet (UI note on the panel).
// C++14 (gnu++14): odr-used static constexpr members of the header need an out-of-class definition
constexpr int hx::FxDroneMaker::SCL[10][7]; constexpr int hx::FxDroneMaker::CNT[10]; constexpr float hx::FxDroneMaker::WET_TRIM;
static void dmWake(hx::Fx* f) { static_cast<hx::FxDroneMaker*>(f)->wake(); }
static void dmSetup(hx::Fx* f) { const int i = f->findParam("GAIN"); if(i >= 0) f->setParam(i, 6.0f); }
static const FxDef FXD_dmT = { "dronemaker", hx::fx_create_dronemaker, 100000, 0.158f, 4, { 63, 207, 156 }, dmSetup, nullptr, dmWake, false, nullptr, nullptr };
static const FxDef FXD_ringT = { "ring", hx::fx_create_ringmod, 4096, 0.011f, 1, { 255, 154, 77 }, ringSetup, ringPre, nullptr, false, nullptr, nullptr };
static const int8_t kTplChain[13] = { GI_HARMONY, GI_LOOPER, GI_FREEZE, GI_CHORUS, GI_BLUR, GI_FLANGE, GI_MOODII, GI_REVERB, GI_GRAIN, GI_DELAY, GI_ROAR, GI_RING, GI_WAH };   // GI_BLUR = trace   // generic-template effects in chain order; the drone runs between FREEZE and CHORUS
static void fxGlueInit() {
    g_ctx.sr = 48000.0f;                                                       // (this statement had been swallowed by a comment since 2.8: ctx.sr was 0 for every effect)
    drGlueInit(GI_DRONE); g_glue[GI_DRONE].lock[0] = "fader 1"; BC(0x51);          // drone MIX is the selected-effect fader (fx_controls_min.h)
    fxgInit(GI_FLANGE, FXD_modT); fxgInit(GI_MOODII, FXD_moodT); BC(0x52); fxgInit(GI_REVERB, FXD_reverbT); BC(0x53); fxgInit(GI_GRAIN, FXD_grainT); BC(0x54); fxgInit(GI_DELAY, FXD_delayT); BC(0x55);
    fxgInit(GI_FREEZE, FXD_freeze); fxgInit(GI_CHORUS, FXD_dmT); BC(0x56);
#ifndef HX_NO_HARMONY
    fxgInit(GI_HARMONY, FXD_harmonyT);
#endif
    BC(0x57); fxgInit(GI_BLUR, FXD_traceT); BC(0x58); fxgInit(GI_RING, FXD_ringT); BC(0x59); fxgInit(GI_ROAR, FXD_glitchT); fxgInit(GI_AUTOPAN, FXD_blooperT); BC(0x5A); fxgInit(GI_WAH, FXD_wahT); BC(0x5B); fxgInit(GI_LOOPER, FXD_looper); BC(0x5C);
    {   // the "modjoy" panel: an Fx that only holds the joystick-feedback params (never run), in registry slot GI_MOOD
        hx::Arena a; a.bind(g_mj_obj, sizeof(g_mj_obj)); FxModJoy* mj = new (g_mj_obj) FxModJoy(a, 48000.0f); mj->init(48000.0f, 48.0f); mj->Fx::snap();
        FxGlue& gl = g_glue[GI_MOOD]; gl.id = "modjoy"; gl.fx = mj; gl.ok = true; glueCaptureDefaults(gl);
    }
    BC(0x5D);
    if(g_glue[GI_REVERB].ok) { FxGlue& r = g_glue[GI_REVERB]; if(r.fx->role[hx::R_MOD] != 0xFF) r.lock[r.fx->role[hx::R_MOD]] = "accel Y"; if(g_rv_modd_idx >= 0) r.lock[g_rv_modd_idx] = "accel Y"; }   // WARP + MODDEPTH are accel-owned
    if(g_glue[GI_DRONE].ok) { const int gi = g_dr->findParam("GATE"); if(gi >= 0) { g_dr->setParam(gi, -75.0f); g_dr->Fx::snap(); g_glue[GI_DRONE].def[gi] = -75.0f; } }   // the mic is ~-70 dB at rest: a -54 dBFS gate never opens
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_cyc_per_sample = (float)HAL_RCC_GetSysClockFreq() / 48000.0f;
}
static volatile uint32_t g_mzC = 0, g_mzN = 0; static bool g_mzNeg = false;
static volatile float g_hwSq = 0.0f, g_hwPk = 0.0f; static volatile uint32_t g_hwN = 0;
static volatile float g_wetSq = 0.0f, g_wetPk = 0.0f; static volatile uint32_t g_wetN = 0;   // the drone's OWN wet signal (output minus dry), accumulated per telemetry frame
static void fxChain(const float* in, float* oL, float* oR, size_t n) {          // audio callback: never blocks. chain: dry -> looper -> freeze -> drone -> chorus -> reverb -> grain -> tape
    static float mm[128], tL[128], tR[128]; bool st = false; memcpy(oL, in, n * sizeof(float));
    {   // BLOOPER source = the LOOPER's loop: describe the looper's playback state to the blooper BEFORE the looper runs (its play head at block start); the blooper itself runs right after the looper stage
        FxG& b = g_fxg[GI_AUTOPAN]; FxG& lg = g_fxg[GI_LOOPER];
        if(b.ok && lg.ok) { hx::FxBlooper* bl = static_cast<hx::FxBlooper*>(b.fx); hx::FxLooper* lp = static_cast<hx::FxLooper*>(lg.fx);
            const bool playing = lg.gate.state != FXS_IDLE && lp->count > 0 && lp->len >= 64;
            const float gain = playing ? hx::dbToLin(lp->sm[hx::FxLooper::P_VOLUME]) * lp->sm[hx::FxLooper::P_MIX] * lp->playG : 0.0f;
            bl->setSource(lp->lay, lp->count, lp->len, lp->pp, hx::semiToRatio(lp->sm[hx::FxLooper::P_TRANSP]) * lp->dirCur, gain); g_blHas = bl->hasSource(); }
    }
    for(int k = 0; k < 14; k++) {                                                     // 13 template stages + the drone at position 3 (after harmony, looper, freeze)
        if(k == 3) {                                                                 // the drone stage (its glue is mono in, stereo out: dry + drone)
            if(g_drg.state != FXS_IDLE || g_dr_mix_req > 0.0005f) {
                const float* src = oL; if(st) { for(size_t i = 0; i < n; i++) mm[i] = 0.5f * (oL[i] + oR[i]); src = mm; }
                const uint32_t a = DWT->CYCCNT;
                if(drGlueBlock(src, tL, tR, n)) {
                    const float l = (float)(DWT->CYCCNT - a) / ((float)n * g_cyc_per_sample); g_dr_cyc_avg += 0.02f * (l - g_dr_cyc_avg);
                    float sq = 0.0f, pk = g_wetPk; for(size_t i = 0; i < n; i++) { const float w = tL[i] - src[i]; sq += w * w; const float aw = fabsf(w); if(aw > pk) pk = aw; } g_wetSq = g_wetSq + sq; g_wetN = g_wetN + (uint32_t)n; g_wetPk = pk;
                    if(st) { for(size_t i = 0; i < n; i++) { oL[i] += tL[i] - mm[i]; oR[i] += tR[i] - mm[i]; } } else { memcpy(oL, tL, n * sizeof(float)); memcpy(oR, tR, n * sizeof(float)); }
                    st = true;
                }
            } else g_dr_cyc_avg *= 0.98f;
            continue;
        }
        FxG& g = g_fxg[kTplChain[k > 3 ? k - 1 : k]]; if(!g.ok) continue; const uint32_t a = DWT->CYCCNT;
        static float mpre[128]; const bool isMood = kTplChain[k > 3 ? k - 1 : k] == GI_MOODII; if(isMood) memcpy(mpre, oL, n * sizeof(float));   // for the wet-pitch meter below
        const bool ran_ = fxgRun(g, oL, oR, st, n); if(kTplChain[k > 3 ? k - 1 : k] == GI_RING) ringPost(g.fx); else if(kTplChain[k > 3 ? k - 1 : k] == GI_FLANGE) modPost(g.fx); else if(kTplChain[k > 3 ? k - 1 : k] == GI_ROAR) glitchPost(g.fx); else if(kTplChain[k > 3 ? k - 1 : k] == GI_MOODII) moodPost(g.fx);
        if(isMood && ran_) { const float kh = g.fx->sm[0] > 0.5f ? g.fx->sm[0] - 0.5f : 0.0f; for(size_t i = 0; i < n; i++) { const float w = oL[i] - kh * mpre[i]; const bool neg = w < 0.0f; if(neg != g_mzNeg) { g_mzC = g_mzC + 1; g_mzNeg = neg; } } g_mzN = g_mzN + (uint32_t)n; }   // zero crossings of the wet alone (out minus the template's dry hold): pitch meter
        if(k == 0 && ran_) { float sq = 0.0f, pk = g_hwPk; for(size_t i = 0; i < n; i++) { const float w = oL[i] - in[i]; sq += w * w; const float aw = fabsf(w); if(aw > pk) pk = aw; } g_hwSq = g_hwSq + sq; g_hwN = g_hwN + (uint32_t)n; g_hwPk = pk; }   // the harmony voices' own level (output minus dry)
        if(ran_) { const uint32_t d = DWT->CYCCNT - a; const float l = (float)d / ((float)n * g_cyc_per_sample); g.cyc += 0.05f * (l - g.cyc); if(l > g.pk) g.pk = l; if(d > g.maxcyc) g.maxcyc = d;
            float bad = 0.0f; for(size_t i = 0; i < n; i++) bad += oL[i] - oL[i] + oR[i] - oR[i]; if(bad != 0.0f) { memset(oL, 0, n * sizeof(float)); memset(oR, 0, n * sizeof(float)); st = true; g.needReset = true; } }
        else g.cyc *= 0.95f;
        if(k == 1) {   // the BLOOPER stage, on the looper's output; no source = dry pass-through and zero work
            FxG& b = g_fxg[GI_AUTOPAN];
            if(b.ok && g_blHas) { hx::FxBlooper* bl = static_cast<hx::FxBlooper*>(b.fx); const uint32_t a2 = DWT->CYCCNT; const float base = bl->v[hx::FxBlooper::P_SPEED];
                bl->v[0] = b.mix; bl->v[hx::FxBlooper::P_SPEED] = g_blSpeed; bl->smoothBlock(); st = bl->process(g_ctx, oL, oR, n, st) || st; bl->v[hx::FxBlooper::P_SPEED] = base;
                const float l = (float)(DWT->CYCCNT - a2) / ((float)n * g_cyc_per_sample); g_blCyc += 0.02f * (l - g_blCyc); if(l > g_blPk) g_blPk = l; g_blOn = bl->isOn() ? 1 : 0; }
            else { g_blCyc *= 0.95f; if(b.ok) g_blOn = static_cast<hx::FxBlooper*>(b.fx)->isOn() ? 1 : 0; }
        }
    }
    if(!st) memcpy(oR, oL, n * sizeof(float));
    { float bad = 0.0f; for(size_t i = 0; i < n; i++) bad += oL[i] - oL[i] + oR[i] - oR[i]; if(bad != 0.0f) { memset(oL, 0, n * sizeof(float)); memset(oR, 0, n * sizeof(float)); } }
}
static inline void fxCpu(float load) {
    g_cb_cpu = g_cb_cpu + 0.05f * (load - g_cb_cpu); if(load > g_cb_cpu_pk) g_cb_cpu_pk = load;
    g_cpu_slowpk = load > g_cpu_slowpk ? load : g_cpu_slowpk * 0.99995f;
    g_cpu_pk2 = load > g_cpu_pk2 ? load : g_cpu_pk2 * 0.9995f;                          // ~2 s memory of the worst block: the admission rule's "projected peak" base
    g_loadHist = (g_loadHist << 1) | (load > 0.75f ? 1u : 0u);                          // runtime guard: 8 of the last 32 blocks over 75 %, or one block over 90 %
    if(load > 0.90f || __builtin_popcount(g_loadHist) >= 8) g_shedReq = true;
}
static void fxGlueTelemetry() {
    float pk = g_cb_cpu_pk; g_cb_cpu_pk = 0;
    puts_(",\"cpu\":{\"a\":"); putu((uint32_t)(g_cb_cpu * 1000.0f)); puts_(",\"p\":"); putu((uint32_t)(pk * 1000.0f)); puts_(",\"s\":"); putu((uint32_t)(g_cpu_slowpk * 1000.0f));
    puts_("},\"st\":{\"xrun\":"); putu(g_xrunOver); puts_(",\"gap\":"); putu(g_xrunGap); puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_(",\"cb\":"); putu(g_cbMax); puts_("},\"ol\":[");
    { auto db10 = [](float x) -> int32_t { return x > 1e-6f ? (int32_t)(200.0f * log10f(x)) : -1200; }; const float mn = g_olMin, av = g_olN ? g_olSum / (float)g_olN : 0.0f;
      putd(db10(mn > 1e8f ? 0.0f : mn)); put(','); putd(db10(av)); put(','); putu(g_olDrops); put(','); putd(db10(g_olLvl)); g_olMin = 1e9f; g_olSum = 0.0f; g_olN = 0; }
    puts_("]"); drGlueTelemetry(g_dr_cyc_avg);
    { puts_(",\"xf\":["); for(int k = 0; k < 13; k++) { FxG& c = g_fxg[kTplChain[k]]; if(k) put(','); put('['); putu((uint32_t)kTplChain[k]); put(','); putu(c.gate.state != FXS_IDLE ? 1 : 0); put(','); putu((uint32_t)(c.cyc * 1000.0f)); put(','); putu((uint32_t)(c.pk * 1000.0f)); put(','); putu((uint32_t)((c.ok ? c.fx->sm[0] : 0.0f) * 1000.0f)); put(']'); c.pk = 0.0f; } put(']'); }
    puts_(",\"bl\":["); putu((uint32_t)g_blOn); put(','); putu((uint32_t)(g_blCyc * 1000.0f)); put(','); putu((uint32_t)(g_blPk * 1000.0f)); put(','); { hx::FxBlooper* bq = g_fxg[GI_AUTOPAN].ok ? static_cast<hx::FxBlooper*>(g_fxg[GI_AUTOPAN].fx) : nullptr; putu(bq ? (uint32_t)(bq->rate() * 1000.0f) : 0); put(','); putu(g_blHas ? 1u : 0u); } put(']'); g_blPk = 0.0f;
    { const uint32_t zc = g_mzC, zn = g_mzN; g_mzC = 0; g_mzN = 0; puts_(",\"mz\":"); putu(zn ? (uint32_t)((float)zc * 0.5f * 48000.0f / (float)zn * 10.0f) : 0); }   // wet pitch, Hz x10, over one telemetry period
    { auto d10 = [](float x) -> int32_t { return x > 1e-7f ? (int32_t)(200.0f * log10f(x)) : -1400; }; const float hr = g_hwN ? sqrtf(g_hwSq / (float)g_hwN) : 0.0f, hp = g_hwPk; g_hwSq = 0.0f; g_hwN = 0; g_hwPk = 0.0f; puts_(",\"hw\":["); putd(d10(hr)); put(','); putd(d10(hp)); put(']'); }
    { auto db10 = [](float x) -> int32_t { return x > 1e-7f ? (int32_t)(200.0f * log10f(x)) : -1400; }; const float wr = g_wetN ? sqrtf(g_wetSq / (float)g_wetN) : 0.0f, wp = g_wetPk; g_wetSq = 0.0f; g_wetN = 0; g_wetPk = 0.0f;
      puts_(",\"dd\":["); putd(g_dr ? db10(g_dr->lastRms) : -1400); put(','); putd(g_dr ? db10(g_dr->floorLin) : -1400); put(','); putu(g_dr ? (uint32_t)(g_dr->gOut * 100.0f) : 0); put(','); putd(g_dr ? db10(g_dr->refMag) : -1400); put(','); putd(db10(wr)); put(','); putd(db10(wp)); put(']'); }
    padTelemetry();
}
static void fxGlueHello() { drGlueHello(); padHello(); }

// ---- output stage: master gain already applied; soft limiter at the very end (ceiling -1 dBFS, look-ahead-free), and the output meter
// Gain computer: g_target = (1 + r^4)^-0.25, r = peak / ceiling  ->  peak * g_target < ceiling for every r (soft knee, never over). Attack = instant (g follows the target down at once),
// release = one-pole 100 ms. Meter: post-limiter peak, RMS, and the deepest gain reduction since the last telemetry frame.
// ---- output CAPTURE (test tool): `cap` records 8192 samples of the final output (after the limiter, what is heard) and dumps them as `cap` lines of 64 int16; a host script rebuilds the wave for THD+N / spectrum analysis.
static int16_t g_cap[8192]; static volatile int g_capN = 0; static volatile bool g_capArm = false, g_capDone = false; static int g_capSent = 0;
static inline void capSample(float x) { if(g_capArm) { g_cap[g_capN] = (int16_t)(x > 1.0f ? 32767 : (x < -1.0f ? -32767 : x * 32767.0f)); if(++g_capN >= 8192) { g_capArm = false; g_capDone = true; g_capSent = 0; } } }
static volatile float g_moPk = 0.0f, g_moSq = 0.0f, g_moGr = 1.0f; static volatile uint32_t g_moN = 0;
static inline void limiterSample(float& L, float& R) {
    static float g = 1.0f; static const float ceil_ = 0.89125094f, rel = 1.0f - expf(-1.0f / (0.100f * 48000.0f));
    const float a = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R), r = a * (1.0f / ceil_), r2 = r * r;
    const float t = 1.0f / sqrtf(sqrtf(1.0f + r2 * r2));
    if(t < g) g = t; else g += rel * (t - g);
    L *= g; R *= g;
    const float o = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R); if(o > g_moPk) g_moPk = o; if(g < g_moGr) g_moGr = g;
    g_moSq = g_moSq + L * L + R * R; g_moN = g_moN + 2;
}
static void moTelemetry() {
    auto db10 = [](float x) -> int32_t { return x > 1e-6f ? (int32_t)(200.0f * log10f(x)) : -1200; };
    const float pk = g_moPk, rms = g_moN ? sqrtf(g_moSq / (float)g_moN) : 0.0f, gr = g_moGr;
    puts_(",\"mo\":["); putd(db10(pk)); put(','); putd(db10(rms)); put(','); putd(-db10(gr)); put(']');       // pk dBFS x10, rms dBFS x10, gain reduction dB x10 (positive = reducing)
    g_moPk = 0.0f; g_moSq = 0.0f; g_moN = 0; g_moGr = 1.0f;
}

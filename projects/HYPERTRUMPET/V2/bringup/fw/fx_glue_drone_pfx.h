// LOOSE FOLLOW (the drone) glue for padfx: a copy of fx_glue_drone.h (that file belongs to the drone agent and is untouched) with the arenas trimmed to what the rebuilt drone needs
// (host measurement 2026-11-17: 39,008 B hot + 10,752 B cold; the old 62 + 28 KB wasted 37 KB of the AXI SRAM, which the phase-cloud freeze needs).
static uint8_t g_dr_mem[44 * 1024] __attribute__((section(".axi_bss"), aligned(32)));
static uint8_t g_dr_cold_mem[14 * 1024] __attribute__((section(".axi_bss"), aligned(32)));
static hx::Arena g_dr_cold;
static hx::Arena g_dr_arena;
static hx::FxDrone* g_dr = nullptr;
static FxGate g_drg;
static volatile float g_dr_mix_req = 0.0f, g_dr_wet = 0.0f, g_dr_pk = 0.0f;   // g_dr_pk: worst block load since the last telemetry read (fraction of a 48-sample block)
static float g_dr_cps = 10000.0f;
static void drGlueInit(int slot) {
    g_dr_arena.bind(g_dr_mem, sizeof(g_dr_mem)); g_dr_cold.bind(g_dr_cold_mem, sizeof(g_dr_cold_mem)); g_dr_arena.cold = &g_dr_cold;
    g_dr = static_cast<hx::FxDrone*>(hx::fx_create_drone(g_dr_arena, 48000.0f));
    FxGlue& g = g_glue[slot]; g.id = "drone"; g.fx = g_dr; g.ok = g_dr && g_dr->ok;
    if(!g.ok) return;
    g_dr_cps = (float)HAL_RCC_GetSysClockFreq() / 48000.0f; g_dr->init(48000.0f, 48.0f); g_drg.smA = g_dr->smA;
    g_dr->setParam(0, 0.0f); g_dr->Fx::snap();                    // every other param keeps the effect's own musical defaults
    glueCaptureDefaults(g);
    g.lock[0] = "fader 2";
}
static void drGlueControls(float mix) { g_dr_mix_req = mix; }
static void drWake(float mixT) {
    g_dr->wake();                                            // light reset: per-bin state up to 8 kHz, no big memsets (they would spike one block)
    g_dr->v[0] = mixT; g_dr->sm[0] = 0.0f; g_dr->mixCur = 0.0f; g_dr->primed = true; g_dr->smA = g_drg.smA;   // fade in from 0
}
// mono in -> stereo out (dry + drone). Returns true when the drone ran.
static bool drGlueBlock(const float* in, float* oL, float* oR, size_t n) {
    for(size_t i = 0; i < n; i++) { oL[i] = in[i]; oR[i] = in[i]; }
    if(!g_glue[1].ok) { g_drg.state = FXS_IDLE; return false; }
    const float mixT = g_dr_mix_req; const uint32_t cyc0 = DWT->CYCCNT;
    if(gateEnter(g_drg, g_dr, mixT)) drWake(mixT);
    if(g_drg.state == FXS_IDLE) { g_dr_wet = 0.0f; return false; }              // no STFT, no oscillator bank
    g_dr->v[0] = (g_drg.state == FXS_TAIL) ? 0.0f : mixT;
    g_dr->smoothBlock();
    hx::FxCtx ctx = {}; ctx.sr = 48000.0f;
    g_dr->process(ctx, oL, oR, (int)n, false);
    g_dr_wet = g_dr->sm[0];
    gateLeave(g_drg, g_dr, n);
    { float l = (float)(DWT->CYCCNT - cyc0) / ((float)n * g_dr_cps); if(l > g_dr_pk) g_dr_pk = l; }
    return true;
}
static void drGlueTelemetry(float cyc) {
        puts_(",\"dr\":{\"on\":"); putu(g_drg.state != FXS_IDLE ? 1 : 0); puts_(",\"st\":"); putu((uint32_t)g_drg.state); puts_(",\"mix\":"); putu((uint32_t)(g_dr_mix_req * 1000.0f));
    puts_(",\"wet\":"); putu((uint32_t)(g_dr_wet * 1000.0f)); puts_(",\"seg\":"); putu(g_dr ? (uint32_t)(g_dr->tI * 1000.0f) : 0); puts_(",\"cyc\":"); putu((uint32_t)(cyc * 1000.0f)); puts_(",\"pk\":"); putu((uint32_t)(g_dr_pk * 1000.0f)); g_dr_pk = 0.0f; puts_("}");
}
static void drGlueHello() { puts_(",\"dri\":{\"ok\":"); put(g_glue[1].ok ? '1' : '0'); puts_("}"); }

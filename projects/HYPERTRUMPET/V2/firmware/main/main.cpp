// HYPERTRUMPET V2 main instrument firmware (skeleton). Daisy Seed 3, libDaisy v9, APP_TYPE=BOOT_SRAM (QSPI app under the Daisy bootloader).
//   audio  : 48 kHz, block 48, mono bell mic on line L (in[0]) -> 4-slot FX engine -> stereo out
//   control: main loop every 5 ms: sensors (hx_sensors.h) -> HxMapper::update (the SAME mapping the sim runs) -> engine
//   USB    : libDaisy CDC, newline text commands in, newline JSON out (protocol below). The sim UI can use it as a live editor.
//
// ============================== USB CDC PROTOCOL v1 (text lines, DTR asserted) ==============================
// host -> seed                                 seed -> host
//   hello                                        {"t":"hello","fw","proto":1,"clk","sr","blk","nfx","slots":4,"sdram":{...},"dev":{"imu","l1","l2"},"ms"}   (also 1 Hz)
//   schema                                       {"t":"fx","i","id","name","v1","np","roles":{"AMT":p,...}} then per param
//                                                {"t":"p","fx","i","n","min","max","def","step","fl","names":[..]}   ... then {"t":"schema_end","n"}
//   get                                          {"t":"slot","s","fx":"id"|null,"fxi","byp","np","v":[..]} x4, {"t":"patch_end","page","sel","status"}
//   set <slot> <idx> <value>                     {"t":"ack","c":"set","ok":1}      (Fx::setParam, clamps; knob semantics: mapper re-takes the param only when its physical control moves)
//   fx <slot> <id|none>                          {"t":"ack","c":"fx","ok":1}       (FxEngine::setEffect in the control thread; slot passes audio while swapping)
//   byp <slot> <0|1>   act <slot> <0..3>         MAIN/ALT/CLEAR/HOLD = Fx::action
//   sel <slot>   patch default                   mapper selectSlot / loadDefaultPatch
//   ctl <name> <value> | ctl release             host-controlled inputs: v0 v1 v2 f0 f1 f2 p0 p1 fsr jx jy tx ty shake lid pads (0..1, -1..1 for jx jy tx ty)
//   cal <ch> b0..b8 | calreset                   fader/pot taper LUT (ch 0..2 faders, 3..4 pots), same as bring-up
//   stream <0|1>                                 {"t":"tel",...} at 20 Hz: ms,in,out,f0,env,hint,cpu[avg%,peak%,over],slot[4 % of block],ctl[16],mm[2],dev
//   dfu                                          {"t":"dfu"} then reboot into the Daisy bootloader (DAISY_INFINITE_TIMEOUT)
// Param descriptor "fl" bits = ParamFlags (QUANT 1, PAIR_X 2, PERCENT 4, ENUM 8, BIPOLAR 16, HIDDEN 32, INT 64, LOG 128), same as the sim UI draws from.
// =============================================================================================================
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "daisy_seed.h"
#include "hyper_hw.h"
#include "hx_seed.h"
#include "hx_fault.h"
#include "fx_mapping.h"
#include "hx_sensors.h"

#define FW_VERSION "hx-main-0.1"
#ifndef HX_BOOST
#define HX_BOOST 1
#endif
using namespace daisy;

DaisySeed hw;
static const int   BLK = 48;
static const float SR  = 48000.f;
static hx::FxEngine g_eng;
static hx::HxMapper g_map;
static HxSensors    g_sens;
static HxOut        g_out;
static uint32_t     g_budget = 480000;

// scratch arena for schema generation (constructs each effect once to read its Param descriptors). 2 MiB of the SDRAM margin.
#define HX_SCRATCH_BYTES (2u * 1024u * 1024u)
HX_SDRAM static uint8_t g_scratchMem[HX_SCRATCH_BYTES] __attribute__((aligned(32)));

// ---------------------------------------------------------------- audio
static volatile uint32_t a_n = 0, a_peak = 0, a_over = 0; static volatile uint64_t a_sum = 0; static volatile bool a_clear = false;
static void audioCb(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    uint32_t t0 = hxCyc();
    g_eng.process(in[0], out[0], out[1], (int)size);              // mono bell mic = line L
    uint32_t cyc = hxCyc() - t0;
    if (a_clear) { a_n = 0; a_sum = 0; a_peak = 0; a_over = 0; a_clear = false; }
    a_n = a_n + 1; a_sum = a_sum + cyc; if (cyc > a_peak) a_peak = cyc; if (cyc > g_budget) a_over = a_over + 1;
}

// ---------------------------------------------------------------- command queue (ISR -> main)
static const int kQ = 8, kLen = 120;
static char g_q[kQ][kLen]; static volatile uint8_t g_qh = 0, g_qt = 0;
static void onLine(const char* l) {
    uint8_t nh = (g_qh + 1) % kQ; if (nh == g_qt) return;         // full: drop
    strncpy(g_q[g_qh], l, kLen - 1); g_q[g_qh][kLen - 1] = 0; g_qh = nh;
}
static volatile bool g_dfu = false;
extern "C" bool hyper_dfu_requested(void) { return g_dfu; }

// ---------------------------------------------------------------- protocol output
static const char* const kRoleName[hx::R_COUNT] = { "AMT", "TIME", "TONE", "MOD", "FB", "PITCH" };
static bool g_stream = false;

static void putDev() {
    g_out.s("\"dev\":{\"imu\":"); g_out.u(g_sens.imuOk); g_out.s(",\"l1\":"); g_out.u(g_L[0].st); g_out.s(",\"l2\":"); g_out.u(g_L[1].st); g_out.c('}');
}
static void sendHello() {
    g_out.begin();
    g_out.s("{\"t\":\"hello\",\"fw\":\""); g_out.s(FW_VERSION); g_out.s("\",\"proto\":1,\"clk\":"); g_out.u(g_hxClkMHz * 1000000u);
    g_out.s(",\"sr\":48000,\"blk\":"); g_out.u(BLK); g_out.s(",\"nfx\":"); g_out.u(hx::fxTypeCount()); g_out.s(",\"slots\":4,\"sdram\":{\"loop\":"); g_out.u(HX_LOOP_POOL_BYTES);
    g_out.s(",\"slots\":"); g_out.u(sizeof g_hxSlotMem); g_out.s(",\"scratch\":"); g_out.u(HX_SCRATCH_BYTES); g_out.s(",\"total\":67108864},");
    putDev(); g_out.s(",\"ms\":"); g_out.u(System::GetNow()); hxFaultJson(g_out); g_out.s("}"); g_out.end(hw);
}
static void ack(const char* c, bool ok) { g_out.begin(); g_out.s("{\"t\":\"ack\",\"c\":\""); g_out.s(c); g_out.s("\",\"ok\":"); g_out.u(ok); g_out.s("}"); g_out.end(hw); }
static void putNames(const Param& p) {
    if (!p.names) return;
    int cnt = (int)(p.max - p.min) + 1; g_out.s(",\"names\":[");
    for (int i = 0; i < cnt; i++) { if (i) g_out.c(','); g_out.str(p.names[i]); }
    g_out.c(']');
}
static void sendSchema() {
    hx::Arena scratch; hx::Arena* savedPool = hx::g_loopPool; hx::g_loopPool = nullptr;      // do NOT touch the live looper pool
    int n = hx::fxTypeCount();
    for (int i = 0; i < n; i++) {
        scratch.bind(g_scratchMem, HX_SCRATCH_BYTES);
        const hx::FxTypeInfo* ti = hx::fxType(i); hx::Fx* f = hx::fxCreate(i, scratch, SR); if (!f) continue;
        f->init(SR);
        g_out.begin(); g_out.s("{\"t\":\"fx\",\"i\":"); g_out.u(i); g_out.s(",\"id\":"); g_out.str(ti->id); g_out.s(",\"name\":"); g_out.str(ti->name);
        g_out.s(",\"v1\":"); g_out.str(ti->v1); g_out.s(",\"np\":"); g_out.u(f->np); g_out.s(",\"roles\":{");
        bool first = true; for (int r = 0; r < hx::R_COUNT; r++) if (f->role[r] != 0xFF) { if (!first) g_out.c(','); first = false; g_out.str(kRoleName[r]); g_out.c(':'); g_out.u(f->role[r]); }
        g_out.s("}}"); g_out.end(hw, 60);
        for (int p = 0; p < f->np; p++) {
            const Param& q = f->pt[p];
            g_out.begin(); g_out.s("{\"t\":\"p\",\"fx\":"); g_out.u(i); g_out.s(",\"i\":"); g_out.u(p); g_out.s(",\"n\":"); g_out.str(q.name);
            g_out.s(",\"min\":"); g_out.f(q.min, 4); g_out.s(",\"max\":"); g_out.f(q.max, 4); g_out.s(",\"def\":"); g_out.f(f->v[p], 4);
            g_out.s(",\"step\":"); g_out.f(q.step, 4); g_out.s(",\"fl\":"); g_out.u(q.flags); putNames(q); g_out.s("}"); g_out.end(hw, 60);
        }
        f->~Fx();
    }
    hx::g_loopPool = savedPool;
    g_out.begin(); g_out.s("{\"t\":\"schema_end\",\"n\":"); g_out.u(n); g_out.s("}"); g_out.end(hw, 60);
}
static void sendPatch() {
    for (int s = 0; s < HX_NSLOTS; s++) {
        hx::Fx* f = g_eng.fx(s); int t = g_eng.effectType(s);
        g_out.begin(); g_out.s("{\"t\":\"slot\",\"s\":"); g_out.u(s); g_out.s(",\"fx\":");
        if (t >= 0) g_out.str(hx::fxType(t)->id); else g_out.s("null");
        g_out.s(",\"fxi\":"); g_out.i(t); g_out.s(",\"byp\":"); g_out.u(g_eng.bypassed(s)); g_out.s(",\"np\":"); g_out.u(f ? f->np : 0); g_out.s(",\"v\":[");
        if (f) for (int p = 0; p < f->np; p++) { if (p) g_out.c(','); g_out.f(f->v[p], 4); }
        g_out.s("]}"); g_out.end(hw, 60);
    }
    g_out.begin(); g_out.s("{\"t\":\"patch_end\",\"page\":"); g_out.u(g_map.page()); g_out.s(",\"sel\":"); g_out.u(g_map.selectedSlot()); g_out.s(",\"status\":"); g_out.str(g_map.statusLine()); g_out.s("}"); g_out.end(hw, 60);
}
static uint32_t g_lastTel = 0;
static void sendTel(uint32_t now) {
    float blockNs = 1e9f * BLK / SR; uint32_t n = a_n;
    float avg = n ? 100.f * (float)(a_sum / n) / (float)g_budget : 0.f, pk = 100.f * (float)a_peak / (float)g_budget; uint32_t ov = a_over; a_clear = true;
    hx::HxControls c = g_sens.out();
    g_out.begin(); g_out.s("{\"t\":\"tel\",\"ms\":"); g_out.u(now); g_out.s(",\"in\":"); g_out.f(g_eng.readInPeak()); g_out.s(",\"out\":"); g_out.f(g_eng.readOutPeak());
    g_out.s(",\"f0\":"); g_out.f(g_eng.f0(), 1); g_out.s(",\"env\":"); g_out.f(g_eng.env()); g_out.s(",\"hint\":"); g_out.f(g_eng.valveHint(), 1);
    g_out.s(",\"cpu\":["); g_out.f(avg, 2); g_out.c(','); g_out.f(pk, 2); g_out.c(','); g_out.u(ov); g_out.s("],\"slot\":[");
    for (int k = 0; k < HX_NSLOTS; k++) { if (k) g_out.c(','); g_out.f(100.f * g_eng.slotNs(k) / blockNs, 2); }
    g_out.s("],\"ctl\":[");
    const float cv[K_N] = { c.valve[0], c.valve[1], c.valve[2], c.fader[0], c.fader[1], c.fader[2], c.pot[0], c.pot[1], c.fsr, c.joyX, c.joyY, c.tiltX, c.tiltY, c.shake, c.lidar, (float)c.pads };
    for (int k = 0; k < K_N; k++) { if (k) g_out.c(','); g_out.f(cv[k], 3); }
    g_out.s("],\"mm\":["); g_out.u(g_sens.mm[0]); g_out.c(','); g_out.u(g_sens.mm[1]); g_out.s("],"); putDev(); g_out.s("}"); g_out.end(hw, 2);
}

// ---------------------------------------------------------------- command handling (main loop)
static int tok(char* s, char** t, int max) { int n = 0; while (*s && n < max) { while (*s == ' ') *s++ = 0; if (!*s) break; t[n++] = s; while (*s && *s != ' ') s++; } return n; }
static void handle(char* line) {
    char* t[16]; int n = tok(line, t, 16); if (!n) return;
    const char* c = t[0]; int slot = n > 1 ? atoi(t[1]) : -1; bool okSlot = slot >= 0 && slot < HX_NSLOTS;
    if (!strcmp(c, "hello")) sendHello();
    else if (!strcmp(c, "schema")) sendSchema();
    else if (!strcmp(c, "get")) sendPatch();
    else if (!strcmp(c, "set") && n >= 4) { bool ok = okSlot && g_eng.fx(slot); if (ok) g_eng.setParam(slot, atoi(t[2]), strtof(t[3], nullptr)); ack("set", ok); }
    else if (!strcmp(c, "fx") && n >= 3) {
        bool ok = okSlot; if (ok) { int ty = strcmp(t[2], "none") ? hx::fxTypeById(t[2]) : -1; ok = (ty >= 0 || !strcmp(t[2], "none")) && g_eng.setEffect(slot, ty); }
        ack("fx", ok);
    }
    else if (!strcmp(c, "byp") && n >= 3) { if (okSlot) g_eng.setBypass(slot, atoi(t[2]) != 0); ack("byp", okSlot); }
    else if (!strcmp(c, "act") && n >= 3) { if (okSlot) g_eng.action(slot, atoi(t[2]), 1.f); ack("act", okSlot); }
    else if (!strcmp(c, "sel") && n >= 2) { if (okSlot) g_map.selectSlot(slot); ack("sel", okSlot); }
    else if (!strcmp(c, "patch") && n >= 2 && !strcmp(t[1], "default")) { g_map.loadDefaultPatch(); ack("patch", true); }
    else if (!strcmp(c, "ctl") && n >= 2) {
        if (!strcmp(t[1], "release")) { g_sens.releaseOverrides(); ack("ctl", true); }
        else { int k = g_sens.ctlByName(t[1]); if (k >= 0 && n >= 3) g_sens.setOverride(k, strtof(t[2], nullptr)); ack("ctl", k >= 0 && n >= 3); }
    }
    else if (!strcmp(c, "cal") && n >= 11) { uint16_t v[9]; for (int k = 0; k < 9; k++) v[k] = (uint16_t)atoi(t[2 + k]); ack("cal", g_sens.setCal(atoi(t[1]), v)); }
    else if (!strcmp(c, "calreset")) { g_sens.resetCal(); ack("cal", true); }
    else if (!strcmp(c, "stream")) { g_stream = n >= 2 && atoi(t[1]) != 0; ack("stream", true); }
    else if (!strcmp(c, "dfu")) g_dfu = true;
}

int main(void) {
    hw.Init(HX_BOOST != 0);                       // 480 MHz
    dfuBootGesture();                             // both DFU keys low at power-on -> Daisy bootloader
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);   // USB BEFORE any heavy load (PMOR rule)
    g_hxLineFn = onLine;
    hw.usb_handle.SetReceiveCallback(hxRxCb, UsbHandle::FS_INTERNAL);
    System::Delay(500);
    hxDwtInit();
    hxFaultBoot();                                // a HardFault from the previous run is reported in hello ("fault")
    g_budget = (uint32_t)((uint64_t)BLK * g_hxClkMHz * 1000000ull / 48000ull);
    hx::g_nowNs = hxNowNs;
    hxSdramInit(g_eng, SR);
    g_sens.init(hw);                              // ADC + I2C + both lidars (takes a few hundred ms)
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(BLK);
    hw.StartAudio(audioCb);
    g_map.init(&g_eng);                           // loads the default patch (control thread)

    uint32_t next = System::GetNow(), nextHello = 0, last = next;
    for (;;) {
        uint32_t now = System::GetNow();
        if (g_dfu) { g_out.begin(); g_out.s("{\"t\":\"dfu\"}"); g_out.end(hw); System::Delay(100); dfuCheck(); g_dfu = false; }
        while (g_qt != g_qh) { char l[kLen]; strcpy(l, g_q[g_qt]); g_qt = (g_qt + 1) % kQ; handle(l); }
        if ((int32_t)(now - next) >= 0) {
            next += 5; if ((int32_t)(now - next) > 50) next = now + 5;
            g_sens.poll(hw, now);
            float dt = (float)(now - last) * 0.001f; last = now; if (dt < 0.001f) dt = 0.005f;
            g_map.update(g_sens.out(), dt);
            hw.SetLed(((now / 500) & 1) != 0);
            if (g_stream && now - g_lastTel >= 50) { g_lastTel = now; sendTel(now); }
        }
        if ((int32_t)(now - nextHello) >= 0) { nextHello = now + 1000; sendHello(); }
    }
}

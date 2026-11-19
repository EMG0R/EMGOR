// hx_seed.h -- shared Seed glue for HYPERTRUMPET V2 firmware (bench + main). NEW 2026-11-15. Header-only.
//  * DWT cycle counter + ns clock for the engine's CPU meter
//  * SDRAM memory map (static arrays in .sdram_bss; NOT zeroed at reset, arenas zero what they hand out)
//  * USB CDC line transmit/receive (libDaisy's own CDC, interrupt driven, same pattern as bringup/fw/main.cpp)
#pragma once
#include <stdint.h>
#include <string.h>
#include "daisy_seed.h"
#include "fx_engine.h"
#include "fx_looper.h"

// ------------------------------------------------------------------ SDRAM memory map (64 MiB = 67,108,864 B)
//   looper pool   46,080,128 B  4 layers x 60 s x 48 kHz x 4 B = 46,080,000 + 128 B arena slack
//   slot arenas   4 x 3 MiB     = 12,582,912 B (largest non-looper effect today: 1.02 MB grain; 3 MiB leaves 3x headroom)
//   total used    58,663,040 B  (87.4 %); free 8,445,824 B (12.6 %) = margin for new effects / protocol buffers
//   MAIN firmware also reserves a 2 MiB schema scratch arena (main.cpp): 60,760,192 B (90.5 %), free 6,348,672 B (9.5 %)
// (the looper object itself and every other effect live in the slot arenas; only ONE looper may exist: it owns the pool)
#ifndef HX_SLOT_BYTES
#define HX_SLOT_BYTES (3u * 1024u * 1024u)
#endif
#define HX_LOOP_POOL_BYTES (46080000u + 128u)
HX_SDRAM static uint8_t g_hxSlotMem[HX_NSLOTS * HX_SLOT_BYTES] __attribute__((aligned(32)));
HX_SDRAM static uint8_t g_hxLoopMem[HX_LOOP_POOL_BYTES] __attribute__((aligned(32)));
static hx::Arena g_hxLoopArena;

static inline void hxSdramInit(hx::FxEngine& e, float sr) {
    g_hxLoopArena.bind(g_hxLoopMem, HX_LOOP_POOL_BYTES);
    hx::g_loopPool = &g_hxLoopArena;
    e.init(sr, g_hxSlotMem, sizeof g_hxSlotMem);
}

// ------------------------------------------------------------------ cycle counter
static uint32_t g_hxClkMHz = 480;
static inline void hxDwtInit() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55;                       // unlock (M7)
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_hxClkMHz = HAL_RCC_GetSysClockFreq() / 1000000u;
}
static inline uint32_t hxCyc() { return DWT->CYCCNT; }
// monotonic ns for FxEngine::g_nowNs. Called only from the audio callback (the wrap tracker is not re-entrant).
static uint64_t hxNowNs() {
    static uint32_t last = 0; static uint64_t ns = 0;
    uint32_t c = DWT->CYCCNT; uint32_t d = c - last; last = c;
    ns += ((uint64_t)d * 1000u) / g_hxClkMHz; return ns;
}

// ------------------------------------------------------------------ USB CDC line I/O
// RX: one line per call to the handler, ISR context, keep it tiny.
typedef void (*HxLineFn)(const char* line);
static HxLineFn g_hxLineFn = nullptr;
static void hxRxCb(uint8_t* buf, uint32_t* len) {
    static char line[240]; static uint16_t n = 0;
    for (uint32_t i = 0; i < *len; i++) {
        char c = (char)buf[i];
        if (c == '\r') continue;
        if (c == '\n') { line[n] = 0; n = 0; if (g_hxLineFn) g_hxLineFn(line); }
        else if (n < sizeof(line) - 1) line[n++] = c;
    }
}
// TX: line builder + double-buffered send. emit() waits up to 4 ms for the previous transfer, else drops the line.
struct HxOut {
    uint8_t buf[2][1100]; int cur = 0; size_t len = 0;
    void begin() { len = 0; }
    void c(char ch) { if (len < sizeof(buf[0]) - 1) buf[cur][len++] = (uint8_t)ch; }
    void s(const char* t) { while (*t) c(*t++); }
    void u(uint32_t v) { char t[11]; int n = 0; do { t[n++] = '0' + v % 10; v /= 10; } while (v); while (n) c(t[--n]); }
    void i(int32_t v) { if (v < 0) { c('-'); v = -v; } u((uint32_t)v); }
    void f(float v, int dec = 3) {     // fixed point, dec <= 4
        if (v != v || v > 1e9f || v < -1e9f) { s("0"); return; }
        if (v < 0) { c('-'); v = -v; }
        uint32_t m = 1; for (int k = 0; k < dec; k++) m *= 10;
        uint64_t x = (uint64_t)(v * (float)m + 0.5f); u((uint32_t)(x / m)); if (dec) { c('.'); uint32_t fr = (uint32_t)(x % m); for (uint32_t d = m / 10; d; d /= 10) { c('0' + (fr / d) % 10); } }
    }
    void hex(uint32_t v) { c('"'); s("0x"); for (int k = 28; k >= 0; k -= 4) c("0123456789abcdef"[(v >> k) & 15]); c('"'); }
    void str(const char* t) { c('"'); s(t); c('"'); }
    bool end(daisy::DaisySeed& hw, uint32_t timeoutMs = 4) {
        c('\n');
        uint32_t t0 = daisy::System::GetNow();
        while (hw.usb_handle.TransmitInternal(buf[cur], len) != daisy::UsbHandle::Result::OK) {
            if (daisy::System::GetNow() - t0 > timeoutMs) return false;
        }
        cur ^= 1; return true;
    }
};

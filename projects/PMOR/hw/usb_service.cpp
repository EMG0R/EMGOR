// usb_service.cpp — the one symbol every blocking hardware driver needs.
// VERBATIM from PMOR_V2.cpp: pmor_usb_service() L3724–3728, usbDelay() L386–392,
// PollCdc() L3642–3660.
#include "hw.h"
#include "hw_priv.h"
#include "studio.h"
#include "bootguard.h"
extern "C" {
#include "tusb.h"
#include "usb/clock_engine.h"
#include "usb/usb_audio_stream.h"
}
using namespace daisy;

// One-stop USB service: task pump + capture-ring drain + MIDI clock/roles.
// Called from the main loop, from OLED Update() between page blits, and from
// usbDelay() — so no code path can starve the USB plane for more than one
// OLED page (~0.4ms). All call sites are main-loop thread context; none are
// reachable from inside clock_engine_poll() itself, so this cannot re-enter.
volatile uint32_t g_svc_calls, g_svc_us_sum, g_svc_us_max;   // per-STATUS-window cost of this function (it runs once per I2C byte)
extern "C" void pmor_usb_service(void) {
    uint32_t t0 = System::GetTick();
    bootguardKick();
    tud_task();
    usb_audio_stream_task();
    clock_engine_poll();
    studioMidiService(System::GetNow());
    uint32_t d = (uint32_t)(((uint64_t)(System::GetTick() - t0) * 1000000ull) / System::GetTickFreq());
    g_svc_calls = g_svc_calls + 1; g_svc_us_sum = g_svc_us_sum + d; if (d > g_svc_us_max) g_svc_us_max = d;
}

// ── USB-safe delay ───────────────────────────────────────────────────────
// System::Delay() is a hard spin: nothing else runs. That is fatal next to
// USB, because TinyUSB does its control-transfer work in tud_task(), not in
// the ISR. Block for ~500ms and the device stops answering the host, macOS
// gives up, and it vanishes from the bus.
//
// MEASURED 2026-08-14 by staged bring-up: chunk 6 (OLED + sensors + SD +
// encoders + patches + audio) enumerated fine; adding ntInit() -- whose
// first act is a seesaw reset followed by System::Delay(500) -- killed USB
// completely. Same code, one extra call.
//
// Use this anywhere a startup delay runs after USB is up.
void usbDelay(uint32_t ms) {
    uint32_t t0 = System::GetNow();
    // Full USB service, not just tud_task(): the capture ring must keep
    // draining and MIDI clock must keep ticking through any wait, or the
    // host sees audio dropouts / BPM flicker during long delays.
    while(System::GetNow() - t0 < ms) { pmor_usb_service(); }
}

// ── USB CDC console — line-buffered, feeds clock_engine's command handler
// (SY mode / STATUS / RATE / DIAG / BEAT / HELP / buttonless DFU trigger).
// Ported from firmware-tinyusb/'s main.cpp PollCdc(); the only debugging
// channel on this enclosed device (see "buttonless DFU" below). Control
// plane only — never called from the audio ISR.
void PollCdc(void) {
    if(!tud_cdc_connected() || !tud_cdc_available()) return;
    uint8_t buf[64];
    uint32_t n = tud_cdc_read(buf, sizeof(buf));
    // Everything goes through the protocol layer (core/proto.cpp): '!'-framed lines are the STUDIO protocol,
    // any other line is handed back to the legacy console (clock_engine_handle_cdc_line: dfu, status, ...).
    if(n) studioCdcRx(buf, n);
}

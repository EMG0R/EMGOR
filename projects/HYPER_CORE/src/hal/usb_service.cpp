/* HYPER_CORE import
 * origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_live/hw/usb_service.cpp
 * commit : PMOR_V3_live ff86e4c
 * changes: includes -> hyper_hw.h; usb_audio_stream_task() and clock_engine_poll() replaced by one hook hyper_usb_extra_poll(); clock_engine_handle_cdc_line() -> hyper_cdc_line(). Pump order, usbDelay and the line-buffered CDC parser are unchanged
 * licence: Emory (own work) */
// usb_service.cpp — the one symbol every blocking hardware driver needs.
// VERBATIM from PMOR_V2.cpp: pmor_usb_service() L3724–3728, usbDelay() L386–392,
// PollCdc() L3642–3660.
#include <string.h>
#include "hyper_hw.h"
extern "C" {
#include "tusb.h"
}
using namespace daisy;

// One-stop USB service: task pump + capture-ring drain + MIDI clock/roles.
// Called from the main loop, from OLED Update() between page blits, and from
// usbDelay() — so no code path can starve the USB plane for more than one
// OLED page (~0.4ms). All call sites are main-loop thread context; none are
// reachable from inside clock_engine_poll() itself, so this cannot re-enter.
extern "C" void pmor_usb_service(void) {
    tud_task();
    hyper_usb_extra_poll();
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
    static char    line[64];
    static uint8_t len = 0;
    uint8_t        buf[64];
    uint32_t       n = tud_cdc_read(buf, sizeof(buf));
    for(uint32_t i = 0; i < n; i++) {
        char c = (char)buf[i];
        if(c == '\r') continue;
        if(c == '\n') {
            line[len] = '\0';
            hyper_cdc_line(line);
            len = 0;
            continue;
        }
        if(len < sizeof(line) - 1) line[len++] = c;
        // else: silently drop overlong-line overflow, never corrupt adjacent memory
    }
}

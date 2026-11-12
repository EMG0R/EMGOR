// transport_link.cpp -- see transport_link.h.
#include "transport_link.h"
#include "clock.h"
#include "shared_state.h"
#include "../config.h"
#include "daisy_seed.h"
extern "C" {
#include "usb/clock_engine.h"
}

void transportTap(uint32_t tap_sample) {
    const uint32_t now_sample = clockSampleNow();
    if(clock_engine_clock_mode() == PMOR_CLKMODE_SLAVE) {
        // Age of the press at the moment we hand it over. The Pi reconstructs the press instant as
        // (its receive time - age), so main-loop stalls between the ISR stamp and this send are
        // measured and subtracted, not baked into the tempo.
        uint32_t age_ms = (uint32_t)(now_sample - tap_sample) / (uint32_t)(SR / 1000.f);
        clock_engine_send_tap_event((uint8_t)(age_ms > 127u ? 127u : age_ms));
        g.tap_sent_ms = daisy::System::GetNow();      // OLED flashes the tempo readout: "the Pi got it"
        return;                                        // no local analysis, no role claim, clock untouched
    }
    if(clockTap(tap_sample) != 0) {                    // 0 = bounce/duplicate: nothing happened
        // A deliberate tap on the instrument in your hands wins the role (existing TASK 4 rule).
        clock_engine_human_gesture(clk.bpm);
    }
}

void transportTick() {
    const pmor_clock_mode_t mode = clock_engine_clock_mode();
    g.clock_mode = (uint8_t)mode;

    if(clock_engine_get_role() == CLOCK_ROLE_OPG_MASTER) {
        // We drive: hand the grid's tempo to clock_engine, which emits 0xF8/Start/Stop and
        // CC116/119/120/121 FROM that value alone -- it never runs a second tempo.
        clock_engine_set_local_bpm(clk.bpm);
    } else if(mode == PMOR_CLKMODE_SLAVE) {
        // The bus is the tempo: feed what clock_engine RECEIVED back into the grid.
        // (INTERNAL while role==DEMIURGE_MASTER used to land here too and overwrote every local
        // BPM edit with the stale last-bus tempo on the very next loop pass -- standalone PMOR
        // could not hold a tempo. Now only a live bus steers the grid.)
        float bus_bpm = clock_engine_get_bpm();
        if(bus_bpm > 1.f) {
            if(bus_bpm < 30.f)  bus_bpm = 30.f;
            if(bus_bpm > 300.f) bus_bpm = 300.f;          // was 250: tap range tops out at 300, the wire at 400
            if(bus_bpm != clk.bpm) clockSetBpm(bus_bpm);
        }
    }
    // What the screen shows: the RECEIVED transport tempo while slaved, the local clock otherwise.
    g.bpm_shown = (mode == PMOR_CLKMODE_SLAVE) ? clock_engine_get_bpm() : clk.bpm;
}

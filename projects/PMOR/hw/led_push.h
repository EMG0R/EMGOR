// led_push.h -- NeoTrellis LED push scheduler (hardware-free, header-only; hw/trellis.cpp owns the bus, tests/test_ledpush.cpp drives it on the host).
//
// Two lanes, both one bus transaction per step():
//  * FULL FRAME: 12 chunks of 4 data bytes (>= gap_t apart: the SAMD09 must finish storing a chunk before the next arrives) + SHOW, started at most every 80 ms
//    and only for a changed frame. Takes ~26 ms, so a pad lit/unlit while it runs used to wait for the rest of it AND for the next 80 ms slot: 25..100+ ms
//    of visible-feedback latency, felt as FX/pad trigger latency.
//  * PRIORITY: a pad press/release marks its pixel (markPad). As soon as the bus is free and the gap since the previous transaction has passed, the changed
//    pixel(s) go out FIRST (3 data bytes each, one transaction) followed by one SHOW, ahead of whatever remains of the full frame. The pixel is also written
//    into the in-flight frame copy so the rest of that push cannot overwrite it. A marked pixel whose frame colour has not changed yet (the UI renders the
//    press a few ms after the event) waits up to wait_t, then is dropped.
// Time unit: the caller's raw timer ticks (any unit; gap_t / fit_t / wait_t are in the same unit).
#pragma once
#include <stdint.h>
#include <string.h>

struct LedPusher {
    static const uint8_t CHUNK = 4, NCH = 48 / CHUNK;
    uint8_t  sent[48];            // what the SAMD09 pixel RAM holds (as far as we know)
    uint8_t  dev[48];             // bytes actually written to the device pixel RAM so far
    uint8_t  buf[48];             // consistent copy of the frame being pushed
    uint8_t  ph;              // 0 idle; 1..NCH next chunk (1-based); NCH+1 = SHOW
    uint32_t next_t;          // earliest start of the next transaction (inter-transaction gap)
    uint32_t next_frame_ms;   // full-frame cadence
    uint16_t urg_mask;        // priority pixels (PHYSICAL index)
    bool     urg_show;     // priority pixels written, SHOW pending
    uint32_t urg_t0;          // tick the oldest unserved event was marked
    uint32_t lat_last, lat_max;   // event -> priority SHOW sent, ticks
    uint32_t gap_t, fit_t, wait_t;
    uint32_t n_full, n_urgent;
    // No constructor on purpose: the instance lives in SDRAM_BSS (a static constructor must not touch SDRAM before it is up, fwcheck rule 3); init() once.
    void init(uint32_t gap, uint32_t fit, uint32_t wait) { memset(this, 0, sizeof *this); memset(sent, 0xFF, sizeof sent); memset(dev, 0xFF, sizeof dev); gap_t = gap; fit_t = fit; wait_t = wait; }
    void reset() { ph = 0; urg_mask = 0; urg_show = false; urg_t0 = 0; }
    void markPad(uint8_t phys, uint32_t t) { urg_mask |= (uint16_t)(1u << (phys & 15)); if (!urg_t0) urg_t0 = t ? t : 1u; }

    enum Result : uint8_t { NONE = 0, CHUNK_SENT, FULL_SHOW, URGENT_PIXEL, URGENT_SHOW, SEND_FAILED };
    // send(pkt, n) -> false when the device did not ACK. next_key_in = ticks until the next keypad poll falls due (a transaction only starts if it fits).
    template <class Send>
    Result step(uint32_t now_ms, uint32_t t, bool bus_idle, int32_t next_key_in, const uint8_t frame[48], Send send) {
        if (ph == 0 && now_ms >= next_frame_ms) {
            next_frame_ms = now_ms + 80;   // gentler bus pacing: fewer SAMD09 overruns
            if (memcmp(frame, sent, 48) != 0) { memcpy(sent, frame, 48); memcpy(buf, frame, 48); ph = 1; if ((int32_t)(t - next_t) > 0) next_t = t; }
        }
        if (!bus_idle) return NONE;
        if ((int32_t)(t - next_t) < 0 || next_key_in <= (int32_t)fit_t) return NONE;

        if (urg_mask || urg_show) {
            for (uint8_t p = 0; p < 16; p++) {
                if (!((urg_mask >> p) & 1u)) continue;
                if (memcmp(frame + p * 3, dev + p * 3, 3) == 0) {                         // the device already shows it (or the UI has not rendered the change yet)
                    if ((int32_t)(t - urg_t0) > (int32_t)wait_t) urg_mask &= (uint16_t)~(1u << p);
                    continue;
                }
                uint8_t pkt[4 + 3] = { 0x0E, 0x04, 0, (uint8_t)(p * 3), frame[p * 3], frame[p * 3 + 1], frame[p * 3 + 2] };
                bool ok = send(pkt, (uint8_t)sizeof pkt);
                next_t = t + gap_t;
                if (!ok) { reset(); return SEND_FAILED; }
                memcpy(dev + p * 3, frame + p * 3, 3); memcpy(buf + p * 3, frame + p * 3, 3); memcpy(sent + p * 3, frame + p * 3, 3);
                urg_mask &= (uint16_t)~(1u << p); urg_show = true; n_urgent++;
                return URGENT_PIXEL;
            }
            if (urg_show && !urg_mask) {                                              // all marked pixels written: show them now
                const uint8_t sh[2] = { 0x0E, 0x05 };
                bool ok = send(sh, 2); next_t = t + gap_t;
                if (!ok) { reset(); return SEND_FAILED; }
                urg_show = false; lat_last = t - urg_t0; if (lat_last > lat_max) lat_max = lat_last; urg_t0 = 0;
                return URGENT_SHOW;
            }
            if (!urg_mask && !urg_show) urg_t0 = 0;
            // (marked pixels still waiting for the UI to render their change do not hold up the background frame)
        }
        if (ph) {
            if (ph <= NCH) {
                uint8_t off = (uint8_t)((ph - 1) * CHUNK);
                uint8_t pkt[4 + CHUNK] = { 0x0E, 0x04, 0, off };
                for (uint8_t i = 0; i < CHUNK; i++) pkt[4 + i] = buf[off + i];
                bool ok = send(pkt, (uint8_t)sizeof pkt); next_t = t + gap_t;
                if (!ok) { reset(); return SEND_FAILED; }
                memcpy(dev + off, buf + off, CHUNK);
                ph++; return CHUNK_SENT;
            }
            const uint8_t sh[2] = { 0x0E, 0x05 };
            send(sh, 2); ph = 0; n_full++; next_t = t + gap_t;
            return FULL_SHOW;
        }
        return NONE;
    }
};

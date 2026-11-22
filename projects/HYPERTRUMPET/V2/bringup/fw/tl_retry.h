// NeoTrellis (re)init scheduling, pure C++ (also compiled by the host test tests/tl_retry_test.cpp).
// Trellis absent (no power, unplugged): every attempt is ONE short probe (bus recovery + a single address probe, well under 1 ms) at an interval that backs off from 150 ms (first 3 s of
// uptime, covers the seesaw power-up) to 2 s, then doubles up to a cap of 5 s, so a hot-plugged Trellis is found within about 5 s. On the first success the full (re)init has run
// (keypad events + LEDs) and `redraw` is set, which forces the next LED frame to resend the whole 48-byte buffer (the current page / LED state is redrawn).
#pragma once
#include <stdint.h>
struct TlRetry {
    uint32_t backoff = 150, next = 0; uint32_t attempts = 0, successes = 0; bool redraw = true;
    static const uint32_t CAP_MS = 5000;
    bool due(uint32_t now) const { return (int32_t)(now - next) >= 0; }
    void result(uint32_t now, bool ok) {              // call after an attempt
        attempts++;
        if(ok) { successes++; backoff = 150; redraw = true; return; }
        backoff = now < 3000 ? 150 : (backoff < 2000 ? 2000 : (backoff * 2 > CAP_MS ? CAP_MS : backoff * 2));
        next = now + backoff;
    }
};

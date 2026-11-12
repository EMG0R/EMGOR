// LED push scheduler (hw/led_push.h): a pad press/release reaches the LEDs ahead of the background 12-chunk frame push.
// Models the real timing: transactions >= 2 ms apart, ~1 ms bus time each, keypad poll every 2 ms (a transaction only starts when the next poll is > 0.7 ms away).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "../hw/led_push.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
// time unit: microseconds
struct Sim {
    LedPusher lp; uint8_t frame[48]; uint32_t t = 0; uint32_t last_tx = 0; int txs = 0; bool gap_ok = true; uint8_t dev_shown[48]; uint8_t pend[48];
    bool marked = false; std::vector<std::pair<uint32_t, int>> shows;   // (time, kind)
    Sim() { lp.init(2000, 700, 40000); memset(frame, 0, 48); memset(dev_shown, 0, 48); memset(pend, 0, 48); }
    int run_until(uint32_t t_end, int mark_pad = -1, uint32_t mark_at = 0, std::vector<uint32_t>* shown_at = nullptr) {
        for (; t < t_end; t += 100) {
            if (!marked && mark_pad >= 0 && t >= mark_at) { marked = true; lp.markPad((uint8_t)mark_pad, t); }
            uint32_t phase = t % 2000;                          // keypad poll falls due every 2 ms
            int32_t next_key_in = (int32_t)(2000 - phase);
            auto r = lp.step(t / 1000, t, true, next_key_in, frame, [&](const uint8_t* pkt, uint8_t n) {
                if (txs && t - last_tx < 2000) gap_ok = false;
                last_tx = t; txs++;
                if (n == 2 && pkt[1] == 0x05) { memcpy(dev_shown, pend, 48); if (shown_at) shown_at->push_back(t); }
                else if (pkt[1] == 0x04) { for (int i = 0; i < n - 4; i++) pend[pkt[3] + i] = pkt[4 + i]; }
                return true; });
            (void)r; if (r == LedPusher::URGENT_SHOW) shows.push_back({t, 1});
        }
        return 0;
    }
};
int main() {
    // background: the lava base changes every frame -> a full push is in flight almost all the time
    {
        Sim s; uint32_t worst_old = 0, worst_new = 0; int n = 0;
        for (int trial = 0; trial < 40; trial++) {
            for (int mode = 0; mode < 2; mode++) {
                Sim x; srand(trial);
                uint32_t press_t = 200000 + (uint32_t)(rand() % 80000);   // anywhere inside the 80 ms cadence
                int pad = 5;
                // frame changes every 5 ms (lava), the press lights pad 5 white at press_t + 3 ms (UI render after the event)
                uint32_t t_end = press_t + 200000; bool lit = false; std::vector<uint32_t> shown;
                uint32_t seen_lit = 0;
                for (; x.t < t_end;) {
                    uint32_t next = x.t + 1000;
                    for (int i = 0; i < 48; i++) if (!(i / 3 == pad)) x.frame[i] = (uint8_t)(x.t / 5000 * 7 + i);
                    if (x.t >= press_t + 3000 && !lit) { lit = true; x.frame[pad * 3] = 255; x.frame[pad * 3 + 1] = 255; x.frame[pad * 3 + 2] = 255; }
                    if (lit) { x.frame[pad * 3] = 255; x.frame[pad * 3 + 1] = 255; x.frame[pad * 3 + 2] = 255; }
                    x.run_until(next, mode == 1 ? pad : -1, press_t, &shown);
                    if (!seen_lit && x.dev_shown[pad * 3] == 255 && x.dev_shown[pad * 3 + 1] == 255) seen_lit = x.t;
                }
                uint32_t lat = seen_lit ? seen_lit - press_t : 999999;
                if (mode == 0) { if (lat > worst_old) worst_old = lat; } else { if (lat > worst_new) worst_new = lat; }
                EXPECT(x.gap_ok, "trial %d mode %d: every bus transaction >= 2 ms after the previous", trial, mode);
                if (!x.gap_ok) break;
            }
            n++;
        }
        printf("  press -> pad lit on the device: background push only (old) worst %.1f ms | priority pixel (new) worst %.1f ms\n", worst_old / 1000.0, worst_new / 1000.0);
        EXPECT(worst_new <= 10000, "priority path: pad lit within 10 ms in the worst of %d presses (<= 3 ms UI render + pixel + 2 ms gap + SHOW, bus windows 1.3 of every 2 ms)", n);
        EXPECT(worst_old > 3 * worst_new, "background push alone is much slower (%.1f ms)", worst_old / 1000.0);
    }
    {   // the rest of the frame still completes and the final device state equals the frame
        Sim x; for (int i = 0; i < 48; i++) x.frame[i] = (uint8_t)(i * 5 + 1);
        x.run_until(20000); x.lp.markPad(2, x.t); x.frame[2 * 3] = 9; x.frame[2 * 3 + 1] = 8; x.frame[2 * 3 + 2] = 7; x.run_until(200000);
        EXPECT(memcmp(x.dev_shown, x.frame, 48) == 0, "final device state == frame after a mid-push priority write");
        EXPECT(x.gap_ok, "gap respected");
    }
    {   // release is a priority update too; an unrendered mark times out and does not wedge the pusher
        Sim x; x.run_until(100000); x.lp.markPad(7, x.t); x.run_until(x.t + 100000);
        EXPECT(x.lp.urg_mask == 0, "a marked pixel whose colour never changes is dropped after the wait");
        memset(x.frame, 50, 48); x.run_until(x.t + 150000);
        EXPECT(memcmp(x.dev_shown, x.frame, 48) == 0, "normal frames still push afterwards");
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}

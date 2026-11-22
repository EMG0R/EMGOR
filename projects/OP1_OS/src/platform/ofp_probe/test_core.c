/* host test for ofp_core: cc -O1 -o /tmp/ofp_test_core test_core.c ofp_core.c && /tmp/ofp_test_core */
#include <stdio.h>
#include "ofp_core.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ofp_core c;
static uint32_t t;

static void blk(uint32_t period, uint32_t d, int usb, int32_t pre, int32_t post, int tape)
{
    ofp_blk b = { 0 };

    t += period;
    b.t0 = t; b.d = d; b.usb_active = usb; b.lead_pre = pre; b.lead_post = post; b.in_tape_write = tape;
    ofp_core_block(&c, &b);
}

int main(void)
{
    int i;
    const uint32_t N = OFP_NOMINAL_DEFAULT;

    /* steady: 30 % load, no events */
    ofp_core_reset(&c);
    for (i = 0; i < 1000; i++)
        blk(N, N * 3 / 10, 0, 0, 0, 0);
    CHECK(c.late == 0 && c.hot == 0, "steady late=%u hot=%u", c.late, c.hot);
    CHECK(c.aud_pct >= 29 && c.aud_pct <= 31, "aud_pct %u", c.aud_pct);
    CHECK(c.aud_pk >= 29 && c.aud_pk <= 31, "aud_pk %u", c.aud_pk);
    CHECK(c.max_d == N * 3 / 10, "max_d");
    CHECK(c.nominal > N - 8 && c.nominal < N + 8, "nominal %u", c.nominal);

    /* a delayed block (period 2x) while a tape write runs, then one without */
    blk(2 * N, N / 4, 0, 0, 0, 1);
    CHECK(c.late == 1 && c.late_tape == 1 && c.late_usb == 0, "late %u/%u/%u", c.late, c.late_tape, c.late_usb);
    blk(2 * N - 10, N / 4, 1, 100, 164, 0);
    CHECK(c.late == 2 && c.late_tape == 1 && c.late_usb == 1, "late2 %u/%u/%u", c.late, c.late_tape, c.late_usb);
    CHECK(c.nominal > N - 8 && c.nominal < N + 8, "late block must not move nominal: %u", c.nominal);

    /* hot block */
    blk(N, N * 95 / 100, 0, 0, 0, 0);
    CHECK(c.hot == 1, "hot %u", c.hot);
    blk(N, N * 85 / 100, 0, 0, 0, 0);
    CHECK(c.hot == 1, "hot 85%% counted: %u", c.hot);

    /* USB ring: steady lead around 100, then drained, then full, then a resync jump */
    ofp_core_reset(&c);
    for (i = 0; i < 2000; i++)
        blk(N, N / 5, 1, 100, 164, 0);
    CHECK(c.usb_seen && c.usb_und == 0 && c.usb_ovr == 0 && c.usb_jump == 0, "usb steady %u/%u/%u", c.usb_und, c.usb_ovr, c.usb_jump);
    CHECK(c.lead_min == 100 && c.lead_max == 100 && c.lead_now == 100, "lead min/max/now");
    CHECK(c.lead_secs >= 2, "lead_secs %u", c.lead_secs);
    CHECK(c.lead_base16 == 1600 && c.lead_avg16 == 1600, "lead avg %d %d", c.lead_base16, c.lead_avg16);
    blk(N, N / 5, 1, 0, 64, 0);                     /* drained: underrun (+ the lead dropped 100 -> 0 = jump? prediction 164-64=100, dev -100) */
    CHECK(c.usb_und == 1, "und %u", c.usb_und);
    blk(N, N / 5, 1, 400, 490, 0);                  /* near full: overrun, dev vs 64-64=0 is +400 = jump */
    CHECK(c.usb_ovr == 1 && c.usb_jump == 1, "ovr %u jump %u", c.usb_ovr, c.usb_jump);
    blk(N, N / 5, 0, 0, 0, 0);                      /* stream stopped */
    CHECK(c.usb_seen == 0, "stream stop");

    /* drift: lead mean creeping up one frame per second */
    ofp_core_reset(&c);
    for (i = 0; i < 689 * 10; i++)
        blk(N, N / 5, 1, 100 + i / 689, 164 + i / 689, 0);
    CHECK(c.lead_avg16 - c.lead_base16 >= 16 * 8 && c.lead_avg16 - c.lead_base16 <= 16 * 10, "drift16 %d", c.lead_avg16 - c.lead_base16);

    /* ms conversion: one period = 1.45 ms (x10 = 14) */
    ofp_core_reset(&c);
    CHECK(ofp_ms10(&c, c.nominal) >= 13 && ofp_ms10(&c, c.nominal) <= 15, "ms10 one block = %u", ofp_ms10(&c, c.nominal));
    CHECK(ofp_ms10(&c, c.nominal * 10) >= 143 && ofp_ms10(&c, c.nominal * 10) <= 147, "ms10 ten blocks");

    /* cycle counter wrap */
    ofp_core_reset(&c);
    t = 0xffffffffu - 5 * N;
    for (i = 0; i < 20; i++)
        blk(N, N / 10, 0, 0, 0, 0);
    CHECK(c.late == 0, "wraparound counted late: %u", c.late);

    printf(fails ? "ofp_core: %d FAILED\n" : "ofp_core: all passed\n", fails);
    return fails != 0;
}

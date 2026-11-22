/* shared by the host test and the Blackfin simulator bench: a fixed, deterministic run of ofp_gain_block whose
 * output checksum must be identical on both (same integer semantics), plus cycles per block when a clock is given */
#ifndef OFP_XCHECK_H
#define OFP_XCHECK_H

#include <stdint.h>
#include "ofp_gain_dsp.h"

#define XCHECK_N 8

static uint32_t xcheck_run(uint32_t (*clk)(void), uint32_t *cyc)
{
    static const int pos[XCHECK_N] = { 0, 60, 100, 101, 130, 170, 190, 200 };
    uint32_t sum = 0, rng = 1;
    int i, b, k;

    for (i = 0; i < XCHECK_N; i++) {
        ofp_gain_state s;

        ofp_gain_init(&s, pos[i]);
        for (b = 0; b < 8; b++) {
            int32_t w[128];

            for (k = 0; k < 128; k++) {
                rng = rng * 1664525u + 1013904223u;
                w[k] = (i & 1) ? (int32_t)rng >> 2 : (int32_t)rng;         /* full-scale noise / -12 dBFS noise */
            }
            {
                uint32_t t0 = clk ? clk() : 0;

                ofp_gain_block(&s, w, 64, pos[i]);
                if (b == 4 && cyc)
                    cyc[i] = clk ? clk() - t0 : 0;
            }
            for (k = 0; k < 128; k++)
                sum = sum * 31u + (uint32_t)w[k];
        }
    }
    return sum;
}

#endif

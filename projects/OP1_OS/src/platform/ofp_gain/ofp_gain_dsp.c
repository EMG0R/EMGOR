#include "ofp_gain_dsp.h"

extern const int32_t ofp_gain_a[2049], ofp_gain_s[2049];

#define POS_STEP 2              /* positions per block the control may move: a full sweep takes ~145 ms */

static int32_t step_w(int32_t cur, int32_t tgt)
{
    int32_t d = tgt - cur;

    return cur + (d > 0 ? (d + 1) >> 1 : -((-d + 1) >> 1));
}

void ofp_gain_init(ofp_gain_state *s, int pos)
{
    unsigned char *b = (unsigned char *)s;
    unsigned int i;

    for (i = 0; i < sizeof(*s); i++)
        b[i] = 0;
    if (pos < 0)
        pos = 0;
    if (pos > 200)
        pos = 200;
    s->pos = pos;
    s->p = (int32_t)ofp_gain_p[pos] << 5;
    s->wl = ofp_gain_wl[pos];
    s->ws = ofp_gain_ws[pos];
    s->wa = 256 - s->wl - s->ws;
}

void ofp_gain_block(ofp_gain_state *s, int32_t *tx, int frames, int target_pos)
{
    int32_t pt, dp, p, n, ch;
    const int32_t ceil = ofp_gain_ceiling;

    if (target_pos < 0)
        target_pos = 0;
    if (target_pos > 200)
        target_pos = 200;
    if (target_pos > s->pos)
        s->pos = s->pos + POS_STEP > target_pos ? target_pos : s->pos + POS_STEP;
    else if (target_pos < s->pos)
        s->pos = s->pos - POS_STEP < target_pos ? target_pos : s->pos - POS_STEP;
    pt = (int32_t)ofp_gain_p[s->pos] << 5;
    dp = (pt - s->p) >> 6;                              /* ramp across the block (64 frames) */
    p = s->p;
    s->wl = step_w(s->wl, ofp_gain_wl[s->pos]);
    s->ws = step_w(s->ws, ofp_gain_ws[s->pos]);
    s->wa = 256 - s->wl - s->ws;
    if (s->wa < 0) {
        s->ws += s->wa;
        s->wa = 0;
    }

    for (n = 0; n < frames; n++) {
        int32_t pq = p >> 5;                            /* Q11 */

        p += dp;
        for (ch = 0; ch < 2; ch++) {
            int32_t x = tx[2 * n + ch] >> 8;            /* Q23 */
            int32_t xh = x >> 8, xl = x & 255;
            int32_t v = ((xh * pq) >> 3) + ((xl * pq) >> 11);
            int32_t l, idx, fr, a, sc, y, t0;

            if (v > (1 << 27) - (1 << 17))
                v = (1 << 27) - (1 << 17);
            if (v < -(1 << 27) + (1 << 17))
                v = -(1 << 27) + (1 << 17);
            l = v > 8388607 ? 8388607 : v < -8388607 ? -8388607 : v;
            idx = (v >> 17) + 1024;             /* table step = 1/64 full scale = 2^17 in Q23 */
            fr = (v & 0x1ffff) >> 4;            /* 13 bits */
            t0 = ofp_gain_a[idx];
            a = t0 + (((ofp_gain_a[idx + 1] - t0) * fr) >> 13);
            t0 = ofp_gain_s[idx];
            sc = t0 + (((ofp_gain_s[idx + 1] - t0) * fr) >> 13);
            y = (s->wl * l + s->wa * a + s->ws * sc) >> 8;

            s->dc[ch] += ((y * 16) - s->dc[ch]) >> 14;
            y -= s->dc[ch] >> 4;
            if (y > ceil)
                y = ceil;
            if (y < -ceil)
                y = -ceil;
            if ((y < 0 ? -y : y) > s->peak)
                s->peak = y < 0 ? -y : y;
            tx[2 * n + ch] = y * 256;
        }
    }
    s->p = pt;
}

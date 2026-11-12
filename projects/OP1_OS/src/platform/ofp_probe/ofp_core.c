#include "ofp_core.h"

void ofp_core_reset(ofp_core *c)
{
    uint32_t keep = c->nominal ? c->nominal : OFP_NOMINAL_DEFAULT;
    unsigned char *p = (unsigned char *)c;
    unsigned int i;

    for (i = 0; i < sizeof(*c); i++)
        p[i] = 0;
    c->nominal = keep;
    c->min_p = 0xffffffffu;
}

uint32_t ofp_ms10(const ofp_core *c, uint32_t cycles)
{
    uint32_t unit = (c->nominal >> 10) | 1u;            /* 1/1024 block */
    uint32_t x = cycles / unit;                         /* blocks x 1024 */

    return ((x >> 4) * 1451u) / 6400u;                  /* x/1024 * 14.512 ms x10 */
}

void ofp_core_block(ofp_core *c, const ofp_blk *b)
{
    uint32_t p, jit;

    c->blocks++;
    if (b->d > c->max_d)
        c->max_d = b->d;
    if (c->have_last) {
        p = b->t0 - c->last_t0;
        if (p < c->min_p)
            c->min_p = p;
        if (p > c->max_p)
            c->max_p = p;
        if (p > c->nominal + (c->nominal >> 1)) {       /* a block started > 1.5 periods after the last */
            c->late++;
            if (b->in_tape_write)
                c->late_tape++;
            if (b->usb_active)
                c->late_usb++;
        } else if (p > (c->nominal - (c->nominal >> 2))) {
            jit = p > c->nominal ? p - c->nominal : c->nominal - p;
            (void)jit;
            c->nominal += ((int32_t)p - (int32_t)c->nominal) >> 6;     /* undisturbed period */
        }
    }
    c->last_t0 = b->t0;
    c->have_last = 1;
    if (b->d > ((c->nominal * 29u) >> 5))                /* took > 90.6 % of a period */
        c->hot++;

    /* ~0.5 s windows: ISR busy share */
    if (!c->win_n)
        c->win_t0 = b->t0;
    c->win_busy += b->d;
    c->win_n++;
    if (c->win_n >= 345) {
        uint32_t wall = b->t0 + b->d - c->win_t0;

        c->aud_pct = c->win_busy / ((wall / 100u) | 1u);
        if (c->aud_pct > 100)
            c->aud_pct = 100;
        if (c->aud_pct > c->aud_pk)
            c->aud_pk = c->aud_pct;
        c->win_n = 0;
        c->win_busy = 0;
    }

    /* USB to-host ring */
    if (b->usb_active) {
        c->usb_blocks++;
        if (!c->usb_seen) {
            c->usb_seen = 1;
            c->lead_min = c->lead_max = b->lead_pre;
            c->prev_post = b->lead_post;
        }
        c->lead_now = b->lead_pre;
        if (b->lead_pre < c->lead_min)
            c->lead_min = b->lead_pre;
        if (b->lead_pre > c->lead_max)
            c->lead_max = b->lead_pre;
        if (b->lead_pre <= 0)
            c->usb_und++;
        if (b->lead_post > 480)
            c->usb_ovr++;
        {
            int32_t want = c->prev_post - 64, dev = b->lead_pre - want;

            if (c->usb_blocks > 1 && (dev > 192 || dev < -192))
                c->usb_jump++;
        }
        c->prev_post = b->lead_post;
        c->lw_sum += (uint32_t)(b->lead_pre < 0 ? 0 : b->lead_pre);
        c->lw_n++;
        if (c->lw_n >= 689) {                           /* ~1 s */
            c->lead_avg16 = (int32_t)((c->lw_sum * 16u) / c->lw_n);
            if (!c->lead_secs)
                c->lead_base16 = c->lead_avg16;
            c->lead_secs++;
            c->lw_sum = 0;
            c->lw_n = 0;
        }
    } else if (c->usb_seen && c->usb_blocks) {
        /* stream stopped: restart the drift baseline next time */
        c->usb_blocks = 0;
        c->lead_secs = 0;
        c->lw_sum = c->lw_n = 0;
        c->usb_seen = 0;
    }
}

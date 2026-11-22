/* TURING: looping shift register + lock knob (Music Thing Turing Machine principle).
 * LOCK 100 = the outgoing bit recirculates (frozen loop of LENGTH steps),
 * LOCK 0 = every new bit is random, in between = that % chance per step of a fresh random bit.
 * Pitch, octave position and gate are pure functions of the register value, so a frozen register
 * repeats exactly. */
#include "seq.h"

static const seq_param_desc P[] = {
    { "LENGTH",  2, 16, 8, 0, 0 },
    { "LOCK",    0, 100, 85, 0, 0 },
    { "RANGE",   1, 12, 7, 0, 0 },       /* degrees used (wrapped into the scale) */
    { "DENSITY", 0, 100, 80, 0, 0 },
};

static uint32_t mask_of(const seq_host *h) { return (1u << h->mp[0]) - 1u; }

static void t_init(seq_host *h) { h->st.turing.reg = 0; }
static void t_reseed(seq_host *h)
{
    h->st.turing.reg = seq_rng_next(&h->rng) & mask_of(h);
    if (!h->st.turing.reg) h->st.turing.reg = 1;
}

static void t_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int L = h->mp[0], range = h->mp[2], n = seq_scale_size(h);
    uint32_t mask = (1u << L) - 1u, reg = h->st.turing.reg & mask, v, outbit, nb, frac;
    (void)step;
    outbit = (reg >> (L - 1)) & 1u;
    nb = outbit;
    if (seq_rng_range(&h->rng, 100) >= (uint32_t)h->mp[1])
        nb = seq_rng_range(&h->rng, 2);
    reg = ((reg << 1) | nb) & mask;
    h->st.turing.reg = reg;
    frac = reg << (16 - L);                           /* 0..65535 */
    if (range > n) range = n;
    v = (frac * (uint32_t)range) >> 16;
    o->n = 1;
    o->note[0].deg = (int8_t)v;
    o->note[0].oct = seq_hash8(reg * 3u + 1u);
    o->note[0].vel = (uint8_t)(70 + (seq_hash8(reg + 77u) >> 3));
    o->note[0].on = (uint8_t)(((uint32_t)seq_hash8(reg * 5u + 9u) * 100u >> 8) < (uint32_t)h->mp[3]);
    o->aux = (uint16_t)frac;
}

static void t_viz(const seq_host *h, seq_viz *v)
{
    int L = h->mp[0], i;
    v->w = 16; v->h = 1;
    for (i = 0; i < L; i++)
        v->cell[0][i] = ((h->st.turing.reg >> (L - 1 - i)) & 1u) ? 255 : 40;
}

const seq_mode_desc seq_mode_turing = { "TURING", 4, P, t_init, t_reseed, t_step, 0, 0, t_viz };

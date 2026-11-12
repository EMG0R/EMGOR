/* TOTAL: fully self-running. Needs no input. A 16-step motif (degree, threshold, octave position,
 * velocity) loops; once per bar a seeded process rewrites it a little:
 *   EVOLVE %  chance per bar of one mutation (pitch +-1..2 degrees, re-roll a step's threshold, or its octave)
 *   DRIFT  %  chance per 8 bars that the harmonic centre steps to a new scale degree
 *   CHAOS  %  chance per 8 bars of a phrase rewrite (4 steps replaced)
 *   BREATHE   depth of a slow density swell (period PERIOD bars): steps fade in and out in a fixed
 *             order of their thresholds, so the texture thickens and thins instead of flickering.
 * At the defaults roughly one bar in three changes one step, so adjacent bars match, and the loop
 * has turned over almost completely after a few minutes. Playing a note moves the harmonic centre. */
#include "seq.h"

static const seq_param_desc P[] = {
    { "EVOLVE",  0, 100, 35, 0, 0 },
    { "DRIFT",   0, 100, 30, 0, 0 },
    { "DENSITY", 10, 100, 65, 0, 0 },
    { "BREATHE", 0, 100, 30, 0, 0 },
    { "LENGTH",  4, 16, 16, 0, 0 },
    { "RANGE",   2, 12, 7, 0, 0 },
    { "CHAOS",   0, 100, 15, 0, 0 },
    { "PERIOD",  8, 128, 32, 0, 0 },
};

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void new_step(seq_host *h, int i, int prev_deg)
{
    int range = h->mp[5];
    h->st.total.deg[i] = (int8_t)clampi(prev_deg + (int)seq_rng_range(&h->rng, 5) - 2, 0, range - 1);
    h->st.total.thr[i] = (uint8_t)seq_rng_range(&h->rng, 256);
    h->st.total.oct[i] = (uint8_t)seq_rng_range(&h->rng, 256);
    h->st.total.vel[i] = (uint8_t)(70 + seq_rng_range(&h->rng, 45));
}

static void t_init(seq_host *h) { h->st.total.center = 0; h->st.total.bars = 0; }
static void t_reseed(seq_host *h)
{
    int i, d = h->mp[5] / 2;
    h->st.total.center = 0; h->st.total.bars = 0;
    for (i = 0; i < 16; i++) { new_step(h, i, d); d = h->st.total.deg[i]; }
    h->st.total.thr[0] = 0;                              /* downbeat always sounds */
    h->st.total.dens = (uint16_t)h->mp[2];
}

static int tri100(uint32_t pos, uint32_t period)          /* 0..100 triangle */
{
    uint32_t p = pos % period, half = period / 2;
    if (!half) return 0;
    return (int)((p < half ? p : period - p) * 100 / half);
}

static void evolve(seq_host *h, int len)
{
    uint32_t bars = h->st.total.bars;
    int i;
    if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[0]) {
        i = (int)seq_rng_range(&h->rng, (uint32_t)len);
        switch (seq_rng_range(&h->rng, 4)) {
        case 0: case 1:
            h->st.total.deg[i] = (int8_t)clampi(h->st.total.deg[i] + (seq_rng_range(&h->rng, 2) ? 1 : -1) * (1 + (int)seq_rng_range(&h->rng, 2)), 0, h->mp[5] - 1);
            break;
        case 2: h->st.total.thr[i] = (uint8_t)seq_rng_range(&h->rng, 256); if (i == 0) h->st.total.thr[0] = 0; break;
        default: h->st.total.oct[i] = (uint8_t)seq_rng_range(&h->rng, 256); break;
        }
    }
    if ((bars & 7) == 0) {
        if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[1]) {
            static const int8_t mv[5] = { -2, -1, 1, 2, 3 };
            { int n = seq_scale_size(h); h->st.total.center = (int8_t)(((h->st.total.center + mv[seq_rng_range(&h->rng, 5)]) % n + n) % n); }
        }
        if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[6]) {
            int k, d = h->st.total.deg[0];
            for (k = 0; k < 4; k++) { i = 1 + (int)seq_rng_range(&h->rng, (uint32_t)(len - 1)); new_step(h, i, d); d = h->st.total.deg[i]; }
        }
    }
}

static void t_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int len = h->mp[4], pos = (int)(step % (uint32_t)len), dens;
    if (pos == 0 && step) { h->st.total.bars++; evolve(h, len); }
    dens = clampi(h->mp[2] + (tri100(h->st.total.bars, (uint32_t)h->mp[7]) - 50) * h->mp[3] / 50, 5, 100);
    h->st.total.dens = (uint16_t)dens;
    o->n = 0;
    o->aux = (uint16_t)(dens * 655);
    if ((int)h->st.total.thr[pos] * 100 / 256 >= dens) return;
    o->n = 1;
    o->note[0].on = 1;
    o->note[0].deg = (int8_t)(h->st.total.deg[pos] + h->st.total.center);
    o->note[0].oct = h->st.total.oct[pos];
    o->note[0].vel = h->st.total.vel[pos];
}

static void t_note_in(seq_host *h, int note, int vel, int on)
{
    const seq_scale *sc = &seq_scales[h->up[SEQ_U_SCALE]];
    int rel = ((note - h->up[SEQ_U_ROOT]) % 12 + 12) % 12, best = 0, bd = 99, i, d;
    (void)vel;
    if (!on) return;
    for (i = 0; i < sc->n; i++) {
        d = rel - sc->iv[i]; if (d < 0) d = -d; if (12 - d < d) d = 12 - d;
        if (d < bd) { bd = d; best = i; }
    }
    h->st.total.center = (int8_t)best;
}

static void t_viz(const seq_host *h, seq_viz *v)
{
    int i, len = h->mp[4], r = h->mp[5];
    v->w = 16; v->h = 1;
    for (i = 0; i < len; i++)
        v->cell[0][i] = ((int)h->st.total.thr[i] * 100 / 256 < h->st.total.dens)
                        ? (uint8_t)(60 + 195 * h->st.total.deg[i] / (r > 1 ? r - 1 : 1)) : 0;
    v->playhead = (int8_t)(h->step_idx % (uint32_t)len);
}

const seq_mode_desc seq_mode_total = { "TOTAL", 8, P, t_init, t_reseed, t_step, t_note_in, 0, t_viz };

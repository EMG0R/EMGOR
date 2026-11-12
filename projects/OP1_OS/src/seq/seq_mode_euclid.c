/* EUCLID: Bjorklund patterns (Toussaint). Up to 3 stacked layers, each hits/steps/rotate, each
 * layer plays its own chord tone (layer i = degree i*SPACING, so the default is a scale triad).
 * MELODY = % chance per hit that the layer's pitch takes a +-1 degree seeded step. */
#include "seq.h"

static const seq_param_desc P[SEQ_MAX_MP] = {
    { "LAYERS", 1, 3, 1, 0, 0 },
    { "HITS 1", 0, 32, 5, 0, 0 }, { "STEPS 1", 1, 32, 16, 0, 0 }, { "ROT 1", 0, 31, 0, 0, 0 },
    { "HITS 2", 0, 32, 3, 0, 0 }, { "STEPS 2", 1, 32, 8, 0, 0 },  { "ROT 2", 0, 31, 0, 0, 0 },
    { "HITS 3", 0, 32, 7, 0, 0 }, { "STEPS 3", 1, 32, 12, 0, 0 }, { "ROT 3", 0, 31, 0, 0, 0 },
    { "MELODY", 0, 100, 20, 0, 0 },
    { "SPACING", 0, 6, 2, 0, 0 },
};

/* Bjorklund, recursive formulation (Toussaint 2005 / "Euclidean rhythm" reference code) */
static void bj_build(int level, const int *counts, const int *rem, uint8_t *pat, int *np)
{
    int i;
    if (level == -1) { pat[(*np)++] = 0; }
    else if (level == -2) { pat[(*np)++] = 1; }
    else {
        for (i = 0; i < counts[level]; i++) bj_build(level - 1, counts, rem, pat, np);
        if (rem[level] != 0) bj_build(level - 2, counts, rem, pat, np);
    }
}

uint32_t seq_euclid_bits(int hits, int steps, int rot)
{
    int counts[34], rem[34], level = 0, divisor, np = 0, i, first = 0;
    uint8_t pat[64], rotd[32];
    uint32_t bits = 0;

    if (steps < 1) return 0;
    if (steps > 32) steps = 32;
    if (hits > steps) hits = steps;
    if (hits <= 0) return 0;
    if (hits == steps) return steps == 32 ? 0xFFFFFFFFu : (1u << steps) - 1u;
    divisor = steps - hits;
    rem[0] = hits;
    for (;;) {
        counts[level] = divisor / rem[level];
        rem[level + 1] = divisor % rem[level];
        divisor = rem[level];
        level++;
        if (rem[level] <= 1) break;
    }
    counts[level] = divisor;
    bj_build(level, counts, rem, pat, &np);
    for (i = 0; i < np; i++) if (pat[i]) { first = i; break; }
    for (i = 0; i < steps; i++) rotd[i] = pat[(first + i) % steps];
    for (i = 0; i < steps; i++)                       /* rotate right by rot: pattern arrives later */
        if (rotd[((i - rot) % steps + steps) % steps]) bits |= 1u << i;
    return bits;
}

static void e_init(seq_host *h)
{
    int i;
    for (i = 0; i < 3; i++) { h->st.euclid.bits[i] = 0; h->st.euclid.key_h[i] = 255;
                              h->st.euclid.key_s[i] = 255; h->st.euclid.walk[i] = 0; }
}
static void e_reseed(seq_host *h) { e_init(h); }

static void e_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int layers = h->mp[0], l, nn = 0, hit_mask = 0;
    for (l = 0; l < layers && l < 3; l++) {
        int hits = h->mp[1 + l * 3], steps = h->mp[2 + l * 3], rot = h->mp[3 + l * 3];
        int pos, key_ok;
        if (hits > steps) hits = steps;
        key_ok = h->st.euclid.key_h[l] == hits && h->st.euclid.key_s[l] == steps;
        if (!key_ok) { h->st.euclid.key_h[l] = (uint8_t)hits; h->st.euclid.key_s[l] = (uint8_t)steps; }
        h->st.euclid.bits[l] = seq_euclid_bits(hits, steps, rot);
        pos = (int)(step % (uint32_t)steps);
        if ((h->st.euclid.bits[l] >> pos) & 1u) {
            seq_note_out *nt = &o->note[nn++];
            if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[10])
                h->st.euclid.walk[l] = (int8_t)((h->st.euclid.walk[l] + (seq_rng_range(&h->rng, 2) ? 1 : -1)) % 16);
            nt->on = 1;
            nt->deg = (int8_t)(l * h->mp[11] + h->st.euclid.walk[l]);
            nt->oct = (uint8_t)(l * 85 + (seq_hash8((uint32_t)(step * 7 + l)) >> 2));
            nt->vel = (uint8_t)((pos == 0 ? 112 : 92) - l * 8);
            nt->part = (uint8_t)l;
            hit_mask |= 1 << l;
        }
    }
    o->n = (uint8_t)nn;
    o->aux = (uint16_t)(hit_mask * 8191);
}

static void e_viz(const seq_host *h, seq_viz *v)
{
    int l, i, steps;
    v->w = 16; v->h = (uint8_t)h->mp[0];
    for (l = 0; l < h->mp[0] && l < 3; l++) {
        uint32_t b = seq_euclid_bits(h->mp[1 + l * 3], h->mp[2 + l * 3], h->mp[3 + l * 3]);
        steps = h->mp[2 + l * 3];
        for (i = 0; i < 16 && i < steps; i++) v->cell[l][i] = ((b >> i) & 1u) ? 255 : 30;
    }
    v->playhead = (int8_t)(h->step_idx % (uint32_t)h->mp[2] % 16);
}

const seq_mode_desc seq_mode_euclid = { "EUCLID", 12, P, e_init, e_reseed, e_step, 0, 0, e_viz };

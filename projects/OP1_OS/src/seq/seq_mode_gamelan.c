/* GAMELAN: kotekan-style interlocking pair. A seeded nuclear melody (balungan, one note per beat of
 * 4 steps) is split across two parts that share the beat: POLOS plays the core note, SANGSIH plays
 * a neighbour (core + OFFSET degrees) in the gaps. The two masks are complementary (PATTERN 0 and
 * 1) or leave one gap (2), so the pair reads as one fast line. The first note of each cycle (the
 * gong tone) drops to degree 0, lowest octave position. Designed for RATE 1/16. */
#include "seq.h"

static const char *const l_pat[] = { "IMBAL", "NOROT", "SYNCOP" };
static const seq_param_desc P[] = {
    { "PATTERN", 0, 2, 0, 0, l_pat },
    { "OFFSET",  1, 3, 1, 0, 0 },
    { "CORELEN", 4, 16, 8, 0, 0 },
    { "MUTATE",  0, 100, 15, 0, 0 },
    { "RANGE",   2, 7, 5, 0, 0 },
    { "GONG",    0, 1, 1, 0, 0 },
    { "SWAP",    0, 1, 0, 0, 0 },
};

static const uint8_t POLOS[3][4]   = { {1,0,1,0}, {1,0,0,1}, {1,0,1,1} };
static const uint8_t SANGSIH[3][4] = { {0,1,0,1}, {0,1,1,0}, {0,1,0,0} };

static void g_init(seq_host *h) { int i; for (i = 0; i < 16; i++) { h->st.gamelan.core[i] = 0; h->st.gamelan.coct[i] = 0; } }
static void g_reseed(seq_host *h)
{
    int i, d = h->mp[4] / 2, range = h->mp[4];
    for (i = 0; i < 16; i++) {
        d += (int)seq_rng_range(&h->rng, 5) - 2;
        if (d < 0) d = 0;
        if (d > range - 1) d = range - 1;
        h->st.gamelan.core[i] = (uint8_t)d;
        h->st.gamelan.coct[i] = (uint8_t)seq_rng_range(&h->rng, 256);
    }
}

static void g_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int len = h->mp[2], pat = h->mp[0], j = (int)(step & 3), swap = h->mp[6];
    int cell = (int)((step >> 2) % (uint32_t)len), isP, isS, deg, range = h->mp[4];

    if (cell == 0 && j == 0 && step && seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[3]) {
        int k = (int)seq_rng_range(&h->rng, (uint32_t)len), d;
        d = (int)h->st.gamelan.core[k] + (int)seq_rng_range(&h->rng, 5) - 2;
        if (d < 0) d = 0; if (d > range - 1) d = range - 1;
        h->st.gamelan.core[k] = (uint8_t)d;
    }
    isP = swap ? SANGSIH[pat][j] : POLOS[pat][j];
    isS = swap ? POLOS[pat][j] : SANGSIH[pat][j];
    o->n = 0;
    if (!isP && !isS) return;
    deg = h->st.gamelan.core[cell];
    if (isS) deg += h->mp[1];
    o->n = 1;
    o->note[0].on = 1;
    o->note[0].deg = (int8_t)deg;
    o->note[0].oct = h->st.gamelan.coct[cell];
    o->note[0].vel = (uint8_t)(isP ? 104 : 84);
    o->note[0].part = (uint8_t)(isS ? 1 : 0);
    if (h->mp[5] && cell == 0 && j == 0) { o->note[0].deg = 0; o->note[0].oct = 0; o->note[0].vel = 124; }
    o->aux = (uint16_t)(isS ? 65535 : 0);
}

static void g_viz(const seq_host *h, seq_viz *v)
{
    int i, len = h->mp[2], j, pat = h->mp[0];
    v->w = 16; v->h = 2;
    for (i = 0; i < 16; i++) {
        j = i & 3;
        v->cell[0][i] = (uint8_t)((h->mp[6] ? SANGSIH : POLOS)[pat][j] ? 40 + 215 * h->st.gamelan.core[(i >> 2) % len] / 7 : 0);
        v->cell[1][i] = (uint8_t)((h->mp[6] ? POLOS : SANGSIH)[pat][j] ? 40 + 215 * h->st.gamelan.core[(i >> 2) % len] / 7 : 0);
    }
    v->playhead = (int8_t)(h->step_idx & 15);
}

const seq_mode_desc seq_mode_gamelan = { "GAMELAN", 7, P, g_init, g_reseed, g_step, 0, 0, g_viz };

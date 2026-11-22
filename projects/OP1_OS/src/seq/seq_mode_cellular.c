/* CELLULAR: cellular automata to pitch + gate.
 *  DIM 0 (1D): elementary rule (RULE 0-255, 30/90/110 are the interesting ones) on a ring of WIDTH
 *    cells; one generation per RATE steps. Pitch = the row value mapped to RANGE degrees, gate = the
 *    centre cell.
 *  DIM 1 (2D): Conway Life on an 8x8 torus. The playhead sweeps the columns (one column per step);
 *    live cells in the column are notes (row = scale degree, up to 3, lowest first). The grid
 *    advances one generation per sweep x RATE. A dead / frozen grid is re-seeded with a few random
 *    cells so it never goes silent for good. INJECT = % chance per generation of one random flip.
 * Same seed = same automaton history. */
#include "seq.h"

static const char *const l_dim[] = { "1D", "2D LIFE" };
static const seq_param_desc P[] = {
    { "DIM",     0, 1, 0, 0, l_dim },
    { "RULE",    0, 255, 30, 0, 0 },
    { "WIDTH",   8, 16, 16, 0, 0 },
    { "RATE",    1, 8, 1, 0, 0 },
    { "RANGE",   1, 12, 7, 0, 0 },
    { "INJECT",  0, 100, 4, 0, 0 },
    { "DENSITY", 5, 95, 40, 0, 0 },
};

static int popc(uint32_t x) { int c = 0; while (x) { c += x & 1; x >>= 1; } return c; }

static void fill(seq_host *h)
{
    int i, w = h->mp[2], r;
    uint32_t row = 0;
    for (i = 0; i < w; i++) if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[6]) row |= 1u << i;
    if (!row) row = 1u << (w / 2);
    h->st.cell.row = row;
    for (r = 0; r < 8; r++) {
        uint8_t b = 0;
        for (i = 0; i < 8; i++) if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[6]) b |= (uint8_t)(1u << i);
        h->st.cell.g[r] = b;
    }
    h->st.cell.gen = 0; h->st.cell.prev_hash = 0;
}
static void c_init(seq_host *h) { fill(h); }
static void c_reseed(seq_host *h) { fill(h); }

static void gen1d(seq_host *h)
{
    int w = h->mp[2], rule = h->mp[1], i;
    uint32_t row = h->st.cell.row & ((1u << w) - 1u), nr = 0;
    for (i = 0; i < w; i++) {
        int l = (row >> ((i + 1) % w)) & 1, c = (row >> i) & 1, r = (row >> ((i + w - 1) % w)) & 1;
        if ((rule >> (l << 2 | c << 1 | r)) & 1) nr |= 1u << i;
    }
    if (!nr || seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[5])
        nr ^= 1u << seq_rng_range(&h->rng, (uint32_t)w);
    if (!nr) nr = 1u << (w / 2);
    h->st.cell.row = nr;
}

static void gen2d(seq_host *h)
{
    uint8_t n[8];
    int r, c, hash = 0, pop = 0;
    for (r = 0; r < 8; r++) {
        uint8_t row = 0;
        for (c = 0; c < 8; c++) {
            int cnt = 0, dr, dc;
            for (dr = -1; dr <= 1; dr++) for (dc = -1; dc <= 1; dc++)
                if (dr || dc) cnt += (h->st.cell.g[(r + dr + 8) & 7] >> ((c + dc + 8) & 7)) & 1;
            if (cnt == 3 || (cnt == 2 && ((h->st.cell.g[r] >> c) & 1))) row |= (uint8_t)(1u << c);
        }
        n[r] = row;
    }
    for (r = 0; r < 8; r++) { h->st.cell.g[r] = n[r]; hash = hash * 31 + n[r]; pop += popc(n[r]); }
    if (pop < 4 || (uint8_t)hash == h->st.cell.prev_hash) {          /* dead or frozen: poke it */
        for (c = 0; c < 5; c++)
            h->st.cell.g[seq_rng_range(&h->rng, 8)] |= (uint8_t)(1u << seq_rng_range(&h->rng, 8));
    } else if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[5]) {
        h->st.cell.g[seq_rng_range(&h->rng, 8)] ^= (uint8_t)(1u << seq_rng_range(&h->rng, 8));
    }
    h->st.cell.prev_hash = (uint8_t)hash;
}

static void c_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int rate = h->mp[3], n = seq_scale_size(h), range = h->mp[4];
    o->n = 0;
    if (range > n) range = n;
    if (h->mp[0] == 0) {
        int w = h->mp[2];
        uint32_t row;
        if (step && (step % (uint32_t)rate) == 0) gen1d(h);
        row = h->st.cell.row & ((1u << w) - 1u);
        h->st.cell.pop = (uint16_t)(popc(row) * 65535 / w);
        o->aux = h->st.cell.pop;
        if ((row >> (w / 2)) & 1u) {
            o->n = 1;
            o->note[0].on = 1;
            o->note[0].deg = (int8_t)(((row & 0xFFu) * (uint32_t)range) >> 8);
            o->note[0].oct = seq_hash8(row);
            o->note[0].vel = (uint8_t)(72 + (popc(row) * 3 > 40 ? 40 : popc(row) * 3));
        }
    } else {
        int col = (int)(step & 7), r, pop = 0;
        if (col == 0 && step && ((step >> 3) % (uint32_t)rate) == 0) gen2d(h);
        for (r = 0; r < 8; r++) {
            pop += popc(h->st.cell.g[r]);
            if (((h->st.cell.g[r] >> col) & 1) && o->n < SEQ_POLY) {
                seq_note_out *nt = &o->note[o->n++];
                nt->on = 1;
                nt->deg = (int8_t)((r * range) / 8);
                nt->oct = seq_hash8((uint32_t)(r * 8 + col));
                nt->vel = (uint8_t)(70 + r * 5);
            }
        }
        h->st.cell.pop = (uint16_t)(pop * 1023);
        o->aux = h->st.cell.pop;
    }
}

static void c_viz(const seq_host *h, seq_viz *v)
{
    int i, r;
    if (h->mp[0] == 0) {
        v->w = 16; v->h = 1;
        for (i = 0; i < h->mp[2]; i++) v->cell[0][i] = ((h->st.cell.row >> i) & 1u) ? 255 : 30;
    } else {
        v->w = 8; v->h = 8;
        for (r = 0; r < 8; r++) for (i = 0; i < 8; i++) v->cell[r][i] = ((h->st.cell.g[r] >> i) & 1u) ? 255 : 30;
        v->playhead = (int8_t)(h->step_idx & 7);
    }
}

const seq_mode_desc seq_mode_cellular = { "CELLULAR", 7, P, c_init, c_reseed, c_step, 0, 0, c_viz };

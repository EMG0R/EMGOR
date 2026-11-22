/* MARKOV (basic): learns from what Emory plays. Every time note A is followed by note B (note-ons,
 * pitch classes), the weight w[A][B] += INC. Generation walks the chain: next pitch class is drawn
 * from row w[cur][*] proportional to weight, so a transition played more is proportionally more
 * likely. FORGET decays every weight once per bar (16 steps); MUTATE = % chance of an unlearned scale
 * tone instead; CLEAR wipes the matrix. An empty row falls back to the column totals (what has been
 * played most); an empty matrix is silent (rests) until something is played. The matrix lives
 * outside the mode union, so it survives switching modes. */
#include "seq.h"

#define WCAP 4095

static const seq_param_desc P[] = {
    { "FORGET", 0, 100, 10, 0, 0 },
    { "MUTATE", 0, 100, 5, 0, 0 },
    { "LEARN",  0, 1, 1, 0, 0 },
    { "INC",    1, 64, 16, 0, 0 },
    { "CLEAR",  0, 1, 0, SEQ_PF_ACTION, 0 },
};

int seq_markov_weight(const seq_host *h, int a, int b) { return h->markov.w[a % 12][b % 12]; }

static void clear_all(seq_host *h)
{
    int a, b;
    for (a = 0; a < 12; a++) for (b = 0; b < 12; b++) h->markov.w[a][b] = 0;
    h->markov.cur = -1; h->markov.prev = -1;
}

static void m_init(seq_host *h) { h->markov.bars = 0; }
static void m_reseed(seq_host *h)
{
    h->markov.bars = 0;
    h->markov.cur = h->markov.prev;      /* start from the last note Emory played */
}

static void m_note_in(seq_host *h, int note, int vel, int on)
{
    int pc = note % 12, a;
    (void)vel;
    if (!on || note < 0) return;
    if (h->mp[2] && h->markov.prev >= 0) {
        int pv = h->markov.prev, w = h->markov.w[pv][pc] + h->mp[3];
        if (w > WCAP) {                    /* saturated: halve the row, keeps ratios */
            for (a = 0; a < 12; a++) h->markov.w[pv][a] = (uint16_t)(h->markov.w[pv][a] / 2);
            w = h->markov.w[pv][pc] + h->mp[3];
        }
        h->markov.w[pv][pc] = (uint16_t)w;
    }
    h->markov.prev = (int8_t)pc;
    if (h->markov.cur < 0) h->markov.cur = (int8_t)pc;
}

static void decay(seq_host *h)
{
    int a, b, f = h->mp[0];
    if (!f) return;
    for (a = 0; a < 12; a++)
        for (b = 0; b < 12; b++) {
            int w = h->markov.w[a][b], d;
            if (!w) continue;
            d = w * f / 200;
            if (d < 1) d = 1;
            h->markov.w[a][b] = (uint16_t)(w - d);
        }
}

static void m_step(seq_host *h, uint32_t step, seq_step_out *o)
{
    int cur = h->markov.cur, b, next = -1;
    uint32_t sum = 0, r, acc = 0, tot = 0;
    uint16_t wsel = 0;

    if (step && (step % 16) == 0) decay(h);
    o->n = 0;
    if (cur < 0) return;
    for (b = 0; b < 12; b++) sum += h->markov.w[cur][b];
    if (sum == 0) {                                      /* column totals */
        for (b = 0; b < 12; b++) { int a; uint32_t c = 0; for (a = 0; a < 12; a++) c += h->markov.w[a][b]; tot += c; }
        if (tot == 0) return;
    }
    if (seq_rng_range(&h->rng, 100) < (uint32_t)h->mp[1]) {
        const seq_scale *sc = &seq_scales[h->up[SEQ_U_SCALE]];
        next = (h->up[SEQ_U_ROOT] + sc->iv[seq_rng_range(&h->rng, sc->n)]) % 12;
        wsel = 0;
    } else if (sum) {
        r = seq_rng_range(&h->rng, sum > 65535 ? 65535 : sum);
        for (b = 0; b < 12; b++) { acc += h->markov.w[cur][b]; if (r < acc) { next = b; wsel = h->markov.w[cur][b]; break; } }
    } else {
        r = seq_rng_range(&h->rng, tot > 65535 ? 65535 : tot);
        for (b = 0; b < 12; b++) {
            int a; uint32_t c = 0;
            for (a = 0; a < 12; a++) c += h->markov.w[a][b];
            acc += c; if (r < acc) { next = b; wsel = (uint16_t)(c > 65535 ? 65535 : c); break; }
        }
    }
    if (next < 0) next = cur;
    h->markov.cur = (int8_t)next;
    o->n = 1;
    o->note[0].on = 1; o->note[0].semi = 1; o->note[0].deg = (int8_t)next;
    o->note[0].oct = seq_hash8(step * 2654435761u + (uint32_t)next);
    o->note[0].vel = (uint8_t)(80 + (seq_hash8(step + 3u) >> 3));
    o->aux = (uint16_t)(wsel * 16u > 65535 ? 65535 : wsel * 16u);
}

static void m_action(seq_host *h, int param) { if (param == 4) clear_all(h); }

static void m_viz(const seq_host *h, seq_viz *v)
{
    int a, b, mx = 1;
    v->w = 12; v->h = 12;
    for (a = 0; a < 12; a++) for (b = 0; b < 12; b++) if (h->markov.w[a][b] > mx) mx = h->markov.w[a][b];
    for (a = 0; a < 12; a++)
        for (b = 0; b < 12; b++)
            v->cell[a][b] = (uint8_t)(h->markov.w[a][b] * 255 / mx);
    v->playhead = h->markov.cur;
}

const seq_mode_desc seq_mode_markov = { "MARKOV", 5, P, m_init, m_reseed, m_step, m_note_in, m_action, m_viz };

/* seq_host.c - transport, clock, scheduling, universal params, scale/octave mapping, UI model. */
#include "seq.h"

/* ---------------------------------------------------------------- PRNG */
void seq_rng_seed(seq_rng *r, uint32_t seed)
{
    uint32_t z = seed + 0x9E3779B9u;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    z ^= z >> 16;
    r->s = z ? z : 0x1234567u;
}
uint32_t seq_rng_next(seq_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    r->s = x;
    return x * 0x2545F491u;
}
uint32_t seq_rng_range(seq_rng *r, uint32_t n)
{
    return n ? ((seq_rng_next(r) >> 16) * n) >> 16 : 0;
}
uint8_t seq_hash8(uint32_t x)
{
    x *= 0x9E3779B1u; x ^= x >> 15; x *= 0x85EBCA6Bu; x ^= x >> 13;
    return (uint8_t)(x >> 24);
}

/* ---------------------------------------------------------------- scales */
const seq_scale seq_scales[SEQ_SC_COUNT] = {
    { "LYDIAN",   7, {0,2,4,6,7,9,11} },
    { "MAJOR",    7, {0,2,4,5,7,9,11} },
    { "MINOR",    7, {0,2,3,5,7,8,10} },
    { "DORIAN",   7, {0,2,3,5,7,9,10} },
    { "MIXOLYD",  7, {0,2,4,5,7,9,10} },
    { "PHRYGIAN", 7, {0,1,3,5,7,8,10} },
    { "PENT MAJ", 5, {0,2,4,7,9} },
    { "PENT MIN", 5, {0,3,5,7,10} },
    { "BLUES",    6, {0,3,5,6,7,10} },
    { "WHOLE",    6, {0,2,4,6,8,10} },
    { "HARM MIN", 7, {0,2,3,5,7,8,11} },
    { "SLENDRO",  5, {0,2,5,7,9} },       /* 12-TET approximation */
    { "PELOG",    5, {0,1,3,7,8} },       /* 12-TET approximation */
    { "CHROMATIC", 12, {0,1,2,3,4,5,6,7,8,9,10,11} },
};

/* ---------------------------------------------------------------- params */
static const char *const l_rate[]  = { "1/8", "1/16", "1/32", "1/8T", "1/16T" };
static const char *const l_clk[]   = { "INT", "EXT", "MASTER" };
static const char *const l_root[]  = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *const l_scale[] = { "LYDIAN","MAJOR","MINOR","DORIAN","MIXOLYD","PHRYGIAN","PENT MAJ",
    "PENT MIN","BLUES","WHOLE","HARM MIN","SLENDRO","PELOG","CHROMATIC" };
const seq_param_desc seq_uparams[SEQ_U_COUNT] = {
    [SEQ_U_SCALE]   = { "SCALE",  0, SEQ_SC_COUNT - 1, SEQ_SC_LYDIAN, 0, l_scale },
    [SEQ_U_ROOT]    = { "ROOT",   0, 11, 0, 0, l_root },
    [SEQ_U_OCT]     = { "OCTAVE", 1, 4, 4, 0, 0 },          /* base C = 12 * value */
    [SEQ_U_PROB]    = { "PROB",   0, 100, 100, 0, 0 },
    [SEQ_U_RATCHET] = { "RATCHET", 0, 100, 0, 0, 0 },       /* % of steps that ratchet x2..x4 */
    [SEQ_U_SWING]   = { "SWING",  0, 100, 0, 0, 0 },        /* 100 = odd steps half a step late */
    [SEQ_U_GATE]    = { "GATE",   1, 200, 50, 0, 0 },       /* % of step */
    [SEQ_U_SPREAD]  = { "OCT SPREAD", 0, 100, 0, 0, 0 },    /* linear: 100 = 5 octaves */
    [SEQ_U_RATE]    = { "RATE",   0, 4, 1, 0, l_rate },
    [SEQ_U_BPM]     = { "BPM",    40, 240, 120, 0, 0 },
    [SEQ_U_CLKSRC]  = { "CLOCK",  0, 2, 0, 0, l_clk },
    [SEQ_U_VEL]     = { "LEVEL",  10, 100, 100, 0, 0 },
};

const seq_mode_desc *const seq_modes[SEQ_NMODES] = {
    &seq_mode_turing, &seq_mode_euclid, &seq_mode_markov, &seq_mode_gamelan,
    &seq_mode_cellular, &seq_mode_total
};
const char *seq_mode_name(int m) { return seq_modes[m < 0 || m >= SEQ_NMODES ? 0 : m]->name; }

int seq_scale_size(const seq_host *h) { return seq_scales[h->up[SEQ_U_SCALE]].n; }

int seq_ticks_per_step(const seq_host *h)
{
    static const uint8_t t[5] = { 48, 24, 12, 32, 16 };
    int r = h->up[SEQ_U_RATE];
    return t[r < 0 || r > 4 ? 1 : r];
}

/* ---------------------------------------------------------------- pitch */
int seq_pitch_of(const seq_host *h, const seq_note_out *nt)
{
    const seq_scale *sc = &seq_scales[h->up[SEQ_U_SCALE]];
    int n = sc->n, root = h->up[SEQ_U_ROOT], d, extra, idx, pitch;

    if (nt->semi) {                       /* snap pitch class to the nearest scale tone */
        int rel = (((int)nt->deg - root) % 12 + 12) % 12, best = 0, bd = 99, i, dist;
        for (i = 0; i < n; i++) {
            dist = rel - sc->iv[i]; if (dist < 0) dist = -dist;
            if (12 - dist < dist) dist = 12 - dist;
            if (dist < bd) { bd = dist; best = i; }   /* ties: lower degree */
        }
        d = best;
    } else {
        d = ((int)nt->deg % n + n) % n;
    }
    extra = ((int)nt->oct * (h->up[SEQ_U_SPREAD] * n * 5 / 100)) >> 8;
    idx = d + extra;
    pitch = 12 * h->up[SEQ_U_OCT] + root + 12 * (idx / n) + sc->iv[idx % n];
    while (pitch > 127) pitch -= 12;
    return pitch;
}

/* ---------------------------------------------------------------- events */
static void ev_push(seq_host *h, int type, int note, int vel)
{
    uint16_t nx = (uint16_t)((h->ev_head + 1) & (SEQ_EVQ - 1));
    if (nx == h->ev_tail) { h->dropped++; return; }
    h->evq[h->ev_head].tick = h->tick;
    h->evq[h->ev_head].type = (uint8_t)type;
    h->evq[h->ev_head].note = (uint8_t)note;
    h->evq[h->ev_head].vel = (uint8_t)vel;
    h->evq[h->ev_head].pad = 0;
    h->ev_head = nx;
}
int seq_event_pop(seq_host *h, seq_event *ev)
{
    if (h->ev_tail == h->ev_head) return 0;
    *ev = h->evq[h->ev_tail];
    h->ev_tail = (uint16_t)((h->ev_tail + 1) & (SEQ_EVQ - 1));
    return 1;
}

static void sched(seq_host *h, uint32_t on_t, uint32_t off_t, int note, int vel)
{
    int i, a = -1, b = -1;
    for (i = 0; i < SEQ_PENDING; i++)
        if (!h->pend[i].used) { if (a < 0) a = i; else { b = i; break; } }
    if (b < 0) { h->dropped++; return; }
    h->pid++;
    h->pend[a].id = h->pend[b].id = h->pid;
    h->pend[a].used = 1; h->pend[a].on = 1; h->pend[a].due = on_t;
    h->pend[a].note = (uint8_t)note; h->pend[a].vel = (uint8_t)vel;
    h->pend[b].used = 1; h->pend[b].on = 0; h->pend[b].due = off_t;
    h->pend[b].note = (uint8_t)note; h->pend[b].vel = 0;
}

static int on_pending(const seq_host *h, uint16_t id)
{
    int j;
    for (j = 0; j < SEQ_PENDING; j++)
        if (h->pend[j].used && h->pend[j].on && h->pend[j].id == id) return 1;
    return 0;
}

static void flush_due(seq_host *h)
{
    int i, j;
    for (i = 0; i < SEQ_PENDING; i++)                       /* offs first */
        if (h->pend[i].used && !h->pend[i].on && (int32_t)(h->pend[i].due - h->tick) <= 0) {
            ev_push(h, SEQ_EV_OFF, h->pend[i].note, 0);
            h->pend[i].used = 0;
        }
    for (i = 0; i < SEQ_PENDING; i++)
        if (h->pend[i].used && h->pend[i].on && (int32_t)(h->pend[i].due - h->tick) <= 0) {
            for (j = 0; j < SEQ_PENDING; j++)               /* retrigger: release the old copy first */
                if (h->pend[j].used && !h->pend[j].on && h->pend[j].note == h->pend[i].note
                    && h->pend[j].id != h->pend[i].id && !on_pending(h, h->pend[j].id)) {
                    ev_push(h, SEQ_EV_OFF, h->pend[j].note, 0);
                    h->pend[j].used = 0;
                    break;
                }
            ev_push(h, SEQ_EV_ON, h->pend[i].note, h->pend[i].vel);
            h->pend[i].used = 0;
        }
}

/* ---------------------------------------------------------------- step */
static void do_step(seq_host *h)
{
    seq_step_out out;
    const seq_mode_desc *m = seq_modes[h->mode];
    int tps = seq_ticks_per_step(h), i, k, c, nsched = 0, bar_steps;
    uint32_t delay = 0, rr, rc, pr[SEQ_POLY];
    int first_note = -1, first_vel = 0;

    out.n = 0; out.aux = 0;
    for (i = 0; i < SEQ_POLY; i++) {
        out.note[i].on = 0; out.note[i].deg = 0; out.note[i].semi = 0;
        out.note[i].oct = 0; out.note[i].vel = 100; out.note[i].part = 0;
    }
    m->step(h, h->step_idx, &out);
    if (out.n > SEQ_POLY) out.n = SEQ_POLY;

    /* fixed number of host draws per step: output is stable when probability changes */
    rr = seq_rng_range(&h->rng, 100);
    rc = seq_rng_range(&h->rng, 3) + 2;
    for (i = 0; i < SEQ_POLY; i++) pr[i] = seq_rng_range(&h->rng, 100);

    if (h->step_idx & 1) delay = (uint32_t)(tps * h->up[SEQ_U_SWING]) / 200;
    c = (rr < (uint32_t)h->up[SEQ_U_RATCHET]) ? (int)rc : 1;

    for (i = 0; i < out.n; i++) {
        int note, vel, gt;
        if (!out.note[i].on || pr[i] >= (uint32_t)h->up[SEQ_U_PROB]) continue;
        note = seq_pitch_of(h, &out.note[i]);
        vel = out.note[i].vel * h->up[SEQ_U_VEL] / 100;
        if (vel < 1) vel = 1; if (vel > 127) vel = 127;
        gt = (tps * h->up[SEQ_U_GATE] / 100) / c;
        if (gt < 1) gt = 1;
        for (k = 0; k < c; k++) {
            uint32_t on_t = h->tick + delay + (uint32_t)(k * tps / c);
            int v = vel * (100 - 12 * k) / 100;
            sched(h, on_t, on_t + (uint32_t)gt, note, v < 1 ? 1 : v);
        }
        if (first_note < 0) { first_note = note; first_vel = vel; }
        nsched++;
    }

    /* mod stream */
    bar_steps = 4 * (SEQ_PPQ / tps);
    {
        int w = (int)h->walk + (int)seq_rng_range(&h->rng, 2049) - 1024;
        h->walk = (uint16_t)(w < 0 ? 0 : w > 65535 ? 65535 : w);
    }
    h->gate_hist = (uint16_t)((h->gate_hist << 1) | (nsched ? 1 : 0));
    h->mod.step = h->step_idx;
    if (first_note >= 0) {
        int lo = 12 * h->up[SEQ_U_OCT] + h->up[SEQ_U_ROOT];
        int hi = lo + 11 + h->up[SEQ_U_SPREAD] * 60 / 100;
        int p = (first_note - lo) * 65535 / (hi - lo);
        h->mod.pitch = (uint16_t)(p < 0 ? 0 : p > 65535 ? 65535 : p);
        h->mod.note = (uint8_t)first_note;
    } else {
        h->mod.note = 0;
    }
    h->mod.rnd = (uint16_t)(seq_rng_next(&h->rng) >> 16);
    h->mod.walk = h->walk;
    h->mod.aux = out.aux;
    h->mod.phase = (uint16_t)(((h->step_idx % (uint32_t)bar_steps) * 65535u) / (uint32_t)bar_steps);
    {
        uint16_t g = h->gate_hist; int cnt = 0;
        while (g) { cnt += g & 1; g >>= 1; }
        h->mod.energy = (uint16_t)(cnt * 4095);
    }
    h->mod.gate = nsched ? 1 : 0;
    h->mod.vel = (uint8_t)(first_note >= 0 ? first_vel : 0);
    h->mod.ratchet = (uint8_t)c;
    h->mod.notes = (uint8_t)nsched;
    h->mod.accent = (uint8_t)((h->step_idx % (uint32_t)bar_steps) == 0);
    if (h->mod_cb) h->mod_cb(h->mod_user, &h->mod);
}

void seq_tick_one(seq_host *h)
{
    int tps;
    if (!h->running) return;
    tps = seq_ticks_per_step(h);
    if (h->tick % (uint32_t)tps == 0) {
        do_step(h);
        h->step_idx++;
    }
    flush_due(h);
    if (h->clock_io && h->up[SEQ_U_CLKSRC] == SEQ_CLK_MASTER && (h->tick % (SEQ_PPQ / 24)) == 0
        && h->clock_io->pulse)
        h->clock_io->pulse(h->clock_io->user);
    h->tick++;
}

/* ---------------------------------------------------------------- transport */
void seq_reseed(seq_host *h, uint32_t seed)
{
    h->seed = seed;
    seq_rng_seed(&h->rng, seed + 0x1000193u * (uint32_t)(h->mode + 1));
    h->walk = 32768;
    seq_modes[h->mode]->reseed(h);
}

static void clear_pending(seq_host *h)
{
    int i;
    for (i = 0; i < SEQ_PENDING; i++) h->pend[i].used = 0;
}

void seq_start(seq_host *h)
{
    clear_pending(h);
    h->tick = 0; h->step_idx = 0; h->acc = 0; h->gate_hist = 0;
    seq_reseed(h, h->seed);
    h->running = 1;
    if (h->clock_io && h->up[SEQ_U_CLKSRC] == SEQ_CLK_MASTER && h->clock_io->start)
        h->clock_io->start(h->clock_io->user);
}

void seq_stop(seq_host *h)
{
    int i;
    if (h->running)
        for (i = 0; i < SEQ_PENDING; i++)
            if (h->pend[i].used && !h->pend[i].on) {        /* only offs whose note-on was already sent */
                if (!on_pending(h, h->pend[i].id)) ev_push(h, SEQ_EV_OFF, h->pend[i].note, 0);
            }
    clear_pending(h);
    if (h->running && h->clock_io && h->up[SEQ_U_CLKSRC] == SEQ_CLK_MASTER && h->clock_io->stop)
        h->clock_io->stop(h->clock_io->user);
    h->running = 0;
}

void seq_set_bpm_x100(seq_host *h, uint32_t b)
{
    if (b < 4000) b = 4000;
    if (b > 24000) b = 24000;
    h->bpm_x100 = b;
}

void seq_run_frames(seq_host *h, uint32_t frames, uint32_t sr)
{
    uint32_t thr = sr * 6000u;
    if (!h->running || h->up[SEQ_U_CLKSRC] == SEQ_CLK_EXTERNAL) return;
    h->acc += frames * h->bpm_x100 * SEQ_PPQ;
    while (h->acc >= thr) { h->acc -= thr; seq_tick_one(h); }
}

void seq_ext_pulse(seq_host *h)
{
    int i;
    if (h->up[SEQ_U_CLKSRC] != SEQ_CLK_EXTERNAL || !h->running) return;
    for (i = 0; i < SEQ_PPQ / 24; i++) seq_tick_one(h);
}
void seq_ext_start(seq_host *h)    { if (h->up[SEQ_U_CLKSRC] == SEQ_CLK_EXTERNAL) seq_start(h); }
void seq_ext_continue(seq_host *h) { if (h->up[SEQ_U_CLKSRC] == SEQ_CLK_EXTERNAL) h->running = 1; }
void seq_ext_stop(seq_host *h)     { if (h->up[SEQ_U_CLKSRC] == SEQ_CLK_EXTERNAL) seq_stop(h); }

void seq_note_in(seq_host *h, int note, int vel, int on)
{
    const seq_mode_desc *m = seq_modes[h->mode];
    if (m->note_in) m->note_in(h, note, vel, on);
}
const seq_mod *seq_mod_get(const seq_host *h) { return &h->mod; }

static void mode_defaults(seq_host *h)
{
    const seq_mode_desc *m = seq_modes[h->mode];
    int i;
    for (i = 0; i < SEQ_MAX_MP; i++) h->mp[i] = i < m->nparams ? m->params[i].def : 0;
}

void seq_set_mode(seq_host *h, int mode)
{
    if (mode < 0 || mode >= SEQ_NMODES) mode = 0;
    h->mode = (uint8_t)mode;
    mode_defaults(h);
    seq_modes[h->mode]->init(h);
    seq_reseed(h, h->seed);
}

void seq_init(seq_host *h, uint32_t seed)
{
    unsigned char *p = (unsigned char *)h;
    unsigned i;
    for (i = 0; i < sizeof(*h); i++) p[i] = 0;
    for (i = 0; i < SEQ_U_COUNT; i++) h->up[i] = seq_uparams[i].def;
    h->bpm_x100 = (uint32_t)h->up[SEQ_U_BPM] * 100;
    h->markov.cur = -1; h->markov.prev = -1;
    h->seed = seed;
    h->mode = SEQ_TURING;
    mode_defaults(h);
    seq_modes[h->mode]->init(h);
    seq_reseed(h, seed);
}

void seq_viz_get(const seq_host *h, seq_viz *v)
{
    unsigned i;
    unsigned char *p = (unsigned char *)v;
    for (i = 0; i < sizeof(*v); i++) p[i] = 0;
    v->playhead = -1;
    v->step = (uint8_t)h->step_idx;
    if (seq_modes[h->mode]->viz) seq_modes[h->mode]->viz(h, v);
}

/* ---------------------------------------------------------------- UI model */
static const char *const l_modes[] = { "TURING", "EUCLID", "MARKOV", "GAMELAN", "CELLULAR", "TOTAL" };
static const char *const l_run[]   = { "STOPPED", "RUNNING" };

int seq_ui_subpages(const seq_host *h, int group)
{
    if (group == 0) return (seq_modes[h->mode]->nparams + 3) / 4;
    if (group == 1) return 2;
    return 1;
}

static void cell_set(seq_ui_cell *c, const char *name, int kind, int idx, int v, int mn, int mx,
                     const char *const *labels)
{
    c->name = name; c->kind = (uint8_t)kind; c->idx = (uint8_t)idx;
    c->value = (int16_t)v; c->min = (int16_t)mn; c->max = (int16_t)mx;
    c->text = labels ? labels[v - mn] : 0;
}
static void cell_uni(const seq_host *h, seq_ui_cell *c, int u)
{
    const seq_param_desc *d = &seq_uparams[u];
    cell_set(c, d->name, SEQ_CELL_UNI, u, h->up[u], d->min, d->max, d->labels);
}

void seq_ui_page(const seq_host *h, int group, int sub, seq_ui_cell cells[4])
{
    int k;
    for (k = 0; k < 4; k++) { cells[k].name = ""; cells[k].text = 0; cells[k].value = 0;
                              cells[k].min = cells[k].max = 0; cells[k].kind = SEQ_CELL_NONE; cells[k].idx = 0; }
    if (group == 0) {
        const seq_mode_desc *m = seq_modes[h->mode];
        for (k = 0; k < 4; k++) {
            int i = sub * 4 + k;
            if (i < m->nparams) {
                const seq_param_desc *d = &m->params[i];
                cell_set(&cells[k], d->name, (d->flags & SEQ_PF_ACTION) ? SEQ_CELL_CLEAR : SEQ_CELL_MP,
                         i, h->mp[i], d->min, d->max, d->labels);
            }
        }
    } else if (group == 1) {
        static const uint8_t p[2][4] = { { SEQ_U_SCALE, SEQ_U_ROOT, SEQ_U_OCT, SEQ_U_SPREAD },
                                         { SEQ_U_PROB, SEQ_U_RATCHET, SEQ_U_VEL, SEQ_U_GATE } };
        for (k = 0; k < 4; k++) cell_uni(h, &cells[k], p[sub & 1][k]);
    } else if (group == 2) {
        static const uint8_t p[4] = { SEQ_U_BPM, SEQ_U_RATE, SEQ_U_SWING, SEQ_U_CLKSRC };
        for (k = 0; k < 4; k++) cell_uni(h, &cells[k], p[k]);
    } else {
        cell_set(&cells[0], "MODE", SEQ_CELL_MODE, 0, h->mode, 0, SEQ_NMODES - 1, l_modes);
        cell_set(&cells[1], "SEED", SEQ_CELL_SEED, 0, (int)(h->seed & 0x3FFF), 0, 0x3FFF, 0);
        cell_set(&cells[2], "RESEED", SEQ_CELL_RESEED, 0, 0, 0, 1, 0);
        cell_set(&cells[3], "RUN", SEQ_CELL_RUN, 0, h->running, 0, 1, l_run);
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void set_uni(seq_host *h, int u, int v)
{
    h->up[u] = (int16_t)clampi(v, seq_uparams[u].min, seq_uparams[u].max);
    if (u == SEQ_U_BPM) h->bpm_x100 = (uint32_t)h->up[u] * 100;
}

int seq_encoder(seq_host *h, int group, int sub, int knob, int delta, int coarse)
{
    seq_ui_cell c[4];
    int step, before, now;
    if (knob < 0 || knob > 3 || delta == 0) return 0;
    seq_ui_page(h, group, sub, c);
    step = coarse && (c[knob].max - c[knob].min) > 20 ? (c[knob].max - c[knob].min) / 10 : 1;
    switch (c[knob].kind) {
    case SEQ_CELL_UNI:
        before = h->up[c[knob].idx];
        set_uni(h, c[knob].idx, before + delta * step);
        return h->up[c[knob].idx] != before;
    case SEQ_CELL_MP:
        before = h->mp[c[knob].idx];
        now = clampi(before + delta * step, c[knob].min, c[knob].max);
        h->mp[c[knob].idx] = (int16_t)now;
        return now != before;
    case SEQ_CELL_CLEAR:
        if (delta > 0 && seq_modes[h->mode]->action) seq_modes[h->mode]->action(h, c[knob].idx);
        return delta > 0;
    case SEQ_CELL_SEED:
        seq_reseed(h, h->seed + (uint32_t)(delta * (coarse ? 256 : 1)));
        return 1;
    case SEQ_CELL_MODE:
        now = clampi(h->mode + (delta > 0 ? 1 : -1), 0, SEQ_NMODES - 1);
        if (now == h->mode) return 0;
        seq_set_mode(h, now);
        return 1;
    case SEQ_CELL_RESEED:
        if (delta > 0) seq_reseed(h, h->seed * 1664525u + 1013904223u);
        return delta > 0;
    case SEQ_CELL_RUN:
        if (delta > 0 && !h->running) seq_start(h);
        else if (delta < 0 && h->running) seq_stop(h);
        else return 0;
        return 1;
    }
    return 0;
}

int seq_push(seq_host *h, int group, int sub, int knob)
{
    seq_ui_cell c[4];
    if (knob < 0 || knob > 3) return 0;
    seq_ui_page(h, group, sub, c);
    switch (c[knob].kind) {
    case SEQ_CELL_UNI:
        set_uni(h, c[knob].idx, seq_uparams[c[knob].idx].def);
        return 1;
    case SEQ_CELL_MP:
        h->mp[c[knob].idx] = seq_modes[h->mode]->params[c[knob].idx].def;
        return 1;
    case SEQ_CELL_CLEAR:
        if (seq_modes[h->mode]->action) seq_modes[h->mode]->action(h, c[knob].idx);
        return 1;
    case SEQ_CELL_SEED: case SEQ_CELL_RESEED:
        seq_reseed(h, h->seed * 1664525u + 1013904223u);
        return 1;
    case SEQ_CELL_MODE:
        seq_set_mode(h, (h->mode + 1) % SEQ_NMODES);
        return 1;
    case SEQ_CELL_RUN:
        if (h->running) seq_stop(h); else seq_start(h);
        return 1;
    }
    return 0;
}

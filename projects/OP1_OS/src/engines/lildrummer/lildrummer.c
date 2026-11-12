/* LIL DRUMMER core. See lildrummer.h. No libc, no divides in the sample loop. */
#include "lildrummer.h"

const char *const ld_param_names[LD_PARAMS] = {
    "TUNE", "DECAY", "PUNCH", "LEVEL", "RATIO", "INDEX", "FEEDBK", "NOISE",
    "DRIVE", "FILTER", "SNAP", "PAN", "MODEL", "CHOKE", "WIDTH", "MACRO" };
const char *const ld_model_names[LD_MODELS] = { "KICK", "TOM", "SNARE", "CLAP", "RIM", "COWBELL", "HAT", "ZAP" };
const char *const ld_key_names[LD_KEYS] = {
    "KICK", "SUB KICK", "SNARE", "TIGHT SNARE", "CLAP", "RIM", "LOW TOM", "MID TOM", "HI TOM", "CONGA",
    "CLOSED HAT", "OPEN HAT", "CYMBAL", "COWBELL", "CLAVE", "MARACAS",
    "ZAP", "ZAP 2", "FM KICK", "FM SNARE", "METAL", "BELL", "NOISE", "BLIP" };

/* archetypes: the classic 808 / FM recipe for each model. note is the settled body pitch. */
typedef struct {
    int16_t note, pen, pen_ms, dec_ms, ndec_ms;
    int16_t wb, wn, wm;
    int16_t mod_off, idx, fm_ms, fb;
    int16_t ncut, nq, nmode;
    int16_t bursts, click, drive;
    int16_t ma[6];
} ld_arch;

static const int16_t metal_note[6] = { 14286, 16031, 16891, 18427, 18572, 20314 };   /* 808 hat cluster, Q8 notes */

static const ld_arch arch[LD_MODELS] = {
 /* KICK   */ { 31, 24, 38, 480,  20, 32000,     0,     0,    0,  3000,   8,     0, 100,     0,     0, 0, 4500, 5000, {0} },
 /* TOM    */ { 47, 10, 55, 340,  40, 30000,  1500,     0,  500,  5000,  25,  2000, 100,     0,     0, 0, 3000, 3000, {0} },
 /* SNARE  */ { 54,  5, 18, 150, 210, 21000, 26000,     0, 2535,  9000,  40,     0, 100, 12000, 16000, 0, 5000, 4000, {0} },
 /* CLAP   */ { 54,  0, 10, 120, 300,  2500, 32000,     0,    0,     0,  10,     0,  88, 17000,     0, 4, 0,    3000, {0} },
 /* RIM    */ { 91,  0, 10,  26,  14, 20000,  8000,     0, 5000, 14000,  10,     0, 110,  8000, 32767, 0, 8000, 3000, {0} },
 /* COWBELL*/ { 56,  0, 10, 300, 300,     0,   600, 30000,    0,     0,  10,     0, 100, 14000,     0, 0, 1500, 3000, { 0, 0, 0, 0, 15000, 15000 } },
 /* HAT    */ { 60,  0, 10,  60,  55,     0,  3000, 32000,    0,     0,  10,     0, 114,  6000, 32767, 0, 2000, 6000, { 5400, 5400, 5400, 5400, 5400, 5400 } },
 /* ZAP    */ { 84, 36, 90, 230, 150, 30000,  2500,     0, 3700, 22000, 120,  8000,  96,  5000,     0, 0, 3000, 8000, {0} },
};

static inline int zone(int p, int n) { int z = (op_clamp(p, 0, OP_PMAX) * n) >> 15; return z >= n ? n - 1 : z; }
static inline int32_t lerpi(int32_t a, int32_t b, int32_t t) { return a + (((b - a) * t) >> 15); }
static inline int32_t sq15(int32_t l24) { int32_t x = l24 >> 9; return (x * x) >> 15; }
static inline int32_t cu15(int32_t l24) { int32_t x = l24 >> 9, x2 = (x * x) >> 15; return (x2 * x) >> 15; }

/* time in ms (scaled by param around centre, +-2.5 octaves) -> sub-block ticks */
static int32_t ticks_ms(int32_t ms, int p)
{
    uint32_t m = op_exp2_q8((uint32_t)(op_clamp(p, 0, OP_PMAX) - 16384) * 10u + 196608u);   /* 8 * 2^x * 256 */
    uint64_t t = ((uint64_t)ms * m * op_sr()) / (2048000ull * OP_SUB);
    return t < 1 ? 1 : t > 1000000 ? 1000000 : (int32_t)t;
}
static int32_t step_of(int32_t ticks) { return (1 << 24) / ticks; }

/* ---------------------------------------------------------------- setup */
void ld_set_param(ld_kit *k, int key, int id, int value)
{
    if ((unsigned)id >= LD_PARAMS || (unsigned)key >= LD_KEYS) return;
    value = op_clamp(value, 0, OP_PMAX);
    if (id == LD_MACRO) k->macro = (int16_t)value; else k->key[key].p[id] = (int16_t)value;
}
int ld_get_param(const ld_kit *k, int key, int id)
{
    if ((unsigned)id >= LD_PARAMS || (unsigned)key >= LD_KEYS) return 0;
    return id == LD_MACRO ? k->macro : k->key[key].p[id];
}
void ld_set_gen(ld_kit *k, int key, int id, int mode, int depth, int rate)
{
    op_gen *g;
    if ((unsigned)id >= LD_PARAMS || (unsigned)key >= LD_KEYS) return;
    g = id == LD_MACRO ? &k->macro_gen : &k->key[key].gen[id];
    g->mode = (uint8_t)op_clamp(mode, 0, OP_GEN_MODES - 1);
    g->depth = (int16_t)op_clamp(depth, -32767, 32767);
    g->rate = (uint8_t)op_clamp(rate, 0, 127);
}
void ld_set_inputs(ld_kit *k, const op_gen_in *in)
{
    k->in.tilt_x = in->tilt_x; k->in.tilt_y = in->tilt_y; k->in.seq = in->seq;
    k->in.clock_valid = in->clock_valid; k->in.step_tick = in->step_tick;
}
void ld_reseed(ld_kit *k, uint32_t seed)
{
    int i, j;
    k->seed = seed;
    op_seed(&k->rng, seed);
    op_gen_state_init(&k->macro_gs, seed ^ 0x5a5a);
    for (i = 0; i < LD_KEYS; i++) {
        for (j = 0; j < LD_PARAMS; j++) op_gen_state_init(&k->key[i].gs[j], seed * 193u + (uint32_t)(i * 31 + j));
        op_seed(&k->v[i].rng[0], seed + (uint32_t)i * 17u + 3u);
        op_seed(&k->v[i].rng[1], seed + (uint32_t)i * 29u + 9u);
    }
}

void ld_default_kit(ld_kit *k)
{
    /* TUNE DECAY PUNCH LEVEL RATIO INDEX FB NOISE DRIVE FILTER SNAP PAN MODEL CHOKE WIDTH MACRO */
    static const int16_t kit[LD_KEYS][LD_PARAMS] = {
 /* KICK        */ { 16384, 17000, 16384, 27000, 16384, 16384, 16384, 16384,  9000, 16384, 12000, 16384, 0 * 4681, 0,     0, 0 },
 /* SUB KICK    */ { 12500, 24000,  9000, 26000, 16384,  9000, 16384, 16384,  4000, 16384,  4000, 16384, 0 * 4681, 0,     0, 0 },
 /* SNARE       */ { 16384, 16384, 16384, 25000, 16384, 16384, 16384, 16384, 10000, 16384, 12000, 16384, 2 * 4681, 0, 12000, 0 },
 /* TIGHT SNARE */ { 19500,  9500, 12000, 24000, 17500, 20000, 16384, 22000, 12000, 18500, 14000, 14500, 2 * 4681, 0, 16000, 0 },
 /* CLAP        */ { 16384, 16384, 16384, 24000, 16384, 16384, 16384, 16384,  9000, 16384,     0, 18500, 3 * 4681, 0, 24000, 0 },
 /* RIM         */ { 16384, 16384, 16384, 22000, 16384, 16384, 16384, 16384,  7000, 16384,  9000, 12500, 4 * 4681, 0,  8000, 0 },
 /* LOW TOM     */ {  9500, 17000, 16384, 25000, 16384, 16384, 16384, 12000,  7000, 16384,  6000, 11000, 1 * 4681, 0,     0, 0 },
 /* MID TOM     */ { 16384, 16384, 16384, 25000, 16384, 16384, 16384, 12000,  7000, 16384,  6000, 16384, 1 * 4681, 0,     0, 0 },
 /* HI TOM      */ { 22500, 15000, 16384, 25000, 16384, 16384, 16384, 12000,  7000, 16384,  6000, 21500, 1 * 4681, 0,     0, 0 },
 /* CONGA       */ { 27500,  9000,  6000, 24000, 18500, 12000, 16384,  9000,  5000, 16384,  8000, 23500, 1 * 4681, 0,     0, 0 },
 /* CLOSED HAT  */ { 16384, 10500, 16384, 20000, 16384, 16384, 16384, 16384,  9000, 16384,  9000, 18500, 6 * 4681, 1, 20000, 0 },
 /* OPEN HAT    */ { 16384, 24500, 16384, 20000, 16384, 16384, 16384, 16384,  9000, 16384,  6000, 14500, 6 * 4681, 1, 20000, 0 },
 /* CYMBAL      */ { 12000, 30500, 16384, 17000, 14000, 16384, 16384, 20000, 11000, 15000,  3000, 16384, 6 * 4681, 2, 28000, 0 },
 /* COWBELL     */ { 16384, 16384, 16384, 24000, 16384, 16384, 16384, 16384,  8000, 16384,  8000, 15000, 5 * 4681, 0,  6000, 0 },
 /* CLAVE       */ { 25000,  5000, 16384, 24000, 20000, 11000, 16384,  8000,  6000, 17000, 10000, 17500, 4 * 4681, 0,     0, 0 },
 /* MARACAS     */ { 16384,  2800, 16384, 19000, 16384, 16384, 16384, 30000,  5000, 23000,  2000, 22000, 6 * 4681, 0, 32000, 0 },
 /* ZAP         */ { 16384, 16384, 16384, 24000, 16384, 16384, 16384, 16384,  9000, 16384,  8000, 16384, 7 * 4681, 0, 10000, 0 },
 /* ZAP 2       */ { 24000, 12000, 28000, 23000, 24000, 20000, 20000, 16384,  9000, 18000,  6000, 10500, 7 * 4681, 0, 18000, 0 },
 /* FM KICK     */ { 15000, 14000, 20000, 26000, 20000, 29000, 22000, 16384, 10000, 16384, 14000, 16384, 0 * 4681, 0,     0, 0 },
 /* FM SNARE    */ { 17500, 13000, 14000, 24000, 22000, 28000, 18000, 20000, 11000, 17000, 12000, 16384, 2 * 4681, 0, 14000, 0 },
 /* METAL       */ { 20000, 14000, 16384, 21000, 16384, 16384, 16384, 16384, 12000, 14000,  5000, 12500, 25745,     0, 30000, 0 },
 /* BELL        */ { 24000, 24000, 16384, 21000, 16384, 16384, 16384, 16384,  6000, 20000,  4000, 20500, 5 * 4681, 0, 22000, 0 },
 /* NOISE       */ { 16384, 11000, 16384, 21000, 16384, 16384, 16384, 16384, 12000, 25000,     0, 16384, 3 * 4681, 3, 32000, 0 },
 /* BLIP        */ { 16384, 16384, 18000, 22000, 16384, 16384, 16384, 16384,  9000, 16384, 10000, 16384, 16383,       0, 12000, 0 },
    };
    int i, j;
    k->macro = 0; k->macro_gen.mode = OP_GEN_OFF; k->macro_gen.rate = 40; k->macro_gen.depth = 0;
    k->swing = 0;
    for (i = 0; i < LD_KEYS; i++) {
        for (j = 0; j < LD_PARAMS; j++) {
            k->key[i].p[j] = kit[i][j];
            k->key[i].gen[j].mode = OP_GEN_OFF; k->key[i].gen[j].rate = 40; k->key[i].gen[j].depth = 0;
        }
        k->key[i].p[LD_MACRO] = 0;
        k->key[i].p[LD_CHOKE] = (int16_t)(kit[i][LD_CHOKE] * 4096 + 2048);     /* zone of 8 */
    }
    /* FM-wandering hats: ratio/index drift per hit (seeded), as in the demo */
    ld_set_gen(k, 10, LD_FM_RATIO, OP_GEN_STEP, 5000, 20);
    ld_set_gen(k, 11, LD_FM_RATIO, OP_GEN_STEP, 5000, 20);
    ld_set_gen(k, 15, LD_NOISE, OP_GEN_STEP, 4000, 20);
    ld_set_gen(k, 20, LD_FM_RATIO, OP_GEN_STEP, 12000, 20);
    ld_set_gen(k, 20, LD_TUNE, OP_GEN_DRIFT, 3000, 30);
}

void ld_init(ld_kit *k, uint32_t seed)
{
    unsigned char *b = (unsigned char *)k;
    int j;
    for (j = 0; j < (int)sizeof(*k); j++) b[j] = 0;
    ld_default_kit(k);
    ld_reseed(k, seed);
}

/* ---------------------------------------------------------------- trigger */
static int32_t eff(ld_kit *k, ld_key *kk, int id, int32_t base, int32_t dt)
{
    op_gen *g = &kk->gen[id];
    if (g->mode == OP_GEN_STEP && !k->in.clock_valid) kk->gs[id].cnt = 0;     /* a new random value every hit */
    return op_gen_apply(base, g, &kk->gs[id], &k->in, dt);
}

void ld_voice_trigger(ld_kit *k, int key, int vel)
{
    ld_key *kk;
    ld_voice *v;
    int32_t e[LD_PARAMS], dt, mac, pos, mi, mf, i;
    ld_arch a;
    int32_t tune_q8, velq;
    if ((unsigned)key >= LD_KEYS) return;
    kk = &k->key[key]; v = &k->v[key];
    dt = (int32_t)(k->time - kk->last_hit); if (dt < 1) dt = 1;
    kk->last_hit = k->time;
    mac = op_gen_apply(k->macro, &k->macro_gen, &k->macro_gs, &k->in, dt);
    for (i = 0; i < LD_PARAMS; i++) {
        int32_t base = kk->p[i];
        if (i == LD_CHOKE) { e[i] = base; continue; }
        if (i == LD_DECAY) base += (mac * 4096) >> 15;
        if (i == LD_DRIVE) base += (mac * 12000) >> 15;
        if (i == LD_FM_INDEX) base += (mac * 8000) >> 15;
        e[i] = eff(k, kk, i, base, dt);
    }

    /* model: morph between adjacent archetypes */
    pos = e[LD_MODEL] * 7; mi = pos >> 15; mf = pos & 32767;
    if (mi >= LD_MODELS - 1) { mi = LD_MODELS - 1; mf = 0; }
    {
        const ld_arch *A = &arch[mi], *B = &arch[mi + (mf ? 1 : 0)];
        a.note = (int16_t)lerpi(A->note, B->note, mf); a.pen = (int16_t)lerpi(A->pen, B->pen, mf);
        a.pen_ms = (int16_t)lerpi(A->pen_ms, B->pen_ms, mf); a.dec_ms = (int16_t)lerpi(A->dec_ms, B->dec_ms, mf);
        a.ndec_ms = (int16_t)lerpi(A->ndec_ms, B->ndec_ms, mf);
        a.wb = (int16_t)lerpi(A->wb, B->wb, mf); a.wn = (int16_t)lerpi(A->wn, B->wn, mf); a.wm = (int16_t)lerpi(A->wm, B->wm, mf);
        a.mod_off = (int16_t)lerpi(A->mod_off, B->mod_off, mf); a.idx = (int16_t)lerpi(A->idx, B->idx, mf);
        a.fm_ms = (int16_t)lerpi(A->fm_ms, B->fm_ms, mf); a.fb = (int16_t)lerpi(A->fb, B->fb, mf);
        a.ncut = (int16_t)lerpi(A->ncut, B->ncut, mf); a.nq = (int16_t)lerpi(A->nq, B->nq, mf);
        a.nmode = (int16_t)lerpi(A->nmode, B->nmode, mf);
        a.bursts = (int16_t)(mf < 16384 ? A->bursts : B->bursts);
        a.click = (int16_t)lerpi(A->click, B->click, mf); a.drive = (int16_t)lerpi(A->drive, B->drive, mf);
        for (i = 0; i < 6; i++) a.ma[i] = (int16_t)lerpi(A->ma[i], B->ma[i], mf);
    }

    /* kill what this hit chokes */
    {
        int grp = zone(e[LD_CHOKE], 8);
        if (grp) for (i = 0; i < LD_KEYS; i++)
            if (i != key && k->v[i].active && zone(k->key[i].p[LD_CHOKE], 8) == grp) {
                int32_t st = (1 << 24) / 5;
                k->v[i].step_b = k->v[i].step_n = k->v[i].step_f = st; k->v[i].choke = 1; k->v[i].bursts = 0; k->v[i].burst_i = 0;
            }
    }

    velq = 8192 + vel * 194; if (velq > 32767) velq = 32767;
    tune_q8 = ((e[LD_TUNE] - 16384) * 3) >> 3;                           /* +-24 semitones */
    v->note0 = a.note * 256 + tune_q8;
    v->pen = (a.pen * 256 * e[LD_PUNCH]) >> 14;                          /* 0 .. 2x archetype */
    v->mod_off = a.mod_off + (((e[LD_FM_RATIO] - 16384) * 3) >> 2);      /* +-12 octaves-ish range -> +-4 oct at ends */
    v->idx = op_clamp((a.idx * e[LD_FM_INDEX]) >> 14, 0, OP_PMAX);
    v->fb = op_clamp((a.fb * e[LD_FM_FB]) >> 14, 0, OP_PMAX);
    {
        int32_t nz = e[LD_NOISE];                                        /* 0 .. 2x */
        v->wb = a.wb;
        v->wn = op_clamp((a.wn * nz) >> 14, 0, 45000);
        v->wm = a.wm;
        v->nfm = ((nz * nz) >> 15) >> 2;                                 /* noise > FM, subtle at centre */
        if (a.wn < 5000 && a.wm == 0) v->nfm = v->nfm >> 1;
    }
    v->click0 = (a.click * e[LD_SNAP]) >> 14;
    v->click = v->click0;
    v->drive = 256 + (((e[LD_DRIVE] + a.drive / 2) * 1024) >> 15);
    v->bias = (a.wb > 20000 && a.wn < 4000) ? 3500 : 0;                  /* even harmonics on tonal drums */
    v->level = (((e[LD_LEVEL] * velq) >> 15) * 21000) >> 15;
    {
        int32_t pn = e[LD_PAN] - 16384;
        v->gl = 32767 - (pn > 0 ? pn * 2 : 0);
        v->gr = 32767 - (pn < 0 ? -pn * 2 : 0);
    }
    v->width = e[LD_WIDTH];

    /* noise / metal filter (Chamberlin SVF) */
    {
        int32_t cn = a.ncut * 256 + (((e[LD_FILTER] - 16384) * 3) >> 3) + (tune_q8 >> 2);
        uint32_t inc = op_note_inc(cn);
        int32_t f = 2 * op_sin_q15(inc >> 1);
        v->svf_f = f > 39000 ? 39000 : f;
        v->svf_q = 65536 - ((a.nq * 63000) >> 15);
        v->svf_mode = a.nmode;
    }
    for (i = 0; i < 6; i++) { v->inc_m[i] = op_note_inc(metal_note[i] + tune_q8); v->ma[i] = a.ma[i]; }

    /* envelopes */
    v->lvl_b = v->lvl_n = v->lvl_p = v->lvl_f = 1 << 24;
    v->step_b = step_of(ticks_ms(a.dec_ms, e[LD_DECAY]));
    v->step_n = step_of(ticks_ms(a.ndec_ms, e[LD_DECAY]));
    v->step_p = step_of(ticks_ms(a.pen_ms, 16384));
    v->step_f = step_of(ticks_ms(a.fm_ms, e[LD_DECAY]));
    v->bursts = a.bursts; v->burst_i = 0; v->gt = 0;
    {
        uint32_t gap = (op_sr() * 10) / (1000 * OP_SUB);                  /* 10 ms between claps */
        v->gap_ticks = (int32_t)(gap ? gap : 1);
        v->inv_gap = 32768 / v->gap_ticks;
    }
    v->eb_prev = v->en_prev = 0;
    v->choke = 0;
    v->p1 = v->p2 = op_rand(&v->rng[0]) & 0xff000000u;
    for (i = 0; i < 6; i++) v->pm[i] = (uint32_t)i * 0x2aaaaaaau;
    v->s_bp[0] = v->s_bp[1] = v->s_lp[0] = v->s_lp[1] = 0; v->y2 = 0;
    v->age = ++k->age;
    v->active = 1;
}
void ld_trigger(ld_kit *k, int key, int vel) { ld_voice_trigger(k, key, vel); }

/* ---------------------------------------------------------------- render */
static inline int32_t svf_tick(ld_voice *v, int c, int32_t in, int32_t *bp_out)
{
    int32_t hp, bp = v->s_bp[c], lp = v->s_lp[c];
    hp = in - lp - (((bp >> 1) * v->svf_q) >> 14);
    hp = op_clamp(hp, -65535, 65535);
    bp = op_clamp(bp + (((hp >> 1) * v->svf_f) >> 14), -65535, 65535);
    lp = op_clamp(lp + (((bp >> 1) * v->svf_f) >> 14), -65535, 65535);
    v->s_bp[c] = bp; v->s_lp[c] = lp;
    *bp_out = (bp * (v->svf_q >> 1)) >> 15;                 /* damping-normalised band-pass */
    return hp;
}

static void voice_sub(ld_voice *v, int32_t *L, int32_t *R, int n)
{
    int32_t eb, en, pe, fe, i, n1, ebd, end_, idx, ebr, enr;
    uint32_t inc1, inc2, p1 = v->p1, p2 = v->p2;
    int stereo = v->width > 0;

    /* envelopes at tick rate: squared decays, cubic pitch fall, clap bursts on the noise path */
    v->lvl_b -= v->step_b; if (v->lvl_b < 0) v->lvl_b = 0;
    v->lvl_p -= v->step_p; if (v->lvl_p < 0) v->lvl_p = 0;
    v->lvl_f -= v->step_f; if (v->lvl_f < 0) v->lvl_f = 0;
    eb = sq15(v->lvl_b);
    pe = cu15(v->lvl_p);
    fe = sq15(v->lvl_f);
    if (v->burst_i < v->bursts) {
        int32_t ph = 32767 - v->gt * v->inv_gap;
        if (ph < 0) ph = 0;
        en = (ph * ph) >> 15;
        if (++v->gt >= v->gap_ticks) { v->gt = 0; v->burst_i++; }
    } else {
        v->lvl_n -= v->step_n; if (v->lvl_n < 0) v->lvl_n = 0;
        en = sq15(v->lvl_n);
    }
    v->click = (v->click * 5) >> 3;

    n1 = v->note0 + ((v->pen * pe) >> 15);
    inc1 = op_note_inc(n1);
    inc2 = op_note_inc(n1 + v->mod_off);
    ebd = n == OP_SUB ? eb - v->eb_prev : ((eb - v->eb_prev) * 16) / n;
    end_ = n == OP_SUB ? en - v->en_prev : ((en - v->en_prev) * 16) / n;
    ebr = v->eb_prev << 4; enr = v->en_prev << 4;
    idx = (v->idx * fe) >> 15;
    {
        const int tone = (v->wn | v->wm) != 0;
        const int32_t fb = v->fb, nfm = v->nfm, wb = v->wb, click = v->click;
        for (i = 0; i < n; i++) {
            int32_t nzl, y2, yb, cmn, oL, oR, mid = 0, sd = 0;
            uint32_t pm;
            nzl = op_rand_bi(&v->rng[0]);
            p2 += inc2;
            y2 = op_sin_q15(p2 + (fb ? (uint32_t)((v->y2 * fb) >> 15) * 65536u : 0u));
            v->y2 = y2;
            p1 += inc1;
            pm = (uint32_t)(((y2 * idx) >> 15) * 4) * 131072u + (uint32_t)((nzl * nfm) >> 15) * 131072u;
            yb = op_sin_q15(p1 + pm);
            cmn = (yb * ((wb * (ebr >> 4)) >> 15)) >> 15;
            if (click) cmn += (nzl * click) >> 15;
            if (tone) {
                int32_t mtl = 0, mtr = 0, tl, bpl, hl;
                if (v->wm) {
                    int m;
                    for (m = 0; m < 6; m++) {
                        uint32_t ph;
                        if (!v->ma[m]) continue;
                        v->pm[m] += v->inc_m[m];
                        ph = v->pm[m];
                        mtl += ph < 0x80000000u ? v->ma[m] : -v->ma[m];
                        if (stereo) mtr += (ph + 0x40000000u) < 0x80000000u ? v->ma[m] : -v->ma[m];
                    }
                }
                tl = ((nzl * v->wn) >> 15) + ((mtl * v->wm) >> 15);
                tl = (tl * (enr >> 4)) >> 15;
                hl = svf_tick(v, 0, tl, &bpl);
                hl = bpl + (((hl - bpl) * v->svf_mode) >> 15);
                if (stereo) {
                    int32_t nzr = op_rand_bi(&v->rng[1]), tr, bpr, hr;
                    tr = ((nzr * v->wn) >> 15) + ((mtr * v->wm) >> 15);
                    tr = (tr * (enr >> 4)) >> 15;
                    hr = svf_tick(v, 1, tr, &bpr);
                    hr = bpr + (((hr - bpr) * v->svf_mode) >> 15);
                    mid = (hl + hr) >> 1;
                    sd = (((hl - hr) >> 1) * v->width) >> 15;
                } else mid = hl;
            }
            oL = op_drive_asym(cmn + mid + sd, v->drive, v->bias);
            oR = sd ? op_drive_asym(cmn + mid - sd, v->drive, v->bias) : oL;
            L[i] += (((oL * v->level) >> 15) * v->gl) >> 15;
            R[i] += (((oR * v->level) >> 15) * v->gr) >> 15;
            ebr += ebd; enr += end_;
        }
    }
    v->p1 = p1; v->p2 = p2; v->eb_prev = eb; v->en_prev = en;
    if (v->lvl_b == 0 && v->lvl_n == 0 && v->burst_i >= v->bursts && eb == 0 && en == 0) v->active = 0;
}

void ld_voice_render(ld_kit *k, ld_voice *v, int32_t *L, int32_t *R, int n)
{
    (void)k;
    while (n > 0 && v->active) {
        int m = n < OP_SUB ? n : OP_SUB;
        voice_sub(v, L, R, m);
        L += m; R += m; n -= m;
    }
}

int32_t ld_master(int32_t x) { return op_drive(x, 256); }

void ld_render(ld_kit *k, int32_t *L, int32_t *R, int n)
{
    int i;
    for (i = 0; i < n; i++) L[i] = R[i] = 0;
    for (i = 0; i < LD_KEYS; i++)
        if (k->v[i].active) ld_voice_render(k, &k->v[i], L, R, n);
    for (i = 0; i < n; i++) {
        L[i] = op_clamp(op_dc_tick(&k->dc[0], ld_master(L[i])), -32767, 32767);
        R[i] = op_clamp(op_dc_tick(&k->dc[1], ld_master(R[i])), -32767, 32767);
    }
    k->time += (uint32_t)n;
}

int ld_active_voices(const ld_kit *k)
{
    int i, n = 0;
    for (i = 0; i < LD_KEYS; i++) n += k->v[i].active;
    return n;
}

#include "op_common.h"
#include "op_tables.h"

static uint32_t g_sr = 44100;
static int32_t g_cref_q8 = 35012;       /* note (Q8) whose frequency is sr/2 */

void op_init(uint32_t sr)
{
    /* 256 * (69 + 12*log2(sr/2/440)), precomputed */
    switch (sr) {
    case 48000: g_cref_q8 = 35388; g_sr = 48000; break;
    case 88200: g_cref_q8 = 38084; g_sr = 88200; break;
    case 96000: g_cref_q8 = 38460; g_sr = 96000; break;
    default:    g_cref_q8 = 35012; g_sr = 44100; break;
    }
}
uint32_t op_sr(void) { return g_sr; }

void op_seed(op_prng *p, uint32_t seed)
{
    uint32_t x = seed * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
    p->s = x ? x : 0x9E3779B9u;
    (void)op_rand(p);
}

int32_t op_sin_q15(uint32_t ph)
{
    uint32_t i = ph >> 24;
    int32_t f = (int32_t)((ph >> 16) & 255), a = op_sin_tab[i], b = op_sin_tab[i + 1];
    return a + (((b - a) * f) >> 8);
}

uint32_t op_exp2_q8(uint32_t x)
{
    uint32_t io = x >> 16, idx = (x >> 8) & 255, f = x & 255;
    uint32_t a = op_exp2_tab[idx], b = op_exp2_tab[idx + 1];
    uint32_t m = a + (((b - a) * f) >> 8);          /* 32768..65536 */
    if (io > 20) io = 20;
    return (m << io) >> 7;
}

uint32_t op_note_inc(int32_t note_q8)
{
    int32_t e = note_q8 - g_cref_q8;
    int32_t oct = (e * 21845) >> 10;                /* e/3072 octaves in Q16 */
    int32_t io = oct >> 16;
    uint32_t fr = (uint32_t)oct & 0xffffu, idx = fr >> 8, f = fr & 255;
    uint32_t a = op_exp2_tab[idx], b = op_exp2_tab[idx + 1];
    uint32_t m = a + (((b - a) * f) >> 8);
    uint32_t base;
    if (io >= 0) return OP_INC_MAX;
    if (io < -31) return 0;
    base = (m << 16) >> 0;                          /* m<=65536: 65536<<16 overflows -> only at exact top */
    if (m >= 65536u) base = 0xffffffffu;
    base >>= (uint32_t)(-io);
    return base > OP_INC_MAX ? OP_INC_MAX : base;
}

int32_t op_g_from_note(int32_t c_q8)
{
    int32_t r = g_cref_q8 - c_q8, i, f, a, b;
    if (r < 0) r = 0;
    i = r >> 5; f = r & 31;
    if (i >= OP_G_TAB_N - 1) return op_g_tab[OP_G_TAB_N - 1];
    a = op_g_tab[i]; b = op_g_tab[i + 1];
    return a + (((b - a) * f) >> 5);
}

uint32_t op_frac_q15(uint32_t num, uint32_t den)
{
    while (den >= (1u << 16)) { den >>= 1; num >>= 1; }
    if (den == 0 || num >= den) return num >= den && den ? 32767u : 0;
    return (num << 15) / den;
}

/* ------------------------------------------------------------------------------- ADSR */
static int32_t ticks_of(int p, int div)
{
    /* 1 ms .. ~10 s, exponential */
    uint32_t m = op_exp2_q8((uint32_t)op_clamp(p, 0, OP_PMAX) * 26u);   /* 256 * T_ms */
    uint64_t t = ((uint64_t)m * g_sr) / (256000ull * (uint32_t)div);
    return t < 1 ? 1 : (int32_t)t;
}
void op_adsr_cfg_set(op_adsr_cfg *c, int a, int d, int s, int r)
{
    c->a_step = (1 << 24) / ticks_of(a, OP_SUB);
    c->d_step = (1 << 24) / ticks_of(d, OP_SUB);
    c->r_step = (1 << 24) / ticks_of(r, OP_SUB);
    c->sus = op_clamp(s, 0, OP_PMAX);
}
void op_adsr_on(op_adsr *e)
{
    e->lvl = e->out << 9; if (e->lvl > (1 << 24)) e->lvl = 1 << 24;   /* retrigger from where we are */
    e->stage = OP_ENV_ATT;
}
void op_adsr_off(op_adsr *e)
{
    if (e->stage == OP_ENV_IDLE) return;
    e->stage = OP_ENV_REL; e->rl = 1 << 24; e->rel_from = e->out;
}
static inline int32_t sq15(int32_t l24) { int32_t x = l24 >> 9; return (x * x) >> 15; }
int32_t op_adsr_tick(op_adsr *e, const op_adsr_cfg *c)
{
    switch (e->stage) {
    case OP_ENV_ATT:
        e->lvl += c->a_step;
        if (e->lvl >= (1 << 24)) { e->lvl = 1 << 24; e->stage = OP_ENV_DEC; e->dl = 1 << 24; }
        e->out = e->lvl >> 9;
        break;
    case OP_ENV_DEC:
        e->dl -= c->d_step;
        if (e->dl <= 0) { e->dl = 0; e->stage = OP_ENV_SUS; }
        e->out = c->sus + (((OP_PMAX - c->sus) * sq15(e->dl)) >> 15);
        break;
    case OP_ENV_SUS:
        e->out = c->sus;
        break;
    case OP_ENV_REL:
        e->rl -= c->r_step;
        if (e->rl <= 0) { e->rl = 0; e->stage = OP_ENV_IDLE; e->out = 0; }
        else e->out = (e->rel_from * sq15(e->rl)) >> 15;
        break;
    default:
        e->out = 0;
        break;
    }
    if (e->out > OP_PMAX) e->out = OP_PMAX;
    if (e->out < 0) e->out = 0;
    return e->out;
}

/* ------------------------------------------------------------------------------- LFO */
void op_lfo_init(op_lfo *l, uint32_t seed)
{
    op_seed(&l->r, seed);
    l->ph = op_rand(&l->r); l->held = op_rand_bi(&l->r); l->cur = l->held;
}
int op_lfo_shape_of(int p)
{
    int z = (op_clamp(p, 0, OP_PMAX) * OP_LFO_SHAPES) >> 15;
    return z >= OP_LFO_SHAPES ? OP_LFO_SHAPES - 1 : z;
}
uint32_t op_lfo_inc(int rate_p, int sync_p, int32_t bpm_q8)
{
    uint64_t inc;
    uint32_t ctl = g_sr / OP_SUB;
    rate_p = op_clamp(rate_p, 0, OP_PMAX);
    if (sync_p > 16384) {       /* synced: 1/4 .. 16 cycles per beat */
        static const uint8_t mult[7] = { 1, 2, 4, 8, 16, 32, 64 };           /* x1/4 */
        int z = (rate_p * 7) >> 15; if (z > 6) z = 6;
        /* hz = bpm/60 * mult/4 ; inc = hz * 2^32 / ctl */
        inc = ((uint64_t)(uint32_t)(bpm_q8 < 0 ? 0 : bpm_q8) * mult[z] << 32) / (60ull * 256 * 4 * ctl);
    } else {                    /* free: 0.05 .. 51 Hz */
        uint32_t m = op_exp2_q8((uint32_t)rate_p * 20u);
        inc = ((uint64_t)m * 838861u) / ctl;
    }
    return inc > 0x7fffffffu ? 0x7fffffffu : (uint32_t)inc;
}
int32_t op_lfo_tick(op_lfo *l, int shape, uint32_t inc)
{
    uint32_t old = l->ph;
    int32_t u, y;
    l->ph += inc;
    if (l->ph < old) l->held = op_rand_bi(&l->r);
    u = (int32_t)(l->ph >> 16);                     /* 0..65535 */
    switch (shape) {
    case OP_LFO_SINE: y = op_sin_q15(l->ph); break;
    case OP_LFO_TRI:  y = u < 32768 ? u * 2 - 32768 : 98303 - u * 2; break;
    case OP_LFO_SAW:  y = u - 32768; break;
    case OP_LFO_SQR:  y = u < 32768 ? 32767 : -32767; break;
    case OP_LFO_SH:   y = l->held; break;
    default:          l->cur += (l->held - l->cur) >> 3; y = l->cur; break;
    }
    return op_clamp(y, -32767, 32767);
}

/* ------------------------------------------------------------------------------- GEN */
const char *const op_gen_names[OP_GEN_MODES] = { "OFF", "DRIFT", "STEP", "LFO1", "LFO2", "TILT X", "TILT Y", "SEQ" };

void op_gen_state_init(op_gen_state *s, uint32_t seed)
{
    op_seed(&s->rng, seed);
    s->cur = op_rand_bi(&s->rng); s->held = s->cur; s->slope_q8 = 0; s->cnt = 0; s->last_tick = 0;
}
static int32_t period_of(int rate)      /* samples: ~8 s (rate 0) .. ~40 ms (127) */
{
    uint32_t m = op_exp2_q8((uint32_t)op_clamp(rate, 0, 127) * 3943u);   /* 256 .. ~50000 */
    uint64_t p = ((uint64_t)g_sr * 8u * 256u) / m;
    return p < 1 ? 1 : (int32_t)p;
}
int32_t op_gen_eval(const op_gen *g, op_gen_state *s, const op_gen_in *in, int32_t n)
{
    int32_t v;
    switch (g->mode) {
    case OP_GEN_DRIFT:
        s->cnt -= n;
        while (s->cnt <= 0) {
            int32_t per = period_of(g->rate), tgt = op_rand_bi(&s->rng);
            s->cur = s->held; s->held = tgt;
            s->slope_q8 = (int32_t)(((int64_t)(tgt - s->cur) * 256) / per);
            s->cnt += per;
        }
        s->cur += (s->slope_q8 * n) >> 8;
        s->cur = op_clamp(s->cur, -32767, 32767);
        return s->cur;
    case OP_GEN_STEP:
        if (in && in->clock_valid) {
            if (in->step_tick != s->last_tick) { s->last_tick = in->step_tick; s->held = op_rand_bi(&s->rng); }
        } else {
            s->cnt -= n;
            while (s->cnt <= 0) { s->held = op_rand_bi(&s->rng); s->cnt += period_of(g->rate); }
        }
        return op_clamp(s->held, -32767, 32767);
    case OP_GEN_LFO1: v = in ? in->lfo1 : 0; break;
    case OP_GEN_LFO2: v = in ? in->lfo2 : 0; break;
    case OP_GEN_TILTX: v = in ? in->tilt_x : 0; break;
    case OP_GEN_TILTY: v = in ? in->tilt_y : 0; break;
    case OP_GEN_SEQ: v = in ? in->seq : 0; break;
    default: return 0;
    }
    return op_clamp(v, -32767, 32767);
}
int32_t op_gen_apply(int32_t base, const op_gen *g, op_gen_state *s, const op_gen_in *in, int32_t n)
{
    if (g->mode == OP_GEN_OFF) return op_clamp(base, 0, OP_PMAX);
    return op_clamp(base + ((op_gen_eval(g, s, in, n) * g->depth) >> 15), 0, OP_PMAX);
}

/* inverse of op_note_inc: log2 of inc/2^31 via the exp2 table (binary search), 3072 = 1 octave */
int32_t op_inc_to_note(uint32_t inc)
{
    int32_t io = -1, lo = 0, hi = 256, m;
    uint32_t mant;
    if (inc == 0) return g_cref_q8 - 31 * 3072;
    while (io > -32 && inc < (0x80000000u >> (uint32_t)(-io))) io--;      /* inc >= 2^(31+io) */
    mant = (io >= 0 ? inc : inc << (uint32_t)(-io)) >> 16;                   /* Q15 mantissa 32768..65535 */
    while (hi - lo > 1) { m = (lo + hi) >> 1; if (op_exp2_tab[m] <= mant) lo = m; else hi = m; }
    {
        uint32_t dd = op_exp2_tab[lo + 1] - op_exp2_tab[lo];
        return g_cref_q8 + io * 3072 + lo * 12 + (int32_t)(((mant - op_exp2_tab[lo]) * 12u) / dd);
    }
}

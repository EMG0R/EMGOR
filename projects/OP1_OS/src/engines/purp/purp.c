/* THE PURP core. See purp.h. Fixed point, no libc, no divides in the sample loop. */
#include "purp.h"

const pu_param_info pu_info[PU_PARAMS] = {
    {"SHAPE A", 0}, {"SHAPE B", 0}, {"DETUNE", 0}, {"NOISE", 0},
    {"RATIO", 1}, {"INDEX", 1}, {"FEEDBK", 1}, {"NOISE>FM", 1},
    {"CUTOFF", 2}, {"RES", 2}, {"DRIVE", 2}, {"ENV AMT", 2},
    {"ATTACK", 3}, {"DECAY", 3}, {"SUSTAIN", 3}, {"RELEASE", 3},
    {"ATTACK", 4}, {"DECAY", 4}, {"SUSTAIN", 4}, {"RELEASE", 4},
    {"RATE", 5}, {"SHAPE", 5}, {"DEPTH", 5}, {"SYNC", 5},
    {"RATE", 6}, {"SHAPE", 6}, {"DEPTH", 6}, {"SYNC", 6},
    {"SOURCE", 7}, {"DEST", 7}, {"AMOUNT", 7}, {"CURVE", 7},
    {"VOICES", 8}, {"GLIDE", 8}, {"DRIFT", 8}, {"MACRO", 8},
};
const char *const pu_subpage_names[PU_SUBPAGES] = {
    "OSC", "FM", "FILTER", "AMP ENV", "MOD ENV", "LFO 1", "LFO 2", "MATRIX", "VOICE" };
const char *const pu_mx_src_names[MXS_COUNT] = {
    "-", "LFO1", "LFO2", "MODENV", "AMPENV", "VEL", "KEY", "TILT X", "TILT Y", "NOISE", "DRIFT" };
const char *const pu_mx_dst_names[MXD_COUNT] = {
    "-", "PITCH", "SHAPE A", "SHAPE B", "DETUNE", "NOISE", "RATIO", "INDEX", "FEEDBK", "NOISE>FM",
    "CUTOFF", "RES", "DRIVE", "AMP" };

static const int16_t ratio_semis_q8[16] = {
    0, 3072, 4869, 6144, 7133, 7941, 8624, 9216, 9738, 10205, 10627, 11013, 11368, 11696, 12002, 12288 };

static inline int zone(int p, int n)
{
    int z = (op_clamp(p, 0, OP_PMAX) * n) >> 15;
    return z >= n ? n - 1 : z;
}

/* ---------------------------------------------------------------- presets */
#define PC(x) ((int16_t)((x) * 32767 / 100))
typedef struct {
    int16_t p[PU_PARAMS];
    struct { uint8_t id, mode, rate; int16_t depth; } gen[3];
    int16_t mx[PU_MX_SLOTS][4];
} pu_preset_t;
#define NG { 0, 0, 0, 0 }
#define NM { 0, 0, 16384, 0 }
const char *const pu_preset_names[PU_PRESETS] = {
    "ANALOG PAD", "GROWL FM", "NOISE FM", "SUB BASS", "RES LEAD", "FM BELL", "DRIFT STRINGS", "PURP MACRO" };
static const pu_preset_t presets[PU_PRESETS] = {
 { /* ANALOG PAD */ { PC(2), PC(8), PC(42), PC(4),  PC(50), PC(0), PC(0), PC(0),
     PC(44), PC(22), PC(28), PC(30),  PC(52), PC(60), PC(80), PC(68),  PC(40), PC(60), PC(40), PC(50),
     PC(20), PC(0), PC(30), PC(0),  PC(10), PC(50), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(100), PC(0), PC(55), PC(0) },
   { { PU_CUTOFF, OP_GEN_LFO1, 0, 4000 }, NG, NG }, { { MXS_LFO1, MXD_CUTOFF, 16384 + 4000, 0 }, NM, NM, NM } },
 { /* GROWL FM */ { PC(55), PC(10), PC(12), PC(3),  PC(41), PC(55), PC(30), PC(5),
     PC(56), PC(35), PC(60), PC(40),  PC(2), PC(40), PC(60), PC(20),  PC(2), PC(35), PC(20), PC(30),
     PC(30), PC(0), PC(40), PC(0),  PC(14), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(60), PC(8), PC(35), PC(0) },
   { { PU_FM_INDEX, OP_GEN_LFO1, 0, 7000 }, NG, NG }, { NM, NM, NM, NM } },
 { /* NOISE FM */ { PC(30), PC(0), PC(20), PC(15),  PC(60), PC(30), PC(10), PC(70),
     PC(60), PC(30), PC(55), PC(35),  PC(0), PC(30), PC(30), PC(25),  PC(0), PC(25), PC(10), PC(25),
     PC(40), PC(5), PC(0), PC(0),  PC(40), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(60), PC(0), PC(40), PC(0) },
   { { PU_NOISE_FM, OP_GEN_DRIFT, 40, 6000 }, NG, NG }, { NM, NM, NM, NM } },
 { /* SUB BASS */ { PC(80), PC(50), PC(5), PC(0),  PC(33), PC(20), PC(10), PC(0),
     PC(30), PC(20), PC(35), PC(35),  PC(0), PC(35), PC(70), PC(20),  PC(0), PC(30), PC(10), PC(20),
     PC(10), PC(0), PC(0), PC(0),  PC(10), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(40), PC(15), PC(20), PC(0) },
   { NG, NG, NG }, { NM, NM, NM, NM } },
 { /* RES LEAD */ { PC(1), PC(3), PC(22), PC(0),  PC(50), PC(8), PC(0), PC(0),
     PC(35), PC(78), PC(45), PC(70),  PC(0), PC(30), PC(70), PC(25),  PC(0), PC(45), PC(10), PC(35),
     PC(35), PC(0), PC(0), PC(0),  PC(10), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(40), PC(12), PC(30), PC(0) },
   { { PU_CUTOFF, OP_GEN_DRIFT, 30, 8000 }, NG, NG }, { NM, NM, NM, NM } },
 { /* FM BELL */ { PC(70), PC(70), PC(15), PC(0),  PC(62), PC(42), PC(0), PC(0),
     PC(75), PC(10), PC(10), PC(0),  PC(0), PC(62), PC(0), PC(55),  PC(0), PC(45), PC(0), PC(30),
     PC(20), PC(0), PC(0), PC(0),  PC(10), PC(0), PC(0), PC(0),  PC(1), PC(1), PC(50), PC(0),
     PC(100), PC(0), PC(30), PC(0) },
   { { PU_FM_RATIO, OP_GEN_STEP, 20, 3000 }, NG, NG }, { { MXS_MODENV, MXD_FM_INDEX, 16384 + 9000, 0 }, NM, NM, NM } },
 { /* DRIFT STRINGS */ { PC(0), PC(0), PC(55), PC(6),  PC(50), PC(0), PC(0), PC(0),
     PC(50), PC(15), PC(25), PC(20),  PC(40), PC(55), PC(85), PC(70),  PC(30), PC(60), PC(50), PC(40),
     PC(15), PC(0), PC(10), PC(0),  PC(10), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(100), PC(5), PC(90), PC(0) },
   { { PU_SHAPE_B, OP_GEN_DRIFT, 12, 9000 }, { PU_CUTOFF, OP_GEN_DRIFT, 18, 6000 }, NG },
   { { MXS_LFO1, MXD_PITCH, 16384 + 500, 0 }, NM, NM, NM } },
 { /* PURP MACRO */ { PC(5), PC(20), PC(10), PC(0),  PC(41), PC(10), PC(5), PC(0),
     PC(35), PC(30), PC(40), PC(40),  PC(1), PC(40), PC(65), PC(30),  PC(1), PC(40), PC(30), PC(30),
     PC(30), PC(0), PC(0), PC(0),  PC(10), PC(0), PC(0), PC(0),  PC(0), PC(0), PC(50), PC(0),
     PC(80), PC(5), PC(40), PC(35) },
   { { PU_MACRO, OP_GEN_LFO1, 0, 14000 }, NG, NG }, { NM, NM, NM, NM } },
};

void pu_set_param(pu_synth *s, int id, int value)
{
    if ((unsigned)id < PU_PARAMS) s->p[id] = (int16_t)op_clamp(value, 0, OP_PMAX);
}
void pu_set_gen(pu_synth *s, int id, int mode, int depth, int rate)
{
    if ((unsigned)id >= PU_PARAMS) return;
    s->gen[id].mode = (uint8_t)op_clamp(mode, 0, OP_GEN_MODES - 1);
    s->gen[id].depth = (int16_t)op_clamp(depth, -32767, 32767);
    s->gen[id].rate = (uint8_t)op_clamp(rate, 0, 127);
}
void pu_set_matrix(pu_synth *s, int slot, int src, int dst, int amt, int curve)
{
    if ((unsigned)slot >= PU_MX_SLOTS) return;
    s->mx[slot].src = (int16_t)op_clamp(src, 0, OP_PMAX);
    s->mx[slot].dst = (int16_t)op_clamp(dst, 0, OP_PMAX);
    s->mx[slot].amt = (int16_t)op_clamp(amt, 0, OP_PMAX);
    s->mx[slot].curve = (int16_t)op_clamp(curve, 0, OP_PMAX);
}
void pu_set_snap(pu_synth *s, int on) { s->snap = on ? 1 : 0; }
void pu_set_wavetable(pu_synth *s, const int16_t *t, int len) { s->wavetable = t; s->wavetable_len = len; }
void pu_set_inputs(pu_synth *s, const op_gen_in *in, int32_t bpm_q8)
{
    s->in.tilt_x = in->tilt_x; s->in.tilt_y = in->tilt_y; s->in.seq = in->seq;
    s->in.clock_valid = in->clock_valid; s->in.step_tick = in->step_tick;
    s->bpm_q8 = bpm_q8;
}

void pu_reseed(pu_synth *s, uint32_t seed)
{
    int i;
    s->seed = seed;
    op_seed(&s->rng, seed);
    for (i = 0; i < PU_PARAMS; i++) op_gen_state_init(&s->gs[i], seed * 131u + (uint32_t)i);
    op_lfo_init(&s->lfo[0], seed + 1001u);
    op_lfo_init(&s->lfo[1], seed + 2002u);
    for (i = 0; i < PU_MAX_VOICES; i++) op_seed(&s->v[i].rng, seed * 977u + (uint32_t)i + 5u);
}

void pu_preset(pu_synth *s, int n)
{
    const pu_preset_t *q = &presets[op_clamp(n, 0, PU_PRESETS - 1)];
    int i;
    for (i = 0; i < PU_PARAMS; i++) { s->p[i] = q->p[i]; s->gen[i].mode = OP_GEN_OFF; s->gen[i].rate = 40; s->gen[i].depth = 0; }
    for (i = 0; i < 3; i++)
        if (q->gen[i].mode) {
            int id = q->gen[i].id;
            s->gen[id].mode = q->gen[i].mode; s->gen[id].rate = q->gen[i].rate; s->gen[id].depth = q->gen[i].depth;
        }
    for (i = 0; i < PU_MX_SLOTS; i++) {
        s->mx[i].src = q->mx[i][0]; s->mx[i].dst = q->mx[i][1];
        s->mx[i].amt = q->mx[i][2]; s->mx[i].curve = q->mx[i][3];
    }
    /* mx[] rows store a zone index; convert to centre-of-zone params */
    for (i = 0; i < PU_MX_SLOTS; i++) {
        if (s->mx[i].src < MXS_COUNT) s->mx[i].src = (int16_t)(((int)s->mx[i].src * 2 + 1) * 32768 / (2 * MXS_COUNT));
        if (s->mx[i].dst < MXD_COUNT) s->mx[i].dst = (int16_t)(((int)s->mx[i].dst * 2 + 1) * 32768 / (2 * MXD_COUNT));
    }
    s->snap = (n == 3 || n == 5) ? 1 : 0;
    pu_reseed(s, s->seed + (uint32_t)n * 7u);
}

void pu_init(pu_synth *s, uint32_t seed)
{
    int i, j;
    unsigned char *b = (unsigned char *)s;
    for (j = 0; j < (int)sizeof(*s); j++) b[j] = 0;
    s->seed = seed;
    s->bpm_q8 = 120 * 256;
    s->glide_key = -1;
    s->lfo_key[0][0] = s->lfo_key[1][0] = -1;
    for (i = 0; i < PU_MX_SLOTS; i++) { s->mx[i].amt = 16384; }
    pu_preset(s, 0);
    pu_reseed(s, seed);
    for (i = 0; i < 4; i++) { s->acfg_key[i] = -1; s->mcfg_key[i] = -1; }
}

/* ---------------------------------------------------------------- global (block-rate) update */
static void key_cfg(op_adsr_cfg *c, int16_t *key, const int32_t *e, int a)
{
    if (key[0] != e[a] || key[1] != e[a + 1] || key[2] != e[a + 2] || key[3] != e[a + 3]) {
        key[0] = (int16_t)e[a]; key[1] = (int16_t)e[a + 1]; key[2] = (int16_t)e[a + 2]; key[3] = (int16_t)e[a + 3];
        op_adsr_cfg_set(c, e[a], e[a + 1], e[a + 2], e[a + 3]);
    }
}

void pu_global_update(pu_synth *s, int n)
{
    int i, ticks = n / OP_SUB;
    int32_t m;
    int32_t b[PU_PARAMS];
    if (ticks < 1) ticks = 1;

    /* LFOs first: GEN modes LFO1/LFO2 read them */
    for (i = 0; i < 2; i++) {
        int base = PU_L1_RATE + i * 4;
        int32_t v = 0, dep = op_gen_apply(s->p[base + 2], &s->gen[base + 2], &s->gs[base + 2], &s->in, n);
        int32_t rate = op_gen_apply(s->p[base], &s->gen[base], &s->gs[base], &s->in, n);
        uint32_t inc;
        int shape = op_lfo_shape_of(s->p[base + 1]), t;
        if (s->lfo_key[i][0] != rate || s->lfo_key[i][1] != s->p[base + 3] || s->lfo_key[i][2] != s->bpm_q8) {
            s->lfo_key[i][0] = rate; s->lfo_key[i][1] = s->p[base + 3]; s->lfo_key[i][2] = s->bpm_q8;
            s->lfo_inc[i] = op_lfo_inc(rate, s->p[base + 3], s->bpm_q8);
        }
        inc = s->lfo_inc[i];
        for (t = 0; t < ticks; t++) v = op_lfo_tick(&s->lfo[i], shape, inc);
        s->lfo_val[i] = (v * dep) >> 15;
        s->eff[base] = rate; s->eff[base + 1] = s->p[base + 1]; s->eff[base + 2] = dep; s->eff[base + 3] = s->p[base + 3];
    }
    s->in.lfo1 = (int16_t)s->lfo_val[0];
    s->in.lfo2 = (int16_t)s->lfo_val[1];

    /* macro: FM index + detune + noise + cutoff */
    m = op_gen_apply(s->p[PU_MACRO], &s->gen[PU_MACRO], &s->gs[PU_MACRO], &s->in, n);
    s->eff[PU_MACRO] = m;
    for (i = 0; i < PU_PARAMS; i++) b[i] = s->p[i];
    b[PU_FM_INDEX] += (m * 19660) >> 15;
    b[PU_DETUNE] += (m * 16384) >> 15;
    b[PU_NOISE_MIX] += (m * 8192) >> 15;
    b[PU_CUTOFF] += (m * 11468) >> 15;
    for (i = 0; i < PU_PARAMS; i++) {
        if (i == PU_MACRO || (i >= PU_L1_RATE && i <= PU_L2_SYNC)) continue;
        s->eff[i] = op_gen_apply(b[i], &s->gen[i], &s->gs[i], &s->in, n);
    }
    key_cfg(&s->acfg, s->acfg_key, s->eff, PU_A_ATT);
    key_cfg(&s->mcfg, s->mcfg_key, s->eff, PU_M_ATT);

    if (s->glide_key != (int16_t)s->eff[PU_GLIDE]) {
        int g = s->eff[PU_GLIDE];
        s->glide_key = (int16_t)g;
        if (g < 400) s->glide_k = 32768;
        else {
            uint32_t mm = op_exp2_q8((uint32_t)g * 22u);           /* ~1 ms .. 6 s */
            uint32_t ticks_g = (uint32_t)(((uint64_t)mm * op_sr()) / (256000ull * OP_SUB));
            s->glide_k = ticks_g < 2 ? 32768 : (int32_t)(32768 / ticks_g) + 1;
        }
    }
}

/* ---------------------------------------------------------------- oscillator */
/* shape morph: zone 0 = saw -> square, zone 1 = square -> triangle; t = position in the zone (Q15).
 * ws/wq/wt are the equivalent per-wave weights, used for the band-limiting corrections. */
typedef struct { int32_t zone, t, ws, wq, wt; } pu_w;

static void shape_w(int s, pu_w *w)
{
    s = op_clamp(s, 0, OP_PMAX);
    if (s < 16384) { w->zone = 0; w->t = s * 2; w->ws = 32767 - w->t; w->wq = w->t; w->wt = 0; }
    else           { w->zone = 1; w->t = (s - 16384) * 2; w->ws = 0; w->wq = 32767 - w->t; w->wt = w->t; }
}

/* Slow path, once per wave edge: polyBLEP residual for the steps and polyBLAMP for the triangle
 * kinks, applied to the previous (delayed) sample and the current one. pv/pe: phase before/after,
 * the two edges of the cycle are phase 0 and phase 0.5; works for either phase direction. */
static void osc_edge(uint32_t pv, uint32_t pe, const pu_w *w, int32_t *cp, int32_t *cn)
{
    int32_t d = (int32_t)(pe - pv);
    uint32_t ad = d > 0 ? (uint32_t)d : 0u - (uint32_t)d, num;
    int32_t f, omf, f2, o2, J, dm, sgn_k, dir;
    int hi_edge;                                    /* 1: the 0.5 edge, 0: the 0 edge */
    if (pv < 0x80000000u) {                         /* was in the lower half, now in the upper */
        if (d > 0) { hi_edge = 1; dir = 1; num = pe - 0x80000000u; }
        else       { hi_edge = 0; dir = -1; num = ad - pv; }
    } else {
        if (d > 0) { hi_edge = 0; dir = 1; num = pe; }
        else       { hi_edge = 1; dir = -1; num = ad - (pv - 0x80000000u); }
    }
    f = (int32_t)op_frac_q15(num, ad); omf = 32768 - f;
    f2 = (f * f) >> 15; o2 = (omf * omf) >> 15;
    J = hi_edge ? -2 * w->wq : 2 * (w->wq - w->ws);   /* forward jump of the wave at this edge */
    if (dir < 0) J = -J;
    *cp = (J * f2) >> 16;
    *cn = -((J * o2) >> 16);
    if (w->wt) {
        int32_t f3 = (f2 * f) >> 15, o3 = (o2 * omf) >> 15;
        dm = (int32_t)(ad >> 17); if (dm > 4095) dm = 4095;
        dm = (dm * w->wt) >> 12; if (dm > 32767) dm = 32767;
        sgn_k = hi_edge ? -1 : 1;
        *cp += sgn_k * ((((dm * f3) >> 15) * 5461) >> 15);
        *cn += sgn_k * ((((dm * o3) >> 15) * 5461) >> 15);
    }
}

/* one band-limited sample for effective phase pe; output delayed by one sample */
static inline __attribute__((always_inline)) int32_t osc_tick(pu_osc *o, uint32_t pe, const pu_w *w)
{
    uint32_t pv = o->pe_prev;
    int32_t y, out, cn = 0, cp = 0;
    if (w->zone == 0) {
        int32_t saw = (int32_t)(pe >> 16) - 32768, sq = pe < 0x80000000u ? 32767 : -32767;
        y = saw + (((sq - saw) * w->t) >> 15);
    } else {
        uint32_t u = pe >> 16;
        int32_t sq = pe < 0x80000000u ? 32767 : -32767;
        int32_t tri = u < 32768 ? (int32_t)u * 2 - 32768 : 98303 - (int32_t)u * 2;
        y = sq + (((tri - sq) * w->t) >> 15);
    }
    if (((pe ^ pv) & 0x80000000u) && ((pe - pv) + 0x30000000u) < 0x60000000u) osc_edge(pv, pe, w, &cp, &cn);
    out = o->y_prev + cp;
    o->y_prev = y + cn;
    o->pe_prev = pe;
    return out;
}

/* ---------------------------------------------------------------- one stereo-half chain, one sub-block */
typedef struct {
    uint32_t incA, incB;
    pu_w wA, wB;
    int32_t idx;            /* FM index, Q15 (already tapered) */
    int32_t fb;             /* B feedback, Q15 */
    int32_t nfm;            /* noise -> FM, Q15 */
    int32_t lvlB;           /* B audible level */
    int32_t nmix;           /* noise mix Q15 */
    int32_t k_q12;          /* resonance */
    int32_t drive_q8;       /* pre-filter gain */
    int32_t G0, G1;         /* filter G start/end */
    int32_t amp0, amp1;
} pu_sb;

static void chain_run(pu_chain *c, const pu_sb *p, int32_t *out, int n)
{
    int32_t sigbuf[OP_SUB];
    int32_t i;
    /* pass 1: oscillators + noise + mix -> sigbuf (own loop: keeps register pressure down on the Blackfin) */
    {
        uint32_t pa = c->pa, pb = c->pb, nzs = c->nz.s;
        const int32_t idx = p->idx, fb = p->fb, nfm = p->nfm;
        const int32_t nmix = p->nmix, nmixc = 32768 - p->nmix;
        const int32_t cA = 19661, cB = (p->lvlB * 19661) >> 15;
        const uint32_t incA = p->incA, incB = p->incB;
        const pu_w wA = p->wA, wB = p->wB;
        pu_osc oa = c->a, ob = c->b;
        for (i = 0; i < n; i++) {
            int32_t yb, ya, mix, nz;
            uint32_t pm, peb, pea;
            pb += incB;
            peb = pb;
            if (fb) peb += (uint32_t)((ob.y_prev * fb) >> 15) * 65536u;
            yb = osc_tick(&ob, peb, &wB);
            nzs ^= nzs << 13; nzs ^= nzs >> 17; nzs ^= nzs << 5;
            nz = (int32_t)nzs >> 16;
            pa += incA;
            pm = (uint32_t)(((yb * idx) >> 15) * 3) * 131072u;
            if (nfm) pm += (uint32_t)((nz * nfm) >> 15) * 131072u;
            pea = pa + pm;
            ya = osc_tick(&oa, pea, &wA);
            mix = (ya * cA + yb * cB) >> 15;
            sigbuf[i] = nmix ? (mix * nmixc + ((nz * 22937) >> 15) * nmix) >> 15 : mix;
        }
        c->pa = pa; c->pb = pb; c->nz.s = nzs; c->a = oa; c->b = ob;
    }
    /* pass 2: Moog-style 4-pole ladder (trapezoidal stages), saturating at the input (resonance feedback loop) */
    {
        int32_t G = p->G0 << 4, dG, amp = p->amp0 << 4, dA;
        int32_t s0 = c->s[0], s1 = c->s[1], s2 = c->s[2], s3 = c->s[3], y4 = c->y4;
        const int32_t k = p->k_q12, gain = p->drive_q8;
        if (n == OP_SUB) { dG = p->G1 - p->G0; dA = p->amp1 - p->amp0; }
        else { dG = ((p->G1 - p->G0) * 16) / n; dA = ((p->amp1 - p->amp0) * 16) / n; }
        for (i = 0; i < n; i++) {
            int32_t u, v, y, g = G >> 4;
            u = ((sigbuf[i] * gain) >> 8) - ((k * y4) >> 12);
            u = op_sat3(u);
            v = (((u - s0) >> 1) * g) >> 14; y = v + s0; s0 = y + v; u = y;
            v = (((u - s1) >> 1) * g) >> 14; y = v + s1; s1 = y + v; u = y;
            v = (((u - s2) >> 1) * g) >> 14; y = v + s2; s2 = y + v; u = y;
            v = (((u - s3) >> 1) * g) >> 14; y = v + s3; s3 = y + v;
            y4 = op_clamp(y, -32767, 32767);
            out[i] = (y4 * (amp >> 4)) >> 15;
            G += dG; amp += dA;
        }
        c->s[0] = s0; c->s[1] = s1; c->s[2] = s2; c->s[3] = s3; c->y4 = y4;
    }
}

/* ---------------------------------------------------------------- voice */
static int32_t ratio_off_q8(int p, int snap)
{
    int32_t r = p * 12 - 131072;                     /* octaves Q16, -2 .. +4 */
    if (!snap) return (r * 3) >> 6;
    {
        uint32_t m = op_exp2_q8((uint32_t)(r + 131072));   /* 256 * 4 * ratio */
        int n4 = (int)((m + 128) >> 8), ri;
        if (n4 < 3) return n4 <= 1 ? -6144 : -3072;
        ri = (n4 + 2) >> 2;
        if (ri > 16) ri = 16;
        return ratio_semis_q8[ri - 1];
    }
}

static int32_t curve_of(int32_t x, int c)
{
    int32_t a = op_abs(x), y;
    switch (c) {
    case MXC_EXP: y = (a * a) >> 15; break;
    case MXC_LOG: y = (a * (65536 - a)) >> 15; break;
    case MXC_S:   y = (((a * a) >> 15) * (98304 - 2 * a)) >> 15; break;
    default: return x;
    }
    y = op_clamp(y, 0, 32767);
    return x < 0 ? -y : y;
}

static void voice_sub(pu_synth *s, pu_voice *v, int32_t *L, int32_t *R, int n)
{
    const int32_t *e = s->eff;
    int32_t src[MXS_COUNT], mxd[MXD_COUNT], i;
    int32_t shA, shB, det, nmix, idx, fb, nfm, res, drv, amp, note, dr, kq;
    int32_t ratio_off, cnote, G1;
    pu_sb sb;
    int32_t a_out, m_out;

    a_out = op_adsr_tick(&v->aenv, &s->acfg);
    m_out = op_adsr_tick(&v->menv, &s->mcfg);
    if (s->ext_env) a_out = 32767;                          /* host (TE) owns the amp envelope */
    else if (v->aenv.stage == OP_ENV_IDLE) { v->active = 0; }

    /* seeded analog drift: slow bounded random walks */
    v->dA += (op_rand_bi(&v->rng) >> 5) - (v->dA >> 7);
    v->dB += (op_rand_bi(&v->rng) >> 5) - (v->dB >> 7);
    v->dC += (op_rand_bi(&v->rng) >> 5) - (v->dC >> 7);
    v->dA = op_clamp(v->dA, -32767, 32767); v->dB = op_clamp(v->dB, -32767, 32767); v->dC = op_clamp(v->dC, -32767, 32767);

    /* mod matrix */
    src[MXS_NONE] = 0;
    src[MXS_LFO1] = s->lfo_val[0]; src[MXS_LFO2] = s->lfo_val[1];
    src[MXS_MODENV] = m_out; src[MXS_AMPENV] = a_out;
    src[MXS_VEL] = v->vel * 258;
    src[MXS_KEY] = op_clamp(((v->note_q8 - 15360) * 7) >> 1, -32767, 32767);
    src[MXS_TILTX] = s->in.tilt_x; src[MXS_TILTY] = s->in.tilt_y;
    src[MXS_NOISE] = op_rand_bi(&v->rng);
    src[MXS_DRIFT] = v->dA;
    for (i = 0; i < MXD_COUNT; i++) mxd[i] = 0;
    for (i = 0; i < PU_MX_SLOTS; i++) {
        int sidx = zone(s->mx[i].src, MXS_COUNT), didx = zone(s->mx[i].dst, MXD_COUNT);
        int32_t amt = (s->mx[i].amt - 16384) * 2;
        if (sidx == MXS_NONE || didx == MXD_NONE) continue;
        mxd[didx] += (curve_of(src[sidx], zone(s->mx[i].curve, MXC_COUNT)) * amt) >> 15;
    }

    shA = op_clamp(e[PU_SHAPE_A] + mxd[MXD_SHAPE_A], 0, OP_PMAX);
    shB = op_clamp(e[PU_SHAPE_B] + mxd[MXD_SHAPE_B], 0, OP_PMAX);
    det = op_clamp(e[PU_DETUNE] + mxd[MXD_DETUNE], 0, OP_PMAX);
    nmix = op_clamp(e[PU_NOISE_MIX] + mxd[MXD_NOISE_MIX], 0, OP_PMAX);
    idx = op_clamp(e[PU_FM_INDEX] + mxd[MXD_FM_INDEX], 0, OP_PMAX);
    fb = op_clamp(e[PU_FM_FB] + mxd[MXD_FM_FB], 0, OP_PMAX);
    nfm = op_clamp(e[PU_NOISE_FM] + mxd[MXD_NOISE_FM], 0, OP_PMAX);
    res = op_clamp(e[PU_RES] + mxd[MXD_RES], 0, OP_PMAX);
    drv = op_clamp(e[PU_DRIVE] + mxd[MXD_DRIVE], 0, OP_PMAX);
    amp = op_clamp(32768 + mxd[MXD_AMP], 0, 65535);

    note = v->note_q8 + ((mxd[MXD_PITCH] * 6144) >> 15);
    dr = (e[PU_DRIFT] * 51) >> 15;                   /* up to ~20 cents */
    ratio_off = ratio_off_q8(op_clamp(e[PU_FM_RATIO] + mxd[MXD_FM_RATIO], 0, OP_PMAX), s->snap);

    {
        int32_t d_q8 = (det * 102) >> 15, nA = note + ((v->dA * dr) >> 15), nB = note + ((v->dB * dr) >> 15) + ratio_off;
        int stereo = d_q8 != 0;
        int ch;
        int32_t cut_p = e[PU_CUTOFF] + mxd[MXD_CUTOFF]
                      + (((m_out * (e[PU_ENV_AMT] - 16384)) >> 14))
                      + ((((v->note_q8 - 15360) * 273) >> 8) >> 1);
        cut_p = op_clamp(cut_p, 0, OP_PMAX);
        cnote = 3840 + ((cut_p * 30720) >> 15) + ((v->dC * 256) >> 15);
        G1 = op_g_from_note(cnote);
        if (G1 > 16384) G1 = 16384;                      /* G <= 0.5 (fc <= sr/4): stage states stay bounded by the input */
        kq = (res * 12400) >> 15;                                          /* k = 0 .. ~3.0 (loop is delayed) */
        kq = (kq * (32768 - (G1 >> 1))) >> 15;
        shape_w(shA, &sb.wA); shape_w(shB, &sb.wB);
        sb.idx = (idx * idx) >> 15;
        sb.fb = (fb * fb) >> 15;
        sb.nfm = (nfm * nfm) >> 15;
        sb.lvlB = 32767 - ((sb.idx * 3) >> 2);
        sb.nmix = nmix;
        sb.k_q12 = kq;
        {
            int32_t gq = 256 + ((drv * 1792) >> 15);
            sb.drive_q8 = (gq * (32768 + kq)) >> 15;
        }
        sb.G0 = v->ch[0].G < 0 ? G1 : v->ch[0].G; sb.G1 = G1;
        sb.amp0 = v->amp;
        {
            int32_t velq = 8192 + v->vel * 194, a;
            if (velq > 32767) velq = 32767;
            a = (a_out * velq) >> 15;
            a = (a * 13107) >> 15;
            sb.amp1 = (a * amp) >> 15;
        }
        v->amp = sb.amp1;
        for (ch = 0; ch < (stereo ? 2 : 1); ch++) {
            int32_t sg = ch ? d_q8 : -d_q8;
            sb.incA = op_note_inc(nA + sg);
            sb.incB = op_note_inc(nB + sg);
            chain_run(&v->ch[ch], &sb, ch ? R : L, n);
            v->ch[ch].G = G1;
        }
        if (!stereo) {
            /* mono: the right output is the left output (same pointer arithmetic below) */
            v->ch[1].G = G1;
            for (i = 0; i < n; i++) R[i] = L[i];
        }
    }
}

/* L/R are accumulated by voice_sub callers: render into scratch then add */
void pu_voice_render(pu_synth *s, pu_voice *v, int32_t *L, int32_t *R, int n)
{
    int32_t tl[OP_SUB], tr[OP_SUB];
    while (n > 0) {
        int m = n < OP_SUB ? n : OP_SUB, i;
        if (!v->active) return;
        voice_sub(s, v, tl, tr, m);
        for (i = 0; i < m; i++) { L[i] += tl[i]; R[i] += tr[i]; }
        L += m; R += m; n -= m;
    }
}

void pu_voice_start(pu_synth *s, pu_voice *v, int32_t note_q8, int vel)
{
    int c;
    v->active = 1; v->gate = 1;
    v->note_q8 = v->note_tgt_q8 = note_q8;
    v->vel = op_clamp(vel, 1, 127);
    v->age = ++s->age;
    for (c = 0; c < 2; c++) {
        pu_chain *ch = &v->ch[c];
        int k;
        if (c == 0) { ch->pa = op_rand(&v->rng); ch->pb = op_rand(&v->rng); }
        else { ch->pa = v->ch[0].pa; ch->pb = v->ch[0].pb; }
        ch->a.pe_prev = ch->pa; ch->b.pe_prev = ch->pb; ch->a.y_prev = ch->b.y_prev = 0;
        for (k = 0; k < 4; k++) ch->s[k] = 0;
        ch->y4 = 0; ch->G = -1;
        op_seed(&ch->nz, v->rng.s + (uint32_t)c * 77u);
    }
    v->amp = 0;
    v->aenv.out = 0; v->menv.out = 0;
    op_adsr_on(&v->aenv); op_adsr_on(&v->menv);
}
void pu_voice_release(pu_synth *s, pu_voice *v)
{
    (void)s;
    v->gate = 0;
    op_adsr_off(&v->aenv); op_adsr_off(&v->menv);
}

/* ---------------------------------------------------------------- synth level */
static int nvoices(const pu_synth *s) { int n = ((s->p[PU_VOICES] * PU_MAX_VOICES) >> 15) + 1; return n > PU_MAX_VOICES ? PU_MAX_VOICES : n; }

void pu_note_on(pu_synth *s, int note, int vel)
{
    int i, nv = nvoices(s), pick = -1;
    uint32_t best = 0xffffffffu;
    pu_voice *v;
    for (i = 0; i < nv; i++) if (!s->v[i].active) { pick = i; break; }
    if (pick < 0) {         /* steal: prefer a releasing voice, then the oldest */
        for (i = 0; i < nv; i++) {
            uint32_t a = s->v[i].age + (s->v[i].gate ? 0x40000000u : 0u);
            if (a < best) { best = a; pick = i; }
        }
    }
    v = &s->v[pick];
    pu_voice_start(s, v, note * 256, vel);
    if (s->eff[PU_GLIDE] >= 400 && s->last_note_q8 > 0) v->note_q8 = s->last_note_q8;
    v->note_tgt_q8 = note * 256;
    s->last_note_q8 = note * 256;
    for (i = nv; i < PU_MAX_VOICES; i++) if (s->v[i].active && s->v[i].gate) pu_voice_release(s, &s->v[i]);
}
void pu_note_off(pu_synth *s, int note)
{
    int i;
    for (i = 0; i < PU_MAX_VOICES; i++)
        if (s->v[i].active && s->v[i].gate && s->v[i].note_tgt_q8 == note * 256) pu_voice_release(s, &s->v[i]);
}
void pu_all_off(pu_synth *s)
{
    int i;
    for (i = 0; i < PU_MAX_VOICES; i++) if (s->v[i].active && s->v[i].gate) pu_voice_release(s, &s->v[i]);
}
int pu_active_voices(const pu_synth *s)
{
    int i, n = 0;
    for (i = 0; i < PU_MAX_VOICES; i++) n += s->v[i].active;
    return n;
}

int32_t pu_master(int32_t x) { return op_drive(x, 300); }

void pu_render(pu_synth *s, int32_t *L, int32_t *R, int n)
{
    int done = 0, i;
    while (done < n) {
        int m = n - done < 64 ? n - done : 64;
        int32_t *l = L + done, *r = R + done;
        for (i = 0; i < m; i++) l[i] = r[i] = 0;
        pu_global_update(s, m);
        for (i = 0; i < PU_MAX_VOICES; i++) {
            pu_voice *v = &s->v[i];
            if (!v->active) continue;
            if (v->note_q8 != v->note_tgt_q8) {                 /* glide, once per chunk */
                if (s->glide_k >= 32768) v->note_q8 = v->note_tgt_q8;
                else {
                    int32_t k = s->glide_k * ((m + OP_SUB - 1) / OP_SUB);
                    int32_t d = ((v->note_tgt_q8 - v->note_q8) * (k > 32768 ? 32768 : k)) >> 15;
                    if (d == 0) d = v->note_tgt_q8 > v->note_q8 ? 1 : -1;
                    v->note_q8 += d;
                }
            }
            pu_voice_render(s, v, l, r, m);
        }
        for (i = 0; i < m; i++) {
            l[i] = op_clamp(op_dc_tick(&s->dc[0], pu_master(l[i])), -32767, 32767);
            r[i] = op_clamp(op_dc_tick(&s->dc[1], pu_master(r[i])), -32767, 32767);
        }
        done += m;
    }
}

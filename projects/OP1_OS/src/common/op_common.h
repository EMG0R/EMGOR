/* OP FREAK common DSP layer: portable fixed-point C (host clang + bfin-elf-gcc, no libc, no FPU).
 * Conventions (same as the SDK engines): audio Q15 in int32 (+-32767 nominal, headroom allowed),
 * parameters int16 0..32767 (bipolar params centre 16384), phase uint32 (2^32 = one cycle),
 * pitch = note number in Q8 (60*256 = middle C), time-varying state updated in sub-blocks. */
#ifndef OP_COMMON_H
#define OP_COMMON_H

#include <stdint.h>

#define OP_PMAX     32767
#define OP_SUB      16                  /* control-rate sub-block (samples) */
#define OP_INC_MAX  0x70000000u         /* phase increment clamp (~0.44 cycles/sample) */

/* ---- sample rate (call once; 44100, 48000, 88200, 96000 supported) -------------------------- */
void op_init(uint32_t sr);
uint32_t op_sr(void);

/* ---- seeded PRNG: xorshift32, deterministic per seed --------------------------------------- */
typedef struct { uint32_t s; } op_prng;
void op_seed(op_prng *p, uint32_t seed);
static inline uint32_t op_rand(op_prng *p)
{
    uint32_t x = p->s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    p->s = x;
    return x;
}
static inline int32_t op_rand_bi(op_prng *p) { return (int32_t)op_rand(p) >> 16; }  /* -32768..32767 */

/* ---- fast approximations ------------------------------------------------------------------- */
static inline int32_t op_clamp(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int32_t op_abs(int32_t x) { return x < 0 ? -x : x; }
static inline int32_t op_lerp(int32_t a, int32_t b, int32_t t_q15) { return a + (((b - a) * t_q15) >> 15); }
static inline int32_t op_clip16(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }

int32_t op_sin_q15(uint32_t phase);                 /* sine, full-scale +-32767 */
uint32_t op_exp2_q8(uint32_t x_q16);                /* 2^x * 256, x in Q16 octaves, 0..~20 */
uint32_t op_note_inc(int32_t note_q8);              /* phase increment for a note, clamped */
int32_t op_g_from_note(int32_t note_q8);            /* TPT one-pole G (Q15) for a cutoff given as a note */
int32_t op_inc_to_note(uint32_t inc);                    /* inverse of op_note_inc (Q8 note) */
uint32_t op_frac_q15(uint32_t num, uint32_t den);   /* num/den as Q15, num < den */

/* denormal/NaN-free by construction (integers); these keep everything bounded */
static inline int32_t op_sat3(int32_t x)            /* 1.5x - 0.5x^3, hard-limited at +-32767, C1 */
{
    int32_t x2, x3;
    if (x > 32767) x = 32767; else if (x < -32767) x = -32767;
    x2 = (x * x) >> 15;
    x3 = (x2 * x) >> 15;
    return x + ((x - x3) >> 1);
}
/* drive: gain Q8 (256 = unity, max 4095), then sat3. Input clamped first (no overflow). */
static inline int32_t op_drive(int32_t x, int32_t gain_q8)
{
    if (x > (1 << 18)) x = 1 << 18; else if (x < -(1 << 18)) x = -(1 << 18);
    return op_sat3((x * gain_q8) >> 8);
}
/* asymmetric drive: positive half pushed harder (even harmonics), still bounded */
static inline int32_t op_drive_asym(int32_t x, int32_t gain_q8, int32_t bias)
{
    int32_t y;
    if (x > (1 << 18)) x = 1 << 18; else if (x < -(1 << 18)) x = -(1 << 18);
    y = op_sat3(((x * gain_q8) >> 8) + bias);
    return y - op_sat3(bias);
}

typedef struct { int32_t x1, y1; } op_dc;           /* DC blocker, pole ~0.998; y1 is Q8 (no truncation limit cycle) */
static inline int32_t op_dc_tick(op_dc *d, int32_t x)
{
    int32_t xc = op_clamp(x, -(1 << 20), 1 << 20), yq;
    yq = ((xc - d->x1) * 256) + d->y1 - (d->y1 >> 9);
    d->x1 = xc; d->y1 = op_clamp(yq, -(1 << 29), 1 << 29);
    return yq >> 8;
}

/* ---- ADSR: sub-block rate, linear attack, squared (exp-like) decay and release ------------- */
enum { OP_ENV_IDLE, OP_ENV_ATT, OP_ENV_DEC, OP_ENV_SUS, OP_ENV_REL };
typedef struct { int32_t a_step, d_step, r_step, sus; } op_adsr_cfg;   /* steps Q24 per tick, sus Q15 */
typedef struct { uint8_t stage; int32_t lvl, dl, rl, rel_from, out; } op_adsr;
void op_adsr_cfg_set(op_adsr_cfg *c, int a, int d, int s, int r);      /* params 0..32767 */
void op_adsr_on(op_adsr *e);
void op_adsr_off(op_adsr *e);
int32_t op_adsr_tick(op_adsr *e, const op_adsr_cfg *c);                /* returns Q15 0..32767 */
static inline int op_adsr_active(const op_adsr *e) { return e->stage != OP_ENV_IDLE; }

/* ---- LFO (per control tick) ----------------------------------------------------------------- */
enum { OP_LFO_SINE, OP_LFO_TRI, OP_LFO_SAW, OP_LFO_SQR, OP_LFO_SH, OP_LFO_SMOOTH, OP_LFO_SHAPES };
typedef struct { uint32_t ph; int32_t held, cur; op_prng r; } op_lfo;
void op_lfo_init(op_lfo *l, uint32_t seed);
int op_lfo_shape_of(int p);                                            /* param -> shape */
uint32_t op_lfo_inc(int rate_p, int sync_p, int32_t bpm_q8);           /* phase inc per OP_SUB samples */
int32_t op_lfo_tick(op_lfo *l, int shape, uint32_t inc);               /* +-32767 */

/* ---- GEN: the universal per-parameter modulation grammar (PLAN.md) --------------------------- */
enum { OP_GEN_OFF, OP_GEN_DRIFT, OP_GEN_STEP, OP_GEN_LFO1, OP_GEN_LFO2, OP_GEN_TILTX, OP_GEN_TILTY,
       OP_GEN_SEQ, OP_GEN_MODES };
extern const char *const op_gen_names[OP_GEN_MODES];
typedef struct { uint8_t mode; uint8_t rate; int16_t depth; } op_gen;   /* rate 0..127, depth bipolar Q15 */
typedef struct {                  /* filled by the platform / engine each block */
    int16_t lfo1, lfo2;           /* +-32767 */
    int16_t tilt_x, tilt_y;       /* G-sensor, +-32767 */
    int16_t seq;                  /* value of the sequencer's current step, +-32767 */
    uint8_t clock_valid;          /* STEP follows step_tick when set */
    uint32_t step_tick;           /* any change = a clock step */
} op_gen_in;
typedef struct { int32_t cur, slope_q8, cnt, held; uint32_t last_tick; op_prng rng; } op_gen_state;
void op_gen_state_init(op_gen_state *s, uint32_t seed);                /* RESEED = call again */
int32_t op_gen_eval(const op_gen *g, op_gen_state *s, const op_gen_in *in, int32_t n_samples); /* +-32767 */
int32_t op_gen_apply(int32_t base, const op_gen *g, op_gen_state *s, const op_gen_in *in, int32_t n_samples);

#endif

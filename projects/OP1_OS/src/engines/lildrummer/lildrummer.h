/* LIL DRUMMER: 24-key 808-ish FM drum synth core (fixed point, portable C).
 * Every key is a full voice with a MODEL parameter that morphs the topology
 * (kick > tom > snare > clap > rim > cowbell > hat > zap). Spec: docs/PLAN.md, docs/ENGINES.md. */
#ifndef LILDRUMMER_H
#define LILDRUMMER_H

#include "../../common/op_common.h"

#define LD_KEYS 24
enum {
    LD_TUNE, LD_DECAY, LD_PUNCH, LD_LEVEL,              /* T1 BODY */
    LD_FM_RATIO, LD_FM_INDEX, LD_FM_FB, LD_NOISE,       /* T2 FM   */
    LD_DRIVE, LD_FILTER, LD_SNAP, LD_PAN,               /* T3 TONE */
    LD_MODEL, LD_CHOKE, LD_WIDTH, LD_MACRO,             /* T4 KIT  (MACRO is kit-level) */
    LD_PARAMS
};
enum { LD_KICK, LD_TOM, LD_SNARE, LD_CLAP, LD_RIM, LD_COWBELL, LD_HAT, LD_ZAP, LD_MODELS };
#define LD_MODEL_AT(m) ((m) * 4681)                     /* param value of archetype m (fractions morph) */

extern const char *const ld_param_names[LD_PARAMS];
extern const char *const ld_model_names[LD_MODELS];
extern const char *const ld_key_names[LD_KEYS];

typedef struct {
    uint32_t p1, p2, pm[6];
    uint32_t inc_m[6];
    int32_t lvl_b, step_b, lvl_n, step_n, lvl_p, step_p, lvl_f, step_f, click;
    int32_t eb_prev, en_prev;
    int32_t note0, pen, mod_off, idx, fb, wb, wn, wm, click0, drive, level, gl, gr, width;
    int32_t svf_f, svf_q, svf_mode;
    int32_t ma[6];
    int32_t s_bp[2], s_lp[2], y2;
    int32_t bursts, burst_i, gap_ticks, gt, inv_gap;
    int32_t nfm, bias;
    int32_t nlvl_ramp;
    uint8_t active, choke;
    uint32_t age;
    op_prng rng[2];
} ld_voice;

typedef struct {
    int16_t p[LD_PARAMS];
    op_gen gen[LD_PARAMS];
    op_gen_state gs[LD_PARAMS];
    uint32_t last_hit;
} ld_key;

typedef struct {
    ld_key key[LD_KEYS];
    int16_t macro;
    op_gen macro_gen;
    op_gen_state macro_gs;
    int16_t swing;                      /* swing-link amount for the sequencer (0..32767) */
    op_gen_in in;
    ld_voice v[LD_KEYS];
    uint32_t time, age, seed;
    op_dc dc[2];
    op_prng rng;
} ld_kit;

void ld_init(ld_kit *k, uint32_t seed);              /* op_init(sr) first; loads the 808 default kit */
void ld_default_kit(ld_kit *k);
void ld_reseed(ld_kit *k, uint32_t seed);
void ld_set_param(ld_kit *k, int key, int id, int value);   /* LD_MACRO is kit-level (key ignored) */
int ld_get_param(const ld_kit *k, int key, int id);
void ld_set_gen(ld_kit *k, int key, int id, int mode, int depth, int rate);
void ld_set_inputs(ld_kit *k, const op_gen_in *in);
void ld_trigger(ld_kit *k, int key, int vel);        /* vel 1..127 */
void ld_render(ld_kit *k, int32_t *L, int32_t *R, int n);
int ld_active_voices(const ld_kit *k);

/* lower level for engine hosts: GEN/macro resolve at trigger, voices render independently */
void ld_voice_trigger(ld_kit *k, int key, int vel);
void ld_voice_render(ld_kit *k, ld_voice *v, int32_t *L, int32_t *R, int n);   /* adds */
int32_t ld_master(int32_t x);

#endif

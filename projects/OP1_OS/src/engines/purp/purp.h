/* THE PURP: 5-voice stereo analog-style FM synth core (fixed point, portable C).
 * Spec: docs/PLAN.md (THE PURP pages) and docs/ENGINES.md. */
#ifndef PURP_H
#define PURP_H

#include "../../common/op_common.h"

#define PU_MAX_VOICES 5

/* parameters, 4 per sub-page. Sub-pages: P0 OSC P1 FM | P2 FILTER P3 AMP P4 MOD | P5 LFO1 P6 LFO2 P7 MATRIX | P8 VOICE */
enum {
    PU_SHAPE_A, PU_SHAPE_B, PU_DETUNE, PU_NOISE_MIX,        /* T1 SOUND  / OSC    */
    PU_FM_RATIO, PU_FM_INDEX, PU_FM_FB, PU_NOISE_FM,        /*           / FM     */
    PU_CUTOFF, PU_RES, PU_DRIVE, PU_ENV_AMT,                /* T2 SHAPE  / FILTER */
    PU_A_ATT, PU_A_DEC, PU_A_SUS, PU_A_REL,                 /*           / AMP ENV */
    PU_M_ATT, PU_M_DEC, PU_M_SUS, PU_M_REL,                 /*           / MOD ENV */
    PU_L1_RATE, PU_L1_SHAPE, PU_L1_DEPTH, PU_L1_SYNC,       /* T3 MOVE   / LFO1   */
    PU_L2_RATE, PU_L2_SHAPE, PU_L2_DEPTH, PU_L2_SYNC,       /*           / LFO2   */
    PU_MX_SRC, PU_MX_DST, PU_MX_AMT, PU_MX_CURVE,           /*           / MATRIX (edits the active slot) */
    PU_VOICES, PU_GLIDE, PU_DRIFT, PU_MACRO,                /* T4 VOICE  */
    PU_PARAMS
};
#define PU_SUBPAGES 9

enum { MXS_NONE, MXS_LFO1, MXS_LFO2, MXS_MODENV, MXS_AMPENV, MXS_VEL, MXS_KEY, MXS_TILTX, MXS_TILTY,
       MXS_NOISE, MXS_DRIFT, MXS_COUNT };
enum { MXD_NONE, MXD_PITCH, MXD_SHAPE_A, MXD_SHAPE_B, MXD_DETUNE, MXD_NOISE_MIX, MXD_FM_RATIO,
       MXD_FM_INDEX, MXD_FM_FB, MXD_NOISE_FM, MXD_CUTOFF, MXD_RES, MXD_DRIVE, MXD_AMP, MXD_COUNT };
enum { MXC_LIN, MXC_EXP, MXC_LOG, MXC_S, MXC_COUNT };
#define PU_MX_SLOTS 4

typedef struct { const char *name; uint8_t page; } pu_param_info;   /* page = sub-page 0..8 */
extern const pu_param_info pu_info[PU_PARAMS];
extern const char *const pu_subpage_names[PU_SUBPAGES];
extern const char *const pu_mx_src_names[MXS_COUNT];
extern const char *const pu_mx_dst_names[MXD_COUNT];

typedef struct { int16_t src, dst, amt, curve; } pu_mx_slot;     /* params 0..32767 (amt centre 16384) */

typedef struct { uint32_t pe_prev; int32_t y_prev; } pu_osc;
typedef struct {
    uint32_t pa, pb;                    /* oscillator phases */
    pu_osc a, b;
    int32_t s[4], y4;                   /* ladder */
    int32_t G;                          /* current G (Q15) */
    op_prng nz;
} pu_chain;

typedef struct {
    pu_chain ch[2];
    op_adsr aenv, menv;
    int32_t note_q8, note_tgt_q8;       /* glide state */
    int32_t vel;
    uint8_t active, gate;
    uint32_t age;
    int32_t dA, dB, dC;                 /* seeded analog drift states */
    int32_t amp;                        /* last amp (Q15) for ramping */
    op_prng rng;
} pu_voice;

typedef struct {
    int16_t p[PU_PARAMS];
    op_gen gen[PU_PARAMS];
    op_gen_state gs[PU_PARAMS];
    pu_mx_slot mx[PU_MX_SLOTS];
    uint8_t mx_active;                  /* slot the MATRIX page edits */
    uint8_t snap;                       /* FM ratio snap-to-integer (push on RATIO) */
    uint8_t ext_env;                    /* 1: host applies the amp envelope (SDK adapter); voices never self-end */
    uint32_t seed;
    /* inputs from the platform */
    op_gen_in in;
    int32_t bpm_q8;
    /* derived, per global update */
    int32_t eff[PU_PARAMS];
    int32_t lfo_val[2];
    op_lfo lfo[2];
    uint32_t lfo_inc[2];
    int32_t lfo_key[2][3];
    op_adsr_cfg acfg, mcfg;
    int16_t acfg_key[4], mcfg_key[4];
    int32_t glide_k;                    /* Q15 per sub-block */
    int16_t glide_key;
    /* wavetable slot (stub): morph target for the shape knob; unused until tables exist */
    const int16_t *wavetable; int32_t wavetable_len;
    pu_voice v[PU_MAX_VOICES];
    uint32_t age;
    int32_t last_note_q8;
    op_dc dc[2];
    op_prng rng;
} pu_synth;

void pu_init(pu_synth *s, uint32_t seed);               /* op_init(sr) must have been called */
void pu_reseed(pu_synth *s, uint32_t seed);             /* RESEED: new random material, same params */
#define PU_PRESETS 8
extern const char *const pu_preset_names[PU_PRESETS];
void pu_preset(pu_synth *s, int n);
void pu_set_param(pu_synth *s, int id, int value);      /* 0..32767 */
void pu_set_gen(pu_synth *s, int id, int mode, int depth, int rate);
void pu_set_matrix(pu_synth *s, int slot, int src, int dst, int amt, int curve); /* param values */
void pu_set_snap(pu_synth *s, int on);
void pu_set_inputs(pu_synth *s, const op_gen_in *in, int32_t bpm_q8);
void pu_set_wavetable(pu_synth *s, const int16_t *table, int len);   /* stub slot */

/* musical API (host / sequencer). note: MIDI number, vel 1..127. Output: L/R int32, +-32767 */
void pu_note_on(pu_synth *s, int note, int vel);
void pu_note_off(pu_synth *s, int note);
void pu_all_off(pu_synth *s);
void pu_render(pu_synth *s, int32_t *L, int32_t *R, int n);   /* overwrites L/R, any n */
int pu_active_voices(const pu_synth *s);

/* lower level, for engine hosts that own voice allocation (SDK adapter) */
void pu_global_update(pu_synth *s, int n_samples);            /* once per block: GEN, LFOs, macro, env cfg */
void pu_voice_start(pu_synth *s, pu_voice *v, int32_t note_q8, int vel);
void pu_voice_release(pu_synth *s, pu_voice *v);
/* adds one voice's stereo output into L/R (n samples); v->note_q8 is the pitch (glide done by the caller) */
void pu_voice_render(pu_synth *s, pu_voice *v, int32_t *L, int32_t *R, int n);
int32_t pu_master(int32_t x);                                 /* final drive/limit stage */

#endif

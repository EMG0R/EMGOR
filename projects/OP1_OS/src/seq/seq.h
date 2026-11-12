/* seq.h - OP FREAK generative sequencer core. Portable C: host build (tests) + bfin-elf.
 *
 * One host (seq_host) + mode plugins (seq_mode_desc). No malloc, no libc, no float, no 64-bit
 * math in the realtime path; every buffer is a fixed array inside seq_host. The caller owns the
 * seq_host (static storage on the OP-1; a few KB).
 *
 *   seq_init(h, seed)                      defaults, mode TURING
 *   seq_set_mode(h, SEQ_EUCLID)            switch mode (state reset from the current seed)
 *   seq_start(h) / seq_stop(h)             transport. start rewinds + reseeds: deterministic
 *   seq_run_frames(h, 64, 44100)           INTERNAL clock: call once per 64-frame audio block
 *   seq_ext_pulse(h)                       EXTERNAL clock: call per MIDI clock (24 PPQN)
 *   seq_ext_start/continue/stop(h)         EXTERNAL transport (MIDI FA / FB / FC)
 *   seq_event_pop(h, &ev)                  drain note on/off events (in order)
 *   seq_note_in(h, note, vel, on)          performance input (MARKOV learns from it)
 *   seq_encoder / seq_push / seq_ui_page   4-knob page UI model (see docs/SEQUENCER.md)
 *   seq_viz(h, &v)                         small grid for the screen
 *   seq_reseed(h, seed)                    new material, same params
 *   seq_mod_get(h)                         per-step value stream (seq_mod.h)
 */
#ifndef SEQ_H
#define SEQ_H
#include <stdint.h>
#include "seq_mod.h"

#define SEQ_PPQ          96          /* internal ticks per quarter note (24 PPQN clock x 4) */
#define SEQ_POLY         3           /* max notes per step */
#define SEQ_MAX_MP       12          /* mode params (three 4-knob pages) */
#define SEQ_PENDING      64
#define SEQ_EVQ          256         /* output ring, power of two */

enum { SEQ_TURING, SEQ_EUCLID, SEQ_MARKOV, SEQ_GAMELAN, SEQ_CELLULAR, SEQ_TOTAL, SEQ_NMODES };

/* universal params (h->up[]) */
enum {
    SEQ_U_SCALE, SEQ_U_ROOT, SEQ_U_OCT, SEQ_U_PROB, SEQ_U_RATCHET, SEQ_U_SWING, SEQ_U_GATE,
    SEQ_U_SPREAD, SEQ_U_RATE, SEQ_U_BPM, SEQ_U_CLKSRC, SEQ_U_VEL, SEQ_U_COUNT
};
/* clock source: follow nobody / follow external MIDI clock / internal master that also sends clock */
enum { SEQ_CLK_INTERNAL = 0, SEQ_CLK_EXTERNAL = 1, SEQ_CLK_MASTER = 2 };

/* scales (index into seq_scales[]) */
enum { SEQ_SC_LYDIAN, SEQ_SC_MAJOR, SEQ_SC_MINOR, SEQ_SC_DORIAN, SEQ_SC_MIXOLYDIAN,
       SEQ_SC_PHRYGIAN, SEQ_SC_PENT_MAJ, SEQ_SC_PENT_MIN, SEQ_SC_BLUES, SEQ_SC_WHOLE,
       SEQ_SC_HARM_MIN, SEQ_SC_SLENDRO, SEQ_SC_PELOG, SEQ_SC_CHROMATIC, SEQ_SC_COUNT };
typedef struct { const char *name; uint8_t n; uint8_t iv[12]; } seq_scale;
extern const seq_scale seq_scales[SEQ_SC_COUNT];

/* ---- PRNG (xorshift32*, tiny, deterministic) ---- */
typedef struct { uint32_t s; } seq_rng;
void     seq_rng_seed(seq_rng *r, uint32_t seed);
uint32_t seq_rng_next(seq_rng *r);
uint32_t seq_rng_range(seq_rng *r, uint32_t n);      /* 0..n-1, n <= 65536 */

/* ---- events ---- */
enum { SEQ_EV_OFF = 0, SEQ_EV_ON = 1 };
typedef struct { uint32_t tick; uint8_t type, note, vel, pad; } seq_event;   /* tick = SEQ_PPQ clock */

/* ---- mode interface ---- */
struct seq_viz;
typedef struct seq_viz seq_viz;
#define SEQ_PF_ACTION 1              /* param is a momentary button (turn right = trigger) */
typedef struct {
    const char *name;
    int16_t min, max, def;
    uint8_t flags;
    const char *const *labels;       /* optional enum names, indexed value-min */
} seq_param_desc;

typedef struct {
    uint8_t on;                      /* 0 = rest slot */
    int8_t  deg;                     /* scale degree (wrapped into the scale), or pitch class 0..11 if semi */
    uint8_t semi;
    uint8_t oct;                     /* 0..255 octave-spread position (linear, mapped by SPREAD) */
    uint8_t vel;
    uint8_t part;                    /* mode tag copied to nothing but the UI/aux (gamelan part etc.) */
} seq_note_out;

typedef struct {
    uint8_t n;                       /* slots used */
    seq_note_out note[SEQ_POLY];
    uint16_t aux;                    /* 0..65535, goes to seq_mod.aux */
} seq_step_out;

struct seq_host;
typedef struct {
    const char *name;
    uint8_t nparams;
    const seq_param_desc *params;
    void (*init)(struct seq_host *h);                       /* defaults into h->mp[] + zero state */
    void (*reseed)(struct seq_host *h);                     /* state from h->seed / h->rng */
    void (*step)(struct seq_host *h, uint32_t step, seq_step_out *out);
    void (*note_in)(struct seq_host *h, int note, int vel, int on);
    void (*action)(struct seq_host *h, int param);          /* SEQ_PF_ACTION params */
    void (*viz)(const struct seq_host *h, struct seq_viz *v);
} seq_mode_desc;

/* small display grid: mode fills what makes sense, UI draws it */
#define SEQ_VIZ_W 16
#define SEQ_VIZ_H 12
struct seq_viz {
    uint8_t w, h;                    /* used size */
    uint8_t cell[SEQ_VIZ_H][SEQ_VIZ_W];   /* 0..255 intensity */
    int8_t  playhead;                /* column of the current step, -1 none */
    uint8_t step;                    /* current step 0..255 */
};

/* ---- clock I/O stub (USB MIDI) ---- */
typedef struct {
    void *user;
    void (*pulse)(void *user);       /* MIDI clock F8, sent 24x per quarter when master */
    void (*start)(void *user);       /* FA */
    void (*stop)(void *user);        /* FC */
} seq_clock_io;

typedef struct { uint32_t due; uint16_t id; uint8_t on, note, vel, used; } seq_pend;   /* id pairs an on with its off */

typedef struct seq_host {
    /* config */
    uint32_t seed;
    seq_rng  rng;
    uint8_t  mode;
    int16_t  up[SEQ_U_COUNT];
    int16_t  mp[SEQ_MAX_MP];
    uint32_t bpm_x100;               /* derived from up[BPM]; seq_set_bpm_x100 may override (TE_BPM) */
    /* transport */
    uint8_t  running;
    uint32_t tick;                   /* internal tick counter since start */
    uint32_t step_idx;               /* steps emitted since start */
    uint32_t acc;                    /* frame accumulator for the internal clock */
    uint8_t  ext_sub;                /* external clock: ticks already run for this pulse (unused) */
    /* output */
    seq_pend pend[SEQ_PENDING];
    seq_event evq[SEQ_EVQ];
    uint16_t ev_head, ev_tail;
    uint16_t pid;                    /* pending pair id counter */
    uint32_t dropped;                /* events lost to a full queue / full pending list */
    /* modulation stream */
    seq_mod  mod;
    uint16_t walk;
    uint16_t gate_hist;              /* last 16 step gates, bit 0 newest */
    void   (*mod_cb)(void *user, const seq_mod *m);
    void    *mod_user;
    seq_clock_io *clock_io;
    /* MARKOV matrix lives outside the union so learning survives mode switches */
    struct { uint16_t w[12][12]; int8_t cur, prev; uint32_t bars; } markov;
    /* mode state: sized for the largest mode */
    union {
        struct { uint32_t reg; } turing;
        struct { uint32_t bits[3]; uint8_t key_h[3], key_s[3]; int8_t walk[3]; } euclid;
        struct { uint8_t core[16], coct[16]; uint8_t part_last; } gamelan;
        struct { uint32_t row; uint8_t g[8]; uint8_t prev_hash; uint8_t gen; uint16_t pop; } cell;
        struct { int8_t deg[16]; uint8_t thr[16], oct[16], vel[16]; int8_t center; uint32_t bars; uint16_t dens; } total;
    } st;
} seq_host;

/* ---- host API ---- */
void seq_init(seq_host *h, uint32_t seed);
void seq_set_mode(seq_host *h, int mode);
void seq_reseed(seq_host *h, uint32_t seed);
void seq_start(seq_host *h);
void seq_stop(seq_host *h);                       /* flushes note-offs */
void seq_set_bpm_x100(seq_host *h, uint32_t bpm_x100);
void seq_run_frames(seq_host *h, uint32_t frames, uint32_t sample_rate);
void seq_tick_one(seq_host *h);                   /* one SEQ_PPQ tick (tests, drivers) */
void seq_ext_pulse(seq_host *h);                  /* 24 PPQN: runs 4 ticks */
void seq_ext_start(seq_host *h);
void seq_ext_continue(seq_host *h);
void seq_ext_stop(seq_host *h);
int  seq_event_pop(seq_host *h, seq_event *ev);   /* 1 if an event was returned */
void seq_note_in(seq_host *h, int note, int vel, int on);
const seq_mod *seq_mod_get(const seq_host *h);

/* helpers shared by modes */
int  seq_scale_size(const seq_host *h);
int  seq_ticks_per_step(const seq_host *h);
int  seq_pitch_of(const seq_host *h, const seq_note_out *n);   /* final MIDI note 0..127 */
uint8_t seq_hash8(uint32_t x);
extern const seq_mode_desc *const seq_modes[SEQ_NMODES];
extern const seq_param_desc seq_uparams[SEQ_U_COUNT];

/* ---- UI model: 4 pages (T1..T4), sub-pages of 4 knobs ---- */
enum { SEQ_CELL_NONE, SEQ_CELL_UNI, SEQ_CELL_MP, SEQ_CELL_SEED, SEQ_CELL_MODE, SEQ_CELL_RESEED,
       SEQ_CELL_RUN, SEQ_CELL_CLEAR };
typedef struct {
    const char *name;                /* "" if unused */
    const char *text;                /* enum label, or NULL (draw value) */
    int16_t value, min, max;
    uint8_t kind, idx;
} seq_ui_cell;
int  seq_ui_subpages(const seq_host *h, int group);          /* sub-pages of group 0..3 */
void seq_ui_page(const seq_host *h, int group, int sub, seq_ui_cell cells[4]);
int  seq_encoder(seq_host *h, int group, int sub, int knob, int delta, int coarse);  /* 1 = changed */
int  seq_push(seq_host *h, int group, int sub, int knob);    /* action / reset-to-default */
void seq_viz_get(const seq_host *h, seq_viz *v);
const char *seq_mode_name(int mode);

/* mode registry externs */
extern const seq_mode_desc seq_mode_turing, seq_mode_euclid, seq_mode_markov, seq_mode_gamelan,
                           seq_mode_cellular, seq_mode_total;
/* euclid helper exposed for tests: fills bit i (LSB first) of the returned word = step i hit */
uint32_t seq_euclid_bits(int hits, int steps, int rot);
/* markov accessors for tests */
int  seq_markov_weight(const seq_host *h, int from_pc, int to_pc);

#endif

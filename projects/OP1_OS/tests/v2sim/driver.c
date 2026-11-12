/* OP FREAK v2 integration bench: the composed opfreak_v2 image (all hooks chained by the composer's _compose.c) against
 * stub TE functions. Returns failed-check bits (0 = pass); detail counters are read by run_v2sim.py. */
#include <op1/te246.h>
#include <op1/gfx.h>
#include "seq.h"

extern void eh_te_render(void *self, int16_t *out, int16_t *knobs);
extern void eh_te_note_on(void *self, int a1, int a2);
extern void compose_init(void);
extern void compose_engine_name(te_string *out, void *db, const char *sql);
extern int compose_on_key(int key, int down);
extern int compose_on_knob(int knob, int16_t *diff);
extern void compose_frame(uint16_t *buf);
extern void compose_seq_tick(void);
extern int compose_kb_note_on(void *note, int extra);
extern int compose_kb_note_off(void *note);
extern seq_host S;                   /* the sequencer instance (static in the image, symbol kept) */
extern volatile uint8_t open_;
extern volatile uint8_t pp_page;
extern volatile int8_t pp_preset;
#include "purp.h"
extern pu_synth g_pu;

#define INC         ((volatile uint32_t *)0xff801950)
#define VOICE(i)    ((void *)(0xffb00000u + (i) * 0x24))
#define P2(t, v)    ((int16_t)(((t) << 12) | (v)))
#define K(i)        TE_SYNTH_KNOBS[2 * 8 + (i)]  /* sound 3 */
#define FB(n)       ((uint16_t *)(0x03000000 + (n) * 0x40000))
#define PIX(n, x, y) (FB(n)[(319 - (x)) * 256 + (y)])

static int drwave_renders, saves, note_ons, note_offs, keys_to_te;
static int16_t out[128];
volatile uint32_t result_fails, seq_notes_by_mode[6], purp_peak, purp_cycles, seq_cycles, frame_cycles;
volatile int fails;
static int mode_note_ons;

void drwave_render(void *self, int16_t *o, int16_t *k)
{
    int i;
    (void)self; (void)k;
    drwave_renders++;
    for (i = 0; i < 128; i++)
        o[i] = 1234;
}
void drwave_note_on(void *self, int a1, int a2) { (void)self; (void)a1; (void)a2; }
void log_save(void *patch, int16_t *k) { (void)patch; (void)k; saves++; }
void log_note_on(te_note *n, int extra) { (void)n; (void)extra; note_ons++; mode_note_ons++; }
void log_note_off(te_note *n) { (void)n; note_offs++; }

static uint32_t cycles(void)
{
    uint32_t c;
    __asm__ volatile("%0 = CYCLES;" : "=d"(c));
    return c;
}

static int push(int key, int shift)
{
    int r;
    *TE_SHIFT_HELD = (uint8_t)shift;
    r = compose_on_key(key, 1);
    r += compose_on_key(key, 0) << 4;
    *TE_SHIFT_HELD = 0;
    return r;                   /* 0x11 = press and release both consumed */
}
static int turn(int knob, int16_t d, int shift)
{
    int r;
    *TE_SHIFT_HELD = (uint8_t)shift;
    r = compose_on_knob(knob, &d);
    *TE_SHIFT_HELD = 0;
    return r;
}
static void name(te_string *s, const char *n)
{
    int i;
    for (i = 0; n[i]; i++)
        s->bx.sso[i] = n[i];
    s->bx.sso[i] = 0;
    s->size = (uint32_t)i;
    s->cap = 15;
}
static void tick(int n) { while (n--) compose_seq_tick(); }

volatile int fail_lines[8];
#define CHECK(bit, c) do { if (!(c)) { int q_; fails |= 1 << (bit); for (q_ = 0; q_ < 8; q_++) if (!fail_lines[q_]) { fail_lines[q_] = __LINE__; break; } } } while (0)

int main(void)
{
    static const int16_t stock[8] = { 20924, 4313, 8063, 0, 32767, 0, 0, 0 };
    int16_t knobs[8];
    int i, b, ok, pk, m, r;
    uint32_t t0;
    pu_synth *ps;

    __asm__ volatile("R0 = SYSCFG; BITSET (R0, 1); SYSCFG = R0; CSYNC;" ::: "R0");
    compose_init();
    ps = &g_pu;
    *TE_EDIT_MODE = 0;
    *TE_SYNTH_SUBSCREEN = 0;
    *TE_SYNTH_SLOT = 2;
    TE_SYNTH_TYPE[2] = TE_INSTR_DRWAVE;
    *TE_PLAYMODE = 0;
    *TE_PLAYMODE_TARGET = 0;
    *TE_BPM = 120.0f;

    /* 0: stock drwave untouched */
    for (i = 0; i < 8; i++) knobs[i] = stock[i];
    eh_te_render(VOICE(0), out, knobs);
    CHECK(0, drwave_renders == 1 && out[0] == 1234);

    /* 1: tag 1 = THE PURP: audible, bounded, never drwave; 6th TE voice is silent and harmless */
    for (i = 0; i < 4; i++) knobs[i] = K(i) = 8192;
    for (i = 4; i < 8; i++) knobs[i] = K(i) = P2(1, 0);
    for (i = 0; i < 128; i++) INC[i] = (uint32_t)(220.0 / 88200 * 4294967296.0);
    eh_te_note_on(VOICE(0), 0, 0);
    drwave_renders = 0;
    pk = 0;
    for (b = 0; b < 300; b++) {
        eh_te_render(VOICE(0), out, knobs);
        for (i = 0; i < 128; i++) { int a = out[i] < 0 ? -out[i] : out[i]; if (a > pk) pk = a; }
        if (b == 150) { t0 = cycles(); eh_te_render(VOICE(0), out, knobs); purp_cycles = cycles() - t0; b++; }
    }
    purp_peak = (uint32_t)pk;
    CHECK(1, drwave_renders == 0 && pk > 2000 && pk <= 32767);
    eh_te_note_on(VOICE(5), 0, 0);
    ok = 1;
    for (b = 0; b < 4; b++) {
        eh_te_render(VOICE(5), out, knobs);
        for (i = 0; i < 128; i++) ok &= out[i] == 0;
    }
    CHECK(2, ok);

    /* 3: engine list: "purp" -> "drwave"; warp/trio still renamed; look-alikes untouched */
    {
        static te_string a, c, d, e;
        name(&a, "purp"); name(&c, "warp"); name(&d, "purps"); name(&e, "trio");
        compose_engine_name(&a, 0, 0); compose_engine_name(&c, 0, 0); compose_engine_name(&d, 0, 0); compose_engine_name(&e, 0, 0);
        CHECK(3, a.size == 6 && a.bx.sso[0] == 'd' && c.size == 6 && c.bx.sso[0] == 'd' && d.size == 5 && d.bx.sso[0] == 'p'
                 && e.size == 6 && e.bx.sso[0] == 'd');
    }

    /* 4: PURP screen input: T keys are TE's; pushes/turns are ours only on a PURP sound */
    CHECK(4, push(TE_KEY_T1, 0) == 0 && push(TE_KEY_T2, 0) == 0 && push(TE_KEY_T3, 0) == 0 && push(TE_KEY_T4, 0) == 0);
    pp_preset = -1;
    compose_frame(FB(0));                                   /* syncs preset 0 from the sound's knobs */
    CHECK(4, pp_preset == 0 && pp_page == 0);
    CHECK(4, turn(0, 4, 0) == 1);                            /* blue turn: SHAPE A moves */
    {
        int before = ps->p[PU_SHAPE_A];
        turn(0, 4, 0);
        CHECK(4, ps->p[PU_SHAPE_A] > before);
        turn(0, -8, 0); turn(0, -8, 0);
        CHECK(4, ps->p[PU_SHAPE_A] >= 0);
    }
    r = push(59, 0);                                         /* unshifted blue push = GEN mode on SHAPE A */
    CHECK(4, r == 0x11 && ps->gen[PU_SHAPE_A].mode == OP_GEN_DRIFT && ps->gen[PU_SHAPE_A].depth != 0);
    turn(0, 3, 1);                                           /* shift + turn = GEN depth */
    CHECK(4, ps->gen[PU_SHAPE_A].depth > 8000);
    for (i = 0; i < 5; i++) push(59, 0);                     /* DRIFT -> STEP -> LFO1 -> LFO2 -> OFF -> DRIFT */
    CHECK(4, ps->gen[PU_SHAPE_A].mode == OP_GEN_DRIFT);      /* 5 more pushes wrap the cycle */
    CHECK(4, push(59, 1) == 0x11 && pp_page == 1);           /* shift + blue push: next page */
    CHECK(4, push(60, 1) == 0x11 && pp_page == 0);           /* shift + green push: previous page */
    CHECK(4, push(60, 1) == 0x11 && pp_page == 9);           /* wraps to the preset page */
    i = saves;
    turn(0, 1, 0);                                           /* preset scroller: PRESET 1 stored in the knobs, saved */
    CHECK(4, pp_preset == 1 && K(4) == P2(1, 1) && K(7) == P2(1, 0) && saves == i + 1);
    turn(0, -1, 0); turn(0, -1, 0);                          /* wraps to 7 */
    CHECK(4, pp_preset == 7 && K(4) == P2(1, 7));
    turn(0, 1, 0);
    CHECK(4, pp_preset == 0);
    /* the same input on a stock drwave sound is TE's */
    for (i = 0; i < 8; i++) K(i) = stock[i];
    CHECK(4, push(59, 0) == 0 && turn(0, 1, 0) == 0 && push(59, 1) == 0);
    for (i = 0; i < 4; i++) K(i) = 8192;
    for (i = 4; i < 8; i++) K(i) = P2(1, 0);
    /* screenshots: osc page with GEN, matrix page, preset page */
    pp_page = 0;
    compose_frame(FB(0));
    t0 = cycles(); compose_frame(FB(0)); frame_cycles = cycles() - t0;
    pp_page = 7;
    turn(0, 1, 0); turn(1, 2, 0); turn(2, 3, 0);
    compose_frame(FB(1));
    pp_page = 9;
    compose_frame(FB(2));
    CHECK(5, PIX(0, 2, 120) == GFX_RGB(0x12, 0x00, 0x1f));  /* the PURP screen background */
    /* all pages: every knob full range, shift + push GEN, no crash, outputs stay bounded */
    for (m = 0; m < 10; m++) {
        pp_page = (uint8_t)m;
        for (b = 0; b < 4; b++) { turn(b, 100, 0); turn(b, 100, 0); push(59 + b, 0); turn(b, -100, 1); }
        compose_frame(FB(3));
    }
    pk = 0;
    eh_te_note_on(VOICE(1), 0, 0);
    for (b = 0; b < 200; b++) {
        eh_te_render(VOICE(1), out, knobs);
        for (i = 0; i < 128; i++) { int a = out[i] < 0 ? -out[i] : out[i]; if (a > pk) pk = a; }
    }
    CHECK(6, pk <= 32767);

    /* 7: sequencer slot: orange push opens ONLY on the synth sequencer screen with tombola picked */
    *TE_EDIT_MODE = TE_EDIT_MODE_SEQ;
    *TE_SYNTH_SEQ_TYPE = TE_SEQ_FINGER;
    CHECK(7, push(62, 0) == 0 && push(TE_KEY_T1, 0) == 0);
    *TE_SYNTH_SEQ_TYPE = TE_SEQ_TOMBOLA;
    *TE_PLAYMODE = 1;
    CHECK(7, push(62, 0) == 0);
    *TE_PLAYMODE = 0;
    compose_frame(FB(4));
    CHECK(7, PIX(4, 2, 100) != GFX_RGB(8, 0, 16));          /* closed: TE's own screen, untouched */
    CHECK(7, push(TE_KEY_T1, 0) == 0);                       /* closed: T keys are TE's */
    CHECK(7, push(62, 0) == 0x11);                           /* open */
    compose_frame(FB(4));
    CHECK(7, PIX(4, 2, 100) == GFX_RGB(8, 0, 16) && open_);   /* open: OP FREAK SEQ background */
    CHECK(7, push(TE_KEY_T2, 0) == 0x11 && turn(0, 1, 0) == 1);  /* T and knobs are the sequencer's */
    CHECK(7, push(62, 1) == 0x11);                           /* shift + orange: close */
    CHECK(7, !open_);
    CHECK(7, push(TE_KEY_T2, 0) == 0);
    CHECK(7, push(62, 0) == 0x11);                           /* open again */
    *TE_EDIT_MODE = 0;                                       /* leaving the screen closes it */
    CHECK(7, push(TE_KEY_T2, 0) == 0);
    *TE_EDIT_MODE = TE_EDIT_MODE_SEQ;
    CHECK(7, push(TE_KEY_T2, 0) == 0);                       /* still closed after returning */
    CHECK(7, push(62, 0) == 0x11);

    /* 8: all six modes run and make notes through TE's voice note-on; T4 blue push = next mode, orange push = RUN */
    {
        te_note kn;
        seq_host *h = &S;
        int seen = 0;
        push(TE_KEY_T4, 0);
        for (m = 0; m < 6; m++) {
            int mode = h->mode;
            seen |= 1 << mode;
            /* feed MARKOV / TOTAL some keyboard notes (the kb hook's observer) */
            for (i = 0; i < 12; i++) {
                kn.note = 60 + (i * 7) % 12; kn.vel = 100; kn.midi_out = 1; kn.extra = 0;
                compose_kb_note_on(&kn, 0);
                kn.vel = 0;
                compose_kb_note_off(&kn);
            }
            mode_note_ons = 0;
            if (!h->running) push(62, 0);                    /* T4 orange: RUN */
            t0 = cycles(); tick(1); seq_cycles = cycles() - t0;
            tick(689 * 12);                                  /* ~12 s */
            seq_notes_by_mode[mode] = (uint32_t)mode_note_ons;
            if (mode == 4) compose_frame(FB(5));
            CHECK(8, mode_note_ons > 0);
            push(59, 0);                                     /* T4 blue push = next mode */
        }
        CHECK(9, seen == 0x3f);
        push(62, 0);                                         /* RUN -> stop */
        tick(8);
        CHECK(10, !h->running);
        tick(689);
        CHECK(10, note_offs >= note_ons - 8);                /* no stuck notes beyond the ones in flight */
    }
    /* 11: probe overlay still on shift + speech (the v1 + probe features survive the composition) */
    CHECK(11, push(TE_KEY_SPEECH, 1) == 0x11 && push(TE_KEY_SPEECH, 0) == 0);
    push(TE_KEY_SPEECH, 1); push(TE_KEY_SPEECH, 1);          /* back to off */
    (void)keys_to_te;
    result_fails = (uint32_t)fails;
    return fails;
}

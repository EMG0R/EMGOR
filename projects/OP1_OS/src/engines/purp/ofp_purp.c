/* ofp_purp: THE PURP as a playable synth engine (feature glue). sdk_adapter.c (host registration, tag 1) is
 * #included so the whole thing is one object; this file adds the engine-list rename, the UI and the preset sync.
 *
 *   pp_init         init           pu_sdk_init(): op_init(44100), seed the synth, eh_register(1, ...)
 *   pp_engine_name  engine_name    the "purp" row of the engine list becomes a drwave sound (default knobs carry tag 1)
 *   pp_key / pp_knob               engine screen (SYNTH, T1) of a PURP sound only. T1-T4 are NOT touched (T2 = TE's
 *                                  amp envelope, which PURP uses; T3 = FX; T4 = LFO), so TE's sub-screens stay reachable.
 *       turn = value | shift + turn = GEN depth | push = cycle GEN mode of that parameter
 *       shift + blue push = next page | shift + green push = previous page
 *       shift + white push = RESEED (new drift / S&H material) | shift + orange push = FM ratio snap (or MATRIX slot on the MATRIX page)
 *   pp_frame        frame          the PURP screen over y 0..140 of the engine screen
 * Sound identity: knob slots 5-8 = (1 << 12) | preset number (0..7). Edits live in the shared pu_synth (RAM) and are not saved
 * per sound; leaving a PURP sound for another PURP sound reloads that sound's preset.
 * Runs on three TE threads (key, GUI, audio) without locks; every shared field is a small int, array bounds are fixed. */
#include <op1/te246.h>
#include <op1/gfx.h>
#include "sdk_adapter.c"

#define TAG             PU_SDK_TAG
#define KEY_KNOB_PUSH   59
#define PAGES           (PU_SUBPAGES + 1)       /* 9 parameter sub-pages + the preset scroller */
#define PG_MATRIX       7
#define PG_PRESET       (PU_SUBPAGES)
#define STEP            327                     /* ~100 clicks full range */
#define GEN_DEPTH_STEP  512
#define NGEN            5                       /* OFF DRIFT STEP LFO1 LFO2 (TILT / SEQ need platform inputs) */

volatile uint8_t pp_page;                       /* 0..PAGES-1 (tests read) */
volatile int8_t pp_preset = -1;                 /* preset loaded into the shared synth, -1 none */
static volatile int8_t synced_slot = -1;
static volatile uint8_t consumed, edited;
static uint32_t reseeds;

static const char *const curve_names[MXC_COUNT] = { "LIN", "EXP", "LOG", "S" };
static const uint8_t gen_modes[NGEN] = { OP_GEN_OFF, OP_GEN_DRIFT, OP_GEN_STEP, OP_GEN_LFO1, OP_GEN_LFO2 };

static int marked(const volatile int16_t *k) { return eh_tag(k) == TAG; }

static volatile int16_t *current(int *slot)
{
    *slot = *TE_SYNTH_SLOT & 7;
    return TE_SYNTH_KNOBS + *slot * 8;
}

static int engine_screen_purp(void)
{
    int slot;

    return *TE_EDIT_MODE == 0 && *TE_SYNTH_SUBSCREEN == 0
           && TE_SYNTH_TYPE[*TE_SYNTH_SLOT & 7] == TE_INSTR_DRWAVE && marked(current(&slot));
}

void pp_init(void)
{
    pu_sdk_init();
}

void pp_engine_name(te_string *out, void *db, const char *sql)
{
    char *d = (char *)te_str_data(out);

    (void)db;
    (void)sql;
    if (out->size == 4 && d[0] == 'p' && d[1] == 'u' && d[2] == 'r' && d[3] == 'p') {
        d[0] = 'd'; d[1] = 'r'; d[2] = 'w'; d[3] = 'a'; d[4] = 'v'; d[5] = 'e'; d[6] = 0;
        out->size = 6;
        pp_page = 0;
        synced_slot = -1;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load_preset(volatile int16_t *k, int slot, int n, int save)
{
    n = clampi(n, 0, PU_PRESETS - 1);
    pu_preset(&g_pu, n);
    pp_preset = (int8_t)n;
    synced_slot = (int8_t)slot;
    edited = 0;
    if (save) {
        k[4] = eh_p2(TAG, n);
        TE_PATCH_SET_KNOBS(TE_SYNTH_PATCH(slot), (int16_t *)k);
    }
}

/* GUI thread: follow the selected PURP sound (slot change / first sight) */
static void sync_sound(void)
{
    int slot;
    volatile int16_t *k;

    if (TE_SYNTH_TYPE[*TE_SYNTH_SLOT & 7] != TE_INSTR_DRWAVE)
        return;
    k = current(&slot);
    if (!marked(k) || synced_slot == slot)
        return;
    load_preset(k, slot, k[4] & EH_P2_MAX, 0);
}

/* ---- parameter access (the MATRIX page edits slot g_pu.mx_active directly; p[PU_MX_*] are unused by the core) */
static int get_val(int id)
{
    pu_mx_slot *m = &g_pu.mx[g_pu.mx_active];

    switch (id) {
    case PU_MX_SRC: return m->src;
    case PU_MX_DST: return m->dst;
    case PU_MX_AMT: return m->amt;
    case PU_MX_CURVE: return m->curve;
    default: return g_pu.p[id];
    }
}

static int zone_of(int v, int n) { int z = (v * n) >> 15; return z >= n ? n - 1 : z; }
static int zone_centre(int z, int n) { return ((2 * z + 1) * 32768) / (2 * n); }

static void set_val(int id, int d)
{
    pu_mx_slot *m = &g_pu.mx[g_pu.mx_active];
    int v;

    switch (id) {
    case PU_MX_SRC:
        m->src = (int16_t)zone_centre(clampi(zone_of(m->src, MXS_COUNT) + d, 0, MXS_COUNT - 1), MXS_COUNT); break;
    case PU_MX_DST:
        m->dst = (int16_t)zone_centre(clampi(zone_of(m->dst, MXD_COUNT) + d, 0, MXD_COUNT - 1), MXD_COUNT); break;
    case PU_MX_CURVE:
        m->curve = (int16_t)zone_centre(clampi(zone_of(m->curve, MXC_COUNT) + d, 0, MXC_COUNT - 1), MXC_COUNT); break;
    case PU_MX_AMT:
        m->amt = (int16_t)clampi(m->amt + d * STEP, 0, 32767); break;
    default:
        v = clampi(g_pu.p[id] + d * STEP, 0, 32767);
        g_pu.p[id] = (int16_t)v;
    }
}

int pp_key(int key, int down)
{
    int b = key - KEY_KNOB_PUSH;

    if (b < 0 || b > 3)
        return 0;
    if (!down) {
        if (consumed & (1 << b)) {
            consumed &= (uint8_t)~(1 << b);
            return 1;
        }
        return 0;
    }
    if (!engine_screen_purp())
        return 0;
    consumed |= (uint8_t)(1 << b);
    if (*TE_SHIFT_HELD) {
        if (b == 0) pp_page = (uint8_t)(pp_page + 1 < PAGES ? pp_page + 1 : 0);
        else if (b == 1) pp_page = (uint8_t)(pp_page ? pp_page - 1 : PAGES - 1);
        else if (b == 2) { pu_reseed(&g_pu, g_pu.seed * 1664525u + 1013904223u + ++reseeds); }
        else if (pp_page == PG_MATRIX) g_pu.mx_active = (uint8_t)((g_pu.mx_active + 1) % PU_MX_SLOTS);
        else pu_set_snap(&g_pu, !g_pu.snap);
    } else if (pp_page < PG_PRESET && pp_page != PG_MATRIX) {   /* push = cycle this parameter's GEN mode */
        int id = pp_page * 4 + b, i, cur = 0;
        op_gen *g = &g_pu.gen[id];

        for (i = 0; i < NGEN; i++)
            if (gen_modes[i] == g->mode) cur = i;
        cur = (cur + 1) % NGEN;
        g->mode = gen_modes[cur];
        if (g->mode != OP_GEN_OFF && g->depth == 0) g->depth = 8000;
        edited = 1;
    }
    return 1;
}

int pp_knob(int knob, int16_t *diff)
{
    int slot, d;
    volatile int16_t *k;

    if ((unsigned int)knob > 3 || !engine_screen_purp())
        return 0;
    d = *diff > 0 ? (*diff > 8 ? 8 : *diff) : (*diff < -8 ? -8 : *diff);
    if (!d)
        return 1;
    if (pp_page == PG_PRESET) {
        if (knob == 0) {
            k = current(&slot);
            load_preset(k, slot, ((pp_preset < 0 ? 0 : pp_preset) + (d > 0 ? 1 : -1) + PU_PRESETS) % PU_PRESETS, 1);
        }
        return 1;
    }
    {
        int id = pp_page * 4 + knob;

        edited = 1;
        if (*TE_SHIFT_HELD) {                   /* GEN depth (bipolar) of this parameter; MATRIX has none */
            if (pp_page != PG_MATRIX) {
                op_gen *g = &g_pu.gen[id];

                g->depth = (int16_t)clampi(g->depth + d * GEN_DEPTH_STEP, -32767, 32767);
                if (g->mode == OP_GEN_OFF && g->depth != 0) g->mode = OP_GEN_DRIFT;
            }
        } else {
            set_val(id, d);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ GUI thread */
void pp_frame(uint16_t *buf)
{
    static const uint16_t col[4] = {
        GFX_RGB(0x69, 0x8e, 0xff), GFX_RGB(0x00, 0xed, 0x95),
        GFX_RGB(0xe6, 0xe6, 0xe6), GFX_RGB(0xff, 0x6a, 0x3d),
    };
    static const uint16_t dim = GFX_RGB(0x5a, 0x58, 0x66), track = GFX_RGB(0x35, 0x32, 0x38),
                          purp = GFX_RGB(0xb4, 0x6e, 0xff);
    static const char *const gtag[OP_GEN_MODES] = { "", "DRIFT", "STEP", "LFO1", "LFO2", "TLT X", "TLT Y", "SEQ" };
    int x, y, i, pg = pp_page;

    sync_sound();
    if (!engine_screen_purp())
        return;
    gfx_begin(buf);
    gfx_rect(0, 0, gfx_w, 140, GFX_RGB(0x12, 0x00, 0x1f));
    x = gfx_text(4, 4, "THE PURP", purp, 2);
    if (pp_preset >= 0) {
        x = gfx_text(x + 8, 4, pu_preset_names[(int)pp_preset], GFX_WHITE, 2);
        if (edited) gfx_text(x, 4, "*", dim, 2);
    }
    gfx_text(250, 4, "P", dim, 2);
    gfx_dec(262, 4, pg + 1, GFX_WHITE, 2);
    gfx_text(274, 4, "/", dim, 2);
    gfx_dec(286, 4, PAGES, dim, 2);
    if (pg == PG_PRESET) {
        gfx_text(4, 26, "PRESETS  (BLUE TURN)", col[0], 1);
        for (i = 0; i < PU_PRESETS; i++) {
            x = 4 + (i & 1) * 156;
            y = 40 + (i >> 1) * 16;
            gfx_text(x, y, pu_preset_names[i], i == pp_preset ? GFX_WHITE : dim, 1);
        }
        gfx_text(4, 116, "SHIFT+BLUE/GREEN PUSH: PAGE", dim, 1);
        return;
    }
    x = gfx_text(4, 24, pu_subpage_names[pg], purp, 2);
    if (pg == PG_MATRIX) {
        gfx_text(x + 8, 24, "SLOT", dim, 1);
        gfx_dec(x + 40, 24, g_pu.mx_active + 1, GFX_WHITE, 1);
    }
    for (i = 0; i < 4; i++) {
        int id = pg * 4 + i, v = get_val(id), n;
        const char *txt = 0;

        x = 4 + i * 80;
        gfx_text(x, 54, pu_info[id].name, col[i], 1);
        gfx_rect(x, 68, 72, 6, track);
        if (pg == PG_MATRIX && i == 0) txt = pu_mx_src_names[zone_of(v, MXS_COUNT)];
        else if (pg == PG_MATRIX && i == 1) txt = pu_mx_dst_names[zone_of(v, MXD_COUNT)];
        else if (pg == PG_MATRIX && i == 3) txt = curve_names[zone_of(v, MXC_COUNT)];
        else if (!(pg == PG_MATRIX && i == 3)) gfx_rect(x, 68, v * 72 / 32767, 6, col[i]);
        if (txt) {
            gfx_text(x, 82, txt, GFX_WHITE, 1);
        } else {
            n = (pg == PG_MATRIX && i == 2) ? (v - 16384) * 100 / 16384 : v * 100 / 32767;
            gfx_dec(x, 82, n, GFX_WHITE, 1);
        }
        if (pg != PG_MATRIX) {
            const op_gen *g = &g_pu.gen[id];

            if (g->mode != OP_GEN_OFF) {
                int dp = g->depth * 100 / 32767;
                gfx_text(x, 98, gtag[g->mode], purp, 1);
                gfx_dec(x + 40, 98, dp, dim, 1);
            }
        }
    }
    if (pg == 3) gfx_text(4, 110, "NO EFFECT HERE: TE ENVELOPE (T2) SHAPES THE AMP", dim, 1);
    if (pg == 8) gfx_text(4, 110, "VOICES + GLIDE: NO EFFECT, TE PLAYS THE VOICES", dim, 1);
    gfx_text(4, 124, pg == PG_MATRIX ? "PUSH N/A  SHIFT+ORANGE: SLOT" : "PUSH: GEN MODE  SHIFT+TURN: DEPTH", dim, 1);
}

// nav.cpp — the interface state machine. See nav.h for the one-rule-set summary
// and PMOR-V3-SPEC.md §1-§8 for the full behaviour this implements.
#include "nav.h"
#include <math.h>
#include <stdio.h>
#include "daisy_seed.h"
extern "C" {
#include "../usb/clock_engine.h"
}
#include "input.h"
#include "render.h"                 // g_trellis_brightness
#include "../config.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/clock.h"
#include "transport_link.h"   // transportTap(): a tap is an event, routed by clock_mode
#include "../pmor_core/scale.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/sampler.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/output.h"
#include "../pmor_core/metro.h"
#include "../store/patch.h"
#include "../hw/studio.h"        // manual clock mode (builder 1)
#include "store_hooks.h"
#include "../hw/hw.h"                // trellisSetRotation
#include "../pmor_core/engine_util.h"   // clampf
#include <string.h>

NavState nav;
const char* const MENU_NAMES[M_COUNT] = { "DRUMS","SYNTH","SAMPLER","FX","PATCHES","GLOBALS","DASH" };
const char* const CARD_NAMES[C_COUNT] = { "TIMBRE","SEQ","PERFORM" };

// weak: store/patch.cpp may not exist yet, or may never provide one — guarded call
extern void patchFactoryReset() __attribute__((weak));

// ==================================================================== helpers
static inline bool   isEngineMenu(Menu m) { return m == M_DRUMS || m == M_SYNTH || m == M_SMP; }
static inline Engine engineOf(Menu m)     { return m == M_DRUMS ? ENG_DRUMS : (m == M_SYNTH ? ENG_SYNTH : ENG_SMP); }
static inline bool   isClearItem(const Param& p)   { return p.name && strcmp(p.name, "CLEAR") == 0; }
static inline bool   isPadCountItem(const Param& p) {
    return p.name && (strcmp(p.name, "BARS") == 0 || strcmp(p.name, "STEPS") == 0);
}
// true when items[idx] is the Y half of a PF_PAIR_X pair (not independently selectable)
static inline bool isPairY(const ParamPage& pg, uint8_t idx) {
    return idx > 0 && idx < pg.count && (pg.items[idx - 1].flags & PF_PAIR_X);
}
// moves nav.cursor by `dir` (±1), hard-stopped, skipping the Y half of a pair
static void moveCursor(const ParamPage& pg, int8_t dir) {
    if(pg.count == 0 || dir == 0) return;
    int c = (int)nav.cursor + dir;
    if(c < 0) c = 0;
    if(c > pg.count - 1) c = pg.count - 1;
    if(isPairY(pg, (uint8_t)c)) {
        int c2 = c + dir;
        if(c2 >= 0 && c2 <= pg.count - 1) c = c2;
    }
    nav.cursor = (uint8_t)c;
}

// ==================================================================== GLOBALS shadow params
// Param.value must be a float*; several Globals fields aren't float (or aren't
// stored anywhere at all, e.g. ROTATE), so every GLOBALS item edits a local
// shadow float here and navTick() diffs shadow-vs-applied to push it live.
static void toast(const char* t);
static float s_g_retry, s_ga_retry;     // RETRYSTO: momentary, only offered after a boot that skipped storage
static float s_g_key, s_g_scale, s_g_bpm, s_g_ingain, s_g_bright, s_g_clock, s_g_rotate, s_g_swing, s_g_gendiv, s_g_factory, s_g_clkmode;
static float s_ga_key, s_ga_scale, s_ga_bpm, s_ga_ingain, s_ga_bright, s_ga_clock, s_ga_rotate, s_ga_swing, s_ga_gendiv, s_ga_factory, s_ga_clkmode;

static const char* const CLOCK_MODE_NAMES[6] = { "SY0","SY1","SY2","SY3","SY4","SY5" };
static const char* const CLKMODE_NAMES[4]    = { "AUTO","INTERNAL","FOLLOW","MASTER" };   // studio.h: 0 AUTO 1 INTERNAL 2 FOLLOW 3 MASTER
static const char* const ROTATE_NAMES[4]     = { "0","90","180","270" };
static const char* const YESNO_NAMES[2]      = { "NO","YES" };

static const Param GLOBAL_PARAMS[] = {
    { "KEY",     &s_g_key,     -12.f, 12.f, 0.01f, (uint8_t)PF_BIPOLAR,             nullptr },
    { "SCALE",   &s_g_scale,     0.f, (float)(SCALE_MODES - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), SCALE_MODE_NAMES },
    { "BPM",     &s_g_bpm,      30.f, 300.f, 1.f, (uint8_t)PF_QUANT,                nullptr },
    { "INGAIN",  &s_g_ingain,    0.f, 2.f,  0.01f, (uint8_t)PF_PERCENT,             nullptr },
    { "BRIGHT",  &s_g_bright,    0.f, 1.f,  0.01f, (uint8_t)PF_PERCENT,             nullptr },
    // OLEDBRT removed (2026-10): the OLED contrast control did nothing useful for the user —
    // gone, not hidden. See store/patch.h / store/patch.cpp for the Globals-struct side of
    // this (GLOB_VERSION bumped so old QSPI blobs don't deserialize garbage into the fields
    // that moved). hw/oled.cpp's oledSetContrast() primitive is left in place (not owned
    // here) — nothing in the UI/GLOBALS path calls it any more; report to hw/ owner that it's
    // now dead code they can remove.
    { "CLOCK",   &s_g_clock,     0.f, 5.f,   1.f, (uint8_t)(PF_QUANT | PF_ENUM),    CLOCK_MODE_NAMES },
    // CLKMODE: the manual STUDIO clock mode (AUTO follows Live whenever a host is connected; INTERNAL ignores it;
    // FOLLOW always slaves to USB MIDI clock; MASTER drives it). Persisted by hw/studio.cpp (own QSPI record).
    { "CLKMODE", &s_g_clkmode,   0.f, 3.f,   1.f, (uint8_t)(PF_QUANT | PF_ENUM),    CLKMODE_NAMES },
    { "ROTATE",  &s_g_rotate,    0.f, 3.f,   1.f, (uint8_t)(PF_QUANT | PF_ENUM),    ROTATE_NAMES },
    { "SWING",   &s_g_swing,     0.f, 1.f,  0.01f, (uint8_t)PF_PERCENT,             nullptr },
    // GENRATE: the rate the GENERATIVE sequencer rolls its dice at, independent of the
    // 16th grid. PLAY mode keeps its own ARPRATE -- different job, see engine/sequencer.cpp.
    { "GENRATE", &s_g_gendiv,    0.f, (float)(GEN_DIV_COUNT - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), kGenDivNames },
    // METRO: level of the quiet play-mode click (engine/metro.h). Edits the engine value directly (no
    // shadow needed -- nothing has to be recomputed on change). 0 = off. Not persisted (runtime, default quiet).
    { "METRO",   &metro.level,   0.f, 1.f,  0.01f, (uint8_t)PF_PERCENT,             nullptr },
    { "FACTORY", &s_g_factory,   0.f, 1.f,   1.f, (uint8_t)(PF_QUANT | PF_ENUM),    YESNO_NAMES },
};
static const Param GLOBAL_PARAMS_RETRY[] = {      // same list + RETRYSTO, used only while patchStoreSkipped()
    GLOBAL_PARAMS[0], GLOBAL_PARAMS[1], GLOBAL_PARAMS[2], GLOBAL_PARAMS[3], GLOBAL_PARAMS[4], GLOBAL_PARAMS[5], GLOBAL_PARAMS[6],
    GLOBAL_PARAMS[7], GLOBAL_PARAMS[8], GLOBAL_PARAMS[9], GLOBAL_PARAMS[10], GLOBAL_PARAMS[11],
    { "RETRYSTO", &s_g_retry,    0.f, 1.f,   1.f, (uint8_t)(PF_QUANT | PF_ENUM),    YESNO_NAMES },
};
static const ParamPage GLOBAL_PAGE_RETRY = { "GLOBALS", GLOBAL_PARAMS_RETRY, sizeof(GLOBAL_PARAMS_RETRY) / sizeof(GLOBAL_PARAMS_RETRY[0]) };
static const ParamPage GLOBAL_PAGE = { "GLOBALS", GLOBAL_PARAMS, sizeof(GLOBAL_PARAMS) / sizeof(GLOBAL_PARAMS[0]) };

static void globalsRefreshShadow() {
    s_g_key    = s_ga_key    = scale.key_semis;
    s_g_scale  = s_ga_scale  = (float)scale.mode;
    s_g_bpm    = s_ga_bpm    = clk.bpm;
    s_g_ingain = s_ga_ingain = outp.input_gain;
    // TASK 9: read back the LINEAR control value, not g_trellis_brightness (which is now
    // gamma-corrected — see render.h) — otherwise re-opening GLOBALS would show a dimmer %
    // than what you actually set.
    s_g_bright = s_ga_bright = g_trellis_brightness_ctrl;
    s_g_clock  = s_ga_clock  = (float)glob.clock_mode;
    s_g_swing  = s_ga_swing  = clk.swing;
    s_g_gendiv = s_ga_gendiv = (float)clk.gen_div_idx;
    s_g_clkmode = s_ga_clkmode = (float)studioClockMode();
    s_g_retry = s_ga_retry = 0.f;
    s_g_factory = s_ga_factory = 0.f;            // momentary confirm, never reflects stored state
    // ROTATE has no backing store to read from; keep whatever was last applied (default 0)
    s_g_rotate = s_ga_rotate;
}

// diff shadow-vs-last-applied and push each change into the real engines, live
static void globalsApply() {
    if(s_g_key != s_ga_key)       { scale.key_semis = s_g_key; scaleRebuild(); s_ga_key = s_g_key; }
    if(s_g_scale != s_ga_scale)   { scale.mode = (uint8_t)s_g_scale; scaleRebuild(); s_ga_scale = s_g_scale; }
    if(s_g_bpm != s_ga_bpm)       { clockSetBpm(s_g_bpm); s_ga_bpm = s_g_bpm; }
    if(s_g_ingain != s_ga_ingain) { outp.input_gain = s_g_ingain; s_ga_ingain = s_g_ingain; }
    if(s_g_bright != s_ga_bright) {
        // TASK 9 (2026-10): write BOTH the linear control (what persists / what the editor
        // shows) and the gamma-corrected scale (what hw/trellis.cpp's single multiply actually
        // uses) — live, in the same tick as the edit. See render.h for why these are two
        // variables and not one.
        g_trellis_brightness_ctrl = s_g_bright;
        g_trellis_brightness = trellisBrightnessGamma(s_g_bright);
        s_ga_bright = s_g_bright;
    }
    if(s_g_clock != s_ga_clock)   { glob.clock_mode = (uint8_t)s_g_clock; clock_engine_set_sync_mode((sync_mode_t)glob.clock_mode); globalsSave(); s_ga_clock = s_g_clock; }
    if(s_g_clkmode != s_ga_clkmode) { studioSetClockMode((uint8_t)s_g_clkmode); s_ga_clkmode = s_g_clkmode; }
    if(s_g_rotate != s_ga_rotate) { trellisSetRotation((uint8_t)s_g_rotate); s_ga_rotate = s_g_rotate; }
    if(s_g_swing != s_ga_swing)   { clk.swing = s_g_swing; s_ga_swing = s_g_swing; }
    // NOT a bare field write like swing: clockSetGenDiv() recomputes gen_step_q and
    // re-anchors the generative accumulator to the master grid. Assigning gen_div_idx
    // directly would change the label and nothing else.
    if(s_g_gendiv != s_ga_gendiv) { clockSetGenDiv((uint8_t)s_g_gendiv); s_ga_gendiv = s_g_gendiv; }
    if(s_g_retry != s_ga_retry) {
        s_ga_retry = s_g_retry;
        if(s_g_retry > 0.5f) {
            int r = patchStoreRetry();
            toast(r == 0 ? "STORAGE RETRY" : "RETRY FAILED");
            s_g_retry = s_ga_retry = 0.f;
            if(r == 0) { nav.level = L_CARD; nav.cursor = 0; nav.edit = nullptr; }   // the item disappears: don't leave the cursor past the list
        }
    }
    if(s_g_factory != s_ga_factory) {
        if(s_g_factory > 0.5f && patchFactoryReset) patchFactoryReset();
        s_ga_factory = s_g_factory;
    }
}

// ==================================================================== SYNTH timbre pages
static const ParamPage EMPTY_PAGE = { "", nullptr, 0 };
static inline const ParamPage& safePage(const ParamPage* p) { return p ? *p : EMPTY_PAGE; }

static float    s_syn_algo_shadow = 0.f, s_syn_algo_applied = 0.f;
static Param    s_syn_sound[7];          // SOUND page: p1..p4 KEY VOL ALGO  (KEY [4] and ALGO [6] are shadows: navTick special-cases them)
static Param    s_syn_motion[5];
static Param    s_syn_env[4];            // ENV page, flat: RELEASE ATTACK DECAY SUSTAIN (no pairs: RELEASE is item 0, its own slider)         // MOTION page: SEMIS GLIDEIN GLIDEOUT WIDTH WIDTHPRB
static ParamPage s_synth_pages[4];

static ParamPage s_drum_pages[2];
static ParamPage s_smp_pages[2];
static ParamPage s_single_page_buf;
static ParamPage s_single_page_arr[1];

static inline bool synEditing(const Param* which) { return nav.level == L_EDIT1 && nav.edit == which; }

const ParamPage* navTimbrePages(Engine e, uint8_t& count) {
    if(e == ENG_SYNTH) {
        // SYNTH TIMBRE: four pages (was seven), ordered by how often they are touched:
        //   SOUND  (the algorithm's four knobs, key, volume, algo) / FILTER / ENV / MOTION (pitch glide + width)
        const ParamPage* ap = synAlgoParamPage(syn.algo);
        for(int i = 0; i < 4; i++) { s_syn_sound[i] = ap->items[i]; s_syn_sound[i].flags &= (uint8_t)~PF_PAIR_X; }   // the algorithm's own 4 knobs first: one click from the ring
        // KEY mirror (EMGOR 2026-09-15): the GLOBAL key, editable from the synth page. Same shadow as GLOBALS -> globalsApply().
        if(!synEditing(&s_syn_sound[4])) s_g_key = s_ga_key = scale.key_semis;
        s_syn_sound[4] = { "KEY", &s_g_key, -12.f, 12.f, 0.01f, (uint8_t)PF_BIPOLAR, nullptr };
        s_syn_sound[5] = { "VOL", &syn.volume, 0.f, ENGINE_VOL_MAX, 0.01f, (uint8_t)PF_PERCENT, nullptr };
        // ALGO last: it is normally chosen in the sound browser (joystick U/D on the ring, then L/R), this is the numeric fallback
        if(!synEditing(&s_syn_sound[6])) s_syn_algo_shadow = (float)syn.algo;   // don't clobber the shadow while it is the live edit target
        s_syn_sound[6] = { "ALGO", &s_syn_algo_shadow, 0.f, (float)(ALG_COUNT - 1), 1.f,
                           (uint8_t)(PF_QUANT | PF_ENUM), SYN_ALGO_NAMES };
        const ParamPage* pp = synPitchPage();   // SEMIS(X) GLIDEIN(Y) GLIDEOUT
        const ParamPage* wp = synWidthPage();   // WIDTH(X) WIDTHPRB(Y)
        for(int i = 0; i < 3; i++) s_syn_motion[i] = pp->items[i];
        for(int i = 0; i < 2; i++) s_syn_motion[3 + i] = wp->items[i];
        s_synth_pages[0] = { "SOUND",  s_syn_sound, 7 };
        s_synth_pages[1] = safePage(synFilterPage());
        const ParamPage* ep = synEnvPage();      // ATTACK DECAY SUSTAIN RELEASE (pairs in the schema); here flat, release first
        { static const uint8_t order[4] = { 3, 0, 1, 2 };
          for(int i = 0; i < 4; i++) { s_syn_env[i] = ep->items[order[i]]; s_syn_env[i].flags &= (uint8_t)~PF_PAIR_X; } }
        s_synth_pages[2] = { "ENV", s_syn_env, 4 };
        s_synth_pages[3] = { "MOTION", s_syn_motion, 5 };
        count = 4;
        return s_synth_pages;
    }
    if(e == ENG_DRUMS) {
        if(nav.focus_pad >= 0) {
            s_single_page_buf = safePage(drumsPadPage((uint8_t)nav.focus_pad));
            s_single_page_arr[0] = s_single_page_buf;
            count = 1;
            return s_single_page_arr;
        }
        // unfocused: MIX (group volumes) and the depth knobs of the engine of the last-played pad
        s_drum_pages[0] = safePage(drumsMixPage());
        s_drum_pages[1] = safePage(drumsEnginePage((uint8_t)drumPadGroup(nav.seq_target[ENG_DRUMS] & 15)));
        count = 2;
        return s_drum_pages;
    }
    if(e == ENG_SMP) {
        if(nav.focus_pad >= 0) {
            s_single_page_buf = safePage(samplerSlotPage((uint8_t)nav.focus_pad));
            s_single_page_arr[0] = s_single_page_buf;
            count = 1;
            return s_single_page_arr;
        }
        // unfocused: page 0 = the sample POOL browser (custom screen, no Param list), page 1 = MIX
        s_smp_pages[0] = { "POOL", nullptr, 0 };
        s_smp_pages[1] = safePage(samplerMixPage());
        count = 2;
        return s_smp_pages;
    }
    count = 0;
    return nullptr;
}

// ==================================================================== SEQ card page
// seqParamPage(e) (engine/sequencer.cpp) already includes exactly one BARS and one
// STEPS item per engine (pointing at LaneSet::bars_f/steps_f, kept in sync by
// seqSetBars/seqSetStepsPerBar). Cross-module fix: this used to ALSO append its own
// shadow BARS/STEPS pair on top of that, showing each twice in the SEQ list — just
// pass the page straight through.
static inline const ParamPage* buildSeqPage(Engine e) { return seqParamPage(e); }

// ==================================================================== navCurrentPage
const ParamPage* navCurrentPage() {
    switch(nav.menu) {
        case M_DRUMS: case M_SYNTH: case M_SMP: {
            Engine e = engineOf(nav.menu);
            if(nav.card == C_TIMBRE) {
                uint8_t cnt = 0;
                const ParamPage* pages = navTimbrePages(e, cnt);
                if(!pages || cnt == 0) return nullptr;
                uint8_t pg = (nav.page < cnt) ? nav.page : (uint8_t)(cnt - 1);
                return &pages[pg];
            }
            if(nav.card == C_SEQ) return buildSeqPage(e);
            return nullptr;   // PERF: no list, joystick is performance
        }
        case M_GLOBALS: return patchStoreSkipped() ? &GLOBAL_PAGE_RETRY : &GLOBAL_PAGE;
        case M_FX: case M_PATCHES: case M_DASH: default: return nullptr;
    }
}

// ==================================================================== both-press undo bookkeeping
static NavLevel s_pre_joy_level;
static Menu     s_pre_joy_menu;
static Card     s_pre_joy_card;
static uint8_t  s_pre_joy_cursor;
static int8_t   s_pre_joy_focus;
static uint8_t  s_pre_joy_target[ENG_COUNT];
static NavLevel s_return_level = L_RING;   // where L_VOLUME closes back to
// Pad latch: what each physical pad's PRESS started, so its RELEASE always reaches that target no matter what the menu, level, card or
// FX bank did in between (a release used to be routed by the *current* menu/level and was dropped under the volume overlay, the step
// editor, the browser, or after a menu change: FX held forever, synth note gated forever, sampler recording forever).
// FX also remembers the effect id (the stick may have moved fx.view since the press).
enum PadAct : uint8_t { PA_NONE = 0, PA_FX, PA_SYN, PA_SMPREC };
static uint8_t  s_pad_fx_id[16] = {0};
static uint8_t  s_pad_act[16] = {0};

static void padLatchRelease(uint8_t pad) {
    uint8_t a = s_pad_act[pad & 15]; s_pad_act[pad & 15] = PA_NONE;
    if(a == PA_FX) fxRelease(s_pad_fx_id[pad & 15]);
    else if(a == PA_SYN) synthNoteOff(pad);
    else if(a == PA_SMPREC) { if(smp.rec_slot == (int8_t)pad) samplerRecordStop(); }
}
// A pad whose press is latched but that is no longer physically down (its release event was dropped, e.g. input ring full): release it.
static void padLatchReconcile() {
    for(uint8_t p = 0; p < 16; p++) if(s_pad_act[p] && !((g.pads_held >> p) & 1u)) padLatchRelease(p);
}

static void closeVolume() {
    nav.level = s_return_level;
    nav.vol_overlay = false;
}

// ==================================================================== toast
static void toast(const char* t) {
    strncpy(nav.toast, t, sizeof nav.toast - 1); nav.toast[sizeof nav.toast - 1] = 0;
    nav.toast_until = daisy::System::GetNow() + 1500u;
}

// ==================================================================== sampler pool browser
// Every sample on the card, from every patch (id = patch*16 + slot, store/patch.h). The list is gathered ONE patch per
// navTick pass while the page is open (each patchPoolScan is a small SD read: never in the audio callback, never a
// long stall). Auditioning a pool sample would need an SD read per cursor move; not done -- assign it and play the pad.
#define POOL_MAX 1024
struct PoolEnt { uint16_t id; uint32_t len; };
static PoolEnt SDRAM_BSS s_pool[POOL_MAX];
static uint16_t s_pool_n = 0;
static uint8_t  s_pool_next = 0;       // next patch to scan; PATCH_SLOTS = done
static bool     s_pool_open = false;
static bool     s_assign_wait = false;

static inline bool inPool() {
    return nav.menu == M_SMP && nav.card == C_TIMBRE && nav.level == L_CARD && nav.focus_pad < 0 && nav.page == 0;
}
uint16_t navPoolCount() { return s_pool_n; }
bool navPoolScanning() { return s_pool_open && s_pool_next < PATCH_SLOTS; }
bool navPoolPageOpen() { return s_pool_open; }
bool navPoolEntry(uint16_t i, uint8_t& patch, uint8_t& slot, uint32_t& len) {
    if(i >= s_pool_n) return false;
    patch = (uint8_t)(s_pool[i].id / 16); slot = (uint8_t)(s_pool[i].id % 16); len = s_pool[i].len; return true;
}
// Audition: the cursor settling on an entry for POOL_AUD_DEBOUNCE_MS queues a background read of that sample into the sampler's
// scratch buffer, POOL_AUD_CHUNK samples per main-loop pass (SD reads happen here, never in the audio callback). It plays once the
// last chunk is in. Moving the cursor stops playback and aborts a load at once, so fast scrolling reads nothing until it rests.
#define POOL_AUD_DEBOUNCE_MS 250
#define POOL_AUD_CHUNK       1024
static uint16_t s_aud_cur = 0xFFFF;     // entry the debounce/load belongs to
static uint32_t s_aud_since = 0;        // when the cursor last moved
static enum { AUD_IDLE, AUD_WAIT, AUD_LOAD, AUD_DONE } s_aud_state = AUD_IDLE;
static uint32_t s_aud_pos = 0, s_aud_len = 0;
static uint16_t s_aud_id = 0;
static void audStop() { samplerAuditionStop(); s_aud_state = AUD_IDLE; s_aud_cur = 0xFFFF; }
static void audTick(uint32_t now) {
    if(!s_pool_open || s_pool_n == 0 || s_assign_wait) { if(s_aud_state != AUD_IDLE) audStop(); return; }
    if(nav.pool_cur != s_aud_cur) {                         // cursor moved: stop, restart the debounce
        samplerAuditionStop();
        s_aud_cur = nav.pool_cur; s_aud_since = now; s_aud_state = AUD_WAIT;
        return;
    }
    if(s_aud_state == AUD_WAIT) {
        if(s_pool_next < PATCH_SLOTS) return;               // still scanning the card: don't compete for the SD
        if((int32_t)(now - s_aud_since) < POOL_AUD_DEBOUNCE_MS) return;
        s_aud_id = s_pool[s_aud_cur].id; s_aud_len = s_pool[s_aud_cur].len; if(s_aud_len > SMP_LEN) s_aud_len = SMP_LEN;
        s_aud_pos = 0; s_aud_state = AUD_LOAD;
    }
    if(s_aud_state == AUD_LOAD) {
        uint32_t n = s_aud_len - s_aud_pos; if(n > POOL_AUD_CHUNK) n = POOL_AUD_CHUNK;
        int32_t got = patchPoolRead(s_aud_id, s_aud_pos * 2u, (uint8_t*)(samplerAuditionBuf() + s_aud_pos), n * 2u);
        if(got != (int32_t)(n * 2u)) { s_aud_state = AUD_DONE; return; }       // unreadable: give up silently
        s_aud_pos += n;
        if(s_aud_pos >= s_aud_len) { samplerAuditionPlay(s_aud_len); s_aud_state = AUD_DONE; }
    }
}
static void poolTick(uint32_t now) {
    bool want = inPool();
    if(want && !s_pool_open) { s_pool_open = true; s_pool_n = 0; s_pool_next = 0; nav.pool_cur = 0; nav.pool_armed = false; }
    if(!want && s_pool_open) { s_pool_open = false; nav.pool_armed = false; audStop(); }
    if(s_pool_open && s_pool_next < PATCH_SLOTS) {
        uint8_t p = s_pool_next++;
        if(patchExists(p) && patchPoolScan(p) > 0) {
            for(uint8_t sl = 0; sl < SMP_SLOTS && s_pool_n < POOL_MAX; sl++) {
                uint32_t len = patchPoolLen(p, sl);
                if(len > 0) { s_pool[s_pool_n].id = (uint16_t)(p * 16 + sl); s_pool[s_pool_n].len = len; s_pool_n++; }
            }
        }
    }
    audTick(now);
    if(s_assign_wait) {
        uint8_t sl; int err;
        if(patchAssignPoll(&sl, &err)) { s_assign_wait = false; toast(err ? "ASSIGN FAILED" : "ASSIGNED"); }
    }
}
static void poolAssignTo(uint8_t pad) {
    if(nav.pool_cur >= s_pool_n) { nav.pool_armed = false; return; }
    audStop();                                   // the assign owns the SD now
    int r = patchAssignSample(pad, s_pool[nav.pool_cur].id);
    if(r == 0)        { s_assign_wait = true; toast("LOADING..."); nav.pool_armed = false; }
    else if(r == -2)  toast("BUSY");
    else              { toast("GONE"); nav.pool_armed = false; }
}

// ==================================================================== sound browser (DRUMS / SYNTH)
// OP-1 style: pick the ENGINE, then step through its PRESETS.
//   DRUMS: engine = KICK/SNARE/HAT/PERC = the quadrant of the target pad (the last pad played, or tapped here);
//          L/R = engine (target jumps to the same spot in the other quadrant), U/D = preset of the target pad, auditioned.
//   SYNTH: engine = algorithm; L/R = algorithm, U/D = its factory presets (INIT PLUCK PAD LEAD BASS). Play pads to hear it.
static_assert(DG_COUNT == 4, "NavState::br_dcur is sized for 4 drum engines");
static uint8_t brDrumTarget() { return (uint8_t)(nav.seq_target[ENG_DRUMS] & 15); }
static void brEngine(int8_t dx) {
    if(nav.menu == M_DRUMS) {
        uint8_t t = brDrumTarget();
        int g = (int)drumPadGroup(t) + dx;
        if(g < 0) g = 0;
        if(g > DG_COUNT - 1) g = DG_COUNT - 1;
        uint8_t idx = drumPadIndexInGroup(t), np = t;
        for(uint8_t p = 0; p < N_PADS; p++) if(drumPadGroup(p) == (DrumGroup)g && drumPadIndexInGroup(p) == idx) { np = p; break; }
        nav.seq_target[ENG_DRUMS] = np;
        drumsPreview(np);
    } else if(nav.menu == M_SYNTH) {
        int a = (int)syn.algo + dx;
        if(a < 0) a = 0;
        if(a > ALG_COUNT - 1) a = ALG_COUNT - 1;
        syn.algo = (uint8_t)a;
        patchMarkDirty();
    }
}
// Hold the joystick button in the browser: save the current sound into the NEXT FREE user slot of this engine (never overwrites,
// factory slots are read-only). Blocking SD ~0.5 s, user-initiated.
static void brSaveUser() {
    char t[16];
    if(nav.menu == M_SYNTH) {
        int r = patchUserSaveSynth(syn.algo);
        if(r >= 0) { nav.br_pre[syn.algo] = (uint8_t)r; snprintf(t, sizeof t, "SAVED U%02d", r); toast(t); }
        else toast(r == -1 ? "USER FULL" : "SAVE FAILED");
    } else if(nav.menu == M_DRUMS) {
        uint8_t tg = brDrumTarget(), g = (uint8_t)drumPadGroup(tg);
        int r = patchUserSaveDrum(g, drums.pad[tg].sound);
        if(r >= 0) { nav.br_dcur[g] = (uint8_t)(DRUM_SOUNDS + r); snprintf(t, sizeof t, "SAVED U%d", r + 1); toast(t); }
        else toast(r == -1 ? "USER FULL" : "SAVE FAILED");
    }
}
static void brPreset(int8_t dp) {
    if(nav.menu == M_DRUMS) {
        uint8_t t = brDrumTarget();
        uint8_t g = (uint8_t)drumPadGroup(t);
        int s = (int)(nav.br_dcur[g] != 0xFF ? nav.br_dcur[g] : drums.pad[t].sound) + dp;
        if(s < 0) s = 0;
        if(s > DRUM_SOUNDS + DRUM_USER_SLOTS - 1) s = DRUM_SOUNDS + DRUM_USER_SLOTS - 1;
        if(s < DRUM_SOUNDS) {
            nav.br_dcur[g] = 0xFF;
            drums.pad[t].sound = (uint8_t)s;
            patchMarkDirty();
            drumsPreview(t);
        } else {                                      // user entry: base sound + the engine's 4 depth knobs; an empty slot only moves the cursor
            nav.br_dcur[g] = (uint8_t)s;
            DrumUserEntry ue;
            if(drumsUserGet(g, (uint8_t)(s - DRUM_SOUNDS), &ue)) {
                drums.pad[t].sound = ue.base;
                for(int i = 0; i < DENG_PARAMS; i++) drums.eng[g].x[i] = ue.k[i];
                patchMarkDirty();
                drumsPreview(t);
            }
        }
    } else if(nav.menu == M_SYNTH) {
        int s = (int)nav.br_pre[syn.algo] + dp;
        if(s < 0) s = 0;
        if(s > SYN_FILE_PRESETS - 1) s = SYN_FILE_PRESETS - 1;
        if((uint8_t)s != nav.br_pre[syn.algo]) {
            nav.br_pre[syn.algo] = (uint8_t)s;
            SynPreset tmp;
            if(s < SYN_PRESETS || synFileGet(syn.algo, (uint8_t)s, tmp)) { synLoadPreset(syn.algo, (uint8_t)s); patchMarkDirty(); }   // empty user slot: cursor only
        }
    }
}

// ==================================================================== reorder (L_MOVE)
// SAMPLER: tap source slot, tap target slot -> the two slots swap (audio, length, per-slot pitch/formant/cutoff/reverse).
//          Sequences stay on their pads, so a swap changes WHICH sample a pattern plays. No file is touched.
// PATCHES: tap source, tap an EMPTY target -> patchMoveSlot() (store/, weak hook): moves the patch, never deletes.
static void movePad(uint8_t pad) {
    if(nav.mv_src < 0) {
        bool ok = (nav.menu == M_SMP) ? (smp.len[pad] > 0) : patchExists((uint8_t)(nav.patch_page * 16 + pad));
        if(ok) nav.mv_src = (int8_t)pad; else toast("EMPTY");
        return;
    }
    uint8_t src = (uint8_t)nav.mv_src;
    if(pad == src) { nav.mv_src = -1; return; }
    if(nav.menu == M_SMP) {
        samplerSwapSlots(src, pad);
        patchMarkDirty();
        toast("SWAPPED");
    } else {
        uint8_t a = (uint8_t)(nav.patch_page * 16 + src), b = (uint8_t)(nav.patch_page * 16 + pad);
        if(patchExists(b))          toast("TARGET USED");
        else if(!patchMoveSlot)     toast("MOVE N/A");
        else                        toast(patchMoveSlot(a, b) ? "MOVED" : "FAILED");
    }
    nav.mv_src = -1;
}

// ==================================================================== padcount (BARS/STEPS chooser)
static void padCountApply(uint8_t pad) {
    const ParamPage* pg = navCurrentPage();
    if(!pg || nav.cursor >= pg->count) { nav.level = L_CARD; return; }
    const Param& p = pg->items[nav.cursor];
    Engine e = engineOf(nav.menu);
    uint8_t val = (uint8_t)(pad + 1);
    if(strcmp(p.name, "BARS") == 0)       seqSetBars(e, val);
    else if(strcmp(p.name, "STEPS") == 0) seqSetStepsPerBar(e, val);
    patchMarkDirty();
    nav.level = L_CARD;
}

// ==================================================================== step editor (hand sequencer)
// ONE mechanism for hand-entering a pattern on all three engines (drums / synth / sampler):
//   - on the SEQ card (ring level) push the joystick UP or DOWN to enter; UP or DOWN again to leave
//   - pads = the steps of the shown PAGE (a page = one bar, up to 16 steps = the 16 pads)
//   - joystick LEFT / RIGHT = previous / next page (hard stops, no wrap)
//   - the lane written is seq_target[engine]: the last pad you played on that engine's pages;
//     joystick CLICK inside the editor advances it (and auditions it) so you can change lane without leaving
// (This replaces the old "hold a pad 500 ms, then the other pads become the steps of whatever bar the
//  playhead is in" place mode, and the synth-only HANDSEQ list item. Same job, one path.)
static void stepEditNextLane() {
    Engine e = engineOf(nav.menu);
    uint8_t t = (uint8_t)((nav.seq_target[e] + 1) & 15);
    nav.seq_target[e] = t;
    if(e == ENG_DRUMS)                         drumsPreview(t);
    else if(e == ENG_SMP && smp.len[t] > 0)    samplerPlay(t, 1.f);
}

static void stepEditPad(uint8_t pad) {
    Engine e = engineOf(nav.menu);
    LaneSet& l = seq.lane[e];
    uint8_t spb = l.steps_per_bar ? l.steps_per_bar : 16;
    if(pad >= spb) return;
    uint32_t idx = (uint32_t)nav.hand_bar * spb + pad;
    if(idx >= MAXSTEPS) return;
    uint8_t lane = nav.seq_target[e];
    seqToggle(e, idx, lane);
    // A pattern entered while the lane is in MANUAL would never sound (seqFire() returns nothing in
    // MANUAL). Placing a step means you want to hear it: promote to PLAY the first time a step lands.
    if(((l.step[idx] >> lane) & 1u) && l.mode_f < 0.5f) { l.mode_f = (float)SEQ_PLAY; }
    patchMarkDirty();
}

// ==================================================================== encoder turn
static void handleEncTurn(int8_t dir, uint32_t /*now_ms*/) {
    // THE RULE: the encoder only ever changes the main menu. Anywhere below the root
    // ring a turn is the BACK button (one level per detent); at the root it moves the ring.
    switch(nav.level) {
        case L_RING: {
            Menu old = nav.menu;
            int m = (int)nav.menu + dir;
            if(m < 0) m = 0;
            if(m > M_COUNT - 1) m = M_COUNT - 1;
            nav.menu = (Menu)m;
            if(nav.menu != old) {
                fxReleaseAll();
                nav.focus_pad = -1;
                syn.perf_active = false;
                fx.perf_engine = 0xFF;
                nav.card = C_TIMBRE; nav.cursor = 0; nav.page = 0;
                nav.dive_ms = daisy::System::GetNow();          // page-switch animation
                // TASK 8 (2026-10): GLOBALS has exactly one page (GLOBAL_PAGE), and
                // ringGlobalsDash() already renders it at L_RING with the same wList() call
                // L_CARD would make (render_oled.cpp) — so L_RING and L_CARD look IDENTICAL
                // for this menu. That made the first joystick interaction after landing here
                // feel dead: handleJoyNav's L_RING switch has no GLOBALS case at all (only
                // isEngineMenu/M_FX/M_PATCHES), so a flick did nothing against a screen that
                // already looked fully interactive; only a CLICK (handleJoyPress) advanced
                // nav.level to L_CARD, and only THEN did flicking move the cursor and a
                // second click open an editor. Root cause: an extra nav level with no visual
                // or functional meaning of its own for a single-page menu. Fix: land straight
                // in L_CARD, which already has full flick-to-move / click-to-edit behaviour
                // (unchanged below) — the first interaction is now the real one. Other menus
                // keep the L_RING step because it means something there (choose a card / FX
                // bank / patch slot); GLOBALS has nothing to choose.
                if(nav.menu == M_GLOBALS) {
                    globalsRefreshShadow();
                    nav.level = L_CARD;
                }
            }
            break;
        }
        case L_VOLUME:   closeVolume(); break;
        case L_EDIT1: case L_EDIT2:
            nav.level = L_CARD; nav.edit = nullptr; break;
        case L_PADCOUNT: nav.level = L_CARD; break;
        case L_BROWSE: case L_MOVE: nav.mv_src = -1; nav.level = L_RING; break;
        case L_HANDSEQ:  nav.level = L_RING; break;   // entered from the ring (joystick U/D) -> back is the ring
        case L_CARD:
            if(nav.pool_armed) { nav.pool_armed = false; break; }    // first back = put the picked pool sample down
            if(nav.focus_pad >= 0) { nav.focus_pad = -1; break; }   // next back = un-focus
            nav.level = L_RING; nav.cursor = 0; break;
        default: nav.level = L_RING; break;
    }
}

// ==================================================================== joystick press
static void handleJoyPress(uint32_t /*now_ms*/) {
    if(nav.level == L_VOLUME) { closeVolume(); return; }
    s_pre_joy_level  = nav.level;
    s_pre_joy_menu   = nav.menu;
    s_pre_joy_card   = nav.card;
    s_pre_joy_cursor = nav.cursor;
    s_pre_joy_focus  = nav.focus_pad;
    for(int i = 0; i < ENG_COUNT; i++) s_pre_joy_target[i] = nav.seq_target[i];

    switch(nav.level) {
        case L_RING:
            if(isEngineMenu(nav.menu)) {
                nav.level = L_CARD; nav.cursor = 0; nav.page = 0;
            } else if(nav.menu == M_FX) {
                // nothing on click: automation records while the button is HELD (see navTick)
            } else if(nav.menu == M_PATCHES) {
                // nothing on click: a grid PAD now owns patch switching (instant load/new on
                // IN_PAD_PRESS, see handlePadPress) — don't ship two mechanisms for one job.
            } else if(nav.menu == M_GLOBALS) {
                globalsRefreshShadow();
                nav.level = L_CARD; nav.cursor = 0; nav.page = 0;
            } else if(nav.menu == M_DASH) {
                nav.level = L_CARD; nav.cursor = 0; nav.page = 0;
            }
            break;
        case L_CARD: {
            if(inPool()) {                                           // pool browser: click picks the sample (flashes), again puts it down
                if(nav.pool_armed) nav.pool_armed = false;
                else if(s_pool_n > 0) nav.pool_armed = true;
                break;
            }
            if(nav.focus_pad >= 0) { nav.focus_pad = -1; break; }    // un-focus only
            const ParamPage* pg = navCurrentPage();
            if(!pg || pg->count == 0) { nav.level = L_RING; break; }
            uint8_t idx = (nav.cursor < pg->count) ? nav.cursor : 0;
            const Param& p = pg->items[idx];
            if(isClearItem(p)) {
                seqClear(engineOf(nav.menu));                     // CLEAR: wipe this engine's whole sequence
                patchMarkDirty();
            } else if(isPadCountItem(p)) {
                nav.level = L_PADCOUNT;
            } else if(p.value != nullptr) {
                nav.edit = &p;
                nav.level = (p.flags & PF_PAIR_X) ? L_EDIT2 : L_EDIT1;
            } else {
                nav.level = L_RING;
            }
            break;
        }
        case L_EDIT1: case L_EDIT2:
            nav.level = L_CARD;
            nav.edit = nullptr;
            break;
        case L_PADCOUNT:
            nav.level = L_CARD;
            break;
        case L_BROWSE: case L_MOVE:
            nav.mv_src = -1; nav.level = L_RING;
            break;
        case L_HANDSEQ:
            stepEditNextLane();               // click = next lane (pad) to write; auditioned so you hear which
            break;
        default: break;
    }
}

// ==================================================================== both-press (volume shortcut)
// first_btn: which button was pressed FIRST (0 = encoder, 1 = joystick), carried in
// IN_BOTH_PRESS.a by input.cpp. Cross-module fix: nav.last_single is not reliable here —
// by the time this event is processed it may have been overwritten by the second
// button's own event (see input.cpp's handleButton for the full rationale).
static void handleBothPress(int8_t first_btn, uint32_t /*now_ms*/) {
    if(first_btn == 0) {
        clockUntap();
    } else if(first_btn == 1) {
        nav.level  = s_pre_joy_level;
        nav.menu   = s_pre_joy_menu;
        nav.card   = s_pre_joy_card;
        nav.cursor = s_pre_joy_cursor;
        nav.focus_pad = s_pre_joy_focus;
        for(int i = 0; i < ENG_COUNT; i++) nav.seq_target[i] = s_pre_joy_target[i];   // the chord's joystick half must not advance the lane
    }
    s_return_level = nav.level;
    nav.level = L_VOLUME;
    nav.vol_overlay = true;
}

// ==================================================================== joystick nav (flick)
static void handleJoyNav(int8_t dx, int8_t dy, bool rpt, uint32_t /*now_ms*/) {
    // focused drum pad: dx swipes its 16 sounds, hard stops (takes priority over list nav)
    if(nav.focus_pad >= 0 && nav.menu == M_DRUMS && nav.card == C_TIMBRE) {
        if(dx != 0) {
            int s = (int)drums.pad[nav.focus_pad].sound + dx;
            if(s < 0) s = 0;
            if(s > DRUM_SOUNDS - 1) s = DRUM_SOUNDS - 1;
            drums.pad[nav.focus_pad].sound = (uint8_t)s;
        }
        if(dy != 0) {                                            // U/D flick = this pad's volume +-10 %
            float v = drums.pad[nav.focus_pad].vol + 0.1f * (float)dy;
            drums.pad[nav.focus_pad].vol = clampf(v, 0.f, 2.f);
            drumsPreview((uint8_t)nav.focus_pad);                // hear it
        }
        return;
    }

    if(nav.level == L_BROWSE) {
        if(dx != 0 && !rpt) brEngine(dx);
        if(dy != 0) brPreset((int8_t)-dy);              // +Y = UP = previous preset
        return;
    }
    if(nav.level == L_MOVE) {
        if(!rpt && (dx != 0 || dy != 0)) { nav.mv_src = -1; nav.level = L_RING; }   // any flick leaves
        return;
    }
    if(nav.level == L_HANDSEQ) {
        if(dy != 0) {                                            // UP or DOWN = leave the editor (flick only, not the auto-repeat)
            if(!rpt) nav.level = L_RING;
            return;
        }
        if(dx != 0) {                                            // LEFT / RIGHT = previous / next page (bar), hard stops
            Engine e = engineOf(nav.menu);
            int maxb = (int)seq.lane[e].bars - 1; if(maxb < 0) maxb = 0;
            int b = (int)nav.hand_bar + dx;
            if(b < 0) b = 0;
            if(b > maxb) b = maxb;
            nav.hand_bar = (uint8_t)b;
        }
        return;
    }
    switch(nav.level) {
        case L_RING:
            if(isEngineMenu(nav.menu)) {
                if(dx != 0) {
                    int c = (int)nav.card + dx;
                    if(c < 0) c = 0;
                    if(c > C_COUNT - 1) c = C_COUNT - 1;
                    if((Card)c != nav.card) { nav.card = (Card)c; nav.page = 0; nav.cursor = 0; }
                }
                // joystick UP or DOWN (the initial flick only) enters the tool of the card:
                //   SEQ card -> step editor; TIMBRE card -> sound browser (DRUMS, SYNTH) / reorder (SAMPLER)
                // SYNTH PERFORM ring: U/D (repeats too) = release time x1.25 / x0.8, shown as a toast. A direct control for the one
                // envelope value you reach for live.
                if(dy != 0 && nav.menu == M_SYNTH && nav.card == C_PERF) {
                    Adsr& a = syn.preset[syn.algo].amp;
                    a.r = clampf(dy > 0 ? a.r * 1.25f : a.r * 0.8f, 0.005f, 6.f);
                    char t[16]; int cs = (int)(a.r * 100.f + 0.5f);
                    snprintf(t, sizeof t, "REL %d.%02ds", cs / 100, cs % 100);
                    toast(t); patchMarkDirty();
                }
                if(dy != 0 && !rpt) {
                    if(nav.card == C_SEQ) {
                        nav.level = L_HANDSEQ;
                        nav.hand_bar = 0;
                    } else if(nav.card == C_TIMBRE) {
                        if(nav.menu == M_SMP) { nav.level = L_MOVE; nav.mv_src = -1; }
                        else                { nav.level = L_BROWSE; memset(nav.br_dcur, 0xFF, sizeof nav.br_dcur); }
                    }
                }
            } else if(nav.menu == M_FX) {
                // FX banks (v2 FX1..FX8): L/R flick = previous/next bank, hard stops, no wrap.
                if(dx != 0) {
                    int b = (int)fx.view + dx;
                    if(b < 0) b = 0;
                    if(b > FX_BANKS - 1) b = FX_BANKS - 1;
                    fxSetView((uint8_t)b);
                }
            } else if(nav.menu == M_PATCHES) {
                if(dx != 0) {
                    int p = (int)nav.patch_page + dx;
                    if(p < 0) p = 0;
                    if(p > PATCH_PAGES - 1) p = PATCH_PAGES - 1;
                    nav.patch_page = (uint8_t)p;
                }
                // U/D (flick) = reorder mode; pads still do cursor + load/new directly (handlePadPress)
                if(dy != 0 && !rpt) { nav.level = L_MOVE; nav.mv_src = -1; }
            }
            // FX: view is set continuously in navTick from g.joy_x/joy_y, not from this event
            break;
        case L_CARD: {
            if(nav.card == C_PERF) break;   // no list; joystick is live performance
            const ParamPage* pg = navCurrentPage();
            if(inPool()) {                                    // pool page: U/D walks the sample list
                if(dy != 0 && s_pool_n > 0) {
                    int c = (int)nav.pool_cur - dy;
                    if(c < 0) c = 0;
                    if(c > (int)s_pool_n - 1) c = (int)s_pool_n - 1;
                    nav.pool_cur = (uint16_t)c;
                }
            } else
            if(dy != 0 && pg) moveCursor(*pg, (int8_t)-dy);   // +Y = UP = previous item
            if(dx != 0 && isEngineMenu(nav.menu) && nav.card == C_TIMBRE) {
                uint8_t cnt = 0;
                navTimbrePages(engineOf(nav.menu), cnt);
                int pg2 = (int)nav.page + dx;
                if(pg2 < 0) pg2 = 0;
                if(cnt > 0 && pg2 > cnt - 1) pg2 = cnt - 1;
                if((uint8_t)pg2 != nav.page) { nav.page = (uint8_t)pg2; nav.cursor = 0; }
            }
            break;
        }
        default: break;   // editors read joy_x/joy_y continuously in navTick
    }
}

// ==================================================================== joystick gesture (focused pad reverse)
static void handleGesture(int8_t a) {
    if(nav.focus_pad < 0) return;
    if(nav.menu == M_DRUMS && nav.card == C_TIMBRE) {
        drums.pad[nav.focus_pad].reverse = (a < 0);     // -1 CCW = reverse, +1 CW = forward
    } else if(nav.menu == M_SMP && nav.card == C_TIMBRE) {
        smp.slot[nav.focus_pad].reverse = (a < 0);
    }
}

// ==================================================================== pads
static void handlePadPress(uint8_t pad, uint32_t /*now_ms*/) {
    if(nav.level == L_VOLUME) {
        outp.fader = clampf((float)pad / 11.25f, 0.f, 1.333f);
        return;
    }
    if(nav.level == L_PADCOUNT) { padCountApply(pad); return; }
    if(nav.level == L_MOVE) { movePad(pad); return; }
    if(nav.level == L_BROWSE) {                       // pads in the browser: pick the pad/engine to browse (drums) or just play (synth)
        if(nav.menu == M_DRUMS) { nav.seq_target[ENG_DRUMS] = pad; drumsPreview(pad); }
        else if(nav.menu == M_SYNTH) { synthNoteOn(pad, 1.f); s_pad_act[pad] = PA_SYN; }
        return;
    }
    if(inPool()) {
        if(nav.pool_armed) { poolAssignTo(pad); return; }
        if(smp.len[pad] > 0) samplerPlay(pad, 1.f);   // never start a recording by accident on the pool page
        return;
    }
    if(nav.level == L_HANDSEQ) {                      // pads = the steps of the shown page, for the target lane
        if(isEngineMenu(nav.menu)) stepEditPad(pad);
        return;
    }

    switch(nav.menu) {
        case M_FX: {
            uint8_t id = fxId(fx.view, pad);             // v2: bit = physical pad in the current bank
            s_pad_fx_id[pad] = id; s_pad_act[pad] = PA_FX;
            fxPress(id);
            return;
        }
        case M_PATCHES: {
            // instant switch: existing slot = load it; empty slot = start a fresh patch there
            // (v2 sessionNew). Was joystick-click-to-load (nav.cursor = pad only); the pad itself
            // now owns patch switching, don't ship two mechanisms for one job.
            nav.cursor = pad;
            uint8_t slot = (uint8_t)(nav.patch_page * 16 + pad);
            if(patchExists(slot)) patchLoad(slot); else patchNew(slot);
            return;
        }
        case M_GLOBALS: case M_DASH: return;
        default: break;
    }
    if(!isEngineMenu(nav.menu)) return;
    nav.seq_target[engineOf(nav.menu)] = pad;         // the pad you last played is the lane the step editor writes

    switch(nav.card) {
        case C_TIMBRE:
            if(nav.menu == M_DRUMS) {
                if(nav.focus_pad >= 0 && nav.focus_pad != (int8_t)pad) nav.focus_pad = (int8_t)pad;
                drumsPreview(pad);
            } else if(nav.menu == M_SYNTH) {
                synthNoteOn(pad, 1.f); s_pad_act[pad] = PA_SYN;
            } else if(nav.menu == M_SMP) {
                if(nav.focus_pad >= 0 && nav.focus_pad != (int8_t)pad) nav.focus_pad = (int8_t)pad;
                if(smp.len[pad] > 0) samplerPlay(pad, 1.f);
                else if(samplerRecordStart(pad)) s_pad_act[pad] = PA_SMPREC;
            }
            break;
        case C_SEQ:
            if(nav.menu == M_DRUMS) {
                drumsPreview(pad);
                if(seq.lane[ENG_DRUMS].mode == SEQ_PLAY && seq.lane[ENG_DRUMS].rec_on > 0.5f) seqRecordHit(ENG_DRUMS, pad, g.pad_press_sample[pad & 15]);
            } else if(nav.menu == M_SYNTH) {
                synthNoteOn(pad, 1.f); s_pad_act[pad] = PA_SYN;
                if(seq.lane[ENG_SYNTH].mode == SEQ_PLAY && seq.lane[ENG_SYNTH].rec_on > 0.5f) seqRecordHit(ENG_SYNTH, pad, g.pad_press_sample[pad & 15]);
            } else if(nav.menu == M_SMP) {
                if(smp.len[pad] > 0) samplerPlay(pad, 1.f);
                if(seq.lane[ENG_SMP].mode == SEQ_PLAY && seq.lane[ENG_SMP].rec_on > 0.5f) seqRecordHit(ENG_SMP, pad, g.pad_press_sample[pad & 15]);
            }
            break;
        case C_PERF:
            if(nav.menu == M_DRUMS) {
                drumsPreview(pad);
                fx.perf_engine = ENG_DRUMS;
            } else if(nav.menu == M_SYNTH) {
                // chord memory: a press with no other pad down starts a new combination; every further
                // pad pressed while any pad is still down joins it; releases never remove notes. The
                // sequencer (GEN / arp) plays from this set until a fresh press-run replaces it.
                uint16_t others = (uint16_t)(g.pads_held & ~(1u << pad));
                g.syn_chord = others ? (uint16_t)(g.syn_chord | (1u << pad)) : (uint16_t)(1u << pad);
                // GEN, or PLAY with the arp on: pads only SELECT notes for the generator/arp, they do
                // not play directly (EMGOR 2026-09-15). MANUAL / plain PLAY: pads play.
                const LaneSet& sl = seq.lane[ENG_SYNTH];
                bool selectOnly = (sl.mode == SEQ_GEN) || (sl.mode == SEQ_PLAY && sl.arp_on >= 0.5f);
                if(!selectOnly) { synthNoteOn(pad, 1.f); s_pad_act[pad] = PA_SYN; }
            } else if(nav.menu == M_SMP) {
                if(smp.len[pad] > 0) samplerPlay(pad, 1.f);
                fx.perf_engine = ENG_SMP;
            }
            break;
        default: break;
    }
}

static void handlePadRelease(uint8_t pad, uint32_t /*now_ms*/) {
    // TASK 2 (2026-10): handlePadHold never had a matching release-side clear, so the
    // delete-countdown ramp (render_trellis.cpp overlayPatches / NAV_HOLD_DELETE_MS) kept
    // showing against the last-held pad after you let go, instead of resetting.
    if(nav.hold_pad == (int8_t)pad) { nav.hold_pad = -1; nav.hold_ms = 0; }
    padLatchRelease(pad);   // drums: one-shot triggers, no release behaviour
}

static void handlePadHold(uint8_t pad, uint32_t held_ms, uint32_t /*now_ms*/) {
    nav.hold_pad = (int8_t)pad;
    nav.hold_ms  = held_ms;

    if(nav.menu == M_PATCHES) return;   // no pad-hold delete on the device any more: hard delete is an editor job (files are never removed from here)
    if(nav.menu == M_DRUMS && nav.card == C_TIMBRE) {
        if(nav.level == L_BROWSE || nav.level == L_MOVE) return;     // the browser owns the stick: don't steal it with a focus
        if(held_ms >= 500) nav.focus_pad = (int8_t)pad;
        return;
    }
    if(nav.menu == M_SMP && nav.card == C_TIMBRE && nav.level != L_MOVE && !nav.pool_armed) {
        if(smp.len[pad] == 0) return;   // only filled slots focus or delete
        if(held_ms >= 500 && !inPool()) nav.focus_pad = (int8_t)pad;
        if(held_ms >= UI_PAD_CLEAR_MS) {
            samplerDelete(pad);
            toast("CLEARED");
            if(nav.focus_pad == (int8_t)pad) nav.focus_pad = -1;
            nav.hold_ms = 0; nav.hold_pad = -1;
        }
        return;
    }
}

// ==================================================================== joystick-button hold
// SAMPLER: hold 3 s = blank ALL slots (length only; no file, no audio memory touched). The click that started the hold already
// acted on press, so the pre-press state is restored first (same mechanism as the both-button undo).
static bool s_joy_blank_done = false;
static void handleJoyHold(uint32_t ms) {
    if((nav.menu == M_DRUMS || nav.menu == M_SYNTH) && s_pre_joy_level == L_BROWSE) {     // the press that started the hold acted as a click (left the browser): undo, then save
        nav.joy_hold_ms = ms;
        if(ms >= UI_JOY_SAVE_MS && !s_joy_blank_done) {
            s_joy_blank_done = true;
            nav.level = L_BROWSE; nav.card = s_pre_joy_card; nav.cursor = s_pre_joy_cursor; nav.focus_pad = s_pre_joy_focus;
            brSaveUser();
        }
        return;
    }
    if(nav.menu != M_SMP) return;
    nav.joy_hold_ms = ms;
    if(ms >= UI_JOY_BLANK_MS && !s_joy_blank_done) {
        s_joy_blank_done = true;
        nav.level = s_pre_joy_level; nav.card = s_pre_joy_card; nav.cursor = s_pre_joy_cursor; nav.focus_pad = s_pre_joy_focus;
        nav.pool_armed = false; nav.mv_src = -1;
        samplerBlankAll();
        patchMarkDirty();
        toast("ALL BLANK");
    }
}

// ==================================================================== navHandle
void navHandle(const InputEvent& e, uint32_t now_ms) {
    if(e.type != IN_PAD_PRESS && e.type != IN_PAD_RELEASE && e.type != IN_PAD_HOLD) {
        nav.saver = false;
        nav.last_nav_ms = now_ms;
    }
    switch(e.type) {
        case IN_ENC_PRESS: {
            // A tap is an EVENT with a timestamp from the audio ISR (hw/controls.cpp), not "whenever
            // this loop got round to it". transportTap() routes it by clock_mode: local analysis when
            // INTERNAL/MASTER, straight to the Pi when SLAVE. Fallback to "now" only if the ISR stamp
            // is missing/stale. ALWAYS. Nothing else, except closing an open volume overlay.
            const uint32_t ns = clockSampleNow();
            uint32_t ts = ns;
            if(!encoderTakePressSample(ns, (uint32_t)(0.12f * SR), &ts)) ts = ns;
            transportTap(ts);
            if(nav.level == L_VOLUME) closeVolume();
            nav.last_single = e;
            break;
        }
        case IN_ENC_TURN:    handleEncTurn((int8_t)e.a, now_ms); break;
        case IN_JOY_PRESS:   handleJoyPress(now_ms); nav.last_single = e; break;
        case IN_BOTH_PRESS:  handleBothPress((int8_t)e.a, now_ms); break;
        case IN_JOY_NAV:     handleJoyNav((int8_t)e.a, (int8_t)e.b, e.rpt, now_ms); break;
        case IN_JOY_GESTURE: handleGesture((int8_t)e.a); break;
        case IN_PAD_PRESS:   handlePadPress((uint8_t)e.a, now_ms); break;
        case IN_PAD_RELEASE: handlePadRelease((uint8_t)e.a, now_ms); break;
        case IN_PAD_HOLD:    handlePadHold((uint8_t)e.a, (uint32_t)e.b, now_ms); break;
        case IN_JOY_HOLD:    handleJoyHold((uint32_t)e.a); break;
        default: break;
    }
}

// ==================================================================== navTick
void navTick(uint32_t now_ms) {
    auto stickNorm = [](float j) { float t = 0.5f + 0.5f * (j / EDIT_STICK_FULL); return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); };
    if(!nav.saver && (now_ms - nav.last_nav_ms) > SAVER_IDLE_MS) nav.saver = true;

    syn.perf_active = (nav.level == L_CARD && nav.card == C_PERF && nav.menu == M_SYNTH);
    // the latched chord PERSISTS when leaving PERF (EMGOR 2026-09-15) — only a new press-run in PERF replaces it

    // ---- FX page (v2 banks): the bank is switched by L/R flicks in handleJoyNav
    g.on_fx_page = (nav.menu == M_FX);
    // held pads only mean something to the engine whose page is showing: on FX/PATCHES/etc. they are
    // nobody's (was: drum GEN rolls / sampler rolls fired from FX-page presses — EMGOR 2026-09-15)
    // (in the step editor the pads are step buttons, not notes: they belong to nobody, so a held pad can
    //  never feed GEN's live roll or the arp while you are placing steps)
    g.pad_engine = (isEngineMenu(nav.menu) && nav.level != L_HANDSEQ) ? (uint8_t)engineOf(nav.menu) : 0xFF;
    if(nav.level == L_HANDSEQ && isEngineMenu(nav.menu)) {     // BARS shrunk under the editor (patch load): stay on a real page
        uint8_t bars = seq.lane[engineOf(nav.menu)].bars;
        if(bars && nav.hand_bar >= bars) nav.hand_bar = (uint8_t)(bars - 1);
    }
    if(nav.menu != M_FX) fx.auto_rec = false;
    else fx.auto_rec = g.joy_sw;   // v2 rule: hold the joystick button = record automation this step; empty hands scrub it clean

    // ---- volume overlay: joystick X also drives the fader
    if(nav.level == L_VOLUME) {
        outp.fader = clampf(stickNorm(g.joy_x) * 1.333f, 0.f, 1.333f);
    }

    // ---- universal editors, RELATIVE: opening an editor never moves the value. The stick adds an offset to the value it had
    // when you opened (or when the stick last passed through centre): a full throw (+-EDIT_STICK_FULL) sweeps the whole range
    // from that anchor. Return to centre to re-anchor and push again (ratchet) -- no hunting for the old value, no snap.
    {
        struct Axis { const Param* p = nullptr; float v0 = 0.f, n0 = 0.5f; };
        static Axis ax, ay;
        auto follow = [&](Axis& a, const Param& pp, float joy) {
            float n = stickNorm(joy);
            if(a.p != &pp || (joy < 0.06f && joy > -0.06f)) { a.p = &pp; a.v0 = *pp.value; a.n0 = n; }   // (re)anchor: wherever the stick is on open, or centre
            float nv = paramClamp(pp, a.v0 + (n - a.n0) * 2.f * (pp.max - pp.min));
            if(nv != *pp.value) { *pp.value = nv; return true; }
            return false;
        };
        bool editing = (nav.level == L_EDIT1 || nav.level == L_EDIT2) && nav.edit;
        if(!editing) { ax.p = nullptr; ay.p = nullptr; }
        else if(nav.level == L_EDIT1) {
            if(follow(ax, *nav.edit, g.joy_x)) patchMarkDirty();
            if(nav.edit->value == &s_syn_algo_shadow) {
                uint8_t a = (uint8_t)clampf(s_syn_algo_shadow, 0.f, (float)(ALG_COUNT - 1));
                if(a != syn.algo) syn.algo = a;
                s_syn_algo_applied = s_syn_algo_shadow;
            }
        } else {
            bool cx = follow(ax, *nav.edit, g.joy_x);
            bool cy = follow(ay, *(nav.edit + 1), g.joy_y);
            if(cx || cy) patchMarkDirty();
        }
    }

    // ---- sampler focused-pad live control
    if(nav.menu == M_SMP && nav.card == C_TIMBRE && nav.focus_pad >= 0) {
        smp.slot[nav.focus_pad].formant = clampf(g.joy_x, -1.f, 1.f);
        smp.slot[nav.focus_pad].pitch   = clampf(g.joy_y, -1.f, 1.f);
    }

    // ---- performance cards: continuous joystick / accel mappings
    if(nav.level == L_CARD && nav.card == C_PERF) {
        const float dz = 0.15f;
        float jx = g.joy_x, jy = g.joy_y;
        if(nav.menu == M_SYNTH) {
            // Y = octave: past 0.35 = +-1, past 0.85 (the extreme) = +-2; centre = the key. Glides in synth.cpp.
            // Y = octave, continuous and exponential over 80 % of the throw: t = |y|/0.8, shift = 2 * t^2.5
            // octaves — small movements barely move the pitch, the last stretch does the big jump; +-2 at 80 %+.
            float ay = jy < 0.f ? -jy : jy;
            float t = ay / 0.8f; if (t > 1.f) t = 1.f;
            float oct = 2.f * t * t * sqrtf(t);
            syn.perf_octave = (jy < 0.f) ? -oct : oct;
            // X = generative probability, LOGARITHMIC on the low side: centre = 50 %, hard left = 1.5 %,
            // so most of the left-hand travel is spent in the sparse region; right side climbs linearly to 100 %.
            syn.master_prob = (jx <= 0.f) ? 0.5f * fastPow2(5.f * jx) : 0.5f + 0.5f * jx;
            seq.lane[ENG_SYNTH].probability = syn.master_prob;
            {   // accel X (the FIRST value on the DASH 'A' row): negative = shorter notes, positive = longer.
                // plucky (0) <-> long ambient (1); full swing at +-0.4 g (~25 deg tilt), smoothed
                float m = clampf(0.5f + 1.25f * g.accel_x, 0.f, 1.f);
                syn.perf_env_macro += (m - syn.perf_env_macro) * 0.05f;
#if PERF_ACCEL_FILTER
                // accel Y (the SECOND DASH value): positive = filter open, negative = closed; floor 0.25
                float c = clampf(0.5f + 0.7f * g.accel_y, 0.25f, 1.f);
                syn.perf_cutoff += (c - syn.perf_cutoff) * 0.05f;
#else
                syn.perf_cutoff = 1.f;                     // filter removed from PERF: preset cutoff untouched
#endif
            }
        } else if(nav.menu == M_DRUMS || nav.menu == M_SMP) {
            fx.perf_engine = (nav.menu == M_DRUMS) ? ENG_DRUMS : ENG_SMP;
            fx.perf_up    = (jy >  dz) ? clampf(( jy - dz) / (1.f - dz), 0.f, 1.f) : 0.f;   // +Y = UP
            fx.perf_down  = (jy < -dz) ? clampf((-jy - dz) / (1.f - dz), 0.f, 1.f) : 0.f;
            fx.perf_right = (jx >  dz) ? clampf(( jx - dz) / (1.f - dz), 0.f, 1.f) : 0.f;
            fx.perf_left  = (jx < -dz) ? clampf((-jx - dz) / (1.f - dz), 0.f, 1.f) : 0.f;
        }
    } else if(fx.perf_engine != 0xFF) {
        // cross-module fix: leaving the drums/sampler PERF card used to leave
        // fx.perf_engine and perf_up/right/down/left exactly where the joystick
        // last put them — the throw kept applying (and audio_callback kept
        // routing that engine through its own perf bus) until an unrelated 3 s
        // tail timer in audio_callback.cpp happened to expire. Clear here,
        // immediately, per spec. audio_callback.cpp latches the engine
        // separately so effect tails (spring reverb etc.) still ring out
        // naturally instead of being cut off by this clear.
        fx.perf_engine = 0xFF;
        fx.perf_up = fx.perf_right = fx.perf_down = fx.perf_left = 0.f;
    }

    // ---- joystick hold progress, pool browser scan/assign, toast
    if(!g.joy_sw) { nav.joy_hold_ms = 0; s_joy_blank_done = false; }
    poolTick(now_ms);
    if(nav.toast[0] && (int32_t)(now_ms - nav.toast_until) > 0) nav.toast[0] = 0;

    padLatchReconcile();
    globalsApply();
}

// ==================================================================== navInit
void navInit() {
    nav.menu = M_DRUMS;
    nav.card = C_TIMBRE;
    nav.level = L_RING;
    nav.cursor = 0;
    nav.page = 0;
    nav.edit = nullptr;
    nav.focus_pad = -1;
    nav.perf_active = false;
    nav.last_nav_ms = 0;
    nav.saver = false;
    nav.vol_overlay = false;
    nav.patch_page = 0;
    nav.hold_ms = 0;
    nav.hold_pad = -1;
    nav.dive_ms = 0;
    for(int i = 0; i < ENG_COUNT; i++) nav.seq_target[i] = 0;
    nav.hand_bar = 0;
    nav.last_single.type = IN_NONE;
    nav.last_single.a = nav.last_single.b = 0;
    nav.last_single.t_ms = 0;
    s_return_level = L_RING;
    s_syn_algo_applied = -1.f;   // force a first sync on the next navTick/navTimbrePages call
    nav.mv_src = -1; nav.pool_cur = 0; nav.pool_armed = false; nav.joy_hold_ms = 0; nav.toast[0] = 0; nav.toast_until = 0;
    for(int i = 0; i < ALG_COUNT; i++) nav.br_pre[i] = 0;
    memset(nav.br_dcur, 0xFF, sizeof nav.br_dcur);
    globalsRefreshShadow();
}

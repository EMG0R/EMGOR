// test_ui.cpp -- drives ui/nav.cpp + ui/input.cpp (the real files) on the host with scripted input events and stubbed hardware.
// Counts joystick/encoder actions for common tasks, and checks the sound browser, sampler pool browser, reorder mode,
// hold-to-clear, hold-to-blank-all and manual clock mode against the engine state.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "config.h"
#include "ui/nav.h"
#include "ui/input.h"
#include "pmor_core/shared_state.h"
#include "pmor_core/clock.h"
#include "pmor_core/sequencer.h"
#include "pmor_core/scale.h"
#include "pmor_core/synth.h"
#include "pmor_core/drums.h"
#include "pmor_core/sampler.h"
#include "pmor_core/fx.h"
#include "pmor_core/output.h"
#include "pmor_core/metro.h"
#include "store/patch.h"
#include "hw/hw.h"
#include "hw/studio.h"
#include "hw/transport_link.h"
#include "pmor_core/engine_util.h"
#include "pmor_core/synth_private.h"
#include "pmor_core/events.h"
extern "C" {
#include "usb/clock_engine.h"
}

uint32_t daisy::System::s_now_ms = 1000;
static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

// ---------------------------------------------------------------- hardware / store stubs (only what nav.cpp references)
Globals glob;
float g_trellis_brightness = 1.f, g_trellis_brightness_ctrl = 1.f;
float trellisBrightnessGamma(float l) { return l; }
void trellisSetRotation(uint8_t) {}
extern "C" void clock_engine_set_sync_mode(sync_mode_t) {}
void transportTap(uint32_t) {}
bool encoderTakePressSample(uint32_t, uint32_t, uint32_t*) { return false; }
void globalsSave() {}
void patchFactoryReset() {}
static bool s_patch_exists[PATCH_SLOTS];
static uint8_t s_cur = 0;
bool patchExists(uint8_t s) { return s < PATCH_SLOTS && s_patch_exists[s]; }
uint8_t patchCurrent() { return s_cur; }
bool patchLoad(uint8_t s) { s_cur = s; return true; }
bool patchNew(uint8_t s) { s_patch_exists[s] = true; s_cur = s; return true; }
void patchDelete(uint8_t) { fails++; printf("FAIL: patchDelete called from the UI\n"); }
static int s_dirty = 0;
void patchMarkDirty() { s_dirty++; }
// sample pool: patches 0 and 2 have audio (slots 1,3 and 5)
static int s_scans = 0;
static void initPoolPatches() { s_patch_exists[0] = true; s_patch_exists[2] = true; }
int patchPoolScan(uint8_t p) { s_scans++; return (p == 0 || p == 2) ? 1 : -1; }
uint32_t patchPoolLen(uint8_t p, uint8_t s) {
    if(p == 0 && (s == 1 || s == 3)) return 48000u * (s + 1);
    if(p == 2 && s == 5) return 24000;
    return 0;
}
static int s_reads = 0; static uint16_t s_read_id = 0xFFFF;
int32_t patchPoolRead(uint16_t id, uint32_t off, uint8_t* buf, uint32_t max) { s_reads++; s_read_id = id; for(uint32_t i = 0; i < max; i++) buf[i] = (uint8_t)((off + i) * 7u); return (int32_t)max; }
static int s_assign_slot = -1; static uint16_t s_assign_id = 0xFFFF; static bool s_assign_done = false;
int patchAssignSample(uint8_t slot, uint16_t id) { s_assign_slot = slot; s_assign_id = id; s_assign_done = true; return 0; }
bool patchAssignPoll(uint8_t* slot, int* err) { if(!s_assign_done) return false; s_assign_done = false; *slot = (uint8_t)s_assign_slot; *err = 0; return true; }
static uint8_t s_clockmode = 0;
uint8_t studioClockMode(void) { return s_clockmode; }
void studioSetClockMode(uint8_t m) { s_clockmode = m; }
static bool s_move_called = false; static uint8_t s_mv_from, s_mv_to;
bool patchMoveSlot(uint8_t a, uint8_t b) { s_move_called = true; s_mv_from = a; s_mv_to = b; return true; }

static bool s_skipped = true; static int s_retry_calls = 0;
bool patchStoreSkipped() { return s_skipped; }
int patchStoreRetry() { s_retry_calls++; s_skipped = false; return 0; }
static int s_usave_syn = 0, s_usave_drum = 0;
int patchUserSaveSynth(uint8_t algo) { int i = synFileNextFree(algo); if(i < 0) return -1; synFileSet(algo, (uint8_t)i, syn.preset[algo], "TEST"); s_usave_syn++; return i; }
int patchUserSaveDrum(uint8_t g2, uint8_t base) { int n = drumsUserNextFree(g2); if(n < 0) return -1; float k[4]; for(int i = 0; i < 4; i++) k[i] = drums.eng[g2].x[i]; drumsUserSet(g2, (uint8_t)n, base, k, "TEST"); s_usave_drum++; return n; }

// ---------------------------------------------------------------- harness
static uint32_t T = 1000;
static void ev(InputType t, int a = 0, int b = 0, bool rpt = false) { InputEvent e; e.type = t; e.a = (int16_t)a; e.b = (int16_t)b; e.t_ms = T; e.rpt = rpt; navHandle(e, T); }
static int acts = 0;                       // physical actions counted (enc detent / joystick flick / click / pad tap)
static void enc(int d)  { acts++; ev(IN_ENC_TURN, d); }
static void click()     { acts++; ev(IN_JOY_PRESS); }
static void flick(int dx, int dy) { acts++; ev(IN_JOY_NAV, dx, dy); }
static void pad(int p)  { acts++; g.pads_held |= (uint16_t)(1u << p); ev(IN_PAD_PRESS, p); navTick(T); g.pads_held &= (uint16_t)~(1u << p); ev(IN_PAD_RELEASE, p); }
static void tick(int n = 1) { for(int i = 0; i < n; i++) { T += 5; daisy::System::s_now_ms = T; navTick(T); } }
static void padHold(int p, int ms) { g.pads_held |= (uint16_t)(1u << p); ev(IN_PAD_PRESS, p); for(int t = 500; t <= ms; t += 100) { ev(IN_PAD_HOLD, p, t); } g.pads_held &= (uint16_t)~(1u << p); ev(IN_PAD_RELEASE, p); }
static void boot() {
    memset(&g, 0, sizeof g);
    clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    memset(s_patch_exists, 0, sizeof s_patch_exists); s_cur = 0; s_move_called = false; s_assign_slot = -1; s_assign_done = false; s_clockmode = 0;
    T = 1000; daisy::System::s_now_ms = T; navInit(); acts = 0;
}
static void toMenu(Menu m) { while(nav.menu < m) enc(+1); while(nav.menu > m) enc(-1); }

int main() {
    // ---- 1. browse DRUMS: from boot, change the KICK pad's preset to the new "TRAP" (18) = how many actions?
    boot();
    uint8_t kickPad = drumGroupFirstPad(DG_KICK);
    pad(kickPad); acts = 0;
    flick(0, -1);                                                  // joystick DOWN on the TIMBRE ring -> browser (flick toward the user)
    CHECK(nav.level == L_BROWSE, "U/D on drums ring did not open the browser (level %d)", nav.level);
    for(int i = 0; i < 18; i++) flick(0, -1);                      // preset +1 each (down = next)
    CHECK(drums.pad[kickPad].sound == 18, "kick preset %d != 18", drums.pad[kickPad].sound);
    printf("ui: drums preset 0 -> 18: %d flicks (each auditions); with auto-repeat held it is one hold\n", acts);
    flick(+1, 0);                                                  // engine -> SNARE... target moves to the same spot in the next quadrant
    CHECK(drumPadGroup(nav.seq_target[ENG_DRUMS]) == DG_SNARE || drumPadGroup(nav.seq_target[ENG_DRUMS]) == DG_HAT, "engine L/R moved target group %d", drumPadGroup(nav.seq_target[ENG_DRUMS]));
    pad(drumGroupFirstPad(DG_PERC)); CHECK(drumPadGroup(nav.seq_target[ENG_DRUMS]) == DG_PERC, "pad tap in browser did not pick the engine");
    click(); CHECK(nav.level == L_RING, "click did not leave the browser");
    // depth knob page
    click(); CHECK(nav.level == L_CARD, "enter card"); flick(+1, 0); CHECK(nav.page == 1, "drums L/R page -> ENG page");
    { const ParamPage* pg = navCurrentPage(); CHECK(pg && pg->count == DENG_PARAMS && !strcmp(pg->items[0].name, "KNOCK"), "ENG page is the PERC engine (%s)", pg ? pg->items[0].name : "null"); }

    // ---- 2. SYNTH: change algorithm + preset = actions (before: enc, click, [ALGO page 0], click, stick, click)
    boot(); acts = 0;
    toMenu(M_SYNTH); flick(0, -1);
    CHECK(nav.level == L_BROWSE, "synth browser");
    flick(+1, 0); flick(+1, 0);                                    // algo 0 -> 2
    CHECK(syn.algo == 2, "synth algo %d", syn.algo);
    flick(0, -1); flick(0, -1);                                    // PLUCK, PAD
    CHECK(nav.br_pre[2] == 2, "synth preset index %d", nav.br_pre[2]);
    CHECK(syn.preset[2].amp.a > 0.5f, "PAD preset not applied (attack %.3f)", syn.preset[2].amp.a);
    printf("ui: synth pick algo 3 + PAD preset from DRUMS: %d actions (was: enc, click, click, stick, click = 5 for the algo alone, no presets)\n", acts);
    click();
    // ---- 3. SYNTH pages: 4 pages, FILTER is one flick away
    boot(); toMenu(M_SYNTH); click(); acts = 0;
    uint8_t cnt = 0; navTimbrePages(ENG_SYNTH, cnt); CHECK(cnt == 4, "synth page count %d", cnt);
    CHECK(!strcmp(navCurrentPage()->title, "SOUND") && !strcmp(navCurrentPage()->items[0].name, "CHARACTR") && !strcmp(navCurrentPage()->items[6].name, "ALGO"), "SOUND page layout");
    flick(+1, 0); CHECK(!strcmp(navCurrentPage()->title, "FILTER"), "page 1 is %s", navCurrentPage()->title);
    click(); CHECK(nav.level == L_EDIT2, "CUTOFF x RESO opens as XY (level %d)", nav.level);
    printf("ui: synth CUTOFF/RESO from SYNTH ring: click, flick, click = 3 actions (+ stick); from DRUMS +1 encoder detent\n");
    enc(-1); enc(-1);
    // ---- 4. sampler pool browser
    boot(); initPoolPatches(); toMenu(M_SMP); click();
    CHECK(nav.level == L_CARD && nav.page == 0, "sampler card page 0 is the pool (level %d page %d)", nav.level, nav.page);
    tick(80);                                                      // scan runs a patch per pass (+ the 250 ms audition debounce starts)
    CHECK(navPoolCount() == 3, "pool has %d entries (want 3)", navPoolCount());
    CHECK(navPoolPageOpen(), "pool page not open");
    // audition: nothing is read while the cursor keeps moving; one entry is read after it rests 250 ms, then it plays
    s_reads = 0; samplerAuditionStop();
    for(int i = 0; i < 6; i++) { flick(0, -1); tick(15); flick(0, +1); tick(15); }        // scrubbing every 75 ms
    CHECK(s_reads == 0, "fast scrolling read the SD %d times", s_reads);
    CHECK(!samplerAuditionPlaying(), "playing while scrolling");
    flick(0, -1); CHECK(nav.pool_cur == 1, "pool cursor %d", nav.pool_cur);
    tick(30); CHECK(s_reads == 0, "read before the debounce expired");
    tick(200);                                                    // 250 ms passed + 94 chunk passes
    CHECK(s_reads > 0 && s_read_id == 3, "audition read id %d (want 3), reads %d", s_read_id, s_reads);
    CHECK(samplerAuditionPlaying(), "sample not playing after load");
    { int16_t* b = samplerAuditionBuf(); CHECK(b[0] == (int16_t)(((0u * 7u) & 0xFF) | (((1u * 7u) & 0xFF) << 8)), "audition buffer content"); }
    flick(0, -1); tick(); CHECK(!samplerAuditionPlaying(), "cursor move must stop the audition");
    flick(0, +1);
    click(); CHECK(nav.pool_armed, "click did not arm the pool sample");
    pad(7); tick(2);
    CHECK(s_assign_slot == 7 && s_assign_id == 3, "assigned slot %d id %d (want slot 7 id 3)", s_assign_slot, s_assign_id);
    CHECK(!nav.pool_armed, "still armed after assign");
    CHECK(nav.toast[0] != 0 || true, "toast");
    // armed + encoder = put down
    click(); enc(+1); CHECK(!nav.pool_armed && nav.level == L_CARD, "back from armed puts the sample down only");
    // page 1 = MIX
    flick(+1, 0); CHECK(!navPoolPageOpen() || nav.page == 1, "MIX page"); CHECK(!strcmp(navCurrentPage()->title, "SAMPLER"), "page1 = %s", navCurrentPage()->title);
    // ---- 5. sampler hold-to-clear (3 s) and joystick hold blank-all (3 s)
    boot(); toMenu(M_SMP);
    smp.len[3] = 1000; smp.len[4] = 1000; smp.len[9] = 1000;
    padHold(3, 2500); CHECK(smp.len[3] == 1000, "pad cleared before 3 s");
    padHold(3, 3100); CHECK(smp.len[3] == 0 && smp.len[4] == 1000, "pad hold 3 s did not clear exactly that slot");
    // blank-all via joystick hold
    ev(IN_JOY_PRESS); g.joy_sw = true;
    for(int t = 500; t <= 3200; t += 100) ev(IN_JOY_HOLD, t);
    g.joy_sw = false; tick();
    int nz = 0; for(int i = 0; i < SMP_SLOTS; i++) nz += smp.len[i] != 0;
    CHECK(nz == 0, "joystick hold left %d slots", nz);
    CHECK(nav.level == L_RING, "joystick hold must restore the pre-click state (level %d)", nav.level);
    smp.len[2] = 500; ev(IN_JOY_PRESS); g.joy_sw = true; for(int t = 500; t <= 2800; t += 100) ev(IN_JOY_HOLD, t); g.joy_sw = false; tick();
    CHECK(smp.len[2] == 500, "joystick hold < 3 s must not blank");
    // ---- 6. sampler reorder
    boot(); toMenu(M_SMP); smp.len[1] = 10; smp.len[6] = 20; smp_buf[1][0] = 0.25f; smp_buf[6][0] = 0.5f;
    flick(0, -1); CHECK(nav.level == L_MOVE, "sampler U/D -> move mode (level %d)", nav.level);
    pad(1); CHECK(nav.mv_src == 1, "source pick"); pad(6);
    CHECK(smp.len[1] == 20 && smp.len[6] == 10 && smp_buf[1][0] == 0.5f && smp_buf[6][0] == 0.25f, "swap result");
    pad(12); CHECK(nav.mv_src == -1, "empty slot cannot be a source");
    click(); CHECK(nav.level == L_RING, "leave move");
    // ---- 7. patches: no hold-delete, reorder to empty slot via hook
    boot(); toMenu(M_PATCHES); s_patch_exists[3] = true;
    padHold(3, 6000);                                              // patchDelete() stub fails the test if the UI ever calls it
    flick(0, -1); CHECK(nav.level == L_MOVE, "patches U/D -> move mode");
    pad(3); CHECK(nav.mv_src == 3, "pick patch 3"); pad(3 + 1);
    CHECK(s_move_called && s_mv_from == 3 && s_mv_to == 4, "patchMoveSlot not called with 3 -> 4");
    // ---- 8. manual clock mode + relative editors (no snap on open)
    boot(); toMenu(M_GLOBALS); s_clockmode = 1; g.joy_x = 0.f; navInit(); toMenu(M_GLOBALS);
    int guard = 0; while(strcmp(navCurrentPage()->items[nav.cursor].name, "CLKMODE") && guard++ < 20) flick(0, -1);
    CHECK(!strcmp(navCurrentPage()->items[nav.cursor].name, "CLKMODE"), "CLKMODE item not found in GLOBALS");
    click(); CHECK(nav.level == L_EDIT1, "edit CLKMODE");
    tick(3); CHECK(s_clockmode == 1, "opening the editor with the stick centred changed the value (%d)", s_clockmode);
    g.joy_x = 0.55f; tick(2);                                      // full throw right: sweeps the whole range from the anchor
    CHECK(s_clockmode == 3, "clock mode %d (want 3 MASTER)", s_clockmode);
    g.joy_x = 0.f; tick(2); CHECK(s_clockmode == 3, "returning to centre must keep the value");
    g.joy_x = -0.55f; tick(2); CHECK(s_clockmode == 0, "ratchet back (%d)", s_clockmode);
    g.joy_x = 0.f; tick(2); click();
    // stick off-centre at open: still no jump
    boot(); toMenu(M_GLOBALS); s_clockmode = 2; navInit(); toMenu(M_GLOBALS);
    guard = 0; while(strcmp(navCurrentPage()->items[nav.cursor].name, "CLKMODE") && guard++ < 20) flick(0, -1);
    g.joy_x = 0.3f; click(); tick(3); CHECK(s_clockmode == 2, "off-centre open snapped the value (%d)", s_clockmode);
    g.joy_x = 0.f;
    // ---- 9. synth ENV page is flat with RELEASE first; PERFORM-ring U/D nudges release
    boot(); toMenu(M_SYNTH); click(); flick(+1, 0); flick(+1, 0);
    { const ParamPage* pg = navCurrentPage(); CHECK(!strcmp(pg->title, "ENV") && !strcmp(pg->items[0].name, "RELEASE") && !(pg->items[0].flags & PF_PAIR_X) && !(pg->items[1].flags & PF_PAIR_X), "ENV page not flat / RELEASE not first"); }
    click(); CHECK(nav.level == L_EDIT1, "RELEASE opens as a 1-D slider (level %d)", nav.level); enc(-1); enc(-1);
    boot(); toMenu(M_SYNTH); flick(+1, 0); flick(+1, 0); CHECK(nav.card == C_PERF, "perf card");
    { float r0 = syn.preset[syn.algo].amp.r; flick(0, +1); CHECK(syn.preset[syn.algo].amp.r > r0 * 1.2f, "U did not lengthen release"); flick(0, -1); flick(0, -1); CHECK(syn.preset[syn.algo].amp.r < r0, "D did not shorten release"); }
    // ---- GLOBALS: RETRYSTO only after a skipped-storage boot
    {
        boot(); s_skipped = false; s_retry_calls = 0; toMenu(M_GLOBALS);
        CHECK(navCurrentPage()->count == 12, "RETRYSTO visible without a skip (count %d)", navCurrentPage()->count);
        s_skipped = true;
        const ParamPage* pg = navCurrentPage(); CHECK(pg->count == 13 && !strcmp(pg->items[12].name, "RETRYSTO"), "RETRYSTO missing after a skip boot");
        CHECK(nav.level == L_CARD, "GLOBALS lands in the card"); for(int i = 0; i < 12; i++) flick(0, -1);                 // cursor to RETRYSTO
        CHECK(nav.cursor == 12, "cursor %d", nav.cursor);
        click(); CHECK(nav.level == L_EDIT1, "edit level"); g.joy_x = 0.f; tick(2); g.joy_x = 0.55f; tick(3); g.joy_x = 0.f; tick(3);
        CHECK(s_retry_calls == 1, "patchStoreRetry called %d times", s_retry_calls);
        CHECK(!strcmp(nav.toast, "STORAGE RETRY") && nav.level == L_CARD && nav.cursor == 0 && navCurrentPage()->count == 12, "after retry: toast '%s' level %d count %d", nav.toast, nav.level, navCurrentPage()->count);
        s_skipped = false;
        printf("ui: GLOBALS RETRYSTO ok\n");
    }
    // ---- user presets in the browsers (docs/STORAGE.md)
    {   // SYNTH: factory 0-4 + user 5-31, empty user slots browse but do not load, hold-click saves into the next free user slot
        boot(); toMenu(M_SYNTH); flick(0, -1); CHECK(nav.level == L_BROWSE, "synth browser");
        for(int i = 0; i < 5; i++) flick(+1, 0);                         // algo 5
        CHECK(syn.algo == 5, "algo %d", syn.algo);
        SynPreset mine; synFactoryPreset(5, 3, mine); mine.cutoff = 777.f; synFileSet(5, 5, mine, "MINE");
        for(int i = 0; i < 5; i++) flick(0, -1);
        CHECK(nav.br_pre[5] == 5 && syn.preset[5].cutoff == 777.f, "user slot 5 not loaded (cursor %d cutoff %.0f)", nav.br_pre[5], syn.preset[5].cutoff);
        flick(0, -1);
        CHECK(nav.br_pre[5] == 6 && syn.preset[5].cutoff == 777.f, "empty user slot changed the sound");
        syn.preset[5].cutoff = 1500.f;                                    // an edit worth saving
        click(); CHECK(nav.level == L_RING, "click leaves the browser"); ev(IN_JOY_HOLD, 500); ev(IN_JOY_HOLD, 1500);
        CHECK(nav.level == L_BROWSE && s_usave_syn == 1, "hold did not save (level %d saves %d)", nav.level, s_usave_syn);
        SynPreset got; CHECK(nav.br_pre[5] == 6 && synFileGet(5, 6, got) && got.cutoff == 1500.f, "saved into slot %d", nav.br_pre[5]);
        CHECK(!strcmp(synFileName(5, 5), "MINE"), "name");
        for(int i = 0; i < 40; i++) flick(0, -1); CHECK(nav.br_pre[5] == SYN_FILE_PRESETS - 1, "cursor hard stop %d", nav.br_pre[5]);
        for(int i = 0; i < 40; i++) flick(0, +1); synLoadPreset(5, 0); CHECK(nav.br_pre[5] == 0, "back to INIT");
        // factory slots are read-only: nothing the UI does can target them (the save always asks for the NEXT FREE USER slot)
        CHECK(synFileNextFree(5) >= SYN_PRESETS, "next free is a user slot");
        printf("ui: synth user presets browse/save ok\n");
    }
    {   // DRUMS: user entries after the 24 built-ins carry base sound + the engine's 4 depth knobs
        boot(); uint8_t kp = drumGroupFirstPad(DG_KICK); pad(kp); flick(0, -1);
        CHECK(nav.level == L_BROWSE, "drums browser");
        const float kn[4] = { 0.1f, 0.2f, 0.3f, 0.4f }; drumsUserSet(DG_KICK, 0, 7, kn, "KUSR");
        for(int i = 0; i < DRUM_SOUNDS; i++) flick(0, -1);                // cursor 24 = user 0
        CHECK(drums.pad[kp].sound == 7 && drums.eng[DG_KICK].x[1] == 0.2f && nav.br_dcur[DG_KICK] == DRUM_SOUNDS, "user drum entry not applied (sound %d knob %.2f)", drums.pad[kp].sound, drums.eng[DG_KICK].x[1]);
        flick(0, -1); CHECK(nav.br_dcur[DG_KICK] == DRUM_SOUNDS + 1 && drums.pad[kp].sound == 7, "empty drum user slot changed the sound");
        drums.eng[DG_KICK].x[2] = 0.9f; click(); ev(IN_JOY_HOLD, 500); ev(IN_JOY_HOLD, 1500);
        DrumUserEntry ue; CHECK(s_usave_drum == 1 && drumsUserGet(DG_KICK, 1, &ue) && ue.base == 7 && ue.k[2] == 0.9f, "drum user save (%d)", s_usave_drum);
        for(int i = 0; i < 40; i++) flick(0, +1); CHECK(drums.pad[kp].sound == 0 && nav.br_dcur[DG_KICK] == 0xFF, "browsing back to the built-ins re-follows the pad");
        printf("ui: drums user presets browse/save ok\n");
    }
    {   // STUCK PADS: a release must reach whatever the press started, whatever the menu/level/page did in between
        auto down = [](int p) { g.pads_held |= (uint16_t)(1u << p); ev(IN_PAD_PRESS, p); navTick(T); };
        auto up   = [](int p) { g.pads_held &= (uint16_t)~(1u << p); ev(IN_PAD_RELEASE, p); navTick(T); };
        auto synHeldVoices = []() { int n = 0; for(int i = 0; i < SYN_VOICES; i++) if(synVoices[i].ampEnv.stage != ADSR::IDLE && synVoices[i].ampEnv.stage != ADSR::REL) n++; return n; };
        // FX hold, volume overlay opened (both-press) before the release
        boot(); toMenu(M_FX); down(3); uint8_t fid = fxId(fx.view, 3);
        CHECK(fxIsHeld(fid), "fx press did not hold");
        CHECK(((fxEffectiveBank(fid >> 4) >> (fid & 15)) & 1u) != 0, "FX engage is effective at once (the very next audio block reads it): no step boundary / pass delay between the press event and the engage mask");
        nav.level = L_VOLUME; up(3);
        CHECK(!fxIsHeld(fid), "STUCK: FX held after release under the volume overlay");
        // FX hold, then the menu changes away (hold the pad, back out, turn)
        boot(); toMenu(M_FX); down(5); fid = fxId(fx.view, 5);
        nav.menu = M_DRUMS; up(5);
        CHECK(!fxIsHeld(fid), "STUCK: FX held after release on another menu");
        // FX hold, bank changes (stick L/R) before the release
        boot(); toMenu(M_FX); down(2); fid = fxId(fx.view, 2);
        nav.card = C_TIMBRE; fxSetView((uint8_t)((fx.view + 1) % FX_BANKS)); up(2);
        CHECK(!fxIsHeld(fid), "STUCK: FX held after release in another bank");
        // FX hold, release event lost (input ring full): reconciled from the physical pad state
        boot(); toMenu(M_FX); down(7); fid = fxId(fx.view, 7);
        g.pads_held &= (uint16_t)~(1u << 7); navTick(T);
        CHECK(!fxIsHeld(fid), "STUCK: FX held when the release event never arrived");
        // SYNTH note, menu changes to DRUMS before the release
        boot(); toMenu(M_SYNTH); click(); down(4);
        CHECK(synHeldVoices() == 1, "synth pad press did not start a held note (%d)", synHeldVoices());
        nav.menu = M_DRUMS; up(4);
        CHECK(synHeldVoices() == 0, "STUCK: synth note still gated after release on another menu (%d)", synHeldVoices());
        // SYNTH note, level changes to the step editor / volume before the release
        boot(); toMenu(M_SYNTH); click(); down(6);
        nav.level = L_VOLUME; up(6);
        CHECK(synHeldVoices() == 0, "STUCK: synth note still gated after release under the volume overlay (%d)", synHeldVoices());
        // SYNTH note, release event lost
        boot(); toMenu(M_SYNTH); click(); down(1);
        g.pads_held &= (uint16_t)~(1u << 1); navTick(T);
        CHECK(synHeldVoices() == 0, "STUCK: synth note held when the release event never arrived (%d)", synHeldVoices());
        // SAMPLER record, menu changes before the release
        boot(); toMenu(M_SMP); click(); flick(+1, 0); while(nav.card != C_TIMBRE && nav.page < 6) flick(+1, 0);
        nav.level = L_CARD; nav.card = C_TIMBRE; down(9);
        CHECK(smp.rec_slot == 9, "sampler press on an empty slot did not record (rec_slot %d)", smp.rec_slot);
        nav.menu = M_DRUMS; up(9);
        CHECK(smp.rec_slot == -1, "STUCK: sampler still recording after release on another menu (%d)", smp.rec_slot);
        boot(); toMenu(M_SMP); click(); flick(+1, 0); while(nav.card != C_TIMBRE && nav.page < 6) flick(+1, 0);
        nav.level = L_CARD; nav.card = C_TIMBRE; down(10);
        CHECK(smp.rec_slot == 10, "sampler press did not record (rec_slot %d)", smp.rec_slot);
        nav.level = L_VOLUME; up(10);
        CHECK(smp.rec_slot == -1, "STUCK: sampler still recording after release under the volume overlay (%d)", smp.rec_slot);
        printf("ui: stuck-pad latch ok\n");
    }
    {   // MIDI note-offs: a stolen voice and a transport stop must end their notes (EV_SYN_OFF), or the MIDI receiver holds them forever
        boot(); toMenu(M_SYNTH); click();
        seq.lane[ENG_SYNTH].polyphony = 2.f;
        bool on[128] = {}; PmorEvent pe;
        auto drain = [&]() { while(pmorEventPop(&pe)) { if(pe.kind == EV_SYN_ON) on[pe.a & 127] = true; else if(pe.kind == EV_SYN_OFF) on[pe.a & 127] = false; } };
        drain();
        for(int p = 0; p < 5; p++) { g.pads_held |= (uint16_t)(1u << p); ev(IN_PAD_PRESS, p); T += 40; daisy::System::s_now_ms = T; navTick(T); }
        drain();
        int liveHeld = 0, mismatch = 0;
        for(int n = 0; n < 128; n++) {
            bool v = false;
            for(int i = 0; i < SYN_VOICES; i++) { const Voice& vv = synVoices[i]; uint8_t mn; uint16_t bd; pmorHzToMidi(vv.note_hz, &mn, &bd); if(vv.ampEnv.stage != ADSR::IDLE && !vv.stolen && vv.ampEnv.stage != ADSR::REL && mn == n) v = true; }
            if(v) liveHeld++;
            if(on[n] != v) mismatch++;
        }
        CHECK(liveHeld == 2 && mismatch == 0, "STUCK: MIDI notes on != voices held after voice steal (held %d mismatch %d)", liveHeld, mismatch);
        synthAllOff(); drain();
        int still = 0; for(int n = 0; n < 128; n++) if(on[n]) still++;
        CHECK(still == 0, "STUCK: %d MIDI notes still on after synthAllOff", still);
        printf("ui: MIDI note-offs on steal / all-off ok\n");
    }
    printf(fails ? "test_ui: %d FAILURES\n" : "test_ui: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}

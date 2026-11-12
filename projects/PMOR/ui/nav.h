// nav.h — the whole interface state machine. One rule set everywhere:
//   encoder turn = move through the current ring (hard stops)
//   encoder press = TAP TEMPO, always
//   joystick click = push into / set / back out / un-focus, by context
//   both = volume shortcut (undoes the first single press)
//   joystick move = nav (flick+repeat) or param edit or performance, by context
#pragma once
#include <stdint.h>
#include "input.h"
#include "../pmor_core/param.h"
#include "../pmor_core/sequencer.h"
#include "../pmor_core/synth.h"

#define UI_PAD_CLEAR_MS  3000   // hold a sampler pad: clear that slot (RAM only, no file is touched)
#define UI_JOY_SAVE_MS   1500   // hold the joystick button in the DRUMS/SYNTH browser: save the current sound as a USER preset
#define UI_JOY_BLANK_MS  3000   // hold the joystick button in SAMPLER: blank ALL slots (no file is touched)

enum Menu : uint8_t { M_DRUMS = 0, M_SYNTH, M_SMP, M_FX, M_PATCHES, M_GLOBALS, M_DASH, M_COUNT };
extern const char* const MENU_NAMES[M_COUNT];

enum Card : uint8_t { C_TIMBRE = 0, C_SEQ, C_PERF, C_COUNT };
extern const char* const CARD_NAMES[C_COUNT];

enum NavLevel : uint8_t {
    L_RING = 0,     // top: menu name + three cards (engines) or the page itself
    L_CARD,         // inside a card: list of params / sub-items
    L_EDIT1,        // full-screen 1-D slider
    L_EDIT2,        // full-screen XY pad
    L_PADCOUNT,     // bars / steps chooser on the pads
    L_HANDSEQ,      // hand step editor (drums/synth/sampler): pads = the steps of PAGE hand_bar for lane seq_target[e].
                    // Enter/exit = joystick UP or DOWN on the SEQ card ring; L/R = page; click = next lane.
    L_VOLUME,       // both-button overlay
    L_BROWSE,       // sound browser (DRUMS, SYNTH): L/R = engine, U/D = its presets. Entered by joystick U/D on the TIMBRE ring.
    L_MOVE,         // reorder mode (SAMPLER slots, PATCHES): tap a source pad then a target pad. Entered by joystick U/D on the ring.
};

struct NavState {
    Menu     menu;
    Card     card;            // engines only
    NavLevel level;
    uint8_t  cursor;          // item index inside the card list
    // editors
    const Param* edit;        // current 1-D param (or X of a pair)
    // focus (drums/sampler timbre): pad focused for swiping sounds / pitch-formant
    int8_t   focus_pad;       // -1 none
    // performance
    bool     perf_active;     // engine performance card is open
    // screensaver
    uint32_t last_nav_ms;
    bool     saver;
    // volume overlay
    bool     vol_overlay;
    // patches
    uint8_t  patch_page;      // 0..3
    // undo of the first press when a chord arrives
    InputEvent last_single;   // what the last single press did (for IN_BOTH_PRESS reversal)
    // additive (ui/input.cpp + ui/nav.cpp), report per AGENT_RULES:
    uint32_t hold_ms;         // current pad-hold duration, for delete-countdown rendering (PATCHES/SAMPLER)
    int8_t   hold_pad;        // which pad is being held for that countdown, -1 = none
    uint8_t  page;            // page index inside the current card (e.g. synth TIMBRE sub-pages)
    uint32_t dive_ms;         // ms timestamp of the last main-menu change (page-switch animation)
    uint8_t  seq_target[ENG_COUNT]; // per engine: the lane (pad) the step editor writes; set by the last pad played, or the click in the editor
    uint8_t  hand_bar;        // step editor: PAGE (= bar) being shown
    uint8_t  br_pre[ALG_COUNT]; // synth browser: cursor per algorithm, 0..SYN_FILE_PRESETS-1 (0-4 factory, 5-31 user; empty user slots can be browsed but do not load)
    uint8_t  br_dcur[4];     // [DG_COUNT] // drums browser: cursor per engine when it is on a USER entry (DRUM_SOUNDS + n); 0xFF = follow the focused pad's sound
    int8_t   mv_src;          // L_MOVE: picked source pad, -1 = none yet
    uint16_t pool_cur;        // sampler pool browser: highlighted entry
    bool     pool_armed;      // a pool sample is picked and flashing; the next pad tap assigns it
    uint32_t joy_hold_ms;     // joystick-button hold progress (SAMPLER blank-all countdown), 0 = not holding
    char     toast[16];       // one-line feedback on the OLED
    uint32_t toast_until;
};
extern NavState nav;

void navInit();
void navHandle(const InputEvent& e, uint32_t now_ms);   // consumes one event
void navTick(uint32_t now_ms);                          // timeouts: screensaver, overlays
// pages ask nav what to draw
const ParamPage* navCurrentPage();
// engines expose their card pages
const ParamPage* navTimbrePages(Engine e, uint8_t& count);   // list of pages in TIMBRE card

// sampler pool browser (nav.cpp): the sample list is gathered a patch per tick while the pool page is open
uint16_t navPoolCount();
bool     navPoolEntry(uint16_t i, uint8_t& patch, uint8_t& slot, uint32_t& len);
bool     navPoolScanning();
bool     navPoolPageOpen();      // true while the sampler pool browser is what the card shows

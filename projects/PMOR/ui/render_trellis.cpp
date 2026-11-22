// render_trellis.cpp — trellisRenderFrame(): lava base + page overlay + brightness.
// CONTROL plane only. Pad index 0 = top-left, row-major (row*4+col); hw/trellis.cpp
// applies the physical rotation remap after this. Output is GRB per pad (48 bytes).
#include "render.h"
#include "../pmor_core/render.h"   // pmorFollowedRun()
#include "palette.h"
#include "nav.h"
#include "../config.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/sampler.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/sequencer.h"
#include "../pmor_core/output.h"
#include "../store/patch.h"

// g_trellis_brightness is defined in hw/trellis.cpp (declared in render.h).

// TASK 9 (2026-10): g_trellis_brightness_ctrl is the user-facing linear control (the GLOBALS
// "BRIGHT" knob, 0..1, also what glob.trellis_brightness persists) — see the long comment in
// render.h for why it has to be a separate variable from g_trellis_brightness. Default matches
// hw/trellis.cpp's own g_trellis_brightness default (0.45) so the two agree before the first
// GLOBALS edit or the first patchInit() boot-apply overwrites either one.
float g_trellis_brightness_ctrl = 0.45f;

// Perceptual gamma: an approximate gamma-2 curve (linear01 squared). LED drive is linear in
// photons but the eye is not — a straight 0..1 multiply (what hw/trellis.cpp's single scale
// does) spends most of the knob's travel barely changing anything and then rockets up near the
// top. Squaring the control value before it reaches that multiply gives the bottom of the
// travel real resolution and the top a gentler final approach, so the knob reads as even
// brightness steps instead of "does nothing, then suddenly bright." Cheap (one multiply, no
// powf) — fine to call every navTick on an edit.
float trellisBrightnessGamma(float linear01) {
    if (linear01 < 0.f) linear01 = 0.f;
    if (linear01 > 1.f) linear01 = 1.f;
    return linear01 * linear01;
}

// ---------------------------------------------------------------------------
// Optional NavState fields: nav.cpp (written concurrently) may add hold_ms,
// hold_pad, page at the end of NavState. Detect at compile time (SFINAE tag
// dispatch) so this file builds whether or not they exist yet, and falls back
// to sensible defaults when absent (no hold-to-delete visual, page 0).
namespace navopt {
template<typename T> static auto holdMs(const T& n, int) -> decltype((uint32_t)n.hold_ms)  { return (uint32_t)n.hold_ms; }
template<typename T> static uint32_t holdMs(const T&, long) { return 0u; }

template<typename T> static auto holdPad(const T& n, int) -> decltype((int16_t)n.hold_pad) { return (int16_t)n.hold_pad; }
template<typename T> static int16_t holdPad(const T&, long) { return -1; }

template<typename T> static auto page(const T& n, int) -> decltype((uint8_t)n.page) { return (uint8_t)n.page; }
template<typename T> static uint8_t page(const T&, long) { return 0u; }
} // namespace navopt

// ---------------------------------------------------------------------------
// lava base: slow per-pad drifting blend of LAVA A/B/C, ~6% brightness, no pad
// ever fully off. Per-pad speed varies (ported drift idea from V2 pphase/DRIFT).
static const uint8_t LAVA_DRIFT[16] = {1,2,1,2, 2,1,2,1, 1,2,2,1, 2,1,1,2};

static Rgb lavaBase(uint8_t phase) {
    // 3 equal segments across the 0..255 wheel: A->B->C->A
    uint8_t seg = (uint8_t)(phase / 86); if (seg > 2) seg = 2;
    uint8_t rem = (uint8_t)(phase - seg * 86);
    float   t   = (float)rem / 85.f;
    const Rgb& a = (seg == 0) ? C_LAVA_A : (seg == 1) ? C_LAVA_B : C_LAVA_C;
    const Rgb& b = (seg == 0) ? C_LAVA_B : (seg == 1) ? C_LAVA_C : C_LAVA_A;
    return rgbLerp(a, b, t);
}

static void fillLavaBase(Rgb pad[16], uint32_t now_ms) {
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t phase = (uint8_t)(((now_ms >> 5) * LAVA_DRIFT[i]) + i * 23u);
        pad[i] = rgbScale(lavaBase(phase), 0.06f);
    }
}

// ---------------------------------------------------------------------------
// helpers shared by the overlays

static inline bool padHeld(uint8_t pad) { return ((g.pads_held >> pad) & 1u) != 0; }

// ---------------------------------------------------------------------------
// DRUMS

static Rgb drumGroupColor(DrumGroup grp) {
    switch (grp) {
        case DG_KICK:  return C_KICKS;
        case DG_SNARE: return C_SNARES;
        case DG_HAT:   return C_HATS;
        default:       return C_PERC;
    }
}

static void overlayDrums(Rgb pad[16]) {
    for (uint8_t p = 0; p < 16; p++) {
        if (nav.focus_pad == (int8_t)p) { pad[p] = C_FOCUS; continue; }
        Rgb tint = drumGroupColor(drumPadGroup(p));
        pad[p] = rgbScale(tint, padHeld(p) ? 0.95f : 0.30f);
    }
    // sound browser: the pad being browsed is white, its whole quadrant (= the engine) is lit brighter
    if (nav.level == L_BROWSE) {
        uint8_t t = nav.seq_target[ENG_DRUMS] & 15;
        DrumGroup tg = drumPadGroup(t);
        for (uint8_t p = 0; p < 16; p++) if (drumPadGroup(p) == tg) pad[p] = rgbScale(drumGroupColor(tg), 0.80f);
        pad[t] = C_FOCUS;
    }
}

// ---------------------------------------------------------------------------
// SYNTH

static void overlaySynth(Rgb pad[16]) {
    for (uint8_t p = 0; p < 16; p++)
        pad[p] = rgbScale(C_SYNTH, padHeld(p) ? 0.95f : 0.25f);
}

// ---------------------------------------------------------------------------
// SAMPLER

static void overlaySampler(Rgb pad[16], uint32_t now_ms) {
    for (uint8_t p = 0; p < 16; p++) {
        bool filled = smp.len[p] > 0;
        pad[p] = rgbScale(C_FILLED, filled ? 0.95f : 0.18f);
        if (!filled) pad[p] = rgbScale(C_SAMPLER, 0.18f);
    }
    {   // shared 600 ms triangle pulse for the pick/flash states below
        uint32_t ph = now_ms % 600u;
        float t = (ph < 300u) ? (float)ph / 300.f : 1.f - (float)(ph - 300u) / 300.f;
        if (nav.pool_armed && navPoolPageOpen()) {
            // a pool sample is picked: EVERY pad flashes. Empty pads flash bright (free target), filled pads flash red (would be replaced).
            for (uint8_t p = 0; p < 16; p++)
                pad[p] = (smp.len[p] > 0) ? rgbLerp(C_DEL3, C_DEL1, t) : rgbLerp(rgbScale(C_SAMPLER, 0.25f), C_CURSOR, t);
        }
        if (nav.level == L_MOVE && nav.mv_src >= 0) pad[nav.mv_src & 15] = rgbLerp(C_FILLED, C_CURSOR, t);
    }
    if (smp.rec_slot >= 0 && smp.rec_slot < 16) {
        // pulse red: triangle wave 0..1 over ~600 ms
        uint32_t ph = now_ms % 600u;
        float t = (ph < 300u) ? (float)ph / 300.f : 1.f - (float)(ph - 300u) / 300.f;
        pad[smp.rec_slot] = rgbLerp(C_DEL1, C_FOCUS, t);
    }
    int16_t hold_pad = navopt::holdPad(nav, 0);
    if (hold_pad >= 0 && hold_pad < 16) {
        uint32_t hold_ms = navopt::holdMs(nav, 0);
        float t = (float)hold_ms / (float)UI_PAD_CLEAR_MS;     // 3 s to clear a sampler pad
        pad[hold_pad] = rgbDeleteRamp(t);
    }
}

// ---------------------------------------------------------------------------
// FX

// v2 FX BANKS: one bank per page, one colour per bank (EMGOR's palette, no red/orange).
// Held pads at full brightness, idle pads dim — v2 FXC[bank] idle = >>4, held = full.
static const Rgb* const FX_BANK_COL[8] = { &C_HOME, &C_UP, &C_LEFT, &C_DOWN, &C_RIGHT, &C_SYNTH, &C_PATCHES, &C_GLOBALS };
static void overlayFx(Rgb pad[16]) {
    uint8_t bank = fx.view < 8 ? fx.view : 0;
    Rgb base = *FX_BANK_COL[bank];
    for (uint8_t p = 0; p < 16; p++) {
        bool held = fxIsHeld(fxId(bank, p));
        // a touch of white toward the bottom rows so the 4 rows read as rows (v2 had a hue per row)
        Rgb row = rgbLerp(base, C_CURSOR, 0.10f * (float)(p / 4));
        pad[p] = rgbScale(row, held ? 1.0f : 0.30f);
    }
}

// ---------------------------------------------------------------------------
// PATCHES

static void overlayPatches(Rgb pad[16], uint32_t now_ms) {
    uint8_t base = (uint8_t)(nav.patch_page * 16);
    uint8_t cur = patchCurrent();
    for (uint8_t p = 0; p < 16; p++) {
        uint8_t slot = (uint8_t)(base + p);
        bool exists = patchExists(slot);
        Rgb c = exists ? rgbScale(C_PATCHES, 0.85f) : rgbScale(C_PATCHES, 0.12f);
        if (slot == cur) c = C_FOCUS;
        if (nav.cursor == p) c = rgbLerp(c, C_CURSOR, 0.6f);
        pad[p] = c;
    }
    if (nav.level == L_MOVE) {          // reorder: picked source pulses, EMPTY slots (valid targets) breathe
        uint32_t ph = now_ms % 600u;
        float t = (ph < 300u) ? (float)ph / 300.f : 1.f - (float)(ph - 300u) / 300.f;
        if (nav.mv_src >= 0) {
            for (uint8_t p = 0; p < 16; p++) if (!patchExists((uint8_t)(base + p))) pad[p] = rgbLerp(rgbScale(C_PATCHES, 0.2f), C_PATCHES, t);
            pad[nav.mv_src & 15] = rgbLerp(C_PATCHES, C_CURSOR, t);
        }
    }
}

// ---------------------------------------------------------------------------
// L_VOLUME overlay: green -> amber -> red bar up to the fader level

static void overlayVolume(Rgb pad[16]) {
    float frac = outp.fader / 1.3333f;
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    int pidx = (int)(frac * 16.f); if (pidx > 15) pidx = 15;
    // fader: cyan up to 100 %, yellow past it (the clipper zone), pink at the top — no red
    for (int p = 0; p <= pidx; p++) {
        Rgb c = (p <= 10) ? C_UP : (p <= 13) ? C_DOWN : C_RIGHT;
        if (p == pidx) c = rgbLerp(c, C_FOCUS, 0.35f);
        pad[p] = c;
    }
}

// ---------------------------------------------------------------------------
// L_PADCOUNT overlay: pads 0..(n-1) lit in engine colour

static void overlayPadCount(Rgb pad[16]) {
    Engine e = (nav.menu == M_SYNTH) ? ENG_SYNTH : (nav.menu == M_SMP) ? ENG_SMP : ENG_DRUMS;
    Rgb col = (e == ENG_SYNTH) ? C_SYNTH : (e == ENG_SMP) ? C_SAMPLER : C_DRUMS;
    uint8_t n = seq.lane[e].steps_per_bar;
    if (n > 16) n = 16;
    for (uint8_t p = 0; p < n; p++) pad[p] = rgbScale(col, 0.9f);
}

// ---------------------------------------------------------------------------

// g_trellis_brightness itself is DEFINED in hw/trellis.cpp (declared extern in
// render.h) — we only read it here, per spec step 3.

void trellisRenderFrame(uint32_t now_ms, uint8_t grb_out[48]) {
    Rgb pad[16];
    fillLavaBase(pad, now_ms);

    if (nav.level == L_VOLUME) {
        overlayVolume(pad);
    } else if (nav.level == L_HANDSEQ) {
        // Step editor: pad p = step p of the shown page for the target lane. Lit = a hit; the playhead
        // washes whichever step it is on (only on the page it is actually in). Pads past STEPS stay dark.
        Engine e = (nav.menu == M_SYNTH) ? ENG_SYNTH : (nav.menu == M_SMP) ? ENG_SMP : ENG_DRUMS;
        const LaneSet& l = seq.lane[e];
        uint8_t spb = l.steps_per_bar ? l.steps_per_bar : 16;
        uint8_t lane = nav.seq_target[e] & 15;
        Rgb eng = (e == ENG_SYNTH) ? C_SYNTH : (e == ENG_SMP) ? C_SAMPLER : drumGroupColor(drumPadGroup(lane));
        uint32_t loop = seqLoopLen(l); if (!loop) loop = 1;
        uint32_t playhead = pmorFollowedRun() ? g.gstep % loop : 0xFFFFFFFFu;   // stopped (followed transport): no playhead
        for (int st = 0; st < 16; st++) {
            if (st >= spb) { pad[st] = rgbScale(pad[st], 0.3f); continue; }
            uint32_t idx = (uint32_t)nav.hand_bar * spb + st;
            bool on = idx < MAXSTEPS && ((l.step[idx] >> lane) & 1u);
            pad[st] = on ? rgbScale(eng, 0.95f) : rgbScale(eng, 0.12f);
            if (idx == playhead) pad[st] = rgbLerp(pad[st], C_CURSOR, 0.6f);
        }
    } else if (nav.level == L_PADCOUNT) {
        overlayPadCount(pad);
    } else {
        switch (nav.menu) {
            case M_DRUMS:   overlayDrums(pad); break;
            case M_SYNTH:   overlaySynth(pad); break;
            case M_SMP:     overlaySampler(pad, now_ms); break;
            case M_FX:      overlayFx(pad); break;
            case M_PATCHES: overlayPatches(pad, now_ms); break;
            case M_GLOBALS:
            case M_DASH:
            default: break; // dim lava only
        }
    }

    // g_trellis_brightness (0..1, from GLOBALS) is NOT applied here: hw/trellis.cpp's
    // trellisSetFrame() is the one and only place that scales by it before the I2C
    // push (checked: applied exactly once, in hw/).
    for (uint8_t i = 0; i < 16; i++) {
        Rgb c = pad[i];
        grb_out[i * 3 + 0] = c.g;
        grb_out[i * 3 + 1] = c.r;
        grb_out[i * 3 + 2] = c.b;
    }
}

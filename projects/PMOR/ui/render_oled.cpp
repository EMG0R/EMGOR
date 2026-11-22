// render_oled.cpp — oledRenderFrame() + widgets. CONTROL plane only, 128x64 landscape.
#include "render.h"
#include "nav.h"
#include "../config.h"
#include "../hw/hw.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/param.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/sequencer.h"
#include "../pmor_core/output.h"
#include "../pmor_core/clock.h"
#include "../pmor_core/render.h"   // TASK 4: clk.gstep/step_phase for the metro dot — audio plane owns it, read-only here
#include "../store/patch.h"
#include "../store/files.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/synth.h"
#include <stdio.h>
#include <string.h>

// tinyusb symbol, forward-declared rather than pulling in the full stack (owned by usb/).
#include "tusb.h"

// ---------------------------------------------------------------------------
// value formatting shared by wList / wSlider

static void fmtParamValue(char* out, size_t n, const Param& p, float v) {
    if (p.flags & PF_ENUM) {
        int idx = (int)v;
        if (idx < 0) idx = 0;
        snprintf(out, n, "%s", p.names ? p.names[idx] : "?");
    } else if (p.flags & PF_PERCENT) {
        snprintf(out, n, "%3d%%", (int)(paramNorm(p, v) * 100.f + 0.5f));
    } else if (p.flags & PF_INT) {
        snprintf(out, n, "%d", (int)(v + 0.5f));
    } else if (p.flags & PF_BIPOLAR) {
        snprintf(out, n, "%+.2f", (double)v);
    } else {
        snprintf(out, n, "%.2f", (double)v);
    }
}

// ---------------------------------------------------------------------------
// widgets

void wTitle(const char* s) {
    // TASK 4 (2026-10): x shifted from 2 -> 8. x=2,y=2 top-left is now the metro dot
    // (wMetro(), drawn centrally once per frame in oledRenderFrame — not every page calls
    // wTitle, e.g. ringFx()/renderPadCount()/wXY(), so the dot can't live inside this
    // function and still be "visible on every page" as specced).
    oledText(8, 0, s, true);
}

void wCards(const char* const* names, int n, int sel) {
    // three big rounded cards, left -> right, filling the 128x64 area below the title
    int cardW = 38, cardH = 40, gap = 4;
    int totalW = n * cardW + (n - 1) * gap;
    int x0 = (128 - totalW) / 2; if (x0 < 1) x0 = 1;
    int y0 = 16;
    for (int i = 0; i < n; i++) {
        int x = x0 + i * (cardW + gap);
        bool selected = (i == sel);
        oledRoundRect(x, y0, x + cardW, y0 + cardH, 6, true, selected);
        int tw = (int)strlen(names[i]) * 6;
        int tx = x + (cardW - tw) / 2; if (tx < x + 1) tx = x + 1;
        int ty = y0 + cardH / 2 - 4;
        oledText(tx, ty, names[i], !selected);
    }
}

void wList(const ParamPage* page, int cursor, int top) {
    if (!page || page->count == 0) return;
    const int rowH = 8, y0 = 10, rowsVisible = 6;
    for (int r = 0; r < rowsVisible; r++) {
        int idx = top + r;
        if (idx >= page->count) break;
        const Param& p = page->items[idx];
        int y = y0 + r * rowH;
        bool sel = (idx == cursor);
        if (sel) oledRect(0, y - 1, 127, y + 7, true, true);
        char line[24];
        char val[12];
        fmtParamValue(val, sizeof val, p, *p.value);
        snprintf(line, sizeof line, " %-8s %6s", p.name, val);
        oledText(0, y, line, !sel);
    }
}

void wSlider(const Param& p, float value) {
    wTitle(p.name);
    char val[16];
    fmtParamValue(val, sizeof val, p, value);
    oledTextBig(2, 12, val, true);
    int x0 = 4, x1 = 124, y0 = 40, y1 = 54;
    oledRect(x0, y0, x1, y1, true, false);
    float n = paramNorm(p, value);
    if (n < 0.f) n = 0.f;
    if (n > 1.f) n = 1.f;
    int fillW = (int)((x1 - x0 - 2) * n + 0.5f);
    if (fillW > 0) oledRect(x0 + 1, y0 + 1, x0 + 1 + fillW, y1 - 1, true, true);
}

void wXY(const Param& px, const Param& py, float vx, float vy) {
    int x0 = 4, y0 = 8, side = 56;
    oledRect(x0, y0, x0 + side, y0 + side, true, false);
    float nx = paramNorm(px, vx); if (nx < 0.f) nx = 0.f; if (nx > 1.f) nx = 1.f;
    float ny = paramNorm(py, vy); if (ny < 0.f) ny = 0.f; if (ny > 1.f) ny = 1.f;
    int dx = x0 + 1 + (int)((side - 2) * nx);
    int dy = y0 + 1 + (int)((side - 2) * (1.f - ny));   // Y up = higher value
    oledRect(dx - 1, dy - 1, dx + 1, dy + 1, true, true);

    int tx = x0 + side + 8;
    char val[12];
    oledText(tx, 8, px.name, true);
    fmtParamValue(val, sizeof val, px, vx);
    oledText(tx, 18, val, true);
    oledText(tx, 34, py.name, true);
    fmtParamValue(val, sizeof val, py, vy);
    oledText(tx, 44, val, true);
}

// Spinner VERBATIM port of PMOR_V2 L5446-5465; solid star ditto. TASK 3 (2026-10): the third,
// dirty/outline state is new — patchSaving() alone is only the multi-second chunked
// sampler-audio write (aud_saving), so the indicator used to show "saved" almost permanently
// and only ever spun during a .AUD save; it never reflected whether your actual edits (the
// 4 Hz CRC autosave in store/patch.cpp) had reached the card. patchDirty() already existed,
// correct, and called from nowhere — wired in here. Same top-right spot, three honest states:
//   dirty (patchDirty(), not yet written)  -> hollow star: the same five tip points as the
//     solid star below, left unconnected so it reads as an outline at a glance
//   writing right now (patchSaving())      -> the existing 8-phase orbiting spinner
//   committed to SD (neither)              -> the existing solid 5-point star
void wSavingStar(bool busy, bool dirty, bool show) {
    if (!show) return;
    if (busy) {
        static int sp = 0; sp = (sp + 1) & 7;
        int cx = 124, cy = 4;
        int dx = (sp < 2) ? 2 : (sp < 4) ? 0 : (sp < 6) ? -2 : 0;
        int dy = (sp < 2) ? 0 : (sp < 4) ? 2 : (sp < 6) ? 0 : -2;
        oledPixel(cx + dx, cy + dy, true);
        oledPixel(cx, cy, true);
    } else if (dirty) {
        int cx = 123, cy = 4;
        oledPixel(cx,     cy - 3, true);
        oledPixel(cx - 3, cy + 3, true);
        oledPixel(cx + 3, cy + 3, true);
        oledPixel(cx - 3, cy - 1, true);
        oledPixel(cx + 3, cy - 1, true);
    } else {
        int cx = 123, cy = 4;
        oledPixel(cx,     cy - 3, true);
        oledPixel(cx,     cy - 2, true);
        oledLine(cx - 3, cy + 3, cx + 3, cy - 1, true);
        oledLine(cx + 3, cy + 3, cx - 3, cy - 1, true);
        oledLine(cx - 3, cy - 1, cx + 3, cy - 1, true);
    }
}

// ---------------------------------------------------------------------------
// SCREENSAVER — the v2 harmonograph figure (two counter-rotating Lissajous loops,
// shaped by the accelerometer) rendered as PARTICLES travelling along the outline
// instead of a drawn line. Size is amplitude-NORMALISED (a slow envelope of the
// output level divides the instantaneous level) so quiet and loud both fill the
// panel; the loud/quiet difference shows as spin speed and particle spread.
// Pixel budget: <= 96 lit pixels, far under the OLED-boost load that killed USB.
// Never drawn in play mode — only when nav.saver is set.
#include <math.h>
void wParticles(float amp, uint32_t now_ms, bool saver) {
    (void)now_ms;
    if (!saver) return;                                   // play mode: nothing (per EMGOR)
    static float env = 0.05f, ph = 0.f, sx = 0.f, sy = 0.f, sz = 0.f, spin = 0.f;
    static float u[96];
    static bool  init = false;
    static uint8_t parity = 0;
    if (!init) { for (int i = 0; i < 96; i++) u[i] = (float)i / 96.f; init = true; }

    if (amp > env) env += (amp - env) * 0.08f; else env += (amp - env) * 0.004f;
    if (env < 0.02f) env = 0.02f;
    float norm = amp / env; if (norm > 1.5f) norm = 1.5f;   // ~1 when steady, spikes on hits
    sx += (g.accel_x - sx) * 0.05f;
    sy += (g.accel_y - sy) * 0.05f;
    sz += (g.accel_z - sz) * 0.05f;
    spin += ((0.6f + norm) * 0.6f - spin) * 0.1f;

    parity ^= 1u;
    if (parity) {                                          // advance every other frame
        ph += 0.010f + spin * 0.012f;
        if (ph > 6.2831853f) ph -= 6.2831853f;
        for (int i = 0; i < 96; i++) {
            u[i] += 0.0025f + 0.004f * norm + 0.0008f * (float)(i & 7);
            if (u[i] >= 1.f) u[i] -= 1.f;
        }
    }
    float fa = 2.0f + sx * 1.6f, fb = 3.0f + sy * 1.6f, fc = 4.0f + sz * 1.6f, fd = 5.0f - sx * 1.4f;
    float R  = 17.5f + 17.5f * (norm > 1.f ? 1.f : norm);   // +25 % (EMGOR 2026-09-15): ~104 x 58 px at full swing, clipped at the edges
    for (int i = 0; i < 96; i++) {
        int  pass = i & 1;
        float dir = pass ? -1.f : 1.f;
        float rr  = pass ? R * 0.62f : R;
        float t   = u[i] * 6.2831853f;
        float fx = 64.f + rr * ( sinf(fa * t + ph * dir) + 0.5f * sinf(fc * t - ph * 0.7f) );
        float fy = 32.f + rr * 0.55f * ( sinf(fb * t + ph * 1.1f * dir) + 0.5f * sinf(fd * t) );
        int xi = (int)fx, yi = (int)fy;
        if (xi < 0) xi = 0; else if (xi > 127) xi = 127;
        if (yi < 0) yi = 0; else if (yi > 63)  yi = 63;
        oledPixel(xi, yi, true);
    }
}

// ---------------------------------------------------------------------------
// Tunnel of nested rectangles — the v2 intro / menu-dive figure (V2 L4255-4289, L5111-5128)
static void tunnel(float prog, bool outward) {
    for (int k = 0; k < 5; k++) {
        float s = prog * 0.99f + (float)k * 0.2f;
        s -= (int)s;
        if (outward) s = 1.f - s;
        s *= s;
        int w = 3 + (int)(s * 61.f);
        int h = 2 + (int)(s * 30.f);
        oledRect(64 - w, 32 - h, 63 + w, 31 + h, true, false);
    }
}

// Boot intro: the tunnel with "PMOR" punching in. Pumps USB every frame (see V2 notes:
// a hard spin here drops the device off the bus).
void oledIntro() {
    for (int f = 0; f < 20; f++) {
        oledClear();
        tunnel((float)f / 19.f * 2.f, false);
        if (f >= 4) oledTextBig(50, 27, "PMOR", true);
        oledFlush();
        usbDelay(30);
    }
}

// ---------------------------------------------------------------------------
// L_RING sub-pages

static void ringEngine() {
    wTitle(MENU_NAMES[nav.menu]);
    wCards(CARD_NAMES, C_COUNT, nav.card);
    if (nav.card == C_SEQ && seqWaitingFirstHit(nav.menu == M_DRUMS ? ENG_DRUMS : nav.menu == M_SYNTH ? ENG_SYNTH : ENG_SMP)) oledText(8, 57, "WAITING FOR 1ST HIT", true);   // REC armed on an empty lane: the loop starts at the first hit
    else if (nav.card == C_SEQ) oledText(8, 57, "UP/DN = STEP EDIT", true);   // the one hint worth the pixels: how to get into the editor
}

static void ringFx() {
    // v2 FX page (PMOR_V2.cpp L5290-5319): "< BANK >" header, the HELD effect names as rows,
    // FSR bar, FSR % + BPM, REC badge. Nothing else — the catalogue is never listed.
    char row[24];
    snprintf(row, sizeof row, "\x11 FX%d \x10", (int)(fx.view % 8) + 1);   // v2: "< FXn >"
    oledText(46, 0, row, true);
    int ln = 0;
    for (uint8_t id = 0; id < FX_TOTAL && ln < 3; id++) {
        if (fxIsHeld(id)) { oledText(2, 11 + ln * 10, fxName(id), true); ln++; }
    }
    if (ln == 0) oledText(2, 11, "HOLD PADS  L/R=BANK", true);
    int fw = (int)(g.fsr * 124.f);
    oledRect(2, 44, 126, 52, true, false);
    if (fw > 0) oledRect(2, 44, 2 + fw, 52, true, true);
    snprintf(row, sizeof row, "FSR%3d%%   BPM:%3d", (int)(g.fsr * 100.f), (int)(g.bpm_shown + 0.5f));   // received tempo when slaved
    oledText(2, 56, row, true);
    if (fx.auto_rec) { oledRect(0, 0, 23, 9, true, true); oledText(2, 1, "REC", false); }
}

static void ringPatches() {
    wTitle("PATCHES");
    uint8_t base = (uint8_t)(nav.patch_page * 16);
    for (int i = 0; i < 16; i++) {
        int x0 = 2 + (i % 4) * 14, y0 = 12 + (i / 4) * 10;
        bool ex = patchExists((uint8_t)(base + i));
        oledRect(x0, y0, x0 + 11, y0 + 8, true, ex);
        if (i == nav.cursor)
            oledRect(x0 - 1, y0 - 1, x0 + 12, y0 + 9, true, false);
    }
    char pl[2] = { (char)('A' + nav.patch_page), 0 };
    oledText(118, 12, pl, true);
}

static void dashPage() {
    char row[24];
    int y = 10, ln = 8;
    snprintf(row, sizeof row, "A%+5d%+5d%+5d", (int)(g.accel_x * 1000.f), (int)(g.accel_y * 1000.f), (int)(g.accel_z * 1000.f));
    oledText(0, y, row, true); y += ln;
    snprintf(row, sizeof row, "J%+5d%+5d %s", g.joy_raw_x, g.joy_raw_y, g.joy_calibrated ? "CAL" : "---");
    oledText(0, y, row, true); y += ln;
    snprintf(row, sizeof row, "D:%4dmm S:%02X", (int)lidarMm(), lidarStatus());
    oledText(0, y, row, true); y += ln;
    snprintf(row, sizeof row, "F:%3d%%", (int)(g.fsr * 100.f));
    oledText(0, y, row, true); y += ln;
    snprintf(row, sizeof row, "CPU:%3d%% BPM:%3d", (int)(g.cpu_load * 100.f), (int)(g.bpm_shown + 0.5f));
    oledText(0, y, row, true); y += ln;
    snprintf(row, sizeof row, "SD:%-2s USB:%-2s", sdOk() ? "OK" : "--", tud_cdc_connected() ? "OK" : "--");
    oledText(0, y, row, true); y += ln;
    {   // SD save health: saves ok/failed, boot load result, last failing step + FRESULT
        const PatchDiag& d = patchDiag();
        snprintf(row, sizeof row, "SV:%u/%u Q:%u LD:%u %c%u %s", (unsigned)d.saves_ok, (unsigned)d.saves_fail,
                 (unsigned)d.qspi_saves, (unsigned)d.boot_load, file_diag.op ? file_diag.op : '-',
                 (unsigned)file_diag.fr, sdIsFast() ? "F" : "S");
        oledText(0, y, row, true);
    }
}

static void ringGlobalsDash() {
    wTitle(MENU_NAMES[nav.menu]);
    if (nav.menu == M_DASH) dashPage();
    else wList(navCurrentPage(), nav.cursor, nav.cursor >= 6 ? nav.cursor - 5 : 0);
}

static void renderRing(uint32_t now_ms) {
    (void)now_ms;
    switch (nav.menu) {
        case M_DRUMS: case M_SYNTH: case M_SMP: ringEngine(); break;
        case M_FX:      ringFx();      break;
        case M_PATCHES: ringPatches(); break;
        case M_GLOBALS: case M_DASH: default: ringGlobalsDash(); break;
    }
}

// ---------------------------------------------------------------------------
// L_BROWSE: engine tabs + preset list. DRUMS: KICK SNARE HAT PERC tabs; SYNTH: "< ALGO >". 5 preset rows around the current one,
// a '*' marks the stage-2 presets (16.. in DRUMS).
static void renderBrowse() {
    char row[28];
    int cur, total; const char* ename; const char* const* names;
    if(nav.menu == M_DRUMS) {
        uint8_t t = nav.seq_target[ENG_DRUMS] & 15;
        int g = (int)drumPadGroup(t);
        cur = nav.br_dcur[g] != 0xFF ? nav.br_dcur[g] : drums.pad[t].sound; total = DRUM_SOUNDS + DRUM_USER_SLOTS; ename = DRUM_GROUP_NAMES[g]; names = DRUM_SOUND_NAMES[g];
        snprintf(row, sizeof row, "%s %d/%d", ename, cur + 1, total);
        wTitle(row);
        int x = 2;
        for(int i = 0; i < DG_COUNT; i++) {
            int w = (int)strlen(DRUM_GROUP_NAMES[i]) * 6;
            if(i == g) { oledRect(x - 2, 9, x + w + 1, 18, true, true); oledText(x, 10, DRUM_GROUP_NAMES[i], false); }
            else oledText(x, 10, DRUM_GROUP_NAMES[i], true);
            x += w + 8;
        }
    } else {
        cur = nav.br_pre[syn.algo]; total = SYN_FILE_PRESETS; names = SYN_PRESET_NAMES;
        snprintf(row, sizeof row, "SYNTH %d/%d", cur + 1, total);
        wTitle(row);
        snprintf(row, sizeof row, "\x11 %s \x10", SYN_ALGO_NAMES[syn.algo]);
        oledRect(0, 9, 127, 18, true, true);
        oledText(4, 10, row, false);
    }
    int top = cur - 2; if(top < 0) top = 0; if(top > total - 5) top = total - 5; if(top < 0) top = 0;
    for(int r = 0; r < 5 && top + r < total; r++) {
        int i = top + r, y = 21 + r * 8;
        bool sel = (i == cur);
        if(sel) oledRect(0, y - 1, 127, y + 7, true, true);
        const char* nm = nullptr; bool usr = false;
        if(nav.menu == M_DRUMS) {
            if(i < DRUM_SOUNDS) nm = names[i];
            else { DrumUserEntry ue; usr = true; static char un[12]; if(drumsUserGet((uint8_t)drumPadGroup(nav.seq_target[ENG_DRUMS] & 15), (uint8_t)(i - DRUM_SOUNDS), &ue)) { snprintf(un, sizeof un, "%s", ue.name); nm = un; } }
        } else {
            if(i < SYN_PRESETS) nm = names[i];
            else { usr = true; nm = synFileName(syn.algo, (uint8_t)i); }
        }
        snprintf(row, sizeof row, " %2d %-8s%s", i + 1, nm ? (nm[0] ? nm : "USER") : "-EMPTY-", (nav.menu == M_DRUMS && !usr && i >= DRUM_CLASSIC_SOUNDS) ? "*" : "");
        oledText(0, y, row, !sel);
    }
}

// L_MOVE: reorder
static void renderMove() {
    wTitle(nav.menu == M_SMP ? "MOVE SAMPLES" : "MOVE PATCHES");
    oledTextBig(6, 18, nav.mv_src < 0 ? "TAP SOURCE" : "TAP TARGET", true);
    oledText(0, 44, nav.menu == M_SMP ? "SWAPS THE 2 SLOTS" : "TARGET MUST BE EMPTY", true);
    oledText(0, 55, "FLICK / CLICK = EXIT", true);
}

// sampler POOL page: every sample on the card. Click picks (grid flashes), tap a pad assigns.
static void renderPool() {
    char row[28];
    uint16_t n = navPoolCount();
    snprintf(row, sizeof row, "POOL %d/%d%s", n ? nav.pool_cur + 1 : 0, n, navPoolScanning() ? " .." : "");
    wTitle(row);
    if(n == 0) { oledText(0, 24, navPoolScanning() ? "SCANNING..." : "NO SAMPLES ON CARD", true); return; }
    int top = nav.pool_cur >= 5 ? nav.pool_cur - 4 : 0;
    for(int r = 0; r < 5 && top + r < n; r++) {
        uint8_t patch, slot; uint32_t len;
        navPoolEntry((uint16_t)(top + r), patch, slot, len);
        int y = 10 + r * 8; bool sel = (top + r == nav.pool_cur);
        if(sel) oledRect(0, y - 1, 127, y + 7, true, true);
        int tenths = (int)(len * 10u / 48000u);
        snprintf(row, sizeof row, " %c%02d.%02d  %d.%ds", 'A' + patch / 16, patch % 16 + 1, slot + 1, tenths / 10, tenths % 10);
        oledText(0, y, row, !sel);
    }
    if(nav.pool_armed) { oledRect(0, 54, 127, 63, true, true); oledText(6, 55, "TAP A PAD TO ASSIGN", false); }
    else oledText(0, 56, "CLICK = PICK", true);
}

// feedback overlays shared by every page: hold countdown (blank all), one-line toast
static void renderOverlays(uint32_t now_ms) {
    (void)now_ms;
    if((nav.menu == M_DRUMS || nav.menu == M_SYNTH) && nav.joy_hold_ms >= 500) {   // only set while a browser hold is in progress
        float t = (float)nav.joy_hold_ms / (float)UI_JOY_SAVE_MS; if(t > 1.f) t = 1.f;
        oledRect(0, 52, 127, 63, true, true);
        oledRect(2, 54, 2 + (int)(123.f * t), 61, false, true);
        oledText(34, 55, "SAVE USER", t < 0.5f ? false : true);
    }
    if(nav.menu == M_SMP && nav.joy_hold_ms >= 500) {
        float t = (float)nav.joy_hold_ms / (float)UI_JOY_BLANK_MS; if(t > 1.f) t = 1.f;
        oledRect(0, 52, 127, 63, true, true);
        oledRect(2, 54, 2 + (int)(123.f * t), 61, false, true);
        oledText(34, 55, "BLANK ALL", t < 0.5f ? false : true);
    }
    if(nav.toast[0]) {
        int w = (int)strlen(nav.toast) * 6 + 6;
        oledRect(64 - w / 2, 28, 64 + w / 2, 38, true, true);
        oledText(64 - w / 2 + 3, 30, nav.toast, false);
    }
}

// ---------------------------------------------------------------------------
// L_PADCOUNT / L_VOLUME

static void renderPadCount() {
    Engine e = (nav.menu == M_SYNTH) ? ENG_SYNTH : (nav.menu == M_SMP) ? ENG_SMP : ENG_DRUMS;
    char row[16];
    snprintf(row, sizeof row, "BARS: %d", seq.lane[e].bars);
    oledTextBig(6, 16, row, true);
    snprintf(row, sizeof row, "STEPS: %d", seq.lane[e].steps_per_bar);
    oledTextBig(6, 36, row, true);
}

static void renderHandSeq() {
    Engine e = (nav.menu == M_SYNTH) ? ENG_SYNTH : (nav.menu == M_SMP) ? ENG_SMP : ENG_DRUMS;
    const LaneSet& l = seq.lane[e];
    uint8_t spb = l.steps_per_bar ? l.steps_per_bar : 16;
    uint8_t lane = nav.seq_target[e] & 15;
    uint32_t loop = seqLoopLen(l); if (!loop) loop = 1;
    uint32_t playhead = pmorFollowedRun() ? g.gstep % loop : 0xFFFFFFFFu;   // stopped (followed transport): no playhead
    uint32_t page = nav.hand_bar < l.bars ? nav.hand_bar : (uint32_t)(l.bars ? l.bars - 1 : 0);
    char row[28];
    static const char* const ENG_TAG[ENG_COUNT] = { "DRUMS", "SYNTH", "SMP" };
    snprintf(row, sizeof row, "%s PAD %d", ENG_TAG[e], lane + 1);
    wTitle(row);
    static const char* const MODE_TAG[SEQ_MODE_COUNT] = { "MAN", "PLAY", "GEN" };
    int m = (int)(l.mode_f + 0.5f); if (m < 0) m = 0; if (m >= SEQ_MODE_COUNT) m = SEQ_MODE_COUNT - 1;
    snprintf(row, sizeof row, "PAGE %d/%d  %s", (int)page + 1, (int)l.bars, MODE_TAG[m]);
    oledText(0, 12, row, true);
    // 16 step boxes, lit where this lane has a hit; the playhead is underlined on whichever page it is in
    for (int st = 0; st < 16; st++) {
        int x0 = 2 + st * 8, y0 = 26, y1 = 42;
        bool inbar = st < spb;
        uint32_t idx = page * spb + st;
        bool on = inbar && idx < MAXSTEPS && ((l.step[idx] >> lane) & 1u);
        if (inbar) {
            oledRect(x0, y0, x0 + 6, y1, true, on);
            if ((st & 3) == 0) oledPixel(x0 + 3, y0 - 3, true);    // beat ticks above every 4th step
        }
        if (inbar && idx == playhead) oledLine(x0, y1 + 3, x0 + 6, y1 + 3, true);
    }
    oledText(0, 46, "L/R PAGE  CLK LANE", true);
    oledText(0, 55, "UP/DN EXIT", true);
}

static void renderVolume() {
    wTitle("VOLUME");
    float frac = outp.fader / 1.3333f;
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    int x0 = 4, x1 = 124, y0 = 26, y1 = 46;
    oledRect(x0, y0, x1, y1, true, false);
    int fillW = (int)((x1 - x0 - 2) * frac + 0.5f);
    if (fillW > 0) oledRect(x0 + 1, y0 + 1, x0 + 1 + fillW, y1 - 1, true, true);
    char pct[8]; snprintf(pct, sizeof pct, "%3d%%", (int)(frac * 100.f + 0.5f));
    oledTextBig(48, 50, pct, true);
}

// ---------------------------------------------------------------------------
// TASK 4 (2026-10): metro dot, top-left (x=2,y=2 — wTitle's old spot, shifted above), on
// every page. Driven off the real master grid (engine/clock.h's clk, audio-plane owned,
// read-only from here) rather than a free-running UI timer, so it tracks tap-tempo and
// tempo changes exactly instead of drifting against the actual clock. A beat = 4 steps of
// the 16th grid; the downbeat (bar start) is taken as gstep % 16 == 0 — 16 steps/bar is the
// PO-style default everywhere else in this UI (STEPS defaults to 16), even though an
// individual engine's own loop can run a different length.
//
// No real brightness on a monochrome panel, so "decaying over ~80-120 ms" is drawn as the
// dot shrinking from a filled box down to a single pixel then off; the downbeat gets the
// bigger box (5x5 vs 3x3) and the longer window so it reads distinctly at a glance.
//
// Collision: ringFx() draws a filled REC badge over (0,0)-(23,9) when fx.auto_rec is set
// (render_oled.cpp ringFx, below) — directly on top of this dot's whole range. Drawing the
// dot "lit" there would be invisible against the badge's own fill anyway, so skip it outright
// rather than fight the badge for the corner.
static void wMetro() {
    if (nav.menu == M_FX && fx.auto_rec) return;   // REC badge owns the top-left corner this frame
    if (!pmorFollowedRun()) return;                 // followed transport stopped: the grid keeps counting, the dot does not
    if ((clk.gstep % 4) != 0) return;               // decayed out well before the next 16th at any sane tempo
    bool  downbeat = (clk.gstep % 16) == 0;
    float elapsed  = clk.step_phase * clockStepMs();
    float window   = downbeat ? 120.f : 80.f;
    if (elapsed >= window) return;
    float t = 1.f - elapsed / window;               // 1 = fresh hit .. 0 = decayed
    int half = downbeat ? ((t > 0.6f) ? 2 : (t > 0.3f) ? 1 : 0)
                         : ((t > 0.5f) ? 1 : 0);
    int cx = 2, cy = 2;
    oledRect(cx - half, cy - half, cx + half, cy + half, true, true);
}

// ---------------------------------------------------------------------------
// CLOCK LINK readout (2026-10-05), top-right, every page: tempo + clock_mode letter.
// The tempo shown is g.bpm_shown (engine/transport_link.cpp): the RECEIVED transport tempo while
// SLAVED (so it can never disagree with the Pi/NEPTR), the local clock otherwise. The letter is
// I (internal / standalone), S (slave: following the Pi), M (master: driving the bus), drawn as an
// inverted box so mode reads at a glance. The whole readout inverts for ~120 ms when a tap has just
// been handed to the Pi (a slaved tap does not move the local clock, so this is the "got it" cue).
// snprintf has no %f in this build (newlib-nano), so tenths are formatted as integers.
static void wTempo(uint32_t now_ms) {
    int tenths = (int)(g.bpm_shown * 10.f + 0.5f);
    if (tenths < 0) tenths = 0;
    char num[16];
    snprintf(num, sizeof num, "%d.%d", tenths / 10, tenths % 10);
    int  len = (int)strlen(num);
    char m   = (g.clock_mode == 1) ? 'S' : (g.clock_mode == 2) ? 'M' : 'I';
    char mc[2] = { m, 0 };
    bool flash = (now_ms - g.tap_sent_ms) < 120u && g.tap_sent_ms != 0;
    int  bx = 121, nx = bx - 2 - len * 6;
    if (flash) oledRect(nx - 1, 0, 127, 8, true, true);
    else       oledRect(bx, 0, 127, 8, true, true);
    oledText(nx, 0, num, !flash);
    oledText(bx + 1, 0, mc, false);               // knocked out of the filled box in both states
}

void oledRenderFrame(uint32_t now_ms) {
    oledClear();

    if (nav.saver) {
        // metro dot intentionally does NOT survive the screensaver: wParticles is a dedicated
        // full-screen idle animation (its own comment: "play mode: nothing" in reverse — the
        // screensaver opts OUT of the normal UI, same reasoning wSavingStar already uses to
        // hide here), so leave it undisturbed rather than bolt a timing readout onto it.
        wParticles(g.out_amp, now_ms, true);
        return;
    }
    // menu-dive transition (~180 ms, 6 frames at 30 fps): tunnel + the new menu's name
    if (nav.dive_ms && (now_ms - nav.dive_ms) < (uint32_t)DIVE_MS) {
        float prog = (float)(now_ms - nav.dive_ms) / (float)DIVE_MS;
        tunnel(prog, false);
        const char* nm = MENU_NAMES[nav.menu];
        int len = 0; while (nm[len]) len++;
        oledText(64 - len * 3, 29, nm, true);
        wMetro();   // DOES survive the dive: it's a timing reference, not decoration, and the
                    // tunnel's rectangles sit centred on-screen, not in the corner.
        return;
    }

    switch (nav.level) {
        case L_RING:
            renderRing(now_ms);
            break;
        case L_CARD: {
            if(navPoolPageOpen()) { renderPool(); break; }
            const ParamPage* page = navCurrentPage();
            wTitle(page ? page->title : CARD_NAMES[nav.card]);
            int top = (nav.cursor >= 6) ? (nav.cursor - 5) : 0;
            wList(page, nav.cursor, top);
            break;
        }
        case L_EDIT1:
            if (nav.edit) wSlider(*nav.edit, *nav.edit->value);
            break;
        case L_EDIT2:
            if (nav.edit) {
                const Param& px = *nav.edit;
                const ParamPage* page = navCurrentPage();
                // Y is the next Param in the owning page (PF_PAIR_X convention).
                const Param* py = &px;
                if (page) {
                    for (uint8_t i = 0; i + 1 < page->count; i++) {
                        if (&page->items[i] == &px) { py = &page->items[i + 1]; break; }
                    }
                }
                wXY(px, *py, *px.value, *py->value);
            }
            break;
        case L_PADCOUNT:
            renderPadCount();
            break;
        case L_HANDSEQ:
            renderHandSeq();
            break;
        case L_VOLUME:
            renderVolume();
            break;
        case L_BROWSE:
            renderBrowse();
            break;
        case L_MOVE:
            renderMove();
            break;
    }
    renderOverlays(now_ms);

    wMetro();
    wTempo(now_ms);
    wSavingStar(patchSaving(), patchDirty(), !nav.saver && nav.level != L_VOLUME);
}

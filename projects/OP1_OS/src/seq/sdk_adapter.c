/* sdk_adapter.c - thin op1-sdk glue for the generative sequencer, in the NOTE ROLL pattern
 * (vendor/op1-sdk/features/noteroll/noteroll.c). Hook points (feature.json "shared"):
 *
 *   osq_init   init           once
 *   osq_tick   seq_tick       Seq thread, once per 64-frame block: runs the clock, plays note events
 *   osq_key    on_key  filter while the screen is open: T1-T4 pick a page group (same T again flips
 *                      sub-pages), knob pushes = seq_push; BLUE+... see docs/SEQUENCER.md
 *   osq_knob   on_knob filter while open: seq_encoder (shift = coarse)
 *   osq_frame  frame          draws the page + the mode's viz grid while open
 *   osq_set_open(int)         the platform layer decides which gesture opens the screen (the
 *                             sequencer-slot takeover is the platform builder's), this file only
 *                             owns what happens once it is open
 *   osq_midi_clock/start/stop the USB MIDI clock in stubs (call from the MIDI-in path)
 *
 * Output: absolute MIDI notes into TE's synth voice (TE_VOICE_NOTE_ON/OFF, synth playmode only),
 * like NOTE ROLL. The per-step value stream (seq_mod.h) is readable through osq_mod().
 * UNVERIFIED on hardware: the build is bfin-elf only; behaviour is the host-tested core plus the
 * same call patterns NOTE ROLL uses. No recipe: the platform builder owns recipes/. */
#include <op1/te246.h>
#include <op1/gfx.h>
#include "seq.h"

#define BLOCK TE_AUDIO_FRAMES

static seq_host S;
static volatile uint8_t inited, open_, group, sub;
static volatile uint8_t consumed;           /* bit: push key whose release is ours */

static void play(int on, int note, int vel)
{
    te_note n;
    n.note = note;
    n.vel = on ? vel : 0;
    n.midi_out = 1;
    n.pad[0] = n.pad[1] = n.pad[2] = 0;
    n.extra = 0;
    if (on) TE_VOICE_NOTE_ON(&n, 0); else TE_VOICE_NOTE_OFF(&n);
}

void osq_init(void)
{
    seq_init(&S, 0x0FE11004u);
    inited = 1;
}

void osq_set_open(int open) { open_ = (uint8_t)(open != 0); }
const seq_mod *osq_mod(void) { return seq_mod_get(&S); }
seq_host *osq_host(void) { return &S; }

void osq_midi_clock(void) { if (inited) seq_ext_pulse(&S); }
void osq_midi_start(void) { if (inited) seq_ext_start(&S); }
void osq_midi_stop(void)  { if (inited) seq_ext_stop(&S); }

/* a note played on the keyboard while the sequencer is armed feeds MARKOV / TOTAL */
void osq_note_in(int note, int vel, int on) { if (inited) seq_note_in(&S, note, vel, on); }

void osq_tick(void)
{
    seq_event e;
    float b;
    if (!inited) return;
    if (*TE_PLAYMODE != 0 || *TE_PLAYMODE_TARGET != 0) {      /* synth playmode only */
        if (S.running) { seq_stop(&S); while (seq_event_pop(&S, &e)) if (!e.type) play(0, e.note, 0); }
        return;
    }
    b = *TE_BPM;                                              /* TE's tempo is the master tempo */
    if (b >= 40.0f && b <= 240.0f) seq_set_bpm_x100(&S, (uint32_t)(b * 100.0f + 0.5f));
    seq_run_frames(&S, BLOCK, TE_SAMPLE_RATE);
    while (seq_event_pop(&S, &e)) play(e.type, e.note, e.vel);
}

int osq_key(int key, int down)
{
    int k;
    if (!inited || !open_) return 0;
    if (key >= TE_KEY_T1 && key <= TE_KEY_T4) {
        if (down) {
            int g = key - TE_KEY_T1;
            if (g == group) sub = (uint8_t)((sub + 1) % seq_ui_subpages(&S, g));
            else { group = (uint8_t)g; sub = 0; }
        }
        return 1;
    }
    k = key - TE_KEY_BLUE_PUSH;
    if (k >= 0 && k < 4) {
        if (down) { seq_push(&S, group, sub, k); consumed |= (uint8_t)(1 << k); return 1; }
        if (consumed >> k & 1) { consumed &= (uint8_t)~(1 << k); return 1; }
    }
    return 0;
}

int osq_knob(int knob, int16_t *diff)
{
    int d = *diff > 0 ? 1 : *diff < 0 ? -1 : 0;
    if (!inited || !open_ || (unsigned)knob > 3) return 0;
    if (d) seq_encoder(&S, group, sub, knob, d, *TE_SHIFT_HELD);
    return 1;
}

void osq_frame(uint16_t *buf)
{
    static const uint16_t col[4] = { GFX_RGB(80,120,255), GFX_RGB(60,220,120), GFX_WHITE, GFX_RGB(255,150,40) };
    seq_ui_cell c[4];
    seq_viz v;
    int i, x, y, cw;
    if (!inited || !open_) return;
    gfx_begin(buf);
    gfx_rect(0, 0, 320, 156, GFX_RGB(8, 0, 16));
    gfx_text(4, 3, seq_mode_name(S.mode), GFX_RGB(190, 120, 255), 2);
    gfx_text(120, 3, "T", GFX_WHITE, 2);
    gfx_dec(132, 3, group + 1, GFX_WHITE, 2);
    gfx_text(144, 3, ".", GFX_WHITE, 2);
    gfx_dec(152, 3, sub + 1, GFX_WHITE, 2);
    gfx_text(250, 3, S.running ? "RUN" : "STOP", S.running ? GFX_GREEN : GFX_RED, 2);
    seq_ui_page(&S, group, sub, c);
    for (i = 0; i < 4; i++) {
        x = 4 + i * 78;
        gfx_rect(x, 24, 72, 2, col[i]);
        gfx_text(x, 30, c[i].name, col[i], 1);
        if (c[i].text) gfx_text(x, 40, c[i].text, GFX_WHITE, 1);
        else gfx_dec(x, 40, c[i].value, GFX_WHITE, 2);
    }
    seq_viz_get(&S, &v);
    cw = v.w ? 300 / v.w : 0;
    for (y = 0; y < v.h; y++)
        for (x = 0; x < v.w; x++) {
            uint16_t g = (uint16_t)(v.cell[y][x] / 3);
            gfx_rect(10 + x * cw, 70 + y * 6, cw - 1, 5, GFX_RGB(g, g / 3, g + 20));
        }
    if (v.playhead >= 0) gfx_rect(10 + v.playhead * cw, 66, cw - 1, 2, GFX_WHITE);
}

/* ofp_seq glue: the generative sequencer (sdk_adapter.c, osq_*) in the sequencer slot of the OP-1.
 *
 * Slot: TE's synth sequencer of type TOMBOLA (sequencer key, synth playmode, tombola picked). Tombola's controller ignores every
 * key (docs/RESEARCH.md (b), te-op1-cfw sequencer-interface.md), so the screen is free. Stock tombola still exists underneath: with no
 * balls on its screen it makes no notes. The other five stock sequencers are untouched (not hidden: LEGACY banks are unproven).
 * Gesture: ORANGE knob push on that screen opens OP FREAK SEQ; SHIFT + ORANGE push closes it. While open: T1-T4 pick the page group
 * (same T again flips sub-pages), turn = value (shift = coarse), push = the page action; the sequencer keeps running when the
 * screen is closed or left (osq_tick), RUN/STOP is orange push on T4.
 *   os_key / os_knob / os_frame   gate on "synth playmode, sequencer screen, tombola picked", then osq_*
 *   os_note_on / os_note_off      keyboard notes feed MARKOV / TOTAL (observe only)
 * The tick (osq_tick) and init (osq_init) are the adapter's own. */
#include <op1/te246.h>
#include "sdk_adapter.c"

static volatile uint8_t gconsumed;

static int slot_screen(void)
{
    return *TE_PLAYMODE == 0 && *TE_PLAYMODE_TARGET == 0 && *TE_EDIT_MODE == TE_EDIT_MODE_SEQ
           && *TE_SYNTH_SEQ_TYPE == TE_SEQ_TOMBOLA;
}

int os_key(int key, int down)
{
    int b = key - TE_KEY_BLUE_PUSH;

    if (!inited)
        return 0;
    if (!slot_screen()) {
        open_ = 0;
        return 0;
    }
    if (b == 3) {
        if (!down && (gconsumed & 1)) {
            gconsumed &= (uint8_t)~1;
            return 1;
        }
        if (down && !open_ && !*TE_SHIFT_HELD) {            /* open */
            open_ = 1;
            gconsumed |= 1;
            return 1;
        }
        if (down && open_ && *TE_SHIFT_HELD) {              /* close */
            open_ = 0;
            gconsumed |= 1;
            return 1;
        }
    }
    return osq_key(key, down);
}

int os_knob(int knob, int16_t *diff)
{
    if (!slot_screen()) {
        open_ = 0;
        return 0;
    }
    return osq_knob(knob, diff);
}

void os_frame(uint16_t *buf)
{
    if (!slot_screen()) {
        open_ = 0;
        return;
    }
    osq_frame(buf);
}

void os_note_on(void *note, int extra)
{
    const te_note *n = (const te_note *)note;

    (void)extra;
    osq_note_in(n->note, n->vel < 0 ? 127 : n->vel, 1);
}

void os_note_off(void *note)
{
    const te_note *n = (const te_note *)note;

    osq_note_in(n->note, 0, 0);
}

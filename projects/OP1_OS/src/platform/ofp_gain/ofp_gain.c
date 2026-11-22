/* Glue: pins TE's own volume to maximum and runs the OP FREAK gain stage on the output block, driven by the knob.
 *
 * TE reads the volume knob (TWI, callback 0xffa0521c in L1) into the byte KNOB at 0x018b62a4; the output ramp at
 * 0x01a10de4 turns that byte into the software output gain (state W[0x01de4f70]). We hook the ramp's entry, remember
 * the real byte, and write 0xfe back before TE reads it: 0xfe is on TE's flat top (targets saturate from about 233),
 * so TE outputs at full level, and the knob only drives ofp_gain. A write of 0xfe by us is told apart from a new
 * reading by value (the knob never reports exactly 0xfe: the top of the map is 251). The byte starts at 0 (patch):
 * silent until the first real reading (no boot burst).
 * UNVERIFIED on a device: the tx word format (left-justified, +-2^31 full scale) is read from the USB ring code
 * (ofp_audio.h), the knob scale from the stock ramp; flash the probe image first and check VOL K and TX PEAK. */
#include <stdint.h>
#include "ofp_gain_dsp.h"

#define KNOB            (*(volatile uint8_t *)0x018B62A4)
#define PIN_VALUE       0xfe

static volatile uint8_t knob_raw;           /* last real reading (0 until TE has read the knob once) */
static ofp_gain_state gs;
static uint8_t inited;

void ofp_vol_pin(void)                      /* entry hook (before TE's code) on the output ramp */
{
    uint8_t k = KNOB;

    if (k != PIN_VALUE)
        knob_raw = k;
    KNOB = PIN_VALUE;
}

void ofp_gain_process(int32_t *tx)          /* from ofp_audio's wrapper, interrupt context */
{
    if (!inited) {
        ofp_gain_init(&gs, 0);              /* start silent */
        inited = 1;
    }
    ofp_gain_block(&gs, tx, 64, ofp_gain_knob[knob_raw]);
}

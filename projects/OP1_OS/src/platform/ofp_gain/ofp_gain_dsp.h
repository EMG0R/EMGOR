/* OP FREAK master gain stage: -100..+100, 0 = unity. Integer only (no float, no division, fixed cost per sample),
 * written to run both in the audio interrupt (Blackfin) and in the host test.
 *
 *   g <= 0   linear gain (100+g)^2 / 100^2, g = -100 is exact silence
 *   g  > 0   pre-gain 0..+24 dB, then a blend of  linear -> asymmetric tanh "analog" saturation -> soft clipper
 *            (flat to a 0.5 knee, tanh above, Fruity-Soft-Clipper style), the soft clipper taking over near +100
 *   always   leaky-integrator DC blocker (~0.4 Hz), hard ceiling at -0.5 dBFS
 * Samples are the OP-1's 32-bit output words (left-justified, +-2^31 = full scale); internal precision is Q23. */
#ifndef OFP_GAIN_DSP_H
#define OFP_GAIN_DSP_H

#include <stdint.h>

#define OFP_GAIN_POS_N 201          /* position = g + 100 */

typedef struct {
    int32_t dc[2];                  /* DC estimate per channel, Q27 */
    int32_t p;                      /* pre-gain, Q16 (unity = 65536) */
    int32_t wl, wa, ws;             /* blend weights, Q8, sum 256 */
    int32_t pos;                    /* smoothed position 0..200 */
    int32_t peak;                   /* highest |output| seen, Q23 (diagnostic) */
} ofp_gain_state;

extern const int32_t ofp_gain_ceiling;                  /* Q23 */
extern const uint16_t ofp_gain_p[OFP_GAIN_POS_N];       /* pre-gain Q11 */
extern const uint16_t ofp_gain_wl[OFP_GAIN_POS_N];      /* linear weight Q8 */
extern const uint16_t ofp_gain_ws[OFP_GAIN_POS_N];      /* soft-clip weight Q8 (analog = 256 - wl - ws) */
extern const uint8_t ofp_gain_knob[256];                /* volume knob byte -> position */

void ofp_gain_init(ofp_gain_state *s, int pos);         /* start at a position, no ramp */
/* one block: tx = frames x (L R) words in place; target position 0..200 (smoothed inside) */
void ofp_gain_block(ofp_gain_state *s, int32_t *tx, int frames, int target_pos);

#endif

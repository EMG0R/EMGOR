/* OP FREAK audio-callback layer. Owns the wrapper around TE's render callback (pointer 0xff80036c).
 * Other platform features plug in with optional (weak) functions; whichever are linked into the image get called:
 *   void ofp_gain_process(int32_t *tx);                    after TE rendered: master gain / ceiling on the output block
 *   void ofp_probe_block(const ofp_audio_blk *b);          after everything: measurement record of this block
 * tx = 64 stereo frames, interleaved L R, one signed 32-bit word per channel, left-justified (full scale +-2^31). */
#ifndef OFP_AUDIO_H
#define OFP_AUDIO_H

#include <stdint.h>

typedef struct {
    uint32_t t0;        /* CYCLES at callback entry */
    uint32_t d;         /* cycles the whole callback took (TE render + our stages) */
    uint32_t wr0, rd0;  /* USB to-host ring indices before the render */
    uint32_t wr1, rd1;  /* ... and after */
    uint32_t peak;      /* highest |word| of TE's output block, before our gain stage (only measured when ofp_probe is linked) */
} ofp_audio_blk;

#endif

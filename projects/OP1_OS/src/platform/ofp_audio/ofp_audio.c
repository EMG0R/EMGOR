/* The wrapper itself. TE installs the render callback (0xffa04804, L1) at audio::init without going through its
 * setter, and re-installs it through the setter 0x0191a7cc in other places; we put ours in front in both. */
#include <op1/te246.h>
#include "ofp_audio.h"

#define AUDIO_RENDER_CB   0xffa04804u
#define CB_SLOT           ((void *volatile *)0xff80036c)
#define RING_WR           (*(volatile uint32_t *)0x018B5FE4)
#define RING_RD           (*(volatile uint32_t *)0x018B5FE8)

void ofp_gain_process(int32_t *tx) __attribute__((weak));
void ofp_probe_block(const ofp_audio_blk *b) __attribute__((weak));

static inline uint32_t au_cycles(void)
{
    uint32_t c;

    __asm__ volatile("%0 = CYCLES;" : "=d"(c));
    return c;
}

void ofp_audio_cb(int32_t *tx, const int32_t *rx)
{
    ofp_audio_blk b;

    b.t0 = au_cycles();
    b.wr0 = RING_WR;
    b.rd0 = RING_RD;
    ((void (*)(int32_t *, const int32_t *))AUDIO_RENDER_CB)(tx, rx);
    b.peak = 0;
    if (ofp_probe_block) {
        int i;

        for (i = 0; i < 128; i++) {
            int32_t v = tx[i];
            uint32_t a = v < 0 ? (uint32_t)(-(v + 1)) : (uint32_t)v;

            if (a > b.peak)
                b.peak = a;
        }
    }
    if (ofp_gain_process)
        ofp_gain_process(tx);
    b.d = au_cycles() - b.t0;
    b.wr1 = RING_WR;
    b.rd1 = RING_RD;
    if (ofp_probe_block)
        ofp_probe_block(&b);
}

/* entry hook (filter) on the setter: TE's own store is skipped, ours stores the wrapper for the render callback */
int ofp_setcb(void *cb)
{
    *CB_SLOT = (uint32_t)cb == AUDIO_RENDER_CB ? (void *)ofp_audio_cb : cb;
    return 1;
}

/* boot_done (main thread, before TE's main loop): the first install did not go through the setter */
void ofp_boot(void)
{
    if ((uint32_t)*CB_SLOT == AUDIO_RENDER_CB)
        *CB_SLOT = (void *)ofp_audio_cb;
}

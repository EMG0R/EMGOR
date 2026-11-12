/* OP FREAK probe: pure bookkeeping for the audio-block measurements (no hardware access; host-testable).
 * The audio-callback wrapper feeds one record per 64-frame block; the overlay reads the results. */
#ifndef OFP_CORE_H
#define OFP_CORE_H

#include <stdint.h>

#define OFP_NOMINAL_DEFAULT 580498u     /* 400 MHz / (44100 / 64) cycles per block; replaced by the measured period */
#define OFP_USB_RING        512

typedef struct {
    /* input of one block */
    uint32_t t0;                /* CYCLES at callback entry */
    uint32_t d;                 /* cycles the callback took */
    int usb_active;             /* the USB push advanced the write index this block */
    int32_t lead_pre;           /* ring write - read index before the push (frames buffered) */
    int32_t lead_post;          /* the same, after the push */
    int in_tape_write;          /* a tape yaffs write is in progress (main thread) */
} ofp_blk;

typedef struct {
    uint32_t nominal;           /* measured block period (cycles), EMA of undisturbed periods */
    uint32_t last_t0;
    int have_last;
    uint32_t blocks, hot, late, late_tape, late_usb;
    uint32_t max_d, min_p, max_p;
    uint32_t win_t0, win_busy, win_n;           /* ~0.5 s windows */
    uint32_t aud_pct, aud_pk;                   /* busy % of the last window, peak of windows */
    /* USB to-host ring */
    int usb_seen;
    int32_t lead_now, lead_min, lead_max;       /* frames buffered, sampled before each push */
    int32_t prev_post;
    uint32_t usb_und, usb_ovr, usb_jump;        /* lead <= 0, lead > 480 after push, lead jumped > 128 off prediction */
    uint32_t usb_blocks;
    uint32_t lw_sum, lw_n;                      /* 1 s window of lead_pre */
    int32_t lead_base16, lead_avg16;            /* mean lead x16: first window after USB went active, latest window */
    uint32_t lead_secs;                         /* windows since the baseline */
} ofp_core;

void ofp_core_reset(ofp_core *c);
void ofp_core_block(ofp_core *c, const ofp_blk *b);
/* cycles -> milliseconds x10 using the measured block period (1 block = 64 / 44100 s) */
uint32_t ofp_ms10(const ofp_core *c, uint32_t cycles);

#endif

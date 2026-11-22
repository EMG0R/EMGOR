/* OP FREAK measurement overlay (probe image). OFF by default; shift + speech cycles OFF -> page 1 -> page 2 -> OFF
 * (opening resets all counters and peaks). Observes only; the only writes outside our own variables are the audio
 * callback pointer (wrapped, the original is always called) and the idle thread's vtable run slot (wrapped).
 *
 * Page 1: CPU % (100 - idle thread share, now + peak), audio ISR busy % (0.5 s windows, peak), worst single block,
 *         block period / jitter, HOT (callback took > 90 % of a block), LATE (block started > 1.5 periods after the
 *         previous one = a missed or delayed block), RAM (TE's TLSF heap, VDK heap 0 and heap 1) and flash free.
 * Page 2: USB to-host ring fill / underrun / overrun / resync jumps / drift, LATE while USB active, tape yaffs write
 *         stalls (count, last, max, slow) and LATE while a tape write is in progress.
 *
 * Address and struct-offset facts for the idle-thread statistics, the VDK heap walk and the yaffs free-space calls
 * were taken from op1r's sysmon (GPL-3.0 reference, docs/VENDOR.md); this code is written from scratch (MIT). */
#include <op1/te246.h>
#include <op1/gfx.h>
#include "ofp_core.h"
#include "../ofp_audio/ofp_audio.h"

#define KNOB_BYTE         (*(volatile uint8_t *)0x018B62A4)     /* volume knob 0..255 (TWI), stock output ramp reads it */
#define ACCEL             ((volatile int16_t *)0x01dff338)      /* ADXL345-like on TWI 0x53: X, Y, Z int16 (RE, UNVERIFIED) */

#define IDLE_RUN_ORIG     0x01A5E240u
#define HEAP_LOOKUP(i)    (((uint32_t *(*)(int))0x01A9F2E2)(i))
#define LOCK_SCHED()      (((void (*)(void))0x01A9F76C)())
#define UNLOCK_SCHED()    (((void (*)(void))0x01A9F78E)())
#define YAFFS_FREE(p)     (((int32_t (*)(const char *))0x01A9B1C0)(p))
#define YAFFS_TOTAL(p)    (((int32_t (*)(const char *))0x01A9B284)(p))

#define TLSF              0x0106b7ccu                  /* TE's operator-new pool (meminfo): +4 used, +8 peak */
#define TLSF_SIZE         0x00840000u
#define TLSF_SIG          0x2a59fa59u

static inline uint32_t cycles(void)
{
    uint32_t c;

    __asm__ volatile("%0 = CYCLES;" : "=d"(c));
    return c;
}

/* ------------------------------------------------------------------ ISR side */

static ofp_core core;                           /* written by the audio ISR only, except reset (via flag) */
static volatile uint8_t reset_req;
static volatile uint8_t tape_busy;              /* main thread sets while a tape yaffs write runs */
static volatile uint32_t isr_alive, tx_peak;             /* blocks seen since boot (wrapper installed and running) */

/* called by ofp_audio's callback wrapper once per 64-frame block (audio interrupt context) */
void ofp_probe_block(const ofp_audio_blk *a)
{
    ofp_blk b;

    b.t0 = a->t0;
    b.d = a->d;
    b.usb_active = a->wr1 != a->wr0;
    b.lead_pre = (int32_t)(a->wr0 - a->rd0);
    b.lead_post = (int32_t)(a->wr1 - a->rd1);
    b.in_tape_write = tape_busy;
    if (reset_req) {
        reset_req = 0;
        ofp_core_reset(&core);
        tx_peak = 0;
    }
    ofp_core_block(&core, &b);
    if (a->peak > tx_peak)
        tx_peak = a->peak;
    isr_alive++;
}

/* ------------------------------------------------------------------ idle thread (CPU %) */

static void *volatile idle_obj;

/* vtable run slot of kTE_Idle (0x01911b98, stock value 0x01a5e240): remember the thread, then run TE's idle code */
void ofp_idle_run(void *self)
{
    idle_obj = self;
    ((void (*)(void *))IDLE_RUN_ORIG)(self);
}

/* ------------------------------------------------------------------ tape write timing (main thread) */

static volatile uint32_t tw_t0, tw_n, tw_last, tw_max, tw_slow;

void ofp_tape_pre(void)                         /* before tape_c::service's lseek + yaffs_write */
{
    tw_t0 = cycles();
    tape_busy = 1;
}

void ofp_tape_post(void)                        /* after the yaffs_write returned */
{
    uint32_t d = cycles() - tw_t0;

    tape_busy = 0;
    tw_n++;
    tw_last = d;
    if (d > tw_max)
        tw_max = d;
    if (d > core.nominal * 10u)                 /* > ~14.5 ms */
        tw_slow++;
}

/* ------------------------------------------------------------------ keys */

static volatile uint8_t page;                   /* 0 off, 1, 2 */

int ofp_key(int key, int down)
{
    if (key != TE_KEY_SPEECH || !*TE_SHIFT_HELD)
        return 0;
    if (down) {
        page = (uint8_t)(page < 2 ? page + 1 : 0);
        if (page == 1) {
            reset_req = 1;
            tw_n = tw_last = tw_max = tw_slow = 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ GUI sampling */

static uint32_t frame_n, last_wall, last_idle;
static int32_t cpu_now = -1, cpu_pk;
static uint32_t ram_next;
static uint32_t heap_free[2], heap_size[2], tlsf_used, tlsf_peak;
static int32_t flash_free_mb = -1, flash_total_mb;

static uint32_t pct(uint32_t a, uint32_t b)
{
    if (!b)
        return 0;
    while (a > 0x7fffffffu / 100) {
        a >>= 1;
        b >>= 1;
    }
    if (!b)
        return 100;
    a = a * 100 / b;
    return a > 100 ? 100 : a;
}

static void sample_cpu(void)
{
    uint32_t now = cycles(), wall = now - last_wall;
    uint32_t s = idle_obj ? ((volatile uint32_t *)idle_obj)[0x34 / 4] : 0;

    if (wall < 100000000u)                      /* ~0.25 s at 400 MHz */
        return;
    if (s >= 0x100 && s < 0x04000000 && !(s & 3)) {
        uint32_t idle = ((volatile uint32_t *)s)[0x20 / 4];

        if (last_wall) {
            cpu_now = 100 - (int32_t)pct(idle - last_idle, wall);
            if (cpu_now > cpu_pk)
                cpu_pk = cpu_now;
        }
        last_idle = idle;
    }
    last_wall = now;
}

static void sample_ram(void)
{
    int i;

    if (*(volatile uint32_t *)TLSF == TLSF_SIG) {
        tlsf_used = *(volatile uint32_t *)(TLSF + 4);
        tlsf_peak = *(volatile uint32_t *)(TLSF + 8);
    }
    for (i = 0; i < 2; i++) {
        uint32_t *h, sz, used, fr = 0;
        const uint32_t *p;
        int n;

        LOCK_SCHED();
        h = HEAP_LOOKUP(i);
        if (!h) {
            UNLOCK_SCHED();
            continue;
        }
        sz = h[1];
        used = h[3] - h[0];
        p = (const uint32_t *)h[0x18 / 4];
        for (n = 0; p && n < 8192; n++) {       /* free list: {size, heap, next} */
            if ((uint32_t)p < 0x100 || (uint32_t)p >= 0x04000000 || ((uint32_t)p & 3))
                break;
            fr += p[0];
            p = (const uint32_t *)p[2];
        }
        UNLOCK_SCHED();
        used = used > fr ? used - fr : 0;
        heap_size[i] = sz;
        heap_free[i] = sz > used ? sz - used : 0;
    }
}

static void sample_flash(void)
{
    int32_t t = YAFFS_TOTAL("/yaffs2"), f = YAFFS_FREE("/yaffs2");

    if (t > 0 && f >= 0) {
        flash_total_mb = t >> 20;
        flash_free_mb = f >> 20;
    }
}

/* ------------------------------------------------------------------ drawing */

#define C_HEAD  GFX_RGB(0xb0, 0x70, 0xff)
#define C_LBL   GFX_RGB(0x8a, 0x8a, 0x99)
#define C_VAL   GFX_WHITE
#define C_OK    GFX_RGB(0x3d, 0xdc, 0x97)
#define C_BAD   GFX_RGB(0xff, 0x4f, 0x5e)
#define C_WARN  GFX_RGB(0xff, 0xb0, 0x3d)
#define SC      2
#define LH      13

static int num(int x, int y, int32_t v, uint16_t c)
{
    return gfx_dec(x, y, v, c, SC);
}

static int fix1(int x, int y, uint32_t v10, uint16_t c)     /* v10 / 10 with one decimal */
{
    x = gfx_dec(x, y, (int32_t)(v10 / 10), c, SC);
    x = gfx_text(x, y, ".", c, SC);
    return gfx_dec(x, y, (int32_t)(v10 % 10), c, SC);
}

static int mb1(int x, int y, uint32_t bytes, uint16_t c)
{
    return fix1(x, y, (bytes >> 10) * 10 / 1024, c);
}

static int lbl(int x, int y, const char *s)
{
    return gfx_text(x, y, s, C_LBL, SC);
}

static uint16_t flag(uint32_t n)
{
    return n ? C_BAD : C_OK;
}

static void page1(void)
{
    int x, y = 16;
    uint32_t blk_pct = core.nominal ? pct(core.max_d, core.nominal) : 0;
    uint32_t jit = core.max_p > core.nominal ? core.max_p - core.nominal : 0;

    if (core.min_p != 0xffffffffu && core.nominal > core.min_p && core.nominal - core.min_p > jit)
        jit = core.nominal - core.min_p;
    x = lbl(2, y, "CPU ");
    if (cpu_now < 0)
        x = gfx_text(x, y, "--", C_VAL, SC);
    else
        x = num(x, y, cpu_now, cpu_now > 85 ? C_BAD : C_VAL);
    x = lbl(x + 8, y, "PK ");
    x = num(x, y, cpu_pk, cpu_pk > 85 ? C_BAD : C_VAL);
    y += LH;
    x = lbl(2, y, "AUD ");
    x = num(x, y, (int32_t)core.aud_pct, C_VAL);
    x = lbl(x + 8, y, "PK ");
    x = num(x, y, (int32_t)core.aud_pk, core.aud_pk > 70 ? C_WARN : C_VAL);
    x = lbl(x + 8, y, "BLK ");
    num(x, y, (int32_t)blk_pct, blk_pct > 90 ? C_BAD : C_VAL);
    y += LH;
    x = lbl(2, y, "PERIOD ");
    x = num(x, y, (int32_t)core.nominal, C_VAL);
    x = lbl(x + 6, y, "JIT ");
    num(x, y, (int32_t)jit, C_VAL);
    y += LH;
    x = lbl(2, y, "HOT ");
    x = num(x, y, (int32_t)core.hot, flag(core.hot));
    x = lbl(x + 8, y, "LATE ");
    x = num(x, y, (int32_t)core.late, flag(core.late));
    x = lbl(x + 8, y, "BLKS ");
    num(x, y, (int32_t)core.blocks, C_VAL);
    y += LH + 3;
    x = lbl(2, y, "TLSF ");
    x = mb1(x, y, tlsf_used, C_VAL);
    x = lbl(x + 4, y, "PK ");
    x = mb1(x, y, tlsf_peak, C_VAL);
    x = lbl(x + 4, y, "OF ");
    mb1(x, y, TLSF_SIZE, C_VAL);
    y += LH;
    x = lbl(2, y, "HEAP0 FREE ");
    x = mb1(x, y, heap_free[0], C_OK);
    x = lbl(x + 4, y, "OF ");
    mb1(x, y, heap_size[0], C_VAL);
    y += LH;
    x = lbl(2, y, "HEAP1 FREE ");
    x = mb1(x, y, heap_free[1], C_OK);
    x = lbl(x + 4, y, "OF ");
    mb1(x, y, heap_size[1], C_VAL);
    y += LH;
    x = lbl(2, y, "TX PK ");
    x = gfx_hex(x, y, tx_peak, 8, C_VAL, SC);
    y += LH;
    x = lbl(2, y, "VOL K ");
    x = num(x, y, KNOB_BYTE, C_VAL);
    x = lbl(x + 6, y, "G ");
    x = num(x, y, ACCEL[0], C_VAL);
    x = num(x + 4, y, ACCEL[1], C_VAL);
    num(x + 4, y, ACCEL[2], C_VAL);
    y += LH;
    x = lbl(2, y, "FLASH FREE ");
    if (flash_free_mb < 0)
        gfx_text(x, y, "--", C_VAL, SC);
    else {
        x = num(x, y, flash_free_mb, C_OK);
        x = lbl(x + 4, y, "OF ");
        num(x, y, flash_total_mb, C_VAL);
    }
}

static void page2(void)
{
    int x, y = 16;
    int32_t d = core.lead_avg16 - core.lead_base16;
    uint32_t ad = (uint32_t)(d < 0 ? -d : d);

    x = lbl(2, y, "USB ");
    if (!core.usb_seen)
        x = gfx_text(x, y, "IDLE", C_LBL, SC);
    else {
        x = gfx_text(x, y, "ACTIVE", C_OK, SC);
        x = lbl(x + 6, y, "LEAD ");
        num(x, y, core.lead_now, C_VAL);
    }
    y += LH;
    x = lbl(2, y, "LEAD MIN ");
    x = num(x, y, core.lead_min, core.lead_min <= 8 ? C_WARN : C_VAL);
    x = lbl(x + 6, y, "MAX ");
    num(x, y, core.lead_max, core.lead_max > 480 ? C_WARN : C_VAL);
    y += LH;
    x = lbl(2, y, "UND ");
    x = num(x, y, (int32_t)core.usb_und, flag(core.usb_und));
    x = lbl(x + 6, y, "OVR ");
    x = num(x, y, (int32_t)core.usb_ovr, flag(core.usb_ovr));
    x = lbl(x + 6, y, "JUMP ");
    num(x, y, (int32_t)core.usb_jump, flag(core.usb_jump));
    y += LH;
    x = lbl(2, y, "DRIFT ");
    x = gfx_text(x, y, d < 0 ? "-" : "+", C_VAL, SC);
    x = fix1(x, y, ad * 10 / 16, C_VAL);
    x = lbl(x + 4, y, "F IN ");
    x = num(x, y, (int32_t)core.lead_secs, C_VAL);
    lbl(x + 2, y, "S");
    y += LH;
    x = lbl(2, y, "LATE W/ USB ");
    num(x, y, (int32_t)core.late_usb, flag(core.late_usb));
    y += LH + 3;
    x = lbl(2, y, "TAPE WR ");
    x = num(x, y, (int32_t)tw_n, C_VAL);
    x = lbl(x + 6, y, "SLOW ");
    num(x, y, (int32_t)tw_slow, flag(tw_slow));
    y += LH;
    x = lbl(2, y, "LAST ");
    x = fix1(x, y, ofp_ms10(&core, tw_last), C_VAL);
    x = lbl(x + 4, y, "MS MAX ");
    x = fix1(x, y, ofp_ms10(&core, tw_max), C_VAL);
    lbl(x + 4, y, "MS");
    y += LH;
    x = lbl(2, y, "LATE W/ TAPE ");
    num(x, y, (int32_t)core.late_tape, flag(core.late_tape));
}

void ofp_frame(uint16_t *buf)
{
    int x;

    if (!page)
        return;
    frame_n++;
#ifndef OFP_SIM                                 /* the simulator bench (sim/) fills the sampled values itself */
    sample_cpu();
    if (frame_n >= ram_next) {
        sample_ram();
        if (!(frame_n & 0xff) || flash_free_mb < 0)
            sample_flash();
        ram_next = frame_n + 40;
    }
#endif
    gfx_begin(buf);
    gfx_rect(0, 0, 320, 156, GFX_BLACK);
    x = gfx_text(2, 2, "OFP PROBE ", C_HEAD, SC);
    x = gfx_dec(x, 2, page, C_HEAD, SC);
    x = gfx_text(x, 2, "/2", C_HEAD, SC);
    if (!isr_alive)
        gfx_text(x + 8, 2, "NO ISR", C_BAD, SC);
    else
        gfx_text(x + 8, 2, "SHIFT+SPEECH. CPU AUD BLK ARE PERCENT", C_LBL, 1);
    if (page == 1)
        page1();
    else
        page2();
}

/* LIL DRUMMER host demos: writes build/demos/lildrummer_*.wav (stereo, 44.1 kHz). */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../lildrummer/lildrummer.h"
#include "wav.h"
#define SR 44100
#define BLK 64
static ld_kit K;
static int32_t *L, *R;

static void finish(const char *name, const char *dir, int n, double gain)
{
    char path[512];
    snprintf(path, sizeof path, "%s/lildrummer_%s.wav", dir, name);
    wav_write(path, L, R, n, SR, gain);
    printf("wrote %s\n", path);
    free(L); free(R);
}
/* generic sequencer: events[step][..] as a char grid, 16th notes at bpm */
typedef struct { int key, vel; } hit;

static void groove(const char *dir)
{
    /* 808 groove, 100 bpm, 4 bars of 16ths */
    int bpm = 100, bars = 4, steps = bars * 16, step_len = SR * 60 / bpm / 4, n = steps * step_len + SR, s, t;
    static const char *kick  = "x..x..x...x..x.." ;
    static const char *snare = "....x.......x..." ;
    static const char *clap  = "....x.......x.x." ;
    static const char *chat  = "x.x.x.x.x.x.x.x." ;
    static const char *ohat  = "......x.......x." ;
    static const char *cow   = "..............x." ;
    L = calloc((size_t)n, 4); R = calloc((size_t)n, 4);
    ld_init(&K, 808); op_init(SR);
    ld_init(&K, 808);
    for (s = 0, t = 0; s < steps; s++, t += step_len) {
        int st = s % 16, bar = s / 16, b;
        int sw = (s & 1) ? (step_len / 6) : 0;                    /* light swing */
        int off = t + sw, blkstart;
        (void)b;
        /* render up to this step's start */
        (void)off;
        if (kick[st] == 'x')  { ld_trigger(&K, 0, 118); }
        if (bar == 3 && st >= 8 && (st & 1) == 0) { /* tom fill */ }
        if (snare[st] == 'x') ld_trigger(&K, 2, 105);
        if (clap[st] == 'x' && bar % 2 == 1) ld_trigger(&K, 4, 100);
        if (chat[st] == 'x')  ld_trigger(&K, 10, st % 4 == 0 ? 110 : 80);
        if (ohat[st] == 'x')  ld_trigger(&K, 11, 95);
        if (cow[st] == 'x' && bar >= 2) ld_trigger(&K, 13, 90);
        if (bar == 3 && st == 12) ld_trigger(&K, 8, 110);
        if (bar == 3 && st == 13) ld_trigger(&K, 7, 105);
        if (bar == 3 && st == 14) ld_trigger(&K, 6, 105);
        if (bar == 3 && st == 15) ld_trigger(&K, 1, 120);
        blkstart = t;
        for (blkstart = t; blkstart < t + step_len; blkstart += BLK) {
            int m = t + step_len - blkstart < BLK ? t + step_len - blkstart : BLK;
            ld_render(&K, L + blkstart, R + blkstart, m);
        }
    }
    ld_render(&K, L + steps * step_len, R + steps * step_len, SR);
    finish("808_groove", dir, n, 1.0);
}

static void fm_hats(const char *dir)
{
    /* FM-wandering hats: STEP gen on ratio+index+noise (new value every hit), DRIFT on tune; seeded */
    int bpm = 118, steps = 64, step_len = SR * 60 / bpm / 4, n = steps * step_len + SR, s, t, blk;
    L = calloc((size_t)n, 4); R = calloc((size_t)n, 4);
    ld_init(&K, 4242);
    ld_set_param(&K, 10, LD_MODEL, LD_MODEL_AT(6) - 1500);              /* hat leaning cowbell */
    ld_set_gen(&K, 10, LD_FM_RATIO, OP_GEN_STEP, 28000, 20);
    ld_set_gen(&K, 10, LD_FM_INDEX, OP_GEN_STEP, 30000, 20);
    ld_set_gen(&K, 10, LD_TUNE, OP_GEN_DRIFT, 9000, 35);
    ld_set_gen(&K, 10, LD_MODEL, OP_GEN_STEP, 6000, 20);
    ld_set_gen(&K, 10, LD_NOISE, OP_GEN_STEP, 10000, 20);
    for (s = 0, t = 0; s < steps; s++, t += step_len) {
        int st = s % 16;
        if (st % 4 == 0 || st == 7 || st == 11 || st == 14) ld_trigger(&K, 0, 115);
        if (st == 4 || st == 12) ld_trigger(&K, 2, 100);
        ld_trigger(&K, 10, (st & 1) ? 70 : 105);
        if (st == 10) ld_trigger(&K, 11, 95);
        for (blk = t; blk < t + step_len; blk += BLK) {
            int m = t + step_len - blk < BLK ? t + step_len - blk : BLK;
            ld_render(&K, L + blk, R + blk, m);
        }
    }
    ld_render(&K, L + steps * step_len, R + steps * step_len, SR);
    finish("fm_wander_hats", dir, n, 1.0);
}

static void morph(const char *dir)
{
    /* one key, MODEL swept kick -> tom -> snare -> clap -> rim -> cowbell -> hat -> zap, one hit per step */
    int hits = 32, gap = SR * 3 / 10, n = hits * gap + SR, h, blk;
    L = calloc((size_t)n, 4); R = calloc((size_t)n, 4);
    ld_init(&K, 99);
    for (h = 0; h < hits; h++) {
        int t = h * gap;
        ld_set_param(&K, 16, LD_MODEL, h * 32767 / (hits - 1));
        ld_set_param(&K, 16, LD_WIDTH, 16000);
        ld_trigger(&K, 16, 110);
        for (blk = t; blk < t + gap; blk += BLK) {
            int m = t + gap - blk < BLK ? t + gap - blk : BLK;
            ld_render(&K, L + blk, R + blk, m);
        }
    }
    ld_render(&K, L + hits * gap, R + hits * gap, SR);
    finish("model_morph", dir, n, 1.0);
}

static void tour(const char *dir)
{
    int key, gap = SR * 45 / 100, n = LD_KEYS * gap + SR, blk;
    L = calloc((size_t)n, 4); R = calloc((size_t)n, 4);
    ld_init(&K, 7);
    for (key = 0; key < LD_KEYS; key++) {
        int t = key * gap;
        ld_trigger(&K, key, 110);
        for (blk = t; blk < t + gap; blk += BLK) {
            int m = t + gap - blk < BLK ? t + gap - blk : BLK;
            ld_render(&K, L + blk, R + blk, m);
        }
    }
    ld_render(&K, L + LD_KEYS * gap, R + LD_KEYS * gap, SR);
    finish("kit_tour", dir, n, 1.0);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/demos";
    op_init(SR);
    groove(dir);
    fm_hats(dir);
    morph(dir);
    tour(dir);
    return 0;
}

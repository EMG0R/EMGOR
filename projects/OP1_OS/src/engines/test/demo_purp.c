/* THE PURP host demos: writes build/demos/purp_*.wav (stereo, 44.1 kHz). */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../purp/purp.h"
#include "wav.h"

#define SR 44100
#define BLK 64
static pu_synth S;
static int32_t *L, *R;
static int total;

typedef void (*scene_cb)(pu_synth *s, int t);   /* t in samples at block start */

static void run(const char *name, const char *outdir, int seconds, scene_cb cb, double gain)
{
    char path[512];
    int t, n = seconds * SR;
    L = calloc((size_t)n + BLK, 4); R = calloc((size_t)n + BLK, 4);
    for (t = 0; t < n; t += BLK) {
        cb(&S, t);
        pu_render(&S, L + t, R + t, BLK);
    }
    snprintf(path, sizeof path, "%s/purp_%s.wav", outdir, name);
    wav_write(path, L, R, n, SR, gain);
    printf("wrote %s\n", path);
    free(L); free(R); total++;
}
#define AT(sec) (t >= (int)((sec) * SR) && t < (int)((sec) * SR) + BLK)
#define BETWEEN(a, b) (t >= (int)((a) * SR) && t < (int)((b) * SR))

/* analog pads: C lydian chords, slow, detuned, drifting */
static void sc_pad(pu_synth *s, int t)
{
    if (t == 0) { pu_init(s, 11); pu_preset(s, 0); pu_set_param(s, PU_VOICES, 32767); pu_set_param(s, PU_DRIFT, 24000);
                  pu_set_param(s, PU_A_REL, 24000); pu_set_param(s, PU_NOISE_MIX, 1500); }
    if (AT(0.0)) { pu_note_on(s, 48, 90); pu_note_on(s, 64, 80); pu_note_on(s, 71, 80); }
    if (AT(3.5)) { pu_all_off(s); }
    if (AT(3.7)) { pu_note_on(s, 50, 90); pu_note_on(s, 66, 80); pu_note_on(s, 69, 80); pu_note_on(s, 73, 70); }
    if (AT(7.5)) { pu_all_off(s); }
    if (AT(7.7)) { pu_note_on(s, 52, 90); pu_note_on(s, 67, 80); pu_note_on(s, 71, 80); pu_note_on(s, 74, 70); }
    if (AT(11.5)) pu_all_off(s);
    if (BETWEEN(0, 12)) pu_set_param(s, PU_CUTOFF, 12000 + (int)(6000 * sin(t / (double)SR * 0.9)));
}
/* growly FM bass line */
static void sc_growl(pu_synth *s, int t)
{
    static const int notes[8] = { 36, 36, 43, 36, 39, 41, 34, 36 };
    int step = (int)(t / (SR * 0.25)), i;
    if (t == 0) { pu_init(s, 22); pu_preset(s, 1); pu_set_param(s, PU_VOICES, 0); pu_set_param(s, PU_GLIDE, 9000); }
    if (t % (SR / 4) < BLK) {
        pu_all_off(s);
        if (step % 8 != 6) pu_note_on(s, notes[step % 8], 110);
        for (i = 0; i < 1; i++) {}
    }
    if (t % (SR / 4) >= (SR / 5) && t % (SR / 4) < (SR / 5) + BLK) pu_all_off(s);
    pu_set_param(s, PU_FM_INDEX, 12000 + (int)(9000 * (0.5 + 0.5 * sin(t / (double)SR * 0.7))));
    pu_set_param(s, PU_FM_RATIO, 13000 + (int)(900 * sin(t / (double)SR * 0.3)));
}
/* noise-FM: rasp, clangorous textures */
static void sc_noise(pu_synth *s, int t)
{
    static const int notes[4] = { 60, 63, 67, 58 };
    if (t == 0) { pu_init(s, 33); pu_preset(s, 2); pu_set_param(s, PU_VOICES, 32767); }
    if (t % (SR / 2) < BLK) { pu_all_off(s); pu_note_on(s, notes[(t / (SR / 2)) % 4], 100); }
    pu_set_param(s, PU_NOISE_FM, 8000 + (int)(16000 * (0.5 + 0.5 * sin(t / (double)SR * 0.8))));
}
/* resonant filter sweep: saw into the ladder, resonance 90%+ */
static void sc_sweep(pu_synth *s, int t)
{
    double sec = t / (double)SR;
    if (t == 0) { pu_init(s, 44); pu_preset(s, 4); pu_set_param(s, PU_VOICES, 0); pu_set_param(s, PU_SHAPE_A, 0);
                  pu_set_param(s, PU_SHAPE_B, 0); pu_set_param(s, PU_DETUNE, 6000); pu_set_param(s, PU_RES, 30000);
                  pu_set_param(s, PU_ENV_AMT, 16384); pu_set_param(s, PU_A_SUS, 32767); pu_set_param(s, PU_A_REL, 12000);
                  pu_set_param(s, PU_DRIVE, 14000);
                  pu_note_on(s, 36, 110); pu_note_on(s, 36, 110); }
    if (AT(10.0)) pu_all_off(s);
    /* exponential sweep up and down */
    pu_set_param(s, PU_CUTOFF, (int)(32767 * (0.5 + 0.5 * -cos(sec * 0.63)) * 0.9 + 800));
    if (sec > 5.0) pu_set_param(s, PU_RES, 32000);
}
/* drift + stereo spread: held chord, long, seeded wander */
static void sc_drift(pu_synth *s, int t)
{
    if (t == 0) { pu_init(s, 55); pu_preset(s, 6); pu_set_param(s, PU_VOICES, 32767); pu_set_param(s, PU_DRIFT, 32767);
                  pu_set_param(s, PU_A_REL, 26000);
                  pu_note_on(s, 53, 85); pu_note_on(s, 60, 85); pu_note_on(s, 64, 80); pu_note_on(s, 69, 80); pu_note_on(s, 71, 70); }
    if (AT(11.0)) pu_all_off(s);
}
/* FM bell / marimba with the stepped ratio GEN and snap */
static void sc_bell(pu_synth *s, int t)
{
    static const int notes[8] = { 72, 76, 79, 83, 79, 76, 74, 71 };
    if (t == 0) { pu_init(s, 66); pu_preset(s, 5); pu_set_param(s, PU_VOICES, 32767); pu_set_param(s, PU_A_REL, 22000); }
    if (t % (SR * 3 / 8) < BLK) pu_note_on(s, notes[(t / (SR * 3 / 8)) % 8], 90);
}
/* PURP macro rising */
static void sc_macro(pu_synth *s, int t)
{
    double sec = t / (double)SR;
    if (t == 0) { pu_init(s, 77); pu_preset(s, 7); pu_set_param(s, PU_VOICES, 32767); pu_set_param(s, PU_A_SUS, 32767);
                  pu_set_param(s, PU_A_REL, 20000); pu_set_param(s, PU_MACRO, 0);
                  pu_set_gen(s, PU_MACRO, OP_GEN_OFF, 0, 0);
                  pu_note_on(s, 41, 100); pu_note_on(s, 48, 100); pu_note_on(s, 52, 90); }
    pu_set_param(s, PU_MACRO, (int)(32767 * (sec / 12.0 > 1 ? 1 : sec / 12.0)));
    if (AT(13.0)) pu_all_off(s);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/demos";
    op_init(SR);
    run("analog_pad", dir, 15, sc_pad, 1.3);
    run("growl_fm", dir, 8, sc_growl, 1.6);
    run("noise_fm", dir, 8, sc_noise, 3.2);
    run("filter_sweep_res", dir, 12, sc_sweep, 1.2);
    run("drift_spread", dir, 15, sc_drift, 1.2);
    run("fm_bell", dir, 9, sc_bell, 2.2);
    run("macro_rise", dir, 16, sc_macro, 1.0);
    printf("%d demos\n", total);
    return 0;
}

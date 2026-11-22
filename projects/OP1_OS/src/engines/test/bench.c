/* Host performance: ns per voice-sample, and cycles/sample assuming HOST_GHZ (default 4.5, M4 Max P-core). */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../purp/purp.h"
#include "../lildrummer/lildrummer.h"
static pu_synth PS; static ld_kit LK;
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int32_t L[64], R[64];
static double ghz = 4.5;
static void rep(const char *n, double sec, double samples)
{
    double ns = sec / samples * 1e9;
    printf("%-44s %7.1f ns/voice-sample  ~%6.0f cycles/sample @%.1f GHz\n", n, ns, ns * ghz, ghz);
}
static double run_purp(int voices, int preset, int sr, int mono)
{
    int i, n = sr * 4 / 64; double t0, ref;
    op_init((uint32_t)sr); pu_init(&PS, 1); pu_preset(&PS, preset);
    pu_set_param(&PS, PU_VOICES, 32767); pu_set_param(&PS, PU_A_ATT, 0); pu_set_param(&PS, PU_A_SUS, 32767);
    pu_set_param(&PS, PU_A_DEC, 32767); pu_set_param(&PS, PU_A_REL, 32767);
    if (mono) pu_set_param(&PS, PU_DETUNE, 0);
    else pu_set_param(&PS, PU_DETUNE, 12000);
    for (i = 0; i < 5; i++) PS.gen[PU_DETUNE].mode = 0;
    for (i = 0; i < voices; i++) pu_note_on(&PS, 40 + i * 4, 110);
    for (i = 0; i < 20; i++) pu_render(&PS, L, R, 64);
    t0 = now();
    for (i = 0; i < n; i++) pu_render(&PS, L, R, 64);
    ref = now() - t0;
    return ref / ((double)n * 64 * voices);
}
int main(int argc, char **argv)
{
    int i, sr;
    if (argc > 1) ghz = atof(argv[1]);
    for (sr = 44100; sr <= 88200; sr *= 2) {
        printf("--- PURP @ %d Hz\n", sr);
        rep("1 voice, GROWL FM (stereo, FM, ladder)", run_purp(1, 1, sr, 0) * 1, 1);
        rep("1 voice, GROWL FM, mono (detune 0)", run_purp(1, 1, sr, 1), 1);
        rep("5 voices, GROWL FM (stereo)", run_purp(5, 1, sr, 0), 1);
        rep("5 voices, ANALOG PAD (stereo)", run_purp(5, 0, sr, 0), 1);
        rep("5 voices, NOISE FM (stereo)", run_purp(5, 2, sr, 0), 1);
        rep("5 voices, RES LEAD (stereo)", run_purp(5, 4, sr, 0), 1);
        { /* LIL DRUMMER: 24 keys sustained long, per voice-sample */
          int n = sr * 2 / 64; double t0, t;
          int keys[6] = { 0, 2, 4, 10, 13, 16 };
          const char *nm[6] = { "KICK", "SNARE", "CLAP", "HAT", "COWBELL", "ZAP" };
          int k;
          printf("--- LIL DRUMMER @ %d Hz\n", sr);
          op_init((uint32_t)sr);
          for (k = 0; k < 6; k++) {
              ld_init(&LK, 3); ld_set_param(&LK, keys[k], LD_DECAY, 32767); ld_set_param(&LK, keys[k], LD_WIDTH, 16000);
              ld_trigger(&LK, keys[k], 110);
              t0 = now();
              for (i = 0; i < n; i++) { ld_render(&LK, L, R, 64); if (!LK.v[keys[k]].active) break; }
              t = now() - t0;
              { char b[64]; snprintf(b, sizeof b, "%s (stereo width on), per active voice", nm[k]); rep(b, t, (double)i * 64); }
          }
        }
    }
    return 0;
}

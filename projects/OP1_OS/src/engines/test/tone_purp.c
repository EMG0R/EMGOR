/* tone probes for the analysis script: writes /tmp/pu_tone_<name>.wav (mono-ish, stereo wav) */
#include <stdio.h>
#include <stdlib.h>
#include "../purp/purp.h"
#include "wav.h"
static pu_synth S;
static void tone(const char *name, int note, int shape, int fm, int cutoff, int res)
{
    static int32_t L[44100 * 2], R[44100 * 2];
    char p[128];
    pu_init(&S, 5);
    pu_set_param(&S, PU_SHAPE_A, shape); pu_set_param(&S, PU_SHAPE_B, shape);
    pu_set_param(&S, PU_DETUNE, 0); pu_set_param(&S, PU_NOISE_MIX, 0);
    pu_set_param(&S, PU_FM_INDEX, fm); pu_set_param(&S, PU_FM_FB, 0); pu_set_param(&S, PU_NOISE_FM, 0);
    pu_set_param(&S, PU_CUTOFF, cutoff); pu_set_param(&S, PU_RES, res); pu_set_param(&S, PU_DRIVE, 0);
    pu_set_param(&S, PU_ENV_AMT, 16384); pu_set_param(&S, PU_DRIFT, 0);
    pu_set_param(&S, PU_A_ATT, 0); pu_set_param(&S, PU_A_SUS, 32767); pu_set_param(&S, PU_VOICES, 0);
    pu_set_param(&S, PU_SHAPE_B, shape); pu_set_param(&S, PU_FM_RATIO, 10923); /* ratio 1.0 */
    pu_set_gen(&S, PU_CUTOFF, 0, 0, 0);
    S.mx[0].src = 0; S.mx[0].dst = 0;
    pu_note_on(&S, note, 127);
    pu_render(&S, L, R, 44100 * 2);
    snprintf(p, sizeof p, "/tmp/pu_tone_%s.wav", name);
    wav_write(p, L, R, 44100 * 2, 44100, 1.0);
}
int main(void)
{
    op_init(44100);
    tone("saw84", 84, 0, 0, 32767, 0);
    tone("sq84", 84, 16384, 0, 32767, 0);
    tone("tri84", 84, 32767, 0, 32767, 0);
    tone("saw96", 96, 0, 0, 32767, 0);
    tone("fm60", 60, 16384, 16000, 32767, 0);
    tone("res", 48, 0, 0, 12000, 32767);
    tone("res2", 48, 0, 0, 20000, 32767);
    return 0;
}

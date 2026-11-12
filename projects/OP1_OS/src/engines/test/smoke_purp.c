#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../purp/purp.h"
static pu_synth S;
int main(void)
{
    int pre, n = 44100, i, pk = 0;
    static int32_t L[44100], R[44100];
    double e = 0;
    op_init(44100);
    for (pre = 0; pre < PU_PRESETS; pre++) {
        pu_init(&S, 1234); pu_preset(&S, pre);
        pu_note_on(&S, 48, 100); pu_note_on(&S, 55, 100);
        pu_render(&S, L, R, n);
        pk = 0; e = 0;
        for (i = 0; i < n; i++) { if (abs(L[i]) > pk) pk = abs(L[i]); e += (double)L[i] * L[i]; }
        printf("%-14s peak %6d rms %7.0f act %d\n", pu_preset_names[pre], pk, sqrt(e / n), pu_active_voices(&S));
    }
    return 0;
}

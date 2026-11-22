/* Host check of the SDK adapters: stub eh_register, drive pu_sdk_render the way eh_te_render does. */
#include <stdio.h>
#include <stdlib.h>
#include "eh_host.h"
#include "../purp/purp.h"
#include "../lildrummer/lildrummer.h"
static eh_render_fn reg[8];
void eh_register(int tag, eh_render_fn f) { reg[tag] = f; }
void pu_sdk_init(void); pu_synth *pu_sdk_synth(void);
void ld_sdk_register(void); void ld_sdk_trigger(int, int); void ld_sdk_render(int16_t *, int);
int main(void)
{
    static uint32_t inc[128]; int16_t out[128]; int i, b, fails = 0, pk = 0; double e = 0;
    pu_sdk_init();
    if (!reg[1]) { printf("FAIL: PURP not registered\n"); return 1; }
    for (i = 0; i < 128; i++) inc[i] = (uint32_t)(220.0 / 88200 * 4294967296.0);
    for (b = 0; b < 400; b++) {
        reg[1](0, out, 0, inc, (b == 0 ? EH_FRESH : 0) | EH_BLOCK);
        for (i = 0; i < 128; i++) { int a = abs(out[i]); if (a > pk) pk = a; e += (double)out[i] * out[i]; }
    }
    printf("PURP adapter: 400 blocks @ 220 Hz peak %d rms %.0f\n", pk, e / (400 * 128.0) > 0 ? __builtin_sqrt(e / (400 * 128.0)) : 0);
    if (pk < 2000 || pk > 32767) fails++;
    /* TE has 6 voices, PURP 5: voice index 5 must be silent and must not touch memory past v[4] */
    for (i = 0, pk = 0; i < 20; i++) { reg[1](5, out, 0, inc, EH_FRESH | EH_BLOCK); for (b = 0; b < 128; b++) if (out[b]) pk++; }
    printf("PURP adapter: 6th voice nonzero samples %d\n", pk);
    if (pk) fails++;
    ld_sdk_register();
    ld_sdk_trigger(0, 110); pk = 0;
    for (b = 0; b < 100; b++) { ld_sdk_render(out, 128); for (i = 0; i < 128; i++) if (abs(out[i]) > pk) pk = abs(out[i]); }
    printf("LD adapter: kick peak %d\n", pk);
    if (pk < 2000) fails++;
    printf(fails ? "FAIL\n" : "PASS\n");
    return fails;
}

/* Deterministic output checksums: identical on host and on the Blackfin simulator (fixed-point portability). */
#include "../purp/purp.h"
#include "../lildrummer/lildrummer.h"
static uint32_t chk_acc(uint32_t a, const int32_t *x, int n) { int i; for (i = 0; i < n; i++) a = a * 31u + (uint32_t)x[i]; return a; }
static uint32_t chk_purp(void)
{
    static pu_synth s; static int32_t L[512], R[512]; uint32_t a = 17; int b;
    op_init(44100); pu_init(&s, 1234); pu_preset(&s, 1);
    pu_note_on(&s, 40, 100); pu_note_on(&s, 47, 90); pu_note_on(&s, 52, 80);
    for (b = 0; b < 16; b++) { pu_render(&s, L, R, 512); a = chk_acc(chk_acc(a, L, 512), R, 512); }
    pu_all_off(&s);
    for (b = 0; b < 4; b++) { pu_render(&s, L, R, 512); a = chk_acc(chk_acc(a, L, 512), R, 512); }
    return a;
}
static uint32_t chk_ld(void)
{
    static ld_kit k; static int32_t L[512], R[512]; uint32_t a = 29; int b, key;
    op_init(44100); ld_init(&k, 4321);
    for (key = 0; key < LD_KEYS; key += 3) ld_trigger(&k, key, 100);
    for (b = 0; b < 12; b++) { ld_render(&k, L, R, 512); a = chk_acc(chk_acc(a, L, 512), R, 512); }
    return a;
}

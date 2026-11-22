/* LIL DRUMMER glue for the SDK. The drum row's engine slot is not an SDK-hosted voice yet (the platform
 * builder is researching the drum host), so this exposes the two things any host needs:
 *   ld_sdk_trigger(key, vel)      - a drum key was hit (key 0..23, vel 1..127)
 *   ld_sdk_render(out, n)         - once per audio block: mixes all keys into mono int16 at 88.2 kHz
 *                                   (core runs at 44.1 kHz, n88 = 128 -> 64 core samples, 2x linear up)
 * and, for hosting on a drwave voice as a tonal-keyboard drum synth, an eh_render_fn (LD_SDK_TAG; the key
 * is the played note relative to F, i.e. note 53 -> key 0, wrapped to 24) when LD_SDK_TAG is defined.
 * Compile with: -I vendor/op1-sdk/features/enginehost (only when LD_SDK_TAG is defined). */
#include "lildrummer.h"

static ld_kit g_kit;
static int32_t last_l;

ld_kit *ld_sdk_kit(void) { return &g_kit; }
void ld_sdk_init(void) { op_init(44100); ld_init(&g_kit, 0x4c494c44u); last_l = 0; }
void ld_sdk_trigger(int key, int vel) { ld_trigger(&g_kit, key, vel); }

void ld_sdk_render(int16_t *out, int n88)
{
    int32_t L[64], R[64], i, prev = last_l;
    int h = n88 >> 1;
    if (h > 64) h = 64;
    ld_render(&g_kit, L, R, h);
    for (i = 0; i < h; i++) {
        int32_t m = (L[i] + R[i]) >> 1;
        out[2 * i] = (int16_t)op_clip16((prev + m) >> 1);
        out[2 * i + 1] = (int16_t)op_clip16(m);
        prev = m;
    }
    last_l = prev;
}

#ifdef LD_SDK_TAG
#include "eh_host.h"
static void ld_sdk_eh_render(int vi, int16_t *out, const int16_t *knobs, const uint32_t *inc, int flags)
{
    (void)knobs; (void)vi;
    if (flags & EH_FRESH) {
        uint32_t i2 = inc[0] > OP_INC_MAX / 2 ? OP_INC_MAX : inc[0] * 2u;
        int note = (op_inc_to_note(i2) + 128) >> 8, key = (note - 53) % LD_KEYS;
        if (key < 0) key += LD_KEYS;
        ld_trigger(&g_kit, key, 100);
    }
    if (flags & EH_BLOCK) ld_sdk_render(out, 128);     /* first voice of the block mixes the whole kit */
    else { int i; for (i = 0; i < 128; i++) out[i] = 0; }
}
void ld_sdk_register(void) { ld_sdk_init(); eh_register(LD_SDK_TAG, ld_sdk_eh_render); }
#endif

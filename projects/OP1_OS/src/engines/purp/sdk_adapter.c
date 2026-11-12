/* THE PURP as an SDK engine on the engine host (features/enginehost, eh_host.h).
 * Thin glue only: the platform registers it by calling pu_sdk_init() from its feature init.
 *
 * Why it is built this way (numbers in docs/ENGINES.md):
 *  - the host renders each TE voice separately, 128 samples at 88.2 kHz, mono, and TE applies its own
 *    envelope / velocity / volume afterwards. PURP is too heavy for 5 voices at 88.2 kHz, so the core
 *    runs at 44.1 kHz (op_init(44100)) on 64 samples and is upsampled 2x (linear) into the 128-sample
 *    block. TE's pitch buffer (88.2 kHz increments) is doubled and converted to a note.
 *  - ext_env = 1: TE owns the amp envelope, so PURP's AMP ADSR is bypassed (the sound is a raw full
 *    length note, like WARP/TRIO). MOD ENV still runs (attack/decay/sustain; no release callback).
 *  - the host sees mono; pu_sdk_render_stereo() gives the L/R pair for when the platform has a stereo bus.
 *  - parameters live in the shared pu_synth (pu_sdk_synth()); the platform's UI pages edit it. The TE knob
 *    slots are ignored except as the engine tag.
 * Compile with: -I vendor/op1-sdk/features/enginehost  (eh_host.h) */
#include "eh_host.h"
#include "purp.h"

#ifndef PU_SDK_TAG
#define PU_SDK_TAG 1                    /* free tag on the host (2 MONO, 3 STATION, 4 TRIO, 5 WHEEL, 6 WARP) */
#endif

static pu_synth g_pu;
static int32_t last_out[EH_VOICES];

pu_synth *pu_sdk_synth(void) { return &g_pu; }

void pu_sdk_render_stereo(int vi, int32_t *L64, int32_t *R64, const uint32_t *inc, int flags)
{
    pu_voice *v;
    uint32_t i2 = inc[0] > OP_INC_MAX / 2 ? OP_INC_MAX : inc[0] * 2u;
    int32_t note = op_inc_to_note(i2), i;
    if (flags & EH_BLOCK) pu_global_update(&g_pu, 64);
    for (i = 0; i < 64; i++) L64[i] = R64[i] = 0;
    if ((unsigned)vi >= PU_MAX_VOICES) return;      /* TE has 6 voices, PURP 5: the 6th is silent (v[] would overrun) */
    v = &g_pu.v[vi];
    if (flags & EH_FRESH) pu_voice_start(&g_pu, v, note, 100);
    v->note_q8 = v->note_tgt_q8 = note;
    pu_voice_render(&g_pu, v, L64, R64, 64);
}

static void pu_sdk_render(int vi, int16_t *out, const int16_t *knobs, const uint32_t *inc, int flags)
{
    int32_t L[64], R[64], i, prev = last_out[vi];
    (void)knobs;
    pu_sdk_render_stereo(vi, L, R, inc, flags);
    for (i = 0; i < 64; i++) {
        int32_t m = pu_master((L[i] + R[i]) >> 1);
        out[2 * i] = (int16_t)op_clip16((prev + m) >> 1);
        out[2 * i + 1] = (int16_t)op_clip16(m);
        prev = m;
    }
    last_out[vi] = prev;
}

void pu_sdk_init(void)
{
    op_init(44100);
    pu_init(&g_pu, 0x50555250u);
    g_pu.ext_env = 1;
    pu_set_param(&g_pu, PU_A_ATT, 0);
    eh_register(PU_SDK_TAG, pu_sdk_render);
}

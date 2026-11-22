/* Blackfin simulator bench for the PURP / LIL DRUMMER cores (freestanding). Results are read by
 * run_bench.py from the volatile globals. Exit code = failed-check bits. */
#include "purp/purp.h"
#include "lildrummer/lildrummer.h"
#include "test/checksum.h"

volatile uint32_t c_pu_1v_st, c_pu_1v_mono, c_pu_5v_pad, c_pu_5v_growl, c_pu_5v_noise, c_pu_5v_res, c_pu_voice_block;
volatile uint32_t c_ld_kick, c_ld_snare, c_ld_clap, c_ld_hat, c_ld_cow, c_ld_zap;
volatile int fails, peak_seen;
volatile uint32_t c_chk_pu, c_chk_ld;

static pu_synth PS;
static ld_kit LK;
static int32_t L[128], R[128];

static uint32_t cycles(void)
{
    uint32_t c;
    __asm__ volatile("%0 = CYCLES;" : "=d"(c));
    return c;
}

/* cycles per sample for `blocks` x 128 samples of the running synth */
static uint32_t run_pu(int preset, int voices, int mono, uint32_t sr)
{
    int i, n = 4;
    uint32_t t0, t;
    op_init(sr);
    pu_init(&PS, 1);
    pu_preset(&PS, preset);
    pu_set_param(&PS, PU_VOICES, 32767); pu_set_param(&PS, PU_A_ATT, 0); pu_set_param(&PS, PU_A_SUS, 32767);
    pu_set_param(&PS, PU_A_DEC, 32767); pu_set_param(&PS, PU_A_REL, 32767);
    pu_set_param(&PS, PU_DETUNE, mono ? 0 : 12000);
    PS.gen[PU_DETUNE].mode = 0;
    for (i = 0; i < voices; i++) pu_note_on(&PS, 40 + i * 4, 110);
    pu_render(&PS, L, R, 128);
    t0 = cycles();
    for (i = 0; i < n; i++) pu_render(&PS, L, R, 128);
    t = cycles() - t0;
    for (i = 0; i < 128; i++) { int a = L[i] < 0 ? -L[i] : L[i]; if (a > peak_seen) peak_seen = a; }
    if (peak_seen == 0) fails |= 1;
    return t / (uint32_t)(n * 128);
}

static uint32_t run_ld(int key, uint32_t sr)
{
    int i, n = 4;
    uint32_t t0, t;
    op_init(sr);
    ld_init(&LK, 3);
    ld_set_param(&LK, key, LD_DECAY, 32767); ld_set_param(&LK, key, LD_WIDTH, 16000);
    ld_trigger(&LK, key, 110);
    ld_render(&LK, L, R, 128);
    t0 = cycles();
    for (i = 0; i < n; i++) ld_render(&LK, L, R, 128);
    t = cycles() - t0;
    if (!LK.v[key].active) fails |= 2;
    return t / (uint32_t)(n * 128);
}

int main(void)
{
    __asm__ volatile("R0 = SYSCFG; BITSET (R0, 1); SYSCFG = R0; CSYNC;" ::: "R0");   /* CCEN */
    c_pu_1v_st   = run_pu(1, 1, 0, 88200);
    c_pu_1v_mono = run_pu(1, 1, 1, 88200);
    c_pu_5v_growl = run_pu(1, 5, 0, 88200);
    c_pu_5v_pad  = run_pu(0, 5, 0, 88200);
    c_pu_5v_noise = run_pu(2, 5, 0, 88200);
    c_pu_5v_res  = run_pu(4, 5, 0, 88200);
    /* one voice block as the SDK adapter does it: core at 44.1 kHz, global update + 64 samples (= 128 host samples) */
    {
        uint32_t t0, t;
        op_init(44100); pu_init(&PS, 1); pu_preset(&PS, 1); pu_set_param(&PS, PU_A_SUS, 32767);
        pu_set_param(&PS, PU_A_ATT, 0); PS.ext_env = 1;
        pu_voice_start(&PS, &PS.v[0], 40 * 256, 110);
        pu_global_update(&PS, 64);
        pu_voice_render(&PS, &PS.v[0], L, R, 64);
        t0 = cycles();
        { int i; for (i = 0; i < 64; i++) { L[i] = R[i] = 0; } }
        pu_global_update(&PS, 64);
        pu_voice_render(&PS, &PS.v[0], L, R, 64);
        t = cycles() - t0;
        c_pu_voice_block = t;
    }
    c_ld_kick = run_ld(0, 88200); c_ld_snare = run_ld(2, 88200); c_ld_clap = run_ld(4, 88200);
    c_ld_hat = run_ld(10, 88200); c_ld_cow = run_ld(13, 88200); c_ld_zap = run_ld(16, 88200);
    c_chk_pu = chk_purp();
    c_chk_ld = chk_ld();
    return fails;
}

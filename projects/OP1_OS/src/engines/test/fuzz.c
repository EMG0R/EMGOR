/* Fuzz + unit tests for common/PURP/LIL DRUMMER. Build with -fsanitize=undefined -fno-sanitize-recover=all:
 * any signed overflow / bad shift aborts. Asserts: finite (integer) bounded output, no stuck voices. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../purp/purp.h"
#include "../lildrummer/lildrummer.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { if (++fails <= 20) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)
static op_prng R0;
static uint32_t rnd(void) { return op_rand(&R0); }
static int pick_val(void)
{
    switch (rnd() % 7) {
    case 0: return 0; case 1: return 1; case 2: return 32766; case 3: return 32767; case 4: return 16384;
    default: return (int)(rnd() % 32768);
    }
}
static const int rates[4] = { 44100, 48000, 88200, 96000 };

static void unit_common(void)
{
    int n, prev = -1;
    op_init(44100);
    for (n = 0; n <= 135 * 256; n += 7) {
        uint32_t inc = op_note_inc(n);
        CHECK(inc <= OP_INC_MAX, "inc max note %d", n);
        CHECK((int)inc >= prev || inc == OP_INC_MAX, "inc monotonic %d", n);
        prev = (int)inc;
        { int32_t g = op_g_from_note(n); CHECK(g >= 0 && g <= 32767, "G range %d", n); }
    }
    { /* pitch accuracy: A4 = 440 Hz at 44.1k */
      double f = op_note_inc(69 * 256) / 4294967296.0 * 44100.0;
      CHECK(f > 439.9 && f < 440.1, "A4 = %f Hz", f);
      int32_t r = op_inc_to_note(op_note_inc(60 * 256 + 100));
      CHECK(abs(r - (60 * 256 + 100)) < 3, "inc_to_note roundtrip %d", r);
      r = op_inc_to_note(op_note_inc(30 * 256));
      CHECK(abs(r - 30 * 256) < 3, "inc_to_note low %d", r);
    }
    /* sat / drive bounded for extremes */
    { int32_t x; for (x = -2000000; x < 2000000; x += 9973) {
        int32_t y = op_drive(x, 4095); CHECK(y >= -32768 && y <= 32768, "drive bound");
        y = op_drive_asym(x, 4095, 3500); CHECK(y >= -65534 && y <= 65534, "drive_asym bound"); } }
    /* GEN determinism + bounds */
    { op_gen_state a, b; op_gen g; op_gen_in in; int i, same = 1, m;
      memset(&in, 0, sizeof in); in.lfo1 = 32767; in.lfo2 = -32767; in.tilt_x = 32767; in.tilt_y = -32767; in.seq = 12345;
      for (m = 0; m < OP_GEN_MODES; m++) {
          g.mode = (uint8_t)m; g.rate = 100; g.depth = 32767;
          op_gen_state_init(&a, 77); op_gen_state_init(&b, 77);
          for (i = 0; i < 2000; i++) {
              int32_t va = op_gen_apply(16384, &g, &a, &in, 64), vb = op_gen_apply(16384, &g, &b, &in, 64);
              if (va != vb) same = 0;
              CHECK(va >= 0 && va <= 32767, "gen range mode %d", m);
          }
      }
      CHECK(same, "GEN deterministic per seed");
      op_gen_state_init(&a, 1); op_gen_state_init(&b, 2); g.mode = OP_GEN_STEP; g.rate = 127; in.clock_valid = 0;
      { int diff = 0; for (i = 0; i < 4000; i++) diff |= op_gen_apply(16384, &g, &a, &in, 64) != op_gen_apply(16384, &g, &b, &in, 64); CHECK(diff, "RESEED changes material"); }
    }
    /* ADSR extremes */
    { op_adsr_cfg c; op_adsr e; int i, a, d;
      for (a = 0; a <= 32767; a += 16383) for (d = 0; d <= 32767; d += 16383) {
          memset(&e, 0, sizeof e); op_adsr_cfg_set(&c, a, d, d / 2, a);
          op_adsr_on(&e);
          for (i = 0; i < 20000; i++) { int32_t o = op_adsr_tick(&e, &c); CHECK(o >= 0 && o <= 32767, "adsr range"); if (i == 5000) op_adsr_off(&e); }
      } }
    { op_lfo l; int s, i; for (s = 0; s < OP_LFO_SHAPES; s++) { op_lfo_init(&l, 3); for (i = 0; i < 100000; i++) { int32_t v = op_lfo_tick(&l, s, op_lfo_inc(32767, 0, 0)); CHECK(v >= -32767 && v <= 32767, "lfo range"); } }
      CHECK(op_lfo_inc(0, 0, 0) > 0 && op_lfo_inc(32767, 32767, 300 * 256) > 0, "lfo inc"); }
}

static pu_synth PS;
static void fuzz_purp(int iters)
{
    int it, i, b;
    static int32_t L[512], R[512];
    for (it = 0; it < iters; it++) {
        long rail = 0, tot = 0;
        op_init((uint32_t)rates[it & 3]);
        pu_init(&PS, rnd());
        pu_preset(&PS, (int)(rnd() % PU_PRESETS));
        for (i = 0; i < PU_PARAMS; i++) { pu_set_param(&PS, i, pick_val()); if (rnd() % 3 == 0) pu_set_gen(&PS, i, (int)(rnd() % 8), (rnd() & 1) ? 32767 : -32767, (int)(rnd() % 128)); }
        for (i = 0; i < PU_MX_SLOTS; i++) pu_set_matrix(&PS, i, pick_val(), pick_val(), (rnd() & 1) ? 32767 : pick_val(), pick_val());
        pu_set_snap(&PS, (int)(rnd() & 1));
        for (b = 0; b < 160; b++) {
            op_gen_in in; int k;
            in.lfo1 = in.lfo2 = 0; in.tilt_x = (int16_t)(pick_val() - 16384) * 2; in.tilt_y = (int16_t)(pick_val() - 16384) * 2;
            in.seq = (int16_t)(pick_val() - 16384) * 2; in.clock_valid = (uint8_t)(rnd() & 1); in.step_tick = rnd();
            pu_set_inputs(&PS, &in, (int32_t)(rnd() % (400 * 256)));
            if (rnd() % 12 == 0) pu_note_on(&PS, (int)(rnd() % 136), 1 + (int)(rnd() % 127));
            if (rnd() % 14 == 0) pu_note_off(&PS, (int)(rnd() % 136));
            if (rnd() % 40 == 0) pu_all_off(&PS);
            if (rnd() % 6 == 0) pu_set_param(&PS, (int)(rnd() % PU_PARAMS), pick_val());
            { int n = (rnd() % 5 == 0) ? 1 + (int)(rnd() % 100) : 64; pu_render(&PS, L, R, n);
              for (k = 0; k < n; k++) { tot++; CHECK(L[k] >= -32767 && L[k] <= 32767 && R[k] >= -32767 && R[k] <= 32767, "PURP out of range %d %d", L[k], R[k]);
                  if (abs(L[k]) >= 32767) rail++; } }
        }
        CHECK(rail < tot / 2 + 1, "PURP rail-stuck %ld/%ld", rail, tot);
    }
    /* no stuck voices: release everything, render 14 s, expect all voices ended */
    { int sr; for (sr = 0; sr < 4; sr++) { int n;
        op_init((uint32_t)rates[sr]); pu_init(&PS, 5); pu_preset(&PS, 1);
        pu_set_param(&PS, PU_A_REL, 32767); pu_set_param(&PS, PU_VOICES, 32767);
        for (i = 0; i < 5; i++) pu_note_on(&PS, 40 + i * 3, 100);
        pu_render(&PS, L, R, 256); pu_all_off(&PS);
        for (n = 0; n < (int)(14 * rates[sr]); n += 512) pu_render(&PS, L, R, 512);
        CHECK(pu_active_voices(&PS) == 0, "PURP stuck voices at %d Hz: %d", rates[sr], pu_active_voices(&PS)); } }
    /* determinism: same seed, same preset => identical output; RESEED => different */
    { int32_t A[2][2048], B[2][2048]; int k, diff = 0;
      op_init(44100);
      pu_init(&PS, 9); pu_preset(&PS, 2); pu_note_on(&PS, 50, 100); pu_render(&PS, A[0], A[1], 2048);
      pu_init(&PS, 9); pu_preset(&PS, 2); pu_note_on(&PS, 50, 100); pu_render(&PS, B[0], B[1], 2048);
      CHECK(memcmp(A, B, sizeof A) == 0, "PURP deterministic per seed");
      pu_init(&PS, 10); pu_preset(&PS, 2); pu_note_on(&PS, 50, 100); pu_render(&PS, B[0], B[1], 2048);
      for (k = 0; k < 2048; k++) diff |= A[0][k] != B[0][k];
      CHECK(diff, "PURP different seed differs"); }
}

static ld_kit LK;
static void fuzz_ld(int iters)
{
    int it, i, b, key;
    static int32_t L[512], R[512];
    for (it = 0; it < iters; it++) {
        long rail = 0, tot = 0;
        op_init((uint32_t)rates[it & 3]);
        ld_init(&LK, rnd());
        for (key = 0; key < LD_KEYS; key++) {
            for (i = 0; i < LD_PARAMS; i++) {
                if (rnd() % 2) ld_set_param(&LK, key, i, pick_val());
                if (rnd() % 3 == 0) ld_set_gen(&LK, key, i, (int)(rnd() % 8), (rnd() & 1) ? 32767 : -32767, (int)(rnd() % 128));
            }
        }
        for (b = 0; b < 200; b++) {
            op_gen_in in; int k, n;
            in.lfo1 = (int16_t)(pick_val() - 16384) * 2; in.lfo2 = (int16_t)(pick_val() - 16384) * 2;
            in.tilt_x = (int16_t)(pick_val() - 16384) * 2; in.tilt_y = (int16_t)(pick_val() - 16384) * 2; in.seq = (int16_t)(pick_val() - 16384) * 2;
            in.clock_valid = (uint8_t)(rnd() & 1); in.step_tick = rnd();
            ld_set_inputs(&LK, &in);
            if (rnd() % 3 == 0) ld_trigger(&LK, (int)(rnd() % LD_KEYS), 1 + (int)(rnd() % 127));
            if (rnd() % 10 == 0) ld_set_param(&LK, (int)(rnd() % LD_KEYS), (int)(rnd() % LD_PARAMS), pick_val());
            n = (rnd() % 5 == 0) ? 1 + (int)(rnd() % 100) : 64;
            ld_render(&LK, L, R, n);
            for (k = 0; k < n; k++) { tot++; CHECK(L[k] >= -32767 && L[k] <= 32767 && R[k] >= -32767 && R[k] <= 32767, "LD out of range"); if (abs(L[k]) >= 32767) rail++; }
        }
        CHECK(rail < tot / 2 + 1, "LD rail-stuck %ld/%ld", rail, tot);
    }
    /* every key at default, every model position, all voices end */
    { int sr;
      for (sr = 0; sr < 4; sr++) { int n;
        op_init((uint32_t)rates[sr]); ld_init(&LK, 1);
        for (key = 0; key < LD_KEYS; key++) { ld_set_param(&LK, key, LD_DECAY, 32767); ld_trigger(&LK, key, 127); }
        for (n = 0; n < (int)(30 * rates[sr]); n += 512) ld_render(&LK, L, R, 512);
        CHECK(ld_active_voices(&LK) == 0, "LD stuck voices at %d Hz: %d", rates[sr], ld_active_voices(&LK)); } }
    { int32_t A[2][2048], B[2][2048];
      op_init(44100); ld_init(&LK, 3); ld_trigger(&LK, 10, 100); ld_render(&LK, A[0], A[1], 2048);
      ld_init(&LK, 3); ld_trigger(&LK, 10, 100); ld_render(&LK, B[0], B[1], 2048);
      CHECK(memcmp(A, B, sizeof A) == 0, "LD deterministic per seed"); }
    { /* choke: closed hat kills open hat */
      int32_t a[2][4096], k2;
      op_init(44100); ld_init(&LK, 3); ld_trigger(&LK, 11, 100); ld_render(&LK, a[0], a[1], 256);
      ld_trigger(&LK, 10, 100); ld_render(&LK, a[0], a[1], 4096);
      CHECK(!LK.v[11].active, "choke group kills open hat");
      (void)k2; }
}

int main(int argc, char **argv)
{
    int iters = argc > 1 ? atoi(argv[1]) : 120;
    op_seed(&R0, 20261104);
    unit_common();
    fuzz_purp(iters);
    fuzz_ld(iters);
    printf("%s: %d checks, %d failures (fuzz iterations: %d PURP + %d LD, sample rates 44.1/48/88.2/96 k)\n",
           fails ? "FAIL" : "PASS", checks, fails, iters, iters);
    return fails ? 1 : 0;
}

/* host test + demo renderer for the master gain stage.
 *   cc -O2 -Wall -o /tmp/ofp_test_gain test_gain.c ofp_gain_dsp.c ofp_gain_tab.c -lm && /tmp/ofp_test_gain <outdir>
 * Writes 24-bit stereo 44.1 kHz WAVs (gain_<setting>.wav, gain_sweep.wav) into outdir and checks the gain curve. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ofp_gain_dsp.h"

#define FS 44100
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- test material: 120 BPM kick + hat + saw bass + pad, peak about -3 dBFS, as OP-1 style 32-bit words ---- */
static uint32_t rng = 12345;
static double noise(void) { rng = rng * 1664525u + 1013904223u; return ((rng >> 8) & 0xffff) / 32768.0 - 1.0; }

static void material(int32_t *buf, long start, long n, double level)
{
    long i;

    for (i = 0; i < n; i++) {
        double t = (double)(start + i) / FS, beat = fmod(t, 0.5), b8 = fmod(t + 0.25, 0.5), l, r;
        double kick = sin(2 * M_PI * (50 * beat + 70 * (1 - exp(-beat * 30)) / 30 * 0 + 120 * (1 - exp(-beat * 30)) / 30)) * exp(-beat * 9);
        double hat = noise() * exp(-b8 * 45) * 0.35;
        double bf = 55.0, ph = fmod(t * bf, 1.0), bass = (2 * ph - 1) * 0.45 * (0.6 + 0.4 * sin(2 * M_PI * t * 0.5));
        double pad = 0.12 * (sin(2 * M_PI * 261.63 * t) + sin(2 * M_PI * 329.63 * t * 1.002) + sin(2 * M_PI * 392.0 * t) + sin(2 * M_PI * 493.88 * t * 0.998));

        l = kick * 0.85 + hat + bass * 0.5 + pad;
        r = kick * 0.85 - hat * 0.8 + bass * 0.5 + pad * 0.9;
        l *= 0.5 * level; r *= 0.5 * level;
        if (l > 0.999) l = 0.999; if (l < -0.999) l = -0.999;
        if (r > 0.999) r = 0.999; if (r < -0.999) r = -0.999;
        buf[2 * i] = (int32_t)(l * 2147483647.0) & ~0xffff;           /* int16 << 16, like the stock output */
        buf[2 * i + 1] = (int32_t)(r * 2147483647.0) & ~0xffff;
    }
}

static void wr_le(FILE *f, uint32_t v, int n) { while (n--) { fputc(v & 255, f); v >>= 8; } }

static void wav(const char *dir, const char *name, const int32_t *buf, long frames)
{
    char path[512];
    FILE *f;
    long i;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); fails++; return; }
    fwrite("RIFF", 1, 4, f); wr_le(f, 36 + frames * 6, 4); fwrite("WAVEfmt ", 1, 8, f);
    wr_le(f, 16, 4); wr_le(f, 1, 2); wr_le(f, 2, 2); wr_le(f, FS, 4); wr_le(f, FS * 6, 4); wr_le(f, 6, 2); wr_le(f, 24, 2);
    fwrite("data", 1, 4, f); wr_le(f, frames * 6, 4);
    for (i = 0; i < frames * 2; i++) wr_le(f, (uint32_t)(buf[i] >> 8), 3);
    fclose(f);
}

static double rms_db(const int32_t *b, long n)
{
    double s = 0; long i;

    for (i = 0; i < n * 2; i++) { double v = b[i] / 2147483648.0; s += v * v; }
    return 10 * log10(s / (n * 2) + 1e-30);
}

static long peak_word(const int32_t *b, long n)
{
    long m = 0, i;

    for (i = 0; i < n * 2; i++) { long v = labs((long)b[i]); if (v > m) m = v; }
    return m;
}

/* process whole buffer at a fixed position */
static void run_fixed(int32_t *buf, long frames, int pos)
{
    ofp_gain_state s; long i;

    ofp_gain_init(&s, pos);
    for (i = 0; i + 64 <= frames; i += 64) ofp_gain_block(&s, buf + 2 * i, 64, pos);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static const int gs[] = { -100, -60, -30, 0, 20, 40, 60, 80, 100 };
    const long N = 64 * 2756;       /* about 4 s, a whole number of 64-frame blocks */
    int32_t *x = malloc(sizeof(int32_t) * 2 * N), *y = malloc(sizeof(int32_t) * 2 * N);
    const long ceil32 = (long)ofp_gain_ceiling * 256;
    double prev = -999, rms[9];
    int k, i;
    long n;

    material(x, 0, N, 1.0);
    printf("input rms %.2f dBFS peak %.2f dBFS\n", rms_db(x, N), 20 * log10(peak_word(x, N) / 2147483648.0));
    for (k = 0; k < 9; k++) {
        char name[64];
        long pk;

        memcpy(y, x, sizeof(int32_t) * 2 * N);
        run_fixed(y, N, gs[k] + 100);
        pk = peak_word(y, N);
        rms[k] = rms_db(y, N);
        CHECK(pk <= ceil32, "g=%d peak %ld over ceiling %ld", gs[k], pk, ceil32);
        printf("g=%+4d  rms %7.2f dBFS  peak %7.2f dBFS\n", gs[k], rms[k], 20 * log10((pk + 1) / 2147483648.0));
        snprintf(name, sizeof name, "gain_%s%03d.wav", gs[k] < 0 ? "m" : "p", abs(gs[k]));
        wav(dir, name, y, N);
    }
    /* level never falls as the control rises */
    for (k = 1; k < 9; k++) { CHECK(rms[k] >= prev - 0.01 || k == 1, "rms not monotonic at g=%d", gs[k]); prev = rms[k]; }
    CHECK(rms[0] < -140, "g=-100 not silent: %.1f dBFS", rms[0]);

    /* unity: g=0 on a -9 dBFS take must come out within 0.05 dB and (apart from the DC blocker) bit-close */
    {
        int32_t *a = malloc(sizeof(int32_t) * 2 * N), *b = malloc(sizeof(int32_t) * 2 * N);
        double e = 0, sx = 0;

        material(a, 0, N, 0.35);
        memcpy(b, a, sizeof(int32_t) * 2 * N);
        run_fixed(b, N, 100);
        for (n = FS; n < N * 2; n++) { double d = (b[n] - a[n]) / 2147483648.0, v = a[n] / 2147483648.0; e += d * d; sx += v * v; }
        printf("unity: error %.1f dB re signal, level change %.3f dB\n", 10 * log10(e / sx + 1e-30), rms_db(b, N) - rms_db(a, N));
        CHECK(fabs(rms_db(b, N) - rms_db(a, N)) < 0.05, "unity level change");
        CHECK(10 * log10(e / sx + 1e-30) < -40, "unity error too large");   /* the DC blocker removes the kick's sub-1 Hz drift */
        /* a zero-mean sine passes within the 0.4 Hz high-pass residue (1 kHz: about -67 dB) */
        for (n = 0; n < N; n++) { int32_t v = (int32_t)(sin(2 * M_PI * 1000 * n / FS) * 0.35 * 2147483647.0) & ~0xffff; a[2 * n] = a[2 * n + 1] = v; }
        memcpy(b, a, sizeof(int32_t) * 2 * N);
        run_fixed(b, N, 100);
        e = sx = 0;
        for (n = 2 * FS; n < N * 2; n++) { double d = (b[n] - a[n]) / 2147483648.0, v = a[n] / 2147483648.0; e += d * d; sx += v * v; }
        printf("unity sine: error %.1f dB re signal\n", 10 * log10(e / sx + 1e-30));
        CHECK(10 * log10(e / sx + 1e-30) < -60, "unity sine error");
        free(a); free(b);
    }

    /* attenuation law: g=-50 is -12 dB (a=0.5, gain a^2 = 0.25) */
    {
        int32_t *a = malloc(sizeof(int32_t) * 2 * N);

        material(a, 0, N, 0.35);
        memcpy(y, a, sizeof(int32_t) * 2 * N);
        run_fixed(y, N, 50);
        printf("g=-50: %.2f dB (expect -12.04)\n", rms_db(y, N) - rms_db(a, N));
        CHECK(fabs(rms_db(y, N) - rms_db(a, N) + 12.04) < 0.1, "g=-50 law");
        free(a);
    }

    /* worst-case inputs at every setting: full-scale square, DC step, noise at full scale */
    for (k = 0; k <= 200; k += 5) {
        for (i = 0; i < 3; i++) {
            long m = 0;
            for (n = 0; n < 20000; n++) {
                int32_t v = i == 0 ? ((n / 50) & 1 ? 2147483647 : -2147483647 - 1) : i == 1 ? (n > 8000 ? 2147483647 : 0) : (int32_t)(noise() * 2147483647.0);
                y[2 * n] = v; y[2 * n + 1] = -v - (v == -2147483647 - 1 ? 0 : 0);
                if (y[2 * n + 1] == 0 && v == -2147483647 - 1) y[2 * n + 1] = 2147483647;
            }
            run_fixed(y, 20000 / 64 * 64, k);
            m = peak_word(y, 20000 / 64 * 64);
            CHECK(m <= ceil32, "worst case %d at pos %d peak %ld > %ld", i, k, m, ceil32);
        }
    }

    /* DC: asymmetric heavy drive; mean of the last 2 s must be tiny */
    {
        double mean = 0;

        material(y, 0, N, 1.0);
        run_fixed(y, N, 200);
        for (n = N - 2 * FS; n < N; n++) mean += (y[2 * n] / 2147483648.0 + y[2 * n + 1] / 2147483648.0) / 2;
        mean /= 2 * FS;
        printf("g=+100 DC (last 2 s): %.5f FS\n", mean);
        CHECK(fabs(mean) < 0.002, "DC at +100: %f", mean);
    }

    /* sweep: -100 -> +100 over 12 s with the control moving every block; peaks, and no click at block joins */
    {
        long M = 64 * (FS * 12 / 64);
        int32_t *s = malloc(sizeof(int32_t) * 2 * M);
        ofp_gain_state st;
        double maxin = 0, inj = 0;
        long i2;

        for (n = 0; n < M; n += N) material(s + 2 * n, n, (M - n) < N ? (M - n) : N, 1.0);
        /* constant 220 Hz sine at -12 dBFS for the click check (second buffer) */
        {
            int32_t *c = malloc(sizeof(int32_t) * 2 * M);

            for (n = 0; n < M; n++) { int32_t v = (int32_t)(sin(2 * M_PI * 220 * n / FS) * 0.25 * 2147483647.0); c[2 * n] = v; c[2 * n + 1] = v; }
            ofp_gain_init(&st, 0);
            for (i2 = 0; i2 + 64 <= M; i2 += 64) ofp_gain_block(&st, c + 2 * i2, 64, (int)(200.0 * i2 / M));
            (void)maxin; (void)inj;
            {
                long b, bad = 0;

                for (b = 1; b < M / 64; b++) {
                    double jd = fabs((c[2 * (b * 64)] - c[2 * (b * 64 - 1)]) / 2147483648.0), local = 0;

                    for (n = (b - 1) * 64 + 1; n < b * 64; n++) {
                        double d = fabs((c[2 * n] - c[2 * (n - 1)]) / 2147483648.0);
                        if (d > local) local = d;
                    }
                    if (jd > local * 1.5 + 0.01) bad++;
                }
                printf("sweep click check: %ld of %ld block joins stand out\n", bad, M / 64);
                CHECK(bad == 0, "clicks at block joins: %ld", bad);
            }
            free(c);
        }
        ofp_gain_init(&st, 0);
        for (i2 = 0; i2 + 64 <= M; i2 += 64) ofp_gain_block(&st, s + 2 * i2, 64, (int)(200.0 * i2 / M));
        CHECK(peak_word(s, M / 64 * 64) <= ceil32, "sweep peak over ceiling");
        wav(dir, "gain_sweep.wav", s, M / 64 * 64);
        free(s);
    }

    printf(fails ? "ofp_gain: %d FAILED\n" : "ofp_gain: all passed\n", fails);
    free(x); free(y);
    return fails != 0;
}

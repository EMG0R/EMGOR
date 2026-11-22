#ifndef WAV_H
#define WAV_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
static inline void wav_write(const char *path, const int32_t *L, const int32_t *R, int n, int sr, double gain)
{
    FILE *f = fopen(path, "wb");
    uint32_t dl = (uint32_t)n * 4, v;
    int i;
    if (!f) { perror(path); exit(1); }
    fwrite("RIFF", 1, 4, f); v = 36 + dl; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    { uint16_t a = 1, c = 2; fwrite(&a, 2, 1, f); fwrite(&c, 2, 1, f); }
    v = (uint32_t)sr; fwrite(&v, 4, 1, f); v = (uint32_t)sr * 4; fwrite(&v, 4, 1, f);
    { uint16_t a = 4, b = 16; fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f); }
    fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
    for (i = 0; i < n; i++) {
        double l = L[i] * gain, r = R[i] * gain;
        int16_t s[2];
        s[0] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        s[1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
        fwrite(s, 2, 2, f);
    }
    fclose(f);
}
#endif

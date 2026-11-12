// f32stat FILE... : prints "name samples peak rms nonfinite" for raw float32 files.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
int main(int argc, char** argv) {
    for (int a = 1; a < argc; a++) {
        FILE* f = fopen(argv[a], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[a]); return 1; }
        float buf[4096]; size_t n; double ss = 0; double pk = 0; uint64_t cnt = 0, bad = 0;
        while ((n = fread(buf, sizeof(float), 4096, f)) > 0)
            for (size_t i = 0; i < n; i++) {
                float x = buf[i];
                if (!(x - x == 0.f)) { bad++; continue; }
                double ax = fabs((double)x); if (ax > pk) pk = ax; ss += (double)x * x; cnt++;
            }
        fclose(f);
        const char* b = strrchr(argv[a], '/'); b = b ? b + 1 : argv[a];
        printf("%s %llu %.6f %.6f %llu\n", b, (unsigned long long)cnt, pk, cnt ? sqrt(ss / cnt) : 0.0, (unsigned long long)bad);
    }
    return 0;
}

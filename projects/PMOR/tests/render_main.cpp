// render_main.cpp -- one scenario -> raw float file.  usage: render SCRIPT.scn OUT.f32
// Built twice from the same driver (scenario.cpp):
//   -DPMOR_REF : against the ORIGINAL engine/audio_callback.cpp (ff86e4c), hardware stubbed -> reference
//   (default)  : against pmor_core's pmorRenderBlock                                        -> candidate
#include <stdio.h>
#include <string.h>
#include "scenario.h"
#ifdef PMOR_REF
#include <stddef.h>
void AudioCallback(float** in, float** out, size_t n);
static void blockFn(const float* in0, float* L, float* R, size_t n) {
    static float zin[256];
    float* in[2]  = { zin, zin };
    float* out[2] = { L, R };
    (void)in0;
    AudioCallback(in, out, n);
}
#else
#include "render.h"
static void blockFn(const float* in0, float* L, float* R, size_t n) { pmorRenderBlock(in0, L, R, n); }
#endif
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s SCRIPT.scn OUT.f32\n", argv[0]); return 1; }
    return scenarioRun(argv[1], argv[2], blockFn);
}

// scale.cpp — global key/scale. MODES + rebuild ported from PMOR_V2.cpp L590-615
// (root F3, key is float/continuous so microtonal key shifts are possible).
#include <string.h>
#include <math.h>
#include "scale.h"
#include "engine_util.h"

ScaleState scale;

const char* const SCALE_MODE_NAMES[SCALE_MODES] = {
    "IONIAN", "DORIAN", "PHRYG", "LYDIAN", "MIXO", "AEOL", "LOCR", "HARMIN", "PENTA"
};

// V2 L590-599: 9 modes, degree->semitone-from-root. Major pentatonic is a
// 5-note scale padded to 7 slots (repeats its last real degree).
static const uint8_t MODES[SCALE_MODES][SCALE_DEGREES] = {
    {0, 2, 4, 5, 7, 9, 11},   // Ionian (major)
    {0, 2, 3, 5, 7, 9, 10},   // Dorian
    {0, 1, 3, 5, 7, 8, 10},   // Phrygian
    {0, 2, 4, 6, 7, 9, 11},   // Lydian
    {0, 2, 4, 5, 7, 9, 10},   // Mixolydian
    {0, 2, 3, 5, 7, 8, 10},   // Aeolian (minor)
    {0, 1, 3, 5, 6, 8, 10},   // Locrian
    {0, 2, 3, 5, 7, 8, 11},   // Harmonic minor
    {0, 2, 4, 7, 9, 9, 9},    // Major pentatonic, padded
};

static const float ROOT_F3 = 174.61f;

void scaleInit() {
    memset(&scale, 0, sizeof(scale));
    scale.key_semis = 0.f;
    scale.mode      = 3;            // Lydian, the V2 default vibe
    scaleRebuild();
}

void scaleRebuild() {
    scale.root_hz = ROOT_F3 * semisToRatio(scale.key_semis);
    const uint8_t* iv = MODES[scale.mode < SCALE_MODES ? scale.mode : 0];
    for (int i = 0; i < 5 * SCALE_DEGREES; i++) {
        int deg   = i % SCALE_DEGREES;
        int oct   = i / SCALE_DEGREES;
        float semis = (float)iv[deg] + 12.f * (float)oct;
        scale.hz[i] = scale.root_hz * semisToRatio(semis);
    }
    // top root: 5 octaves above scale.root_hz (index 5*7)
    scale.hz[5 * SCALE_DEGREES] = scale.root_hz * semisToRatio(12.f * 5.f);
}

float scaleHz(int degree, int octave) {
    if (degree < 0) degree = 0; else if (degree >= SCALE_DEGREES) degree = SCALE_DEGREES - 1;
    if (octave < 0) octave = 0; else if (octave > 4) octave = 4;
    return scale.hz[octave * SCALE_DEGREES + degree];
}

// bottom row lowest: pad k -> idx = (3 - k/4)*4 + (k%4) -> degree = idx%7, octave = idx/7 (one octave lower than before, EMGOR 2026-09-15)
void scalePadToNote(uint8_t pad, int& degree, int& octave) {
    int row = pad / 4, col = pad % 4;
    int idx = (3 - row) * 4 + col;
    degree = idx % SCALE_DEGREES;
    octave = idx / SCALE_DEGREES;
}

// circle-of-fifths weighted degree pick for toms: never 0 (root). Order of
// likelihood: 4 (fifth), 1, 2, 5, 6, 3 -- weight 0.5^rank, sampled by `rnd` 0..1.
int scaleTomDegree(float rnd) {
    static const int   degs[6] = {4, 1, 2, 5, 6, 3};
    static const float w[6]    = {1.f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
    static const float SUM     = 1.96875f;   // sum of w[]
    float target = clampf(rnd, 0.f, 1.f) * SUM;
    float acc = 0.f;
    for (int i = 0; i < 6; i++) {
        acc += w[i];
        if (target <= acc) return degs[i];
    }
    return degs[5];
}

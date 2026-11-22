// scale.h — global key/scale. Key is CONTINUOUS (±12 st, microtonal allowed).
#pragma once
#include <stdint.h>

#define SCALE_DEGREES 7
#define SCALE_MODES   9

struct ScaleState {
    float   key_semis;      // -12..+12, float
    uint8_t mode;           // index into MODE table
    float   root_hz;        // F3-based root after key shift
    float   hz[5 * 7 + 1];  // 5 octaves × 7 degrees (+1 top root), rebuilt on change
};
extern ScaleState scale;
extern const char* const SCALE_MODE_NAMES[SCALE_MODES];

void  scaleInit();
void  scaleRebuild();                          // call after key/mode change
float scaleHz(int degree, int octave);         // degree 0..6, octave 0..4
// pad → (degree, octave): bottom row lowest. 16 pads = 2 octaves + 2 degrees.
void  scalePadToNote(uint8_t pad, int& degree, int& octave);
// circle-of-fifths weighted degree pick for toms: never 0 (root); 4 (fifth) most likely,
// then 1, 2, 5, 6 with exponential decay. Uses `rnd` 0..1.
int   scaleTomDegree(float rnd);

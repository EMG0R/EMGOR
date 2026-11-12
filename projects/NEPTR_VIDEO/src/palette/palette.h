// Colour.
//
// A palette is four vec3 coefficients of a cosine gradient:
//
//     rgb(t) = a + b * cos(2*pi * (c*t + d))
//
// Four numbers per channel gives an entire continuous space of palettes that
// interpolate smoothly into each other, which is what makes moving between
// archetypes a gesture rather than a jump cut. The named bank below are just
// well-chosen points in that space -- so you get a curated list AND the space
// between the list entries, from the same four numbers.
//
// Black and white is not a special case: it is saturation pulled to zero.
//
// This file is deliberately GL-free. It produces bytes; the renderer uploads
// them. That keeps the colour maths testable without a context.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nv {

struct PaletteDef {
    std::string name;
    float a[3];
    float b[3];
    float c[3];
    float d[3];
};

const std::vector<PaletteDef>& paletteBank();

// Evaluate at t in 0..1 (wrapped). out is 3 floats, 0..1.
void paletteEval(const PaletteDef& p, float t, float* outRGB);

// Linear blend of the coefficients themselves, not of the output colours.
// Interpolating the generator keeps the result a legal palette the whole way
// across, so a slow crossfade never passes through mud.
PaletteDef paletteLerp(const PaletteDef& a, const PaletteDef& b, float t);

constexpr int kLutWidth = 256;

// Writes a 256x2 RGBA8 image: row 0 is palette A, row 1 is palette B. The
// shader crossfades between the rows, so re-colouring is a texture update,
// never a recompile.
void buildDualLut(const PaletteDef& a, const PaletteDef& b, uint8_t* rgbaOut);

}  // namespace nv

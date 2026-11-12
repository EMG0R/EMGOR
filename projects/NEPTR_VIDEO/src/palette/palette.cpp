#include "palette/palette.h"

#include <algorithm>
#include <cmath>

namespace nv {

static std::vector<PaletteDef> buildBank() {
    // Coefficients are {a, b, c, d} per the cosine-gradient form. Names are
    // the archetype, not the hex code -- you pick a mood, not a colour.
    return {
        // c = d = 0.5 makes this a MONOTONIC black-to-white ramp
        // (0.5 - 0.5*cos(pi*t)). With c = 1, d = 0 it ran white -> black ->
        // white, so both the far exterior and the boundary came out white and
        // the image read as a blown-out silhouette.
        {"MONO",       {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}},
        {"SPECTRUM",   {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {1.0f,1.0f,1.0f}, {0.00f,0.33f,0.67f}},
        {"EMBER",      {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {1.0f,1.0f,1.0f}, {0.00f,0.10f,0.20f}},
        {"OCEAN",      {0.0f,0.5f,0.5f}, {0.0f,0.5f,0.5f}, {0.0f,0.5f,0.33f},{0.00f,0.50f,0.67f}},
        {"ACID",       {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {2.0f,1.0f,0.0f}, {0.50f,0.20f,0.25f}},
        {"BRUISE",     {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {1.0f,1.0f,0.5f}, {0.80f,0.90f,0.30f}},
        {"COPPER",     {0.5f,0.4f,0.3f}, {0.5f,0.4f,0.3f}, {1.0f,1.0f,1.0f}, {0.00f,0.05f,0.10f}},
        {"ULTRAVIOLET",{0.4f,0.3f,0.6f}, {0.4f,0.3f,0.5f}, {1.0f,1.0f,1.0f}, {0.30f,0.20f,0.60f}},
        {"CHLORINE",   {0.5f,0.6f,0.5f}, {0.4f,0.4f,0.4f}, {1.0f,1.0f,0.5f}, {0.10f,0.40f,0.70f}},
        {"MAGMA",      {0.3f,0.2f,0.2f}, {0.6f,0.4f,0.3f}, {1.0f,1.0f,1.0f}, {0.00f,0.15f,0.35f}},
        {"GLACIER",    {0.6f,0.7f,0.8f}, {0.3f,0.3f,0.2f}, {1.0f,1.0f,1.0f}, {0.60f,0.65f,0.70f}},
        {"NEON",       {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {2.0f,2.0f,2.0f}, {0.00f,0.25f,0.50f}},
        {"RUST",       {0.4f,0.3f,0.2f}, {0.4f,0.3f,0.2f}, {1.0f,0.7f,0.4f}, {0.00f,0.10f,0.20f}},
        {"SODIUM",     {0.6f,0.4f,0.1f}, {0.4f,0.4f,0.1f}, {1.0f,1.0f,1.0f}, {0.05f,0.10f,0.00f}},
        {"MIDNIGHT",   {0.1f,0.15f,0.3f},{0.2f,0.25f,0.5f},{1.0f,1.0f,1.0f}, {0.00f,0.10f,0.20f}},
        {"BONE",       {0.7f,0.68f,0.6f},{0.3f,0.3f,0.35f},{1.0f,1.0f,1.0f}, {0.00f,0.05f,0.10f}},
        {"TOXIC",      {0.4f,0.6f,0.3f}, {0.5f,0.4f,0.3f}, {1.0f,0.5f,2.0f}, {0.20f,0.60f,0.10f}},
        {"ROSE",       {0.6f,0.4f,0.45f},{0.4f,0.3f,0.3f}, {1.0f,1.0f,1.0f}, {0.00f,0.20f,0.40f}},
        {"INFRARED",   {0.5f,0.2f,0.2f}, {0.5f,0.3f,0.2f}, {1.0f,2.0f,3.0f}, {0.00f,0.30f,0.60f}},
        {"CYANOTYPE",  {0.2f,0.4f,0.6f}, {0.2f,0.3f,0.4f}, {1.0f,1.0f,1.0f}, {0.50f,0.55f,0.60f}},
        {"SIGNAL",     {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {3.0f,3.0f,3.0f}, {0.00f,0.33f,0.67f}},
        {"DUSK",       {0.4f,0.35f,0.45f},{0.35f,0.3f,0.4f},{1.0f,1.0f,0.5f},{0.10f,0.30f,0.60f}},
        {"IRIDESCENT", {0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {1.0f,1.0f,1.0f}, {0.30f,0.20f,0.20f}},
        {"VOID",       {0.15f,0.15f,0.2f},{0.15f,0.15f,0.25f},{1.0f,1.0f,1.0f},{0.00f,0.10f,0.20f}},
    };
}

const std::vector<PaletteDef>& paletteBank() {
    static const std::vector<PaletteDef> bank = buildBank();
    return bank;
}

void paletteEval(const PaletteDef& p, float t, float* out) {
    constexpr float kTau = 6.28318530718f;
    t = t - std::floor(t);
    for (int i = 0; i < 3; ++i) {
        float v = p.a[i] + p.b[i] * std::cos(kTau * (p.c[i] * t + p.d[i]));
        out[i] = std::clamp(v, 0.0f, 1.0f);
    }
}

PaletteDef paletteLerp(const PaletteDef& x, const PaletteDef& y, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PaletteDef r;
    r.name = t < 0.5f ? x.name : y.name;
    for (int i = 0; i < 3; ++i) {
        r.a[i] = x.a[i] + (y.a[i] - x.a[i]) * t;
        r.b[i] = x.b[i] + (y.b[i] - x.b[i]) * t;
        r.c[i] = x.c[i] + (y.c[i] - x.c[i]) * t;
        r.d[i] = x.d[i] + (y.d[i] - x.d[i]) * t;
    }
    return r;
}

void buildDualLut(const PaletteDef& a, const PaletteDef& b, uint8_t* out) {
    const PaletteDef* rows[2] = {&a, &b};
    for (int row = 0; row < 2; ++row) {
        for (int x = 0; x < kLutWidth; ++x) {
            float rgb[3];
            paletteEval(*rows[row], static_cast<float>(x) / (kLutWidth - 1), rgb);
            uint8_t* px = out + (row * kLutWidth + x) * 4;
            px[0] = static_cast<uint8_t>(rgb[0] * 255.0f + 0.5f);
            px[1] = static_cast<uint8_t>(rgb[1] * 255.0f + 0.5f);
            px[2] = static_cast<uint8_t>(rgb[2] * 255.0f + 0.5f);
            px[3] = 255;
        }
    }
}

}  // namespace nv

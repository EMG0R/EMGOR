#include "palette/palette.h"

#include <cstdint>
#include <string>
#include <vector>

#include "check.h"

using namespace nv;

namespace {

void testBankIsSane() {
    const auto& b = paletteBank();
    CHECK(b.size() >= 16);
    for (const auto& p : b) CHECK(!p.name.empty());
}

void testEvalStaysInGamut() {
    // Cosine palettes can overshoot; the evaluator must clamp, or the LUT
    // byte conversion wraps and a smooth gradient gets a hard seam in it.
    for (const auto& p : paletteBank()) {
        for (int i = 0; i <= 64; ++i) {
            float rgb[3];
            paletteEval(p, static_cast<float>(i) / 64.0f, rgb);
            for (int c = 0; c < 3; ++c) {
                CHECK(rgb[c] >= 0.0f && rgb[c] <= 1.0f);
            }
        }
    }
}

void testEvalWrapsAtOne() {
    const auto& p = paletteBank()[1];
    float a[3], b[3];
    paletteEval(p, 0.0f, a);
    paletteEval(p, 1.0f, b);
    // t is wrapped, so 1.0 lands back on 0.0 -- the palette-rotate control
    // has to cycle without a discontinuity.
    for (int c = 0; c < 3; ++c) CHECK_NEAR(a[c], b[c], 1e-5f);
}

void testLerpEndpoints() {
    const auto& bank = paletteBank();
    PaletteDef at0 = paletteLerp(bank[0], bank[3], 0.0f);
    PaletteDef at1 = paletteLerp(bank[0], bank[3], 1.0f);
    for (int c = 0; c < 3; ++c) {
        CHECK_NEAR(at0.a[c], bank[0].a[c], 1e-6f);
        CHECK_NEAR(at1.d[c], bank[3].d[c], 1e-6f);
    }
}

void testLerpIsContinuous() {
    // Interpolating the generator, not the output, is what keeps a slow
    // palette crossfade from passing through mud. Adjacent steps must stay
    // close in output space.
    const auto& bank = paletteBank();
    float prev[3] = {0, 0, 0};
    bool first = true;
    for (int i = 0; i <= 32; ++i) {
        PaletteDef m = paletteLerp(bank[2], bank[9], static_cast<float>(i) / 32.0f);
        float rgb[3];
        paletteEval(m, 0.37f, rgb);
        if (!first) {
            for (int c = 0; c < 3; ++c) CHECK(std::abs(rgb[c] - prev[c]) < 0.2f);
        }
        for (int c = 0; c < 3; ++c) prev[c] = rgb[c];
        first = false;
    }
}

void testDualLutLayout() {
    std::vector<uint8_t> lut(kLutWidth * 2 * 4, 0);
    const auto& bank = paletteBank();
    buildDualLut(bank[1], bank[5], lut.data());

    // Row 0 is A, row 1 is B, and alpha is opaque throughout.
    for (int x = 0; x < kLutWidth; ++x) {
        CHECK_EQ(static_cast<int>(lut[x * 4 + 3]), 255);
        CHECK_EQ(static_cast<int>(lut[(kLutWidth + x) * 4 + 3]), 255);
    }

    float rgb[3];
    paletteEval(bank[1], 0.5f, rgb);
    int x = kLutWidth / 2;
    CHECK_NEAR(static_cast<int>(lut[x * 4]), static_cast<int>(rgb[0] * 255.0f + 0.5f), 2);

    paletteEval(bank[5], 0.5f, rgb);
    CHECK_NEAR(static_cast<int>(lut[(kLutWidth + x) * 4 + 1]),
               static_cast<int>(rgb[1] * 255.0f + 0.5f), 2);
}

void testMonoIsActuallyGrey() {
    // MONO must be neutral by construction, so black and white is reachable
    // without touching the saturation control.
    const auto& p = paletteBank()[0];
    for (int i = 0; i <= 16; ++i) {
        float rgb[3];
        paletteEval(p, static_cast<float>(i) / 16.0f, rgb);
        CHECK_NEAR(rgb[0], rgb[1], 1e-5f);
        CHECK_NEAR(rgb[1], rgb[2], 1e-5f);
    }
}

}  // namespace

int main() {
    testBankIsSane();
    testEvalStaysInGamut();
    testEvalWrapsAtOne();
    testLerpEndpoints();
    testLerpIsContinuous();
    testDualLutLayout();
    testMonoIsActuallyGrey();
    return check::finish("palette");
}

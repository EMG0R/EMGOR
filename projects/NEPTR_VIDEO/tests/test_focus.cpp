// Boundary finding, against synthetic frames whose right answer is known.
//
// The claim is specific and worth pinning: the target is the frontier between
// interior and exterior, NOT wherever the picture happens to be busiest.
// Getting that backwards is the failure that ends a dive in the void, and it
// is invisible on a single frame -- you only see it five decades later, when
// the screen is flat.
#include <cstdint>
#include <vector>

#include "check.h"
#include "focus.h"

using namespace nv;

namespace {

constexpr int kDim = 32;

std::vector<uint8_t> blank(int v = 0) {
    std::vector<uint8_t> f(kDim * kDim * 4, 0);
    for (int i = 0; i < kDim * kDim; ++i) {
        f[4 * i + 0] = static_cast<uint8_t>(v);
        f[4 * i + 1] = static_cast<uint8_t>(v);
        f[4 * i + 2] = static_cast<uint8_t>(v);
        f[4 * i + 3] = 255;
    }
    return f;
}

void put(std::vector<uint8_t>& f, int x, int y, int v) {
    const int i = y * kDim + x;
    f[4 * i + 0] = f[4 * i + 1] = f[4 * i + 2] = static_cast<uint8_t>(v);
}

// An all-interior frame has no boundary and must report none: this is the
// "already in the void" case, and inventing a target here would steer the
// dive to an arbitrary black cell.
void testSolidInteriorHasNoTarget() {
    auto f = blank(0);
    const FocusScore s = scoreFocus(f.data(), kDim);
    CHECK(!s.valid);
    CHECK(s.liveFraction == 0.0f);
}

// Likewise a frame with nothing interior in it at all.
void testSolidExteriorHasNoTarget() {
    auto f = blank(200);
    const FocusScore s = scoreFocus(f.data(), kDim);
    CHECK(!s.valid);
    CHECK(s.liveFraction == 1.0f);
}

// A half-black frame: the target must land ON the seam, not in either field.
void testFindsTheSeam() {
    auto f = blank(0);
    for (int y = 0; y < kDim; ++y)
        for (int x = kDim / 2; x < kDim; ++x) put(f, x, y, 180);

    const FocusScore s = scoreFocus(f.data(), kDim);
    CHECK(s.valid);
    // Within one cell of the seam.
    CHECK(s.cx >= kDim / 2 - 1 && s.cx <= kDim / 2);
}

// The decisive one. A bright, high-contrast CHECKERBOARD sits in the
// exterior, far from the seam -- exactly what palette banding looks like.
// A "find the busiest region" scorer goes there and dives into the void.
// The boundary must win.
void testBoundaryBeatsExteriorContrast() {
    auto f = blank(0);
    // Left third interior (black), middle third a plain lit field: the seam
    // is at x = kDim/3.
    for (int y = 0; y < kDim; ++y)
        for (int x = kDim / 3; x < kDim; ++x) put(f, x, y, 120);
    // A violent checkerboard in the far right, nowhere near the seam.
    for (int y = 2; y < kDim - 2; ++y)
        for (int x = 3 * kDim / 4; x < kDim - 2; ++x)
            put(f, x, y, ((x + y) & 1) ? 255 : 40);

    const FocusScore s = scoreFocus(f.data(), kDim);
    CHECK(s.valid);
    // Must be at the seam, NOT in the checkerboard.
    CHECK(s.cx <= kDim / 3 + 1);
    CHECK(s.cx < 3 * kDim / 4);
}

}  // namespace

int main() {
    testSolidInteriorHasNoTarget();
    testSolidExteriorHasNoTarget();
    testFindsTheSeam();
    testBoundaryBeatsExteriorContrast();
    return check::finish("focus");
}

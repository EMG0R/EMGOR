// Screen rotation.
//
// The picture is turned in a shader and the touch input is turned in a
// Linux-only evdev reader. Those two live nowhere near each other and cannot
// disagree quietly: if the image rotates and the finger does not, every tap
// lands somewhere else and the panel reads as BROKEN INPUT rather than as a
// sideways screen. This is the only place the pair can be held together.

#include "check.h"
#include "platform/rotate.h"

using namespace nv;

namespace {

void near(Pt a, float x, float y) {
    CHECK_NEAR(a.x, x, 0.0001f);
    CHECK_NEAR(a.y, y, 0.0001f);
}

// 0 is the identity in both directions -- the default must cost nothing and
// change nothing.
void testIdentity() {
    near(rotateForward(0, {0.25f, 0.75f}), 0.25f, 0.75f);
    near(rotateInverse(0, {0.25f, 0.75f}), 0.25f, 0.75f);
    CHECK(!isQuarterTurn(0));
    CHECK(!isQuarterTurn(180));
    CHECK(isQuarterTurn(90));
    CHECK(isQuarterTurn(270));
}

// Degrees CLOCKWISE. Coordinates are 0..1 with y DOWN, so the logical
// top-left corner is (0,0).
void testCorners() {
    // 90 CW: top-left goes to the top-RIGHT.
    near(rotateForward(90, {0.0f, 0.0f}), 1.0f, 0.0f);
    near(rotateForward(90, {1.0f, 0.0f}), 1.0f, 1.0f);

    // 270 CW (= 90 counter-clockwise, this rig's panel): top-left goes to the
    // BOTTOM-left. Getting this backwards is the 180-degree error the whole
    // convention comment exists to prevent, so it is pinned explicitly.
    near(rotateForward(270, {0.0f, 0.0f}), 0.0f, 1.0f);
    near(rotateForward(270, {1.0f, 0.0f}), 0.0f, 0.0f);

    near(rotateForward(180, {0.0f, 0.0f}), 1.0f, 1.0f);
}

// The property that actually matters: whatever the display does, the touch
// reader undoes exactly.
void testRoundTrip() {
    const float pts[][2] = {{0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f},
                            {0.13f, 0.87f}, {0.62f, 0.5f}};
    for (int deg : {0, 90, 180, 270}) {
        for (const auto& p : pts) {
            Pt phys = rotateForward(deg, {p[0], p[1]});
            Pt back = rotateInverse(deg, phys);
            near(back, p[0], p[1]);
        }
    }
}

// Negative and over-360 values normalise rather than falling through to the
// identity, so a config carrying -90 does what it says.
void testNormalisation() {
    near(rotateForward(-90, {0.0f, 0.0f}), rotateForward(270, {0.0f, 0.0f}).x,
         rotateForward(270, {0.0f, 0.0f}).y);
    near(rotateForward(450, {0.3f, 0.4f}), rotateForward(90, {0.3f, 0.4f}).x,
         rotateForward(90, {0.3f, 0.4f}).y);
    CHECK(isQuarterTurn(-90));
}

// The concrete case on the device: at 270, tapping the physical point 0.62
// across / 0.50 down must land on the GAMES row -- logical y around 0.62, i.e.
// the third of four full-height rows.
void testDeviceCase() {
    Pt logical = rotateInverse(270, {0.62f, 0.50f});
    near(logical, 0.50f, 0.62f);
}

}  // namespace

int main() {
    testIdentity();
    testCorners();
    testRoundTrip();
    testNormalisation();
    testDeviceCase();
    return check::finish("rotate");
}

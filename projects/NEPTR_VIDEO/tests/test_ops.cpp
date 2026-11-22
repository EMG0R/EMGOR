#include <cmath>

#include "chain/ops.h"
#include "check.h"

using namespace nv;

namespace {

void testCatalogueStableAndUnique() {
    const auto& b = jitOpBank();
    CHECK(b.size() >= 50);
    for (size_t i = 0; i < b.size(); ++i) {
        CHECK(b[i].id != nullptr);
        CHECK(b[i].name != nullptr);
        CHECK(jitOpIndex(b[i].id) == static_cast<int>(i));
        for (size_t j = i + 1; j < b.size(); ++j) CHECK(std::string(b[i].id) != b[j].id);
    }
}

void testInactiveIdentity() {
    OpVec4 a{0.25f, 0.5f, 0.75f, 1.0f};
    for (const auto& op : jitOpBank()) {
        if (op.kind != JitOpKind::Pointwise) continue;
        auto got = jitOpApply(op, a, {}, false, false, 0.0f);
        auto expected = jitOpApply(op, a, {op.identity, op.identity, op.identity, op.identity}, true, false, 0.0f);
        CHECK_NEAR(got.x, expected.x, 1e-5f);
        CHECK_NEAR(got.y, expected.y, 1e-5f);
        CHECK_NEAR(got.z, expected.z, 1e-5f);
        CHECK_NEAR(got.w, expected.w, 1e-5f);
    }
}

void testArithmeticIdentities() {
    OpVec4 a{0.2f, 0.4f, 0.6f, 0.8f};
    const auto& mult = jitOpBank()[jitOpIndex("mul")];
    auto m = jitOpApply(mult, a, {}, false, false, 0.0f);
    CHECK_NEAR(m.x, a.x, 1e-5f);
    CHECK_NEAR(m.w, a.w, 1e-5f);
    const auto& add = jitOpBank()[jitOpIndex("add")];
    auto z = jitOpApply(add, a, {}, false, false, 0.0f);
    CHECK_NEAR(z.x, a.x, 1e-5f);
    const auto& sub = jitOpBank()[jitOpIndex("sub")];
    auto black = jitOpApply(sub, a, a, true, false, 0.0f);
    CHECK(std::fabs(black.x) < 1e-5f && std::fabs(black.w) < 1e-5f);
}

void testExplicitValueOverridesIdentity() {
    const auto& mult = jitOpBank()[jitOpIndex("mul")];
    OpVec4 a{0.25f, 0.5f, 0.75f, 1.0f};
    auto got = jitOpApply(mult, a, {}, false, true, 0.5f);
    CHECK_NEAR(got.x, 0.125f, 1e-5f);
    CHECK_NEAR(got.w, 0.5f, 1e-5f);

    const auto& passB = jitOpBank()[jitOpIndex("pass_b")];
    auto fallback = jitOpApply(passB, a, {}, false, false, 0.0f);
    CHECK_NEAR(fallback.x, a.x, 1e-5f);
    auto explicitB = jitOpApply(passB, a, {}, false, true, 0.25f);
    CHECK_NEAR(explicitB.x, 0.25f, 1e-5f);
}

}  // namespace

int main() {
    testCatalogueStableAndUnique();
    testInactiveIdentity();
    testArithmeticIdentities();
    testExplicitValueOverridesIdentity();
    return 0;
}

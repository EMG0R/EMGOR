// The fusion rule is the load-bearing performance decision in the engine, and
// it is pure logic with no GL involved, so it is tested directly.
#include "chain/compiler.h"

#include <string>
#include <vector>

#include "check.h"

using namespace nv;

namespace {

ChainSlot slot(const char* id, bool enabled = true) {
    ChainSlot s;
    s.def = findEffect(id);
    s.enabled = enabled;
    return s;
}

void testEmptyChain() {
    auto passes = groupIntoPasses({});
    CHECK_EQ(static_cast<int>(passes.size()), 0);
}

void testPointwiseAllFuse() {
    // Six pointwise effects must cost exactly one full-screen pass. This is
    // the whole point of the compiler.
    std::vector<ChainSlot> c = {
        slot("saturate"), slot("levels"), slot("hue"),
        slot("posterize"), slot("invert"), slot("grain"),
    };
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 1);
    CHECK_EQ(static_cast<int>(passes[0].slots.size()), 6);
    CHECK(!passes[0].readsHistory);
    CHECK(!passes[0].readsNeighbors);
}

void testSamplerOpensPass() {
    // A sampler establishes the colour itself, so pointwise work queued
    // before it cannot share its pass -- it would be discarded.
    std::vector<ChainSlot> c = {slot("saturate"), slot("blur"), slot("levels")};
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 2);
    CHECK_EQ(static_cast<int>(passes[0].slots.size()), 1);  // saturate
    CHECK_EQ(static_cast<int>(passes[1].slots.size()), 2);  // blur + levels
    CHECK(passes[1].readsNeighbors);
}

void testTwoSamplersDoNotShare() {
    std::vector<ChainSlot> c = {slot("blur"), slot("edge")};
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 2);
}

void testHistoryOpensPassAndIsFlagged() {
    std::vector<ChainSlot> c = {slot("feedback"), slot("saturate"), slot("grain")};
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 1);
    CHECK(passes[0].readsHistory);
    CHECK_EQ(static_cast<int>(passes[0].slots.size()), 3);
}

void testRealisticChainCollapses() {
    // The headline claim: a twelve-effect chain does not cost twelve passes.
    std::vector<ChainSlot> c = {
        slot("feedback"),   // pass 0 opens
        slot("saturate"),   // fuses
        slot("levels"),     // fuses
        slot("hue"),        // fuses
        slot("kaleido"),    // pass 1 opens
        slot("posterize"),  // fuses
        slot("gamma"),      // fuses
        slot("blur"),       // pass 2 opens
        slot("threshold"),  // fuses
        slot("vignette"),   // fuses
        slot("grain"),      // fuses
        slot("gain"),       // fuses
    };
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 3);
}

void testDisabledSlotsCostNothing() {
    std::vector<ChainSlot> c = {
        slot("saturate"), slot("blur", false), slot("levels"),
    };
    auto passes = groupIntoPasses(c);
    // With the blur bypassed the two pointwise effects fuse: a disabled
    // effect must not leave a pass boundary behind it.
    CHECK_EQ(static_cast<int>(passes.size()), 1);
    CHECK_EQ(static_cast<int>(passes[0].slots.size()), 2);
}

void testUnknownEffectIsSkipped() {
    ChainSlot bad;
    bad.def = 9999;
    std::vector<ChainSlot> c = {slot("saturate"), bad, slot("levels")};
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 1);
    CHECK_EQ(static_cast<int>(passes[0].slots.size()), 2);
}

void testGeneratedSourceShape() {
    std::vector<ChainSlot> c = {slot("feedback"), slot("saturate")};
    auto passes = groupIntoPasses(c);
    CHECK_EQ(static_cast<int>(passes.size()), 1);
    std::string src = generatePassSource(c, passes[0]);

    CHECK(src.find("void main()") != std::string::npos);
    CHECK(src.find("fx_0_feedback") != std::string::npos);
    CHECK(src.find("fx_1_saturate") != std::string::npos);
    CHECK(src.find("uniform vec4 uFxP0;") != std::string::npos);
    CHECK(src.find("uniform vec4 uFxP1;") != std::string::npos);
    // A pass that opens with a sampling effect must NOT also do the default
    // fetch: that would be a wasted texture read every pixel, every frame.
    CHECK(src.find("vec4 col = vec4(0.0);") != std::string::npos);
}

void testPointwiseFirstPassKeepsDefaultFetch() {
    std::vector<ChainSlot> c = {slot("saturate")};
    auto passes = groupIntoPasses(c);
    std::string src = generatePassSource(c, passes[0]);
    CHECK(src.find("vec4 col = nvSample(uInput, uv);") != std::string::npos);
}

void testKeyIgnoresParameterBinding() {
    // Turning a knob must never recompile a shader. Knob values do not even
    // live in the chain any more -- the slot only carries bank indices, and
    // the key has to be blind to those too.
    std::vector<ChainSlot> a = {slot("blur"), slot("saturate")};
    std::vector<ChainSlot> b = a;
    b[0].pIndex[0] = 41;
    b[1].pIndex[0] = 77;
    CHECK(compileChain(a).key == compileChain(b).key);
}

void testKeyChangesOnReorder() {
    std::vector<ChainSlot> a = {slot("blur"), slot("saturate")};
    std::vector<ChainSlot> b = {slot("saturate"), slot("blur")};
    CHECK(compileChain(a).key != compileChain(b).key);
}

void testKeyChangesOnBypass() {
    std::vector<ChainSlot> a = {slot("blur"), slot("saturate")};
    std::vector<ChainSlot> b = {slot("blur", false), slot("saturate")};
    CHECK(compileChain(a).key != compileChain(b).key);
}

void testEveryBankEffectDeclaresItsParams() {
    for (const auto& e : effectBank()) {
        CHECK(!e.id.empty());
        CHECK(!e.glsl.empty());
        CHECK(e.params.size() <= 4);  // knobs are packed into a single vec4
    }
}

}  // namespace

int main() {
    testEmptyChain();
    testPointwiseAllFuse();
    testSamplerOpensPass();
    testTwoSamplersDoNotShare();
    testHistoryOpensPassAndIsFlagged();
    testRealisticChainCollapses();
    testDisabledSlotsCostNothing();
    testUnknownEffectIsSkipped();
    testGeneratedSourceShape();
    testPointwiseFirstPassKeepsDefaultFetch();
    testKeyIgnoresParameterBinding();
    testKeyChangesOnReorder();
    testKeyChangesOnBypass();
    testEveryBankEffectDeclaresItsParams();
    return check::finish("compiler");
}

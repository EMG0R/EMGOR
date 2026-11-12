// The parameter bank is the single place any control value lives. If a
// rebuild loses values, or two effects collide on an id, every input path in
// the instrument is wrong at once -- so this is tested directly.
#include "patch.h"

#include <set>
#include <string>
#include <vector>

#include "check.h"

using namespace nv;

namespace {

ChainSlot slot(const char* id) {
    ChainSlot s;
    s.def = findEffect(id);
    return s;
}

const KernelDef& kernelById(const char* id) {
    for (const auto& k : kernelBank()) {
        if (k.id == id) return k;
    }
    return kernelBank()[0];
}

ParamBank freshBank() {
    ParamBank b;
    registerGlobalParams(b);
    return b;
}

void testGlobalsSurviveRebuild() {
    ParamBank b = freshBank();
    int zoom = b.find(gid::kZoom);
    CHECK(zoom >= 0);
    b.setNorm(zoom, 0.77f);

    std::vector<ChainSlot> chain = {slot("blur")};
    rebuildPatchParams(b, kernelById("julia"), nullptr, chain);
    // Global indices must not move, and their values must not change.
    CHECK_EQ(b.find(gid::kZoom), zoom);
    CHECK_NEAR(b.norm(zoom), 0.77f, 1e-6f);

    chain.push_back(slot("saturate"));
    rebuildPatchParams(b, kernelById("phoenix"), nullptr, chain);
    CHECK_EQ(b.find(gid::kZoom), zoom);
    CHECK_NEAR(b.norm(zoom), 0.77f, 1e-6f);
}

void testEffectKnobsGetBankEntries() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("feedback"), slot("saturate")};
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);

    // FEEDBACK declares four knobs, SATURATE one.
    CHECK(chain[0].pIndex[0] >= 0);
    CHECK(chain[0].pIndex[3] >= 0);
    CHECK(chain[1].pIndex[0] >= 0);
    CHECK_EQ(chain[1].pIndex[1], -1);

    for (int i = 0; i < 4; ++i) {
        CHECK(chain[0].pIndex[i] < b.count());
    }
}

void testEffectKnobValuesSurviveAnUnrelatedEdit() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("feedback"), slot("saturate")};
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);

    b.setNorm(chain[0].pIndex[0], 0.31f);
    b.setNorm(chain[1].pIndex[0], 0.62f);

    // Bypassing an effect must not disturb any other knob on the instrument.
    chain[1].enabled = false;
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);
    CHECK_NEAR(b.norm(chain[0].pIndex[0]), 0.31f, 1e-6f);
    CHECK_NEAR(b.norm(chain[1].pIndex[0]), 0.62f, 1e-6f);
}

void testAppendingAnEffectKeepsEarlierSlots() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("feedback")};
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);
    b.setNorm(chain[0].pIndex[1], 0.88f);

    chain.push_back(slot("blur"));
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);
    CHECK_NEAR(b.norm(chain[0].pIndex[1]), 0.88f, 1e-6f);
}

void testTwoInstancesOfOneEffectAreIndependent() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("blur"), slot("blur")};
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);

    CHECK(chain[0].pIndex[0] != chain[1].pIndex[0]);
    b.setNorm(chain[0].pIndex[0], 0.2f);
    b.setNorm(chain[1].pIndex[0], 0.9f);
    CHECK_NEAR(b.norm(chain[0].pIndex[0]), 0.2f, 1e-6f);
    CHECK_NEAR(b.norm(chain[1].pIndex[0]), 0.9f, 1e-6f);
}

void testEveryIdIsUnique() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("blur"), slot("blur"), slot("feedback"),
                                    slot("levels")};
    rebuildPatchParams(b, kernelById("multijulia"), nullptr, chain);

    std::set<std::string> ids;
    for (int i = 0; i < b.count(); ++i) {
        bool inserted = ids.insert(b.def(i).id).second;
        CHECK(inserted);
    }
}

void testKernelKnobsSitAtGlobalEnd() {
    // The renderer reads uKernelP as the four entries starting at
    // globalEnd(). That convention is load-bearing, so it is asserted.
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain = {slot("blur")};
    const KernelDef& k = kernelById("multijulia");
    rebuildPatchParams(b, k, nullptr, chain);

    CHECK(b.globalEnd() + 4 <= b.count());
    for (size_t i = 0; i < k.params.size(); ++i) {
        CHECK(b.def(b.globalEnd() + static_cast<int>(i)).id == k.params[i].id);
    }
}

void testKernelWithNoKnobsStillPadsToFour() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain;
    rebuildPatchParams(b, kernelById("mandelbrot"), nullptr, chain);
    // Mandelbrot has no knobs, and there is no Channel B or morph target --
    // but all THREE kernel sections remain four wide, because activating B
    // must not shift effects or uniform offsets.
    CHECK_EQ(b.count(), b.globalEnd() + 12);
}

void testMorphTargetGetsItsOwnKnobs() {
    // A morph pair of the same family must not share one set of knobs, or
    // moving A's C RE would drag B's along with it and the morph would have
    // nothing to interpolate between.
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain;
    rebuildPatchParams(b, kernelById("julia"), &kernelById("burningjulia"), chain);

    int aRe = b.find("k.cre");
    int bRe = b.find("a.mk.k.cre");
    CHECK(aRe >= 0);
    CHECK(bRe >= 0);
    CHECK(aRe != bRe);

    b.setNorm(aRe, 0.2f);
    b.setNorm(bRe, 0.8f);
    CHECK_NEAR(b.norm(aRe), 0.2f, 1e-6f);
    CHECK_NEAR(b.norm(bRe), 0.8f, 1e-6f);
}

void testMorphSectionSitsAtGlobalEndPlusEight() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain;
    const KernelDef& kb = kernelById("burningjulia");
    rebuildPatchParams(b, kernelById("julia"), &kb, chain);
    for (size_t i = 0; i < kb.params.size(); ++i) {
        CHECK(b.def(b.globalEnd() + 8 + static_cast<int>(i)).id ==
              "a.mk." + kb.params[i].id);
    }
}

void testChannelBDoesNotShareMorphStorage() {
    ParamBank b = freshBank();
    std::vector<ChainSlot> chain;
    const KernelDef& kb = kernelById("burningjulia");
    rebuildPatchParams(b, kernelById("julia"), &kb, chain, &kb);

    int bRe = b.find("b.k.cre");
    int morphRe = b.find("a.mk.k.cre");
    CHECK(bRe >= 0);
    CHECK(morphRe >= 0);
    CHECK(bRe != morphRe);
    b.setValueNow(bRe, -1.25f);
    b.setValueNow(morphRe, 1.25f);
    CHECK_NEAR(b.valueOf("b.k.cre", 0.0f), -1.25f, 1e-5f);
    CHECK_NEAR(b.valueOf("a.mk.k.cre", 0.0f), 1.25f, 1e-5f);
}

void testFieldAndEscapeCannotMorph() {
    // There is nothing shared to interpolate between an iterated orbit and a
    // direct function of position, so the pairing is refused rather than
    // faked with a crossfade.
    CHECK(!canMorph(kernelById("mandelbrot"), kernelById("lava")));
    CHECK(canMorph(kernelById("mandelbrot"), kernelById("burningship")));
    CHECK(canMorph(kernelById("lava"), kernelById("plasma")));
}

void testCurvesRoundTrip() {
    ParamBank b = freshBank();
    for (int i = 0; i < b.count(); ++i) {
        b.setNorm(i, 0.3f);
        float v = b.value(i);
        b.setValue(i, v);
        CHECK_NEAR(b.norm(i), 0.3f, 1e-3f);
    }
}

void testNormIsAlwaysClamped() {
    ParamBank b = freshBank();
    int z = b.find(gid::kZoom);
    b.setNorm(z, 5.0f);
    CHECK_NEAR(b.norm(z), 1.0f, 1e-6f);
    b.nudgeNorm(z, -9.0f);
    CHECK_NEAR(b.norm(z), 0.0f, 1e-6f);
}

}  // namespace

int main() {
    testGlobalsSurviveRebuild();
    testEffectKnobsGetBankEntries();
    testEffectKnobValuesSurviveAnUnrelatedEdit();
    testAppendingAnEffectKeepsEarlierSlots();
    testTwoInstancesOfOneEffectAreIndependent();
    testEveryIdIsUnique();
    testKernelKnobsSitAtGlobalEnd();
    testKernelWithNoKnobsStillPadsToFour();
    testMorphTargetGetsItsOwnKnobs();
    testMorphSectionSitsAtGlobalEndPlusEight();
    testChannelBDoesNotShareMorphStorage();
    testFieldAndEscapeCannotMorph();
    testCurvesRoundTrip();
    testNormIsAlwaysClamped();
    return check::finish("patch");
}

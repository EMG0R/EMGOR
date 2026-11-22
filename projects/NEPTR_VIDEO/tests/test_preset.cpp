// A preset's whole value is that it still works LATER, on the OTHER machine.
// Every test here is a compatibility guarantee, not a formatting check.
#include "preset.h"

#include <string>
#include <vector>

#include "check.h"
#include "core/kernels.h"
#include "patch.h"

using namespace nv;

namespace {

ChainSlot slot(const char* id, bool on = true) {
    ChainSlot s;
    s.def = findEffect(id);
    s.enabled = on;
    return s;
}

const KernelDef& kernelById(const char* id) {
    for (const auto& k : kernelBank()) {
        if (k.id == id) return k;
    }
    return kernelBank()[0];
}

struct Rig {
    ParamBank bank;
    Selection sel;
    std::vector<ChainSlot> chain;
};

Rig makeRig() {
    Rig r;
    registerGlobalParams(r.bank);
    r.chain = {slot("feedback"), slot("kaleido", false), slot("saturate")};
    rebuildPatchParams(r.bank, kernelById("julia"), nullptr, r.chain);
    return r;
}

void testRoundTrip() {
    Rig a = makeRig();
    a.sel.mode = Mode::Particles;
    a.sel.sourceB = SourceKind::Fractal;
    a.sel.channelBKernel = 4;
    a.sel.colorMode = 4;
    a.bank.setValue(a.bank.find(gid::kDetail), 211.0f);
    a.bank.setValue(a.bank.find(gid::kPtGravity), -2.25f);

    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    PresetData q;
    std::string err;
    CHECK(parsePreset(serializePreset(p), q, &err));

    Rig b = makeRig();
    applyPresetSelection(q, b.sel, b.chain);
    rebuildPatchParams(b.bank, kernelById(q.kernel.c_str()), nullptr, b.chain);
    applyPresetParams(q, b.bank, nullptr);

    CHECK(b.sel.mode == Mode::Particles);
    CHECK(b.sel.sourceB == SourceKind::Fractal);
    CHECK_EQ(b.sel.channelBKernel, 4);
    CHECK_EQ(b.sel.colorMode, 4);
    CHECK_NEAR(b.bank.valueOf(gid::kDetail), 211.0f, 0.5f);
    CHECK_NEAR(b.bank.valueOf(gid::kPtGravity), -2.25f, 0.01f);
    CHECK_EQ(static_cast<int>(b.chain.size()), 3);
    CHECK(!b.chain[1].enabled);  // the bypass survived
}

void testUnknownKeysAreSkipped() {
    // A preset written by a NEWER build. It must load, minus what this build
    // cannot represent -- not fail.
    std::string text =
        "version 2\n"
        "mode SYNTH\n"
        "kernel mandelbrot\n"
        "palette_a SPECTRUM\n"
        "palette_b MAGMA\n"
        "quantum_flux 3.5\n"          // a key from the future
        "p g.detail 128\n"
        "p g.not_a_real_param 7\n"    // a knob from the future
        "p g.zoom 2.5\n";
    PresetData p;
    std::string err;
    CHECK(parsePreset(text, p, &err));

    Rig r = makeRig();
    int unknown = 0;
    int applied = applyPresetParams(p, r.bank, &unknown);
    CHECK_EQ(applied, 2);
    CHECK_EQ(unknown, 1);
    CHECK_NEAR(r.bank.valueOf(gid::kDetail), 128.0f, 0.5f);
}

void testMissingKeysKeepDefaults() {
    // A preset from an OLDER build, before some knobs existed. The knobs it
    // does not mention must stay at their defaults rather than reset to zero.
    std::string text =
        "version 1\n"
        "kernel mandelbrot\n"
        "p g.detail 64\n";
    PresetData p;
    CHECK(parsePreset(text, p, nullptr));

    Rig r = makeRig();
    float satBefore = r.bank.valueOf(gid::kPaletteSat);
    applyPresetParams(p, r.bank, nullptr);
    CHECK_NEAR(r.bank.valueOf(gid::kDetail), 64.0f, 0.5f);
    CHECK_NEAR(r.bank.valueOf(gid::kPaletteSat), satBefore, 1e-5f);
}

void testIdsNotIndices() {
    // THE guarantee. Inserting a parameter ahead of the saved ones must not
    // shift what a stored value means -- which is exactly what would happen
    // if the file recorded positions.
    Rig a = makeRig();
    a.bank.setValue(a.bank.find(gid::kZoom), 4.0f);
    a.bank.setValue(a.bank.find(gid::kDetail), 150.0f);
    PresetData p = capturePreset(a.bank, a.sel, a.chain);

    ParamBank shifted;
    // A build that gained a knob at the very front.
    shifted.add({"g.brand_new", "NEW", "global", 0.0f, 1.0f, 0.5f, Curve::Linear});
    registerGlobalParams(shifted);
    std::vector<ChainSlot> c = a.chain;
    rebuildPatchParams(shifted, kernelById("julia"), nullptr, c);

    applyPresetParams(p, shifted, nullptr);
    CHECK_NEAR(shifted.valueOf(gid::kZoom), 4.0f, 0.01f);
    CHECK_NEAR(shifted.valueOf(gid::kDetail), 150.0f, 0.5f);
    // The knob the preset never heard of keeps its default.
    CHECK_NEAR(shifted.valueOf("g.brand_new"), 0.5f, 1e-5f);
}

void testGeneratorAndPaletteByName() {
    // Stored by id/name, so reordering the banks cannot silently change which
    // generator or palette a preset means.
    Rig a = makeRig();
    a.sel.kernelIndex = 4;  // BURNING SHIP
    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    CHECK(p.kernel == "burningship");
    CHECK(!p.paletteA.empty());
    CHECK(p.paletteA.find(' ') == std::string::npos);  // stays one token

    Selection s2;
    std::vector<ChainSlot> c2;
    applyPresetSelection(p, s2, c2);
    CHECK_EQ(s2.kernelIndex, 4);
}

void testMorphTargetSurvives() {
    Rig a = makeRig();
    a.sel.kernelIndex = 0;
    a.sel.kernelB = 4;
    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    CHECK(p.kernelB == "burningship");

    Selection s2;
    std::vector<ChainSlot> c2;
    applyPresetSelection(p, s2, c2);
    CHECK_EQ(s2.kernelB, 4);
}

void testUnknownGeneratorDoesNotWipeEverything() {
    // A preset naming a generator this build lacks should still restore
    // everything else rather than snapping to index 0.
    std::string text =
        "version 1\n"
        "kernel some_future_fractal\n"
        "palette_a SPECTRUM\n"
        "p g.detail 77\n";
    PresetData p;
    CHECK(parsePreset(text, p, nullptr));

    Selection s;
    s.kernelIndex = 6;
    std::vector<ChainSlot> c;
    applyPresetSelection(p, s, c);
    CHECK_EQ(s.kernelIndex, 6);  // left alone, not reset
}

void testUnknownEffectIsDropped() {
    std::string text =
        "version 1\n"
        "kernel mandelbrot\n"
        "fx feedback on\n"
        "fx warp_drive on\n"   // not in this build
        "fx saturate off\n";
    PresetData p;
    CHECK(parsePreset(text, p, nullptr));

    Selection s;
    std::vector<ChainSlot> c;
    applyPresetSelection(p, s, c);
    CHECK_EQ(static_cast<int>(c.size()), 2);
    CHECK(c[0].enabled);
    CHECK(!c[1].enabled);
}

void testRejectsNonPreset() {
    PresetData p;
    std::string err;
    CHECK(!parsePreset("hello\nworld\n", p, &err));
    CHECK(!err.empty());
}

void testCommentsAndBlankLines() {
    PresetData p;
    CHECK(parsePreset("# a comment\n\nversion 1\np g.zoom 3  # trailing\n", p,
                      nullptr));
    CHECK_EQ(static_cast<int>(p.params.size()), 1);
    CHECK_NEAR(p.params[0].second, 3.0f, 1e-5f);
}

void testPaddingIsNotWritten() {
    // The hidden uniform-packing slots are not state; writing them would put
    // meaningless keys in every file forever.
    Rig a = makeRig();
    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    for (const auto& kv : p.params) {
        CHECK(kv.first.find("unused") == std::string::npos);
    }
}

void testValuesAreRealUnitsNotNormalized() {
    // Real units survive a range change; a normalized 0.5 would silently mean
    // something different if a range were ever widened.
    Rig a = makeRig();
    a.bank.setValue(a.bank.find(gid::kDetail), 300.0f);
    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    bool found = false;
    for (const auto& kv : p.params) {
        if (kv.first == gid::kDetail) {
            CHECK(kv.second > 2.0f);  // 300, not a 0..1 fraction
            found = true;
        }
    }
    CHECK(found);
}

void testNegativeSpeedSurvivesTheFile() {
    // SPEED went bipolar, so a preset now has to carry a MINUS SIGN through
    // the text format and back. "%.6g" writes one; the parser has to read one.
    // If it did not, a reversed patch would come back playing forwards and the
    // failure would look like the reverse feature not working at all.
    Rig a = makeRig();
    a.bank.setValue(a.bank.find(gid::kSpeed), -2.5f);
    a.bank.setValue(a.bank.find(gid::kEvolve), 0.4f);
    std::string text = serializePreset(capturePreset(a.bank, a.sel, a.chain));
    CHECK(text.find("p g.speed -2.5") != std::string::npos);

    PresetData back;
    CHECK(parsePreset(text, back, nullptr));
    Rig b = makeRig();
    applyPresetParams(back, b.bank, nullptr);
    CHECK_NEAR(b.bank.valueOf(gid::kSpeed, 0.0f), -2.5f, 1e-4f);
    CHECK_NEAR(b.bank.valueOf(gid::kEvolve, 0.0f), 0.4f, 1e-4f);
}

void testFrozenIsSavedAsFrozen() {
    // Zero is the one value on this control that has to survive EXACTLY: a
    // patch saved held still must not come back creeping.
    Rig a = makeRig();
    a.bank.setValue(a.bank.find(gid::kSpeed), 0.0f);
    PresetData p = capturePreset(a.bank, a.sel, a.chain);
    PresetData back;
    CHECK(parsePreset(serializePreset(p), back, nullptr));
    Rig b = makeRig();
    applyPresetParams(back, b.bank, nullptr);
    CHECK(b.bank.valueOf(gid::kSpeed, 1.0f) == 0.0f);
}

void testPresetWithoutEvolveComesUpMoving() {
    // The whole point of the default. A preset written before EVOLVE existed
    // has no line for it; rule 3 says it keeps the default, and the default is
    // 1 -- so an old patch loads MOVING, which is what was asked for.
    Rig b = makeRig();
    PresetData p;
    p.version = 1;
    p.params.emplace_back(gid::kDetail, 200.0f);
    applyPresetParams(p, b.bank, nullptr);
    CHECK_NEAR(b.bank.valueOf(gid::kEvolve, -1.0f), 1.0f, 1e-5f);
    // ...and a preset written before SPEED went bipolar stores a real number
    // the new range still represents, unchanged and still forwards.
    PresetData old;
    old.version = 1;
    old.params.emplace_back(gid::kSpeed, 2.0f);
    applyPresetParams(old, b.bank, nullptr);
    CHECK_NEAR(b.bank.valueOf(gid::kSpeed, 0.0f), 2.0f, 1e-4f);
}

void testV1MorphNamespaceMigrates() {
    // V1 called the morph target b.k.*. V2 reserves b.* for Channel B, so a
    // versioned load must preserve the old meaning without creating a silent
    // collision with the new channel namespace.
    Rig r = makeRig();
    rebuildPatchParams(r.bank, kernelById("julia"), &kernelById("burningjulia"), r.chain);

    PresetData old;
    old.version = 1;
    old.params.emplace_back("b.k.cre", 0.75f);
    int unknown = 0;
    CHECK_EQ(applyPresetParams(old, r.bank, &unknown), 1);
    CHECK_EQ(unknown, 0);
    CHECK_NEAR(r.bank.valueOf("a.mk.k.cre", 0.0f), 0.75f, 1e-5f);
    CHECK(r.bank.find("b.k.cre") < 0);
}

void testSlotPathIsStable() {
    CHECK(presetPath("/tmp/x", 3) == "/tmp/x/preset03.nvp");
}

}  // namespace

int main() {
    testRoundTrip();
    testUnknownKeysAreSkipped();
    testMissingKeysKeepDefaults();
    testIdsNotIndices();
    testGeneratorAndPaletteByName();
    testMorphTargetSurvives();
    testUnknownGeneratorDoesNotWipeEverything();
    testUnknownEffectIsDropped();
    testRejectsNonPreset();
    testCommentsAndBlankLines();
    testPaddingIsNotWritten();
    testValuesAreRealUnitsNotNormalized();
    testNegativeSpeedSurvivesTheFile();
    testFrozenIsSavedAsFrozen();
    testPresetWithoutEvolveComesUpMoving();
    testV1MorphNamespaceMigrates();
    testSlotPathIsStable();
    return check::finish("preset");
}

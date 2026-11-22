// Slew, LFOs and macros.
//
// These three are exactly the kind of thing eyes do not catch. A slew that is
// subtly frame-rate dependent looks fine on this Mac at 120 Hz and feels
// "laggy" on the Pi at 60 with no way to say why. A triangle that is actually
// a sawtooth is a modulation you would describe as "a bit odd". A macro that
// fires every frame instead of on change looks perfect until you try to move
// one of the parameters it touches and cannot.
//
// So all of it is pure logic with no GL, no socket and no clock, and it is
// checked by SIMULATION -- stepping a real number of frames at real dt values
// -- rather than by reading a formula back to itself.
#include <cmath>
#include <string>
#include <vector>

#include "check.h"
#include "control/mapping.h"
#include "mod.h"
#include "preset.h"
#include "state.h"

using namespace nv;

namespace {

// A minimal bank: one plain linear knob, plus one that must never interpolate.
ParamBank makeBank() {
    ParamBank b;
    b.add({"a", "A", "test", 0.0f, 1.0f, 0.0f, Curve::Linear});
    b.add({"c", "C", "test", 0.0f, 1.0f, 0.0f, Curve::Linear});
    ParamDef snap{"sel", "SEL", "test", 0.0f, 4.0f, 0.0f, Curve::Linear};
    snap.slew = 0.0f;
    b.add(snap);
    b.markGlobalEnd();
    return b;
}

// Runs exactly `steps` frames of `dt` and returns how far the parameter got.
//
// Steps and dt rather than "seconds at a frame rate": the first version of
// this helper rounded seconds*fps to a whole number of frames, which quietly
// simulated 0.267 s at 30 fps against 0.25 s at 120 and made a
// frame-rate-INDEPENDENT slew look dependent. The test has to control the
// elapsed time exactly if it wants to say anything about it.
float advance(ParamBank& b, int steps, float dt) {
    for (int i = 0; i < steps; ++i) b.update(dt);
    return b.norm(0);
}

void testSlewOffIsInstant() {
    ParamBank b = makeBank();
    // A bank nobody has set a glide on does not glide. This is what keeps
    // every other suite -- and any test written later -- reading back exactly
    // what it wrote, and it is why GLIDE has to be pushed in from the frame
    // loop rather than defaulted here.
    CHECK_EQ(b.glideSeconds(), 0.0f);
    b.setNorm(0, 0.8f);
    CHECK_NEAR(b.norm(0), 0.8f, 1e-6f);
    CHECK_NEAR(b.value(0), 0.8f, 1e-6f);
}

void testSlewMovesThroughTheMiddle() {
    ParamBank b = makeBank();
    b.setGlideSeconds(0.4f);
    b.setNorm(0, 1.0f);

    // The TARGET is immediate -- a fader knows where it was put the instant it
    // was put there. The position is not.
    CHECK_NEAR(b.target(0), 1.0f, 1e-6f);
    CHECK_NEAR(b.norm(0), 0.0f, 1e-6f);

    // One frame in, it has moved and it has not arrived. This single check is
    // the whole feature: without slew both of these would be 1.0.
    b.update(1.0f / 60.0f);
    CHECK(b.norm(0) > 0.0f);
    CHECK(b.norm(0) < 1.0f);

    // Monotone, never overshooting, all the way there.
    float prev = b.norm(0);
    for (int i = 0; i < 120; ++i) {
        b.update(1.0f / 60.0f);
        CHECK(b.norm(0) >= prev - 1e-6f);
        CHECK(b.norm(0) <= 1.0f + 1e-6f);
        prev = b.norm(0);
    }
    // And it LANDS. An exponential is asymptotic; a preset that stopped a
    // millionth short forever would not be the preset that was saved.
    CHECK_EQ(b.norm(0), b.target(0));
}

void testSlewIsFrameRateIndependent() {
    // The bug this exists to catch: a coefficient of dt/tau (or worse, a
    // constant per frame) makes the glide take a different amount of TIME on
    // a 60 Hz panel than on a 120 Hz one. It is invisible on the machine you
    // developed on and it is the whole feel of the instrument on the other.
    // 8 frames of 1/32 s and 32 frames of 1/128 s are both exactly 0.25 s, and
    // both step sizes are exact in binary so the comparison is about the slew
    // and not about float noise.
    float slow, fast, oneStep;
    {
        ParamBank b = makeBank();
        b.setGlideSeconds(0.5f);
        b.setNorm(0, 1.0f);
        slow = advance(b, 8, 1.0f / 32.0f);
    }
    {
        ParamBank b = makeBank();
        b.setGlideSeconds(0.5f);
        b.setNorm(0, 1.0f);
        fast = advance(b, 32, 1.0f / 128.0f);
    }
    {
        // ...and so is ONE frame of the whole 0.25 s. Composing
        // 1 - exp(-dt/tau) over n steps is exactly 1 - exp(-n*dt/tau), so all
        // three have to agree to float precision -- not merely "closely".
        // The tolerance here is deliberately tight: a loose one would pass for
        // a linear coefficient too, which is the exact bug being excluded.
        ParamBank b = makeBank();
        b.setGlideSeconds(0.5f);
        b.setNorm(0, 1.0f);
        oneStep = advance(b, 1, 0.25f);
    }
    CHECK_NEAR(slow, fast, 1e-4f);
    CHECK_NEAR(slow, oneStep, 1e-4f);

    // Sanity: half a second of glide, sampled a quarter of the way in, is
    // genuinely partway -- not "basically done" and not "barely started".
    CHECK(slow > 0.5f);
    CHECK(slow < 0.99f);
}

void testSlewSurvivesAStalledFrame() {
    // This engine really does drop the odd ~290 ms frame (see the README's
    // note on async compilation). With a linear dt/tau coefficient a 290 ms
    // frame against a 100 ms glide gives a coefficient of 2.9: the parameter
    // shoots to nearly twice its target and then oscillates about it forever.
    // 1 - exp(-dt/tau) cannot exceed 1 whatever dt is.
    ParamBank b = makeBank();
    b.setGlideSeconds(0.1f);
    b.setNorm(0, 1.0f);
    b.update(0.29f);
    CHECK(b.norm(0) <= 1.0f);
    CHECK(b.norm(0) > 0.9f);   // a stall should ARRIVE, not stall the glide too

    // And in the other direction, where an overshoot would go negative.
    b.setNorm(0, 0.0f);
    b.update(5.0f);
    CHECK(b.norm(0) >= 0.0f);
    CHECK_EQ(b.norm(0), 0.0f);
}

void testPerParameterOptOut() {
    ParamBank b = makeBank();
    b.setGlideSeconds(2.0f);
    // The selector. Interpolating it would not soften the change, it would
    // sweep through every value between the two for two seconds.
    int sel = b.find("sel");
    b.setValue(sel, 3.0f);
    CHECK_NEAR(b.value(sel), 3.0f, 1e-5f);
    // ...while its neighbour, on the same bank and the same GLIDE, does glide.
    b.setNorm(0, 1.0f);
    CHECK(b.norm(0) < 0.1f);
}

void testGestureIsImmediate() {
    // What a finger on a slider does. A drag is already sixty positions a
    // second from a hand that is itself the smoother.
    ParamBank b = makeBank();
    b.setGlideSeconds(1.0f);
    b.setNormNow(0, 0.7f);
    CHECK_NEAR(b.norm(0), 0.7f, 1e-6f);
    CHECK_NEAR(b.target(0), 0.7f, 1e-6f);
}

void testRelativeMovesAccumulateOnTheTarget() {
    // An encoder mid-glide: each detent must be worth the same amount. If
    // nudge worked off the CURRENT position, a fast twist under a long glide
    // would give less travel per detent the faster you turned it.
    ParamBank b = makeBank();
    b.setGlideSeconds(1.0f);
    for (int i = 0; i < 5; ++i) b.nudgeNorm(0, 0.1f);
    CHECK_NEAR(b.target(0), 0.5f, 1e-5f);
}

void testGlideScopeRestores() {
    ParamBank b = makeBank();
    b.setGlideSeconds(0.08f);
    {
        GlideScope g(b, b.presetGlideSeconds());
        CHECK_NEAR(b.glideSeconds(), 0.64f, 1e-5f);
        b.setNorm(0, 1.0f);
    }
    CHECK_NEAR(b.glideSeconds(), 0.08f, 1e-5f);
    // The move keeps the rate it was STARTED with. A recall's slow flight has
    // to stay slow for its whole duration even though the live rate is
    // reapplied every frame after it.
    advance(b, 12, 1.0f / 60.0f);
    CHECK(b.norm(0) < 0.9f);
}

void testPresetLandsExactly() {
    // The contract from the README: values are stored in real units and a
    // preset must land where it was saved. Slew is allowed to take its time
    // getting there; it is not allowed to arrive somewhere else.
    ParamBank b;
    b.add({"z", "Z", "test", 1e-4f, 1e2f, 1.0f, Curve::Exp});
    b.markGlobalEnd();
    b.setGlideSeconds(0.64f);
    b.setValue(0, 37.5f);
    CHECK(std::fabs(b.value(0) - 37.5f) > 1.0f);   // still on its way
    advance(b, 300, 1.0f / 60.0f);
    CHECK_NEAR(b.value(0), 37.5f, 0.01f);
    CHECK_EQ(b.norm(0), b.target(0));
}

// ---- LFO shapes ---------------------------------------------------------

void testShapes() {
    const float e = 1e-4f;
    CHECK_NEAR(lfoShapeValue(LfoShape::Sine, 0.0f, 0, 0), 0.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Sine, 0.25f, 0, 0), 1.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Sine, 0.5f, 0, 0), 0.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Sine, 0.75f, 0, 0), -1.0f, e);

    // Triangle: rises through zero at 0 like the sine, peaks at a quarter.
    // The bug this catches is a triangle that is really a sawtooth, or one
    // that starts at -1 and therefore jumps when you switch shape.
    CHECK_NEAR(lfoShapeValue(LfoShape::Triangle, 0.0f, 0, 0), 0.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Triangle, 0.25f, 0, 0), 1.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Triangle, 0.5f, 0, 0), 0.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Triangle, 0.75f, 0, 0), -1.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Triangle, 0.999f, 0, 0), 0.0f, 0.01f);

    CHECK_NEAR(lfoShapeValue(LfoShape::Ramp, 0.0f, 0, 0), -1.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Ramp, 0.5f, 0, 0), 0.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Ramp, 0.999f, 0, 0), 1.0f, 0.01f);

    CHECK_NEAR(lfoShapeValue(LfoShape::Square, 0.1f, 0, 0), 1.0f, e);
    CHECK_NEAR(lfoShapeValue(LfoShape::Square, 0.9f, 0, 0), -1.0f, e);

    // Every shape stays inside ±1 across a full cycle. Depth is in normalized
    // units, so a shape that peaked at 1.4 would silently make DEPTH mean
    // something different for that one waveform.
    for (int s = 0; s < static_cast<int>(LfoShape::kCount); ++s) {
        for (int i = 0; i <= 200; ++i) {
            float v = lfoShapeValue(static_cast<LfoShape>(s), i / 200.0f,
                                    static_cast<unsigned>(i), 3);
            CHECK(v >= -1.0f && v <= 1.0f);
        }
    }

    // Phase wraps rather than clamps: a phase OFFSET routinely pushes past 1
    // and "1.25" has to mean a quarter into the next cycle.
    CHECK_NEAR(lfoShapeValue(LfoShape::Sine, 1.25f, 0, 0),
               lfoShapeValue(LfoShape::Sine, 0.25f, 0, 0), e);
}

void testRandomHold() {
    // Held for the whole cycle...
    float a = lfoShapeValue(LfoShape::RandomHold, 0.0f, 7, 1);
    float b = lfoShapeValue(LfoShape::RandomHold, 0.99f, 7, 1);
    CHECK_EQ(a, b);
    // ...and different in the next one.
    CHECK(lfoShapeValue(LfoShape::RandomHold, 0.0f, 8, 1) != a);
    // Two LFOs at the same rate must not hold the identical sequence, or four
    // modulators read as one.
    CHECK(lfoShapeValue(LfoShape::RandomHold, 0.0f, 7, 2) != a);
    // Deterministic across runs and machines: it is a hash of the cycle
    // number, not a call to rand().
    CHECK_EQ(lfoShapeValue(LfoShape::RandomHold, 0.4f, 7, 1), a);
}

// ---- LFOs against a real bank -------------------------------------------

ParamBank modBank() {
    ParamBank b;
    b.add({"a", "A", "test", 0.0f, 1.0f, 0.5f, Curve::Linear});
    b.add({"c", "C", "test", 0.0f, 1.0f, 0.0f, Curve::Linear});
    registerModParams(b);
    b.markGlobalEnd();
    return b;
}

void setP(ParamBank& b, const std::string& id, float v) {
    int i = b.find(id);
    CHECK(i >= 0);
    if (i >= 0) b.setValueNow(i, v);
}

// An LFO's routing lives in the mapping table now, exactly like a macro's, so
// every test that used to poke `lfo.target` sets up a table instead. This is
// the same call the device's LEARN gesture makes.
MappingTable pointedAt(int slot, const std::string& id) {
    MappingTable t;
    CHECK(assignLfoTarget(t, slot, id) == BindResult::Assigned);
    return t;
}

void testLfoMovesAParameter() {
    ParamBank b = modBank();
    LfoBank lfos;
    MappingTable table = pointedAt(0, "a");
    setP(b, lfoParamId(0, "rate"), 1.0f);     // 1 Hz
    setP(b, lfoParamId(0, "depth"), 0.4f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Sine)));

    int a = b.find("a");
    float lo = 2.0f, hi = -2.0f;
    // One second at 60 fps is exactly one cycle at 1 Hz.
    for (int i = 0; i < 60; ++i) {
        updateLfos(lfos, table, b, 1.0f / 60.0f);
        b.update(1.0f / 60.0f);
        lo = std::min(lo, b.value(a));
        hi = std::max(hi, b.value(a));
    }
    // Base 0.5, depth 0.4 -> the excursion is 0.1 .. 0.9.
    CHECK_NEAR(lo, 0.1f, 0.02f);
    CHECK_NEAR(hi, 0.9f, 0.02f);
    // The knob is NOT dead: nothing about the modulation moved where the
    // control is set. This is the difference between adding an offset and
    // overwriting the value, and overwriting is the version that makes a
    // performer think the instrument is broken.
    CHECK_NEAR(b.target(a), 0.5f, 1e-5f);
}

void testLfoIsAnOffsetNotAnOverride() {
    ParamBank b = modBank();
    LfoBank lfos;
    MappingTable table = pointedAt(0, "a");
    setP(b, lfoParamId(0, "rate"), 0.0f);      // parked at phase 0
    setP(b, lfoParamId(0, "depth"), 0.25f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Square)));  // +1 at 0

    int a = b.find("a");
    updateLfos(lfos, table, b, 0.0f);
    CHECK_NEAR(b.value(a), 0.75f, 1e-4f);      // 0.5 + 0.25

    // Move the base; the whole excursion moves with it.
    b.setNormNow(a, 0.2f);
    updateLfos(lfos, table, b, 0.0f);
    CHECK_NEAR(b.value(a), 0.45f, 1e-4f);

    // And the offset is clamped into the parameter's range, not wrapped.
    b.setNormNow(a, 0.9f);
    updateLfos(lfos, table, b, 0.0f);
    CHECK_NEAR(b.value(a), 1.0f, 1e-4f);
}

void testLfoIsFreeRunningAndSingleWriter() {
    ParamBank b = modBank();
    LfoBank lfos;
    MappingTable table = pointedAt(0, "a");
    setP(b, lfoParamId(0, "rate"), 2.0f);
    setP(b, lfoParamId(0, "depth"), 0.0f);     // silent, but running

    // 60 frames of 1/60 s at 2 Hz is two cycles. Deliberately not the 30
    // frames that would land EXACTLY on the first wrap: 30 accumulations of
    // 2*(1/60) in float come to a hair under 1.0, so that version of this test
    // was asserting on a knife edge and failing for a reason that had nothing
    // to do with whether the LFO runs. The wrap itself is pinned below, with a
    // dt that is exact in binary.
    for (int i = 0; i < 60; ++i) updateLfos(lfos, table, b, 1.0f / 60.0f);
    CHECK(lfos.lfo[0].cycle >= 1u);

    // Turning depth up picks the shape up where it would have been.
    setP(b, lfoParamId(0, "depth"), 0.5f);
    updateLfos(lfos, table, b, 1.0f / 60.0f);
    int a = b.find("a");
    CHECK(std::fabs(b.mod(a)) > 0.0f);

    // Un-assigning must not leave the last offset stuck on the parameter --
    // updateLfos is the single writer of modulation and clears first.
    CHECK(assignLfoTarget(table, 0, "") == BindResult::Cleared);
    updateLfos(lfos, table, b, 1.0f / 60.0f);
    CHECK_EQ(b.mod(a), 0.0f);
}

void testPhaseWrapCountsCycles() {
    // The wrap on its own, driven with exactly-representable numbers so the
    // arithmetic is exact and the assertion is about the LOGIC: 1 Hz stepped
    // in quarter seconds must land on phase 0 of cycle 1 after four frames,
    // not on cycle 0 (missing the wrap) or cycle 2 (counting it twice).
    ParamBank b = modBank();
    LfoBank lfos;
    MappingTable table;
    setP(b, lfoParamId(0, "rate"), 1.0f);
    for (int i = 0; i < 4; ++i) updateLfos(lfos, table, b, 0.25f);
    CHECK_EQ(lfos.lfo[0].cycle, 1u);
    CHECK_NEAR(lfos.lfo[0].phase, 0.0f, 1e-6f);

    // A frame long enough to span several cycles counts all of them -- the
    // engine drops the odd 290 ms frame and a fast LFO must not lose the
    // cycles that happened during it.
    updateLfos(lfos, table, b, 3.5f);
    CHECK_EQ(lfos.lfo[0].cycle, 4u);
    CHECK_NEAR(lfos.lfo[0].phase, 0.5f, 1e-5f);
}

void testLfoOnAMissingTargetIsInert() {
    ParamBank b = modBank();
    LfoBank lfos;
    // An effect knob that is not in the chain right now. Same rule as a
    // mapping and as a preset's unknown keys: inert, not an error.
    MappingTable table = pointedAt(0, "fx7.amount");
    setP(b, lfoParamId(0, "rate"), 1.0f);
    setP(b, lfoParamId(0, "depth"), 1.0f);
    for (int i = 0; i < 10; ++i) updateLfos(lfos, table, b, 1.0f / 60.0f);
    CHECK_EQ(b.mod(b.find("a")), 0.0f);
}

void testShapeParameterSnaps() {
    // The shape knob is the reason per-parameter slew exists. Under a two
    // second GLIDE it must still be the new shape on the very next frame.
    ParamBank b = modBank();
    b.setGlideSeconds(2.0f);
    int s = b.find(lfoParamId(0, "shape"));
    b.setValue(s, static_cast<float>(static_cast<int>(LfoShape::Square)));
    CHECK_NEAR(b.value(s), static_cast<float>(static_cast<int>(LfoShape::Square)),
               1e-4f);
}

// ---- macros -------------------------------------------------------------

void testMacroFansOutThroughTheMappingTable() {
    // A macro is not a second routing system. It is a SOURCE fed into the
    // table that already does many-to-one, ranges and polarity -- so this test
    // is really checking that no second mechanism was needed.
    ParamBank b = modBank();
    MappingTable table;
    std::string err;
    CHECK(table.loadFromString(
        "macro:0 a abs 0.0 1.0\n"
        "macro:0 c abs 1.0 0.0\n",   // lo > hi: the same macro, inverted
        &err));

    MacroBank macros;
    int a = b.find("a"), c = b.find("c");
    setP(b, macroParamId(0), 0.25f);
    applyMacros(macros, table, b);
    CHECK_NEAR(b.target(a), 0.25f, 1e-5f);
    CHECK_NEAR(b.target(c), 0.75f, 1e-5f);

    setP(b, macroParamId(0), 1.0f);
    applyMacros(macros, table, b);
    CHECK_NEAR(b.target(a), 1.0f, 1e-5f);
    CHECK_NEAR(b.target(c), 0.0f, 1e-5f);
}

void testMacroOnlyFiresWhenItMoves() {
    // The failure this guards against is silent and total: a macro pushed
    // every frame pins everything it names to itself, and no fader, preset or
    // sensor can ever move one of those parameters again.
    ParamBank b = modBank();
    MappingTable table;
    CHECK(table.loadFromString("macro:0 a abs 0.0 1.0\n"));

    MacroBank macros;
    setP(b, macroParamId(0), 0.3f);
    applyMacros(macros, table, b);
    CHECK_NEAR(b.target(b.find("a")), 0.3f, 1e-5f);

    // Somebody else moves that parameter. The macro has not been touched.
    b.setNormNow(b.find("a"), 0.9f);
    CHECK_EQ(applyMacros(macros, table, b), 0);
    CHECK_NEAR(b.target(b.find("a")), 0.9f, 1e-5f);
}

void testMacroTakesEffectOnceWithoutBeingTouched() {
    // A preset restores a macro's position. It has to fan out that position
    // once, or the patch would be wrong until somebody wiggled the knob.
    ParamBank b = modBank();
    MappingTable table;
    CHECK(table.loadFromString("macro:0 a abs 0.0 1.0\n"));
    setP(b, macroParamId(0), 0.6f);
    MacroBank macros;                       // fresh, as after a load
    CHECK_EQ(applyMacros(macros, table, b), 1);
    CHECK_NEAR(b.target(b.find("a")), 0.6f, 1e-5f);
}

void testMacroCanBeModulated() {
    // One LFO, one macro, eight parameters -- with no machinery for that case
    // at all. It works because the macro reads its MODULATED position.
    ParamBank b = modBank();
    MappingTable table;
    CHECK(table.loadFromString("macro:0 a abs 0.0 1.0\n"));
    LfoBank lfos;
    CHECK(assignLfoTarget(table, 0, macroParamId(0)) == BindResult::Assigned);
    setP(b, lfoParamId(0, "rate"), 0.0f);
    setP(b, lfoParamId(0, "depth"), 0.5f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Square)));
    setP(b, macroParamId(0), 0.2f);

    MacroBank macros;
    updateLfos(lfos, table, b, 0.0f);
    applyMacros(macros, table, b);
    CHECK_NEAR(b.target(b.find("a")), 0.7f, 1e-4f);   // 0.2 + 0.5
}

// ---- assigning a target, from the device --------------------------------

// The names an LFO drives, in table order.
std::vector<std::string> targetNames(const MappingTable& t, int slot) {
    std::vector<std::string> out;
    for (const Mapping& m : lfoTargets(t, slot)) out.push_back(m.param);
    return out;
}

void testAssignIsAbsoluteAndIdempotent() {
    MappingTable t;
    // The wire / env form: absolute. A datagram can arrive twice and the
    // second one must not undo the first -- and, now that the table can hold
    // several rows per LFO, must not STACK a second identical one either.
    CHECK(assignLfoTarget(t, 0, "g.detail") == BindResult::Assigned);
    CHECK(assignLfoTarget(t, 0, "g.detail") == BindResult::Assigned);
    CHECK_EQ(t.countFor(lfoSource(0)), 1);
    CHECK(targetNames(t, 0) == std::vector<std::string>{"g.detail"});

    // Absolute means EXACTLY this: a second assign replaces the fan-out
    // rather than adding to it, because a surface saying "L1 drives zoom" is
    // stating the routing, not appending to it.
    CHECK(learnLfoTarget(t, 0, "g.zoom") == BindResult::Assigned);
    CHECK_EQ(t.countFor(lfoSource(0)), 2);
    CHECK(assignLfoTarget(t, 0, "g.morph") == BindResult::Assigned);
    CHECK(targetNames(t, 0) == std::vector<std::string>{"g.morph"});

    // ...and an empty id is how it is turned off.
    CHECK(assignLfoTarget(t, 0, "") == BindResult::Cleared);
    CHECK_EQ(t.countFor(lfoSource(0)), 0);
}

void testLearnAddsAndIsItsOwnUndoPerDestination() {
    MappingTable t;
    // The GESTURE form. Each touch ADDS a destination, which is the only way
    // fan-out is reachable from a device with no keyboard.
    CHECK(learnLfoTarget(t, 1, "g.zoom") == BindResult::Assigned);
    CHECK(learnLfoTarget(t, 1, "g.morph") == BindResult::Assigned);
    CHECK(learnLfoTarget(t, 1, "g.detail") == BindResult::Assigned);
    CHECK(targetNames(t, 1) ==
          std::vector<std::string>({"g.zoom", "g.morph", "g.detail"}));

    // Pointing it at one it ALREADY drives removes THAT destination and
    // leaves the others alone. This is why there is still no separate
    // unassign control, and it is the only way to take one knob back out of
    // three without retyping the other two.
    CHECK(learnLfoTarget(t, 1, "g.morph") == BindResult::Cleared);
    CHECK(targetNames(t, 1) ==
          std::vector<std::string>({"g.zoom", "g.detail"}));

    // Removing them one at a time ends at nothing, with no special case for
    // the last one.
    CHECK(learnLfoTarget(t, 1, "g.zoom") == BindResult::Cleared);
    CHECK(learnLfoTarget(t, 1, "g.detail") == BindResult::Cleared);
    CHECK_EQ(t.countFor(lfoSource(1)), 0);

    // Slots do not leak into each other: two LFOs on the same parameter are
    // two rows, and undoing one leaves the other running.
    CHECK(learnLfoTarget(t, 0, "g.zoom") == BindResult::Assigned);
    CHECK(learnLfoTarget(t, 2, "g.zoom") == BindResult::Assigned);
    CHECK(learnLfoTarget(t, 0, "g.zoom") == BindResult::Cleared);
    CHECK_EQ(t.countFor(lfoSource(0)), 0);
    CHECK_EQ(t.countFor(lfoSource(2)), 1);
}

void testOneLfoFansOutWithItsOwnRangePerDestination() {
    // The payoff of putting LFOs in the mapping table: one modulator, three
    // parameters, a different amount and a different DIRECTION on each. This
    // is the case the old one-target-plus-depth model could not express at
    // all, and it needed no new machinery -- lo/hi already meant this.
    ParamBank b = modBank();
    b.addDynamic({"d", "D", "test", 0.0f, 1.0f, 0.5f, Curve::Linear});
    b.endRebuild();

    MappingTable t;
    std::string err;
    CHECK(t.loadFromString(
        "lfo:0 a 0.0 1.0\n"      // all of it, upward
        "lfo:0 c 0.0 0.25\n"     // a quarter of it, upward
        "lfo:0 d 0.4 0.0\n",     // 40%, INVERTED
        &err));

    LfoBank lfos;
    setP(b, lfoParamId(0, "rate"), 0.0f);     // parked at phase 0
    setP(b, lfoParamId(0, "depth"), 1.0f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Square)));   // +1 at 0

    updateLfos(lfos, t, b, 0.0f);
    CHECK_NEAR(b.mod(b.find("a")), 1.0f, 1e-5f);
    CHECK_NEAR(b.mod(b.find("c")), 0.25f, 1e-5f);
    CHECK_NEAR(b.mod(b.find("d")), -0.4f, 1e-5f);

    // Depth scales all three together -- it is the modulator's own level, and
    // the row's span is how much of it that destination gets.
    setP(b, lfoParamId(0, "depth"), 0.5f);
    updateLfos(lfos, t, b, 0.0f);
    CHECK_NEAR(b.mod(b.find("a")), 0.5f, 1e-5f);
    CHECK_NEAR(b.mod(b.find("c")), 0.125f, 1e-5f);
    CHECK_NEAR(b.mod(b.find("d")), -0.2f, 1e-5f);

    // And none of it moved where the user put any of the three faders. This
    // is the whole reason a modulator row ADDS instead of SETS.
    CHECK_NEAR(b.target(b.find("a")), 0.5f, 1e-5f);
    CHECK_NEAR(b.target(b.find("c")), 0.0f, 1e-5f);
    CHECK_NEAR(b.target(b.find("d")), 0.5f, 1e-5f);
}

void testAModulatorNeverSetsAndAControlNeverAdds() {
    // The crux, pinned from both sides. A row's character is read off its
    // source, and neither call can be talked into the other's job.
    ParamBank b = modBank();
    MappingTable t;
    CHECK(t.loadFromString("lfo:0 a 0 1\nsensor:x c abs\n"));

    int a = b.find("a"), c = b.find("c");
    b.setNormNow(a, 0.5f);

    // apply() -- the EVENT path -- refuses to set through a modulator row.
    // Nothing on the wire can name one, and this is what makes that a
    // property of the table rather than of who called it.
    CHECK(!t.apply(lfoSource(0), 1.0f, b));
    CHECK_NEAR(b.target(a), 0.5f, 1e-5f);
    CHECK_EQ(b.mod(a), 0.0f);

    // applyMod() -- the FRAME path -- refuses to add through a control row.
    // Adding one every frame would ratchet a parameter away and never bring
    // it back.
    // Hoisted, because CHECK_EQ evaluates its arguments twice and these calls
    // have a side effect -- a lesson this file has now learned the hard way.
    int addedThroughAControl = t.applyMod("sensor:x", 1.0f, b);
    CHECK_EQ(addedThroughAControl, 0);
    CHECK_EQ(b.mod(c), 0.0f);

    // Each through its own door works.
    CHECK(t.apply("sensor:x", 0.75f, b));
    CHECK_NEAR(b.target(c), 0.75f, 1e-5f);
    int addedThroughAModulator = t.applyMod(lfoSource(0), 0.25f, b);
    CHECK_EQ(addedThroughAModulator, 1);
    CHECK_NEAR(b.mod(a), 0.25f, 1e-5f);
    CHECK_NEAR(b.target(a), 0.5f, 1e-5f);   // still exactly where it was put

    // `rel` on a modulator is refused at the LINE. A level counted as detents
    // would walk the parameter away and never bring it back.
    MappingTable bad;
    std::string err;
    CHECK(!bad.loadFromString("lfo:0 a rel 0 1 0.02\n", &err));
    CHECK(!err.empty());
}

void testAssignRejectsSelfAndBadSlots() {
    MappingTable lfos;
    // Its own knobs are refused: on the MOD page an LFO's DEPTH slider is
    // inches from its own TARGET button, so this is the easiest gesture to
    // fire by accident, and an LFO swinging its own depth through zero looks
    // exactly like one that has stopped working.
    CHECK(lfoTargetRejected(0, lfoParamId(0, "depth")));
    CHECK(assignLfoTarget(lfos, 0, lfoParamId(0, "rate")) == BindResult::Rejected);
    CHECK_EQ(lfos.countFor(lfoSource(0)), 0);

    // ANOTHER LFO is fair game -- cross-modulation is the reason to have four.
    CHECK(!lfoTargetRejected(0, lfoParamId(1, "rate")));
    CHECK(assignLfoTarget(lfos, 0, lfoParamId(1, "rate")) == BindResult::Assigned);
    // And a macro, which is how one LFO drives eight parameters.
    CHECK(assignLfoTarget(lfos, 2, macroParamId(0)) == BindResult::Assigned);

    // Slots that do not exist change nothing at all -- never clamped onto
    // slot 0, the same rule the preset parser follows.
    CHECK(assignLfoTarget(lfos, -1, "g.zoom") == BindResult::Rejected);
    CHECK(assignLfoTarget(lfos, kLfoCount, "g.zoom") == BindResult::Rejected);
    CHECK(learnLfoTarget(lfos, 99, "g.zoom") == BindResult::Rejected);
    for (int n = 0; n < kLfoCount; ++n) {
        CHECK(lfos.findRow(lfoSource(n), "g.zoom") < 0);
    }
}

void testAssignDoesNotRestartTheLfo() {
    // Re-pointing is a patch edit, not a trigger. If assignment reset the
    // phase, an LFO would become an envelope the moment you moved it -- and
    // two LFOs assigned in the same gesture would lock together.
    ParamBank b = modBank();
    LfoBank lfos;
    MappingTable table;
    setP(b, lfoParamId(0, "rate"), 1.0f);
    for (int i = 0; i < 4; ++i) updateLfos(lfos, table, b, 0.1f);
    float phase = lfos.lfo[0].phase;
    CHECK(phase > 0.3f);
    assignLfoTarget(table, 0, "a");
    CHECK_EQ(lfos.lfo[0].phase, phase);
}

void testLearnArmsCancelsAndExpires() {
    ModLearn learn;
    CHECK(!learn.armed());

    learn.arm(2, 100.0);
    CHECK(learn.armed());
    CHECK_EQ(learn.slot(), 2);

    // The button is its own cancel: pressing the armed one disarms.
    learn.toggle(2, 100.5);
    CHECK(!learn.armed());
    // ...and pressing a different one moves the arming rather than stacking.
    learn.toggle(1, 101.0);
    CHECK_EQ(learn.slot(), 1);
    learn.toggle(3, 101.5);
    CHECK_EQ(learn.slot(), 3);

    learn.cancel();
    CHECK(!learn.armed());

    // It expires. An armed learn CONSUMES the next parameter you touch, so
    // one left running behind your back would eat a slider you meant to drag.
    learn.arm(0, 200.0);
    learn.tick(200.0 + ModLearn::kTimeout * 0.5);
    CHECK(learn.armed());
    CHECK(learn.secondsLeft(200.0 + ModLearn::kTimeout * 0.5) > 0.0f);
    learn.tick(200.0 + ModLearn::kTimeout + 0.01);
    CHECK(!learn.armed());
    CHECK_EQ(learn.secondsLeft(999.0), 0.0f);

    // Out-of-range arming is not armed at all, rather than armed on a slot
    // that does not exist.
    learn.arm(kLfoCount, 300.0);
    CHECK(!learn.armed());
}

void testTargetSurvivesTheBankLosingIt() {
    // The case that makes id-addressing necessary: point an LFO at a knob,
    // change the patch so that knob is gone, and the modulation must go quiet
    // WITHOUT the assignment being forgotten and without landing on whatever
    // parameter now occupies that index.
    ParamBank b = modBank();
    b.addDynamic({"fx0.amount", "AMOUNT", "grain", 0.0f, 1.0f, 0.5f,
                  Curve::Linear});
    b.endRebuild();

    LfoBank lfos;
    MappingTable table;
    CHECK(learnLfoTarget(table, 0, "fx0.amount") == BindResult::Assigned);
    setP(b, lfoParamId(0, "rate"), 0.0f);
    setP(b, lfoParamId(0, "depth"), 0.5f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Square)));   // +1 at 0

    updateLfos(lfos, table, b, 0.0f);
    CHECK_NEAR(b.mod(b.find("fx0.amount")), 0.5f, 1e-5f);

    // The effect leaves the chain, and something else takes its place at the
    // same index. THIS is the misrouting an index-addressed LFO would do.
    b.beginRebuild();
    b.addDynamic({"fx0.mix", "MIX", "bloom", 0.0f, 1.0f, 0.25f, Curve::Linear});
    b.endRebuild();
    CHECK_EQ(b.find("fx0.amount"), -1);

    updateLfos(lfos, table, b, 1.0f / 60.0f);
    CHECK_EQ(b.mod(b.find("fx0.mix")), 0.0f);      // not modulating the usurper
    CHECK_EQ(table.countFor(lfoSource(0)), 1);     // and it has not forgotten
    CHECK(table.all()[0].param == "fx0.amount");
    // Nothing crashed, and the whole bank is quiet.
    for (int i = 0; i < b.count(); ++i) CHECK_EQ(b.mod(i), 0.0f);

    // It comes back.
    b.beginRebuild();
    b.addDynamic({"fx0.amount", "AMOUNT", "grain", 0.0f, 1.0f, 0.5f,
                  Curve::Linear});
    b.endRebuild();
    updateLfos(lfos, table, b, 1.0f / 60.0f);
    CHECK_NEAR(b.mod(b.find("fx0.amount")), 0.5f, 1e-5f);
}

// ---- presets ------------------------------------------------------------

void testLfoTargetsRoundTrip() {
    PresetData p;
    p.version = 1;
    p.kernel = "mandelbrot";
    p.lfoRows.push_back({0, "g.palette_shift", 0.0f, 1.0f});
    p.lfoRows.push_back({0, "g.zoom", 0.3f, 0.0f});   // fan-out, inverted
    p.lfoRows.push_back({2, "fx0.amount", 0.0f, 0.5f});

    PresetData back;
    std::string err;
    CHECK(parsePreset(serializePreset(p), back, &err));
    CHECK_EQ(static_cast<int>(back.lfoRows.size()), 3);
    CHECK_EQ(back.lfoRows[0].slot, 0);
    CHECK(back.lfoRows[0].param == "g.palette_shift");
    // The SPAN round-trips, which is the whole reason the key grew two
    // numbers: a fan-out whose ranges came back as full depth on every
    // destination would be a different patch wearing the same name.
    CHECK_NEAR(back.lfoRows[1].lo, 0.3f, 1e-5f);
    CHECK_NEAR(back.lfoRows[1].hi, 0.0f, 1e-5f);
    CHECK_EQ(back.lfoRows[2].slot, 2);

    MappingTable table;
    CHECK(table.loadFromString(
        "lfo:1 g.zoom 0 1\n"          // left over from the previous patch
        "sensor:jx g.center_x abs\n"  // ...and the performer's own mapping
        "macro:0 g.detail abs\n"));
    applyPresetLfos(back, table);

    CHECK_EQ(table.countFor(lfoSource(0)), 2);
    CHECK_EQ(table.countFor(lfoSource(2)), 1);
    // Slots the file does not mention are CLEARED. A modulator still running
    // with nothing in the file to explain it is a ghost.
    CHECK_EQ(table.countFor(lfoSource(1)), 0);
    // ...but the RIG's rows are untouched. A preset recall changes the sound,
    // not the wiring between this instrument and the hardware in front of it,
    // and this single assertion is the whole ownership rule.
    CHECK(table.findRow("sensor:jx", "g.center_x") >= 0);
    CHECK(table.findRow("macro:0", "g.detail") >= 0);
}

void testAPresetCannotSmuggleInASelfModulation() {
    // A preset is a front door like any other, and a hand-edited one must get
    // the same refusal the gesture and the wire get.
    PresetData p;
    p.version = 1;
    p.lfoRows.push_back({0, lfoParamId(0, "depth"), 0.0f, 1.0f});
    p.lfoRows.push_back({0, lfoParamId(1, "rate"), 0.0f, 1.0f});
    MappingTable table;
    applyPresetLfos(p, table);
    CHECK_EQ(table.countFor(lfoSource(0)), 1);
    CHECK(table.all()[0].param == lfoParamId(1, "rate"));
}

void testAnOldPresetsLfoLineStillMeansFullDepth() {
    // Rule 3, applied INSIDE a line: a preset written before fan-out existed
    // has no span on its `lfo` key, and it has to keep meaning exactly what it
    // meant -- the whole travel.
    PresetData p;
    std::string err;
    CHECK(parsePreset("version 1\nlfo 0 g.detail\n", p, &err));
    CHECK_EQ(static_cast<int>(p.lfoRows.size()), 1);
    CHECK_NEAR(p.lfoRows[0].lo, 0.0f, 1e-6f);
    CHECK_NEAR(p.lfoRows[0].hi, 1.0f, 1e-6f);

    ParamBank b = modBank();
    MappingTable table;
    applyPresetLfos(p, table);
    LfoBank lfos;
    setP(b, lfoParamId(0, "rate"), 0.0f);
    setP(b, lfoParamId(0, "depth"), 0.6f);
    setP(b, lfoParamId(0, "shape"),
         static_cast<float>(static_cast<int>(LfoShape::Square)));
    b.addDynamic({"g.detail", "DETAIL", "global", 0.0f, 1.0f, 0.5f,
                  Curve::Linear});
    b.endRebuild();
    updateLfos(lfos, table, b, 0.0f);
    CHECK_NEAR(b.mod(b.find("g.detail")), 0.6f, 1e-5f);
}

void testLearnedTargetRoundTripsThroughAPreset() {
    // The whole point of making this assignable on the device: an assignment
    // made by hand has to be part of the patch, through the preset key that
    // already existed and unchanged by any of this.
    ParamBank b = modBank();
    MappingTable lfos;
    CHECK(learnLfoTarget(lfos, 0, "a") == BindResult::Assigned);
    CHECK(learnLfoTarget(lfos, 0, "c") == BindResult::Assigned);   // fan-out
    CHECK(learnLfoTarget(lfos, 3, macroParamId(1)) == BindResult::Assigned);
    // A range narrowed on the MAP page has to survive too, or every recall
    // would quietly restore a modulation at full depth.
    lfos.setRange(lfos.findRow(lfoSource(0), "c"), 0.2f, 0.0f);
    setP(b, lfoParamId(0, "depth"), 0.4f);

    Selection sel;
    std::vector<ChainSlot> chain;
    std::string text = serializePreset(capturePreset(b, sel, chain, &lfos));

    PresetData back;
    std::string err;
    CHECK(parsePreset(text, back, &err));
    MappingTable reloaded;
    CHECK(reloaded.loadFromString("lfo:2 g.zoom 0 1\n"));
    applyPresetLfos(back, reloaded);
    CHECK(targetNames(reloaded, 0) == std::vector<std::string>({"a", "c"}));
    CHECK(targetNames(reloaded, 3) ==
          std::vector<std::string>({macroParamId(1)}));
    CHECK_EQ(reloaded.countFor(lfoSource(2)), 0);
    int r = reloaded.findRow(lfoSource(0), "c");
    CHECK(r >= 0);
    CHECK_NEAR(reloaded.all()[r].lo, 0.2f, 1e-5f);
    CHECK_NEAR(reloaded.all()[r].hi, 0.0f, 1e-5f);

    // ...and the depth came back with it, through the ordinary parameter path.
    ParamBank b2 = modBank();
    applyPresetParams(back, b2, nullptr);
    CHECK_NEAR(b2.value(b2.find(lfoParamId(0, "depth"))), 0.4f, 1e-4f);

    // An LFO cleared by a second learn gesture leaves NO line behind: the
    // preset says it is off by not mentioning it.
    CHECK(learnLfoTarget(lfos, 0, "a") == BindResult::Cleared);
    CHECK(learnLfoTarget(lfos, 0, "c") == BindResult::Cleared);
    PresetData after = capturePreset(b, sel, chain, &lfos);
    for (const auto& l : after.lfoRows) CHECK(l.slot != 0);
}

void testPresetWithoutModStillLoads() {
    // The forward/backward compatibility rule, for the key this change adds:
    // a preset written before any of this existed has no `lfo` lines and no
    // lfo*/macro* parameters, and must load with those at their defaults.
    const char* old =
        "version 1\n"
        "mode SYNTH\n"
        "kernel burningship\n"
        "p g.detail 211\n";
    PresetData p;
    std::string err;
    CHECK(parsePreset(old, p, &err));
    CHECK_EQ(static_cast<int>(p.lfoRows.size()), 0);

    ParamBank b = modBank();
    int unknown = 0;
    applyPresetParams(p, b, &unknown);
    CHECK_NEAR(b.value(b.find(lfoParamId(0, "depth"))), 0.0f, 1e-5f);
    CHECK_NEAR(b.value(b.find(macroParamId(0))), 0.0f, 1e-5f);

    // ...and an `lfo` line naming a slot this build does not have is dropped
    // rather than clamped onto slot 0.
    PresetData future;
    CHECK(parsePreset("version 1\nlfo 99 g.zoom\n", future, &err));
    CHECK_EQ(static_cast<int>(future.lfoRows.size()), 0);
}

void testCaptureSavesTheTargetNotTheMomentaryValue() {
    // Saving mid-glide, with an LFO running, must record the patch -- not a
    // photograph of one instant of it. Otherwise the same patch saved twice a
    // second apart is two different presets.
    ParamBank b = modBank();
    b.setGlideSeconds(1.0f);
    int a = b.find("a");
    b.setNorm(a, 1.0f);
    b.addMod(a, -0.3f);
    b.update(1.0f / 60.0f);
    CHECK(std::fabs(b.value(a) - 1.0f) > 0.2f);      // nowhere near it
    CHECK_NEAR(b.targetValue(a), 1.0f, 1e-5f);
}

}  // namespace

int main() {
    testSlewOffIsInstant();
    testSlewMovesThroughTheMiddle();
    testSlewIsFrameRateIndependent();
    testSlewSurvivesAStalledFrame();
    testPerParameterOptOut();
    testGestureIsImmediate();
    testRelativeMovesAccumulateOnTheTarget();
    testGlideScopeRestores();
    testPresetLandsExactly();

    testShapes();
    testRandomHold();
    testLfoMovesAParameter();
    testLfoIsAnOffsetNotAnOverride();
    testLfoIsFreeRunningAndSingleWriter();
    testPhaseWrapCountsCycles();
    testLfoOnAMissingTargetIsInert();
    testShapeParameterSnaps();

    testMacroFansOutThroughTheMappingTable();
    testMacroOnlyFiresWhenItMoves();
    testMacroTakesEffectOnceWithoutBeingTouched();
    testMacroCanBeModulated();

    testAssignIsAbsoluteAndIdempotent();
    testLearnAddsAndIsItsOwnUndoPerDestination();
    testOneLfoFansOutWithItsOwnRangePerDestination();
    testAModulatorNeverSetsAndAControlNeverAdds();
    testAssignRejectsSelfAndBadSlots();
    testAssignDoesNotRestartTheLfo();
    testLearnArmsCancelsAndExpires();
    testTargetSurvivesTheBankLosingIt();

    testLfoTargetsRoundTrip();
    testAPresetCannotSmuggleInASelfModulation();
    testAnOldPresetsLfoLineStillMeansFullDepth();
    testLearnedTargetRoundTripsThroughAPreset();
    testPresetWithoutModStillLoads();
    testCaptureSavesTheTargetNotTheMomentaryValue();
    return check::finish("mod");
}

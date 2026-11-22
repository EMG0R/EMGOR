#include "mod.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace nv {

const char* lfoShapeName(LfoShape s) {
    switch (s) {
        case LfoShape::Sine:       return "SINE";
        case LfoShape::Triangle:   return "TRI";
        case LfoShape::Ramp:       return "RAMP";
        case LfoShape::Square:     return "SQR";
        case LfoShape::RandomHold: return "RAND";
        default:                   return "?";
    }
}

// Integer hash -> 0..1. Same family as the one the particle pool uses for
// lifetime staggering: cheap, no state, and identical on both machines because
// it is all integer arithmetic with defined overflow.
static float hash01(unsigned x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float lfoShapeValue(LfoShape s, float phase, unsigned cycle, unsigned seed) {
    // Wrap rather than clamp: a caller handing us 1.25 means "a quarter of the
    // way through the next cycle", which is what a phase OFFSET produces.
    phase -= std::floor(phase);
    switch (s) {
        case LfoShape::Sine:
            return std::sin(phase * 6.28318530718f);
        case LfoShape::Triangle:
            // Rises through zero at phase 0, like the sine, so switching shape
            // mid-run does not step the offset.
            if (phase < 0.25f) return phase * 4.0f;
            if (phase < 0.75f) return 2.0f - phase * 4.0f;
            return phase * 4.0f - 4.0f;
        case LfoShape::Ramp:
            return phase * 2.0f - 1.0f;
        case LfoShape::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LfoShape::RandomHold:
            // One level per cycle, from the cycle number. The seed keeps four
            // LFOs at the same rate from marching in lockstep -- without it
            // they would all hold the identical random sequence and read as
            // one modulator.
            return hash01(cycle * 747796405u + seed * 2891336453u) * 2.0f - 1.0f;
        default:
            return 0.0f;
    }
}

void registerModParams(ParamBank& b) {
    for (int n = 0; n < kLfoCount; ++n) {
        char label[16];
        // Rate is exponential: the useful span runs from "once every minute and
        // a half" to "fast enough to be texture", and a linear fader would put
        // everything slow in the bottom two percent of its travel.
        std::snprintf(label, sizeof(label), "L%d RATE", n + 1);
        b.add({lfoParamId(n, "rate"), label, "lfo", 0.01f, 20.0f, 0.5f,
               Curve::Exp});

        // Depth is BIPOLAR, which is what gives an LFO its polarity for free:
        // negative is the same shape inverted, and the centre detent is
        // exactly zero, so "off" is a place your finger can find. That is also
        // why there is no separate enable switch -- one would be a second way
        // to say what depth 0 already says.
        std::snprintf(label, sizeof(label), "L%d DEPTH", n + 1);
        b.add({lfoParamId(n, "depth"), label, "lfo", -1.0f, 1.0f, 0.0f,
               Curve::Bipolar});

        std::snprintf(label, sizeof(label), "L%d PHASE", n + 1);
        b.add({lfoParamId(n, "phase"), label, "lfo", 0.0f, 1.0f, 0.0f,
               Curve::Linear});

        // SHAPE is a selection, and this is the parameter the per-parameter
        // slew opt-out exists for. Gliding it would not soften the change --
        // it would sweep through every waveform between the two on the way,
        // for as long as GLIDE is set to.
        std::snprintf(label, sizeof(label), "L%d SHAPE", n + 1);
        ParamDef shape{lfoParamId(n, "shape"), label, "lfo", 0.0f,
                       static_cast<float>(static_cast<int>(LfoShape::kCount) - 1),
                       0.0f, Curve::Linear};
        shape.slew = 0.0f;
        b.add(shape);
    }

    for (int n = 0; n < kMacroCount; ++n) {
        char label[16];
        std::snprintf(label, sizeof(label), "M%d", n + 1);
        // A macro is just a position. Everything about what it MEANS -- which
        // parameters, over what range, in which direction -- lives in the
        // mapping table, where that vocabulary already exists.
        b.add({macroParamId(n), label, "macro", 0.0f, 1.0f, 0.0f, Curve::Linear});
    }
}

bool lfoTargetRejected(int slot, const std::string& id) {
    if (slot < 0 || slot >= kLfoCount) return true;
    if (id.empty()) return false;   // clearing is always allowed
    // Its own knobs -- "lfo<slot>." -- and only its own. Another LFO's are
    // fair game.
    const std::string self = "lfo" + std::to_string(slot) + ".";
    return id.compare(0, self.size(), self) == 0;
}

std::vector<Mapping> lfoTargets(const MappingTable& table, int slot) {
    std::vector<Mapping> out;
    if (slot < 0 || slot >= kLfoCount) return out;
    for (int i : table.rowsFor(lfoSource(slot))) out.push_back(table.all()[i]);
    return out;
}

// A freshly learned or assigned destination: the full span, which is exactly
// what the old single-target model did. Narrowing it is a later, deliberate
// act on the MAP page, never something a bind gesture decides for you.
static Mapping lfoRow(int slot, const std::string& id) {
    Mapping m;
    m.source = lfoSource(slot);
    m.param = id;
    m.lo = 0.0f;
    m.hi = 1.0f;
    return m;
}

BindResult assignLfoTarget(MappingTable& table, int slot,
                           const std::string& id) {
    if (lfoTargetRejected(slot, id)) return BindResult::Rejected;
    // Absolute: whatever this LFO was driving, it now drives exactly this.
    // Clearing first is what makes a repeated datagram idempotent instead of
    // stacking a second identical row every time it arrives.
    table.clearSource(lfoSource(slot));
    if (id.empty()) return BindResult::Cleared;
    table.addRow(lfoRow(slot, id));
    // Phase is deliberately untouched. The LFO is free-running; re-pointing it
    // is a patch edit, not a trigger, and restarting the shape on a re-assign
    // would make an LFO an envelope the moment you moved it.
    return BindResult::Assigned;
}

BindResult learnLfoTarget(MappingTable& table, int slot,
                          const std::string& id) {
    if (lfoTargetRejected(slot, id)) return BindResult::Rejected;
    if (id.empty()) {
        table.clearSource(lfoSource(slot));
        return BindResult::Cleared;
    }
    // Already driving this one: the gesture is its own undo. Per DESTINATION,
    // so taking one knob back out of three leaves the other two exactly as
    // they were, ranges and all.
    int existing = table.findRow(lfoSource(slot), id);
    if (existing >= 0) {
        table.removeRow(existing);
        return BindResult::Cleared;
    }
    // ADDS rather than replaces. An LFO that can drive many things has no
    // reason to forget the last one you named just because you named another,
    // and "replace" would make fan-out unreachable from the device at all.
    table.addRow(lfoRow(slot, id));
    return BindResult::Assigned;
}

float lfoRateHz(const ParamBank& bank, int n) {
    return bank.valueOf(lfoParamId(n, "rate"), 0.0f);
}

float lfoOutput(const LfoBank& lfos, const ParamBank& bank, int n) {
    if (n < 0 || n >= kLfoCount) return 0.0f;
    const Lfo& l = lfos.lfo[n];

    float depth = bank.valueOf(lfoParamId(n, "depth"), 0.0f);
    if (depth == 0.0f) return 0.0f;

    int shapeIdx = static_cast<int>(
        std::lround(bank.valueOf(lfoParamId(n, "shape"), 0.0f)));
    shapeIdx = std::clamp(shapeIdx, 0, static_cast<int>(LfoShape::kCount) - 1);

    float off = bank.valueOf(lfoParamId(n, "phase"), 0.0f);
    float p = l.phase + off;
    // The offset can carry us into the next cycle, and RandomHold's level
    // comes from the cycle number, so the cycle has to move with it --
    // otherwise two LFOs differing only in phase would hold the identical
    // random level.
    unsigned cyc = l.cycle + static_cast<unsigned>(std::floor(p));

    float v = lfoShapeValue(static_cast<LfoShape>(shapeIdx), p, cyc,
                            static_cast<unsigned>(n) + 1u);
    // Depth is in NORMALIZED units, so +-1 is the parameter's whole travel
    // whatever its real range or curve. That is what lets one LFO be pointed at
    // zoom and then at a hue angle without re-scaling anything -- and it is
    // why a row's span composes with it by plain multiplication.
    return v * depth;
}

void updateLfos(LfoBank& lfos, const MappingTable& table, ParamBank& bank,
                float dt) {
    // Single writer: this function owns the modulation offsets for the frame,
    // so a target that lost its LFO cannot keep a stale offset stuck on it.
    bank.clearMod();

    for (int n = 0; n < kLfoCount; ++n) {
        Lfo& l = lfos.lfo[n];

        // Advance FIRST and unconditionally. An unassigned or silent LFO is
        // still running -- that is what "free-running" means, and it is why
        // bringing up a depth catches the shape where it would have been
        // rather than restarting it from zero.
        l.phase += lfoRateHz(bank, n) * dt;
        if (l.phase >= 1.0f) {
            float turns = std::floor(l.phase);
            l.phase -= turns;
            l.cycle += static_cast<unsigned>(turns);
        } else if (l.phase < 0.0f) {
            // dt is never negative in the frame loop, but a scrubbed clock in
            // a test would be, and a negative phase would break the wrap.
            float turns = std::floor(l.phase);
            l.phase -= turns;
            l.cycle -= static_cast<unsigned>(-turns);
        }

        float v = lfoOutput(lfos, bank, n);
        if (v == 0.0f) continue;
        // Through the table, exactly like a macro: every row this LFO owns
        // gets the same output scaled by its own span. A row naming a knob
        // that is not in the bank right now is inert and kept, which is the
        // rule the table already enforced for every other source.
        table.applyMod(lfoSource(n), v, bank);
    }
}

int applyMacros(MacroBank& macros, const MappingTable& table, ParamBank& bank) {
    int moved = 0;
    for (int n = 0; n < kMacroCount; ++n) {
        int i = bank.find(macroParamId(n));
        if (i < 0) continue;
        // The MODULATED position, so an LFO can drive a macro and the macro
        // fans that out -- one modulator moving eight parameters through eight
        // different ranges, with no new machinery for it.
        float v = bank.normMod(i);

        // Only on change. A macro pushed every frame would pin every parameter
        // it names to itself, and no fader, preset or sensor could ever move
        // one again -- the macro would silently become the only control on the
        // instrument. `primed` makes the first frame a move, so a macro
        // restored by a preset takes effect once without being touched.
        if (macros.primed && std::fabs(v - macros.last[n]) < 1e-6f) continue;
        macros.last[n] = v;

        if (table.apply(macroSource(n), v, bank)) ++moved;
    }
    macros.primed = true;
    return moved;
}

}  // namespace nv

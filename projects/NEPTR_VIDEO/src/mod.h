// Modulation: things that move parameters without a hand on them.
//
// Two of them, and NEITHER has a routing system of its own.
//
// The mapping table already does many-to-one, per-destination ranges and
// per-destination polarity (`lo > hi` inverts). That is the entire definition
// of "one thing moving several parameters", so both a MACRO and an LFO are
// simply SOURCES fed into that table:
//
//   macro:0  g.zoom     abs 0.2 0.8
//   lfo:0    g.zoom          0.0 0.4     # 40% of travel, upward
//   lfo:0    fx0.amount      0.3 0.0     # the same LFO, less, inverted
//
// The only difference between them is the one that matters -- a macro is a
// knob and therefore SETS, an LFO is an excursion and therefore ADDS -- and
// the table reads that off the source rather than off a flag. See mapping.h.
//
// What an LFO still owns here is its CLOCK: a free-running phase and a cycle
// count. That is state, not routing, so it stays in LfoBank. Rate, depth,
// phase offset and shape are ordinary bank parameters (`lfo0.rate`), which is
// what makes presets carry modulation for free.
#pragma once

#include <string>
#include <vector>

#include "control/mapping.h"
#include "state.h"

namespace nv {

// Four of each. Small on purpose: a modulation page you can read at a glance
// beats sixteen slots that are all at zero. One constant to raise, and the
// panel, the preset and the state broadcast all follow it.
constexpr int kLfoCount = 4;
constexpr int kMacroCount = 4;

enum class LfoShape {
    Sine = 0,
    Triangle,
    Ramp,
    Square,
    RandomHold,   // sample-and-hold: one random level per cycle
    kCount
};

const char* lfoShapeName(LfoShape s);

// The shape, evaluated. Pure -- no state, no bank, no clock -- because a wrong
// waveform is invisible in a screenshot and obvious in a test.
//
// Phase runs 0..1. The three continuous shapes -- sine, triangle, ramp -- all
// leave zero at phase 0 and rise, so switching between them mid-run does not
// step the offset. Square cannot: a square wave that passed through zero would
// not be a square wave, and that is the shape's whole point.
//
// RandomHold takes its level from a hash of the CYCLE NUMBER rather
// than keeping a held value: no state to initialise, no state to get out of
// step with the phase, and the same sequence every run, which is what makes it
// testable at all. (It is the same trick the particle pool uses for lifetimes.)
float lfoShapeValue(LfoShape s, float phase, unsigned cycle, unsigned seed);

// Parameter ids. Built rather than listed so raising kLfoCount is one edit.
inline std::string lfoParamId(int n, const char* p) {
    return "lfo" + std::to_string(n) + "." + p;
}
inline std::string macroParamId(int n) { return "macro" + std::to_string(n); }

// A macro's source id in the mapping table. `macro:0 g.zoom abs 0.2 0.8` is a
// complete macro assignment, in the file format that already exists.
inline std::string macroSource(int n) { return "macro:" + std::to_string(n); }
// ...and an LFO's, in exactly the same table. `lfo:0 g.zoom 0 0.4` is a
// complete modulation assignment. There is no second place routing is written.
inline std::string lfoSource(int n) { return "lfo:" + std::to_string(n); }

struct Lfo {
    // Free-running phase, advanced by the engine clock. Runs whether or not the
    // LFO is assigned or turned up, so bringing one in picks it up where it
    // would have been rather than restarting it -- an LFO that resets when you
    // touch its depth is a envelope wearing a disguise.
    float phase = 0.0f;
    unsigned cycle = 0;
};

struct LfoBank {
    Lfo lfo[kLfoCount];
};

// Evaluates every LFO and writes the frame's modulation offsets into the bank,
// through the mapping table -- so an LFO with three rows moves three
// parameters, each by its own amount and in its own direction.
//
// Clears the previous frame's offsets first, so this is the single writer.
// That is also why it runs every frame rather than on change: the offsets are
// rebuilt from nothing, and a modulator that only spoke when it moved would
// disappear the moment it sat still.
//
// `dt` is SCENE time, not wall time: SPEED scales the clock the whole engine
// runs on, and a modulator that ignored it would be the one thing that did not
// slow down with everything else.
void updateLfos(LfoBank& lfos, const MappingTable& table, ParamBank& bank,
                float dt);

// The LFO's own output this frame, in normalized units, before any row's span
// is applied: shape(phase) * depth. Split out because it is what the panel and
// the tests want to talk about, and because updateLfos would otherwise be the
// only place the shape is ever evaluated against the clock.
float lfoOutput(const LfoBank& lfos, const ParamBank& bank, int n);

// Rate is free-running Hz today. TEMPO SYNC HOOK: the rig broadcasts BPM on
// ch16 CC119 (40..400), which arrives here as the source `cc:16:119` like any
// other -- so sync is "convert that to Hz and multiply by a division", and the
// only piece missing is the division control. Deliberately not built yet: the
// engine is not on the clock bus at all today (see the receiver's note on the
// 8004 fan-out), so it would be an untestable knob wired to nothing.
float lfoRateHz(const ParamBank& bank, int n);

struct MacroBank {
    // Last value each macro was seen at. A macro is only pushed through the
    // mapping table WHEN IT MOVES: pushing it every frame would mean a macro
    // sitting still permanently overwrote every parameter it touches, and no
    // other control -- fader, preset, sensor -- could ever move one again.
    float last[kMacroCount] = {0};
    bool primed = false;
};

// Feeds every macro that moved into the mapping table. Returns how many did.
int applyMacros(MacroBank& macros, const MappingTable& table, ParamBank& bank);

// Registers the LFO and macro knobs. Called from registerGlobalParams, because
// modulation belongs to the instrument and not to whichever kernel is loaded --
// it has to keep its values, and its indices, across a patch change.
void registerModParams(ParamBank& bank);

// ---- assigning a target -------------------------------------------------
//
// The one thing about an LFO that is a NAME rather than a number, and
// therefore the one thing no fader could set. Every route that can point an
// LFO at something -- a finger on the panel, the L key, `/nv/lfo` off the
// wire, NV_LFO at startup -- ends up in exactly one of the two functions
// below, so there is a single answer to "what may an LFO be pointed at".
enum class BindResult {
    Rejected,   // nothing changed, and the caller should say why
    Assigned,
    Cleared,
};

// True when `id` must not be assigned to LFO `slot`.
//
// An LFO may modulate ANOTHER LFO -- cross-modulation is the reason to have
// four -- but not its own four knobs. That is not squeamishness about
// recursion: on the MOD page an LFO's own DEPTH slider is inches from its own
// TARGET button, so "modulate yourself" is the single easiest gesture to fire
// by accident, and an LFO whose depth is being swung through zero by itself
// looks exactly like one that has stopped working.
bool lfoTargetRejected(int slot, const std::string& id);

// Point LFO `slot` at `id`, and at NOTHING ELSE; an empty id clears it
// entirely. Absolute, and therefore idempotent: the same datagram twice is
// still that one assignment. This is what `/nv/lfo` and NV_LFO get, because a
// message that arrives twice must not mean the opposite the second time -- and
// because a remote surface saying "L1 drives detail" is stating the routing,
// not adding to it.
BindResult assignLfoTarget(MappingTable& table, int slot, const std::string& id);

// The LEARN version, for a gesture, and the one that changed shape when an LFO
// gained fan-out. It is ADDITIVE: each touch adds a destination, so naming
// three knobs in a row gives an LFO three of them. Pointing it at one it
// ALREADY drives removes that destination -- so the gesture is still its own
// undo, now per destination, which is the only way to take one knob back out
// of three without retyping the other two. An empty id still clears the slot.
//
// A new row lands at full span (0..1): the same amount the old one-target
// model always had, so a learn behaves exactly as it used to until the range
// is deliberately narrowed on the MAP page.
BindResult learnLfoTarget(MappingTable& table, int slot, const std::string& id);

// What LFO `slot` drives right now, in bank order. Enough for the panel to say
// "one destination and its name" or "three destinations", and for a preset to
// write the rows out.
std::vector<Mapping> lfoTargets(const MappingTable& table, int slot);

// Which LFO the next parameter you touch will be bound to.
//
// A modal state on a performance surface has to earn itself. It is here
// because the alternative -- a scrollable list of every id in the bank, on a
// bank whose tail changes with the kernel and the chain -- is a second way of
// naming parameters that has to be built, laid out and kept in step with the
// sliders that already name them. Touching the slider IS the name.
//
// Three escapes, because an armed learn eats the next thing you touch:
// pressing the same TARGET button again, ESC, and a timeout that disarms it
// on its own if you walked away.
class ModLearn {
public:
    static constexpr double kTimeout = 10.0;

    void arm(int slot, double now) {
        slot_ = (slot >= 0 && slot < kLfoCount) ? slot : -1;
        armedAt_ = now;
    }
    // The button is its own cancel: pressing the armed one disarms.
    void toggle(int slot, double now) {
        if (slot_ == slot) cancel();
        else arm(slot, now);
    }
    void cancel() { slot_ = -1; }

    bool armed() const { return slot_ >= 0; }
    int slot() const { return slot_; }

    // Called once a frame. Disarms after kTimeout so a learn cannot sit armed
    // behind your back and swallow a slider you meant to drag.
    void tick(double now) {
        if (slot_ >= 0 && now - armedAt_ >= kTimeout) cancel();
    }
    float secondsLeft(double now) const {
        if (slot_ < 0) return 0.0f;
        double left = kTimeout - (now - armedAt_);
        return left < 0.0 ? 0.0f : static_cast<float>(left);
    }

private:
    int slot_ = -1;
    double armedAt_ = 0.0;
};

}  // namespace nv

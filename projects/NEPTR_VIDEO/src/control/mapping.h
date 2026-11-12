// Control mapping: an external source drives a parameter.
//
// One table, one rule. A MIDI CC, an OSC message and a sensor axis are all
// just "a source id and a 0..1 value" by the time they arrive here, so they
// share every feature -- range, curve, invert, relative-vs-absolute -- instead
// of each growing its own half-implementation.
//
// Sources are named strings rather than an enum so a new transport needs no
// change here at all:
//
//   "cc:1:74"      MIDI channel 1, controller 74
//   "osc:/p/tilt"  the first numeric argument of that OSC address
//   "macro:0"      a knob on this instrument's own MOD page
//   "lfo:0"        a modulator on this instrument's own MOD page
//
// The mapping file is plain text, hot-reloadable, and lives next to the
// patches. Nothing about the engine is recompiled to re-map a knob.
//
// ---- SET rows and ADD rows ------------------------------------------------
//
// A row has a CHARACTER, and it is not a per-row option: it is read off the
// source, because it is a property of what the source IS.
//
//   a physical control SETS      a fader, a sensor, a pedal, a macro knob --
//                                things that HAVE a position. The value is
//                                where the parameter now is.
//   a modulator ADDS             an LFO -- a thing with no position at all,
//                                only an excursion. The value is a bipolar
//                                offset on top of wherever control put the
//                                parameter.
//
// Routing an LFO through the SET path would pin every parameter it names to
// itself: you would turn the fader and nothing would happen, which is the
// single worst thing a modulation system can do. Routing a sensor through the
// ADD path would ratchet, because ADD is re-applied every frame and a sensor
// sitting still would keep adding. So neither direction is worth offering, and
// there is no flag to get wrong -- `lfo:` adds, everything else sets.
//
// The two also have different LIFECYCLES, and are therefore two calls:
//
//   apply()     fires on an incoming EVENT. Once, when something moved.
//   applyMod()  fires EVERY FRAME, after clearMod(), because modulation is
//               continuous and the bank's offsets are rebuilt from scratch.
//
// For an ADD row `lo`/`hi` is read as a SPAN rather than as endpoints: a
// modulator has no absolute position, so only the DIFFERENCE between them
// means anything. `hi - lo` is how much of the parameter's travel this
// destination gets (0..1 = all of it, the plain full-depth default), and
// `lo > hi` inverts it. That is the payoff of putting LFOs in this table: one
// modulator, three destinations, a different amount and a different direction
// on each -- exactly what `lo`/`hi` already did for a macro.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "state.h"

namespace nv {

enum class MapMode {
    Absolute,   // the value IS the position
    Relative,   // the value is a delta (encoder detents: 127 = +1, 0 = -1)
};

struct Mapping {
    std::string source;   // "cc:1:74", "osc:/p/tilt", "lfo:0"
    std::string param;    // parameter id in the bank
    MapMode mode = MapMode::Absolute;
    float lo = 0.0f;      // output range within the parameter's own 0..1 span
    float hi = 1.0f;
    float step = 0.02f;   // per-detent movement in Relative mode
    // Derived from `source`, never parsed from the line. See the header note:
    // a row's character belongs to what is driving it, not to the line.
    bool mod = false;
};

// True when this source is a MODULATOR -- something with an excursion and no
// position. One function so "which sources add" is answered in one place; a
// later envelope or follower joins by extending it and nothing else changes.
bool isModSource(const std::string& source);

class MappingTable {
public:
    // Text format, one mapping per line; '#' comments; blank lines ignored:
    //
    //   <source> <param> [abs|rel] [lo hi] [step]
    //
    // Unknown parameter ids are kept, not dropped: a mapping may name a knob
    // belonging to an effect that is not in the current chain, and it must
    // start working again when that effect comes back.
    bool loadFromString(const std::string& text, std::string* error = nullptr);
    bool loadFile(const std::string& path, std::string* error = nullptr);

    // Applies a source EVENT: sets the position of every parameter this
    // source drives. Returns true if it moved something -- false means nothing
    // is mapped to that source, which is not an error.
    //
    // Modulator rows are skipped here on purpose. They are not events and they
    // do not set; routing one through this call is the bug this whole split
    // exists to make unrepresentable.
    bool apply(const std::string& source, float value, ParamBank& bank) const;

    // Applies a MODULATOR's current output: adds `bipolar` (-1..1, already
    // scaled by depth) to every parameter this source drives, scaled by the
    // row's span and signed by its direction. Returns how many rows fired.
    //
    // Called every frame, after ParamBank::clearMod(), because the bank's
    // offsets are rebuilt from nothing each frame -- a modulator that applied
    // only "on change" would vanish the frame after it stopped moving.
    int applyMod(const std::string& source, float bipolar, ParamBank& bank) const;

    // Built-in defaults, so a fresh install responds to a controller without
    // anyone writing a file first.
    void loadDefaults();

    size_t size() const { return maps_.size(); }
    const std::vector<Mapping>& all() const { return maps_; }

    // ---- editing at runtime -----------------------------------------------
    //
    // The table stopped being a file read once at startup the moment an LFO
    // became a source in it: a learn gesture on the device edits routing, and
    // the mapping page can delete a row. Everything below is that, and it is
    // deliberately small -- add, remove, retune the span, drop a whole source.
    //
    // Indices are positions in all(); they are invalidated by any of these, so
    // a caller holding one across a call is holding a stale one.

    // The row driving `param` from `source`, or -1. This is what makes the
    // learn gesture its own undo per DESTINATION: touching a knob an LFO
    // already drives finds the row and removes it, leaving its other
    // destinations alone.
    int findRow(const std::string& source, const std::string& param) const;
    // Appends. No de-duplication: the caller decides, because "the same source
    // twice on the same parameter" is a mistake for a gesture and merely a
    // sum for a hand-written file.
    void addRow(const Mapping& m);
    void removeRow(int index);
    // Everything driven by one source. How `/nv/lfo` says "this and only this"
    // and how a preset load replaces the patch's modulation wholesale.
    void clearSource(const std::string& source);
    int countFor(const std::string& source) const;
    std::vector<int> rowsFor(const std::string& source) const;
    // Retunes a row's span in place. The source cannot change this way, so the
    // index is still valid afterwards and nothing needs reindexing.
    bool setRange(int index, float lo, float hi);

    // Every mapping naming a parameter the bank does not currently have.
    // Diagnostic, not an error: reported so a typo is visible.
    std::vector<std::string> danglingParams(const ParamBank& bank) const;

private:
    std::vector<Mapping> maps_;
    // Source -> indices into maps_. One source may drive several parameters;
    // that is a feature (macro controls), not a conflict.
    std::unordered_map<std::string, std::vector<int>> bySource_;

    void reindex();
};

// Canonical source id for a MIDI controller change. Channel is 1-indexed, to
// match how every other part of DEMIURGE talks about channels.
std::string ccSource(int channel, int controller);

}  // namespace nv

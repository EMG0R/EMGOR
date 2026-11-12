// Presets.
//
// Three rules, and they are what make a preset saved today still load in a
// year, on either machine:
//
//   1. EVERYTHING IS ADDRESSED BY ID, NEVER BY INDEX. Parameter indices shift
//      the moment a knob is added; ids do not. Generators, palettes and
//      effects are stored by their string id for the same reason.
//   2. VALUES ARE IN REAL UNITS, not normalized. If a range is ever widened,
//      a stored 96.0 still means 96.0; a stored 0.5 would silently become a
//      different number.
//   3. UNKNOWN KEYS ARE SKIPPED, MISSING KEYS KEEP THEIR DEFAULT. A preset
//      from a newer build loads on an older one minus what it cannot know,
//      and an old preset loads on a new build with new knobs at their
//      defaults. Neither is an error.
//
// The format is plain text: diffable, git-able, hand-editable, and identical
// on Mac and Pi (no endianness, no struct packing, no binary).
#pragma once

#include <string>
#include <vector>

#include "chain/effects.h"
#include "mod.h"
#include "state.h"

namespace nv {

// The neutral form. Deliberately NOT the live engine state: loading has to
// resolve ids to indices and rebuild the parameter bank before values can be
// applied, so parsing and applying are separate steps.
struct PresetData {
    int version = 2;
    std::string mode = "SYNTH";
    std::string kernel;        // generator id
    std::string kernelB;       // morph target id ("" = none)
    std::string channelBKernel; // Channel B generator id ("" = current)
    int sourceA = 0;           // SourceKind, Fractal by default
    int sourceB = 255;         // SourceKind, None by default
    int colorMode = 0;
    int trapShape = 0;
    std::string paletteA;      // palette NAME
    std::string paletteB;
    // Effect id + enabled, in chain order.
    std::vector<std::pair<std::string, bool>> chain;
    // Parameter id -> value, in real units.
    std::vector<std::pair<std::string, float>> params;
    // One modulation destination: which LFO, which parameter, and how much of
    // that parameter's travel it gets (`lo > hi` inverts, as everywhere else).
    // Everything else about an LFO -- rate, depth, phase, shape -- is in the
    // bank and rides along in `params` for free; the routing is the one part
    // of a modulator that is a NAME rather than a number.
    struct LfoRow {
        int slot = 0;
        std::string param;
        float lo = 0.0f;
        float hi = 1.0f;
    };
    // Several rows may share a slot: an LFO fanning out to three parameters is
    // three lines, which is the same shape the mapping file has always had.
    std::vector<LfoRow> lfoRows;
};

// ---- who owns a mapping row --------------------------------------------
//
// The mapping table now carries two kinds of row, and a preset load has to
// know which of them it may touch. The answer is read off the SOURCE, because
// the source is what says who made the row:
//
//   lfo:*         PATCH. This is how this sound modulates itself, it is made
//                 by a learn gesture on stage, and it was already inside the
//                 preset before any of this (as the `lfo <slot> <id>` key).
//                 A preset SAVES these and a preset LOAD REPLACES them.
//   everything    RIG. sensor:, cc:, p:, osc:, macro: describe how this
//   else          performer's hardware is wired to the instrument. They live
//                 in a file the performer owns. A preset never writes them and
//                 a preset load never touches them -- recalling a sound must
//                 not silently re-wire the pedal.
//
// That is the question that kept macro-binding-by-touch off the instrument,
// and this is the answer: it is decided by the row, not by the gesture.

// Captures the live state. `chain` supplies the effect order, `table` the
// modulation rows -- only the `lfo:*` ones, per the rule above.
PresetData capturePreset(const ParamBank& bank, const Selection& sel,
                         const std::vector<ChainSlot>& chain,
                         const MappingTable* table = nullptr);

std::string serializePreset(const PresetData& p);
bool parsePreset(const std::string& text, PresetData& out, std::string* error);

bool savePresetFile(const std::string& path, const PresetData& p);
bool loadPresetFile(const std::string& path, PresetData& out, std::string* error);

// Resolves ids to indices and rebuilds the chain. Values are NOT applied here
// -- the caller must rebuild the parameter bank first, because the bank does
// not contain the new generator's or new chain's knobs until it does.
void applyPresetSelection(const PresetData& p, Selection& sel,
                          std::vector<ChainSlot>& chain);

// Applies values by id. Unknown ids are counted, not fatal.
//
// Note what is NOT here: a glide argument. Whether a recall glides is the
// caller's business and it says so with a GlideScope -- the same call applied
// at startup by NV_LOAD should land instantly, and the same call from a
// SHIFT+key on stage should fly. One function, the rate set around it.
int applyPresetParams(const PresetData& p, ParamBank& bank, int* unknown);

// Restores the modulation rows into the mapping table.
//
// EVERY `lfo:*` row is cleared first, including ones the mapping file
// supplied: a preset describes a whole patch, and a modulator left running
// from the previous one would be a ghost nothing in the file explains. Rows
// belonging to any other source are left exactly as they are -- see the
// ownership note above. That is the whole rule, and it is why a preset recall
// on stage cannot cost you the sensor mappings you arrived with.
void applyPresetLfos(const PresetData& p, MappingTable& table);

// Default path for a numbered slot.
std::string presetPath(const std::string& dir, int slot);

}  // namespace nv

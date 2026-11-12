#include "preset.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "core/kernels.h"
#include "palette/palette.h"

namespace nv {

static const char* modeToText(Mode m) { return modeName(m); }

static Mode modeFromText(const std::string& s) {
    for (int i = 0; i < static_cast<int>(Mode::kCount); ++i) {
        if (s == modeName(static_cast<Mode>(i))) return static_cast<Mode>(i);
    }
    return Mode::Synth;
}

PresetData capturePreset(const ParamBank& bank, const Selection& sel,
                         const std::vector<ChainSlot>& chain,
                         const MappingTable* table) {
    PresetData p;
    const auto& kb = kernelBank();
    const auto& pb = paletteBank();
    const auto& eb = effectBank();

    p.mode = modeToText(sel.mode);
    if (sel.kernelIndex >= 0 && sel.kernelIndex < static_cast<int>(kb.size()))
        p.kernel = kb[sel.kernelIndex].id;
    if (sel.kernelB != sel.kernelIndex && sel.kernelB >= 0 &&
        sel.kernelB < static_cast<int>(kb.size()))
        p.kernelB = kb[sel.kernelB].id;
    if (sel.channelBKernel >= 0 &&
        sel.channelBKernel < static_cast<int>(kb.size()) &&
        sel.channelBKernel != sel.kernelIndex)
        p.channelBKernel = kb[sel.channelBKernel].id;
    p.sourceA = static_cast<int>(sel.sourceA);
    p.sourceB = static_cast<int>(sel.sourceB);
    p.colorMode = sel.colorMode;
    p.trapShape = sel.trapShape;
    if (sel.paletteA >= 0 && sel.paletteA < static_cast<int>(pb.size()))
        p.paletteA = pb[sel.paletteA].name;
    if (sel.paletteB >= 0 && sel.paletteB < static_cast<int>(pb.size()))
        p.paletteB = pb[sel.paletteB].name;

    for (const auto& s : chain) {
        if (s.def < 0 || s.def >= static_cast<int>(eb.size())) continue;
        p.chain.emplace_back(eb[s.def].id, s.enabled);
    }

    for (int i = 0; i < bank.count(); ++i) {
        const ParamDef& d = bank.def(i);
        // Padding slots exist only to keep the uniform packing regular; they
        // are not state and writing them would be noise in the file.
        if (d.hidden) continue;
        // The TARGET, not the live value. Saving value() would bake in
        // wherever a glide had got to and whatever an LFO happened to be
        // adding at the instant you pressed the key -- so the same patch,
        // saved twice a second apart, would be two different presets.
        p.params.emplace_back(d.id, bank.targetValue(i));
    }

    if (table) {
        // Only the LFO rows. A sensor or an encoder mapping belongs to the rig
        // and to the file the performer owns, and writing it into a patch
        // would make recalling a sound a way to lose your pedal.
        for (int n = 0; n < kLfoCount; ++n) {
            for (const Mapping& m : lfoTargets(*table, n)) {
                PresetData::LfoRow r;
                r.slot = n;
                r.param = m.param;
                r.lo = m.lo;
                r.hi = m.hi;
                p.lfoRows.push_back(r);
            }
        }
    }
    return p;
}

std::string serializePreset(const PresetData& p) {
    std::ostringstream o;
    o << "# neptr-video preset\n";
    o << "version " << p.version << "\n";
    o << "mode " << p.mode << "\n";
    o << "kernel " << p.kernel << "\n";
    if (!p.kernelB.empty()) o << "kernel_b " << p.kernelB << "\n";
    if (!p.channelBKernel.empty()) o << "channel_b_kernel " << p.channelBKernel << "\n";
    o << "source_a " << p.sourceA << "\n";
    o << "source_b " << p.sourceB << "\n";
    o << "color " << p.colorMode << "\n";
    o << "trap " << p.trapShape << "\n";
    // Palette names can contain no spaces by construction, so a bare token is
    // unambiguous and stays hand-editable.
    o << "palette_a " << p.paletteA << "\n";
    o << "palette_b " << p.paletteB << "\n";
    for (const auto& c : p.chain) {
        o << "fx " << c.first << " " << (c.second ? "on" : "off") << "\n";
    }
    char buf[128];
    // `lfo <slot> <parameter-id> <lo> <hi>`. Slots with no destination are
    // simply absent -- an assignment that is not there is not a thing to
    // record -- and a slot with three of them writes three lines.
    //
    // The two numbers are an EXTENSION of the key that already existed, not a
    // second key: a build that predates fan-out reads `lfo 0 g.detail 0 1`
    // with its old `slot >> target` and gets exactly the assignment it always
    // did, ignoring a span it has no way to represent. That is rule 3 --
    // unknown parts are skipped -- applied inside a line instead of to it.
    for (const auto& l : p.lfoRows) {
        std::snprintf(buf, sizeof(buf), "%.6g %.6g", l.lo, l.hi);
        o << "lfo " << l.slot << " " << l.param << " " << buf << "\n";
    }
    for (const auto& kv : p.params) {
        // %.6g keeps the file readable while round-tripping a float exactly
        // enough that a save/load cycle is not audible on any control.
        std::snprintf(buf, sizeof(buf), "%.6g", kv.second);
        o << "p " << kv.first << " " << buf << "\n";
    }
    return o.str();
}

bool parsePreset(const std::string& text, PresetData& out, std::string* error) {
    PresetData p;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    bool sawVersion = false;

    while (std::getline(in, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;

        if (key == "version") {
            ls >> p.version;
            sawVersion = true;
        } else if (key == "mode") {
            ls >> p.mode;
        } else if (key == "kernel") {
            ls >> p.kernel;
        } else if (key == "kernel_b") {
            ls >> p.kernelB;
        } else if (key == "channel_b_kernel") {
            ls >> p.channelBKernel;
        } else if (key == "source_a") {
            ls >> p.sourceA;
        } else if (key == "source_b") {
            ls >> p.sourceB;
        } else if (key == "color") {
            ls >> p.colorMode;
        } else if (key == "trap") {
            ls >> p.trapShape;
        } else if (key == "palette_a") {
            ls >> p.paletteA;
        } else if (key == "palette_b") {
            ls >> p.paletteB;
        } else if (key == "fx") {
            std::string id, state;
            if (ls >> id >> state) p.chain.emplace_back(id, state != "off");
        } else if (key == "lfo") {
            int slot = -1;
            std::string target;
            // Out-of-range slots are dropped rather than clamped: a file from
            // a build with eight LFOs must not land its fifth on this build's
            // first. Same reason presets never address anything by index.
            if ((ls >> slot >> target) && slot >= 0 && slot < kLfoCount) {
                PresetData::LfoRow r;
                r.slot = slot;
                r.param = target;
                // The span is OPTIONAL, which is what makes every preset
                // written before fan-out existed still mean what it said: no
                // numbers = the full travel = the old single-target depth.
                float lo = 0.0f, hi = 0.0f;
                if (ls >> lo >> hi) {
                    r.lo = lo;
                    r.hi = hi;
                }
                p.lfoRows.push_back(r);
            }
        } else if (key == "p") {
            std::string id;
            float v = 0.0f;
            if (ls >> id >> v) p.params.emplace_back(id, v);
        }
        // Anything else is a key this build does not know. Skipping it is the
        // whole forward-compatibility story: a preset written by a newer
        // version loads here minus what cannot be represented.
    }

    if (!sawVersion) {
        if (error) *error = "not a preset (no version line)";
        return false;
    }
    out = std::move(p);
    return true;
}

bool savePresetFile(const std::string& path, const PresetData& p) {
    std::ofstream f(path);
    if (!f) return false;
    f << serializePreset(p);
    return f.good();
}

bool loadPresetFile(const std::string& path, PresetData& out, std::string* error) {
    std::ifstream f(path);
    if (!f) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parsePreset(ss.str(), out, error);
}

void applyPresetSelection(const PresetData& p, Selection& sel,
                          std::vector<ChainSlot>& chain) {
    const auto& kb = kernelBank();
    const auto& pb = paletteBank();

    sel.mode = modeFromText(p.mode);
    // A generator that no longer exists leaves the current one in place
    // rather than snapping to index 0 and losing the rest of the preset.
    for (size_t i = 0; i < kb.size(); ++i) {
        if (kb[i].id == p.kernel) sel.kernelIndex = static_cast<int>(i);
    }
    sel.kernelB = sel.kernelIndex;
    if (!p.kernelB.empty()) {
        for (size_t i = 0; i < kb.size(); ++i) {
            if (kb[i].id == p.kernelB) sel.kernelB = static_cast<int>(i);
        }
    }
    sel.channelBKernel = sel.kernelIndex;
    if (!p.channelBKernel.empty()) {
        for (size_t i = 0; i < kb.size(); ++i) {
            if (kb[i].id == p.channelBKernel)
                sel.channelBKernel = static_cast<int>(i);
        }
    }
    // The source wire shape is reserved for the later adapters, but this
    // slice only renders fractals. Unknown/unimplemented sources fall back to
    // the safe identity state instead of claiming to show a stream.
    sel.sourceA = SourceKind::Fractal;
    sel.sourceB = p.sourceB == static_cast<int>(SourceKind::Fractal)
                      ? SourceKind::Fractal : SourceKind::None;
    sel.colorMode = p.colorMode;
    sel.trapShape = p.trapShape;
    for (size_t i = 0; i < pb.size(); ++i) {
        if (pb[i].name == p.paletteA) sel.paletteA = static_cast<int>(i);
        if (pb[i].name == p.paletteB) sel.paletteB = static_cast<int>(i);
    }

    if (!p.chain.empty()) {
        std::vector<ChainSlot> c;
        for (const auto& e : p.chain) {
            int def = findEffect(e.first);
            if (def < 0) continue;  // an effect this build does not have
            ChainSlot s;
            s.def = def;
            s.enabled = e.second;
            c.push_back(s);
        }
        chain = std::move(c);
    }
}

int applyPresetParams(const PresetData& p, ParamBank& bank, int* unknown) {
    int applied = 0;
    int miss = 0;
    for (const auto& kv : p.params) {
        // V1 used b.k.* for the morph target. V2 reserves b.* for Channel B,
        // so migrate the old meaning explicitly instead of allowing a future
        // b.k.* channel id to reinterpret it silently.
        std::string id = kv.first;
        if (p.version < 2 && id.compare(0, 4, "b.k.") == 0) {
            id = "a.mk.k." + id.substr(4);
        }
        int i = bank.find(id);
        if (i < 0) {
            ++miss;
            continue;
        }
        // setValue clamps into the current range, so a value saved when the
        // range was wider lands at the edge instead of being rejected.
        bank.setValue(i, kv.second);
        ++applied;
    }
    if (unknown) *unknown = miss;
    return applied;
}

void applyPresetLfos(const PresetData& p, MappingTable& table) {
    // Clear the PATCH-owned rows first, and only those. Every slot the file
    // does not name is unassigned, not whatever the last patch left running --
    // a modulator moving something with nothing in the file to explain it is
    // the worst kind of ghost. Rig rows (sensor:, cc:, macro:, ...) are not
    // touched at all: they belong to the mapping file and to the hardware in
    // front of the performer, not to the sound being recalled.
    //
    // Phase is NOT reset: the LFOs are free-running and a recall should not
    // restart the instrument's clock.
    for (int n = 0; n < kLfoCount; ++n) table.clearSource(lfoSource(n));
    for (const auto& l : p.lfoRows) {
        if (l.slot < 0 || l.slot >= kLfoCount) continue;
        // The same refusal the gesture and the wire get. A hand-edited preset
        // is still a front door, and an LFO modulating its own depth is the
        // one assignment no door may make.
        if (lfoTargetRejected(l.slot, l.param)) continue;
        Mapping m;
        m.source = lfoSource(l.slot);
        m.param = l.param;
        m.lo = l.lo;
        m.hi = l.hi;
        table.addRow(m);
    }
}

std::string presetPath(const std::string& dir, int slot) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/preset%02d.nvp", slot);
    return dir + buf;
}

}  // namespace nv

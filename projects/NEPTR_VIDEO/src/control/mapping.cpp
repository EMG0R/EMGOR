#include "control/mapping.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace nv {

bool isModSource(const std::string& source) {
    return source.compare(0, 4, "lfo:") == 0;
}

std::string ccSource(int channel, int controller) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "cc:%d:%d", channel, controller);
    return buf;
}

void MappingTable::reindex() {
    bySource_.clear();
    for (size_t i = 0; i < maps_.size(); ++i) {
        bySource_[maps_[i].source].push_back(static_cast<int>(i));
    }
}

bool MappingTable::loadFromString(const std::string& text, std::string* error) {
    std::vector<Mapping> parsed;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);

        std::istringstream ls(line);
        Mapping m;
        if (!(ls >> m.source)) continue;  // blank or comment-only
        if (!(ls >> m.param)) {
            if (error) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "line %d: no parameter given", lineNo);
                *error = buf;
            }
            return false;
        }

        m.mod = isModSource(m.source);

        // The mode word is OPTIONAL, and a line may go straight to its range.
        // `lfo:0 g.zoom 0 0.4` reads better than `lfo:0 g.zoom abs 0 0.4` and
        // is more honest -- a modulator is neither absolute nor relative, and
        // making it write one of those words would be asking for a fact that
        // does not exist. So a third token that parses as a number is the
        // range starting early, not a typo.
        std::string mode;
        bool haveLo = false;
        if (ls >> mode) {
            if (mode == "rel") m.mode = MapMode::Relative;
            else if (mode == "abs") m.mode = MapMode::Absolute;
            else {
                char* endp = nullptr;
                float v = std::strtof(mode.c_str(), &endp);
                if (endp && *endp == '\0' && endp != mode.c_str()) {
                    m.lo = v;
                    haveLo = true;
                } else {
                    if (error) {
                        char buf[160];
                        std::snprintf(buf, sizeof(buf),
                                      "line %d: expected abs, rel or a number, "
                                      "got '%s'", lineNo, mode.c_str());
                        *error = buf;
                    }
                    return false;
                }
            }
        }

        // `rel` counts detents onto the target. A modulator emits a level, not
        // detents, and one counted onto the target would walk the parameter
        // away and never bring it back -- so this is refused at the line
        // rather than quietly reinterpreted.
        if (m.mod && m.mode == MapMode::Relative) {
            if (error) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                              "line %d: '%s' is a modulator and cannot be rel",
                              lineNo, m.source.c_str());
                *error = buf;
            }
            return false;
        }

        if (haveLo) {
            // The mode slot already held `lo`, so only `hi` is left.
            float b = 0.0f;
            if (ls >> b) m.hi = b;
        } else {
            float a = 0.0f, b = 0.0f;
            if (ls >> a >> b) {
                m.lo = a;
                m.hi = b;
            }
        }
        float s = 0.0f;
        if (ls >> s) m.step = s;

        parsed.push_back(std::move(m));
    }

    maps_ = std::move(parsed);
    reindex();
    return true;
}

bool MappingTable::loadFile(const std::string& path, std::string* error) {
    std::ifstream f(path);
    if (!f) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return loadFromString(ss.str(), error);
}

void MappingTable::loadDefaults() {
    // Sensors first: these are demiurge-io's Teensy source ids, conditioned
    // to 0..1 and arriving on the telemetry port. Chosen so the physical
    // gesture suits the parameter -- the joystick moves you through the
    // plane, the pedal drives detail, tilt drives colour.
    //
    // The encoders are the Teensy's PHYSICAL rotation CCs (ch1 CC20-26), sent
    // one message per detent, hence `rel`. They are the same numbers the
    // audio engine sees, so one controller drives either without rewiring.
    loadFromString(
        "sensor:jx   g.center_x      abs\n"
        "sensor:jy   g.center_y      abs\n"
        "sensor:exp  g.detail        abs\n"
        "sensor:tof1 g.zoom          abs\n"
        "sensor:tof2 g.palette_shift abs\n"
        "sensor:fsr1 g.palette_mix   abs\n"
        "sensor:ax   g.rotation      abs\n"
        "sensor:ml   g.speed         abs 0.2 1.0\n"

        "cc:1:20 g.speed         rel 0 1 0.02\n"
        "cc:1:21 g.zoom          rel 0 1 0.02\n"
        "cc:1:22 g.rotation      rel 0 1 0.02\n"
        "cc:1:23 g.detail        rel 0 1 0.02\n"
        "cc:1:24 g.palette_shift rel 0 1 0.02\n"
        "cc:1:25 g.palette_mix   rel 0 1 0.02\n"
        "cc:1:26 g.render_scale  rel 0 1 0.02\n");
}

bool MappingTable::apply(const std::string& source, float value,
                         ParamBank& bank) const {
    auto it = bySource_.find(source);
    if (it == bySource_.end()) return false;

    bool moved = false;
    for (int idx : it->second) {
        const Mapping& m = maps_[idx];
        // A modulator never sets. Nothing on the wire can name one -- sources
        // off the bus are `sensor:`/`cc:`/`p:`/`osc:` -- but the guard is what
        // makes "modulators add" a property of the table rather than of who
        // happened to call it.
        if (m.mod) continue;
        int p = bank.find(m.param);
        // A mapping for a parameter that is not currently in the bank is
        // simply inert until that effect comes back.
        if (p < 0) continue;

        if (m.mode == MapMode::Relative) {
            // Encoder convention, shared with the Teensy: 127 = one detent
            // clockwise, 0 = one counter-clockwise.
            float dir = (value >= 0.5f) ? 1.0f : -1.0f;
            bank.nudgeNorm(p, dir * m.step);
        } else {
            float t = std::clamp(value, 0.0f, 1.0f);
            bank.setNorm(p, m.lo + (m.hi - m.lo) * t);
        }
        moved = true;
    }
    return moved;
}

int MappingTable::applyMod(const std::string& source, float bipolar,
                           ParamBank& bank) const {
    auto it = bySource_.find(source);
    if (it == bySource_.end()) return 0;

    int fired = 0;
    for (int idx : it->second) {
        const Mapping& m = maps_[idx];
        // Symmetric with the guard in apply(): a physical control has a
        // position and adding it would ratchet.
        if (!m.mod) continue;
        int p = bank.find(m.param);
        // Same rule as everywhere else: a parameter that is not in the bank
        // right now is inert, not an error, and the row is kept.
        if (p < 0) continue;
        // hi - lo is the SPAN. Signed, so lo > hi inverts this destination
        // without inverting the modulator itself -- which is the whole reason
        // one LFO can pull one parameter up while it pushes another down.
        bank.addMod(p, (m.hi - m.lo) * bipolar);
        ++fired;
    }
    return fired;
}

int MappingTable::findRow(const std::string& source,
                          const std::string& param) const {
    auto it = bySource_.find(source);
    if (it == bySource_.end()) return -1;
    for (int idx : it->second) {
        if (maps_[idx].param == param) return idx;
    }
    return -1;
}

void MappingTable::addRow(const Mapping& m) {
    maps_.push_back(m);
    maps_.back().mod = isModSource(m.source);
    reindex();
}

void MappingTable::removeRow(int index) {
    if (index < 0 || index >= static_cast<int>(maps_.size())) return;
    maps_.erase(maps_.begin() + index);
    reindex();
}

void MappingTable::clearSource(const std::string& source) {
    maps_.erase(std::remove_if(maps_.begin(), maps_.end(),
                               [&source](const Mapping& m) {
                                   return m.source == source;
                               }),
                maps_.end());
    reindex();
}

int MappingTable::countFor(const std::string& source) const {
    auto it = bySource_.find(source);
    return it == bySource_.end() ? 0 : static_cast<int>(it->second.size());
}

std::vector<int> MappingTable::rowsFor(const std::string& source) const {
    auto it = bySource_.find(source);
    return it == bySource_.end() ? std::vector<int>{} : it->second;
}

bool MappingTable::setRange(int index, float lo, float hi) {
    if (index < 0 || index >= static_cast<int>(maps_.size())) return false;
    maps_[index].lo = lo;
    maps_[index].hi = hi;
    return true;
}

std::vector<std::string> MappingTable::danglingParams(const ParamBank& bank) const {
    std::vector<std::string> out;
    for (const auto& m : maps_) {
        if (bank.find(m.param) < 0) {
            if (std::find(out.begin(), out.end(), m.param) == out.end()) {
                out.push_back(m.param);
            }
        }
    }
    return out;
}

}  // namespace nv

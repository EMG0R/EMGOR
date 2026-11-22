// OSC 1.0 message parsing, and the two message shapes this end sends.
//
// Hand-rolled rather than pulled in: the subset the control bus uses is a few
// dozen lines, and a dependency here would have to be cross-compiled for the
// Pi for no benefit.
//
// Wire format, all big-endian, everything padded to 4-byte boundaries:
//   address  null-terminated string, padded
//   typetag  null-terminated string starting ',', padded
//   args     packed per the type tag
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nv {

struct OscArg {
    enum class Type { Int, Float, String } type = Type::Int;
    int32_t i = 0;
    float f = 0.0f;
    std::string s;

    // Numeric value whatever the sender chose to encode it as. Senders are
    // inconsistent about int-vs-float for what is conceptually one number,
    // and a receiver that cares about the difference is a receiver that
    // breaks the first time someone changes a literal.
    float number() const {
        switch (type) {
            case Type::Float: return f;
            case Type::Int: return static_cast<float>(i);
            default: return 0.0f;
        }
    }
};

struct OscMessage {
    std::string address;
    std::vector<OscArg> args;
};

// Parses one packet. Handles #bundle by flattening it into `out`. Returns
// false on anything malformed, having appended nothing.
bool oscParse(const uint8_t* data, size_t len, std::vector<OscMessage>& out);

// ---- encoding --------------------------------------------------------------
//
// Only the two shapes the engine actually emits, because a general OSC writer
// here would be dead code with its own bugs. Both are round-tripped through
// oscParse() in the tests, which is the only definition of "correct" that
// matters: whatever we write, our own parser -- and demiurge_web's, which
// pads identically -- must read back unchanged.

// `<address> ,sf <s> <f>` -- the state broadcast's one message.
std::vector<uint8_t> oscMessage(const std::string& address,
                                const std::string& s, float f);

// `<address> ,ii <a> <b>` -- the selection line that rides with it.
std::vector<uint8_t> oscMessage(const std::string& address, int32_t a,
                                int32_t b);

// `<address> ,si <s> <i>` -- a list terminator, `<what> <count>`.
std::vector<uint8_t> oscMessage(const std::string& address,
                                const std::string& s, int32_t i);

// `<address> ,iiiiiii ...` -- the selection line, which grew past two ints
// the day a remote surface needed to light the right button in a grid of
// thirty rather than print an index. Appending is safe: a reader built for
// two arguments still reads the same two.
std::vector<uint8_t> oscMessageInts(const std::string& address,
                                    const std::vector<int32_t>& v);

// `<address> ,siss <what> <index> <id> <name>` -- one row of an enumeration.
// The name is on the wire because otherwise every surface has to carry its
// own copy of the bank and go stale the day one entry is added.
std::vector<uint8_t> oscListItem(const std::string& address,
                                 const std::string& what, int32_t index,
                                 const std::string& id,
                                 const std::string& name);

// Packs messages into one #bundle with an immediate time tag. Bundling is not
// decoration: 45 parameters as 45 datagrams is 45 syscalls and 45 chances to
// arrive interleaved with the next tick's values.
std::vector<uint8_t> oscBundle(const std::vector<std::vector<uint8_t>>& msgs);

}  // namespace nv

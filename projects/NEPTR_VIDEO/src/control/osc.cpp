#include "control/osc.h"

#include <cstring>

namespace nv {
namespace {

struct Reader {
    const uint8_t* p;
    size_t len;
    size_t pos = 0;

    bool ok(size_t n) const { return pos + n <= len; }

    bool readU32(uint32_t& v) {
        if (!ok(4)) return false;
        v = (static_cast<uint32_t>(p[pos]) << 24) |
            (static_cast<uint32_t>(p[pos + 1]) << 16) |
            (static_cast<uint32_t>(p[pos + 2]) << 8) |
            static_cast<uint32_t>(p[pos + 3]);
        pos += 4;
        return true;
    }

    bool readI32(int32_t& v) {
        uint32_t u = 0;
        if (!readU32(u)) return false;
        std::memcpy(&v, &u, 4);
        return true;
    }

    bool readF32(float& v) {
        uint32_t u = 0;
        if (!readU32(u)) return false;
        std::memcpy(&v, &u, 4);
        return true;
    }

    // OSC strings are null-terminated and padded to a 4-byte boundary, with
    // at least one null always present.
    bool readString(std::string& s) {
        size_t start = pos;
        while (pos < len && p[pos] != 0) ++pos;
        if (pos >= len) return false;
        s.assign(reinterpret_cast<const char*>(p + start), pos - start);
        size_t total = pos - start + 1;
        pos = start + ((total + 3) & ~static_cast<size_t>(3));
        return pos <= len;
    }

    bool skipBlob() {
        int32_t n = 0;
        if (!readI32(n) || n < 0) return false;
        size_t padded = (static_cast<size_t>(n) + 3) & ~static_cast<size_t>(3);
        if (!ok(padded)) return false;
        pos += padded;
        return true;
    }
};

bool parseMessage(Reader& r, std::vector<OscMessage>& out) {
    OscMessage m;
    if (!r.readString(m.address)) return false;
    if (m.address.empty() || m.address[0] != '/') return false;

    std::string tags;
    if (!r.readString(tags)) return false;
    if (tags.empty() || tags[0] != ',') return false;

    for (size_t i = 1; i < tags.size(); ++i) {
        OscArg a;
        switch (tags[i]) {
            case 'i':
                a.type = OscArg::Type::Int;
                if (!r.readI32(a.i)) return false;
                break;
            case 'f':
                a.type = OscArg::Type::Float;
                if (!r.readF32(a.f)) return false;
                break;
            case 's':
            case 'S':
                a.type = OscArg::Type::String;
                if (!r.readString(a.s)) return false;
                break;
            case 'b':
                // Blobs are skipped rather than rejected: an unknown argument
                // must not invalidate an otherwise usable message.
                if (!r.skipBlob()) return false;
                continue;
            case 'T':
                a.type = OscArg::Type::Int;
                a.i = 1;
                break;
            case 'F':
                a.type = OscArg::Type::Int;
                a.i = 0;
                break;
            case 'N':
            case 'I':
                continue;  // nil / infinitum carry no data
            default:
                return false;  // an unknown tag means unknown widths ahead
        }
        m.args.push_back(std::move(a));
    }
    out.push_back(std::move(m));
    return true;
}

}  // namespace

bool oscParse(const uint8_t* data, size_t len, std::vector<OscMessage>& out) {
    if (!data || len < 4) return false;
    const size_t before = out.size();

    if (len >= 8 && std::memcmp(data, "#bundle", 7) == 0 && data[7] == 0) {
        Reader r{data, len, 16};  // skip "#bundle\0" + the 8-byte time tag
        while (r.pos + 4 <= r.len) {
            int32_t size = 0;
            if (!r.readI32(size) || size < 0) break;
            if (!r.ok(static_cast<size_t>(size))) break;
            // Nested bundles are legal; recursing keeps that free.
            oscParse(data + r.pos, static_cast<size_t>(size), out);
            r.pos += static_cast<size_t>(size);
        }
        return out.size() > before;
    }

    Reader r{data, len, 0};
    if (!parseMessage(r, out)) {
        out.resize(before);  // append nothing on a malformed packet
        return false;
    }
    return true;
}

// ---- encoding --------------------------------------------------------------

namespace {

void putU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v >> 24));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}

// Null-terminated, then padded to a 4-byte boundary. The null is ALWAYS
// written, so a string whose length is already a multiple of 4 still grows by
// four bytes -- getting that wrong shifts every argument after it and shows up
// as garbage values rather than as a parse error.
void putStr(std::vector<uint8_t>& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    while (b.size() % 4 != 0) b.push_back(0);
}

}  // namespace

std::vector<uint8_t> oscMessage(const std::string& address,
                                const std::string& s, float f) {
    std::vector<uint8_t> b;
    putStr(b, address);
    putStr(b, ",sf");
    putStr(b, s);
    uint32_t u;
    std::memcpy(&u, &f, 4);
    putU32(b, u);
    return b;
}

std::vector<uint8_t> oscMessage(const std::string& address, int32_t a,
                                int32_t b) {
    std::vector<uint8_t> out;
    putStr(out, address);
    putStr(out, ",ii");
    putU32(out, static_cast<uint32_t>(a));
    putU32(out, static_cast<uint32_t>(b));
    return out;
}

std::vector<uint8_t> oscMessage(const std::string& address,
                                const std::string& s, int32_t i) {
    std::vector<uint8_t> b;
    putStr(b, address);
    putStr(b, ",si");
    putStr(b, s);
    putU32(b, static_cast<uint32_t>(i));
    return b;
}

std::vector<uint8_t> oscMessageInts(const std::string& address,
                                    const std::vector<int32_t>& v) {
    std::vector<uint8_t> out;
    putStr(out, address);
    std::string tags = ",";
    tags.append(v.size(), 'i');
    putStr(out, tags);
    for (int32_t x : v) putU32(out, static_cast<uint32_t>(x));
    return out;
}

std::vector<uint8_t> oscListItem(const std::string& address,
                                 const std::string& what, int32_t index,
                                 const std::string& id,
                                 const std::string& name) {
    std::vector<uint8_t> b;
    putStr(b, address);
    putStr(b, ",siss");
    putStr(b, what);
    putU32(b, static_cast<uint32_t>(index));
    putStr(b, id);
    putStr(b, name);
    return b;
}

std::vector<uint8_t> oscBundle(const std::vector<std::vector<uint8_t>>& msgs) {
    std::vector<uint8_t> b;
    putStr(b, "#bundle");
    putU32(b, 0);
    putU32(b, 1);  // immediate: act on it now, there is no scheduler here
    for (const auto& m : msgs) {
        putU32(b, static_cast<uint32_t>(m.size()));
        b.insert(b.end(), m.begin(), m.end());
    }
    return b;
}

}  // namespace nv

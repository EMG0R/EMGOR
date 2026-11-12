// pmor_cache.h -- the PMOR sample cache (PROTOCOL section 8 / EMGOR spec: "Write to <Live set>/PMOR/, fall back to ~/Music/PMOR_Cache/. Read session first, then cache").
// Header-only, no Max: used by the pmor.proto external and tested by tests/test_cache.cpp.
//   session dir  = <Live set folder>/PMOR (chosen by pmor_where.js, may be empty = unknown / unsaved set)
//   cache dir    = ~/Music/PMOR_Cache (or whatever `cachedir` set)
//   find(): session first, then cache;  store(): the session dir when it is usable (created on demand), else the cache; temp file + rename.
// A file is named <name>_<crc8hex>.aud. Pool ids (>= 1024): name = S<id>, crc = PMOR's manifest CRC-32 of the PCM. Legacy ids (< 1024): the manifest `ver` is 0 / a timestamp
// (untrusted), so the owner (pmor_slotsync.h) names the file by the CRC-32 the HOST computed of the downloaded bytes and never reuses it from disk.
// find() counts a file as present only when its size is exactly len*2 bytes AND (verify, default) the CRC-32 of its content equals the crc in its name;
// a file whose content CRC differs is renamed <file>.bad (quarantined, never used again) and the search goes on in the other directory.
// Two files of one id with different crc coexist (different names); only the exact crc is ever used; session-vs-cache precedence cannot matter.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string>
#include <vector>

namespace pmorcache {

inline uint32_t crc32(const uint8_t* p, size_t n) {
    static uint32_t t[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); t[i] = c; } init = true; }
    uint32_t c = 0xFFFFFFFFu; for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8); return ~c;
}

struct Cache {
    std::string session, cache;
    std::string cacheRoot() const { if (!cache.empty()) return cache; const char* h = getenv("HOME"); return std::string(h ? h : ".") + "/Music/PMOR_Cache"; }
    static std::string expand(const std::string& p) { if (!p.empty() && p[0] == '~') { const char* h = getenv("HOME"); return std::string(h ? h : "") + p.substr(1); } return p; }
    static std::string fileName(const std::string& name, uint32_t ver) { char b[128]; snprintf(b, sizeof b, "%s_%08X.aud", name.c_str(), (unsigned)ver); return b; }
    static bool sizeIs(const std::string& f, uintmax_t want) { struct stat st; return stat(f.c_str(), &st) == 0 && S_ISREG(st.st_mode) && (uintmax_t)st.st_size == want; }
    static bool makeDirs(const std::string& d) {          // mkdir -p
        if (d.empty()) return false;
        for (size_t i = 1; i <= d.size(); i++)
            if (i == d.size() || d[i] == '/') { std::string p = d.substr(0, i); if (mkdir(p.c_str(), 0755) != 0 && errno != EEXIST) return false; }
        struct stat st; return stat(d.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    static std::string poolName(int id) { return "S" + std::to_string(id); }
    static bool readAll(const std::string& f, std::vector<uint8_t>& b) {
        FILE* h = fopen(f.c_str(), "rb"); if (!h) return false; b.clear(); uint8_t t[8192]; size_t k;
        while ((k = fread(t, 1, sizeof t, h)) > 0) b.insert(b.end(), t, t + k);
        fclose(h); return true;
    }
    // rename f -> f.bad (an older .bad is replaced); if it cannot be moved aside it is removed (a bad file must never stay usable)
    static void quarantine(const std::string& f) { std::string b = f + ".bad"; remove(b.c_str()); if (rename(f.c_str(), b.c_str()) != 0) remove(f.c_str()); }
    mutable std::vector<std::string> quarantined;                 // every file find() moved aside (for the owner to report / tests)
    std::string find(const std::string& name, uint32_t crc, uint32_t lenSamples, bool verify = true) const {
        for (const std::string& d : {session, cacheRoot()}) {
            if (d.empty()) continue;
            std::string f = d + "/" + fileName(name, crc);
            if (!sizeIs(f, (uintmax_t)lenSamples * 2)) continue;
            if (verify) { std::vector<uint8_t> b; if (!readAll(f, b) || crc32(b.data(), b.size()) != crc) { quarantine(f); quarantined.push_back(f); continue; } }
            return f;
        }
        return {};
    }
    // returns the final path ("" on failure); *usedSession tells which directory won
    std::string store(const std::string& name, uint32_t ver, const std::vector<uint8_t>& data, bool* usedSession = nullptr) const {
        bool first = true;
        for (const std::string& d : {session, cacheRoot()}) {
            bool isSession = first; first = false;
            if (d.empty() || !makeDirs(d)) continue;
            std::string fin = d + "/" + fileName(name, ver), tmp = fin + ".part";
            FILE* o = fopen(tmp.c_str(), "wb"); if (!o) continue;
            bool w = fwrite(data.data(), 1, data.size(), o) == data.size(); w = (fclose(o) == 0) && w;
            if (w && rename(tmp.c_str(), fin.c_str()) == 0) { if (usedSession) *usedSession = isSession; return fin; }
            remove(tmp.c_str());
        }
        return {};
    }
};

}  // namespace pmorcache

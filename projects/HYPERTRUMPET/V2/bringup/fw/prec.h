// Persisted parameter record (QSPI, PersistentStorage). Pure C++, no libDaisy: also compiled by the host test (tests/host_prec_test.cpp).
// Keyed by NAME: every entry is {fnv1a("<effect id>:<PARAM NAME>"), value}. When an effect's parameter set changes (names added, renamed, removed) the unknown keys are
// ignored and the missing ones keep their defaults, so values can never land in the wrong parameter. A layout change bumps PREC_VERSION (-> all defaults).
// Load rule: wrong magic, wrong version, bad CRC, a count out of range or a non-finite value means "use the defaults".
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
static const uint32_t PREC_MAGIC = 0x4F475233u;           // "OGR3"
static const uint32_t PREC_VERSION = 4;
enum { PREC_MAX = 192 };      // 14 effects, ~130 params
struct ParamEntry { uint32_t key; float val; };
struct ParamRec {
    uint32_t magic, version; int32_t db10; uint32_t count;
    uint16_t flo[3], fhi[3]; uint32_t fvalid;     // fader endpoint calibration: raw ADC lo/hi per fader (fvalid = 1 once learned or set)
    ParamEntry e[PREC_MAX]; uint32_t crc;
    bool operator==(const ParamRec& o) const { return memcmp(this, &o, sizeof(ParamRec)) == 0; }
    bool operator!=(const ParamRec& o) const { return !(*this == o); }
};
static inline uint32_t prec_fnv(const char* fx, const char* name) { uint32_t h = 2166136261u; for(const char* p = fx; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; } h ^= ':'; h *= 16777619u; for(const char* p = name; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; } return h; }
static inline uint32_t prec_crc32(const uint8_t* d, size_t n) { uint32_t c = 0xFFFFFFFFu; for(size_t i = 0; i < n; i++) { c ^= d[i]; for(int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); } return ~c; }
static inline void prec_clear(ParamRec& r) { memset(&r, 0, sizeof r); }
static inline bool prec_add(ParamRec& r, uint32_t key, float v) { if(r.count >= PREC_MAX) return false; r.e[r.count].key = key; r.e[r.count].val = v; r.count++; return true; }
static inline void prec_seal(ParamRec& r) { r.magic = PREC_MAGIC; r.version = PREC_VERSION; r.crc = prec_crc32((const uint8_t*)&r, offsetof(ParamRec, crc)); }
static inline bool prec_valid(const ParamRec& r) {
    if(r.magic != PREC_MAGIC || r.version != PREC_VERSION || r.count > PREC_MAX) return false;
    if(r.crc != prec_crc32((const uint8_t*)&r, offsetof(ParamRec, crc))) return false;
    if(r.db10 < -400 || r.db10 > 360) return false;
    for(uint32_t i = 0; i < r.count; i++) if(!(fabsf(r.e[i].val) < 1e6f)) return false;
    return true;
}
static inline bool prec_find(const ParamRec& r, uint32_t key, float* out) { for(uint32_t i = 0; i < r.count; i++) if(r.e[i].key == key) { *out = r.e[i].val; return true; } return false; }

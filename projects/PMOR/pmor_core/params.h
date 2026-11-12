// params.h -- the param schema: every engine and FX parameter as {id, name, min, max, default}.
// One table, built from the engines' own ParamPages (so it can never drift from what the OLED
// editor shows) plus the few engine values that are not paged (drum sound, algo, FX engage...).
// UI, presets, sync and the Max device read this; nothing else defines parameter ranges.
#pragma once
#include <stdint.h>
#include <stddef.h>

// id = (domain << 10) | index. Stable: never renumber, only append.
enum PmorDomain : uint8_t {
    PD_SYNTH = 1, PD_DRUMS, PD_SAMPLER, PD_SEQ, PD_FX, PD_GLOBAL,
    // drum synth engine: every synthesis parameter of every pad. Pad p lives in domain PD_DRUMSYN0 + p/4, index (p%4)*256 + k (k = SDEF_PARAMS order, drum_super.cpp; append-only).
    PD_DRUMSYN0 = 7, PD_DRUMSYN1, PD_DRUMSYN2, PD_DRUMSYN3,
    PD_COUNT
};

struct PmorParamDesc {
    uint16_t    id;
    const char* name;     // <= 8 chars, upper case (same strings the OLED shows)
    float       min, max;
    float       def;      // value right after the engine inits, i.e. before any patch loads
    uint8_t     flags;    // ParamFlags (param.h): PF_QUANT / PF_ENUM / PF_INT / ...
    const char* group;    // "SYN.THIEF", "DRUM.PAD3", "SEQ.SYNTH", "FX3", ...
};

static inline uint16_t pmorParamId(PmorDomain d, uint16_t index) { return (uint16_t)(((uint16_t)d << 10) | (index & 0x3FFu)); }

// Build the table. Call once after every engine's *Init() and before any patch load (hw/boot.cpp does).
void pmorSchemaInit(void);
size_t pmorSchemaCount(void);
const PmorParamDesc* pmorSchemaAt(size_t i);
const PmorParamDesc* pmorSchemaFind(uint16_t id);          // nullptr if absent
// Live value of a schema parameter, and a clamped write (control plane). false if the id is unknown.
bool pmorParamGet(uint16_t id, float* out);
bool pmorParamSet(uint16_t id, float v);
int  pmorSchemaIndexOf(uint16_t id);                      // table index of an id, or -1
bool pmorParamGetAt(size_t i, float* out);              // live value by table index (no search)
void pmorParamAfterSet(uint16_t id, float v);           // side effects an edit needs (tempo, scale rebuild) -- pmorParamSet calls it

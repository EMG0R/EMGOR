// test_schema.cpp -- the param schema is complete and well-formed.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <set>
#include <string>
#include "params.h"
#include "proto_util.h"
#include "param.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "fx_v2.h"
#include "output.h"
#include "metro.h"
#include "drum_super.h"

static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

int main() {
    memset(&g, 0, sizeof(g));
    clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
    size_t synthN = 0; for (int p = 0; p < N_PADS; p++) for (size_t k = 0; k < SDEF_NPARAMS; k++) if (sdefUsed(drumPadGroup((uint8_t)p), k)) synthN++;
    size_t n = pmorSchemaCount();
    printf("schema entries: %zu\n", n);
    // each engine shows its own set: KICK and HAT must differ, and kick pitch-sweep / hat band names exist
    { int cnt[4] = {0,0,0,0}; for (int g2 = 0; g2 < 4; g2++) for (size_t k = 0; k < SDEF_NPARAMS; k++) cnt[g2] += sdefUsed(g2, k); printf("engine params: KICK %d SNARE %d HAT %d PERC %d\n", cnt[0], cnt[1], cnt[2], cnt[3]);
      CHECK(cnt[0] < (int)SDEF_NPARAMS && cnt[2] < (int)SDEF_NPARAMS && cnt[0] != cnt[2], "engines are not distinct"); }
    CHECK(n > 400 && n <= 3072, "entry count %zu out of expected range", n);

    std::set<uint16_t> ids;
    int perDomain[PD_COUNT] = {0};
    for (size_t i = 0; i < n; i++) {
        const PmorParamDesc* d = pmorSchemaAt(i);
        CHECK(ids.insert(d->id).second, "duplicate id 0x%04x (%s)", d->id, d->name ? d->name : "?");
        CHECK(d->name && d->name[0], "id 0x%04x: empty name", d->id);
        CHECK(d->name && strlen(d->name) <= 8, "id 0x%04x: name '%s' longer than 8", d->id, d->name);
        CHECK(d->group && d->group[0], "id 0x%04x: empty group", d->id);
        CHECK(d->min < d->max, "id 0x%04x %s: min %g !< max %g", d->id, d->name, d->min, d->max);
        CHECK(d->def >= d->min - 1e-4f && d->def <= d->max + 1e-4f, "id 0x%04x %s.%s: default %g outside [%g,%g]", d->id, d->group, d->name, d->def, d->min, d->max);
        CHECK(isfinite(d->min) && isfinite(d->max) && isfinite(d->def), "id 0x%04x: non-finite", d->id);
        unsigned dom = d->id >> 10; CHECK(dom >= 1 && dom < PD_COUNT, "id 0x%04x: bad domain", d->id);
        if (dom < PD_COUNT) perDomain[dom]++;
        float v = -12345.f;
        CHECK(pmorParamGet(d->id, &v), "id 0x%04x: not gettable", d->id);
        CHECK(fabsf(v - d->def) < 1e-4f, "id 0x%04x %s: live value %g != default %g right after init", d->id, d->name, v, d->def);
        CHECK(pmorSchemaFind(d->id) == d, "id 0x%04x: Find mismatch", d->id);
    }
    // ---- ADDITIVE: every id that existed before the synth LFO (PD_SYNTH 258..261) and the drum deep recipe knobs (PD_DRUMS 320..383) was added keeps its
    // name/range/default/flags/group, and the table order of the old entries is unchanged: the schema hash over the OLD entries alone is the old hash.
    {
        uint32_t h = 2166136261u; char line[160]; size_t old = 0;
        for (size_t i = 0; i < n; i++) {
            const PmorParamDesc* d = pmorSchemaAt(i);
            unsigned dom = d->id >> 10, idx = d->id & 0x3FF;
            if (dom >= PD_DRUMSYN0) continue;   // drum synth engine params (additive domains 7..10)
            if ((dom == PD_SYNTH && idx >= 258 && idx <= 261) || (dom == PD_DRUMS && idx >= 320 && idx < 384) || (dom == PD_GLOBAL && idx == 7)) continue;
            char a[24], b[24], c[24]; pmorFmtFloat(a, d->min); pmorFmtFloat(b, d->max); pmorFmtFloat(c, d->def);
            int m = snprintf(line, sizeof line, "%u %s %s %s %s %u %s\n", (unsigned)d->id, d->name, a, b, c, (unsigned)d->flags, d->group);
            h = pmorFnv1a(h, line, (size_t)m); old++;
        }
        CHECK(old == 612 && h == 0x776F12EAu, "the 612 pre-existing entries changed: count %zu hash %08X (want 612 / 776F12EA)", old, (unsigned)h);
        CHECK(n == old + 4 + 1 + DG_COUNT * DDEEP_PARAMS + synthN, "new entries: %zu (want 4 LFO + 1 CLKLEAD + %d deep + %d drum synth)", n - old, DG_COUNT * DDEEP_PARAMS, (int)synthN);
    }
    // ---- completeness against the engines' own tables
    for (int id = 0; id < FX_TOTAL; id++) {                       // every FX (8 banks x 16)
        const PmorParamDesc* d = pmorSchemaFind(pmorParamId(PD_FX, (uint16_t)id));
        CHECK(d != nullptr, "FX %d missing", id);
        if (d) CHECK(!strcmp(d->name, FX_V2_NAMES[id >> 4][id & 15]) && strcmp(d->name, "?"), "FX %d name '%s'", id, d->name);
    }
    for (int a = 0; a < ALG_COUNT; a++) {                         // every synth algorithm: 4 knobs + 4 env + 7 filter + 3 pitch + 2 width
        CHECK(SYN_ALGO_NAMES[a] && SYN_ALGO_NAMES[a][0], "synth algo %d unnamed", a);
        int have = 0;
        for (int k = 0; k < 32; k++) if (pmorSchemaFind(pmorParamId(PD_SYNTH, (uint16_t)(a * 32 + k)))) have++;
        CHECK(have == 20, "synth algo %d has %d params, want 20", a, have);
    }
    CHECK(pmorSchemaFind(pmorParamId(PD_SYNTH, 256)) && pmorSchemaFind(pmorParamId(PD_SYNTH, 257)), "synth ALGO/VOLUME missing");
    for (int gi = 0; gi < DG_COUNT; gi++) for (int s = 0; s < DRUM_SOUNDS; s++)
        CHECK(DRUM_SOUND_NAMES[gi][s] && DRUM_SOUND_NAMES[gi][s][0], "drum sound %d/%d unnamed", gi, s);
    for (int p = 0; p < N_PADS; p++) for (int k = 0; k < 6; k++)   // length pitch tone rev vol sound
        CHECK(pmorSchemaFind(pmorParamId(PD_DRUMS, (uint16_t)(p * 8 + k))) != nullptr, "drum pad %d param %d missing", p, k);
    for (int k = 0; k < 4; k++) CHECK(pmorSchemaFind(pmorParamId(PD_DRUMS, (uint16_t)(256 + k))), "drum group vol %d missing", k);
    // drum synth engine: every pad has every SuperDef param, at domain 7 + pad/4, index (pad%4)*256 + k, named + ranged from SDEF_PARAMS, live value = the pad's preset
    for (int p = 0; p < N_PADS; p++) for (size_t k = 0; k < SDEF_NPARAMS; k++) {
        if (!sdefUsed(drumPadGroup((uint8_t)p), k)) { CHECK(!pmorSchemaFind(pmorParamId((PmorDomain)(PD_DRUMSYN0 + p / 4), (uint16_t)((p % 4) * 256 + k))), "pad %d field %zu is not in its engine but is in the schema", p, k); continue; }
        const PmorParamDesc* d = pmorSchemaFind(pmorParamId((PmorDomain)(PD_DRUMSYN0 + p / 4), (uint16_t)((p % 4) * 256 + k)));
        CHECK(d != nullptr, "drum synth pad %d param %zu missing", p, k);
        if (d) { CHECK(!strcmp(d->name, sdefName(drumPadGroup((uint8_t)p), k)) && d->min == SDEF_PARAMS[k].mn && d->max == SDEF_PARAMS[k].mx, "drum synth pad %d param %zu (%s) range/name", p, k, d->name);
                 CHECK(d->group && !strncmp(d->group, SDEF_ENGINE_NAMES[drumPadGroup((uint8_t)p)], strlen(SDEF_ENGINE_NAMES[drumPadGroup((uint8_t)p)])) && strchr(d->group, '.'), "drum synth pad %d param %zu group '%s'", p, k, d->group ? d->group : "?"); }
    }
    for (int s = 0; s < SMP_SLOTS; s++) for (int k = 0; k < 4; k++)
        CHECK(pmorSchemaFind(pmorParamId(PD_SAMPLER, (uint16_t)(s * 8 + k))) != nullptr, "sampler slot %d param %d missing", s, k);
    for (int e = 0; e < ENG_COUNT; e++) {
        int have = 0; for (int k = 0; k < 32; k++) if (pmorSchemaFind(pmorParamId(PD_SEQ, (uint16_t)(e * 32 + k)))) have++;
        CHECK(have >= 6, "sequencer engine %d has only %d params", e, have);
    }
    for (int k = 0; k <= 6; k++) CHECK(pmorSchemaFind(pmorParamId(PD_GLOBAL, (uint16_t)k)), "global %d missing", k);
    for (int dom = 1; dom < PD_COUNT; dom++) CHECK(perDomain[dom] > 0, "domain %d empty", dom);

    // ---- set/get: every writable param accepts a clamp-and-quantise write, and the default survives a round trip
    for (size_t i = 0; i < n; i++) {
        const PmorParamDesc* d = pmorSchemaAt(i);
        CHECK(pmorParamSet(d->id, d->max + 100.f), "id 0x%04x: set failed", d->id);
        float v; pmorParamGet(d->id, &v);
        if ((d->id >> 10) != PD_FX) CHECK(v <= d->max + 1e-4f, "id 0x%04x %s: set above max stored %g", d->id, d->name, v);
        pmorParamSet(d->id, d->def);
    }
    CHECK(!pmorParamSet(0xFFFF, 1.f), "unknown id accepted");
    printf("domains: synth %d drums %d sampler %d seq %d fx %d global %d\n", perDomain[PD_SYNTH], perDomain[PD_DRUMS], perDomain[PD_SAMPLER], perDomain[PD_SEQ], perDomain[PD_FX], perDomain[PD_GLOBAL]);
    printf(fails ? "test_schema: %d FAILURES\n" : "test_schema: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}

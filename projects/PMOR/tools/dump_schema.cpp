// dump_schema.cpp -- prints the param schema + engine/preset/FX names as JSON (host build). max/devices/schema.json is its output;
// tests/test_devices_schema.py fails if the committed file drifts from the firmware (ids are stable forever, but ranges/names can change).
//   build_host/dump_schema > max/devices/schema.json
#include <stdio.h>
#include <string.h>
#include "params.h"
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
#include "proto.h"
static void strs(const char* const* a, int n) { printf("["); for (int i = 0; i < n; i++) printf("%s\"%s\"", i ? "," : "", a[i]); printf("]"); }
int main() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
    static const ProtoOps none = {}; protoInit(&none);       // computes the schema hash
    printf("{\n\"schema_hash\": \"%08X\",\n\"schema\": [\n", (unsigned)protoSchemaHash());
    for (size_t i = 0; i < pmorSchemaCount(); i++) { const PmorParamDesc* d = pmorSchemaAt(i);
        printf("%s[%u,\"%s\",%g,%g,%g,%u,\"%s\"]", i ? ",\n" : "", d->id, d->name, d->min, d->max, d->def, d->flags, d->group); }
    printf("\n],\n\"synth_algos\": "); strs(SYN_ALGO_NAMES, ALG_COUNT);
    printf(",\n\"synth_presets\": "); strs(SYN_PRESET_NAMES, SYN_PRESETS);
    printf(",\n\"drum_groups\": "); strs(DRUM_GROUP_NAMES, DG_COUNT);
    printf(",\n\"drum_sounds\": [");
    for (int g2 = 0; g2 < DG_COUNT; g2++) { printf("%s", g2 ? "," : ""); strs(DRUM_SOUND_NAMES[g2], DRUM_SOUNDS); }
    printf("],\n\"fx_banks\": [");
    for (int b = 0; b < 8; b++) { printf("%s", b ? "," : ""); strs(FX_V2_NAMES[b], 16); }
    printf("],\n\"scales\": "); strs(SCALE_MODE_NAMES, SCALE_MODES);
    printf("\n}\n");
    return 0;
}

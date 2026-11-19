#include "../fx_registry.h"
#include "fx_testutil.h"
// prints "BENCH <id> <ns/sample>" for every registered effect (mono-in; stereo-in line too)
int main() {
    for (int i = 0; i < hx::fxTypeCount(); i++) {
        const hx::FxTypeInfo* t = hx::fxType(i);
        hxt::benchNsPerSample(t->create, t->id, false);
        char nm[32]; snprintf(nm, sizeof nm, "%s_st", t->id);
        hxt::benchNsPerSample(t->create, nm, true);
    }
    return 0;
}

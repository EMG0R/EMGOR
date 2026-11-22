// Host test of the persisted record: seal, round trip, bad CRC, version/magic mismatch, name-keyed lookup when the param set changes, old layouts, erased flash.
// Build: c++ -std=c++14 -I. tests/host_prec_test.cpp -o /tmp/prec_test && /tmp/prec_test     (run from bringup/fw)
#include <stdio.h>
#include "prec.h"
static int fails = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while(0)
int main() {
    ParamRec r; prec_clear(r); r.db10 = -120;
    prec_add(r, prec_fnv("reverb", "DECAY"), 0.9f); prec_add(r, prec_fnv("reverb", "SIZE"), 1.6f); prec_add(r, prec_fnv("drone", "PARTIALS"), 6.f); prec_seal(r);
    CHECK(prec_valid(r), "sealed record is valid");
    ParamRec c = r; CHECK(prec_valid(c) && c == r, "copy round trip");
    float v = 0; CHECK(prec_find(r, prec_fnv("reverb", "SIZE"), &v) && v == 1.6f, "lookup by name returns the saved value");
    CHECK(!prec_find(r, prec_fnv("drone", "SHIMMER"), &v), "a param added later is simply not found (keeps its default)");
    CHECK(!prec_find(r, prec_fnv("reverb", "PARTIALS"), &v), "same param name under another effect does not collide");
    CHECK(prec_find(r, prec_fnv("drone", "PARTIALS"), &v) && v == 6.f, "a param removed from the effect is just never asked for; the others still load");
    c = r; c.e[1].val += 0.001f; CHECK(!prec_valid(c), "flipped value -> bad CRC -> defaults");
    c = r; ((uint8_t*)&c)[30] ^= 0x10; CHECK(!prec_valid(c), "flipped bit -> bad CRC");
    c = r; c.version = 3; CHECK(!prec_valid(c), "version mismatch -> defaults");
    c = r; c.magic ^= 1; CHECK(!prec_valid(c), "wrong magic -> defaults");
    c = r; c.count = 999; prec_seal(c); CHECK(!prec_valid(c), "count out of range even with a good CRC");
    c = r; c.db10 = 500; prec_seal(c); CHECK(!prec_valid(c), "out-of-range gain even with a good CRC");
    c = r; c.e[0].val = NAN; prec_seal(c); CHECK(!prec_valid(c), "NaN param even with a good CRC");
    uint8_t flash[sizeof(ParamRec)]; memset(flash, 0xFF, sizeof flash); ParamRec old; memcpy(&old, flash, sizeof old); CHECK(!prec_valid(old), "blank (erased) flash -> defaults");
    uint32_t m1 = 0x4F475231u; int32_t d1 = -120; memcpy(flash, &m1, 4); memcpy(flash + 4, &d1, 4); memcpy(&old, flash, sizeof old); CHECK(!prec_valid(old), "fw 1.2 record (8 bytes) + erased flash -> defaults");
    r.fvalid = 1; r.flo[1] = 1234; r.fhi[1] = 60000; prec_seal(r); CHECK(prec_valid(r) && r.flo[1] == 1234, "fader calibration fields are covered by the CRC"); c = r; c.fhi[1] ^= 1; CHECK(!prec_valid(c), "flipped fader endpoint -> bad CRC");
    printf("sizeof(ParamRec) = %zu\n", sizeof(ParamRec)); return fails;
}

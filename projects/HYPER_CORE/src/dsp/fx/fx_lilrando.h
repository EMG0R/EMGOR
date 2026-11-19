// fx_lilrando.h -- v1 `p lil rando` (PERFRMNC.maxpat): metro 4n -> random 0..1 -> scale 0..127 -> flonum -> `line` with a 400 ms ramp. The subpatcher has NO inlets and NO outlets
// and nothing in the top-level patch connects to it (traced 2026-11-17), so in v1 it modulates nothing. Kept as a control-rate source (not an audio effect, not in the registry):
// tick(dt) returns the ramped value 0..1 (v1 scale 0..127). Nothing in the firmware reads it, matching v1.
#pragma once
#include "fx_common.h"
namespace hx {
struct LilRando {
    float cur = 0.f, tgt = 0.f, tim = 0.f, bar = 0.5f; uint32_t rng = 0x11ABD0u;      // bar: metro 4n period in seconds (120 bpm quarter note)
    float tick(float dt) { tim += dt; if (tim >= bar) { tim -= bar; tgt = rnd01(rng); } cur += (tgt - cur) * (dt / 0.4f > 1.f ? 1.f : dt / 0.4f); return cur; }
};
}

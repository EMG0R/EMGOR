// The effect bank.
//
// Effects are classified by ONE property, and it is the only property that
// matters for performance: what does this effect need to read?
//
//   Pointwise  reads only the colour already in hand. Costs arithmetic.
//   Sampler    must fetch the input texture at a uv it chooses (blur, edge,
//              pixelate, mirror) or must fetch the previous frame (feedback,
//              trails). Costs a texture read, and forces a pass boundary if
//              the pass already has one.
//
// That distinction is the whole basis of pass fusion: a twelve-effect chain
// of pointwise colour work is ONE full-screen pass, not twelve.
#pragma once

#include <string>
#include <vector>

#include "state.h"

namespace nv {

enum class EffectKind {
    Pointwise,  // fuses freely
    Sampler,    // reads uInput at arbitrary uv; opens a pass
    History,    // reads uHistory (previous frame); opens a pass
};

struct EffectParam {
    std::string id;
    std::string label;
    float min, max, def;
    Curve curve = Curve::Linear;
};

struct EffectDef {
    std::string id;
    std::string name;
    EffectKind kind = EffectKind::Pointwise;

    // GLSL body of:  vec4 fx_<id>(vec4 col, vec2 uv, vec4 p)
    // In scope: uInput, uHistory, uTexel (1/size), uTime, uAspect.
    // p.x..p.w are this slot's four knobs.
    std::string glsl;

    std::vector<EffectParam> params;  // at most 4
};

const std::vector<EffectDef>& effectBank();
int findEffect(const std::string& id);

// One entry in the user's chain. The chain is an ordered list; reordering it
// is a first-class performance gesture, not a config change.
struct ChainSlot {
    int def = -1;   // index into effectBank()
    bool enabled = true;

    // Indices into the ONE ParamBank, resolved when the patch is rebuilt.
    // The slot does not own its knob values -- there is exactly one place a
    // parameter lives, so a sensor, a CC, a slider and an LFO all reach an
    // effect knob by the same route they reach a global one.
    int pIndex[4] = {-1, -1, -1, -1};
};

}  // namespace nv

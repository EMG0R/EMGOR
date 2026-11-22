#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nv {

struct OpVec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

enum class JitOpKind { Pointwise, Sampler };

struct JitOpDef {
    const char* id;
    const char* name;
    JitOpKind kind;
    float identity;
};

// Stable order is part of the wire contract. Append new operators; never
// reorder this list, because surfaces store the index while ids are used for
// presets and mappings.
const std::vector<JitOpDef>& jitOpBank();
int jitOpIndex(const std::string& id);

// The inactive channel is explicit. With useValue=false the operator's
// identity is used; with useValue=true opValue is used exactly as jit.op's
// scalar @val attribute. This avoids hidden "touched" state in ParamBank.
float jitOpIdentity(const JitOpDef& op);
OpVec4 jitOpApply(const JitOpDef& op, OpVec4 a, OpVec4 b,
                 bool bActive, bool useValue, float opValue,
                 bool swap = false);

// GLSL 3.00 ES implementation used by the binary operator pass. The switch
// is deliberately uniform-driven: changing an operator never recompiles a
// shader while an artist sweeps the catalogue.
const char* jitOpGlsl();

}  // namespace nv

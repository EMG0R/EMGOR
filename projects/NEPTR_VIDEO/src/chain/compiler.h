// Chain compiler: turns an ordered effect list into the smallest set of
// full-screen passes that computes it.
//
// The fusion rule, in full:
//
//   A pass may contain AT MOST ONE sampling effect (Sampler or History), and
//   it must come first in that pass, because it is what establishes the
//   colour. Any number of Pointwise effects then fuse on top of it.
//
// So [feedback, saturate, hue, posterize, blur, contrast, grain] compiles to
// two passes, not seven. On a fill-rate-bound tiler that is the difference
// between 60fps and 20.
//
// This file has no GL dependency on purpose -- it is pure logic, it is where
// the subtle bugs would live, and so it is the part that gets tested hardest.
#pragma once

#include <string>
#include <vector>

#include "chain/effects.h"

namespace nv {

struct CompiledPass {
    std::vector<int> slots;        // indices into the chain, in execution order
    bool readsHistory = false;
    bool readsNeighbors = false;
    std::string fragmentSource;    // complete, ready to compile
};

struct CompiledChain {
    std::vector<CompiledPass> passes;
    // Cheap identity for the shader cache: same key means the generated
    // sources are identical, so nothing needs recompiling.
    std::string key;
};

// Groups the chain into passes. Disabled slots are skipped entirely -- a
// bypassed effect costs literally nothing, not even a branch.
std::vector<CompiledPass> groupIntoPasses(const std::vector<ChainSlot>& chain);

// Grouping plus source generation.
CompiledChain compileChain(const std::vector<ChainSlot>& chain);

// The generated fragment source for one pass. Exposed for testing.
std::string generatePassSource(const std::vector<ChainSlot>& chain,
                               const CompiledPass& pass);

}  // namespace nv

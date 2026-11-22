// A patch is the whole instrument state: which kernel, which effects, and the
// one parameter bank they both draw from.
//
// The bank has three sections, in this order:
//
//   [0, globalEnd)              global -- survives every change
//   [globalEnd, +4)             the current kernel's macro knobs
//   [globalEnd+4, +4)         Channel B's macro knobs
//   [globalEnd+8, +4)         A's MORPH TARGET macro knobs
//   [.., count)                 one entry per effect knob, in chain order
//
// All kernel sections are padded to exactly four so the renderer can read
// uKernelP and uKernelPB from fixed offsets.
//
// Changing the kernel OR the chain rebuilds both dynamic sections, preserving
// by id whatever survived. That is what keeps the rest of the instrument still
// when you swap one effect.
#pragma once

#include <vector>

#include "chain/effects.h"
#include "core/kernels.h"
#include "state.h"

namespace nv {

// Rebuilds the dynamic sections and writes the resolved indices back into the
// chain slots. Call after any kernel change or chain edit.
void rebuildPatchParams(ParamBank& bank, const KernelDef& kernel,
                        const KernelDef* kernelB,
                        std::vector<ChainSlot>& chain,
                        const KernelDef* channelB = nullptr);

// The id an effect knob is registered under: "fx<slot>.<param>". Slot-scoped
// so two instances of the same effect are independently controllable.
std::string effectParamId(int slot, const std::string& paramId);

}  // namespace nv

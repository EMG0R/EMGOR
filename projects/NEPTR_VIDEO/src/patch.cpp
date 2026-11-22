#include "patch.h"

#include <cstdio>

namespace nv {

std::string effectParamId(int slot, const std::string& paramId) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "fx%d.%s", slot, paramId.c_str());
    return buf;
}

// Registers one kernel's four knob slots, padded so the offset convention
// holds. `prefix` keeps A's and B's ids distinct when both are the same
// kernel family -- otherwise the morph pair would share one set of knobs.
static void addKernelSlots(ParamBank& bank, const KernelDef* k,
                           const char* prefix, const char* group) {
    size_t n = 0;
    if (k) {
        for (const auto& p : k->params) {
            if (n >= 4) break;
            bank.addDynamic({prefix + p.id, p.label, group, p.min, p.max, p.def,
                             p.curve});
            ++n;
        }
    }
    for (; n < 4; ++n) {
        char id[64];
        std::snprintf(id, sizeof(id), "%sunused%zu", prefix, n);
        ParamDef pad{id, "-", group, 0.0f, 1.0f, 0.0f, Curve::Linear};
        pad.hidden = true;  // keeps packing regular without cluttering the panel
        bank.addDynamic(pad);
    }
}

void rebuildPatchParams(ParamBank& bank, const KernelDef& kernel,
                        const KernelDef* kernelB,
                        std::vector<ChainSlot>& chain,
                        const KernelDef* channelB) {
    bank.beginRebuild();

    // Kernel knobs at known offsets: A at globalEnd(), Channel B at
    // globalEnd()+4, and A's morph target at globalEnd()+8.
    // The renderer reads uKernelP and uKernelPB from exactly there, so this
    // order and the padding are load-bearing.
    addKernelSlots(bank, &kernel, "", "patch");
    // `b.*` belongs to Channel B. Keep its four-wide section even when the
    // source is inactive, so activating it never shifts effects or hitches.
    addKernelSlots(bank, channelB, "b.", "channel_b");
    // Morph is A's secondary kernel, so its knobs must carry a different
    // namespace and must not share storage with Channel B.
    addKernelSlots(bank, kernelB, "a.mk.", "morph");

    const auto& ebank = effectBank();
    for (size_t s = 0; s < chain.size(); ++s) {
        ChainSlot& slot = chain[s];
        for (int i = 0; i < 4; ++i) slot.pIndex[i] = -1;
        if (slot.def < 0 || slot.def >= static_cast<int>(ebank.size())) continue;

        const EffectDef& d = ebank[slot.def];
        for (size_t i = 0; i < d.params.size() && i < 4; ++i) {
            const EffectParam& p = d.params[i];
            // Grouped by the effect's display name so the panel can lay the
            // chain out without knowing anything about effects.
            slot.pIndex[i] = bank.addDynamic({effectParamId(static_cast<int>(s), p.id),
                                              p.label, d.name, p.min, p.max, p.def,
                                              p.curve});
        }
    }

    bank.endRebuild();
}

}  // namespace nv

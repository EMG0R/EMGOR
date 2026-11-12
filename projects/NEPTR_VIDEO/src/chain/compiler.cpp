#include "chain/compiler.h"

#include <cstdio>

#include "shader_source.h"

namespace nv {

static bool isSampling(EffectKind k) {
    return k == EffectKind::Sampler || k == EffectKind::History;
}

std::vector<CompiledPass> groupIntoPasses(const std::vector<ChainSlot>& chain) {
    const auto& bank = effectBank();
    std::vector<CompiledPass> passes;
    CompiledPass cur;
    bool curOpen = false;

    auto flush = [&]() {
        if (curOpen && !cur.slots.empty()) passes.push_back(cur);
        cur = CompiledPass{};
        curOpen = false;
    };

    for (size_t i = 0; i < chain.size(); ++i) {
        const ChainSlot& s = chain[i];
        // A disabled slot costs nothing at all -- not a pass, not a branch,
        // not a uniform. Bypass is free.
        if (!s.enabled) continue;
        if (s.def < 0 || s.def >= static_cast<int>(bank.size())) continue;

        const EffectDef& d = bank[s.def];

        if (isSampling(d.kind)) {
            // A sampling effect establishes the colour by fetching it itself,
            // so anything already accumulated in this pass would be thrown
            // away. It must start a pass of its own.
            if (curOpen) flush();
            cur = CompiledPass{};
            curOpen = true;
            cur.readsHistory = (d.kind == EffectKind::History);
            cur.readsNeighbors = (d.kind == EffectKind::Sampler);
            cur.slots.push_back(static_cast<int>(i));
        } else {
            // Pointwise: fuses onto whatever pass is open, or opens one that
            // begins with the default fetch.
            if (!curOpen) {
                cur = CompiledPass{};
                curOpen = true;
            }
            cur.slots.push_back(static_cast<int>(i));
        }
    }
    flush();
    return passes;
}

std::string generatePassSource(const std::vector<ChainSlot>& chain,
                               const CompiledPass& pass) {
    const auto& bank = effectBank();
    std::string src;
    src += glslPreamble();
    src += "in vec2 vUV;\n";
    src += "out vec4 fragColor;\n";
    src += "uniform sampler2D uInput;\n";
    src += "uniform sampler2D uHistory;\n";
    src += "uniform vec2 uTexel;\n";
    src += "uniform float uTime;\n";
    src += "uniform float uAspect;\n";
    src += glslCommon();

    // One vec4 of knobs per slot, named by the slot's position in the chain
    // so a reorder changes the generated source (and therefore the cache key)
    // exactly when it actually needs to.
    char buf[256];
    for (int slot : pass.slots) {
        std::snprintf(buf, sizeof(buf), "uniform vec4 uFxP%d;\n", slot);
        src += buf;
    }

    for (int slot : pass.slots) {
        const EffectDef& d = bank[chain[slot].def];
        std::snprintf(buf, sizeof(buf),
                      "vec4 fx_%d_%s(vec4 col, vec2 uv, vec4 p) {\n",
                      slot, d.id.c_str());
        src += buf;
        src += d.glsl;
        src += "\n}\n";
    }

    src += "void main() {\n";
    src += "  vec2 uv = vUV;\n";
    // If the pass opens with a sampling effect it does its own fetch, so the
    // default one would be a wasted texture read on a bandwidth-bound part.
    if (pass.slots.empty()) {
        src += "  vec4 col = nvSample(uInput, uv);\n";
    } else {
        const EffectDef& first = bank[chain[pass.slots[0]].def];
        if (isSampling(first.kind)) {
            src += "  vec4 col = vec4(0.0);\n";
        } else {
            src += "  vec4 col = nvSample(uInput, uv);\n";
        }
    }
    for (int slot : pass.slots) {
        const EffectDef& d = bank[chain[slot].def];
        std::snprintf(buf, sizeof(buf), "  col = fx_%d_%s(col, uv, uFxP%d);\n",
                      slot, d.id.c_str(), slot);
        src += buf;
    }
    src += "  fragColor = col;\n";
    src += "}\n";
    return src;
}

CompiledChain compileChain(const std::vector<ChainSlot>& chain) {
    CompiledChain out;
    out.passes = groupIntoPasses(chain);
    const auto& bank = effectBank();

    // The key covers everything that changes generated source: which effects,
    // in what order, enabled or not. Parameter VALUES are deliberately absent
    // -- they are uniforms, so turning a knob never recompiles anything.
    for (const auto& s : chain) {
        if (!s.enabled) { out.key += "-;"; continue; }
        if (s.def < 0 || s.def >= static_cast<int>(bank.size())) { out.key += "?;"; continue; }
        out.key += bank[s.def].id;
        out.key += ";";
    }

    for (auto& p : out.passes) {
        p.fragmentSource = generatePassSource(chain, p);
    }
    return out;
}

}  // namespace nv

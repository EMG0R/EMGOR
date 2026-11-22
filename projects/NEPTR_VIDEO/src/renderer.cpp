#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "palette/palette.h"
#include "chain/ops.h"
#include "shader_source.h"

namespace nv {

// The hard ceiling on the escape loop, and the thing that decided how deep a
// dive could go.
//
// 512 was not a precision limit -- the hi/lo centre pair carries ~48 bits,
// good to about 1e-14 -- it was an ITERATION limit, and it bound first by a
// wide margin: measured at DETAIL pinned to 512, the fraction of the frame
// that is not interior-black went 100% at 3 decades, 21% at 6 and 0% at 7. So
// a dive had to stop around 5 decades or descend into a black screen, and
// stopping meant recycling to somewhere new about every twenty seconds --
// which reads, correctly, as the zoom RESETTING rather than going anywhere.
//
// Raised so the depth wall is the precision one instead. This is a CEILING,
// not a floor: what a frame actually spends is `DETAIL * Journey::detailBoost`
// scaled by the governor, so a shallow view costs exactly what it did before
// and only a deep dive climbs. The governor still defends the framerate by
// lowering that number, so a machine that cannot afford the depth loses
// detail rather than frames.
static constexpr int kIterCap = 2048;

static std::string replaceAll(std::string s, const std::string& from,
                              const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

bool Renderer::init(int maxW, int maxH, int contextCount) {
    maxW_ = maxW;
    maxH_ = maxH;
    tri_.create(contextCount);

    for (auto& b : buf_) {
        if (!b.create(maxW, maxH, true)) return false;
    }

    std::string vert = std::string(glslPreamble()) + glslFullscreenVert();
    std::string presentFrag =
        std::string(glslPreamble()) + glslCommon() + glslPresentFrag();
    // The PRESENT pass is the only chain stage that reaches the physical
    // framebuffer, so it is the only one that turns. Rotating the shared
    // fullscreen vertex shader would turn every offscreen pass too, and each
    // effect would then sample a sideways image.
    std::string presentVert =
        std::string(glslPreamble()) + glslPresentVert();
    if (!present_.build(presentVert, presentFrag, "present")) return false;

    glGenTextures(1, &paletteTex_);
    glBindTexture(GL_TEXTURE_2D, paletteTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kLutWidth, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    // LINEAR across t so the gradient is smooth; the two rows sit exactly on
    // texel centres (v = 0.25 and 0.75) so they never bleed into each other.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // REPEAT on s: the palette-rotate control is meant to cycle forever.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    setPalettes(0, 1);

    std::string compFrag =
        std::string(glslPreamble()) + glslCommon() +
        "in vec2 vUV;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D uInput;\n"
        "uniform sampler2D uInk;\n"
        "uniform float uFractalLevel;\n"
        "uniform float uInkLevel;\n"
        "void main() {\n"
        "  vec3 c = nvSample(uInput, vUV).rgb * uFractalLevel;\n"
        "  vec4 k = nvSample(uInk, vUV) * uInkLevel;\n"
        // Ink is premultiplied, so it composites over the generator without
        // needing its own blend state.
        "  fragColor = vec4(c * (1.0 - k.a) + k.rgb, 1.0);\n"
        "}\n";
    if (!composite_.build(vert, compFrag, "composite")) return false;

    std::string opFrag =
        std::string(glslPreamble()) + glslCommon() + jitOpGlsl() +
        "in vec2 vUV;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D uInputA;\n"
        "uniform sampler2D uInputB;\n"
        "uniform vec2 uScaleA;\n"
        "uniform vec2 uScaleB;\n"
        "uniform int uBActive;\n"
        "uniform int uOp;\n"
        "uniform float uOpMix;\n"
        "uniform float uOpVal;\n"
        "uniform float uOpUseVal;\n"
        "uniform float uOpIdentity;\n"
        "uniform int uOpSwap;\n"
        "uniform int uOpPlane;\n"
        "vec4 nvOpSample(sampler2D t, vec2 uv, vec2 scale) {\n"
        "  return texture(t, clamp(uv, 0.0, 1.0) * scale);\n"
        "}\n"
        "void main() {\n"
        "  vec4 a = nvOpSample(uInputA, vUV, uScaleA);\n"
        "  vec4 b = (uBActive != 0) ? nvOpSample(uInputB, vUV, uScaleB) :\n"
        "           vec4(mix(uOpIdentity, uOpVal, step(0.5, uOpUseVal)));\n"
        "  if (uOpSwap != 0) { vec4 t = a; a = b; b = t; }\n"
        "  vec4 r = (uBActive == 0 && step(0.5, uOpUseVal) == 0.0 && uOp == 1)\n"
        "              ? a : nvJitOp(a, b, uOp);\n"
        "  if (uOpPlane == 1) r.a = a.a;\n"
        "  else if (uOpPlane == 2) { float l = nvLuma(r.rgb); r = vec4(l, l, l, a.a); }\n"
        "  r.a = 1.0;\n"
        "  fragColor = mix(a, r, clamp(uOpMix, 0.0, 1.0));\n"
        "}\n";
    if (!op_.build(vert, opFrag, "jit.op")) return false;

    // Particle state is a fixed pool; COUNT scales how much of it is alive,
    // so changing it never reallocates.
    particles_.init(poolDim_, poolDim_, contextCount);
    ink_.init(maxW, maxH, contextCount);

    governor_.configure(60.0);
    return glCheck("renderer init");
}

void Renderer::shutdown() {
    for (auto& b : buf_) b.destroy();
    tri_.destroy();
    present_.destroy();
    passPrograms_.clear();
    coreCache_.clear();
    composite_.destroy();
    op_.destroy();
    particles_.shutdown();
    ink_.shutdown();
    if (paletteTex_) glDeleteTextures(1, &paletteTex_);
    paletteTex_ = 0;
}

void Renderer::setPalettes(int a, int b) {
    const auto& bank = paletteBank();
    if (bank.empty()) return;
    a = ((a % static_cast<int>(bank.size())) + static_cast<int>(bank.size())) %
        static_cast<int>(bank.size());
    b = ((b % static_cast<int>(bank.size())) + static_cast<int>(bank.size())) %
        static_cast<int>(bank.size());
    if (a == palA_ && b == palB_) return;
    palA_ = a;
    palB_ = b;

    uint8_t lut[kLutWidth * 2 * 4];
    buildDualLut(bank[a], bank[b], lut);
    glBindTexture(GL_TEXTURE_2D, paletteTex_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kLutWidth, 2, GL_RGBA,
                    GL_UNSIGNED_BYTE, lut);
}

bool Renderer::setKernel(const KernelDef& k, ColorMode mode,
                         const KernelDef* kb) {
    // Morphing only makes sense between kernels that iterate the same way,
    // and compiling the B side when it is unused would double the inner loop
    // for nothing.
    if (kb && (kb->id == k.id || !canMorph(k, *kb))) kb = nullptr;
    // Keyed by kernel AND colour mode: the mode decides which intrinsic
    // features the inner loop computes, and those are #defines. Browsing back
    // and forth through either must not recompile, so both live in the key.
    char key[256];
    std::snprintf(key, sizeof(key), "%s|%s:%d", k.id.c_str(),
                  kb ? kb->id.c_str() : "-", static_cast<int>(mode));
    if (coreKey_ == key) return true;

    auto cached = coreCache_.find(key);
    if (cached != coreCache_.end() && cached->second.valid()) {
        coreKey_ = key;
        return true;
    }

    const ColorFeatures f = featuresFor(mode);
    char defines[1024];
    std::snprintf(defines, sizeof(defines),
                  "#define SEED_JULIA %d\n"
                  "#define NV_CONVERGENT %d\n"
                  "#define NV_ESCAPE_R2 %.8f\n"
                  "#define NV_ESCAPE_R %.8f\n"
                  "#define NV_ITER_CAP %d\n"
                  "#define FEAT_ANGLE %d\n"
                  "#define FEAT_STRIPE %d\n"
                  "#define FEAT_CURV %d\n"
                  "#define FEAT_TRAP %d\n"
                  "#define MODE_TRAP %d\n"
                  "#define MODE_ANGLE %d\n"
                  "#define MODE_DERIV %d\n"
                  "#define MODE_STRIPE %d\n"
                  "#define MODE_CURV %d\n"
                  "#define MODE_PRISM %d\n"
                  "#define MODE_DUAL %d\n"
                  "#define KERNEL_FIELD %d\n"
                  "#define MORPH %d\n"
                  "#define KERNEL_B_FIELD %d\n",
                  k.seed == SeedMode::Julia ? 1 : 0,
                  k.convergent ? 1 : 0,
                  k.escapeR2,
                  std::sqrt(k.escapeR2 > 0.0f ? k.escapeR2 : 4.0f),
                  kIterCap,
                  f.angle ? 1 : 0, f.stripe ? 1 : 0, f.curvature ? 1 : 0,
                  f.trap ? 1 : 0,
                  mode == ColorMode::OrbitTrap ? 1 : 0,
                  mode == ColorMode::FinalAngle ? 1 : 0,
                  mode == ColorMode::Derivative ? 1 : 0,
                  mode == ColorMode::Stripe ? 1 : 0,
                  mode == ColorMode::Curvature ? 1 : 0,
                  mode == ColorMode::Prism ? 1 : 0,
                  mode == ColorMode::Dual ? 1 : 0,
                  k.kind == KernelKind::Field ? 1 : 0,
                  kb ? 1 : 0,
                  (kb && kb->kind == KernelKind::Field) ? 1 : 0);

    std::string body = glslCoreTemplate();
    // Compiles the motion OUT, as a constant rather than a uniform, so the
    // driver folds every term that reads it away to nothing. This is the only
    // way to measure what the motion actually costs: turning EVOLVE to 0 zeroes
    // the numbers but leaves every instruction in the inner loop, so it
    // measures nothing. Test scaffolding, and named as such.
    static const bool kStaticMotion = std::getenv("NV_MOTION_STATIC") != nullptr;
    if (kStaticMotion) {
        body = replaceAll(body, "uniform vec4  uMotion;",
                          "const vec4 uMotion = vec4(0.0);");
    }
    body = replaceAll(body, "%%DEFINES%%", defines);
    body = replaceAll(body, "%%STEP%%", k.step.empty() ? "return s;" : k.step);
    body = replaceAll(body, "%%FIELD%%",
                      k.field.empty() ? "return 0.0;" : k.field);
    body = replaceAll(body, "%%STEPB%%",
                      (kb && !kb->step.empty()) ? kb->step : "return s;");
    body = replaceAll(body, "%%FIELDB%%",
                      (kb && !kb->field.empty()) ? kb->field : "return 0.0;");
    // Two different defaults behind one hook: the seed mode's answer unless
    // the kernel overrides it. Substituted rather than branched so an
    // override costs nothing in the shader.
    body = replaceAll(body, "%%ZINIT%%",
                      !k.zInit.empty() ? k.zInit
                                       : (k.seed == SeedMode::Julia
                                              ? std::string("p")
                                              : std::string("vec2(0.0)")));
    body = replaceAll(body, "%%AUXINIT%%",
                      k.auxInit.empty() ? "0.0, 0.0" : k.auxInit);

    std::string vert = std::string(glslPreamble()) + glslFullscreenVert();
    std::string frag = std::string(glslPreamble()) + glslCommon() + body;

    Program prog;
    if (!prog.build(vert, frag, key)) return false;

    coreCache_[key] = std::move(prog);
    coreKey_ = key;
    return true;
}

bool Renderer::setChannelBKernel(const KernelDef& k, ColorMode mode) {
    // Reuse the exact core compiler/cache path without making Channel B a
    // morph target. Save the active A key because setKernel historically owns
    // the one active key; V2 keeps both keys live in the same cache.
    const std::string saved = coreKey_;
    bool ok = setKernel(k, mode, nullptr);
    coreKeyB_ = coreKey_;
    coreKey_ = saved;
    return ok;
}

bool Renderer::setChainBlocking(const std::vector<ChainSlot>& chain) {
    CompiledChain cc = compileChain(chain);
    if (cc.key == chainKey_ && !passPrograms_.empty()) return true;

    std::vector<Program> built;
    built.resize(cc.passes.size());
    std::string vert = std::string(glslPreamble()) + glslFullscreenVert();
    for (size_t i = 0; i < cc.passes.size(); ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "chain_pass_%zu", i);
        if (!built[i].build(vert, cc.passes[i].fragmentSource, name)) {
            // Keep rendering what we had. A bad chain must never take the
            // picture away mid-set.
            return false;
        }
    }

    passPrograms_ = std::move(built);
    compiled_ = std::move(cc);
    chainKey_ = compiled_.key;
    pendingToken_ = 0;
    return true;
}

void Renderer::requestChain(const std::vector<ChainSlot>& chain) {
    CompiledChain cc = compileChain(chain);
    // Structure unchanged means nothing to build. Only adding, removing,
    // reordering or bypassing an effect gets this far.
    if (cc.key == chainKey_ && !passPrograms_.empty()) return;
    if (pendingToken_ != 0 && cc.key == pending_.key) return;

    if (!compiler_ || !compiler_->running()) {
        setChainBlocking(chain);
        return;
    }

    std::vector<std::string> frags;
    frags.reserve(cc.passes.size());
    for (const auto& p : cc.passes) frags.push_back(p.fragmentSource);

    pending_ = std::move(cc);
    pendingToken_ = nextToken_++;
    compiler_->submit(pendingToken_, std::string(glslPreamble()) + glslFullscreenVert(),
                      std::move(frags));
}

bool Renderer::pumpCompiler() {
    if (!compiler_ || pendingToken_ == 0) return false;

    uint64_t token = 0;
    std::vector<GLuint> progs;
    if (!compiler_->poll(token, progs)) return false;
    if (token != pendingToken_) {
        // A result for a chain we have already moved past.
        for (GLuint p : progs) glDeleteProgram(p);
        return false;
    }
    if (progs.size() != pending_.passes.size()) {
        for (GLuint p : progs) glDeleteProgram(p);
        pendingToken_ = 0;
        return false;
    }

    // Programs and their pass grouping go live together, in one frame. A
    // half-swapped chain would render one pass against another's uniforms.
    std::vector<Program> built(progs.size());
    for (size_t i = 0; i < progs.size(); ++i) built[i].adopt(progs[i]);

    passPrograms_ = std::move(built);
    compiled_ = std::move(pending_);
    chainKey_ = compiled_.key;
    pendingToken_ = 0;
    return true;
}

// Excludes three buffers at once: with the swarm running there are four live
// at the same time -- the generator, the history, the field being sampled and
// the target being drawn into.
int Renderer::pickTarget3(int a, int b, int c) const {
    for (int i = 0; i < 8; ++i) {
        if (i != a && i != b && i != c) return i;
    }
    return 0;
}

int Renderer::pickTarget(int cur, int prev) const {
    for (int i = 0; i < 8; ++i) {
        if (i != cur && i != prev) return i;
    }
    return 0;
}

void Renderer::bindSourceUniforms(Program& p, int renderW, int renderH) {
    p.set2f("uSrcScale", static_cast<float>(renderW) / static_cast<float>(maxW_),
            static_cast<float>(renderH) / static_cast<float>(maxH_));
    p.set2f("uTexel", 1.0f / static_cast<float>(renderW),
            1.0f / static_cast<float>(renderH));
}

FrameStats Renderer::renderScene(const ParamBank& params, const Selection& sel,
                                 const std::vector<ChainSlot>& chain,
                                 double time, double dt, int outW, int outH,
                                 float aspect, int ctx) {
    FrameStats fs;

    const float userRes = params.valueOf(gid::kRenderScale, 1.0f);
    const float res = userRes * governor_.resScale();
    // The sub-rect can never exceed the once-allocated buffer, whatever the
    // output or the resolution control say.
    int renderW = std::clamp(static_cast<int>(outW * res), 32, maxW_);
    int renderH = std::clamp(static_cast<int>(outH * res), 32, maxH_);
    lastRenderW_ = renderW;
    lastRenderH_ = renderH;
    fs.renderWidth = renderW;
    fs.renderHeight = renderH;

    // What the user asked for, and what the loop will actually run. Both are
    // needed: the second is the cost, the first is what the colouring is
    // normalised against, so the governor stepping in takes fine structure
    // away without repainting the whole frame in different colours.
    // Depth buys iterations. The escape count needed to resolve the boundary
    // grows as the view descends, so a dive drawn with the wide view's budget
    // goes SOLID -- every sample hits the ceiling without escaping and the
    // frame is interior-black. That is not a subtle degradation: it is the
    // journey diving into a black screen, which is exactly what the first
    // measured run of journey_probe.py did.
    //
    // Applied to detailRef as well as to detail, so the palette still
    // traverses exactly once at the boosted ceiling and the picture does not
    // change colour as it descends. The GOVERNOR remains the only thing that
    // moves detail without moving detailRef -- that separation is what keeps
    // it from repainting the frame when it engages.
    // Blended across the dissolve, not read from view A alone.
    //
    // At the recycle boundary the visible picture is almost entirely view B,
    // which is WIDE (negative depth), while view A is at the bottom of its
    // dive. Boosting on A's depth there asks for deep-dive iterations to draw
    // a wide frame, and the count collapses the instant the flip completes --
    // measured on the Pi as iterations running 174, 252, 330, 408 and then
    // dropping to 102 in one frame, which showed up as a 37.44 step spike
    // against neighbours of 6.9 and 4.97. The dissolve was smooth; the
    // ITERATION COUNT was the cut.
    const float depth =
        Journey::detailBoost(journey_.a.logZoom) * (1.0f - journey_.xfade) +
        Journey::detailBoost(journey_.b.logZoom) * journey_.xfade;
    const float asked = params.valueOf(gid::kDetail, 96.0f) * depth;
    const float detailRef =
        std::clamp(asked, 4.0f, static_cast<float>(kIterCap));
    float detail = asked * governor_.detailScale();
    detail = std::clamp(detail, 4.0f, static_cast<float>(kIterCap));
    fs.iterations = static_cast<int>(detail);

    // ---- generator channels --------------------------------------------
    // Both channels use the same active render rectangle. Wireless and
    // display output scaling is deliberately outside this graph; the
    // operator must never combine differently sized internal pictures.
    auto drawCore = [&](Program& core, int target, bool morph,
                        int macroBase) {
        buf_[target].bind(renderW, renderH);
        core.use();
        bindSourceUniforms(core, renderW, renderH);
        core.set1f("uAspect", aspect);
        core.set1f("uTime", static_cast<float>(time));
        core.set1f("uRotation", params.valueOf(gid::kRotation, 0.0f));
        // ZOOM the knob multiplies the JOURNEY's depth; the journey is
        // decades, so it enters as a power of ten. The knob therefore keeps
        // meaning exactly what it meant -- a hand zoom on top of wherever the
        // journey has travelled to -- which is what lets an old preset load
        // and behave.
        //
        // The centre is the journey's (in double) plus the X/Y knobs as an
        // offset, and the sum is split into the hi/lo pair HERE, at full
        // double precision, because splitting after any float round-trip
        // would be splitting a number that had already lost the bits.
        {
            const float zoomKnob = params.valueOf(gid::kZoom, 1.0f);
            const double offX = params.valueOf(gid::kCenterX, 0.0f);
            const double offY = params.valueOf(gid::kCenterY, 0.0f);

            const auto upload = [&](const JourneyView& v, const char* zn,
                                    const char* hin, const char* lon) {
                const double zoom = static_cast<double>(zoomKnob) *
                                    std::pow(10.0, v.logZoom);
                core.set1f(zn, static_cast<float>(zoom));
                const double cx = v.cx + offX;
                const double cy = v.cy + offY;
                // Veltkamp-style split: the high word is the double rounded to
                // float, the low word is the REMAINDER, computed in double.
                // hi + lo reconstructs the centre to ~48 bits.
                const float hix = static_cast<float>(cx);
                const float hiy = static_cast<float>(cy);
                core.set2f(hin, hix, hiy);
                core.set2f(lon, static_cast<float>(cx - static_cast<double>(hix)),
                           static_cast<float>(cy - static_cast<double>(hiy)));
            };

            upload(journey_.a, "uZoom", "uCenterHi", "uCenterLo");
            upload(journey_.b, "uZoomB", "uCenterHiB", "uCenterLoB");
            core.set1f("uRotA", static_cast<float>(journey_.a.rot));
            core.set1f("uRotB", static_cast<float>(journey_.b.rot));
            core.set1f("uXfade", journey_.xfade);
        }
        core.set1f("uDetail", detail);
        core.set1f("uDetailRef", detailRef);
        core.set4f("uMotion", motion_[0], motion_[1], motion_[2], motion_[3]);
        core.set1f("uPaletteMix", params.valueOf(gid::kPaletteMix, 0.0f));
        core.set1f("uPaletteShift", params.valueOf(gid::kPaletteShift, 0.0f));
        core.set1f("uPaletteSat", params.valueOf(gid::kPaletteSat, 1.0f));
        core.set1i("uTrapShape", sel.trapShape);
        core.set1f("uTrapScale", sel.trapScale);
        core.set1f("uPaletteSpread", params.valueOf(gid::kPaletteSpread, 1.0f));
        core.set1f("uStripeFreq", params.valueOf(gid::kStripeFreq, 6.0f));
        core.set1f("uPrism", params.valueOf(gid::kPrism, 0.0f));

        float kp[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int idx = macroBase + i;
            if (idx < params.count()) kp[i] = params.value(idx);
        }
        core.set4f("uKernelP", kp[0], kp[1], kp[2], kp[3]);

        float kpb[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int idx = params.globalEnd() + 8 + i;
            if (idx < params.count()) kpb[i] = params.value(idx);
        }
        core.set4f("uKernelPB", kpb[0], kpb[1], kpb[2], kpb[3]);
        core.set1f("uMorph", morph ? params.valueOf(gid::kMorph, 0.0f) : 0.0f);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, paletteTex_);
        core.set1i("uPalette", 2);
        tri_.draw(ctx);
        ++fs.passes;
    };

    auto coreA = coreCache_.find(coreKey_);
    if (coreA == coreCache_.end()) return fs;
    int targetA = pickTarget(historyIdx_, -1);
    drawCore(coreA->second, targetA, coreKey_.find("|-:") == std::string::npos,
             params.globalEnd());

    // ---- steering: find the boundary in what was just drawn -------------
    //
    // Measured from CHANNEL A's raw generator output, before the effect
    // chain. That is deliberate: FEEDBACK and GRAIN put non-black pixels
    // everywhere, and interior-is-black is the whole basis of the
    // classification. Measuring after the chain would report a boundary in
    // frames that have none.
    //
    // Every kFocusEvery frames rather than every frame, because a readback is
    // a pipeline stall. The journey needs a direction, not a per-frame
    // correction, and the recorded path is interpolated between samples.
    // Gated on NV_STEER, like the use of its result.
    //
    // Gating only the CONSUMER left the producer running unconditionally: a
    // 64x64 FBO and a glReadPixels every 15 frames whose output nothing read.
    // On the Pi's V3D/Mesa that FBO's teardown then took a SIGBUS inside
    // libgallium during ~Renderer(), reproducible on exactly the frame count
    // that first triggers a measurement -- 10 frames exit clean, 20 crash.
    // It crashes at process teardown only, so the live instrument never saw
    // it, but it truncated buffered stdout and so failed motion_time by
    // losing the "wrote ..." lines of frames that had rendered correctly.
    // Gating here also drops a readback pipeline stall nobody was using.
    static const bool kSteerOn = std::getenv("NV_STEER") != nullptr;
    if (kSteerOn && ++focusTick_ >= kFocusEvery) {
        focusTick_ = 0;
        measureFocus(targetA, aspect, params, ctx);
    }
    int cur = targetA;

    if (sel.sourceB == SourceKind::Fractal) {
        auto coreB = coreCache_.find(coreKeyB_);
        if (coreB == coreCache_.end()) return fs;
        int targetB = pickTarget(targetA, historyIdx_);
        drawCore(coreB->second, targetB, false, params.globalEnd() + 4);

        int opIndex = static_cast<int>(std::lround(
            params.valueOf(gid::kOpMode, 0.0f)));
        opIndex = std::clamp(opIndex, 0, static_cast<int>(jitOpBank().size()) - 1);
        const JitOpDef& opDef = jitOpBank()[opIndex];
        int opTarget = pickTarget3(targetA, targetB, historyIdx_);
        buf_[opTarget].bind(renderW, renderH);
        op_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, buf_[targetA].texture());
        op_.set1i("uInputA", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, buf_[targetB].texture());
        op_.set1i("uInputB", 1);
        op_.set2f("uScaleA", static_cast<float>(renderW) / maxW_,
                   static_cast<float>(renderH) / maxH_);
        op_.set2f("uScaleB", static_cast<float>(renderW) / maxW_,
                   static_cast<float>(renderH) / maxH_);
        op_.set1i("uBActive", 1);
        op_.set1i("uOp", opIndex);
        op_.set1f("uOpMix", params.valueOf(gid::kOpMix, 1.0f));
        op_.set1f("uOpVal", params.valueOf(gid::kOpVal, 0.0f));
        op_.set1f("uOpUseVal", params.valueOf(gid::kOpUseVal, 0.0f));
        op_.set1f("uOpIdentity", jitOpIdentity(opDef));
        op_.set1i("uOpSwap", params.valueOf(gid::kOpSwap, 0.0f) >= 0.5f);
        op_.set1i("uOpPlane", static_cast<int>(std::lround(
            params.valueOf(gid::kOpPlane, 0.0f))));
        tri_.draw(ctx);
        ++fs.passes;
        cur = opTarget;
    }

    // ---- layers ---------------------------------------------------------
    // Particles and ink composite BEFORE the chain, so feedback, warp and
    // kaleido process them exactly as they process the generator. That is
    // what turns one brushed dab into structure instead of leaving it a dab.
    if (sel.mode != Mode::Synth) {
        const float sx = static_cast<float>(renderW) / maxW_;
        const float sy = static_cast<float>(renderH) / maxH_;

        // Ink is stamped BEFORE the field is assembled, so in particle mode
        // the swarm samples the strokes too -- draw something and it is made
        // of particles exactly like the fractal is.
        if (sel.mode == Mode::Draw || ink_.hasContent()) {
            InkParams ip;
            ip.size = params.valueOf(gid::kInkSize, 0.04f);
            ip.flow = params.valueOf(gid::kInkFlow, 0.6f);
            ip.softness = params.valueOf(gid::kInkSoft, 0.6f);
            ip.fade = params.valueOf(gid::kInkFade, 0.0f);
            ip.fieldColor = params.valueOf(gid::kInkFieldColor, 1.0f);
            ink_.stamp(renderW, renderH, buf_[cur].texture(), sx, sy, paletteTex_,
                       params.valueOf(gid::kPaletteShift, 0.0f),
                       params.valueOf(gid::kPaletteMix, 0.0f), ip,
                       static_cast<float>(dt), ctx);
        }

        const float level = params.valueOf(gid::kFractalLevel, 1.0f);
        const float inkLevel = ink_.hasContent() ? 1.0f : 0.0f;
        const bool wantParticles =
            (sel.mode == Mode::Particles && particles_.available());

        auto compositeInto = [&](int dstBuf, int srcBuf, float fractalLevel) {
            buf_[dstBuf].bind(renderW, renderH);
            composite_.use();
            bindSourceUniforms(composite_, renderW, renderH);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, buf_[srcBuf].texture());
            composite_.set1i("uInput", 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ink_.texture());
            composite_.set1i("uInk", 1);
            composite_.set1f("uFractalLevel", fractalLevel);
            composite_.set1f("uInkLevel", inkLevel);
            tri_.draw(ctx);
            ++fs.passes;
        };

        // The field the layers READ is composited at FULL generator level.
        // FRACTAL controls how much of the generator you SEE behind the
        // swarm -- it must not dim what is steering it, or turning the
        // background off would leave the particles sampling black and the
        // whole layer would go dark.
        int fieldBuf = pickTarget(cur, historyIdx_);
        compositeInto(fieldBuf, cur, 1.0f);

        if (wantParticles) {
            ParticleParams pp;
            pp.cohesion = params.valueOf(gid::kPtCohesion, 1.0f);
            // The governor may thin the swarm, but ONLY when it is loose.
            // At high cohesion the particles ARE the image, and removing
            // them would punch holes in it rather than costing detail.
            governor_.setCountEffective(pp.cohesion < 0.5f);
            pp.count = params.valueOf(gid::kPtCount, 0.5f) * governor_.countScale();
            pp.size = params.valueOf(gid::kPtSize, 3.0f);
            pp.speed = params.valueOf(gid::kPtSpeed, 1.0f);
            pp.gravity = params.valueOf(gid::kPtGravity, 1.5f);
            pp.swirl = params.valueOf(gid::kPtSwirl, 1.0f);
            pp.motion = params.valueOf(gid::kPtMotion, 0.15f);
            pp.paletteShift = params.valueOf(gid::kPaletteShift, 0.0f);
            pp.paletteMix = params.valueOf(gid::kPaletteMix, 0.0f);
            pp.damping = params.valueOf(gid::kPtDamping, 0.94f);
            pp.life = params.valueOf(gid::kPtLife, 6.0f);
            pp.brightness = params.valueOf(gid::kPtBright, 1.0f);
            pp.fieldColor = params.valueOf(gid::kPtFieldColor, 1.0f);
            pp.carry = params.valueOf(gid::kPtCarry, 1.0f);
            pp.dieInterior = params.valueOf(gid::kPtDieInterior, 0.0f);
            pp.dieEdge = params.valueOf(gid::kPtDieEdge, 0.0f);
            pp.dieExhaust = params.valueOf(gid::kPtDieExhaust, 0.0f);
            pp.deposit = params.valueOf(gid::kPtDeposit, 0.0f);

            particles_.update(buf_[fieldBuf].texture(), sx, sy,
                              static_cast<float>(dt), time, pp, ctx);
            lastParticles_ = pp;

            // A fourth buffer: the field is being SAMPLED, so it cannot also
            // be the render target -- reading and writing one texture in a
            // single pass is undefined.
            int dst = pickTarget3(cur, historyIdx_, fieldBuf);
            // The visible background gets the level applied here, which is
            // the only place it belongs.
            compositeInto(dst, cur, level);
            buf_[dst].bind(renderW, renderH);
            particles_.draw(renderW, renderH, buf_[fieldBuf].texture(), sx, sy,
                            paletteTex_, pp, ctx);

            // The loop closes here. The swarm writes into the ink AFTER it has
            // been drawn, so this frame's picture and the field that steered it
            // are the same thing; the deposit reaches the particles on the next
            // frame, through the composite, exactly like feedback does. Ink
            // FADE is the release valve on a loop with no other one.
            //
            // Tested here as well as inside deposit(): binding the canvas is
            // what marks it as having content, and doing that at DEPOSIT 0
            // would put an empty ink layer into the composite forever after.
            if (pp.deposit > 0.0f) {
                ink_.beginDeposit(renderW, renderH);
                particles_.deposit(renderW, renderH, buf_[fieldBuf].texture(),
                                   sx, sy, paletteTex_, pp, ctx);
            }
            cur = dst;
        } else {
            // No swarm: the field buffer is the picture, but it was built at
            // full level, so rebuild it with the level applied.
            if (level < 0.999f || inkLevel > 0.0f) {
                int dst = pickTarget3(cur, historyIdx_, fieldBuf);
                compositeInto(dst, cur, level);
                cur = dst;
            } else {
                cur = fieldBuf;
            }
        }
    }

    // ---- effect chain ---------------------------------------------------
    fs.passes += static_cast<int>(compiled_.passes.size());

    for (size_t i = 0; i < compiled_.passes.size() && i < passPrograms_.size(); ++i) {
        const CompiledPass& pass = compiled_.passes[i];
        Program& prog = passPrograms_[i];
        if (!prog.valid()) continue;

        int dst = pickTarget(cur, historyIdx_);
        buf_[dst].bind(renderW, renderH);
        prog.use();
        bindSourceUniforms(prog, renderW, renderH);
        prog.set1f("uTime", static_cast<float>(time));
        prog.set1f("uAspect", aspect);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, buf_[cur].texture());
        prog.set1i("uInput", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, buf_[historyIdx_].texture());
        prog.set1i("uHistory", 1);

        for (int slot : pass.slots) {
            const ChainSlot& s = chain[slot];
            char name[32];
            std::snprintf(name, sizeof(name), "uFxP%d", slot);
            // Knob values come from the shared bank, never from the slot --
            // one place a parameter lives.
            float v[4] = {0, 0, 0, 0};
            for (int i = 0; i < 4; ++i) {
                if (s.pIndex[i] >= 0 && s.pIndex[i] < params.count()) {
                    v[i] = params.value(s.pIndex[i]);
                }
            }
            prog.set4f(name, v[0], v[1], v[2], v[3]);
        }

        tri_.draw(ctx);
        cur = dst;
    }

    // This frame's result becomes next frame's history, with no copy: we
    // simply never target it again until it has been consumed.
    resultIdx_ = cur;
    historyIdx_ = cur;
    return fs;
}

void Renderer::present(int outW, int outH, int ctx) {
    // Bind the window explicitly. renderScene leaves its last chain FBO
    // bound, so a present that only set the viewport would draw the picture
    // straight back into an offscreen buffer and leave the screen black.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // outW/outH are the LOGICAL size -- already transposed for a quarter turn,
    // because everything above the platform seam lays out for the screen as the
    // user sees it. The VIEWPORT is physical, so it has to be transposed back.
    //
    // Getting this wrong does not look like a rotation bug: the picture is
    // drawn into a wrongly-shaped rectangle, so part of the panel stays black
    // and the rest is clipped. Observed on the device before this line existed.
    bool quarter = (rotation_ == 90 || rotation_ == 270);
    glViewport(0, 0, quarter ? outH : outW, quarter ? outW : outH);
    present_.use();
    present_.set1i("uRot", rotation_);
    bindSourceUniforms(present_, lastRenderW_ > 0 ? lastRenderW_ : outW,
                       lastRenderH_ > 0 ? lastRenderH_ : outH);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, buf_[resultIdx_].texture());
    present_.set1i("uInput", 0);
    tri_.draw(ctx);
}


// Draw the generator's raw output small, read it back, and locate the
// boundary. See focus.h for why the boundary is the right target and not
// "wherever the picture is busiest".
void Renderer::measureFocus(int srcIdx, float aspect, const ParamBank& params,
                            int ctx) {
    if (focus_.maxWidth() == 0 && !focus_.create(kFocusDim, kFocusDim, true))
        return;

    // Save and restore the bound framebuffer and viewport.
    //
    // Without this the measurement leaks its 64x64 target into whatever pass
    // runs next, so one frame in kFocusEvery is drawn into a 64-pixel
    // viewport. It does not read as a broken renderer -- most passes rebind
    // and the frame recovers -- it reads as an intermittent JUMP, measured as
    // a step of 20.74 against a median of 4.35 in the continuity window.
    GLint prevFbo = 0, prevVp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_VIEWPORT, prevVp);

    focus_.bind(kFocusDim, kFocusDim);
    present_.use();
    // No rotation: this is a measurement, not a picture for a panel, and the
    // mapping back to plane coordinates below assumes the unrotated frame.
    present_.set1i("uRot", 0);
    bindSourceUniforms(present_, kFocusDim, kFocusDim);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, buf_[srcIdx].texture());
    present_.set1i("uInput", 0);
    tri_.draw(ctx);

    std::vector<uint8_t> px(static_cast<size_t>(kFocusDim) * kFocusDim * 4);
    glReadPixels(0, 0, kFocusDim, kFocusDim, GL_RGBA, GL_UNSIGNED_BYTE,
                 px.data());

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);

    const FocusScore sc = scoreFocus(px.data(), kFocusDim);
    focusPoint_.valid = false;
    if (!sc.valid) return;   // no boundary in frame: nothing to steer to

    // Cell -> plane coordinate, by the SAME transform the shader uses, so the
    // point handed back is the one the performer is looking at. glReadPixels
    // is bottom-up and the shader's vUV is bottom-up too, so no flip here --
    // getting that wrong steers consistently at the mirror image of the
    // target, which looks like the dive drifting off structure for no reason.
    const double u = (sc.cx + 0.5) / kFocusDim;
    const double v = (sc.cy + 0.5) / kFocusDim;
    double bx = (u - 0.5) * 2.0 * aspect;
    double by = (v - 0.5) * 2.0;

    const double rot = params.valueOf(gid::kRotation, 0.0f);
    const double cs = std::cos(rot), sn = std::sin(rot);
    const double rx = bx * cs - by * sn;
    const double ry = bx * sn + by * cs;

    const double zoom = static_cast<double>(params.valueOf(gid::kZoom, 1.0f)) *
                        std::pow(10.0, journey_.a.logZoom);
    focusPoint_.x = rx / zoom + journey_.a.cx;
    focusPoint_.y = ry / zoom + journey_.a.cy;
    focusPoint_.valid = true;
}

}  // namespace nv

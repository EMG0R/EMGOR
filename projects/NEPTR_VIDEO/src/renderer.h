#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "async_compiler.h"
#include "chain/compiler.h"
#include "core/kernels.h"
#include "gfx/fbo.h"
#include "gfx/quad.h"
#include "gfx/shader.h"
#include "governor.h"
#include "motion.h"
#include "focus.h"
#include "journey.h"
#include "layers/ink.h"
#include "layers/particles.h"
#include "state.h"

namespace nv {

struct FrameStats {
    int renderWidth = 0;
    int renderHeight = 0;
    int passes = 0;
    int iterations = 0;
};

class Renderer {
public:
    // maxW/maxH come from the platform's largest possible output. Every
    // buffer is allocated once at this size and never resized again.
    bool init(int maxW, int maxH, int contextCount);
    void shutdown();

    // Compiles (or fetches from cache) the core program for this kernel.
    // kb is the morph target. Pass the same kernel (or morph == 0) to compile
    // a single-kernel program, which is the common case and half the work.
    bool setKernel(const KernelDef& k, ColorMode mode,
                   const KernelDef* kb = nullptr);
    bool setChannelBKernel(const KernelDef& k, ColorMode mode);

    // Requests a chain. Returns immediately: if the structure changed, the
    // build is handed to the compiler worker and the CURRENT chain keeps
    // drawing until the new programs are ready. Turning a knob never comes
    // through here -- parameters are uniforms.
    void requestChain(const std::vector<ChainSlot>& chain);

    // Collects any finished compile. Call once per frame before rendering.
    // Returns true on the frame a new chain went live.
    bool pumpCompiler();

    // Compiles synchronously. Used at startup, and as the fallback when the
    // platform cannot give the worker a shared context.
    bool setChainBlocking(const std::vector<ChainSlot>& chain);

    void attachCompiler(AsyncCompiler* c) { compiler_ = c; }
    bool chainPending() const { return pendingToken_ != 0; }

    void setPalettes(int indexA, int indexB);

    // This frame's intrinsic-motion vector, from MotionClock::vector(). Set
    // once per frame before renderScene; it is one uniform upload, not
    // per-pixel or per-iteration work.
    void setMotion(const float m[4]) {
        motion_[0] = m[0]; motion_[1] = m[1];
        motion_[2] = m[2]; motion_[3] = m[3];
    }

    // This frame's zoom journey. Set once per frame before renderScene,
    // alongside setMotion, and for the same reason: the caller owns the
    // clock, so a kernel change or a chain recompile cannot disturb it.
    void setJourney(const JourneyFrame& f) { journey_ = f; }

    // Where the fractal's boundary was last seen, in PLANE coordinates, or
    // invalid when the frame had no boundary in it (all interior, or all
    // exterior). The caller feeds this to Journey::steerTo -- the renderer
    // deliberately does not own the journey, because the journey has to
    // survive a chain recompile and the renderer's own state does not.
    struct FocusPoint { bool valid = false; double x = 0.0, y = 0.0; };
    const FocusPoint& focus() const { return focusPoint_; }

    // Renders the scene into the internal buffers, sized for this output.
    FrameStats renderScene(const ParamBank& params, const Selection& sel,
                           const std::vector<ChainSlot>& chain,
                           double time, double dt, int outW, int outH,
                           float aspect, int ctx);

    // Draws the last scene result into whatever framebuffer is currently
    // bound, scaled to fill it.
    // Degrees CLOCKWISE (0/90/180/270). Turns only what reaches the screen.
    void setRotation(int deg) { rotation_ = deg; }

    void present(int outW, int outH, int ctx);

    Governor& governor() { return governor_; }
    Particles& particles() { return particles_; }
    void setPoolDim(int d) { poolDim_ = d; }
    Ink& ink() { return ink_; }
    const std::string& chainKey() const { return chainKey_; }

private:
    int pickTarget(int cur, int prev) const;
    int pickTarget3(int a, int b, int c) const;
    void bindSourceUniforms(Program& p, int renderW, int renderH);

    int maxW_ = 0, maxH_ = 0;
    // V2 keeps the history/result buffers aliased, but two generators and the
    // binary stage can have both inputs live while particles sample the
    // combined field. Eight is the measured upper bound for this graph.
    Fbo buf_[8];
    FullscreenTri tri_;

    // Core programs are cached by kernel id AND colour mode: flipping back
    // and forth through either must not recompile, or browsing becomes
    // unplayable.
    std::string coreKey_;
    std::string coreKeyB_;
    std::unordered_map<std::string, Program> coreCache_;

    std::vector<Program> passPrograms_;
    CompiledChain compiled_;
    std::string chainKey_;

    // The chain that has been submitted but has not gone live yet. Held so
    // its pass grouping can be installed atomically with its programs.
    AsyncCompiler* compiler_ = nullptr;
    CompiledChain pending_;
    uint64_t pendingToken_ = 0;
    uint64_t nextToken_ = 1;

    // Composites the generator, ink and particles into one image BEFORE the
    // effect chain, so feedback and warp process the layers too.
    Program composite_;
    Program op_;
    Particles particles_;
    Ink ink_;

    float motion_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    JourneyFrame journey_;
    // A tiny render of the scene, read back on the CPU to find the boundary.
    // 64x64 is 16 KB per readback -- small enough that the pipeline stall is
    // not a frame, and far more resolution than picking a steer direction
    // needs.
    static constexpr int kFocusDim = 64;
    // ~4 readbacks a second at 60 fps.
    static constexpr int kFocusEvery = 15;
    void measureFocus(int srcIdx, float aspect, const ParamBank& params,
                      int ctx);
    Fbo focus_;
    FocusPoint focusPoint_;
    int focusTick_ = 0;
    Program present_;
    GLuint paletteTex_ = 0;
    int palA_ = -1, palB_ = -1;

    int resultIdx_ = 0;
    int historyIdx_ = 1;
    // Degrees clockwise applied by the present pass only.
    int rotation_ = 0;
    int lastRenderW_ = 0, lastRenderH_ = 0;

    int poolDim_ = 256;
    ParticleParams lastParticles_;
    Governor governor_;
};

}  // namespace nv

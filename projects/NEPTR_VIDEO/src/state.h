// Engine state, in two layers.
//
//   GLOBAL  survives a patch change: speed, zoom, rotation, centre, detail,
//           palette, resolution. These are what make a set feel continuous
//           when you move between fractals.
//   PATCH   belongs to the current kernel: its own macro knobs.
//
// Every controllable value in the engine lives in ONE flat bank, addressed by
// a stable string id and carrying a normalized 0..1 position alongside its
// real value. That is not incidental tidiness -- it is what lets a sensor, a
// MIDI CC, a slider or an LFO target *anything* through a single mechanism
// instead of three parallel ones.
#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nv {

enum class Curve {
    Linear,    // norm maps straight onto [min,max]
    Exp,       // perceptual for zoom, speed, decay -- min must be > 0
    Bipolar,   // symmetric around zero, with a detent at the centre
    // Symmetric around zero AND perceptual on both sides: fine control near
    // the centre, the whole range at the ends. A cubic taper rather than a
    // geometric one, because a geometric curve has no zero -- it approaches
    // it asymptotically, and would need a floor magnitude just outside the
    // detent, which is a real (if small) discontinuity every time the control
    // crosses the middle. v = sign(t) * max * |t|^3 IS zero at zero, is
    // continuous and differentiable through it, and still gives four times
    // the resolution near the centre that a linear bipolar would.
    BipolarCube,
};

struct ParamDef {
    std::string id;      // stable; what a mapping or a patch file refers to
    std::string label;   // what the panel prints
    std::string group;   // "global", "patch", "feedback", ... for UI layout
    float min = 0.0f;
    float max = 1.0f;
    float def = 0.0f;
    Curve curve = Curve::Linear;

    // Exists in the bank (so uniform packing stays regular) but is never
    // drawn or mapped. Used for the padding that keeps uKernelP four-wide.
    bool hidden = false;

    // GLIDE multiplier. 1 = glides at the instrument's glide time; 2 = twice
    // as slow; 0 = NEVER interpolated.
    //
    // Slew has to be per-parameter because "smoothly" is not a property of the
    // instrument, it is a property of the quantity. A palette rotation wants to
    // glide. A selection -- an LFO's shape, an index into a list -- has no
    // meaningful in-between: interpolating it does not produce a gentler
    // change, it produces a sweep through every value on the way, which is
    // both wrong and, on a shape selector, audible as a stutter of four
    // different waveforms. Those get 0 and snap.
    float slew = 1.0f;
};

class ParamBank {
public:
    // Returns the index. Indices are stable for the lifetime of the bank
    // section; patch params are cleared and re-added on a kernel change.
    int add(const ParamDef& def);

    int count() const { return static_cast<int>(defs_.size()); }
    const ParamDef& def(int i) const { return defs_[i]; }

    // Real units -- what the shader gets. This is the CURRENT position plus
    // whatever the modulators are adding, i.e. the value you can actually see.
    float value(int i) const;
    void setValue(int i, float v);

    // Normalized 0..1 -- what a control surface gets. Everything that maps an
    // input to a parameter goes through here, so all inputs are interchangeable.
    //
    // Three readings, because with slew and modulation in play they genuinely
    // differ and conflating them is how a fader ends up fighting an LFO:
    //
    //   target()   where control asked for it. What a fader draws, what a
    //              preset saves, what the state broadcast sends.
    //   norm()     the slewed current. Chases target() at the glide rate.
    //   normMod()  that, plus the modulators' bipolar offset. What is on screen.
    float target(int i) const { return target_[i]; }
    float norm(int i) const { return norm_[i]; }
    float normMod(int i) const;
    // The target in real units. What capturePreset() writes -- saving value()
    // would bake in whatever an LFO happened to be doing at that instant.
    float targetValue(int i) const;

    void setNorm(int i, float n);
    void nudgeNorm(int i, float delta);
    // Snap: target and current together, no glide, whatever GLIDE is set to.
    //
    // This is for a CONTINUOUS gesture -- a finger dragging a slider. A drag is
    // already sixty positions a second from a hand that is itself the smoother,
    // so slewing it adds pure latency and buys nothing; the control just feels
    // like it is on the end of a rope. Slew exists to smooth the sources that
    // arrive as STEPS -- a preset recall, a CC that jumped from 0 to 127, a
    // mapping that just came back into range.
    void setNormNow(int i, float n);
    void setValueNow(int i, float v);

    int find(const std::string& id) const;
    float valueOf(const std::string& id, float fallback = 0.0f) const;

    void resetToDefaults();

    // ---- glide ------------------------------------------------------------
    //
    // Seconds for a full 0..1 traverse to essentially complete. Zero means
    // instantaneous, and zero is the DEFAULT -- a freshly constructed bank does
    // not glide. That is deliberate: it keeps every existing suite (and any
    // test written later) reading back exactly what it wrote, and it means the
    // only thing that can introduce lag is the engine explicitly asking for it
    // each frame from the GLIDE knob.
    void setGlideSeconds(float s) { glide_ = s < 0.0f ? 0.0f : s; }
    float glideSeconds() const { return glide_; }

    // A preset recall wants a different rate from a knob move: a knob move is
    // de-stepping a control you are holding, a recall is a scene transition and
    // reads as an event of its own. One knob still, though -- a second GLIDE
    // control that only presets used would be two mechanisms for one job. The
    // recall rate is the live rate times a constant.
    static constexpr float kPresetGlideScale = 8.0f;
    float presetGlideSeconds() const { return glide_ * kPresetGlideScale; }

    // Advances every current towards its target. Called ONCE per frame, from
    // the frame loop, with the real elapsed time -- the renderer reads value()
    // many times per frame and must see the same number every time.
    void update(float dt);

    // ---- modulation -------------------------------------------------------
    //
    // A bipolar offset ADDED to the parameter's position, not a value written
    // over it. Overwriting would make every mapped knob dead the moment an LFO
    // pointed at it -- you would turn the fader and nothing would happen, which
    // is the single worst thing a modulation system can do. Added, the fader
    // still moves the whole excursion around, exactly like offsetting a
    // waveform. Several modulators on one target sum.
    //
    // Not slewed: a modulator's output is already continuous, so smoothing it
    // would only phase-shift and shrink it.
    void clearMod();
    void addMod(int i, float bipolarOffset);
    float mod(int i) const { return mod_[i]; }

    // Everything past the global section is dynamic: fixed-width Channel A,
    // Channel B, and morph macro sections, then one entry per effect knob in
    // the chain. The whole tail is rebuilt whenever structure changes.
    //
    //   beginRebuild()  snapshots the dynamic values by id, then truncates
    //   addDynamic()    adds, restoring the snapshot if that id survived
    //   endRebuild()    drops the snapshot
    //
    // The snapshot is why changing one effect does not reset every other
    // knob on the instrument.
    void markGlobalEnd() { globalEnd_ = static_cast<int>(defs_.size()); }
    void beginRebuild();
    int addDynamic(const ParamDef& def);
    void endRebuild();

    int globalEnd() const { return globalEnd_; }

private:
    void rebuildIndex();

    std::vector<ParamDef> defs_;
    // Ids remain the public address. This cache only memoizes find(); it is
    // rebuilt whenever the dynamic tail is rebuilt and is never exposed as a
    // positional contract.
    std::unordered_map<std::string, int> index_;
    // Where control put it, where it currently is, and what the modulators are
    // adding this frame. Three parallel arrays rather than a struct because the
    // slew loop walks all of them linearly, every frame, for the whole bank.
    std::vector<float> target_;
    std::vector<float> norm_;
    std::vector<float> mod_;
    // Per-parameter glide time in seconds, captured WHEN THE TARGET WAS SET.
    // A preset recall's slow flight has to stay slow for its whole duration
    // even though the live rate is reapplied every frame after it.
    std::vector<float> tau_;
    float glide_ = 0.0f;
    int globalEnd_ = 0;
    std::vector<std::pair<std::string, float>> preserved_;
};

// Sets the glide time for the lifetime of the scope, then puts it back. The
// call site that means "this is a preset recall, not a knob move" says so in
// one line and cannot forget to restore it.
class GlideScope {
public:
    GlideScope(ParamBank& b, float seconds) : bank_(b), prev_(b.glideSeconds()) {
        b.setGlideSeconds(seconds);
    }
    ~GlideScope() { bank_.setGlideSeconds(prev_); }
    GlideScope(const GlideScope&) = delete;
    GlideScope& operator=(const GlideScope&) = delete;

private:
    ParamBank& bank_;
    float prev_;
};

// Stable ids for the global layer. Referenced by the renderer, the panel UI
// and (later) the mapping table, so they are named once, here.
namespace gid {
constexpr const char* kSpeed        = "g.speed";
// How far the GENERATOR'S OWN parameters travel under the scene clock. Not
// "p.motion" -- that is the particles' undirected energy and lives on a
// different page. This one is the amount of the intrinsic motion every
// escape kernel now carries: the Julia seed walking its circle, the plane's
// twist, Newton's relaxation. At exactly 0 every kernel reduces to the
// static maths it had before the knob existed, which is what makes a still
// image reachable and what keeps a morph pair identical at the ends.
constexpr const char* kEvolve       = "g.evolve";
constexpr const char* kJourney      = "g.journey";
constexpr const char* kZoom         = "g.zoom";
constexpr const char* kRotation     = "g.rotation";
constexpr const char* kCenterX      = "g.center_x";
constexpr const char* kCenterY      = "g.center_y";
constexpr const char* kDetail       = "g.detail";
constexpr const char* kRenderScale  = "g.render_scale";
constexpr const char* kPaletteMix   = "g.palette_mix";
constexpr const char* kPaletteShift = "g.palette_shift";
constexpr const char* kPaletteSat   = "g.palette_sat";
constexpr const char* kPaletteSpread = "g.palette_spread";
constexpr const char* kStripeFreq   = "g.stripe_freq";
constexpr const char* kPrism        = "g.prism";
constexpr const char* kMorph        = "g.morph";
// How long a parameter takes to reach where it was just sent. Global on
// purpose: one GLIDE, with a per-parameter multiplier in the ParamDef, rather
// than forty glide knobs nobody would ever set individually.
constexpr const char* kGlide        = "g.glide";
// The generator's own visibility, so a mode can run over it or on black.
constexpr const char* kFractalLevel = "g.fractal_level";
constexpr const char* kOpMode       = "op.mode";
constexpr const char* kOpMix        = "op.mix";
constexpr const char* kOpVal        = "op.val";
constexpr const char* kOpUseVal     = "op.use_val";
constexpr const char* kOpSwap       = "op.swap";
constexpr const char* kOpPlane      = "op.plane";
// Particles.
constexpr const char* kPtCount      = "p.count";
constexpr const char* kPtSize       = "p.size";
constexpr const char* kPtSpeed      = "p.speed";
constexpr const char* kPtGravity    = "p.gravity";
constexpr const char* kPtSwirl      = "p.swirl";
constexpr const char* kPtMotion     = "p.motion";
constexpr const char* kPtDamping    = "p.damping";
constexpr const char* kPtLife       = "p.life";
constexpr const char* kPtBright     = "p.bright";
constexpr const char* kPtFieldColor = "p.field_color";
constexpr const char* kPtCohesion   = "p.cohesion";
constexpr const char* kPtCarry      = "p.carry";
// What a particle can die OF. All zero by default: the swarm then recycles on
// the timer alone, exactly as it always did.
constexpr const char* kPtDieInterior = "p.die_interior";
constexpr const char* kPtDieEdge     = "p.die_edge";
constexpr const char* kPtDieExhaust  = "p.die_exhaust";
// How much of the swarm each frame writes into the ink layer -- the actors
// modifying the world that moves them.
constexpr const char* kPtDeposit     = "p.deposit";
// Drawing.
constexpr const char* kInkSize      = "d.size";
constexpr const char* kInkFlow      = "d.flow";
constexpr const char* kInkSoft      = "d.soft";
constexpr const char* kInkFade      = "d.fade";
constexpr const char* kInkFieldColor = "d.field_color";
// Feedback has no globals: the FEEDBACK effect owns those knobs.
}  // namespace gid

// Registers the global layer. Called once at startup, before any patch loads.
void registerGlobalParams(ParamBank& bank);

// What the primary layer is. The fractal is always available underneath as a
// modulation source AND as an image, so a mode is not a separate program --
// it decides what gets composited on top of the generator before the chain.
enum class Mode {
    Synth = 0,   // the generator alone
    Particles,   // a swarm steered by the generator's field
    Draw,        // a persistent ink layer, brushed with the generator's colour
    kCount
};

enum class SourceKind {
    Fractal = 0,
    Video = 1,
    Draw = 2,
    Stream = 3,
    None = 255,
};

const char* modeName(Mode m);

// Non-parameter state: discrete selections, not continuous knobs.
struct Selection {
    Mode mode = Mode::Synth;
    int kernelIndex = 0;
    int kernelB = 0;      // morph target
    SourceKind sourceA = SourceKind::Fractal;
    SourceKind sourceB = SourceKind::None;
    int channelBKernel = 0;
    int paletteA = 1;   // SPECTRUM: the instrument opens in colour
    int paletteB = 7;   // ULTRAVIOLET

    // Colouring is independent of the fractal family. That independence is
    // where most of the bank's real size comes from: N families x M colourings
    // x T trap shapes is a very large space out of a very small amount of code.
    int colorMode = 0;   // ColorMode
    int trapShape = 0;   // TrapShape
    float trapScale = 1.0f;

    bool panelRendersScene = false;  // true when no projector is attached
};

}  // namespace nv

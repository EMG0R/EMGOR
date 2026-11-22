#include "state.h"

#include <algorithm>
#include <cmath>

#include "mod.h"
#include "chain/ops.h"

namespace nv {

static float applyCurve(const ParamDef& d, float n) {
    n = std::clamp(n, 0.0f, 1.0f);
    switch (d.curve) {
        case Curve::Linear:
            return d.min + (d.max - d.min) * n;
        case Curve::Exp:
            // min must be > 0; geometric interpolation so a slider feels even
            // across four decades of zoom.
            return d.min * std::pow(d.max / d.min, n);
        case Curve::Bipolar: {
            // Centre of travel is exactly zero, with a small detent so you can
            // find "no rotation" with a finger.
            float t = n * 2.0f - 1.0f;
            if (std::fabs(t) < 0.02f) t = 0.0f;
            return t * std::max(std::fabs(d.min), std::fabs(d.max));
        }
        case Curve::BipolarCube: {
            // Same centre detent, so "stopped" is findable with a finger and
            // is EXACTLY stopped rather than a thousandth of a unit of drift.
            // The cube then gives the perceptual feel: half the travel is an
            // eighth of the range, so slow is where the resolution is.
            float t = n * 2.0f - 1.0f;
            if (std::fabs(t) < 0.02f) t = 0.0f;
            return t * t * t * std::max(std::fabs(d.min), std::fabs(d.max));
        }
    }
    return d.def;
}

static float inverseCurve(const ParamDef& d, float v) {
    switch (d.curve) {
        case Curve::Linear:
            return d.max == d.min ? 0.0f : (v - d.min) / (d.max - d.min);
        case Curve::Exp:
            return std::log(v / d.min) / std::log(d.max / d.min);
        case Curve::Bipolar: {
            float range = std::max(std::fabs(d.min), std::fabs(d.max));
            return range == 0.0f ? 0.5f : (v / range + 1.0f) * 0.5f;
        }
        case Curve::BipolarCube: {
            float range = std::max(std::fabs(d.min), std::fabs(d.max));
            if (range == 0.0f) return 0.5f;
            float r = v / range;
            float t = std::cbrt(r);   // exact inverse, sign-preserving
            return (t + 1.0f) * 0.5f;
        }
    }
    return 0.0f;
}

int ParamBank::add(const ParamDef& def) {
    float n = std::clamp(inverseCurve(def, def.def), 0.0f, 1.0f);
    defs_.push_back(def);
    // A new parameter starts AT its default, not gliding towards it: an
    // instrument that spends its first second sliding out of zero on every
    // kernel change would look like it was still loading.
    target_.push_back(n);
    norm_.push_back(n);
    mod_.push_back(0.0f);
    tau_.push_back(0.0f);
    int i = static_cast<int>(defs_.size()) - 1;
    index_[def.id] = i;
    return i;
}

float ParamBank::normMod(int i) const {
    return std::clamp(norm_[i] + mod_[i], 0.0f, 1.0f);
}

float ParamBank::value(int i) const { return applyCurve(defs_[i], normMod(i)); }

float ParamBank::targetValue(int i) const {
    return applyCurve(defs_[i], target_[i]);
}

void ParamBank::setNorm(int i, float n) {
    target_[i] = std::clamp(n, 0.0f, 1.0f);
    // The glide time for THIS move, frozen now. slew == 0 is the opt-out and
    // lands immediately whatever GLIDE says.
    tau_[i] = glide_ * defs_[i].slew;
    if (tau_[i] <= 0.0f) norm_[i] = target_[i];
}

void ParamBank::setValue(int i, float v) {
    setNorm(i, inverseCurve(defs_[i], v));
}

void ParamBank::setNormNow(int i, float n) {
    target_[i] = std::clamp(n, 0.0f, 1.0f);
    norm_[i] = target_[i];
    tau_[i] = 0.0f;
}

void ParamBank::setValueNow(int i, float v) {
    setNormNow(i, inverseCurve(defs_[i], v));
}

// Relative moves accumulate on the TARGET, not on the current position.
// Nudging the current would make an encoder's travel depend on how far the
// glide had got: turn it fast under a long GLIDE and each detent would move
// less than the one before.
void ParamBank::nudgeNorm(int i, float delta) { setNorm(i, target_[i] + delta); }

void ParamBank::clearMod() {
    std::fill(mod_.begin(), mod_.end(), 0.0f);
}

void ParamBank::addMod(int i, float offset) {
    if (i < 0 || i >= static_cast<int>(mod_.size())) return;
    mod_[i] += offset;
}

void ParamBank::update(float dt) {
    if (dt <= 0.0f) return;
    for (size_t i = 0; i < norm_.size(); ++i) {
        float tau = tau_[i];
        if (tau <= 0.0f) {
            norm_[i] = target_[i];
            continue;
        }
        float d = target_[i] - norm_[i];
        // Exponential approach, with the coefficient computed from dt rather
        // than being dt * k. That matters: this engine drops the odd 290 ms
        // frame, and a linear coefficient of dt/tau would exceed 1 on such a
        // frame and overshoot -- at 2 it oscillates forever. 1 - exp(-dt/tau)
        // is bounded by 1 for any dt, so a stalled frame lands ON the target
        // instead of past it, and the glide is the same shape at 30 fps as at
        // 120 rather than being a per-frame fraction in disguise.
        //
        // tau is the time to close ~63% of the gap, so the full traverse
        // "completes" in about 4 tau. Divide, so the knob reads as the time
        // the move actually takes.
        float k = 1.0f - std::exp(-dt / (tau * 0.25f));
        norm_[i] += d * k;
        // Exponentials are asymptotic and a preset must land EXACTLY where it
        // was saved, not a millionth away from it forever. Below a threshold
        // finer than any control surface or display can resolve, snap.
        if (std::fabs(target_[i] - norm_[i]) < 1e-5f) norm_[i] = target_[i];
    }
}

int ParamBank::find(const std::string& id) const {
    auto it = index_.find(id);
    return it == index_.end() ? -1 : it->second;
}

float ParamBank::valueOf(const std::string& id, float fallback) const {
    int i = find(id);
    return i < 0 ? fallback : value(i);
}

void ParamBank::resetToDefaults() {
    // Through setNorm, so a reset GLIDES like everything else. Reset is a
    // performance gesture -- it is the one place where watching forty
    // parameters sail home at once is the point.
    for (size_t i = 0; i < defs_.size(); ++i) {
        setNorm(static_cast<int>(i),
                std::clamp(inverseCurve(defs_[i], defs_[i].def), 0.0f, 1.0f));
    }
}

void ParamBank::beginRebuild() {
    preserved_.clear();
    preserved_.reserve(defs_.size() - globalEnd_);
    // The TARGET is what survives, not the in-flight position: a knob that was
    // half way through a glide when the chain changed was on its way
    // somewhere, and arriving is what it was asked to do.
    for (size_t i = globalEnd_; i < defs_.size(); ++i) {
        preserved_.emplace_back(defs_[i].id, target_[i]);
    }
    defs_.resize(globalEnd_);
    target_.resize(globalEnd_);
    norm_.resize(globalEnd_);
    mod_.resize(globalEnd_);
    tau_.resize(globalEnd_);
    rebuildIndex();
}

int ParamBank::addDynamic(const ParamDef& def) {
    int i = add(def);
    for (const auto& kv : preserved_) {
        if (kv.first == def.id) {
            // Restored, not glided to: this knob is not moving, it is the same
            // knob it was a moment ago and the bank underneath it was rebuilt.
            target_[i] = kv.second;
            norm_[i] = kv.second;
            break;
        }
    }
    return i;
}

void ParamBank::endRebuild() { preserved_.clear(); }

void ParamBank::rebuildIndex() {
    index_.clear();
    index_.reserve(defs_.size());
    for (size_t i = 0; i < defs_.size(); ++i) {
        index_[defs_[i].id] = static_cast<int>(i);
    }
}

void registerGlobalParams(ParamBank& b) {
    // Time and space.
    // The scene clock's RATE, and it is signed. Time is accumulated
    // (sceneTime += dt * speed), never derived from the wall clock, which is
    // what makes a negative rate a smooth rewind rather than a jump: changing
    // speed changes the derivative of the phase, and the phase itself never
    // moves. Exactly 0 is a true freeze -- the accumulator stops, so the
    // generator, the motion path and the LFOs all hold the frame they are on.
    //
    // BipolarCube, so the detent at the centre is a real stop and the useful
    // slow speeds get most of the travel: 1x sits at exactly half-way out from
    // the centre (0.5^3 * 8 = 1.0), and the ends are the daft ones.
    b.add({gid::kSpeed,    "SPEED",  "global", -8.0f, 8.0f,  1.0f, Curve::BipolarCube});
    // How much intrinsic motion the GENERATOR carries -- see gid::kEvolve.
    // Default 1: a preset written before this knob existed loads without the
    // key, keeps the default, and therefore comes up moving, which is the
    // whole point of the change.
    b.add({gid::kEvolve,   "EVOLVE", "global", 0.0f, 2.0f, 1.0f, Curve::Linear});
    // Four decades of zoom is roughly where fp32 gives out; the precision
    // tiers extend this later without the control changing meaning.
    // JOURNEY: decades of zoom per second of SCENE time -- the knob that
    // makes the picture ADVANCE rather than oscillate. Bipolar so the travel
    // can be inverted independently of SPEED (SPEED reverses the whole
    // instrument; JOURNEY reverses only the descent), and 0 parks the journey
    // without freezing anything else. Default 0.25: about 48 s per dive,
    // slow enough to read as flying rather than as a zoom effect.
    b.add({gid::kJourney,  "JRNY",   "global", -2.0f, 2.0f, 0.25f, Curve::Bipolar});
    b.add({gid::kZoom,     "ZOOM",   "global", 1e-4f, 1e2f,  1.0f, Curve::Exp});
    b.add({gid::kRotation, "ROT",    "global", -3.14159f, 3.14159f, 0.0f, Curve::Bipolar});
    b.add({gid::kCenterX,  "X",      "global", -2.0f, 2.0f,  0.0f, Curve::Bipolar});
    b.add({gid::kCenterY,  "Y",      "global", -2.0f, 2.0f,  0.0f, Curve::Bipolar});

    // Detail is the iteration ceiling. It is simultaneously the most
    // expensive knob and the most beautiful one, which is what lets the
    // governor defend the framerate without the result reading as a fault.
    b.add({gid::kDetail,   "DETAIL", "global", 8.0f, 512.0f, 96.0f, Curve::Exp});

    // Master resolution, as a fraction of the output. Live-changeable with no
    // reallocation because every buffer is allocated at max and sub-viewported.
    b.add({gid::kRenderScale, "RES",  "global", 0.15f, 2.0f, 1.0f, Curve::Exp});

    // How long a parameter takes to arrive where it was just sent. This is the
    // difference between a preset recall being a cut and being a move, and
    // between a CC step and a slide. Default 80 ms: enough that nothing
    // arrives as a step, short enough that a knob still feels connected.
    //
    // Its own slew is 0 for the obvious reason -- a glide time that glides
    // would mean the instrument kept changing rate for a second after you set
    // the rate, and the knob would never quite read as what it did.
    {
        ParamDef d{gid::kGlide, "GLIDE", "global", 0.0f, 4.0f, 0.08f, Curve::Linear};
        d.slew = 0.0f;
        b.add(d);
    }

    // Morph towards the B kernel. Not a crossfade -- it interpolates the
    // iteration itself, so the middle of the travel is a fractal that exists
    // nowhere in the bank.
    b.add({gid::kMorph,    "MORPH",  "global", 0.0f, 1.0f, 0.0f, Curve::Linear});

    // Colour. Labels carry no group prefix -- the panel prints the group
    // itself, and repeating it reads as stutter.
    b.add({gid::kPaletteMix,   "MIX", "palette", 0.0f, 1.0f, 0.0f, Curve::Linear});
    b.add({gid::kPaletteShift, "ROT", "palette", 0.0f, 1.0f, 0.0f, Curve::Linear});
    b.add({gid::kPaletteSat,   "SAT", "palette", 0.0f, 1.5f, 1.0f, Curve::Linear});
    // How many times the palette repeats across the structure. At 1 it is a
    // single gradient; above that it becomes banding that follows the
    // fractal's own contours, which is what SHIFT then sweeps along.
    b.add({gid::kPaletteSpread, "SPREAD", "palette", 0.25f, 16.0f, 1.0f, Curve::Exp});
    // Stripe-average frequency: how many bands the orbit's angle generates.
    b.add({gid::kStripeFreq,   "STRIPE", "palette", 1.0f, 32.0f, 6.0f, Curve::Exp});
    // Chromatic split, driven by the orbit trap in PRISM mode.
    b.add({gid::kPrism,        "PRISM", "palette", 0.0f, 0.25f, 0.05f, Curve::Linear});

    // NOTE: temporal feedback is deliberately NOT here. It belongs to the
    // FEEDBACK effect, which owns its amount/decay/warp/rotate/zoom knobs
    // through the same bank. A second set of feedback globals would be two
    // mechanisms for one job, and the one nothing read would rot.

    // How much of the generator shows through. At 0 a mode runs on black; at
    // 1 the particles or ink sit over the fractal that is steering them.
    b.add({gid::kFractalLevel, "FRACTAL", "layer", 0.0f, 1.0f, 1.0f, Curve::Linear});

    // jit.op controls. OP is stepped but remains a bank parameter so the
    // existing mapping mechanism can intentionally sweep the catalogue.
    {
        ParamDef d{gid::kOpMode, "OP", "operator", 0.0f,
                   static_cast<float>(jitOpBank().size() - 1), 0.0f,
                   Curve::Linear};
        d.slew = 0.0f;
        b.add(d);
    }
    b.add({gid::kOpMix, "MIX", "operator", 0.0f, 1.0f, 1.0f, Curve::Linear});
    b.add({gid::kOpVal, "VAL", "operator", 0.0f, 1.0f, 0.0f, Curve::Linear});
    {
        ParamDef d{gid::kOpUseVal, "USE VAL", "operator", 0.0f, 1.0f, 0.0f, Curve::Linear};
        d.slew = 0.0f;
        b.add(d);
    }
    {
        ParamDef d{gid::kOpSwap, "SWAP", "operator", 0.0f, 1.0f, 0.0f, Curve::Linear};
        d.slew = 0.0f;
        b.add(d);
    }
    {
        ParamDef d{gid::kOpPlane, "PLANE", "operator", 0.0f, 2.0f, 0.0f, Curve::Linear};
        d.slew = 0.0f;
        b.add(d);
    }

    // ---- particles -----------------------------------------------------
    b.add({gid::kPtCount,    "COUNT",  "particles", 0.02f, 1.0f, 0.5f, Curve::Exp});
    b.add({gid::kPtSize,     "SIZE",   "particles", 1.0f, 24.0f, 3.0f, Curve::Exp});
    b.add({gid::kPtSpeed,    "SPEED",  "particles", 0.0f, 4.0f, 1.0f, Curve::Linear});
    // The two ways the fractal steers the swarm: along its gradient, and
    // along the perpendicular. Bipolar so either can run backwards.
    // FRACTAL GRAVITY: how hard the generator's structure pulls the swarm
    // in. This is what gives the particles the shape of the thing they are
    // moving through -- at zero they are just dust.
    b.add({gid::kPtGravity,  "GRAVITY", "particles", -4.0f, 4.0f, 1.5f, Curve::Bipolar});
    // Along the contour rather than across it, so they circulate.
    b.add({gid::kPtSwirl,    "SWIRL",  "particles", -4.0f, 4.0f, 1.0f, Curve::Bipolar});
    // Undirected energy. Gravity gives structure, motion fights it.
    b.add({gid::kPtMotion,   "MOTION", "particles", 0.0f, 2.0f, 0.15f, Curve::Linear});
    b.add({gid::kPtDamping,  "DAMP",   "particles", 0.5f, 0.999f, 0.94f, Curve::Linear});
    b.add({gid::kPtLife,     "LIFE",   "particles", 0.2f, 30.0f, 6.0f, Curve::Exp});
    b.add({gid::kPtBright,   "BRIGHT", "particles", 0.0f, 3.0f, 1.0f, Curve::Linear});
    // 0 = carry the generator's actual colour (a cohesive swarm then
    // reproduces the picture exactly); 1 = re-read the palette by brightness.
    b.add({gid::kPtFieldColor, "TINT", "particles", 0.0f, 1.0f, 0.0f, Curve::Linear});
    // THE knob. At 1 the pool tiles the screen and reconstructs the image --
    // it is secretly particles the whole time. At 0 it is a free swarm.
    b.add({gid::kPtCohesion, "COHESION", "particles", 0.0f, 1.0f, 1.0f, Curve::Linear});
    // 1 = pieces keep the colour of where they came from (the image
    // shatters); 0 = they take the colour of where they are (dust drifting).
    b.add({gid::kPtCarry,    "CARRY",   "particles", 0.0f, 1.0f, 1.0f, Curve::Linear});

    // Causes of death. LIFE is still the scheduler; these are reasons, and
    // each is a RATE PER SECOND while its condition holds rather than a
    // duration -- so 0 is off, and turning one up shortens a life instead of
    // setting one. Defaults are zero, which is what keeps every preset written
    // before they existed looking exactly the same.
    //
    // SINK: the generator's interior is black -- zero field, zero gradient --
    // so particles that wander in find no force and pool there forever. This
    // is what makes that a death rather than a car park.
    b.add({gid::kPtDieInterior, "SINK",  "particles", 0.0f, 8.0f, 0.0f, Curve::Linear});
    // EDGE: 0 wraps (right for a world with no outside), 1 dies at the frame
    // (right for a world you can leave). In between, some of them make it.
    b.add({gid::kPtDieEdge,     "EDGE",  "particles", 0.0f, 1.0f, 0.0f, Curve::Linear});
    // BURN: a particle thrown hard by MOTION exhausts; one drifting gently
    // persists. The only cause that MOTION and SPEED can drive on their own.
    b.add({gid::kPtDieExhaust,  "BURN",  "particles", 0.0f, 8.0f, 0.0f, Curve::Linear});
    // DEPOSIT: the swarm writes itself into the ink layer, which is composited
    // into the field the swarm reads. A closed loop, bounded because the
    // deposit is a mix and not an accumulation.
    b.add({gid::kPtDeposit,     "DEPOSIT", "particles", 0.0f, 1.0f, 0.0f, Curve::Linear});

    // ---- drawing -------------------------------------------------------
    b.add({gid::kInkSize,    "SIZE",   "draw", 0.002f, 0.3f, 0.04f, Curve::Exp});
    b.add({gid::kInkFlow,    "FLOW",   "draw", 0.02f, 1.0f, 0.6f, Curve::Linear});
    b.add({gid::kInkSoft,    "SOFT",   "draw", 0.0f, 1.0f, 0.6f, Curve::Linear});
    // 0 keeps strokes forever; above that they decay on their own.
    b.add({gid::kInkFade,    "FADE",   "draw", 0.0f, 3.0f, 0.0f, Curve::Linear});
    b.add({gid::kInkFieldColor, "TINT", "draw", 0.0f, 4.0f, 1.0f, Curve::Linear});

    // ---- modulation ----------------------------------------------------
    // In the GLOBAL section, before the mark: modulation belongs to the
    // instrument, not to whichever kernel is loaded. An LFO that reset every
    // time you changed fractal would be unusable, and its INDEX has to be
    // stable too because the renderer and the panel walk the bank by index.
    registerModParams(b);

    b.markGlobalEnd();
}

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Synth:     return "SYNTH";
        case Mode::Particles: return "PARTICLES";
        case Mode::Draw:      return "DRAW";
        default:              return "?";
    }
}

}  // namespace nv

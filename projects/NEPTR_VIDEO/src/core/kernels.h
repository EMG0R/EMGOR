// The fractal bank.
//
// A kernel is not a shader. It is a few lines of GLSL describing ONE iteration
// step, plus its parameter metadata. The core program is a single template
// that gets the step substituted into it, so the bank can grow to hundreds of
// entries without hundreds of files -- and every entry automatically inherits
// zoom, rotation, detail, colouring, orbit traps and feedback.
//
// The iterate state is a vec4, not a vec2: .xy is z, .zw is auxiliary state.
// That one decision is what lets second-order systems (Phoenix, Nova, Secant)
// live in the same machinery as z^2+c instead of needing their own path.
#pragma once

#include <string>
#include <vector>

#include "state.h"

namespace nv {

enum class SeedMode {
    Mandel,  // z starts at origin, c is the pixel   -> parameter-space map
    Julia,   // z starts at the pixel, c is a knob   -> dynamical-plane map
};

// Two kinds of generator, because not everything worth looking at is an
// iterated orbit.
//
//   Escape  iterate z per pixel and colour by what the orbit did. Fractals.
//   Field   evaluate a function of position (and time) directly. Noise,
//           domain warps, cellular/giraffe patterns, lava lamps, plasma.
//
// Both land in the same palette, both obey zoom/rotation/centre/speed, and
// both feed the same effect chain -- so a chill flowing noise field and a
// Burning Ship are the same instrument, not two programs.
enum class KernelKind {
    Escape,
    Field,
};

struct KernelParam {
    std::string id;
    std::string label;
    float min, max, def;
    Curve curve = Curve::Linear;
};

struct KernelDef {
    std::string id;
    std::string name;
    std::string family;   // grouping for the browse UI

    KernelKind kind = KernelKind::Escape;

    // Escape kernels: GLSL body of  vec4 nvStep(vec4 s, vec2 c, vec4 p)
    // s.xy = z, s.zw = aux state, c = seed, p = the four macro knobs.
    std::string step;

    // Field kernels: GLSL body of  float nvField(vec2 p, float t, vec4 k)
    // Returns 0..1. p is the aspect-corrected plane coordinate after zoom,
    // rotation and centre; t is the engine clock (already scaled by SPEED).
    std::string field;

    SeedMode seed = SeedMode::Mandel;
    float escapeR2 = 4.0f;      // bailout radius squared
    bool convergent = false;    // Newton-like: escape on |dz| small, not large

    std::vector<KernelParam> params;  // up to 4, mapped to p.x..p.w

    // GLSL for the INITIAL z, when the seed mode's own answer is wrong for
    // this map. Empty means the seed mode decides: Mandel starts z at the
    // origin, Julia starts it at the pixel. That is right for z^2 + c and
    // wrong for the logistic map, whose origin is a FIXED POINT -- z(1-z) at
    // z = 0 is 0 forever, every pixel is "interior", and LAMBDA rendered a
    // black screen from the day it was added. A map is entitled to say where
    // its orbit starts.
    std::string zInit;

    // GLSL for the two auxiliary components of the initial state, i.e. the
    // "zw" of vec4(z, <auxInit>). Empty starts it at zero. Kernels whose seed
    // itself evolves (Spider) carry their initial seed here.
    std::string auxInit;

    // How far this generator rewards being ZOOMED INTO, in decades per dive.
    // 0 means it does not travel at all: it holds normal position and its
    // motion comes from EVOLVE alone.
    //
    // This is not a stylistic preference, it is a property of the map. The
    // journey descends toward a point on the MANDELBROT boundary (see the
    // target table in journey.h), because that is where structure is known to
    // regenerate all the way down. Three different things can be true of a
    // kernel:
    //
    //   * It IS that boundary -- MANDELBROT -- and a five-decade dive is
    //     resolved the whole way.
    //   * It is a different map that happens to have structure near those
    //     coordinates. It survives a shallower dive and then runs out.
    //   * It has no self-similar structure to descend into at all -- every
    //     field kernel, because value noise, Voronoi and metaballs have a
    //     finite number of octaves and go smooth the moment you pass them.
    //     Zooming one is not a journey, it is a slow fade to a flat colour.
    //
    // Which of the three a kernel is was MEASURED, not assumed: see the
    // depth-sweep numbers recorded beside the table in kernels.cpp. Guessing
    // here fails in the specific way that looks like a broken renderer -- a
    // dive that ends on a solid screen.
    float journeyDepth = 0.0f;

    // Where on THIS map's plane a dive should aim, in plane coordinates.
    //
    // Empty means fall back to the journey's own seed table, which holds
    // MANDELBROT coordinates -- right for one kernel in the bank and
    // arbitrary for the rest, which is why most of them measured as having no
    // dive. Several entries rather than one so successive dives land
    // somewhere different; a single aim would make the instrument loop with a
    // period of one dive. Measured by tests/target_search.py; see the table
    // in kernels.cpp.
    std::vector<double> journeyAims;   // x, y, x, y, ...
};

// The bank. Static, built once; a "huge bank" is a matter of adding entries
// here, and nothing else in the engine has to change.
const std::vector<KernelDef>& kernelBank();

// Colouring is orthogonal to the family, which is where most of the bank's
// real size comes from: every mode reads the same iterate, so N families x
// M colourings is N*M looks for the cost of N+M.
//
// Every mode reads a quantity the ITERATION ITSELF produces, so the colour is
// a property of the fractal rather than a gradient laid over a picture of it.
// Shifting the palette then sweeps colour ALONG the structure.
//
// Each mode declares which features it needs; the core shader computes only
// those (they are #defines, resolved at compile time) so an unused feature
// costs nothing in the inner loop. Programs are cached per kernel x mode.
enum class ColorMode {
    SmoothEscape = 0,  // continuous escape time, no banding
    OrbitTrap,         // closest approach of the orbit to a shape
    FinalAngle,        // arg(z) at bailout
    Derivative,        // step size at bailout; clean filamentary edges
    Stripe,            // stripe average: paints the orbit's DYNAMICS
    Curvature,         // how sharply the orbit turned, averaged
    Prism,             // escape sets position, trap splits R/G/B apart
    Dual,              // angle and escape drive the palette independently
    kCount
};

// Which intrinsic quantities a mode needs. Drives the #defines.
struct ColorFeatures {
    bool angle = false;
    bool stripe = false;
    bool curvature = false;
    bool trap = false;
};
ColorFeatures featuresFor(ColorMode m);

// Two kernels can be morphed only if they iterate the same WAY: two escape
// kernels blend their step functions, two fields blend their values. An
// escape kernel and a field have nothing in common to interpolate, so that
// pairing is refused rather than faked with a crossfade.
inline bool canMorph(const KernelDef& a, const KernelDef& b) {
    return a.kind == b.kind;
}

const char* colorModeName(ColorMode m);

enum class TrapShape {
    Point = 0, Cross, Circle, Line, kCount
};

const char* trapShapeName(TrapShape t);

}  // namespace nv

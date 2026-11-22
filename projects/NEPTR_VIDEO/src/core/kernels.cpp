#include "core/kernels.h"

namespace nv {

// Every step body may use the complex helpers from shaders/core/complex.glsl:
//   cmul cdiv cinv cconj csqr cpowr cexp clog csin ccos cabs2
// s.xy is z, s.zw is auxiliary state, c is the seed, p.x..p.w are the knobs.
//
// ---- m: intrinsic motion ---------------------------------------------------
//
// An escape kernel is an iterated orbit and therefore a PURE FUNCTION OF
// POSITION. Nothing in z <- z^2 + c mentions time, so the picture is frozen
// however fast the scene clock runs -- which is exactly what most of this bank
// did, and why only the field kernels appeared to be alive. A global wobble
// applied on top would be a post-process wearing the fractal's clothes, so
// instead every kernel below animates SOMETHING OF ITS OWN, and says so here.
//
// `m` is four numbers computed once per frame on the CPU (see MotionClock in
// renderer.h), never per pixel and never per iteration:
//
//   m.xy   A * (cos phi, sin phi)  -- a CLOSED circle. A closed path is the
//          point: a seed that merely drifted would wander out of the region
//          where the fractal is interesting and never come back.
//   m.z    A * sin(phi/2), m.w  A * sin(phi/3) -- two more bounded oscillators
//          at rates that are not the circle's, for the kernels that would
//          rather move a real-valued number of their own: Newton's relaxation,
//          Phoenix's feedback coefficient, the Multibrot exponent, Spider's
//          seed decay, Magnet's pole. Three rates rather than one so the bank
//          does not breathe in lockstep and no single beat is visible across it.
//
// What is deliberately NOT here is a rotation. The first cut multiplied z^2 by
// a unit complex number, which is a similarity of the plane, and it looked
// exactly like what it was: the picture spinning. Everything below instead
// moves a number the map is DEFINED BY -- an exponent, a seed, a relaxation, a
// pole, a fold -- so every frame is an honest escape-time fractal of a
// genuinely different map and the structure re-forms rather than turning.
// Where a z^2 kernel has no such number, the squared value is squeezed
// anisotropically (real and imaginary scaled by reciprocal amounts), which is
// non-conformal and therefore cannot read as a camera move.
//
// A is the EVOLVE knob. At A = 0, m = (0, 0, 0, 0) and every body below is
// algebraically identical to the static version it replaced -- which is what
// makes "hold still" a real option and what keeps a morph pair's endpoints
// exactly the fractals they name.
//
// Cost: the squeeze is two multiplies per iteration; a seed circle is two
// multiplies and two adds; the scalar forms are one of each. No transcendental
// function was added to the inner loop, and nothing here is per-pixel work --
// the four numbers are computed once per frame on the CPU.


// Named constructors. The struct has enough fields that positional
// initialisers were a trap -- inserting one member silently shifted every
// entry in the bank.
static KernelDef esc(const char* id, const char* name, const char* family,
                     const char* step, SeedMode seed, float escapeR2,
                     bool convergent, std::vector<KernelParam> params = {},
                     const char* auxInit = "", const char* zInit = "") {
    KernelDef k;
    k.id = id;
    k.name = name;
    k.family = family;
    k.kind = KernelKind::Escape;
    k.step = step;
    k.seed = seed;
    k.escapeR2 = escapeR2;
    k.convergent = convergent;
    k.params = std::move(params);
    k.auxInit = auxInit;
    k.zInit = zInit;
    return k;
}

static KernelDef fld(const char* id, const char* name, const char* family,
                     const char* field, std::vector<KernelParam> params = {}) {
    KernelDef k;
    k.id = id;
    k.name = name;
    k.family = family;
    k.kind = KernelKind::Field;
    k.field = field;
    k.params = std::move(params);
    return k;
}

static std::vector<KernelDef> buildBank() {
    std::vector<KernelDef> k;

    // ---- escape-time, the classical core -------------------------------
    k.push_back(esc("mandelbrot", "MANDELBROT", "escape",
                 // A NON-CONFORMAL squeeze of the squared value, which is the
                 // difference between the set evolving and the set spinning. A
                 // twist (z^2 * e^(i th)) was the first cut and it reads as the
                 // picture rotating, because a rotation is exactly what it is.
                 // Scaling the real and imaginary parts by reciprocal amounts is
                 // not a similarity of the plane, so it cannot be seen as a
                 // camera move: the cardioid changes SHAPE, bulbs swell and
                 // pinch off, and every frame is an honest escape-time fractal
                 // of a genuinely different map. Two multiplies.
                 "vec2 q = csqr(s.xy);\n"
                 "q = vec2(q.x * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    k.push_back(esc("julia", "JULIA", "escape",
                 // The Julia seed walks a small closed circle about wherever the knobs
                 // put it. That is the classic animation for this family and it is the
                 // right one: the whole character of a Julia set is a function of c.
                 "return vec4(csqr(s.xy) + vec2(p.x, p.y) + m.xy * 0.12, s.zw);",
                 SeedMode::Julia, 4.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, -0.4f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.6f, Curve::Bipolar}}));

    k.push_back(esc("multibrot", "MULTIBROT ZN+C", "escape",
                 // The exponent itself, which is the one number this family is
                 // named after. Between z^3 and z^3.4 the set grows and
                 // reabsorbs whole limbs -- and cpowr already pays for the
                 // polar form, so animating n costs nothing at all.
                 "return vec4(cpowr(s.xy, p.x + m.w * 0.6) + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false,
                 {{"k.power", "POWER", 1.5f, 10.0f, 3.0f, Curve::Linear}}));

    k.push_back(esc("multijulia", "MULTI JULIA ZN", "escape",
                 "return vec4(cpowr(s.xy, p.z) + vec2(p.x, p.y) + m.xy * 0.12, s.zw);",
                 SeedMode::Julia, 4.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, -0.6f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.42f, Curve::Bipolar},
                  {"k.power", "POWER", 1.5f, 10.0f, 4.0f, Curve::Linear}}));

    k.push_back(esc("burningship", "BURNING SHIP", "escape",
                 "vec2 q = csqr(abs(s.xy));\n"
                 "q = vec2(q.x * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    k.push_back(esc("burningjulia", "BURNING SHIP JULIA", "escape",
                 "return vec4(csqr(abs(s.xy)) + vec2(p.x, p.y) + m.xy * 0.12, s.zw);",
                 SeedMode::Julia, 4.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, -0.5f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, -0.6f, Curve::Bipolar}}));

    k.push_back(esc("tricorn", "TRICORN", "escape",
                 "vec2 q = csqr(cconj(s.xy));\n"
                 "q = vec2(q.x * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    k.push_back(esc("celtic", "CELTIC", "escape",
                 "vec2 q = csqr(s.xy);\n"
                 "q = vec2(abs(q.x) * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    k.push_back(esc("buffalo", "BUFFALO", "escape",
                 "vec2 q = csqr(abs(s.xy));\n"
                 "q = vec2(abs(q.x) * (1.0 + m.z * 0.22), -abs(q.y) * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    k.push_back(esc("perpendicular", "PERPENDICULAR", "escape",
                 "vec2 z = vec2(abs(s.x), -s.y);\n"
                 "vec2 q = csqr(z);\n"
                 "q = vec2(q.x * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "return vec4(q + c, s.zw);",
                 SeedMode::Mandel, 4.0f, false, {}));

    // ---- second order: this is why the state is a vec4 ------------------
    k.push_back(esc("phoenix", "PHOENIX", "escape",
                 // Both halves of what makes a Phoenix a Phoenix move: the seed on its
                 // circle, and the FEEDBACK TERM -- the coefficient on the previous
                 // iterate, which is the whole second-order character of the family.
                 "vec2 zn = csqr(s.xy) + vec2(p.x, p.y) + m.xy * 0.10\n"
                 "        + (p.z + m.z * 0.22) * s.zw;\n"
                 "return vec4(zn, s.xy);",
                 SeedMode::Julia, 4.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, 0.5667f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.0f, Curve::Bipolar},
                  {"k.pre", "P", -1.0f, 1.0f, -0.5f, Curve::Bipolar}}));

    k.push_back(esc("manowar", "MANOWAR", "escape",
                 "vec2 q = csqr(s.xy);\n"
                 "q = vec2(q.x * (1.0 + m.z * 0.22), q.y * (1.0 - m.z * 0.22));\n"
                 "vec2 zn = q + s.zw + c;\n"
                 "return vec4(zn, s.xy);",
                 SeedMode::Mandel, 4.0f, false, {}));

    // Spider: the seed itself drifts, carried in the aux slot.
    k.push_back(esc("spider", "SPIDER", "escape",
                 // Spider's character is entirely in the rate its seed decays
                 // towards the orbit, so that is the number that moves. Nothing
                 // is rotated; the webbing re-grows.
                 "vec2 cc = s.zw;\n"
                 "vec2 zn = csqr(s.xy) + cc;\n"
                 "return vec4(zn, cc * (0.5 + m.z * 0.18) + zn);",
                 SeedMode::Mandel, 4.0f, false, {}, "c.x, c.y"));

    // ---- convergent: Newton-family, escape on |dz| small ----------------
    k.push_back(esc("newton3", "NEWTON Z3-1", "convergent",
                 // Newton has no seed and no knobs, so what moves is its RELAXATION --
                 // the step length. At 1.0 it is Newton exactly; either side of that the
                 // three basins fight over the boundary and the whole filigree writhes.
                 // Held inside [0.83, 1.17] at EVOLVE 1, which still converges.
                 "vec2 z = s.xy;\n"
                 "vec2 z3 = cmul(csqr(z), z);\n"
                 "float a = 1.0 + m.z * 0.22;\n"
                 "return vec4(z - a * cdiv(z3 - vec2(1.0, 0.0), 3.0 * csqr(z)), s.zw);",
                 SeedMode::Julia, 1e-6f, true, {}));

    k.push_back(esc("nova", "NOVA Z3-1", "convergent",
                 "vec2 z = s.xy;\n"
                 "vec2 z3 = cmul(csqr(z), z);\n"
                 "vec2 d  = 3.0 * csqr(z);\n"
                 "return vec4(z - p.x * (1.0 + m.w * 0.3) * cdiv(z3 - vec2(1.0, 0.0), d) + c, s.zw);",
                 SeedMode::Mandel, 1e-6f, true,
                 {{"k.relax", "RELAX", 0.1f, 2.0f, 1.0f, Curve::Linear}}));

    // ---- transcendental: unbounded, wild, cheap --------------------------
    k.push_back(esc("sine", "SINE JULIA", "transcendental",
                 "return vec4(cmul(vec2(p.x, p.y) + m.xy * 0.15, csin(s.xy)), s.zw);",
                 SeedMode::Julia, 64.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, 1.0f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.3f, Curve::Bipolar}}));

    k.push_back(esc("cosine", "COSINE JULIA", "transcendental",
                 "return vec4(cmul(vec2(p.x, p.y) + m.xy * 0.15, ccos(s.xy)), s.zw);",
                 SeedMode::Julia, 64.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, 1.0f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.6f, Curve::Bipolar}}));

    k.push_back(esc("expo", "EXPONENTIAL JULIA", "transcendental",
                 "return vec4(cmul(vec2(p.x, p.y) + m.xy * 0.08, cexp(s.xy)), s.zw);",
                 SeedMode::Julia, 256.0f, false,
                 {{"k.cre", "C RE", -2.0f, 2.0f, 0.5f, Curve::Bipolar},
                  {"k.cim", "C IM", -2.0f, 2.0f, 0.0f, Curve::Bipolar}}));

    k.push_back(esc("lambda", "LAMBDA LOGISTIC", "transcendental",
                 // The logistic map's other fixed point moves: z(k - z) rather than
                 // z(1 - z). That is a real one-parameter family, and it
                 // reorganises the basins instead of turning them.
                 "vec2 z = s.xy;\n"
                 "return vec4(cmul(c, cmul(z, vec2(1.0 + m.w * 0.35, 0.0) - z)), s.zw);",
                 SeedMode::Mandel, 16.0f, false, {}, "",
                 // NOT the origin. z(1-z) has a fixed point at 0, so a Mandel
                 // seed leaves every single pixel bounded and this generator
                 // drew a BLACK SCREEN in every colour mode from the day it
                 // was added -- found by the motion probe, which correctly
                 // reported a mean pixel difference of exactly zero. The
                 // logistic map's critical point is 1/2, which is where an
                 // orbit that says anything about the parameter starts.
                 "vec2(0.5, 0.0)"));

    k.push_back(esc("magnet", "MAGNET 1", "escape",
                 "vec2 z = s.xy;\n"
                 // Magnet 1 is a rational map and its whole look is where the pole
                 // sits. Moving the constant in the denominator drags the pole
                 // through the plane and the structure re-forms around it.
                 "vec2 num = csqr(z) + c - vec2(1.0, 0.0);\n"
                 "vec2 den = 2.0 * z + c - vec2(2.0 + m.w * 0.45, 0.0);\n"
                 "return vec4(csqr(cdiv(num, den)), s.zw);",
                 SeedMode::Mandel, 100.0f, false, {}));

    // ---- folds: not escape-time in spirit, but they iterate a plane, so
                 //      they run through the exact same machinery ------------------------
    k.push_back(esc("kifs", "KIFS", "fold",
                 // A fold system is defined by WHERE it folds and by how much it turns
                 // between folds, so those are what move. Small: KIFS is the most
                 // sensitive thing in the bank and a large excursion empties the frame.
                 "vec2 z = abs(s.xy) / max(cabs2(s.xy), 1e-6) - vec2(p.x, p.y) - m.xy * 0.10;\n"
                 "float a = p.z + m.z * 0.35;\n"
                 "z = mat2(cos(a), -sin(a), sin(a), cos(a)) * z;\n"
                 "return vec4(z * p.w, s.zw);",
                 SeedMode::Julia, 1e4f, false,
                 {{"k.foldx", "FOLD X", -2.0f, 2.0f, 1.0f, Curve::Bipolar},
                  {"k.foldy", "FOLD Y", -2.0f, 2.0f, 0.5f, Curve::Bipolar},
                  {"k.angle", "ANGLE", -3.14159f, 3.14159f, 0.6f, Curve::Bipolar},
                  {"k.scale", "SCALE", 0.5f, 2.5f, 1.4f, Curve::Linear}}));

    k.push_back(esc("kleinian", "KLEINIAN GROUP", "fold",
                 "vec2 z = s.xy;\n"
                 "z.x = mod(z.x + p.x, 2.0 * p.x) - p.x;\n"
                 "float r = cabs2(z);\n"
                 "z = z / max(r, 1e-6) * (p.y * (1.0 + m.z * 0.12));\n"
                 "return vec4(z + vec2(p.z, p.w) + m.xy * 0.08, s.zw);",
                 // Bailout 1e4 with no translation is a group whose every
                 // orbit stays bounded: the whole frame was interior, i.e.
                 // black, exactly like LAMBDA and for a different reason. A
                 // Kleinian group needs a translation to be a group at all --
                 // the fold and the inversion alone are an involution.
                 SeedMode::Julia, 100.0f, false,
                 {{"k.period", "PERIOD", 0.2f, 3.0f, 1.0f, Curve::Linear},
                  {"k.inv", "INVERT", 0.2f, 2.0f, 1.0f, Curve::Linear},
                  {"k.tx", "T X", -1.0f, 1.0f, 0.55f, Curve::Bipolar},
                  {"k.ty", "T Y", -1.0f, 1.0f, 0.35f, Curve::Bipolar}}));

    // ---- fields: no orbit, no escape. A function of position and time.
                 // These are the slow, chill, flowing end of the instrument, and they run
                 // through the same palette, the same zoom/rotate/centre and the same
                 // effect chain as everything above. Bodies may use nvFbm / nvWorley /
                 // nvWorley2 / nvNoise from the shared shader library.

    // The lava lamp: two octaves of noise, the second warped by the first.
                 // Almost all the "chill flowing" looks are this with different knobs.
    k.push_back(fld("warp", "DOMAIN WARP FBM", "field",
                    "vec2 q = p * k.x;\n"
                    "vec2 w = vec2(nvFbm(q + vec2(0.0, t * k.y), 4),\n"
                    "              nvFbm(q + vec2(5.2, 1.3) + t * k.y * 0.7, 4));\n"
                    "return nvFbm(q + w * k.z, 5);",
                    {{"k.scale", "SCALE", 0.2f, 8.0f, 1.6f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 1.0f, 0.12f, Curve::Linear},
                     {"k.warp", "WARP", 0.0f, 4.0f, 1.6f, Curve::Linear}}));

    k.push_back(fld("fbm", "FBM VALUE NOISE", "field",
                    "return nvFbm(p * k.x + vec2(t * k.y, t * k.y * 0.3), 6);",
                    {{"k.scale", "SCALE", 0.2f, 8.0f, 1.5f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 1.0f, 0.1f, Curve::Linear}}));

    k.push_back(fld("ridged", "RIDGED MULTIFRACTAL", "field",
                    "float v = 0.0, a = 0.5;\n"
                    "vec2 q = p * k.x + t * k.y;\n"
                    "for (int i = 0; i < 6; ++i) {\n"
                    "  v += a * (1.0 - abs(nvNoise(q) * 2.0 - 1.0));\n"
                    "  q *= 2.03; a *= 0.5;\n"
                    "}\n"
                    "return pow(clamp(v, 0.0, 1.0), k.z);",
                    {{"k.scale", "SCALE", 0.2f, 8.0f, 1.8f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 1.0f, 0.08f, Curve::Linear},
                     {"k.sharp", "SHARP", 0.5f, 6.0f, 2.0f, Curve::Linear}}));

    // Giraffe skin / cracked earth: cellular noise, with the cell interiors
                 // flattened and the seams left dark. SHARP closes the seams up.
    k.push_back(fld("giraffe", "WORLEY F2-F1", "field",
                    "vec2 q = p * k.x;\n"
                    "q += vec2(nvFbm(q * 0.5 + t * k.y, 3) - 0.5) * k.w;\n"
                    "vec2 f = nvWorley2(q, t * k.y);\n"
                    "float seam = clamp((f.y - f.x) * k.z, 0.0, 1.0);\n"
                    "return smoothstep(0.0, 0.35, seam);",
                    {{"k.scale", "SCALE", 0.5f, 12.0f, 3.0f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 0.6f, 0.05f, Curve::Linear},
                     {"k.sharp", "SHARP", 0.5f, 8.0f, 2.5f, Curve::Linear},
                     {"k.wobble", "WOBBLE", 0.0f, 2.0f, 0.4f, Curve::Linear}}));

    k.push_back(fld("cells", "VORONOI F1", "field",
                    "vec2 q = p * k.x;\n"
                    "float d = nvWorley(q, t * k.y);\n"
                    "return pow(clamp(d * k.z, 0.0, 1.0), k.w);",
                    {{"k.scale", "SCALE", 0.5f, 12.0f, 3.0f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 1.0f, 0.15f, Curve::Linear},
                     {"k.gain", "GAIN", 0.5f, 4.0f, 1.4f, Curve::Linear},
                     {"k.sharp", "SHARP", 0.3f, 4.0f, 1.0f, Curve::Linear}}));

    // The actual lava lamp: a handful of blobs on slow orbits, summed as an
                 // inverse-square field and thresholded. Merges and splits like the real
                 // thing because the field is additive.
    k.push_back(fld("lava", "METABALLS", "field",
                    "float v = 0.0;\n"
                    "for (int i = 0; i < 7; ++i) {\n"
                    "  float fi = float(i);\n"
                    "  float sp = 0.35 + fi * 0.11;\n"
                    "  vec2 c0 = vec2(sin(t * k.y * sp + fi * 2.1) * 0.75,\n"
                    "                 cos(t * k.y * sp * 0.83 + fi * 1.7) * 0.6);\n"
                    "  vec2 d = (p - c0) * k.x;\n"
                    "  v += (0.05 + 0.03 * sin(fi)) / max(dot(d, d), 1e-4);\n"
                    "}\n"
                    "return smoothstep(k.z - k.w, k.z + k.w, v);",
                    {{"k.scale", "SCALE", 0.3f, 4.0f, 1.0f, Curve::Exp},
                     {"k.speed", "FLOW", 0.0f, 2.0f, 0.35f, Curve::Linear},
                     {"k.thresh", "THRESH", 0.2f, 3.0f, 0.9f, Curve::Linear},
                     {"k.soft", "SOFT", 0.02f, 1.5f, 0.35f, Curve::Linear}}));

    k.push_back(fld("plasma", "SINE PLASMA", "field",
                    "vec2 q = p * k.x;\n"
                    "float v = sin(q.x + t * k.y)\n"
                    "        + sin(q.y * 1.3 - t * k.y * 0.8)\n"
                    "        + sin((q.x + q.y) * 0.7 + t * k.y * 1.3)\n"
                    "        + sin(length(q) * k.z - t * k.y * 1.7);\n"
                    "return v * 0.125 + 0.5;",
                    {{"k.scale", "SCALE", 0.5f, 20.0f, 4.0f, Curve::Exp},
                     {"k.speed", "FLOW", 0.0f, 3.0f, 0.5f, Curve::Linear},
                     {"k.rings", "RINGS", 0.0f, 20.0f, 5.0f, Curve::Linear}}));

    // Flow lines through a noise field: streaky, directional, very different
                 // in character from the blobby ones above.
    k.push_back(fld("curl", "CURL NOISE", "field",
                    "vec2 q = p * k.x;\n"
                    "float e = 0.05;\n"
                    "for (int i = 0; i < 4; ++i) {\n"
                    "  float n1 = nvFbm(q + vec2(0.0, e), 3);\n"
                    "  float n2 = nvFbm(q - vec2(0.0, e), 3);\n"
                    "  float n3 = nvFbm(q + vec2(e, 0.0), 3);\n"
                    "  float n4 = nvFbm(q - vec2(e, 0.0), 3);\n"
                    "  q += vec2(n1 - n2, n4 - n3) * k.z;\n"
                    "}\n"
                    "return nvFbm(q + t * k.y, 4);",
                    {{"k.scale", "SCALE", 0.2f, 6.0f, 1.4f, Curve::Exp},
                     {"k.drift", "DRIFT", 0.0f, 1.0f, 0.1f, Curve::Linear},
                     {"k.flow", "FLOW", 0.0f, 3.0f, 1.2f, Curve::Linear}}));

    return k;
}

// ---- which generators travel, and how far --------------------------------
//
// MEASURED, on the Pi, with tests/depth_sweep.py: each kernel rendered at the
// dive-0 target at 0..5 decades, scored on live fraction (not interior-black)
// and edge energy (not a flat wash). Both scores appear below as
// `live/edge` at 0,1,2,3,4,5 decades. See kernels.h for why the answer is a
// property of the map rather than a taste.
//
// The headline result is that the picture almost never survives the descent:
// 22 of the 30 generators score zero. That is not a bug, it is the geometry.
// The journey aims at a point on the MANDELBROT boundary, and only MANDELBROT
// is that boundary. Everything else is either a different map that merely
// passes near those coordinates, or -- for all eight FIELD kernels -- a
// function with a finite number of octaves, which cannot have anything below
// them by construction.
//
// The failure is invisible in the obvious statistic, which is why edge energy
// is measured too. DOMAIN WARP FBM goes 0.78/0.9 -> 1.00/0.0: its live
// fraction RISES to a perfect 100% while it turns into a single flat colour.
// A black-frame check would have called that healthy.
//
// A one-decade SAFETY MARGIN is subtracted from every measured figure except
// MANDELBROT's and KIFS's, because the sweep measures dive 0 and the journey
// rotates through sixteen targets. What holds at one of them is not promised
// at the other fifteen. MANDELBROT is exempt because all sixteen targets were
// chosen as points on its boundary; KIFS is exempt because an iterated
// function system is scale-invariant by construction, so its depth does not
// depend on where you aim it -- and it is the one kernel whose edge energy
// GREW all the way down (2.2 -> 7.7), which is the actual signature of
// self-similarity.
//
// The default is 0 -- a generator does not travel unless it was shown to.
// That is the safe direction: a kernel wrongly parked is merely still, while
// a kernel wrongly dived ends its dive on a solid screen, which is the one
// failure that reads as the renderer being broken.
// ---- measured dive aims, one table per kernel ------------------------------
//
// Found by tests/target_search.py: a beam search down each map's OWN plane,
// seeded from two view scales, refining the aim at every half decade and
// scoring each frame on live fraction and edge energy. A depth counts only
// where structure GREW on the way down -- surviving a floor is not enough,
// because a smooth wash survives any floor (JULIA holds a dead-flat 0.30
// from surface to five decades and has nothing at the bottom).
//
// This is what the journey was missing. The 8 seed coordinates in journey.h
// are MANDELBROT points, and 21 of the 22 escape kernels are not that map,
// so they were measured as having no dive and parked. They all had dives.
// BURNING SHIP went from edge energy 3.1 -> 0.1 within one decade to a
// four-and-a-half decade descent peaking at 61.
//
// SEVERAL aims per kernel, not one: a single aim means every dive lands in
// the same place, so the instrument loops with a period of one dive -- the
// exact recurrence the journey exists to eliminate.
struct KernelAims { const char* id; float depth; int n; double xy[12]; };
static const KernelAims kAims[] = {
    {"mandelbrot", 4.5f, 6, {-0.7222390428473684, -0.20145221794116205, -0.74366616092008009, 0.10704510780376816, -0.74506016384922247, 0.089165200409091497, -0.7474162308408735, -0.057463107599257435, -0.74815738966747547, -0.083650719472526835, -0.74006530721621255, 0.071798512822035851}},  // edge 35
    {"multibrot", 4.5f, 3, {0.41203267134078292, -0.5945331706691428, 0.41602360940894118, 0.60512221489468376, 0.41624917391277305, 0.59344841880136989}},  // edge 37
    {"burningship", 4.5f, 1, {0.34004934180302182, 0.048704236666069524}},  // edge 52
    {"tricorn", 4.5f, 6, {0.24365640190463275, 0.49951158832424652, 0.24682573342352032, 0.51053977129613237, 0.25848848705601624, 0.54601134337569568, 0.25315260725103483, 0.57035751343142538, 0.26599936024546889, 0.52835851325731409, 0.24319672745468907, 0.53224199260097049}},  // edge 10
    {"celtic", 4.5f, 6, {-0.76609934359582843, 0.66828494881533984, -0.76576040553228186, -0.66793551528192863, -0.74311302238925581, 1.0195462750926909, -0.76325314747631146, 0.68051624163158286, -0.76157106876809122, -0.65329479540944757, -0.76609820407449902, 0.70795521603967437}},  // edge 53
    {"perpendicular", 4.5f, 6, {-0.31953286431985739, 0.70749448311811858, -0.30664046941587064, 0.70349540391625531, -0.29244580977536844, -0.70328589257485385, -0.28169575383873374, 0.70472811905465971, -0.27959905678093439, -0.70056831021064658, -0.26338244803268251, -0.69383396317896395}},  // edge 53
    {"manowar", 4.5f, 5, {-2.7594523159341911e-05, -0.039145320791428941, -0.033784520790940409, -0.043739865227181179, -0.024175832453617992, -0.0081991116097473119, -0.036336086153132718, -0.021983531534893463, 0.00077328884686727814, -0.010655406534893462}},  // edge 9
    {"spider", 4.5f, 4, {-0.10336543781780444, 0.012771320731808875, -0.10276681745560162, 0.023554484913828916, -0.10022119626566302, 0.039578681728771027, -0.075125110114885443, 0.01659641913300982}},  // edge 50
    {"nova", 4.5f, 6, {0.33418960737009806, 0.40183854672690156, 0.33417928540482977, 0.41430943975014334, 0.26202135570165447, 0.44655760483109808, 0.26303698070165449, 0.46499510483109807, 0.28032640503139605, 0.45460019820679382, -0.46059636847152696, 0.070112044133009824}},  // edge 28
    {"lambda", 4.5f, 6, {2.9759124668784205, 0.22929570048590225, 2.9688492476268484, 0.26325111996222877, 2.9744536685212655, 0.21577919132460283, 2.9763098185386765, 0.24456344431903687, 2.9932656956974464, 0.18318674616440647, 2.9880775839112323, 0.13970542833709126}},  // edge 31
    {"magnet", 5.0f, 6, {0.52086212491870676, 1.7513561804061721, 2.1460472954547805, 1.1939474692588916, 2.1761269115559436, 1.1946273846044673, 2.1259700209222205, 1.1975741986016843, 0.5299507749676029, 1.7658050394142988, 2.0946333326736122, 1.1472404574108752}},  // edge 11
};

// How deep a kernel is allowed to dive on measurements alone, with nobody
// having looked at the bottom of it.
//
// ONE decade, and the number is measured rather than cautious. BURNING SHIP
// was inspected frame by frame on the panel at its own baked aim, scoring the
// fraction of pixels deviating by more than 60 from their 3x3 median -- the
// standard salt-and-pepper measure, and it tracks what the eye sees exactly:
//
//   0 decades  0.13%  clean; crisp silhouette, smooth gradient
//   1 decade   0.24%  onset: isolated speckles on the boundary, cosmetic
//   2 decades  1.26%  a continuous noise band obscuring the filaments
//   3 decades  8.29%  salt-and-pepper is the dominant feature; structure gone
//
// The first cap here was 3.0, chosen as "where burning ship was still clean".
// It was not: at 3 decades the frame is a failure, so the cap sat PAST the
// breakdown rather than short of it.
//
// This is precision breakdown in the iteration loop on the Pi's V3D, and it
// is invisible to the metric that picked these depths -- noise is the
// maximum-edge signal there is, so edge energy SCORES it, 66.1 for a frame
// that is mostly speckle. Downsample coherence does not separate them either
// (0.68 clean vs 0.56 speckled). Nothing available to the search can see this
// failure, which is why the ceiling is set by what somebody looked at.
//
// Raising one kernel is a one-line exception below, after its dive has been
// eyeballed at depth on the PANEL -- not on a Mac, which renders these depths
// cleanly and will tell you everything is fine.
static constexpr float kUnseenDepthCap = 1.0f;

// Clamp every generator to what has actually been LOOKED AT, whichever table
// its depth came from.
//
// Applied to the whole bank rather than inside the aim applier, because the
// two sources of depth fail the same way: JULIA at 3.0 and KIFS at 4.0 come
// from the older Mandelbrot-aimed sweep, not from kAims, so an exception
// living in the aim applier let them straight past the ceiling. Speckle is
// precision breakdown in the iteration loop -- it does not care which table
// chose the number.
static void capJourneyDepths(std::vector<KernelDef>& bank) {
    for (KernelDef& k : bank) {
        // mandelbrot is the only kernel inspected at depth ON THE PANEL.
        //
        // 10 decades rather than 4.5 because the ceiling that produced 4.5
        // was the 512-iteration cap, not anything about this map: at 512 the
        // frame went black past ~6 decades, so the dive had to turn round
        // early. With kIterCap at 2048 and detailBoost tracking depth, the
        // binding limit is the hi/lo centre pair instead -- ~48 bits, ~1e-14
        // absolute. At 1e10 a pixel spans ~5e-13, still fifty times coarser
        // than that; by 1e12 the pixel is FINER than the precision and the
        // frame breaks up. 10 leaves that margin intact.
        const float seen = (k.id == "mandelbrot") ? 10.0f : kUnseenDepthCap;
        if (k.journeyDepth > seen) k.journeyDepth = seen;
    }
}

static void applyJourneyAims(std::vector<KernelDef>& bank) {
    for (const KernelAims& a : kAims) {
        for (KernelDef& k : bank) {
            if (k.id != a.id) continue;
            k.journeyAims.assign(a.xy, a.xy + 2 * a.n);
            // The measured depth REPLACES whatever the Mandelbrot-aimed sweep
            // concluded: that sweep was measuring the wrong point.
            //
            // But capped, because the metric that produced these numbers
            // cannot see the failure that actually matters at depth. Looked
            // at on the Pi, BURNING SHIP's last decade and a half is
            // green/red salt-and-pepper speckle -- numerical breakdown, not
            // structure -- and speckle scores WELL on edge energy (66.1),
            // because noise is the maximum-edge signal there is. Downsample
            // coherence does not separate the two either: measured on the
            // Mac, speckle-free MANDELBROT reads 0.68 and speckled BURNING
            // SHIP 0.56, too close to threshold on.
            //
            // It is also platform-specific. The Mac renders those depths
            // cleanly; the Pi's V3D carries less precision through the
            // iteration loop, so the breakdown appears there and cannot be
            // measured from here at all.
            //
            // So depth is capped at what has been SEEN to be good, per
            // kernel, and only MANDELBROT has been looked at deeper (clean
            // filaments, spirals and mini-brots at 4 and 5 decades on the
            // panel). Every other kernel is metric-verified only, and gets
            // the conservative cap until someone looks. Raising one is a
            // one-line change once its dive has been eyeballed at depth.
            // mandelbrot is the ONLY kernel eyeballed at depth on the
            // panel (clean filaments, spirals and mini-brots at 4 and 5
            // decades). Every other kernel takes the unseen cap; kifs and
            // newton3 end up lower still, from their own aim-table depth.
            k.journeyDepth = a.depth;
            break;
        }
    }
}

static void applyJourneyDepths(std::vector<KernelDef>& bank) {
    struct Depth { const char* id; float decades; };
    static const Depth kDepths[] = {
        // id            keep   measured live/edge at 0,1,2,3,4,5 decades
        // ---- the ones that genuinely go somewhere ----------------------
        {"mandelbrot",   5.0f},  // .48/4.3 .19/4.2 .17/8.0 .42/22.7 .61/16 .65/14
        {"kifs",         4.0f},  // .01/2.2 .01/3.2 .01/4.4 .02/5.6 .02/6.4 .02/7.7
        {"julia",        3.0f},  // .81/13  .80/12  .72/22.7 .73/17.1 .89/8.5 .98/2.0
        {"tricorn",      2.0f},  // .84/1.4 .57/2.1 .72/3.0 .77/0.9 1.0/0.0
        {"celtic",       1.0f},  // .30/7.8 .15/11.4 .17/24.2 .05/9.1 .00/0.0
        {"newton3",      1.0f},  // .90/1.1 .80/1.2 .95/0.7 1.0/0.0
        // ---- measured 1 decade, which the margin takes to 0 -------------
        // manowar  .98/0.3 .97/0.3 1.0/0.0   nova  .80/0.8 .43/0.3 .00/0.0
        // Both are nearly flat at the surface already; parking is honest.
        //
        // ---- everything else measured 0 and is left at the default ------
        // Escape maps that die within a decade at these coordinates:
        //   multibrot .73/3.0 -> .99/0.4 -> 1.0/0.0      buffalo   .28/1.9 -> .04/2.8
        //   burningship .55/3.1 -> .58/0.1               spider    .38/3.4 -> .09/1.2
        //   multijulia .58/4.7 -> .16/4.6                lambda    .24/3.4 -> .00/0.0
        //   perpendicular .49/5.0 -> .16/3.4             magnet    .04/4.0 -> .00/0.0
        //   phoenix   .94/3.8 -> 1.0/0.0                 kleinian  .01/0.8 -> .00/0.0
        //   sine      .57/7.9 -> .21/1.8
        // Flat at the surface too, so there was never a dive to have:
        //   burningjulia .06/0.1   cosine .03/0.4   expo 1.00/0.0 at every depth
        // Every FIELD kernel, all eight, edge energy gone by 2 decades:
        //   warp .9->0.1  fbm .6->0.1  ridged 1.8->0.4  giraffe 7.9->2.6->0.0
        //   cells 2.9->0.8  lava 1.0->0.0  plasma 1.5->0.4  curl .6->0.1
    };
    for (const Depth& d : kDepths) {
        for (KernelDef& k : bank) {
            if (k.id == d.id) { k.journeyDepth = d.decades; break; }
        }
    }
}

const std::vector<KernelDef>& kernelBank() {
    static const std::vector<KernelDef> bank = [] {
        std::vector<KernelDef> b = buildBank();
        applyJourneyDepths(b);
        applyJourneyAims(b);   // measured aims win over the depth table
        capJourneyDepths(b);   // ...but nothing dives deeper than we have seen
        return b;
    }();
    return bank;
}

const char* colorModeName(ColorMode m) {
    switch (m) {
        case ColorMode::SmoothEscape: return "SMOOTH";
        case ColorMode::OrbitTrap:    return "TRAP";
        case ColorMode::FinalAngle:   return "ANGLE";
        case ColorMode::Derivative:   return "DERIV";
        case ColorMode::Stripe:       return "STRIPE";
        case ColorMode::Curvature:    return "CURVE";
        case ColorMode::Prism:        return "PRISM";
        case ColorMode::Dual:         return "DUAL";
        default:                      return "?";
    }
}

ColorFeatures featuresFor(ColorMode m) {
    ColorFeatures f;
    switch (m) {
        case ColorMode::OrbitTrap:
            f.trap = true;
            break;
        case ColorMode::FinalAngle:
            f.angle = true;
            break;
        case ColorMode::Stripe:
            f.angle = true;
            f.stripe = true;
            break;
        case ColorMode::Curvature:
            f.curvature = true;
            break;
        case ColorMode::Prism:
            f.trap = true;
            break;
        case ColorMode::Dual:
            f.angle = true;
            break;
        default:
            break;  // SMOOTH and DERIV need nothing extra
    }
    return f;
}

const char* trapShapeName(TrapShape t) {
    switch (t) {
        case TrapShape::Point:  return "POINT";
        case TrapShape::Cross:  return "CROSS";
        case TrapShape::Circle: return "CIRCLE";
        case TrapShape::Line:   return "LINE";
        default:                return "?";
    }
}

}  // namespace nv

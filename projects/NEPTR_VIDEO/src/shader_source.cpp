#include "shader_source.h"

namespace nv {

const char* glslPreamble() {
#if defined(NV_PLATFORM_DRM)
    return "#version 300 es\n"
           "precision highp float;\n"
           "precision highp int;\n"
           "precision highp sampler2D;\n";
#else
    // macOS tops out at 4.1. Precision qualifiers are legal here too, so the
    // shader bodies below need no divergence at all.
    return "#version 410 core\n"
           "precision highp float;\n";
#endif
}

const char* glslFullscreenVert() {
    // One triangle, not two: no diagonal seam, and the tiler sees a single
    // primitive covering the surface.
    return
        "out vec2 vUV;\n"
        "void main() {\n"
        "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "  vUV = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
}

const char* glslPresentVert() {
    // The scene was rendered at the LOGICAL size (already swapped for a
    // quarter turn), so rotating normalized device coordinates maps it onto
    // the physical surface exactly, with no letterbox and no second buffer.
    // uRot is degrees CLOCKWISE, matching the kernel's convention.
    return
        "out vec2 vUV;\n"
        "uniform int uRot;\n"
        "void main() {\n"
        "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "  vUV = p;\n"
        "  vec2 n = p * 2.0 - 1.0;\n"
        "  if (uRot == 90)       n = vec2(n.y, -n.x);\n"
        "  else if (uRot == 180) n = -n;\n"
        "  else if (uRot == 270) n = vec2(-n.y, n.x);\n"
        "  gl_Position = vec4(n, 0.0, 1.0);\n"
        "}\n";
}

const char* glslCommon() {
    return
        // --- sub-viewport sampling -------------------------------------
        // Framebuffers are allocated once at the largest size the machine can
        // ever output, and every frame renders into the bottom-left sub-rect
        // implied by the resolution control. uSrcScale converts a logical
        // 0..1 uv into the physical uv of that sub-rect, so changing
        // resolution is two floats and never a reallocation.
        "uniform vec2 uSrcScale;\n"
        "vec4 nvSample(sampler2D t, vec2 uv) {\n"
        "  return texture(t, clamp(uv, 0.0, 1.0) * uSrcScale);\n"
        "}\n"

        // --- complex arithmetic ----------------------------------------
        "vec2 cmul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }\n"
        "vec2 csqr(vec2 a) { return vec2(a.x*a.x - a.y*a.y, 2.0*a.x*a.y); }\n"
        "vec2 cconj(vec2 a) { return vec2(a.x, -a.y); }\n"
        "float cabs2(vec2 a) { return dot(a, a); }\n"
        "vec2 cinv(vec2 a) { float d = max(dot(a, a), 1e-20); return vec2(a.x, -a.y) / d; }\n"
        "vec2 cdiv(vec2 a, vec2 b) { return cmul(a, cinv(b)); }\n"
        "vec2 cexp(vec2 a) { float e = exp(a.x); return vec2(e*cos(a.y), e*sin(a.y)); }\n"
        "vec2 clog(vec2 a) { return vec2(0.5*log(max(dot(a,a),1e-20)), atan(a.y, a.x)); }\n"
        "vec2 csin(vec2 a) { return vec2(sin(a.x)*cosh(a.y), cos(a.x)*sinh(a.y)); }\n"
        "vec2 ccos(vec2 a) { return vec2(cos(a.x)*cosh(a.y), -sin(a.x)*sinh(a.y)); }\n"
        // Real exponent via polar form: this is what makes the whole
        // multibrot family one kernel instead of one per integer power.
        "vec2 cpowr(vec2 a, float n) {\n"
        "  float r = length(a);\n"
        "  if (r < 1e-20) return vec2(0.0);\n"
        "  float th = atan(a.y, a.x) * n;\n"
        "  float rn = pow(r, n);\n"
        "  return vec2(rn * cos(th), rn * sin(th));\n"
        "}\n"

        // --- misc -------------------------------------------------------
        "mat2 nvRot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }\n"
        "float nvLuma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }\n"
        "float nvHash(vec2 p) {\n"
        "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
        "}\n"
        "vec3 nvHueRotate(vec3 c, float a) {\n"
        "  const vec3 k = vec3(0.57735);\n"
        "  float cs = cos(a);\n"
        "  return c * cs + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - cs);\n"
        "}\n"

        // --- noise, for the field kernels ------------------------------
        // Hash-based rather than texture-based: no asset to ship, no upload,
        // and it works identically on both drivers.
        "vec2 nvHash22(vec2 p) {\n"
        "  vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
        "  q += dot(q, q.yzx + 33.33);\n"
        "  return fract((q.xx + q.yz) * q.zy);\n"
        "}\n"
        // Value noise with a smoothstep fade -- cheaper than gradient noise
        // and, once stacked into fBm, visually indistinguishable here.
        "float nvNoise(vec2 p) {\n"
        "  vec2 i = floor(p), f = fract(p);\n"
        "  f = f * f * (3.0 - 2.0 * f);\n"
        "  float a = nvHash22(i).x;\n"
        "  float b = nvHash22(i + vec2(1.0, 0.0)).x;\n"
        "  float c = nvHash22(i + vec2(0.0, 1.0)).x;\n"
        "  float d = nvHash22(i + vec2(1.0, 1.0)).x;\n"
        "  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        // Constant loop bound with an early break: GLSL ES needs the bound to
        // be a constant, and the driver still unrolls what it can.
        "float nvFbm(vec2 p, int oct) {\n"
        "  float s = 0.0, a = 0.5, norm = 0.0;\n"
        "  for (int i = 0; i < 6; ++i) {\n"
        "    if (i >= oct) break;\n"
        "    s += a * nvNoise(p);\n"
        "    norm += a;\n"
        "    p *= 2.03;\n"
        "    a *= 0.5;\n"
        "  }\n"
        "  return s / max(norm, 1e-5);\n"
        "}\n"
        // Cellular (Worley) noise. The cell points drift with t, so the
        // pattern reorganises slowly instead of merely scrolling.
        "float nvWorley(vec2 p, float t) {\n"
        "  vec2 n = floor(p), f = fract(p);\n"
        "  float d = 1e9;\n"
        "  for (int j = -1; j <= 1; ++j) {\n"
        "    for (int i = -1; i <= 1; ++i) {\n"
        "      vec2 g = vec2(float(i), float(j));\n"
        "      vec2 o = nvHash22(n + g);\n"
        "      o = 0.5 + 0.5 * sin(t + 6.2831 * o);\n"
        "      vec2 r = g + o - f;\n"
        "      d = min(d, dot(r, r));\n"
        "    }\n"
        "  }\n"
        "  return sqrt(d);\n"
        "}\n"
        // F1 and F2. Their difference is the distance to the cell BOUNDARY,
        // which is what draws giraffe seams and cracked mud.
        "vec2 nvWorley2(vec2 p, float t) {\n"
        "  vec2 n = floor(p), f = fract(p);\n"
        "  float d1 = 1e9, d2 = 1e9;\n"
        "  for (int j = -1; j <= 1; ++j) {\n"
        "    for (int i = -1; i <= 1; ++i) {\n"
        "      vec2 g = vec2(float(i), float(j));\n"
        "      vec2 o = nvHash22(n + g);\n"
        "      o = 0.5 + 0.5 * sin(t + 6.2831 * o);\n"
        "      vec2 r = g + o - f;\n"
        "      float d = dot(r, r);\n"
        "      if (d < d1) { d2 = d1; d1 = d; }\n"
        "      else if (d < d2) { d2 = d; }\n"
        "    }\n"
        "  }\n"
        "  return vec2(sqrt(d1), sqrt(d2));\n"
        "}\n";
}

const char* glslCoreTemplate() {
    return
        "%%DEFINES%%\n"
        "in vec2 vUV;\n"
        "out vec4 fragColor;\n"
        "uniform float uAspect;\n"
        "uniform float uTime;\n"
        "uniform float uZoom;\n"
        "uniform float uRotation;\n"
        // The centre, as a hi/lo FLOAT PAIR rather than one float.
        //
        // This is the whole reason a zoom can be endless. The plane
        // coordinate is `p / uZoom + centre`, so the centre has to be
        // resolved to about one pixel span; a single float near magnitude 1
        // holds ~6e-8, which is exactly why ZOOM used to stop at four
        // decades. Splitting the double into two floats that sum to it
        // carries ~48 bits of mantissa, or ~1e-14 -- and the low part is
        // added to the SMALL pixel offset first (see nvPlane), so its bits
        // survive instead of being shifted away against the large one.
        "uniform vec2  uCenterHi;\n"
        "uniform vec2  uCenterLo;\n"
        // The incoming dive during a recycle: same three numbers, plus the
        // dissolve weight. uXfade is 0 for all but a fraction of the
        // instrument's life, and it is a UNIFORM, so the branch in main() is
        // coherent across the entire draw -- every invocation takes the same
        // side. Deliberately not a #define: a compile-time variant would make
        // the shader recompile at the dive boundary, which is a stall at
        // exactly the moment the dissolve exists to hide.
        "uniform vec2  uCenterHiB;\n"
        "uniform vec2  uCenterLoB;\n"
        "uniform float uZoomB;\n"
        "uniform float uXfade;\n"        "uniform float uRotA;\n"
        "uniform float uRotB;\n"
        "uniform float uDetail;\n"
        // The iteration ceiling the USER asked for, which is not the one the
        // loop is running. Escape time is normalised against this rather than
        // against uDetail so that the governor giving up iterations degrades
        // the fine structure without re-mapping every colour in the frame --
        // dividing by the governed value made the whole picture change hue the
        // instant the governor engaged, which is a frame jump with a cause.
        "uniform float uDetailRef;\n"
        "uniform vec4  uKernelP;\n"
        // The intrinsic-motion vector. FOUR NUMBERS COMPUTED ONCE PER FRAME on
        // the CPU, never per pixel and never per iteration:
        //
        //   m.xy  A * (cos phi, sin phi)   a CLOSED circle, for seed paths
        //   m.zw  (cos th, sin th)          a unit complex number swinging
        //                                   about 1, for a twist of the plane
        //
        // with th = A * 0.35 * sin(phi/2). Every component is a sinusoid in
        // phi at a rational multiple of one base frequency, so the whole
        // vector is exactly periodic in phi with period 4*pi -- which is what
        // lets the CPU wrap the phase accumulator and never drift, and never
        // jump, however long the instrument runs.
        //
        // At A = 0 this is exactly (0, 0, 1, 0): every kernel below reduces to
        // the maths it had before motion existed, bit for bit.
        "uniform vec4  uMotion;\n"
        "uniform vec4  uKernelPB;\n"
        "uniform float uMorph;\n"
        "uniform int   uColorMode;\n"
        "uniform int   uTrapShape;\n"
        "uniform float uTrapScale;\n"
        "uniform sampler2D uPalette;\n"
        "uniform float uPaletteMix;\n"
        "uniform float uPaletteShift;\n"
        "uniform float uPaletteSat;\n"
        "uniform float uPaletteSpread;\n"
        "uniform float uStripeFreq;\n"
        "uniform float uPrism;\n"
        "\n"
        // The kernel body. An Escape kernel supplies one iteration step; a
        // Field kernel supplies a direct function of position and time. Only
        // the one this kernel actually is gets compiled in.
        "#if KERNEL_FIELD\n"
        "float nvField(vec2 p, float t, vec4 k, vec4 m) {\n"
        "%%FIELD%%\n"
        "}\n"
        "#else\n"
        "vec4 nvStep(vec4 s, vec2 c, vec4 p, vec4 m) {\n"
        "%%STEP%%\n"
        "}\n"
        "#endif\n"
        // The morph target. Interpolating the STEP FUNCTION, not the picture:
        //   z <- mix(fA(z), fB(z), m)
        // At 0 and 1 you get each fractal exactly; in between you get a
        // continuum of iterations that are neither, with their own structure.
        // A crossfade of two finished images would just be a dissolve.
        "#if MORPH\n"
        "#if KERNEL_B_FIELD\n"
        "float nvFieldB(vec2 p, float t, vec4 k, vec4 m) {\n"
        "%%FIELDB%%\n"
        "}\n"
        "#else\n"
        "vec4 nvStepB(vec4 s, vec2 c, vec4 p, vec4 m) {\n"
        "%%STEPB%%\n"
        "}\n"
        "#endif\n"
        "#endif\n"
        "\n"
        "float trapDist(vec2 z) {\n"
        "  vec2 q = z * uTrapScale;\n"
        "  if (uTrapShape == 1) return min(abs(q.x), abs(q.y));\n"        // cross
        "  if (uTrapShape == 2) return abs(length(q) - 1.0);\n"           // circle
        "  if (uTrapShape == 3) return abs(q.y);\n"                       // line
        "  return length(q);\n"                                           // point
        "}\n"
        "\n"
        // Two palettes live in one 256x2 texture; the mix control crossfades
        // between them, which is what lets palette change be a slow gesture
        // rather than a jump cut.
        // SPREAD multiplies before the wrap, so the palette repeats across the
        // structure instead of being stretched once over it -- that is what
        // turns a gradient into banding that follows the fractal. SHIFT then
        // sweeps those bands ALONG the structure rather than sliding a
        // gradient across a picture of it.
        "vec3 paletteLookup(float t) {\n"
        "  t = fract(t * uPaletteSpread + uPaletteShift);\n"
        "  vec3 a = texture(uPalette, vec2(t, 0.25)).rgb;\n"
        "  vec3 b = texture(uPalette, vec2(t, 0.75)).rgb;\n"
        "  vec3 c = mix(a, b, uPaletteMix);\n"
        "  return mix(vec3(nvLuma(c)), c, uPaletteSat);\n"
        "}\n"
        // Each channel reads the palette at its own offset, so a second
        // intrinsic quantity pulls R, G and B apart. The dispersion is the
        // fractal's, not a post-process smear.
        "vec3 palettePrism(float t, float split) {\n"
        "  return vec3(paletteLookup(t - split).r,\n"
        "              paletteLookup(t).g,\n"
        "              paletteLookup(t + split).b);\n"
        "}\n"
        "\n"
        "vec3 nvScene(vec2 p) {\n"
        // Aspect-correct plane coordinates. The vertical span is ALWAYS
        // [-1,1]; the horizontal span widens with the output's aspect. A 4:3
        // projector, a 16:9 TV and an 800x480 panel therefore show the same
        // composition with more or less of the plane visible -- the fractal
        // is never stretched, whatever it is plugged into.
        // A field kernel has no orbit, so none of the escape machinery below
        // applies. It goes straight to the palette -- and therefore inherits
        // SPREAD, SHIFT, MIX and SAT exactly like a fractal does.
        "#if KERNEL_FIELD\n"
        "  float fv = clamp(nvField(p, uTime, uKernelP, uMotion), 0.0, 1.0);\n"
        "#if MORPH && KERNEL_B_FIELD\n"
        // Two fields mix directly: the value IS the thing, so blending it is
        // blending the pattern, not dissolving between two pictures.
        "  fv = mix(fv, clamp(nvFieldB(p, uTime, uKernelPB, uMotion), 0.0, 1.0), uMorph);\n"
        "#endif\n"
        "  return paletteLookup(fv);\n"
        "#else\n"
        "\n"
        "#if SEED_JULIA\n"
        "  vec2 z = %%ZINIT%%;\n"
        "  vec2 c = vec2(uKernelP.x, uKernelP.y);\n"
        "#else\n"
        "  vec2 z = %%ZINIT%%;\n"
        "  vec2 c = p;\n"
        "#endif\n"
        "  vec4 s = vec4(z, %%AUXINIT%%);\n"
        "\n"
        "  int maxIter = int(uDetail);\n"
        "  float n = 0.0;\n"
        "  bool done = false;\n"
        "  vec2 zPrev = z;\n"
        // Intrinsic quantities the ITERATION produces. Each is gated by the
        // colour mode at compile time, so a mode that does not read a feature
        // does not pay for it anywhere in the inner loop.
        "#if FEAT_TRAP\n"
        "  float trap = 1e9;\n"
        "#endif\n"
        "#if FEAT_ANGLE\n"
        "  float sumAngle = 0.0;\n"
        "  float lastAngle = 0.0;\n"
        "#endif\n"
        "#if FEAT_STRIPE\n"
        // Stripe average keeps the last two partial sums so the value can be
        // interpolated at bailout. Without that the bands break into hard
        // rings on the iteration boundaries -- which is precisely the banding
        // the technique exists to remove.
        "  float stripe = 0.0;\n"
        "  float stripePrev = 0.0;\n"
        "  float stripeN = 0.0;\n"
        "#endif\n"
        "#if FEAT_CURV\n"
        "  float curv = 0.0;\n"
        "  float curvN = 0.0;\n"
        "  vec2 zPrev2 = z;\n"
        "#endif\n"
        "\n"
        // Constant loop bound with an early break: v3d compiles this to a
        // tight loop, and the governor lowers uDetail rather than
        // recompiling anything.
        "  for (int i = 0; i < NV_ITER_CAP; ++i) {\n"
        "    if (i >= maxIter) { n = float(i); break; }\n"
        "#if FEAT_CURV\n"
        "    zPrev2 = zPrev;\n"
        "#endif\n"
        "    zPrev = s.xy;\n"
        "#if MORPH && !KERNEL_B_FIELD\n"
        "    s = mix(nvStep(s, c, uKernelP, uMotion), nvStepB(s, c, uKernelPB, uMotion), uMorph);\n"
        "#else\n"
        "    s = nvStep(s, c, uKernelP, uMotion);\n"
        "#endif\n"
        "#if FEAT_TRAP\n"
        "    trap = min(trap, trapDist(s.xy));\n"
        "#endif\n"
        "#if FEAT_ANGLE\n"
        "    lastAngle = atan(s.y, s.x);\n"
        "    sumAngle += lastAngle;\n"
        "#endif\n"
        "#if FEAT_STRIPE\n"
        "    stripePrev = stripe;\n"
        "    stripe += 0.5 + 0.5 * sin(uStripeFreq * lastAngle);\n"
        "    stripeN += 1.0;\n"
        "#endif\n"
        "#if FEAT_CURV\n"
        "    vec2 d1 = s.xy - zPrev;\n"
        "    vec2 d0 = zPrev - zPrev2;\n"
        "    float l = length(d1) * length(d0);\n"
        "    if (l > 1e-12) {\n"
        // The turn angle between successive orbit steps. Averaged, it reads
        // as the orbit's "roughness" and is completely unlike escape time.
        "      curv += abs(atan(d1.x * d0.y - d1.y * d0.x, dot(d1, d0)));\n"
        "      curvN += 1.0;\n"
        "    }\n"
        "#endif\n"
        "#if NV_CONVERGENT\n"
        "    vec2 d = s.xy - zPrev;\n"
        "    if (dot(d, d) < NV_ESCAPE_R2) { n = float(i); done = true; break; }\n"
        "#else\n"
        "    float r2 = dot(s.xy, s.xy);\n"
        "    if (r2 > NV_ESCAPE_R2) {\n"
        // Continuous escape time: removes the iteration banding that makes
        // fractals look like contour maps.
        "      float lr = log(max(r2, 1.0001)) * 0.5;\n"
        "      n = float(i) + 1.0 - log(lr / log(NV_ESCAPE_R)) / log(2.0);\n"
        "      done = true; break;\n"
        "    }\n"
        "#endif\n"
        "  }\n"
        "\n"
        // Points that never escape are the interior: they have no escape time
        // and no meaningful orbit average, so they stay black in every mode.
        "  if (!done) return vec3(0.0);\n"
        "\n"
        // Normalized escape, not a fixed multiplier. fract(n * 0.02) both
        // wrapped (n runs past 50) and crammed the whole exterior into the
        // first sliver of the palette, so most of the frame came out one flat
        // colour. Dividing by the iteration ceiling traverses the palette
        // exactly once at any DETAIL setting, and sqrt spends more of it near
        // the boundary, where the structure is.
        "  float esc = sqrt(clamp(n / max(uDetailRef, 1.0), 0.0, 1.0));\n"
        "  float v = esc;\n"
        "  float split = 0.0;\n"
        "\n"
        "#if MODE_TRAP\n"
        "  v = 1.0 - exp(-trap * 3.0);\n"
        "#elif MODE_ANGLE\n"
        "  v = fract(sumAngle * 0.15915494 + 0.5);\n"
        "#elif MODE_DERIV\n"
        "  v = 1.0 - exp(-length(s.xy - zPrev) * 0.5);\n"
        "#elif MODE_STRIPE\n"
        // Interpolate between the last two partial averages by the same
        // fractional part the smooth escape uses, so the bands cross
        // iteration boundaries continuously.
        "  float f = fract(n);\n"
        "  float a1 = stripe / max(stripeN, 1.0);\n"
        "  float a0 = stripePrev / max(stripeN - 1.0, 1.0);\n"
        "  v = mix(a0, a1, f);\n"
        "#elif MODE_CURV\n"
        "  v = clamp(curv / max(curvN, 1.0) / 3.14159265, 0.0, 1.0);\n"
        "#elif MODE_PRISM\n"
        // Escape sets the position in the palette; the orbit's closest
        // approach pulls the channels apart. Both come from the iteration.
        "  v = esc;\n"
        "  split = uPrism * (1.0 - exp(-trap * 3.0));\n"
        "#elif MODE_DUAL\n"
        // Two intrinsic quantities on two axes: the angle chooses the band,
        // the escape time positions you within it.
        "  float ang = fract(sumAngle * 0.15915494 + 0.5);\n"
        "  v = fract(ang + esc * 0.5);\n"
        "#endif\n"
        "\n"
        "  vec3 rgb = (split != 0.0) ? palettePrism(v, split) : paletteLookup(v);\n"
        "  return rgb;\n"
        "#endif\n"
        "}\n"
        "\n"
        // Compensated reconstruction of the plane coordinate. The order is
        // load-bearing: the pixel offset is tiny at depth and the low word of
        // the centre is tiny too, so they are summed with EACH OTHER first.
        // Adding the high word first would shift both out of the mantissa and
        // hand back exactly the single-float precision this pair exists to
        // escape.
        "vec2 nvPlane(vec2 base, float zoom, vec2 chi, vec2 clo, float rot) {\n"
        "  vec2 p = nvRot(rot) * base;\n"
        "  return (p / zoom + clo) + chi;\n"
        "}\n"
        "\n"
        "void main() {\n"
        // Aspect-correct plane coordinates. The vertical span is ALWAYS
        // [-1,1]; the horizontal span widens with the output's aspect. A 4:3
        // projector, a 16:9 TV and an 800x480 panel therefore show the same
        // composition with more or less of the plane visible -- the fractal
        // is never stretched, whatever it is plugged into.
        "  vec2 base = (vUV - 0.5) * 2.0 * vec2(uAspect, 1.0);\n"
        // The recycle. Two views dissolve rather than cut, and the incoming
        // one is WIDE (negative depth), so a dive ends by opening out into
        // somewhere new rather than by one field of deep detail replacing
        // another.
        //
        // A view is drawn only while it can still change a pixel. The output
        // is 8-bit, so a mix weight below half of 1/255 cannot move the stored
        // value however expensive the operand was -- and each operand here is
        // a full escape-time field, the most expensive thing in this engine.
        // Both ends are culled: `uXfade > 0.0` alone still drew the incoming
        // view at weight 1e-6, and it never stopped drawing the OUTGOING view,
        // which is the one that has passed the zoom threshold and is no longer
        // visible at all by the end of the dissolve.
        //
        // Rotation is applied PER VIEW, not once here, because each dive
        // carries its own orientation -- see JourneyView::rot.
        //
        // The branch is on a UNIFORM, so it stays coherent across the whole
        // draw -- every invocation takes the same side and V3D never pays for
        // divergence. Deliberately not a #define, for the reason recorded at
        // uXfade: a compile-time variant would recompile the shader at the
        // dive boundary, stalling exactly where the dissolve exists to hide a
        // stall.
        "  const float kFadeEps = 1.0 / 512.0;\n"
        "  vec3 col;\n"
        "  if (uXfade < kFadeEps) {\n"
        "    col = nvScene(nvPlane(base, uZoom, uCenterHi, uCenterLo,\n"
        "                          uRotation + uRotA));\n"
        "  } else if (uXfade > 1.0 - kFadeEps) {\n"
        "    col = nvScene(nvPlane(base, uZoomB, uCenterHiB, uCenterLoB,\n"
        "                          uRotation + uRotB));\n"
        "  } else {\n"
        "    vec3 cur = nvScene(nvPlane(base, uZoom, uCenterHi, uCenterLo,\n"
        "                               uRotation + uRotA));\n"
        "    vec3 nx = nvScene(nvPlane(base, uZoomB, uCenterHiB, uCenterLoB,\n"
        "                              uRotation + uRotB));\n"
        "    col = mix(cur, nx, uXfade);\n"
        "  }\n"
        "  fragColor = vec4(col, 1.0);\n"
        "}\n";
}

const char* glslPresentFrag() {
    return
        "in vec2 vUV;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D uInput;\n"
        "void main() {\n"
        "  fragColor = nvSample(uInput, vUV);\n"
        "}\n";
}

}  // namespace nv

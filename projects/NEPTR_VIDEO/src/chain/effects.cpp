#include "chain/effects.h"

namespace nv {

// Effect bodies may use, from shaders/core/common.glsl:
//   nvSample(tex, uv)   -- respects the sub-viewport of max-allocated buffers
//   nvLuma(rgb) nvHueRotate(rgb, a) nvHash(vec2) nvRot(a)
// and the uniforms uTexel, uTime, uAspect, uInput, uHistory.

static std::vector<EffectDef> buildBank() {
    std::vector<EffectDef> e;

    // ---- pointwise: these all fuse into a single pass -------------------
    e.push_back({"gain", "GAIN", EffectKind::Pointwise,
                 "col.rgb *= p.x; return col;",
                 {{"amount", "AMT", 0.0f, 4.0f, 1.0f, Curve::Linear}}});

    e.push_back({"levels", "LEVELS", EffectKind::Pointwise,
                 "col.rgb = (col.rgb - 0.5) * p.x + 0.5 + p.y; return col;",
                 {{"contrast", "CONTR", 0.0f, 4.0f, 1.0f, Curve::Linear},
                  {"bright", "BRIGHT", -1.0f, 1.0f, 0.0f, Curve::Bipolar}}});

    e.push_back({"saturate", "SATURATE", EffectKind::Pointwise,
                 "float l = nvLuma(col.rgb);\n"
                 "col.rgb = mix(vec3(l), col.rgb, p.x); return col;",
                 {{"amount", "SAT", 0.0f, 3.0f, 1.0f, Curve::Linear}}});

    e.push_back({"hue", "HUE ROTATE", EffectKind::Pointwise,
                 "col.rgb = nvHueRotate(col.rgb, p.x + p.y * uTime); return col;",
                 {{"angle", "HUE", -3.14159f, 3.14159f, 0.0f, Curve::Bipolar},
                  {"cycle", "CYCLE", -2.0f, 2.0f, 0.0f, Curve::Bipolar}}});

    e.push_back({"posterize", "POSTERIZE", EffectKind::Pointwise,
                 "float n = max(p.x, 2.0);\n"
                 "col.rgb = floor(col.rgb * n) / n; return col;",
                 {{"steps", "STEPS", 2.0f, 64.0f, 8.0f, Curve::Exp}}});

    e.push_back({"invert", "INVERT", EffectKind::Pointwise,
                 "col.rgb = mix(col.rgb, 1.0 - col.rgb, p.x); return col;",
                 {{"amount", "AMT", 0.0f, 1.0f, 1.0f, Curve::Linear}}});

    e.push_back({"solarize", "SOLARIZE", EffectKind::Pointwise,
                 "vec3 s = abs(col.rgb - p.x) * (1.0 / max(p.x, 0.001));\n"
                 "col.rgb = mix(col.rgb, s, p.y); return col;",
                 {{"pivot", "PIVOT", 0.05f, 0.95f, 0.5f, Curve::Linear},
                  {"amount", "AMT", 0.0f, 1.0f, 1.0f, Curve::Linear}}});

    e.push_back({"gamma", "GAMMA", EffectKind::Pointwise,
                 "col.rgb = pow(max(col.rgb, 0.0), vec3(1.0 / max(p.x, 0.01)));\n"
                 "return col;",
                 {{"gamma", "GAMMA", 0.2f, 4.0f, 1.0f, Curve::Exp}}});

    e.push_back({"threshold", "THRESHOLD", EffectKind::Pointwise,
                 "float l = nvLuma(col.rgb);\n"
                 "float t = smoothstep(p.x - p.y, p.x + p.y, l);\n"
                 "col.rgb = mix(col.rgb, vec3(t), p.z); return col;",
                 {{"level", "LEVEL", 0.0f, 1.0f, 0.5f, Curve::Linear},
                  {"soft", "SOFT", 0.001f, 0.5f, 0.05f, Curve::Linear},
                  {"amount", "AMT", 0.0f, 1.0f, 1.0f, Curve::Linear}}});

    e.push_back({"grain", "GRAIN", EffectKind::Pointwise,
                 "float n = nvHash(uv * 917.0 + uTime * 60.0) - 0.5;\n"
                 "col.rgb += n * p.x; return col;",
                 {{"amount", "AMT", 0.0f, 0.5f, 0.06f, Curve::Linear}}});

    e.push_back({"vignette", "VIGNETTE", EffectKind::Pointwise,
                 "vec2 d = (uv - 0.5) * vec2(uAspect, 1.0);\n"
                 "col.rgb *= 1.0 - smoothstep(p.x, p.x + p.y, length(d) * 1.4);\n"
                 "return col;",
                 {{"start", "START", 0.0f, 1.0f, 0.55f, Curve::Linear},
                  {"width", "WIDTH", 0.01f, 1.0f, 0.45f, Curve::Linear}}});

    // ---- samplers: each opens a pass -------------------------------------
    e.push_back({"blur", "BLUR", EffectKind::Sampler,
                 "vec2 o = uTexel * p.x;\n"
                 "vec4 a = nvSample(uInput, uv) * 0.25;\n"
                 "a += (nvSample(uInput, uv + vec2( o.x, 0.0)) +\n"
                 "      nvSample(uInput, uv + vec2(-o.x, 0.0)) +\n"
                 "      nvSample(uInput, uv + vec2(0.0,  o.y)) +\n"
                 "      nvSample(uInput, uv + vec2(0.0, -o.y))) * 0.125;\n"
                 "a += (nvSample(uInput, uv + o) + nvSample(uInput, uv - o) +\n"
                 "      nvSample(uInput, uv + vec2(o.x, -o.y)) +\n"
                 "      nvSample(uInput, uv + vec2(-o.x, o.y))) * 0.0625;\n"
                 "return a;",
                 {{"radius", "RADIUS", 0.0f, 16.0f, 2.0f, Curve::Linear}}});

    e.push_back({"sharpen", "SHARPEN", EffectKind::Sampler,
                 "vec2 o = uTexel * p.y;\n"
                 "vec4 c0 = nvSample(uInput, uv);\n"
                 "vec4 s = nvSample(uInput, uv + vec2(o.x, 0.0)) +\n"
                 "         nvSample(uInput, uv - vec2(o.x, 0.0)) +\n"
                 "         nvSample(uInput, uv + vec2(0.0, o.y)) +\n"
                 "         nvSample(uInput, uv - vec2(0.0, o.y));\n"
                 "return c0 + (c0 * 4.0 - s) * p.x * 0.25;",
                 {{"amount", "AMT", 0.0f, 3.0f, 1.0f, Curve::Linear},
                  {"radius", "RADIUS", 0.5f, 8.0f, 1.0f, Curve::Linear}}});

    e.push_back({"edge", "EDGE", EffectKind::Sampler,
                 "vec2 o = uTexel * p.y;\n"
                 "float gx = nvLuma(nvSample(uInput, uv + vec2(o.x, 0.0)).rgb) -\n"
                 "           nvLuma(nvSample(uInput, uv - vec2(o.x, 0.0)).rgb);\n"
                 "float gy = nvLuma(nvSample(uInput, uv + vec2(0.0, o.y)).rgb) -\n"
                 "           nvLuma(nvSample(uInput, uv - vec2(0.0, o.y)).rgb);\n"
                 "float g = length(vec2(gx, gy)) * p.x;\n"
                 "vec4 src = nvSample(uInput, uv);\n"
                 "return mix(src, vec4(vec3(g), 1.0), p.z);",
                 {{"gain", "GAIN", 0.0f, 20.0f, 4.0f, Curve::Linear},
                  {"radius", "RADIUS", 0.5f, 8.0f, 1.0f, Curve::Linear},
                  {"mix", "MIX", 0.0f, 1.0f, 1.0f, Curve::Linear}}});

    e.push_back({"pixelate", "PIXELATE", EffectKind::Sampler,
                 "float n = max(p.x, 2.0);\n"
                 "vec2 g = vec2(n * uAspect, n);\n"
                 "vec2 q = (floor(uv * g) + 0.5) / g;\n"
                 "return nvSample(uInput, q);",
                 {{"blocks", "BLOCKS", 2.0f, 512.0f, 96.0f, Curve::Exp}}});

    e.push_back({"kaleido", "KALEIDO", EffectKind::Sampler,
                 "vec2 d = (uv - 0.5) * vec2(uAspect, 1.0);\n"
                 "float a = atan(d.y, d.x) + p.y;\n"
                 "float r = length(d);\n"
                 "float seg = 6.28318 / max(p.x, 1.0);\n"
                 "a = abs(mod(a + seg * 0.5, seg) - seg * 0.5);\n"
                 "vec2 q = vec2(cos(a), sin(a)) * r / vec2(uAspect, 1.0) + 0.5;\n"
                 "return nvSample(uInput, q);",
                 {{"slices", "SLICES", 1.0f, 24.0f, 6.0f, Curve::Linear},
                  {"spin", "SPIN", -3.14159f, 3.14159f, 0.0f, Curve::Bipolar}}});

    e.push_back({"mirror", "MIRROR", EffectKind::Sampler,
                 "vec2 q = uv;\n"
                 "if (p.x > 0.5) q.x = abs(q.x - 0.5) + 0.5;\n"
                 "if (p.y > 0.5) q.y = abs(q.y - 0.5) + 0.5;\n"
                 "return nvSample(uInput, q);",
                 {{"horiz", "HORIZ", 0.0f, 1.0f, 1.0f, Curve::Linear},
                  {"vert", "VERT", 0.0f, 1.0f, 0.0f, Curve::Linear}}});

    e.push_back({"rgbsplit", "RGB SPLIT", EffectKind::Sampler,
                 "vec2 o = vec2(cos(p.y), sin(p.y)) * p.x * uTexel * 40.0;\n"
                 "return vec4(nvSample(uInput, uv + o).r,\n"
                 "            nvSample(uInput, uv).g,\n"
                 "            nvSample(uInput, uv - o).b, 1.0);",
                 {{"amount", "AMT", 0.0f, 8.0f, 1.0f, Curve::Linear},
                  {"angle", "ANGLE", -3.14159f, 3.14159f, 0.0f, Curve::Bipolar}}});

    e.push_back({"zoomblur", "ZOOM BLUR", EffectKind::Sampler,
                 "vec2 dir = (vec2(0.5) - uv) * p.x * 0.05;\n"
                 "vec4 a = vec4(0.0);\n"
                 "for (int i = 0; i < 8; ++i) {\n"
                 "  a += nvSample(uInput, uv + dir * float(i) * 0.125);\n"
                 "}\n"
                 "return a * 0.125;",
                 {{"amount", "AMT", 0.0f, 4.0f, 1.0f, Curve::Linear}}});

    e.push_back({"warp", "NOISE WARP", EffectKind::Sampler,
                 "vec2 q = uv;\n"
                 "q.x += sin(uv.y * p.y + uTime * p.z) * p.x * 0.05;\n"
                 "q.y += cos(uv.x * p.y * 1.3 + uTime * p.z * 0.8) * p.x * 0.05;\n"
                 "return nvSample(uInput, q);",
                 {{"amount", "AMT", 0.0f, 4.0f, 1.0f, Curve::Linear},
                  {"freq", "FREQ", 0.5f, 40.0f, 8.0f, Curve::Exp},
                  {"speed", "SPEED", -4.0f, 4.0f, 1.0f, Curve::Bipolar}}});

    // ---- history: temporal feedback, the other iteration axis -----------
    e.push_back({"feedback", "FEEDBACK", EffectKind::History,
                 "vec2 d = (uv - 0.5) * vec2(uAspect, 1.0);\n"
                 "d = nvRot(p.z) * d * (1.0 - p.y);\n"
                 "vec2 q = d / vec2(uAspect, 1.0) + 0.5;\n"
                 "vec4 prev = nvSample(uHistory, q) * p.w;\n"
                 "vec4 src = nvSample(uInput, uv);\n"
                 // A crossfade, not max(). max() is the classic trails op, but
                 // it is a RATCHET: it can only ever raise a pixel, so any
                 // effect downstream that adds energy -- grain, bloom, a
                 // brightness lift -- feeds back in and the whole frame climbs
                 // to white and stays there. mix() is bounded by its inputs, so
                 // it gives the same smear and cannot run away.
                 "return mix(src, prev, p.x);",
                 {{"amount", "AMT", 0.0f, 0.98f, 0.5f, Curve::Linear},
                  {"zoom", "ZOOM", -0.1f, 0.1f, 0.01f, Curve::Bipolar},
                  {"rotate", "ROT", -0.2f, 0.2f, 0.01f, Curve::Bipolar},
                  {"decay", "DECAY", 0.5f, 1.0f, 0.96f, Curve::Linear}}});

    // Deliberately still max(): this IS the ratchet look, kept as its own
    // effect rather than as a hidden mode of FEEDBACK. The decay ceiling stops
    // short of 1.0 so it always eventually settles.
    e.push_back({"trails", "TRAILS", EffectKind::History,
                 "vec4 prev = nvSample(uHistory, uv) * p.x;\n"
                 "return max(nvSample(uInput, uv), prev);",
                 {{"decay", "DECAY", 0.5f, 0.99f, 0.93f, Curve::Linear}}});

    e.push_back({"displace", "DISPLACE", EffectKind::History,
                 "vec4 prev = nvSample(uHistory, uv);\n"
                 "vec2 o = (prev.rg - 0.5) * p.x * 0.05;\n"
                 "return nvSample(uInput, uv + o);",
                 {{"amount", "AMT", 0.0f, 4.0f, 1.0f, Curve::Linear}}});

    return e;
}

const std::vector<EffectDef>& effectBank() {
    static const std::vector<EffectDef> bank = buildBank();
    return bank;
}

int findEffect(const std::string& id) {
    const auto& b = effectBank();
    for (size_t i = 0; i < b.size(); ++i) {
        if (b[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace nv

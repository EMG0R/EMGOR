#include "layers/ink.h"

#include <cmath>
#include <cstdio>

#include "shader_source.h"

namespace nv {

// One dab. Drawn as a full-screen triangle with a distance test rather than
// as geometry: the brush is a handful of pixels and the setup cost of a real
// quad per dab would dwarf the fill.
static const char* kBrushFrag =
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uField;\n"
    "uniform sampler2D uPalette;\n"
    "uniform vec2  uSrcScale;\n"
    "uniform vec2  uPos;\n"
    "uniform float uRadius;\n"
    "uniform float uFlow;\n"
    "uniform float uSoft;\n"
    "uniform float uAspect;\n"
    "uniform float uPaletteShift;\n"
    "uniform float uPaletteMix;\n"
    "uniform float uFieldColor;\n"
    "void main() {\n"
    "  vec2 d = (vUV - uPos) * vec2(uAspect, 1.0);\n"
    "  float r = length(d) / max(uRadius, 1e-4);\n"
    "  if (r > 1.0) discard;\n"
    "  float a = pow(1.0 - r, mix(1.0, 4.0, uSoft)) * uFlow;\n"
    "\n"
    // The brush takes its colour from the fractal under the cursor, so a
    // stroke belongs to the image it is drawn on.
    "  vec3 c = texture(uField, clamp(uPos, 0.0, 1.0) * uSrcScale).rgb;\n"
    "  float t = fract(dot(c, vec3(0.2126, 0.7152, 0.0722)) * uFieldColor\n"
    "                  + uPaletteShift);\n"
    "  vec3 c1 = texture(uPalette, vec2(t, 0.25)).rgb;\n"
    "  vec3 c2 = texture(uPalette, vec2(t, 0.75)).rgb;\n"
    "  vec3 col = mix(c1, c2, uPaletteMix);\n"
    "  fragColor = vec4(col * a, a);\n"
    "}\n";

static const char* kFadeFrag =
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "uniform float uKeep;\n"
    "void main() { fragColor = vec4(uKeep); }\n";

bool Ink::init(int maxW, int maxH, int contextCount) {
    maxW_ = maxW;
    maxH_ = maxH;
    tri_.create(contextCount);

    glGenTextures(1, &fboTex_);
    glBindTexture(GL_TEXTURE_2D, fboTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, maxW, maxH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fboTex_, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[ink] framebuffer incomplete (0x%04x)\n", st);
        shutdown();
        return false;
    }

    std::string vert = std::string(glslPreamble()) + glslFullscreenVert();
    if (!brush_.build(vert, std::string(glslPreamble()) + kBrushFrag, "ink_brush"))
        return false;
    if (!fade_.build(vert, std::string(glslPreamble()) + kFadeFrag, "ink_fade"))
        return false;

    clear();
    return true;
}

void Ink::shutdown() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (fboTex_) glDeleteTextures(1, &fboTex_);
    fbo_ = fboTex_ = 0;
    brush_.destroy();
    fade_.destroy();
    tri_.destroy();
}

void Ink::clear() {
    cleared_ = false;
    hasContent_ = false;
    dabs_.clear();
    haveLast_ = false;
}

void Ink::addDab(float nx, float ny) {
    // Interpolate from the previous sample. At 60Hz a fast drag lands its
    // samples far apart, and stamping only at those points draws a dotted
    // line rather than a stroke.
    if (haveLast_) {
        float dx = nx - lastX_, dy = ny - lastY_;
        float dist = std::sqrt(dx * dx + dy * dy);
        int steps = static_cast<int>(dist / 0.004f);
        if (steps > 64) steps = 64;  // a jump across the surface is not a stroke
        for (int i = 1; i < steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            dabs_.push_back(lastX_ + dx * t);
            dabs_.push_back(lastY_ + dy * t);
        }
    }
    dabs_.push_back(nx);
    dabs_.push_back(ny);
    lastX_ = nx;
    lastY_ = ny;
    haveLast_ = true;
}

void Ink::ensureCleared() {
    if (cleared_) return;
    // The canvas is allocated once at max size and only ever sub-viewported,
    // so the clear is the one moment its contents are defined. Anything that
    // binds it has to come through here or it inherits the last mode's frame.
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    cleared_ = true;
}

void Ink::beginDeposit(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w, h);
    ensureCleared();
    // Whatever the caller draws now is content. Without this the composite
    // would keep the canvas at zero level and the deposits would go into a
    // buffer nothing samples -- the loop would be closed but invisible.
    hasContent_ = true;
}

void Ink::stamp(int w, int h, GLuint field, float sx, float sy, GLuint palette,
                float paletteShift, float paletteMix, const InkParams& p,
                float dt, int ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w, h);
    ensureCleared();

    // Fade, if asked. Multiplying the whole buffer down is how strokes decay
    // without any per-stroke bookkeeping.
    if (p.fade > 0.0001f) {
        float keep = std::pow(1.0f - p.fade, dt);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        fade_.use();
        fade_.set1f("uKeep", keep);
        tri_.draw(ctx);
        glDisable(GL_BLEND);
    }

    if (!dabs_.empty()) {
        glEnable(GL_BLEND);
        // Premultiplied-alpha over: dabs accumulate into a stroke instead of
        // each one replacing the last.
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        brush_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, field);
        brush_.set1i("uField", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette);
        brush_.set1i("uPalette", 1);
        brush_.set2f("uSrcScale", sx, sy);
        brush_.set1f("uRadius", p.size);
        brush_.set1f("uFlow", p.flow);
        brush_.set1f("uSoft", p.softness);
        brush_.set1f("uAspect", h > 0 ? static_cast<float>(w) / h : 1.0f);
        brush_.set1f("uPaletteShift", paletteShift);
        brush_.set1f("uPaletteMix", paletteMix);
        brush_.set1f("uFieldColor", p.fieldColor);

        for (size_t i = 0; i + 1 < dabs_.size(); i += 2) {
            brush_.set2f("uPos", dabs_[i], dabs_[i + 1]);
            tri_.draw(ctx);
        }
        glDisable(GL_BLEND);
        hasContent_ = true;
        dabs_.clear();
    }
}

}  // namespace nv

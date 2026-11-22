#include "gfx/fbo.h"

#include <cstdio>

namespace nv {

Fbo::~Fbo() { destroy(); }

bool Fbo::create(int maxW, int maxH, bool linearFilter) {
    destroy();
    maxW_ = maxW;
    maxH_ = maxH;

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, maxW, maxH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    GLint filt = linearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    // Clamp, not repeat: a feedback loop that wraps at the edge smears the
    // opposite side of the frame back in, which reads as a glitch rather than
    // as feedback.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex_, 0);

    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[fbo] incomplete (0x%04x) at %dx%d\n", st, maxW, maxH);
        destroy();
        return false;
    }
    return true;
}

void Fbo::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_ = 0;
    tex_ = 0;
    maxW_ = maxH_ = 0;
}

void Fbo::bind(int activeW, int activeH) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, activeW, activeH);
}

}  // namespace nv

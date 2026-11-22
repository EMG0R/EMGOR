#include "gfx/quad.h"

namespace nv {

FullscreenTri::~FullscreenTri() { destroy(); }

void FullscreenTri::create(int contextCount) {
    destroy();
    vaos_.assign(contextCount > 0 ? contextCount : 1, 0);
}

void FullscreenTri::destroy() {
    // Only the VAO belonging to the current context can legally be deleted
    // here, and the process is exiting anyway; the driver reclaims the rest
    // when its context goes away.
    for (GLuint v : vaos_) {
        if (v) glDeleteVertexArrays(1, &v);
    }
    vaos_.clear();
}

void FullscreenTri::draw(int ctx) {
    if (ctx < 0 || ctx >= static_cast<int>(vaos_.size())) ctx = 0;
    if (vaos_.empty()) return;
    // Created on first use in each context: at init time only one context is
    // current, so the others cannot be set up yet.
    if (!vaos_[ctx]) glGenVertexArrays(1, &vaos_[ctx]);
    glBindVertexArray(vaos_[ctx]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace nv

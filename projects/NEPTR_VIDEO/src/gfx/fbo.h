#pragma once

#include "gfx/glcommon.h"

namespace nv {

// An offscreen colour target.
//
// Allocated ONCE at the largest size the machine can ever output, then
// rendered into a sub-rect. That is what makes the resolution control a live
// performance knob: changing it sets a viewport and a uv scale, and never
// reallocates, reformats or stalls the pipeline.
//
// RGBA8, deliberately. The Pi's V3D is a tile-based renderer and is bandwidth
// bound long before it is arithmetic bound; RGBA16F would double the traffic
// through the tile store for a precision nobody sees at 8-bit scanout.
class Fbo {
public:
    ~Fbo();
    Fbo() = default;
    Fbo(const Fbo&) = delete;
    Fbo& operator=(const Fbo&) = delete;

    bool create(int maxW, int maxH, bool linearFilter = true);
    void destroy();

    // Binds and sets the viewport to the active sub-rect.
    void bind(int activeW, int activeH);

    GLuint texture() const { return tex_; }
    int maxWidth() const { return maxW_; }
    int maxHeight() const { return maxH_; }

private:
    GLuint fbo_ = 0;
    GLuint tex_ = 0;
    int maxW_ = 0;
    int maxH_ = 0;
};

}  // namespace nv

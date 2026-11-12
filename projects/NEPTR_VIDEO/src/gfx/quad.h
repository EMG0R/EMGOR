#pragma once

#include <vector>

#include "gfx/glcommon.h"

namespace nv {

// The full-screen triangle.
//
// No vertex buffer at all: the vertex shader derives its position from
// gl_VertexID. One primitive, three vertices, zero attribute fetch, and no
// diagonal seam where two triangles would meet -- which matters on a tiler,
// because a seam splits tiles across primitives.
class FullscreenTri {
public:
    ~FullscreenTri();
    void create(int contextCount);
    void destroy();

    // ctx is the currently-bound context. A VAO is not a shared object, so
    // each context gets its own -- drawing with another context's VAO is
    // undefined and on some drivers silently draws nothing.
    void draw(int ctx);

private:
    std::vector<GLuint> vaos_;
};

}  // namespace nv

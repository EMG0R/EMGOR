// Single place where the two GL flavours are reconciled.
//
// macOS: OpenGL 4.1 core (the last version Apple shipped).
// Pi:    OpenGL ES 3.0 via Mesa's v3d driver.
//
// Neither needs a function loader: Apple ships gl3.h with the functions
// linked directly, and libGLESv2 exports the ES 3.0 entry points. The only
// real divergence is the #version line, handled in shader_source.cpp.
#pragma once

#if defined(NV_PLATFORM_GLFW)
  #include <OpenGL/gl3.h>
  #include <OpenGL/gl3ext.h>
#elif defined(NV_PLATFORM_DRM)
  #include <GLES3/gl3.h>
  #include <GLES3/gl3ext.h>
#else
  #error "no platform defined"
#endif

#include <cstdio>

namespace nv {

// Only ever called at pipeline-construction time, never per frame. A glGetError
// in the render loop would force a pipeline flush on a tiled renderer.
inline bool glCheck(const char* where) {
    GLenum e = glGetError();
    if (e == GL_NO_ERROR) return true;
    std::fprintf(stderr, "[gl] error 0x%04x at %s\n", e, where);
    return false;
}

}  // namespace nv

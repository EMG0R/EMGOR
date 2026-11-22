#pragma once

#include <string>
#include <unordered_map>

#include "gfx/glcommon.h"

namespace nv {

// A linked program plus a uniform-location cache. Locations are looked up
// once at link time and never again -- a glGetUniformLocation per frame is a
// driver round trip we cannot afford at 60Hz on the Pi.
class Program {
public:
    ~Program();
    Program() = default;
    Program(Program&&) noexcept;
    Program& operator=(Program&&) noexcept;
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    // vertSrc / fragSrc must already carry their #version preamble.
    bool build(const std::string& vertSrc, const std::string& fragSrc,
               const char* debugName);
    void destroy();

    // Takes ownership of an already-linked program -- how a program built on
    // the compiler worker's shared context is handed to the render thread.
    void adopt(GLuint prog);

    bool valid() const { return prog_ != 0; }
    void use() const { glUseProgram(prog_); }
    GLuint id() const { return prog_; }

    GLint loc(const char* name);

    void set1f(const char* n, float v);
    void set2f(const char* n, float a, float b);
    void set3f(const char* n, float a, float b, float c);
    void set4f(const char* n, float a, float b, float c, float d);
    void set1i(const char* n, int v);

private:
    GLuint prog_ = 0;
    std::unordered_map<std::string, GLint> locs_;
};

// Compile + link without wrapping. Callable from any thread that has a
// current context. Returns 0 and logs on failure.
GLuint buildProgramRaw(const std::string& vertSrc, const std::string& fragSrc,
                       const char* debugName);

}  // namespace nv

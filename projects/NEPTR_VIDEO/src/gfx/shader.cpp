#include "gfx/shader.h"

#include <cstdio>
#include <vector>

namespace nv {

static GLuint compileStage(GLenum type, const std::string& src, const char* name) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] %s (%s) failed:\n%s\n", name,
                     type == GL_VERTEX_SHADER ? "vert" : "frag", log.data());
        // Numbered source, because a generated shader has no file to look at.
        int line = 1;
        size_t start = 0;
        for (size_t i = 0; i <= src.size(); ++i) {
            if (i == src.size() || src[i] == '\n') {
                std::fprintf(stderr, "%4d | %.*s\n", line++,
                             static_cast<int>(i - start), src.c_str() + start);
                start = i + 1;
            }
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Program::~Program() { destroy(); }

Program::Program(Program&& o) noexcept : prog_(o.prog_), locs_(std::move(o.locs_)) {
    o.prog_ = 0;
}

Program& Program::operator=(Program&& o) noexcept {
    if (this != &o) {
        destroy();
        prog_ = o.prog_;
        locs_ = std::move(o.locs_);
        o.prog_ = 0;
    }
    return *this;
}

GLuint buildProgramRaw(const std::string& vertSrc, const std::string& fragSrc,
                       const char* debugName) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, vertSrc, debugName);
    if (!vs) return 0;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragSrc, debugName);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] link %s failed:\n%s\n", debugName, log.data());
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

bool Program::build(const std::string& vertSrc, const std::string& fragSrc,
                    const char* debugName) {
    GLuint p = buildProgramRaw(vertSrc, fragSrc, debugName);
    if (!p) return false;
    adopt(p);
    return true;
}

void Program::adopt(GLuint prog) {
    destroy();
    prog_ = prog;
    locs_.clear();
}

void Program::destroy() {
    if (prog_) glDeleteProgram(prog_);
    prog_ = 0;
    locs_.clear();
}

GLint Program::loc(const char* name) {
    auto it = locs_.find(name);
    if (it != locs_.end()) return it->second;
    GLint l = glGetUniformLocation(prog_, name);
    locs_.emplace(name, l);
    return l;
}

// A -1 location is normal, not an error: an optimizing compiler drops
// uniforms a generated shader never ends up reading.
void Program::set1f(const char* n, float v) { GLint l = loc(n); if (l >= 0) glUniform1f(l, v); }
void Program::set2f(const char* n, float a, float b) { GLint l = loc(n); if (l >= 0) glUniform2f(l, a, b); }
void Program::set3f(const char* n, float a, float b, float c) { GLint l = loc(n); if (l >= 0) glUniform3f(l, a, b, c); }
void Program::set4f(const char* n, float a, float b, float c, float d) { GLint l = loc(n); if (l >= 0) glUniform4f(l, a, b, c, d); }
void Program::set1i(const char* n, int v) { GLint l = loc(n); if (l >= 0) glUniform1i(l, v); }

}  // namespace nv

// Minimal assertion harness. No framework dependency: these tests must build
// and run anywhere the engine does, including on the Pi over ssh.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace check {

inline int& failures() {
    static int f = 0;
    return f;
}

inline void report(bool ok, const char* expr, const char* file, int line,
                   const std::string& detail) {
    if (ok) return;
    ++failures();
    std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expr);
    if (!detail.empty()) std::fprintf(stderr, "     %s\n", detail.c_str());
}

inline int finish(const char* suite) {
    if (failures() == 0) {
        std::printf("ok   %s\n", suite);
        return 0;
    }
    std::fprintf(stderr, "FAILED %s (%d)\n", suite, failures());
    return 1;
}

}  // namespace check

#define CHECK(expr) \
    ::check::report((expr), #expr, __FILE__, __LINE__, "")

#define CHECK_EQ(a, b)                                                     \
    ::check::report((a) == (b), #a " == " #b, __FILE__, __LINE__,          \
                    "got " + std::to_string(a) + ", want " + std::to_string(b))

#define CHECK_NEAR(a, b, eps)                                              \
    ::check::report(((a) - (b) < (eps)) && ((b) - (a) < (eps)),            \
                    #a " ~= " #b, __FILE__, __LINE__,                      \
                    "got " + std::to_string(a) + ", want " + std::to_string(b))

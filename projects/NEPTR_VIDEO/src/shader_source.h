// Shader text assembly.
//
// The two GL flavours differ in exactly one line -- the #version -- so that
// difference is isolated here and nothing else in the engine has to know
// which machine it is running on. Shader bodies are written once, in a GLSL
// ES 3.0 subset that desktop GLSL 4.10 also accepts.
#pragma once

#include <string>

namespace nv {

// #version + precision, chosen at compile time for the target GL.
const char* glslPreamble();

// Helpers every generated shader gets: complex arithmetic, colour utilities,
// and nvSample(), which hides the fact that buffers are allocated at max size
// and rendered into a sub-viewport.
const char* glslCommon();

// Vertex shader for the full-screen triangle. Shared by every pass.
const char* glslFullscreenVert();
// Same triangle, but the OUTPUT is turned by uRot degrees clockwise. Used
// only by the present pass -- the offscreen chain passes must never rotate,
// or every effect would sample a turned image.
const char* glslPresentVert();

// The fractal core template. %%STEP%%, %%AUXINIT%%, %%DEFINES%% get
// substituted per kernel; the result is cached by kernel id.
const char* glslCoreTemplate();

// Final present pass: samples the chain result and scales it to the output.
const char* glslPresentFrag();

}  // namespace nv

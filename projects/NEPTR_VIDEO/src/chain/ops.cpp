#include "chain/ops.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nv {
namespace {

const std::vector<JitOpDef> kOps = {
    {"pass", "PASS A", JitOpKind::Pointwise, 0.0f},
    {"pass_b", "PASS B", JitOpKind::Pointwise, 0.0f},
    {"add", "ADD", JitOpKind::Pointwise, 0.0f},
    {"sub", "SUB", JitOpKind::Pointwise, 0.0f},
    {"rsub", "REVERSE SUB", JitOpKind::Pointwise, 0.0f},
    {"mul", "MULT", JitOpKind::Pointwise, 1.0f},
    {"div", "DIV", JitOpKind::Pointwise, 1.0f},
    {"rdiv", "REVERSE DIV", JitOpKind::Pointwise, 1.0f},
    {"mod", "MOD", JitOpKind::Pointwise, 1.0f},
    {"rmod", "REVERSE MOD", JitOpKind::Pointwise, 1.0f},
    {"min", "MIN", JitOpKind::Pointwise, 1.0f},
    {"max", "MAX", JitOpKind::Pointwise, 0.0f},
    {"avg", "AVERAGE", JitOpKind::Pointwise, 0.0f},
    {"absdiff", "ABS DIFF", JitOpKind::Pointwise, 0.0f},
    {"abs", "ABS A", JitOpKind::Pointwise, 0.0f},
    {"addm", "ADD WRAP", JitOpKind::Pointwise, 0.0f},
    {"subm", "SUB WRAP", JitOpKind::Pointwise, 0.0f},
    {"fold", "FOLD", JitOpKind::Pointwise, 0.0f},
    {"wrap", "WRAP", JitOpKind::Pointwise, 0.0f},
    {"sin", "SINE", JitOpKind::Pointwise, 0.0f},
    {"cos", "COSINE", JitOpKind::Pointwise, 0.0f},
    {"tan", "TANGENT", JitOpKind::Pointwise, 0.0f},
    {"asin", "ARC SINE", JitOpKind::Pointwise, 0.0f},
    {"acos", "ARC COSINE", JitOpKind::Pointwise, 0.0f},
    {"atan", "ARC TANGENT", JitOpKind::Pointwise, 0.0f},
    {"atan2", "ATAN 2", JitOpKind::Pointwise, 0.0f},
    {"sinh", "HYPERBOLIC SINE", JitOpKind::Pointwise, 0.0f},
    {"cosh", "HYPERBOLIC COSINE", JitOpKind::Pointwise, 0.0f},
    {"tanh", "HYPERBOLIC TANGENT", JitOpKind::Pointwise, 0.0f},
    {"pow", "POWER", JitOpKind::Pointwise, 1.0f},
    {"sqrt", "ROOT", JitOpKind::Pointwise, 0.0f},
    {"exp", "EXPONENTIAL", JitOpKind::Pointwise, 0.0f},
    {"exp2", "EXPONENTIAL 2", JitOpKind::Pointwise, 0.0f},
    {"ln", "LOG", JitOpKind::Pointwise, 1.0f},
    {"log2", "LOG 2", JitOpKind::Pointwise, 1.0f},
    {"hypot", "HYPOTENUSE", JitOpKind::Pointwise, 0.0f},
    {"bit_and", "BIT AND", JitOpKind::Pointwise, 1.0f},
    {"bit_or", "BIT OR", JitOpKind::Pointwise, 0.0f},
    {"bit_xor", "BIT XOR", JitOpKind::Pointwise, 0.0f},
    {"bit_not", "BIT NOT", JitOpKind::Pointwise, 0.0f},
    {"shift_left", "SHIFT LEFT", JitOpKind::Pointwise, 0.0f},
    {"shift_right", "SHIFT RIGHT", JitOpKind::Pointwise, 0.0f},
    {"gt", "GREATER", JitOpKind::Pointwise, 0.0f},
    {"lt", "LESS", JitOpKind::Pointwise, 1.0f},
    {"eq", "EQUAL", JitOpKind::Pointwise, 0.0f},
    {"gte", "GREATER OR EQUAL", JitOpKind::Pointwise, 0.0f},
    {"lte", "LESS OR EQUAL", JitOpKind::Pointwise, 1.0f},
    {"gt_pass", "GREATER PASS", JitOpKind::Pointwise, 0.0f},
    {"lt_pass", "LESS PASS", JitOpKind::Pointwise, 1.0f},
    {"eq_pass", "EQUAL PASS", JitOpKind::Pointwise, 0.0f},
    {"neq_pass", "NOT EQUAL PASS", JitOpKind::Pointwise, 0.0f},
    {"screen", "SCREEN", JitOpKind::Pointwise, 0.0f},
    {"displace", "DISPLACE", JitOpKind::Sampler, 0.0f},
    {"polar_displace", "POLAR DISPLACE", JitOpKind::Sampler, 0.0f},
};

template <typename F>
OpVec4 each(OpVec4 a, OpVec4 b, F f) {
    return {f(a.x, b.x), f(a.y, b.y), f(a.z, b.z), f(a.w, b.w)};
}

float safeDiv(float a, float b) { return a / (std::fabs(b) < 1e-3f ? (b < 0 ? -1e-3f : 1e-3f) : b); }
float safeMod(float a, float b) { return std::fmod(a, std::fabs(b) < 1e-3f ? 1e-3f : b); }
float byte(float v) { return std::clamp(v, 0.0f, 1.0f); }
float bit(float a, float b, int op) {
    int x = static_cast<int>(byte(a) * 255.0f + 0.5f);
    int y = static_cast<int>(byte(b) * 255.0f + 0.5f);
    int z = 0;
    switch (op) {
        case 0: z = x & y; break;
        case 1: z = x | y; break;
        case 2: z = x ^ y; break;
        case 3: z = (~x) & 255; break;
        case 4: z = (x << (y & 7)) & 255; break;
        default: z = (x >> (y & 7)) & 255; break;
    }
    return static_cast<float>(z) / 255.0f;
}

}  // namespace

const std::vector<JitOpDef>& jitOpBank() { return kOps; }

int jitOpIndex(const std::string& id) {
    for (size_t i = 0; i < kOps.size(); ++i) {
        if (id == kOps[i].id) return static_cast<int>(i);
    }
    return -1;
}

float jitOpIdentity(const JitOpDef& op) { return op.identity; }

OpVec4 jitOpApply(const JitOpDef& op, OpVec4 a, OpVec4 b,
                 bool bActive, bool useValue, float opValue, bool swap) {
    float inactive = useValue ? opValue : op.identity;
    // PASS B has no scalar input identity that can preserve the primary
    // picture. Its inactive form is therefore the graph identity (PASS A);
    // an explicit op.val still deliberately selects the B-side constant.
    if (!bActive && !useValue && std::strcmp(op.id, "pass_b") == 0) return a;
    if (!bActive) b = {inactive, inactive, inactive, inactive};
    if (swap) std::swap(a, b);
    const std::string id = op.id;
    if (id == "pass") return a;
    if (id == "pass_b") return b;
    if (id == "abs") return each(a, b, [](float x, float) { return std::fabs(x); });
    if (id == "sin") return each(a, b, [](float x, float) { return std::sin(x * 6.2831853f) * 0.5f + 0.5f; });
    if (id == "cos") return each(a, b, [](float x, float) { return std::cos(x * 6.2831853f) * 0.5f + 0.5f; });
    if (id == "tan") return each(a, b, [](float x, float) { return std::tanh(std::tan(x * 3.1415926f)); });
    if (id == "asin") return each(a, b, [](float x, float) { return std::asin(std::clamp(x, -1.0f, 1.0f)) / 1.5707963f * 0.5f + 0.5f; });
    if (id == "acos") return each(a, b, [](float x, float) { return std::acos(std::clamp(x, -1.0f, 1.0f)) / 3.1415926f; });
    if (id == "atan") return each(a, b, [](float x, float) { return std::atan(x) / 1.5707963f * 0.5f + 0.5f; });
    if (id == "sinh") return each(a, b, [](float x, float) { return std::tanh(std::sinh(x)); });
    if (id == "cosh") return each(a, b, [](float x, float) { return std::tanh(std::cosh(x) - 1.0f); });
    if (id == "tanh") return each(a, b, [](float x, float) { return std::tanh(x); });
    if (id == "sqrt") return each(a, b, [](float x, float) { return std::sqrt(std::max(x, 0.0f)); });
    if (id == "exp") return each(a, b, [](float x, float) { return 1.0f - std::exp(-x * 3.0f); });
    if (id == "exp2") return each(a, b, [](float x, float) { return 1.0f - std::exp2(-x * 3.0f); });
    if (id == "ln") return each(a, b, [](float x, float) { return std::log(std::max(x, 1e-3f)) * 0.2f + 0.5f; });
    if (id == "log2") return each(a, b, [](float x, float) { return std::log2(std::max(x, 1e-3f)) * 0.15f + 0.5f; });
    if (id == "bit_and") return each(a, b, [](float x, float y) { return bit(x, y, 0); });
    if (id == "bit_or") return each(a, b, [](float x, float y) { return bit(x, y, 1); });
    if (id == "bit_xor") return each(a, b, [](float x, float y) { return bit(x, y, 2); });
    if (id == "bit_not") return each(a, b, [](float x, float y) { return bit(x, y, 3); });
    if (id == "shift_left") return each(a, b, [](float x, float y) { return bit(x, y, 4); });
    if (id == "shift_right") return each(a, b, [](float x, float y) { return bit(x, y, 5); });
    if (id == "gt") return each(a, b, [](float x, float y) { return x > y ? 1.0f : 0.0f; });
    if (id == "lt") return each(a, b, [](float x, float y) { return x < y ? 1.0f : 0.0f; });
    if (id == "eq") return each(a, b, [](float x, float y) { return std::fabs(x-y) < 0.01f ? 1.0f : 0.0f; });
    if (id == "gte") return each(a, b, [](float x, float y) { return x >= y ? 1.0f : 0.0f; });
    if (id == "lte") return each(a, b, [](float x, float y) { return x <= y ? 1.0f : 0.0f; });
    if (id == "gt_pass") return each(a, b, [](float x, float y) { return x > y ? x : 0.0f; });
    if (id == "lt_pass") return each(a, b, [](float x, float y) { return x < y ? x : 0.0f; });
    if (id == "eq_pass") return each(a, b, [](float x, float y) { return std::fabs(x-y) < 0.01f ? x : 0.0f; });
    if (id == "neq_pass") return each(a, b, [](float x, float y) { return std::fabs(x-y) >= 0.01f ? x : 0.0f; });
    if (id == "screen") return each(a, b, [](float x, float y) { return 1.0f - (1.0f-x) * (1.0f-y); });
    if (id == "pow") return each(a, b, [](float x, float y) { return std::pow(std::max(x, 0.0f), y * 4.0f); });
    if (id == "hypot") return each(a, b, [](float x, float y) { return std::min(std::sqrt(x*x+y*y), 1.0f); });
    if (id == "atan2") return each(a, b, [](float x, float y) { return std::atan2(x, y) / 3.1415926f * 0.5f + 0.5f; });
    if (id == "addm") return each(a, b, [](float x, float y) { return std::fmod(x+y+1.0f, 1.0f); });
    if (id == "subm") return each(a, b, [](float x, float y) { return std::fmod(x-y+2.0f, 1.0f); });
    if (id == "fold") return each(a, b, [](float x, float y) { return 1.0f - std::fabs(std::fmod(x+y, 2.0f) - 1.0f); });
    if (id == "wrap") return each(a, b, [](float x, float y) { return x+y-std::floor(x+y); });
    if (id == "sub") return each(a, b, [](float x, float y) { return x-y; });
    if (id == "rsub") return each(a, b, [](float x, float y) { return y-x; });
    if (id == "mul") return each(a, b, [](float x, float y) { return x*y; });
    if (id == "div") return each(a, b, safeDiv);
    if (id == "rdiv") return each(a, b, [](float x, float y) { return safeDiv(y, x); });
    if (id == "mod") return each(a, b, safeMod);
    if (id == "rmod") return each(a, b, [](float x, float y) { return safeMod(y, x); });
    if (id == "min") return each(a, b, [](float x, float y) { return std::min(x,y); });
    if (id == "max") return each(a, b, [](float x, float y) { return std::max(x,y); });
    if (id == "avg") return each(a, b, [](float x, float y) { return (x+y)*0.5f; });
    if (id == "absdiff") return each(a, b, [](float x, float y) { return std::fabs(x-y); });
    return a;
}

const char* jitOpGlsl() {
    return R"GLSL(
float nvSafeDiv(float a, float b) { return a / (abs(b) < 1e-3 ? (b < 0.0 ? -1e-3 : 1e-3) : b); }
float nvSafeMod(float a, float b) { return mod(a, abs(b) < 1e-3 ? 1e-3 : b); }
float nvByte(float x) { return clamp(x, 0.0, 1.0); }
float nvBit(float a, float b, int op) {
  int x = int(nvByte(a) * 255.0 + 0.5);
  int y = int(nvByte(b) * 255.0 + 0.5);
  int z = 0;
  if (op == 0) z = x & y; else if (op == 1) z = x | y;
  else if (op == 2) z = x ^ y; else if (op == 3) z = (~x) & 255;
  else if (op == 4) z = (x << (y & 7)) & 255; else z = (x >> (y & 7)) & 255;
  return float(z) / 255.0;
}
vec4 nvJitOp(vec4 a, vec4 b, int op) {
  switch (op) {
    case 0: return a; case 1: return b; case 2: return a+b; case 3: return a-b;
    case 4: return b-a; case 5: return a*b; case 6: return vec4(nvSafeDiv(a.x,b.x),nvSafeDiv(a.y,b.y),nvSafeDiv(a.z,b.z),nvSafeDiv(a.w,b.w));
    case 7: return vec4(nvSafeDiv(b.x,a.x),nvSafeDiv(b.y,a.y),nvSafeDiv(b.z,a.z),nvSafeDiv(b.w,a.w));
    case 8: return vec4(nvSafeMod(a.x,b.x),nvSafeMod(a.y,b.y),nvSafeMod(a.z,b.z),nvSafeMod(a.w,b.w));
    case 9: return vec4(nvSafeMod(b.x,a.x),nvSafeMod(b.y,a.y),nvSafeMod(b.z,a.z),nvSafeMod(b.w,a.w));
    case 10: return min(a,b); case 11: return max(a,b); case 12: return (a+b)*0.5; case 13: return abs(a-b); case 14: return abs(a);
    case 15: return mod(a+b+1.0,1.0); case 16: return mod(a-b+2.0,1.0); case 17: return 1.0-abs(mod(a+b,2.0)-1.0); case 18: return fract(a+b);
    case 19: return sin(a*6.2831853)*0.5+0.5; case 20: return cos(a*6.2831853)*0.5+0.5; case 21: return tanh(tan(a*3.1415926));
    case 22: return asin(clamp(a,-1.0,1.0))/1.5707963*0.5+0.5; case 23: return acos(clamp(a,-1.0,1.0))/3.1415926;
    case 24: return atan(a)/1.5707963*0.5+0.5; case 25: return atan(a,b)/3.1415926*0.5+0.5;
    case 26: return tanh(sinh(a)); case 27: return tanh(cosh(a)-1.0); case 28: return tanh(a);
    case 29: return pow(max(a,0.0),b*4.0); case 30: return sqrt(max(a,0.0)); case 31: return 1.0-exp(-a*3.0); case 32: return 1.0-exp2(-a*3.0);
    case 33: return log(max(a,1e-3))*0.2+0.5; case 34: return log2(max(a,1e-3))*0.15+0.5; case 35: return min(sqrt(a*a+b*b),1.0);
    case 36: return vec4(nvBit(a.x,b.x,0),nvBit(a.y,b.y,0),nvBit(a.z,b.z,0),nvBit(a.w,b.w,0));
    case 37: return vec4(nvBit(a.x,b.x,1),nvBit(a.y,b.y,1),nvBit(a.z,b.z,1),nvBit(a.w,b.w,1));
    case 38: return vec4(nvBit(a.x,b.x,2),nvBit(a.y,b.y,2),nvBit(a.z,b.z,2),nvBit(a.w,b.w,2));
    case 39: return vec4(nvBit(a.x,b.x,3),nvBit(a.y,b.y,3),nvBit(a.z,b.z,3),nvBit(a.w,b.w,3));
    case 40: return vec4(nvBit(a.x,b.x,4),nvBit(a.y,b.y,4),nvBit(a.z,b.z,4),nvBit(a.w,b.w,4));
    case 41: return vec4(nvBit(a.x,b.x,5),nvBit(a.y,b.y,5),nvBit(a.z,b.z,5),nvBit(a.w,b.w,5));
    case 42: return step(b,a); case 43: return step(a,b); case 44: return vec4(equal(a,b)); case 45: return step(b,a); case 46: return step(a,b);
    case 47: return step(b,a)*a; case 48: return step(a,b)*a;
    case 49: return vec4(equal(a,b))*a; case 50: return (1.0-vec4(equal(a,b)))*a;
    case 51: return 1.0-(1.0-a)*(1.0-b); default: return a;
  }
}
)GLSL";
}

}  // namespace nv

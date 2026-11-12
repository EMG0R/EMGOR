// GORMAPPER shaders — GLSL sources + the hot-reloadable pattern wrapper.
// Pattern bodies live in /patterns/*.glsl (each defines
// `vec3 pattern(vec2 uv, float t)`); wrapPattern() turns one into a full
// fragment shader. Edit a pattern file while the server runs and every zone
// using it recompiles live (SSE from /api/events).

// ---- vertex shaders ---------------------------------------------------------

// Plain polygon vertex shader: positions in projector pixel space,
// vUV in polygon-bounding-box space.
export const ZONE_VS = `#version 300 es
layout(location = 0) in vec2 aPos;  // projector pixels
layout(location = 1) in vec2 aUV;   // bbox uv
uniform vec2 uRes; // projector resolution
out vec2 vUV;
void main() {
  vUV = aUV;
  vec2 ndc = vec2(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
}`;

// Screen-quad vertex shader: applies a 3x3 homography (unit square -> quad)
// with homogeneous coordinates so the varying interpolation is
// perspective-correct (gl_Position keeps the projective w).
export const QUAD_VS = `#version 300 es
layout(location = 0) in vec2 aUV;   // unit-square corner (0..1)
uniform mat3 uH;   // homography: (u,v,1) -> (x*w, y*w, w) in projector pixels
uniform vec2 uRes;
out vec2 vUV;
void main() {
  vUV = aUV;
  vec3 p = uH * vec3(aUV, 1.0);         // p.xy = pixel*w, p.z = w
  // NDC transform applied homogeneously; keeping w in gl_Position.w makes
  // the GPU interpolate vUV perspective-correctly across the two triangles.
  float cx = 2.0 * p.x / uRes.x - p.z;
  float cy = p.z - 2.0 * p.y / uRes.y;  // flip y (pixels are y-down)
  gl_Position = vec4(cx, cy, 0.0, p.z);
}`;

// ---- shared GLSL helpers (in scope for every pattern file) ------------------

export const NOISE_GLSL = `
float hash21(vec2 p) {
  p = fract(p * vec2(234.34, 435.345));
  p += dot(p, p + 34.23);
  return fract(p.x * p.y);
}
vec2 hash22(vec2 p) {
  float n = hash21(p);
  return vec2(n, hash21(p + n + 17.17));
}
// value noise with smooth (quintic) interpolation
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
  for (int i = 0; i < 5; i++) {
    v += a * vnoise(p);
    p = rot * p * 2.03 + 11.5;
    a *= 0.5;
  }
  return v;
}`;

// Wrap a pattern file's source (which defines `vec3 pattern(vec2 uv, float t)`)
// into a complete fragment shader.
export function wrapPattern(src) {
  // strip //! metadata lines (harmless as comments, but keep line numbers
  // meaningful by replacing them with blank comments)
  const body = src.replace(/^\s*\/\/!.*$/gm, '//');
  return `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform float uTime;
uniform float uSpeed;
uniform float uBrightness;
uniform float uOpacity;
uniform vec3  uColor1;
uniform vec3  uColor2;
uniform vec2  uAspect;   // bbox aspect so noise isn't stretched
${NOISE_GLSL}
${body}
void main() {
  vec3 col = pattern(vUV, uTime * uSpeed);
  col *= uBrightness;
  // hyper-vivid pipeline for dim projectors: saturate, then lift mids
  float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
  col = clamp(mix(vec3(lum), col, 1.5), 0.0, 1.0);
  col = pow(col, vec3(0.72));
  outColor = vec4(col, uOpacity);
}`;
}

// Parse the `//! key: value` metadata header of a pattern file.
// Returns { name, params: [..] } with sensible fallbacks.
export function parsePatternMeta(src, fallbackId) {
  const meta = { name: fallbackId, params: ['speed', 'color1', 'color2', 'brightness', 'opacity'] };
  for (const line of src.split('\n').slice(0, 12)) {
    const m = /^\s*\/\/!\s*(\w+)\s*:\s*(.+)$/.exec(line);
    if (!m) continue;
    if (m[1] === 'name') meta.name = m[2].trim();
    else if (m[1] === 'params') meta.params = m[2].split(',').map((s) => s.trim()).filter(Boolean);
  }
  return meta;
}

// ---- fixed fragment shaders -------------------------------------------------

// Textured — screen-capture objects and media zones. uUVScale/uUVOffset
// implement cover-fit (identity for a homography quad).
export const TEX_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTex;
uniform float uOpacity;
uniform float uBrightness;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;
void main() {
  vec2 uv = vUV * uUVScale + uUVOffset;
  vec4 c = texture(uTex, clamp(uv, 0.0, 1.0));
  outColor = vec4(c.rgb * uBrightness, c.a * uOpacity);
}`;

// LED-ring bleed — the outer-shell object continues the physical LED strip
// above it down the wall: the smartGOR lightserver's 1920px room ring streams
// in (UDP tap -> /api/ring), a calibrated window of it maps across the zone,
// glows downward, and bright objects crossing the window spawn reflection
// bursts that shoot off. Feed stale/absent -> renders black (uLive = 0).
export const RING_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uRing;      // 1920x1 ring frame
uniform vec2 uWin;            // window start / span, as fractions of the ring
uniform float uFlip;
uniform float uBrightness;
uniform float uOpacity;
uniform float uLive;          // 0 when the tap is silent — nothing renders
uniform vec4 uBursts[8];      // x: u pos, y: age s, z: lateral dir, w: strength
uniform int uBurstCount;

vec3 ringAt(float u) {
  float x = uWin.x + clamp(u, 0.0, 1.0) * uWin.y;
  return texture(uRing, vec2(x, 0.5)).rgb;
}
void main() {
  float u = uFlip > 0.5 ? 1.0 - vUV.x : vUV.x;
  vec3 c = vec3(0.0);
  float wsum = 0.0;
  for (int i = -3; i <= 3; i++) {          // soften the 1-D strip into a glow
    float w = 1.0 - abs(float(i)) / 4.0;
    c += ringAt(u + float(i) * 0.004) * w;
    wsum += w;
  }
  c /= wsum;
  // brightest at the top (nearest the physical strip), bleeding down the wall
  c *= pow(1.0 - vUV.y, 1.6) * 0.92 + 0.08;
  // reflections: crossing objects shoot down and off
  for (int i = 0; i < 8; i++) {
    if (i >= uBurstCount) break;
    vec4 b = uBursts[i];
    vec2 d = vec2((u - (b.x + b.z * b.y * 0.35)) * 2.2, vUV.y - b.y * 1.6);
    c += ringAt(b.x) * exp(-dot(d, d) * 42.0) * b.w * exp(-b.y * 2.2) * 3.0;
  }
  c *= uBrightness * uLive;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));   // vivid pipeline, as patterns
  c = clamp(mix(vec3(lum), c, 1.5), 0.0, 1.0);
  c = pow(c, vec3(0.72));
  outColor = vec4(c, uOpacity);
}`;

// Shared preamble for the reactive ring family (aurora / particles / custom
// wall programs): ring window sampler, room mood, ground-truth comets, style
// params, noise kit. Custom Pi-authored programs compile against exactly this
// interface — see wrapWallProgram().
export const RING_COMMON = `
uniform sampler2D uRing;
uniform vec2 uWin;
uniform float uFlip;
uniform float uBrightness;
uniform float uOpacity;
uniform float uLive;
uniform float uEnergy;        // 0..1.5 smoothed room energy
uniform vec3 uPalA;           // dominant ring color
uniform vec3 uPalB;           // secondary ring color
uniform vec4 uComets[8];      // x: u pos, y: age s, z: vel (window units/s), w: strength
uniform int uCometCount;
uniform vec4 uPrm;            // density, speed, turbulence, hue_drift
uniform float uFade;

float rhash(vec2 p) {
  p = fract(p * vec2(234.34, 435.345));
  p += dot(p, p + 34.23);
  return fract(p.x * p.y);
}
float rnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(rhash(i), rhash(i + vec2(1, 0)), u.x),
             mix(rhash(i + vec2(0, 1)), rhash(i + vec2(1, 1)), u.x), u.y);
}
float rfbm(vec2 p) {
  float a = 0.5, s = 0.0;
  for (int i = 0; i < 4; i++) { s += a * rnoise(p); p *= 2.03; a *= 0.5; }
  return s;
}
vec3 ringAt(float u) {
  float x = uWin.x + clamp(u, 0.0, 1.0) * uWin.y;
  return texture(uRing, vec2(x, 0.5)).rgb;
}`;

// Shared tail: vivid post + live gate, applied to whatever color the body made.
export const RING_POST = `
  c *= uBrightness * uLive;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = clamp(mix(vec3(lum), c, 1.5), 0.0, 1.0);
  c = pow(c, vec3(0.72));
  outColor = vec4(c, uOpacity);`;

// AURORA style — slow curtains hanging from the strip: fbm-displaced vertical
// bands in the room's palette, breathing with energy; comets send a slow
// brightness wave through the curtains from their position. Vibes, not mirror.
export const RING_AURORA_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform float uTime;
${RING_COMMON}
void main() {
  float u = uFlip > 0.5 ? 1.0 - vUV.x : vUV.x;
  float t = uTime * (0.4 + uPrm.y * 0.6);
  // curtain displacement field
  float sway = rfbm(vec2(u * 3.0 + t * 0.07, vUV.y * 0.8 - t * 0.05)) - 0.5;
  float bands = 0.0;
  for (int i = 0; i < 3; i++) {
    float fi = float(i);
    float x = u * (4.0 + fi * 3.0) + sway * (1.2 + fi * 0.5) + fi * 7.31 + t * (0.03 + fi * 0.02);
    bands += (0.5 + 0.5 * sin(x * 6.2831)) * (0.5 - fi * 0.12);
  }
  bands = pow(bands, 2.2 - uEnergy);
  // hang from the top, shimmer at the hem
  float hang = pow(1.0 - vUV.y, 0.9 + uFade) * (0.75 + 0.25 * rnoise(vec2(u * 20.0, t * 0.7)));
  vec3 c = mix(uPalB, uPalA, bands) * bands * hang * (0.5 + uEnergy * 1.1);
  c += ringAt(u) * 0.10 * hang;   // faint grounding in the actual strip colors
  // pin-in-water: comet strikes ring outward through the curtains carrying
  // the EXACT strip color from the strike point, free to reach every edge
  // (deliberately NOT multiplied by hang/bands — full-screen propagation)
  for (int i = 0; i < 8; i++) {
    if (i >= uCometCount) break;
    vec4 b = uComets[i];
    float r = length((vec2(u, vUV.y) - vec2(b.x, 0.04)) * vec2(1.9, 1.0));
    float wave = exp(-abs(r - b.y * 0.75) * 11.0) * exp(-b.y * 0.9) * b.w;
    c += ringAt(b.x) * wave * (0.5 + bands * 1.1);
  }
${RING_POST}
}`;

// PARTICLES style — a drifting ember/firefly field: procedural particles rise
// and wander with turbulence, spawn density and pace track room energy, and
// comets burst into short-lived spark showers at their position.
export const RING_PARTICLES_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform float uTime;
${RING_COMMON}
vec3 particleGlow(vec2 p, vec2 pos, float size, vec3 col, float k) {
  vec2 d = (p - pos) * vec2(1.9, 1.0);
  return col * exp(-dot(d, d) * k / size);
}
void main() {
  float u = uFlip > 0.5 ? 1.0 - vUV.x : vUV.x;
  vec2 uv = vec2(u, vUV.y);
  float t = uTime * (0.5 + uPrm.y * 0.7);
  vec3 c = vec3(0.0);
  // ambient wash so the field never floats in pure black
  c += mix(uPalB, uPalA, rfbm(uv * 2.0 + t * 0.05)) * 0.10 * (0.4 + uEnergy);
  int n = int(10.0 + uPrm.x * 14.0);
  for (int i = 0; i < 24; i++) {
    if (i >= n) break;
    float fi = float(i);
    float s1 = rhash(vec2(fi, 1.7)), s2 = rhash(vec2(fi, 9.1)), s3 = rhash(vec2(fi, 4.3));
    float rise = 0.03 + s2 * 0.10 * (0.5 + uEnergy);
    float y = fract(s2 + t * rise * (uFade > 1.0 ? 1.0 : -1.0) * -1.0);   // drift upward
    float x = fract(s1 + t * 0.01 * (s3 - 0.5)
      + (rnoise(vec2(fi * 3.1, t * 0.25)) - 0.5) * 0.22 * (0.4 + uPrm.z));
    float tw = 0.55 + 0.45 * sin(t * (2.0 + s3 * 5.0) + fi);
    vec3 col = mix(uPalA, uPalB, s3);
    c += particleGlow(uv, vec2(x, y), 0.5 + s1, col, 2400.0) * tw * (0.35 + uEnergy * 0.9);
  }
  // pin-in-water strikes: an expanding ripple ring in the strike point's
  // EXACT strip color (reaches every edge), plus a spark shower riding it
  for (int i = 0; i < 8; i++) {
    if (i >= uCometCount) break;
    vec4 b = uComets[i];
    vec3 strike = ringAt(b.x);
    float r = length((uv - vec2(b.x, 0.05)) * vec2(1.9, 1.0));
    c += strike * exp(-abs(r - b.y * 0.8) * 14.0) * exp(-b.y * 0.9) * b.w * 0.9;
    for (int j = 0; j < 6; j++) {
      float fj = float(j);
      float a1 = rhash(vec2(fj, b.x * 91.0)), a2 = rhash(vec2(fj * 2.7, b.x * 37.0));
      vec2 pos = vec2(b.x + (a1 - 0.5) * (0.1 + b.y * 0.5) + b.z * b.y * 0.4,
                      0.06 + b.y * (0.4 + a2 * 0.9));
      c += particleGlow(uv, pos, 0.4 + a2, strike, 3200.0)
        * exp(-b.y * 1.8) * b.w * 1.5;
    }
  }
${RING_POST}
}`;

// Wrap a Pi-authored wall program (defines: vec3 wall(vec2 uv, float t))
// into a complete fragment shader against the RING_COMMON interface.
export function wrapWallProgram(body) {
  return `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform float uTime;
${RING_COMMON}
${body}
void main() {
  vec2 uv = vec2(uFlip > 0.5 ? 1.0 - vUV.x : vUV.x, vUV.y);
  vec3 c = wall(uv, uTime);
${RING_POST}
}`;
}

// LED-ring REACTIVE mode — the default "cool-ass" shell renderer: a curl-noise
// flow field the room conducts. The ring window acts as an energy spectrum
// injected along the top edge; global ring energy drives the field's
// liveliness; the dominant ring palette colors it; crossing objects spawn
// comets that dive into the field with their tracked velocity. It *reacts* to
// the room rather than mirroring it — the Pi stays the conductor.
export const RING_REACT_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uRing;
uniform vec2 uWin;
uniform float uFlip;
uniform float uTime;
uniform float uBrightness;
uniform float uOpacity;
uniform float uLive;
uniform float uEnergy;        // 0..1 global ring energy (smoothed)
uniform vec3 uPalA;           // dominant ring color (smoothed)
uniform vec3 uPalB;           // secondary ring color (smoothed)
uniform vec4 uBursts[8];      // x: u pos, y: age s, z: signed speed, w: strength
uniform int uBurstCount;
uniform float uTrackLive;     // 1 = ground-truth tracks present: they own the
                              // moving object; the lagged pixel plume ducks

float rhash(vec2 p) {
  p = fract(p * vec2(234.34, 435.345));
  p += dot(p, p + 34.23);
  return fract(p.x * p.y);
}
float rnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(rhash(i), rhash(i + vec2(1, 0)), u.x),
             mix(rhash(i + vec2(0, 1)), rhash(i + vec2(1, 1)), u.x), u.y);
}
float rfbm(vec2 p) {
  float a = 0.5, s = 0.0;
  for (int i = 0; i < 4; i++) { s += a * rnoise(p); p *= 2.03; a *= 0.5; }
  return s;
}
vec3 ringAt(float u) {
  float x = uWin.x + clamp(u, 0.0, 1.0) * uWin.y;
  return texture(uRing, vec2(x, 0.5)).rgb;
}
void main() {
  float u = uFlip > 0.5 ? 1.0 - vUV.x : vUV.x;
  float t = uTime;
  float drive = 0.25 + uEnergy * 1.6;          // room energy sets the tempo

  // curl-ish flow field: domain-warped fbm falling downward, faster when hot
  vec2 p = vec2(u * 3.0, vUV.y * 1.8);
  vec2 warp = vec2(rfbm(p * 1.7 + vec2(0.0, -t * 0.55 * drive)),
                   rfbm(p * 1.7 + vec2(5.2, -t * 0.42 * drive)));
  float field = rfbm(p + warp * 1.6 + vec2(0.0, -t * 0.35 * drive));
  field = pow(field, 1.7 - uEnergy * 0.6);

  // the window as a spectrum: local strip brightness feeds plumes from the top
  vec3 spec = ringAt(u + (rfbm(vec2(u * 9.0, t * 0.3)) - 0.5) * 0.06);
  float specLum = dot(spec, vec3(0.30, 0.59, 0.11));
  float plume = specLum * exp(-vUV.y * (3.5 - uEnergy * 1.8));
  // when tracks are live, the extrapolated comets are the single source of
  // truth for motion — mute the raw-pixel plume so the lagged ghost never
  // competes with the sync-corrected object
  plume *= mix(1.0, 0.12, uTrackLive);

  vec3 c = mix(uPalB, uPalA, field) * field * (0.35 + uEnergy);
  c += mix(uPalA, spec, 0.65) * plume * 1.4;

  // crossing objects dive in as comets, curving with their real velocity
  for (int i = 0; i < 8; i++) {
    if (i >= uBurstCount) break;
    vec4 b = uBursts[i];
    float age = b.y;
    vec2 head = vec2(b.x + b.z * age * 0.9, age * (1.1 + abs(b.z) * 0.5));
    vec2 d = vUV - head;
    d.x = (uFlip > 0.5 ? -(1.0 - vUV.x - head.x) : d.x);
    d.x *= 2.0;
    float comet = exp(-dot(d, d) * 55.0);
    vec2 tail = d + vec2(b.z, -1.0) * 0.12;     // smeared back along travel
    comet += 0.5 * exp(-dot(tail, tail) * 28.0);
    c += ringAt(b.x) * comet * b.w * exp(-age * 1.4) * 3.5;
  }

  c *= uBrightness * uLive;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = clamp(mix(vec3(lum), c, 1.5), 0.0, 1.0);
  c = pow(c, vec3(0.72));
  outColor = vec4(c, uOpacity);
}`;

// Flat color — masks.
export const FLAT_FS = `#version 300 es
precision highp float;
out vec4 outColor;
uniform vec4 uColor;
void main() { outColor = uColor; }`;

// Placeholder test card drawn where screen-capture content sits before the
// capture starts (editor preview always uses this; output until you press C).
export const TESTCARD_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 outColor;
uniform float uTime;
uniform float uOpacity;
${NOISE_GLSL}
void main() {
  vec2 uv = vUV;
  // grid
  vec2 g = abs(fract(uv * vec2(16.0, 9.0)) - 0.5);
  float grid = smoothstep(0.47, 0.5, max(g.x, g.y));
  vec3 col = vec3(0.07, 0.08, 0.1) + grid * vec3(0.12, 0.25, 0.3);
  // border
  vec2 b = min(uv, 1.0 - uv);
  float border = smoothstep(0.012, 0.008, min(b.x * 16.0 / 9.0, b.y));
  col = mix(col, vec3(0.2, 0.9, 0.8), border);
  // sweeping diagonal
  float sweep = smoothstep(0.02, 0.0, abs(fract(uv.x + uv.y - uTime * 0.05) - 0.5) - 0.02);
  col += sweep * vec3(0.05, 0.12, 0.14);
  outColor = vec4(col, uOpacity);
}`;

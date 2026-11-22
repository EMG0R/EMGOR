//! name: Solid color
//! params: color1, brightness, opacity

// Every pattern defines: vec3 pattern(vec2 uv, float t)
//   uv  — 0..1 across the zone's bounding box
//   t   — seconds * speed param
// In scope: uColor1, uColor2 (vec3), uAspect (vec2, bbox aspect),
//           hash21(), hash22(), vnoise(), fbm()  — see shaders.js prelude.
// Edit this file while the server runs — zones using it recompile live.

vec3 pattern(vec2 uv, float t) {
  return uColor1;
}

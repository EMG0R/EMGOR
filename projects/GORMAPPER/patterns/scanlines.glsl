//! name: Scanlines
//! params: speed, color1, color2, brightness, opacity

// soft CRT phosphor with a rolling bar, jitter bands, vignette
vec3 pattern(vec2 uv, float t) {
  vec2 p = uv * uAspect;
  vec3 col = mix(uColor1, uColor2, 0.4 + 0.3 * fbm(p * 2.0 + t * 0.1));
  float lines = 0.72 + 0.28 * sin(uv.y * 340.0);
  float roll = fract(uv.y * 0.6 - t * 0.11);
  float bar = smoothstep(0.0, 0.12, roll) * smoothstep(0.3, 0.12, roll);
  col *= lines * (0.75 + 0.6 * bar);
  float glitch = step(0.985, hash21(vec2(floor(uv.y * 60.0), floor(t * 6.0))));
  col += uColor2 * glitch * 0.35;
  vec2 v = uv - 0.5;
  col *= 1.0 - dot(v, v) * 0.9;
  return col;
}

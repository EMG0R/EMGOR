//! name: Lava
//! params: speed, color1, color2, brightness, opacity

// double domain-warped fbm with a molten color ramp
vec3 pattern(vec2 uv, float t) {
  vec2 p = uv * uAspect * 3.0;
  vec2 q = vec2(fbm(p + vec2(0.0, t * 0.12)), fbm(p + vec2(5.2, 1.3 - t * 0.09)));
  vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2)), fbm(p + 4.0 * q + vec2(8.3, 2.8) + t * 0.05));
  float n = fbm(p + 4.0 * r);
  vec3 dark = uColor1 * 0.15;
  vec3 col = mix(dark, uColor1, smoothstep(0.2, 0.55, n));
  col = mix(col, uColor2, smoothstep(0.5, 0.78, n));
  col += uColor2 * 0.6 * smoothstep(0.72, 0.9, n);  // molten highlights
  return col;
}

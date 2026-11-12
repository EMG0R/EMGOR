//! name: Gradient wash
//! params: speed, color1, color2, brightness, opacity

// slowly rotating soft gradient with a breathing blend point
vec3 pattern(vec2 uv, float t) {
  float ang = t * 0.15;
  vec2 dir = vec2(cos(ang), sin(ang));
  float g = dot(uv - 0.5, dir) + 0.5;
  g += 0.08 * fbm(uv * 3.0 + t * 0.2);          // organic edge
  g = smoothstep(0.0, 1.0, g + 0.15 * sin(t * 0.4));
  return mix(uColor1, uColor2, clamp(g, 0.0, 1.0));
}

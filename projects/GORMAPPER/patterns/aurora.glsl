//! name: Aurora
//! params: speed, color1, color2, brightness, opacity

// drifting curtains of light, fading toward the top of the zone
vec3 pattern(vec2 uv, float t) {
  vec2 p = uv * uAspect;
  float warp = fbm(vec2(p.x * 2.0 + t * 0.12, p.y * 0.8 - t * 0.05));
  float x = p.x * 3.0 + warp * 2.2 - t * 0.07;
  float band = fbm(vec2(x, t * 0.1));
  float curtain = smoothstep(0.35, 0.85, band);
  float veil = pow(1.0 - uv.y, 1.4) * (0.55 + 0.45 * fbm(vec2(x * 1.7, uv.y * 2.0 + t * 0.3)));
  float glow = curtain * veil;
  vec3 col = mix(uColor1, uColor2, clamp(uv.y + 0.35 * warp, 0.0, 1.0));
  col += 0.25 * uColor2 * pow(glow, 3.0);       // hot core
  return col * glow * 1.8;
}

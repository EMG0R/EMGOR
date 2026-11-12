//! name: Fireflies
//! params: speed, color1, color2, brightness, opacity

// jittered grid of wandering, pulsing glow points
vec3 pattern(vec2 uv, float t) {
  vec2 p = uv * uAspect * 7.0;
  vec3 col = uColor1 * 0.06;
  vec2 cell = floor(p);
  for (int dy = -1; dy <= 1; dy++)
  for (int dx = -1; dx <= 1; dx++) {
    vec2 c = cell + vec2(float(dx), float(dy));
    vec2 rnd = hash22(c);
    // slow lissajous wander inside the cell
    vec2 pos = c + 0.5 + 0.38 * vec2(sin(t * (0.3 + rnd.x * 0.5) + rnd.x * 6.28),
                                     cos(t * (0.24 + rnd.y * 0.5) + rnd.y * 6.28));
    float d = length(p - pos);
    float pulse = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(t * (0.8 + rnd.y * 1.6) + rnd.x * 40.0), 3.0);
    float glow = pulse * 0.05 / (d * d + 0.004);
    col += mix(uColor1, uColor2, rnd.y) * glow;
  }
  return col * 0.5;
}

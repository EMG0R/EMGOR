//! name: Rain
//! params: speed, color1, color2, brightness, opacity

// three parallax layers of falling streaks with bright heads
vec3 pattern(vec2 uv, float t) {
  vec3 col = uColor1 * 0.12;                        // dim wash behind the rain
  for (int layer = 0; layer < 3; layer++) {
    float fl = float(layer);
    float nCols = 26.0 + fl * 18.0;
    float x = uv.x * uAspect.x * nCols;
    float colId = floor(x);
    float fx = fract(x);
    float rnd = hash21(vec2(colId, fl * 7.0));
    float fall = t * (0.25 + 0.6 * rnd) * (1.0 - fl * 0.22);
    float y = fract(uv.y * (0.6 + 0.5 * rnd) + fall + rnd * 7.0);
    float head = smoothstep(0.10, 0.0, y);          // bright head
    float tail = smoothstep(0.45, 0.0, y) * 0.35;   // fading tail above
    float streak = smoothstep(0.42, 0.5, fx) * smoothstep(0.58, 0.5, fx);
    float drop = (head + tail) * streak * (0.35 + 0.65 * rnd) * (1.0 - fl * 0.28);
    col += mix(uColor1, uColor2, rnd) * drop * 1.6;
  }
  return col;
}

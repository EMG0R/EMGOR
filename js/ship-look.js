// ship-look.js — the ONE look for every structure (7/11, Burger House, CVS, village huts, hub arch, station). Pure module: needs THREE passed in, no DOM.
//
//   API
//     DUSK                        the violet-anchored family 'dusk-lav' (mirror of js/galaxy3d.js:73 FAMILIES[0], cdcc583): { ramp:[deep, lo, mid, hi, sky], accent }  (hex ints)
//     palette(ps)                 -> { deep, lo, mid, hi, sky, accent }  hex ints. Reads the LIVE planet ramp (ps.palette, set by ship-planet when a planet is landed on), else DUSK.
//     TONES                       building roles derived from DUSK (never hand-picked hex per building): the five ramp stops (deep lo mid hi sky) + wall, trim, dark, floor, shelf, shelfD, chrome, accent,
//                                 glow, plus stock[] (4 shelf-stock tones).
//                                 Brand marks stay the only saturated colours and live in the building files (7/11 stripes, Burger red, CVS cross).
//     tone(role, ps?)             the same roles from a live planet ramp.
//     softMat(T, ps, opts)        THE lit material: vertex colour + vertex normal, shared planet light rig (sky fill + wrapped sun key + moon key + fog, exactly ship-planet LIGHT_GLSL) with a
//                                 4-step soft toon ramp on the key and a faint view rim. opts.flat = true derives normals per pixel (geometry with no normal attribute), opts.ambient = the flat ambient floor (0.22; the station hangar uses a brighter one).
//                                 Binds to ps.shared (uLightDir, uUpW, uFogCol, uFogK, uNight) or globalThis.EMGOR_SKY_UNIFORMS.
//     NEAR_FADE                   { value } shared uniform: surfaces (not up-facing) closer than value world units to the camera dissolve to ~15 % coverage. ship-foot.js drives it (1.2 H on foot).
//     glowMat(T, uTime)           the emissive twin (neon, windows, lamps): vertex colour, a slow shimmer, no lighting, no tone map.
//     unlitMat(T, opts)           MeshBasicMaterial pass-through for baked-light / flat things (hub stone, flames, glass): one place to restyle them.
//     LOOK                        tunables: STEPS (toon bands, baked into the shader define), SOFT (band edge softness), RIM, and BEVEL { geoMin, softMin, frac, rMax, m }.
//     bevelRadius(w,h,d)          the default corner radius for a w x h x d box (store metres): min(BEVEL.rMax, BEVEL.frac * smallest side).
//     boxKind(w,h,d,skip,rx,geoMin?)  'bevel' (real rounded geometry) | 'soft' (12-tri box with pillowed vertex normals) | 'flat' (thin boards, nothing to round).
//     bevelTemplate(w,h,d,rad,m)  cached { pos:Float32Array, nrm:Float32Array } of a rounded box centred on the origin. Cache key = dims + radius + segments (ship-geo-cache.js).
//     softNormal(fn, sx, sy, sz)  the pillowed normal for a corner (sx,sy,sz = +-1) of a flat face normal fn.
//     rboxEmit(hx,hy,hz,rad,m,emit)   the single rounded-box generator everyone shares (ship-world rbox, ship-village boxG, ship-hub arch).
import { createGeoCache } from './ship-geo-cache.js';

export const DUSK = { ramp: [0x1E1536, 0x4B3A7C, 0x8A74C4, 0xB9A5E3, 0xE6DCF7], accent: 0xFFC38A };
export const LOOK = {
  STEPS: 4, SOFT: 0.07, RIM: 0.22,
  BEVEL: { geoMin: 0.2, softMin: 0.12, frac: 0.3, rMax: 0.16, m: 1, soft: 0.55 },
};

const _a = { r: 0, g: 0, b: 0 };
export function mixHex(a, b, t) {
  const ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  return (Math.round(ar + (br - ar) * t) << 16) | (Math.round(ag + (bg - ag) * t) << 8) | Math.round(ab + (bb - ab) * t);
}
export function palette(ps) {
  const p = ps && ps.palette, r = p && p.ramp && p.ramp.length >= 5 ? p.ramp : DUSK.ramp;
  return { deep: r[0], lo: r[1], mid: r[2], hi: r[3], sky: r[4], accent: p && p.accent != null ? p.accent : DUSK.accent };
}
function tonesOf(P) {
  return {
    deep: P.deep, lo: P.lo, mid: P.mid, hi: P.hi, sky: P.sky,
    wall: mixHex(P.sky, 0xFFFFFF, 0.55), trim: P.hi, dark: P.deep, floor: mixHex(P.hi, P.mid, 0.5), shelf: P.sky, shelfD: P.lo, chrome: mixHex(P.hi, P.sky, 0.4),
    accent: P.accent, glow: mixHex(P.accent, 0xFFFFFF, 0.45), stock: [P.accent, P.hi, P.mid, P.sky],
  };
}
export const TONES = tonesOf(palette(null));
export function tone(role, ps) { const t = tonesOf(palette(ps)); return t[role]; }

// ── lit material: the planet light rig (ship-planet LIGHT_GLSL: hemisphere fill + wrapped key + cool moon key + fog) with a soft toon ramp on the key ──
const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV; varying vec3 vNw;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec4 lp = vec4(position, 1.0);
  vec3 nl = normal;
  #ifdef USE_INSTANCING
    lp = instanceMatrix * lp;
    nl = mat3(instanceMatrix) * nl;
  #endif
  vNw = mat3(modelMatrix) * nl;
  #ifdef USE_INSTANCING_COLOR
    vC *= instanceColor;
  #endif
  vec4 mv = modelViewMatrix * lp;
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG_SOFT = /* glsl */`
uniform vec3 uLight; uniform vec3 uUpW; uniform vec3 uSky; uniform float uFogK; uniform float uNightF; uniform float uRim; uniform float uSoft; uniform float uAmb; uniform float uNear;
varying vec3 vC; varying vec3 vV; varying vec3 vNw;
#include <common>
#include <logdepthbuf_pars_fragment>
float toon(float x){ float s = 0.0; for (int i = 1; i < STEPS; i++){ float e = float(i) / float(STEPS); s += smoothstep(e - uSoft, e + uSoft, x); } return s / float(STEPS - 1); }
void main(){
  #include <logdepthbuf_fragment>
  vec3 Lw = normalize(uLight), Uw = normalize(uUpW);
  #ifdef FLAT_N
    vec3 Nv = normalize(cross(dFdx(vV), dFdy(vV)));
    vec3 N = Nv * mat3(viewMatrix);
  #else
    vec3 N = normalize(vNw);
    if (dot(N, N) < 0.5) N = normalize(cross(dFdx(vV), dFdy(vV)) * mat3(viewMatrix));
    vec3 Nv = normalize(mat3(viewMatrix) * N);
  #endif
  // camera near-fade (ship-foot.js sets NEAR_FADE): a wall / ceiling / awning within uNear (1.2 H) of the lens dissolves to ~15 % coverage (screen-door dither, no sorting). Up-facing surfaces (floors, plate tops) never fade.
  float nd = length(vV);
  if (uNear > 0.0 && nd < uNear && dot(N, Uw) < 0.75) {
    float cov = mix(0.15, 1.0, smoothstep(0.55 * uNear, uNear, nd));
    if (fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) > cov) discard;
  }
  float h = dot(N, Uw) * 0.5 + 0.5;
  vec3 fill = mix(vC * vec3(0.2, 0.13, 0.3), uSky * 0.8 + vec3(0.04, 0.02, 0.09), h);
  float w = clamp((dot(N, Lw) + 0.3) / 1.3, 0.0, 1.0);
  float wm = clamp((dot(N, -Lw) + 0.3) / 1.3, 0.0, 1.0);
  vec3 key = vec3(1.0, 0.9, 0.8) * toon(w) * (1.0 - uNightF) + vec3(0.5, 0.62, 1.0) * toon(wm) * 0.4 * uNightF;
  vec3 col = vC * (fill * 0.6 + key * 0.5 + uAmb);
  col += pow(1.0 - abs(dot(Nv, normalize(-vV))), 3.0) * uRim * vec3(0.5, 0.3, 0.85) * (0.4 + 0.6 * h);
  float f = min(1.0 - exp(-pow(length(vV) * uFogK, 2.0)), 0.985);
  col = mix(col, uSky, f);
  gl_FragColor = vec4(col, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const FRAG_GLOW = /* glsl */`
uniform float uTime;
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  gl_FragColor = vec4(vC * (0.94 + 0.08 * sin(uTime * 3.0 + vC.g * 9.0 + vV.x * 0.02)), 1.0);
  #include <colorspace_fragment>
}`;
export const NEAR_FADE = { value: 0 };      // world units: fade radius around the camera for every softMat surface (0 = off). ship-foot.js sets 1.2 H each foot frame.
export function softMat(T, ps, opts) {
  opts = opts || {};
  const m = new T.ShaderMaterial({
    vertexShader: VERT, fragmentShader: FRAG_SOFT, vertexColors: true, side: T.DoubleSide, defines: Object.assign({ STEPS: LOOK.STEPS }, opts.flat ? { FLAT_N: 1 } : {}),
    uniforms: { uNear: NEAR_FADE, uLight: { value: new T.Vector3(-0.62, 0.52, 0.4).normalize() }, uUpW: { value: new T.Vector3(0, 1, 0) }, uSky: { value: new T.Color(0.36, 0.3, 0.5) }, uFogK: { value: 0 }, uNightF: { value: 0 }, uRim: { value: LOOK.RIM }, uSoft: { value: LOOK.SOFT }, uAmb: { value: opts.ambient != null ? opts.ambient : 0.22 } },
  });
  const S = (ps && ps.shared) || (typeof globalThis !== 'undefined' && globalThis.EMGOR_SKY_UNIFORMS) || null;
  if (S) { const u = m.uniforms; u.uLight = S.uLightDir; u.uUpW = S.uUpW; u.uSky = S.uFogCol; u.uFogK = S.uFogK; u.uNightF = S.uNight; }
  return m;
}
export function glowMat(T, uTime) {
  return new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_GLOW, vertexColors: true, side: T.DoubleSide, uniforms: { uTime: uTime || { value: 0 } } });
}
export function unlitMat(T, opts) { return new T.MeshBasicMaterial(opts || {}); }

// ── rounded boxes ──
// rounded box shell centred on the origin (half extents hx,hy,hz, bevel r, m segments per 45 degrees): emit(pa, pb, pc, na, nb, nc) per triangle with smooth outward normals.
export function rboxEmit(hx, hy, hz, rad, m, emit) {
  const r = Math.max(1e-3, Math.min(rad, hx * 0.999, hy * 0.999, hz * 0.999));
  const ang = []; for (let j = 0; j <= m; j++) ang.push(Math.tan(Math.PI / 4 * j / m));
  for (let axis = 0; axis < 3; axis++) for (let sgn = -1; sgn <= 1; sgn += 2) for (let sa2 = -1; sa2 <= 1; sa2 += 2) for (let sb = -1; sb <= 1; sb += 2) {
    const grid = [];
    for (let i = 0; i <= m; i++) { const row = []; for (let j = 0; j <= m; j++) {
      const dv = [0, 0, 0]; dv[axis] = sgn; dv[(axis + 1) % 3] = sa2 * ang[i]; dv[(axis + 2) % 3] = sb * ang[j];
      const Ln = Math.hypot(dv[0], dv[1], dv[2]), nx = dv[0] / Ln, ny = dv[1] / Ln, nz = dv[2] / Ln;
      row.push({ p: [(hx - r) * (nx >= 0 ? 1 : -1) + r * nx, (hy - r) * (ny >= 0 ? 1 : -1) + r * ny, (hz - r) * (nz >= 0 ? 1 : -1) + r * nz], n: [nx, ny, nz] });
    } grid.push(row); }
    for (let i = 0; i < m; i++) for (let j = 0; j < m; j++) {
      const A = grid[i][j], B = grid[i + 1][j], C = grid[i + 1][j + 1], D = grid[i][j + 1];
      emit(A.p, B.p, C.p, A.n, B.n, C.n); emit(A.p, C.p, D.p, A.n, C.n, D.n);
    }
  }
}
export function bevelRadius(w, h, d) { const B = LOOK.BEVEL; return Math.min(B.rMax, B.frac * Math.min(w, h, d)); }
export function boxKind(w, h, d, skip, rx, geoMin) {      // geoMin overrides BEVEL.geoMin (the big old compact store is modelled at 2.3x the scale)
  const mn = Math.min(w, h, d), B = LOOK.BEVEL;
  if (mn < B.softMin) return 'flat';
  if (skip || mn < (geoMin != null ? geoMin : B.geoMin)) return 'soft';
  return 'bevel';
}
const BEVEL_GEO = createGeoCache();
const _k = (v) => Math.round(v * 1000);
export function bevelTemplate(w, h, d, rad, m) {
  const key = 'bevel|' + [w, h, d, rad, m].map(_k).join('|');
  return BEVEL_GEO.acquire(key, () => {
    const pos = [], nrm = [];
    rboxEmit(w / 2, h / 2, d / 2, rad, m, (a, b, c, na, nb, nc) => { pos.push(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]); nrm.push(na[0], na[1], na[2], nb[0], nb[1], nb[2], nc[0], nc[1], nc[2]); });
    return { pos: new Float32Array(pos), nrm: new Float32Array(nrm), n: pos.length / 9 };
  });
}
export function softNormal(fn, sx, sy, sz) {
  const k = LOOK.BEVEL.soft, x = fn[0] + k * sx, y = fn[1] + k * sy, z = fn[2] + k * sz, l = Math.hypot(x, y, z) || 1;
  return [x / l, y / l, z / l];
}
export function bevelCacheStats() { return BEVEL_GEO.stats(); }

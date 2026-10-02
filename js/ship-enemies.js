// ship-enemies.js — alien-hybrid creature ships for NO MANS GOR (Revision 3 + Revision 13 generator).
//
// ── Rev 13 EXPORT CONTRACT (procedural enemies; legacy exports below are unchanged) ──────────────
//   generateEnemy(THREE, seed, tier, role) -> {
//       group,            // THREE.Group, scale 1. Contents are in L units (length = 6 * 1.6^(tier-1)); caller multiplies
//                         // group.scale by L. Nose -Z, up +Y, centred on the spine. 3 merged meshes + eye sprites.
//       length, hitR,     // body length (L units, ~6 at tier 1) and bounding hit radius (L units)
//       eye, eyes[],      // eye = eyes[0] = primary weak point (Sprite; getWorldPosition works); each has userData.r = hit radius (L units)
//       stats: { hp, speed, turn, dmg [, spawnOnDeath] },   // speed in L/s, turn in rad/s, dmg per hit (HP)
//       attacks: [attackId...],   // e.g. 'ram','strafe','acidSpit','acidFan','bolt','snipeShot','lanceCharge','larvaSpawn'
//       tier, role, seed, signature,
//       update(t, dt),    // call every frame: tentacle sway / body flex (uTime), eye pulse
//       dispose()
//   }
//   role: 'interceptor'|'spitter'|'sniper'|'lancer'|'brood' (anything else -> picked from seed).
//   silhouetteSignature(seed, role) -> 'segs-finpairs-spikes'; pickDistinctSeeds(baseSeed, n[, roles]) -> n seeds, pairwise-distinct signatures.
//   ENEMY_ROLES (names), BOSS_TIERS = { mini: 4, giant: 6, titan: 8 }, bossAttacksFor(seed, tier) -> [{id, tele, dmg}] ordered.
//   Titan-only ids: 'gravityWell' (pull player + nearby planets), 'planetRam' (engine.nudgeBody a planet), 'terrainBeam' (carve a scar).
// ─────────────────────────────────────────────────────────────────────────────────────────────────
// Pure geometry + material + name generator. Unit length 1.0 along Z, nose -Z, up +Y.
// Geometry is built once per palette and pooled by ship.js; the shader does the flex.
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const PALS = [
  { hull: 0xC2331A, hull2: 0x8A2410, dark: 0x3A0A08, acc: 0xFF8A2A, eye: 0xFFE070 },   // ember
  { hull: 0x6FC21A, hull2: 0x3F8A12, dark: 0x16300A, acc: 0xD6FF3A, eye: 0xF4FFB0 },   // acid
  { hull: 0x2E48C8, hull2: 0x1E2C8A, dark: 0x080C30, acc: 0x4FE8FF, eye: 0xC8F8FF },   // frost (sniper)
];
// rev 9b roles: same builder, parameterised silhouette. pal indexes PALS; len/wid scale the finished mesh.
export const ROLES = [
  { name: 'interceptor', pal: 0, len: 1.1, wid: 0.78, fins: 2, bulb: false },   // lean dart, swept pair of fins
  { name: 'spitter', pal: 1, len: 0.88, wid: 1.3, fins: 3, bulb: true },        // fat acid sac with a bulb abdomen
  { name: 'sniper', pal: 2, len: 1.4, wid: 0.6, fins: 3, bulb: false },         // long cold needle
];
export function roleEyeZ(i) { return CREATURE_EYE[2] * ROLES[i].len; }
export const CREATURE_EYE = [0, 0.035, -0.36];      // local eye-core position (unit length)
export const CREATURE_EYE_R = 0.11;                 // local eye-core hit radius

const VERT = /* glsl */`
uniform float uTime; uniform float uPh;
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec3 p = position;
  float tail = smoothstep(-0.25, 0.45, p.z);
  p.x += sin(p.z * 10.0 - uTime * 4.0 + uPh) * 0.045 * tail;
  p.y += sin(p.z * 8.0 - uTime * 3.0 + uPh) * 0.03 * tail;
  float fin = abs(p.x);
  p.y += sin(uTime * 3.6 + uPh + fin * 5.0) * fin * 0.30 * smoothstep(0.08, 0.3, fin);
  p.xy *= 1.0 + 0.035 * sin(uTime * 2.4 + uPh + p.z * 6.0);
  vec4 mv = modelViewMatrix * vec4(p, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG = /* glsl */`
uniform float uTime; uniform float uPh; uniform float uHit;
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = max(0.0, dot(n, L)) * 0.7 + 0.3;
  float rim = pow(1.0 - max(0.0, dot(n, normalize(-vV))), 3.0) * 0.35;
  vec3 col = vC * d + rim * vec3(0.9, 0.3, 0.15);
  float em = step(1.15, max(vC.r, max(vC.g, vC.b)));
  col = mix(col, vC * (0.85 + 0.3 * sin(uTime * 5.0 + uPh)), em);
  col += uHit * vec3(1.0, 0.65, 0.5);
  gl_FragColor = vec4(col, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// One material per live creature (13 total, same program): uPh is per-creature so
// the flex does not jitter as the creature moves through world space.
export function makeCreatureMaterial(THREE) {
  return new THREE.ShaderMaterial({
    vertexShader: VERT, fragmentShader: FRAG, vertexColors: true, side: THREE.DoubleSide,
    uniforms: { uTime: { value: 0 }, uPh: { value: 0 }, uHit: { value: 0 } },
  });
}

function buildOne(THREE, pal, boss, role) {
  const parts = [];
  const hullC = new THREE.Color(pal.hull), hull2C = new THREE.Color(pal.hull2), darkC = new THREE.Color(pal.dark);
  const accC = new THREE.Color(pal.acc), eyeC = new THREE.Color(pal.eye);
  const hash = (x, y, z) => {
    const s = Math.sin(Math.round(x * 60) * 12.9898 + Math.round(y * 60) * 78.233 + Math.round(z * 60) * 37.719) * 43758.5453;
    return s - Math.floor(s);
  };
  function finish(g, colorFn) {
    if (g.index) g = g.toNonIndexed();
    g.deleteAttribute('normal'); g.deleteAttribute('uv');
    const p = g.attributes.position, n = p.count, arr = new Float32Array(n * 3), c = new THREE.Color();
    for (let i = 0; i < n; i++) {
      colorFn(c, p.getX(i), p.getY(i), p.getZ(i), i);
      arr[i * 3] = c.r; arr[i * 3 + 1] = c.g; arr[i * 3 + 2] = c.b;
    }
    g.setAttribute('color', new THREE.BufferAttribute(arr, 3));
    parts.push(g);
  }
  // stacked, banded body segments
  function seg(rx, ry, rz, z, y = 0, bands = true) {
    const g = new THREE.SphereGeometry(1, 10, 7);
    g.scale(rx, ry, rz); g.translate(0, y, z);
    finish(g, (c, x, yy, zz) => {
      const band = bands && Math.sin((zz - z) / rz * 5.0) > 0.2;
      c.copy(yy - y < -ry * 0.25 ? darkC : (band ? hull2C : hullC));
      c.multiplyScalar(1 - 0.22 * hash(x, yy, zz));
    });
  }
  function glowOrb(r, x, y, z, col, k) {
    const g = new THREE.SphereGeometry(r, 6, 4); g.translate(x, y, z);
    finish(g, (c) => { c.copy(col).multiplyScalar(k); });
  }
  function cone(r, h, x, y, z, rx, rz, col) {
    const g = new THREE.ConeGeometry(r, h, 5);
    g.translate(0, h / 2, 0);
    g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, 0, rz)));
    g.translate(x, y, z);
    finish(g, (c, px, py, pz) => { c.copy(col).multiplyScalar(0.8 + 0.4 * hash(px, py, pz)); });
  }
  function tri(a, b, c3, ca, cb, cc) {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute([...a, ...b, ...c3], 3));
    const cols = [ca, cb, cc];
    finish(g, (c, x, y, z, i) => { c.copy(cols[i % 3]); });
  }
  // body: head, thorax, abdomen, tail
  seg(0.12, 0.10, 0.15, -0.28);
  seg(0.16, 0.13, 0.17, -0.10);
  seg(0.14, 0.12, 0.17, 0.09);
  seg(0.09, 0.08, 0.14, 0.24);
  seg(0.045, 0.04, 0.10, 0.37);
  if (boss) { seg(0.10, 0.09, 0.12, -0.20, 0.07, false); seg(0.06, 0.06, 0.1, 0.46); }
  if (role && role.bulb) seg(0.2, 0.17, 0.2, 0.16, 0, false);
  // eye core + brow
  glowOrb(0.05, CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2], eyeC, 2.2);
  glowOrb(0.016, -0.075, 0.015, -0.33, accC, 1.9);
  glowOrb(0.016, 0.075, 0.015, -0.33, accC, 1.9);
  // biolum spots down the spine and tail
  for (let i = 0; i < 5; i++) glowOrb(0.014, 0, 0.125 - i * 0.004, -0.12 + i * 0.12, accC, 1.7);
  // dorsal spikes
  const nsp = boss ? 7 : 4;
  for (let i = 0; i < nsp; i++) {
    const z = -0.14 + i * (0.5 / nsp);
    cone(boss ? 0.03 : 0.022, boss ? 0.16 : 0.1, 0, 0.12, z, -0.5, 0, accC);
  }
  // mandibles
  for (const s of [-1, 1]) {
    cone(0.018, 0.17, s * 0.06, -0.04, -0.40, -Math.PI / 2 - 0.15, s * 0.45, darkC);
    cone(0.012, 0.1, s * 0.1, -0.02, -0.36, -Math.PI / 2 + 0.2, s * 0.9, accC);
  }
  // horns (boss)
  if (boss) for (const s of [-1, 1]) cone(0.026, 0.2, s * 0.08, 0.1, -0.30, -0.7, s * 0.5, accC);
  // fin-wings: veined membranes, root dark to tip bright
  for (const s of [-1, 1]) {
    tri([s * 0.10, 0.0, -0.14], [s * 0.54, 0.05, 0.14], [s * 0.09, 0.0, 0.18], hull2C, accC, darkC);
    tri([s * 0.10, 0.02, 0.08], [s * 0.34, -0.08, 0.38], [s * 0.07, 0.02, 0.30], hullC, accC, darkC);
    if (!role || role.fins >= 3) tri([s * 0.12, 0.0, -0.10], [s * 0.40, 0.14, -0.02], [s * 0.30, 0.04, 0.10], hull2C, accC, hullC);
    if (boss) tri([s * 0.10, 0.05, -0.22], [s * 0.46, 0.2, -0.16], [s * 0.18, 0.05, -0.04], hullC, accC, hull2C);
  }
  // tail fin
  tri([0, 0.04, 0.30], [0, 0.17, 0.5], [0, 0.03, 0.5], hull2C, accC, darkC);
  const g = mergeGeometries(parts);
  if (role) g.scale(role.wid, role.wid, role.len);
  g.computeBoundingSphere();
  return g;
}

// returns { geoms: [ember, acid], boss: [ember, acid], roles: [interceptor, spitter, sniper] }
export function buildCreatureGeoms(THREE) {
  return {
    geoms: PALS.slice(0, 2).map(p => buildOne(THREE, p, false)),
    boss: PALS.slice(0, 2).map(p => buildOne(THREE, p, true)),
    roles: ROLES.map(r => buildOne(THREE, PALS[r.pal], false, r)),
  };
}

// seeded name generator: wave number -> stable boss name
function mulberry(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const SYL = ['XAL', 'MOR', 'ZETH', 'VOR', 'KRUN', 'ILU', 'THAX', 'NEM', 'OBB', 'SKAR', 'YUL', 'DRAK', 'ESS', 'QOR', 'VHEL', 'UMBR', 'GOR', 'NYX'];
const TITLE = ['THE HOLLOW', 'THE UNBLINKING', 'DEVOURER', 'OF THE DEEP DARK', 'THE PATIENT', 'HIVE-MOTHER', 'THE LONG HUNGER', 'EMBER-LORD', 'THE SLOW TIDE'];
export function bossName(wave) {
  const r = mulberry(wave * 2654435761 + 12345), n = 2 + (r() < 0.5 ? 1 : 0);
  let s = '';
  for (let i = 0; i < n; i++) s += SYL[Math.floor(r() * SYL.length)] + (i < n - 1 && r() < 0.3 ? "'" : '');
  return s + ' ' + TITLE[Math.floor(r() * TITLE.length)];
}

// rev 12: every boss owns ONE signature attack, seeded from its name (stable per name).
export const BOSS_ATTACKS = ['BEAM SWEEP', 'ORB RING', 'RAM CHARGE', 'MINE FIELD', 'GRAVITY PULL'];
export function bossAttack(name) {
  let h = 2166136261;
  for (let i = 0; i < name.length; i++) { h ^= name.charCodeAt(i); h = Math.imul(h, 16777619); }
  return (h >>> 0) % BOSS_ATTACKS.length;
}


// ═════════════════════════ Revision 13: procedural generator ═════════════════════════
export const ENEMY_ROLES = ['interceptor', 'spitter', 'sniper', 'lancer', 'brood'];
export const BOSS_TIERS = { mini: 4, giant: 6, titan: 8 };

// role -> palette family hue (ember/acid/abyss-blue/amber/violet), stat multipliers, attacks
const FAMILY = {
  interceptor: { h: 0.015, eyeH: 0.0,  hp: 0.8, sp: 1.25, tn: 1.2, dm: 1.0, atk: ['ram', 'strafe'] },
  spitter:     { h: 0.25,  eyeH: 0.22, hp: 1.0, sp: 0.9,  tn: 0.9, dm: 1.0, atk: ['acidSpit', 'acidFan'] },
  sniper:      { h: 0.60,  eyeH: 0.54, hp: 0.7, sp: 0.8,  tn: 0.8, dm: 1.5, atk: ['bolt', 'snipeShot'] },
  lancer:      { h: 0.105, eyeH: 0.12, hp: 0.9, sp: 1.5,  tn: 0.6, dm: 1.6, atk: ['lanceCharge', 'strafe'] },
  brood:       { h: 0.77,  eyeH: 0.82, hp: 1.4, sp: 0.7,  tn: 0.7, dm: 0.8, atk: ['larvaSpawn', 'acidSpit'] },
};
const rn = (r, a, b) => a + r() * (b - a);
const ri = (r, a, b) => a + Math.floor(r() * (b - a + 1));

// all structural randomness lives here so signatures never need THREE
function params(seed, role) {
  const r = mulberry((seed | 0) * 2654435761 + 9176);
  if (!FAMILY[role]) role = ENEMY_ROLES[Math.floor(r() * ENEMY_ROLES.length)];
  const P = { role, r };
  P.segs = ri(r, 3, 9); P.fins = ri(r, 0, 4); P.spikes = ri(r, 0, 6);
  if (role === 'lancer') { P.segs = Math.max(P.segs, 6); P.spikes = Math.max(P.spikes, 2); }
  if (role === 'brood') P.segs = Math.min(P.segs, 4);
  return P;
}
export function silhouetteSignature(seed, role) { const P = params(seed, role); return P.segs + '-' + P.fins + '-' + P.spikes; }
export function pickDistinctSeeds(baseSeed, n, roles) {
  const out = [], seen = new Set();
  for (let i = 0, k = 0; out.length < n && k < 20000; k++) {
    const sd = ((baseSeed | 0) + k * 7919 + 1) | 0, sig = silhouetteSignature(sd, roles && roles[out.length]);
    if (seen.has(sig)) continue;
    seen.add(sig); out.push(sd); i++;
  }
  return out;
}

let _glowTex = null;
function glowTexture(THREE) {
  if (_glowTex) return _glowTex;
  const c = document.createElement('canvas'); c.width = c.height = 64;
  const x = c.getContext('2d'), g = x.createRadialGradient(32, 32, 0, 32, 32, 32);
  g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.25, 'rgba(255,255,255,0.55)'); g.addColorStop(1, 'rgba(255,255,255,0)');
  x.fillStyle = g; x.fillRect(0, 0, 64, 64);
  _glowTex = new THREE.CanvasTexture(c); _glowTex.colorSpace = THREE.SRGBColorSpace;
  return _glowTex;
}

// flex shader v2: aFx = (tentacle sway weight, phase, fin-flap flag). Same fragment shader as the legacy material.
const VERT2 = /* glsl */`
uniform float uTime; uniform float uPh;
attribute vec3 aFx;
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec3 p = position;
  float tail = smoothstep(-0.25, 0.45, p.z);
  p.x += sin(p.z * 9.0 - uTime * 3.2 + uPh) * 0.03 * tail;
  p.y += sin(p.z * 7.0 - uTime * 2.4 + uPh) * 0.022 * tail;
  float w = aFx.x;
  p.x += sin(uTime * 2.3 + aFx.y + w * 4.5) * 0.11 * w * w;
  p.y += cos(uTime * 1.8 + aFx.y * 1.3 + w * 3.5) * 0.08 * w * w;
  p.z += sin(uTime * 2.0 + aFx.y + w * 3.0) * 0.05 * w * w;
  float fin = abs(p.x);
  p.y += aFx.z * sin(uTime * 3.0 + uPh + fin * 6.0) * fin * 0.22 * smoothstep(0.05, 0.25, fin);
  p.xy *= 1.0 + 0.03 * sin(uTime * 2.2 + uPh + p.z * 6.0);
  vec4 mv = modelViewMatrix * vec4(p, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
function makeEnemyMaterial(THREE) {
  return new THREE.ShaderMaterial({
    vertexShader: VERT2, fragmentShader: FRAG, vertexColors: true, side: THREE.DoubleSide,
    uniforms: { uTime: { value: 0 }, uPh: { value: 0 }, uHit: { value: 0 } },
  });
}

export function generateEnemy(THREE, seed, tier, role) {
  tier = Math.max(1, tier | 0 || 1);
  const P = params(seed, role), r = P.r; role = P.role;
  const F = FAMILY[role];
  const length = 6 * Math.pow(1.6, tier - 1);
  // palette
  const hue = (F.h + rn(r, -0.03, 0.03) + 1) % 1;
  const hullC = new THREE.Color().setHSL(hue, 0.7, 0.36), hull2C = new THREE.Color().setHSL((hue + 0.02) % 1, 0.75, 0.24);
  const darkC = new THREE.Color().setHSL(hue, 0.7, 0.07), accC = new THREE.Color().setHSL((hue + 0.03) % 1, 1, 0.56);
  const eyeC = new THREE.Color().setHSL(F.eyeH, 1, 0.72);
  const hash = (x, y, z) => { const q = Math.sin(Math.round(x * 60) * 12.9898 + Math.round(y * 60) * 78.233 + Math.round(z * 60) * 37.719) * 43758.5453; return q - Math.floor(q); };
  const body = [], fx = [], em = [];
  function finish(list, g, colorFn, fxFn) {
    if (g.index) g = g.toNonIndexed();
    g.deleteAttribute('normal'); g.deleteAttribute('uv');
    const p = g.attributes.position, n = p.count, col = new Float32Array(n * 3), f = new Float32Array(n * 3), c = new THREE.Color();
    for (let i = 0; i < n; i++) {
      colorFn(c, p.getX(i), p.getY(i), p.getZ(i), i);
      col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
      if (fxFn) { const q = fxFn(p.getX(i), p.getY(i), p.getZ(i), i); f[i * 3] = q[0]; f[i * 3 + 1] = q[1]; f[i * 3 + 2] = q[2]; }
    }
    g.setAttribute('color', new THREE.BufferAttribute(col, 3));
    g.setAttribute('aFx', new THREE.BufferAttribute(f, 3));
    list.push(g);
  }
  // ── spine: 3-9 tapered ellipsoid segments along a seeded curve ──
  const n = P.segs;
  let R = rn(r, 0.085, 0.15);
  if (role === 'lancer') R = rn(r, 0.035, 0.05);
  if (role === 'brood') R = rn(r, 0.18, 0.23);
  if (role === 'interceptor') R *= 0.85;
  const curveA = role === 'lancer' ? 0.01 : rn(r, 0.02, 0.09), curveP = rn(r, 0, 6.28), curveK = rn(r, 0.8, 2.2);
  const zNose = -0.46, zTail = 0.46, spacing = (zTail - zNose) / (n - 1);
  const cy = (t) => Math.sin(t * Math.PI * curveK + curveP) * curveA;
  const cx = (t) => Math.cos(t * Math.PI * curveK * 0.7 + curveP) * curveA * 0.6;
  const segs = [];
  for (let i = 0; i < n; i++) {
    const t = i / (n - 1), prof = Math.sin(Math.PI * (0.12 + 0.78 * Math.pow(t, 0.9)));
    let rad = R * (0.35 + 0.65 * prof) * (i === 0 ? 1.12 : 1);
    if (role === 'brood') rad *= 0.8 + 0.5 * Math.sin(Math.PI * t) ;
    const rz = Math.max(spacing * (role === 'lancer' ? 0.95 : 0.78), rad * 1.05);
    segs.push({ t, x: cx(t), y: cy(t), z: zNose + i * spacing, rx: rad * (role === 'brood' ? 1.1 : 1), ry: rad * (role === 'sniper' ? 0.85 : 1), rz });
  }
  segs.forEach((s, i) => {
    const g = new THREE.SphereGeometry(1, 12, 8); g.scale(s.rx, s.ry, s.rz); g.translate(s.x, s.y, s.z);
    const bandF = 3 + (seed & 3);
    finish(body, g, (c, x, y, z) => {
      const band = Math.sin((z - s.z) / s.rz * bandF) > 0.25;
      c.copy(y - s.y < -s.ry * 0.3 ? darkC : (band ? hull2C : hullC)).multiplyScalar(1 - 0.25 * hash(x, y, z));
    });
  });
  const head = segs[0];
  // ── eyes: 1-3 real, primary front-top of head ──
  const nEyes = role === 'interceptor' ? 1 : role === 'brood' ? 3 : ri(r, 1, 3);
  const eyeR = Math.max(0.03, head.rx * 0.33) * (role === 'brood' ? 0.8 : 1);
  const eyePos = [[head.x, head.y + head.ry * 0.38, head.z - head.rz * 0.8]];
  if (nEyes > 1) eyePos.push([head.x - head.rx * 0.62, head.y + head.ry * 0.2, head.z - head.rz * 0.62]);
  if (nEyes > 2) eyePos.push([head.x + head.rx * 0.62, head.y + head.ry * 0.2, head.z - head.rz * 0.62]);
  const glowK = 2.3;
  eyePos.forEach(([x, y, z], i) => {
    const g = new THREE.SphereGeometry(eyeR * (i ? 0.75 : 1), 8, 6); g.translate(x, y, z);
    finish(em, g, (c) => c.copy(eyeC).multiplyScalar(glowK));
  });
  if (role === 'brood') for (let i = 0; i < 9; i++) {   // clustered small eyes
    const a = rn(r, 0, 6.28), b = rn(r, -0.3, 1.0);
    const g = new THREE.SphereGeometry(eyeR * 0.32, 5, 4);
    g.translate(head.x + Math.cos(a) * head.rx * 0.8, head.y + Math.sin(a) * head.ry * 0.8 + 0.01, head.z - head.rz * (0.45 + 0.4 * b));
    finish(em, g, (c) => c.copy(eyeC).multiplyScalar(2.0));
  }
  // biolum spots along the spine
  segs.forEach((s, i) => {
    if (i === 0 || (i + (seed & 1)) % 2) return;
    const g = new THREE.SphereGeometry(Math.max(0.012, s.rx * 0.16), 5, 4); g.translate(s.x, s.y + s.ry * 0.97, s.z);
    finish(em, g, (c) => c.copy(accC).multiplyScalar(1.9));
  });
  // ── fin-wing pairs: thin extruded fins with seeded sweep/dihedral ──
  for (let k = 0; k < P.fins; k++) {
    const si = Math.min(n - 1, Math.max(0, Math.round((0.2 + (P.fins === 1 ? 0.25 : k / (P.fins - 1) * 0.55)) * (n - 1))));
    const sg = segs[si];
    const span = rn(r, 0.2, 0.5) * (role === 'sniper' ? 0.8 : 1) * (1 - k * 0.12), chord = rn(r, 0.12, 0.3), sweep = rn(r, 0.05, 0.28), dih = rn(r, -0.35, 0.5);
    const sh = new THREE.Shape();
    sh.moveTo(0, -chord * 0.4); sh.quadraticCurveTo(span * 0.55, -chord * 0.35 + sweep * 0.2, span, sweep);
    sh.quadraticCurveTo(span * 0.65, chord * 0.2 + sweep * 0.5, 0, chord * 0.6); sh.lineTo(0, -chord * 0.4);
    for (const side of [-1, 1]) {
      const g = new THREE.ExtrudeGeometry(sh, { depth: 0.012, bevelEnabled: false, curveSegments: 6 });
      g.rotateX(Math.PI / 2); g.rotateZ(dih);
      g.scale(side, 1, 1); g.translate(side * sg.rx * 0.7 + sg.x, sg.y, sg.z);
      finish(fx, g, (c, x, y, z) => { const q = Math.min(1, Math.abs(x - sg.x) / span); c.copy(darkC).lerp(q > 0.85 ? accC : hull2C, q).multiplyScalar(0.85 + 0.3 * hash(x, y, z)); },
        (x, y, z) => [0, 0, 1]);
    }
  }
  // ── spikes + tentacles (tapered, tip-glowing) ──
  const nTent = P.spikes ? Math.floor(P.spikes * (role === 'lancer' ? 0 : role === 'brood' ? 0.7 : rn(r, 0.2, 0.7))) : 0;
  const up = new THREE.Vector3(0, 1, 0);
  function limb(anchor, dir, len, rBase, tentacle, tint) {
    const g = new THREE.CylinderGeometry(tentacle ? rBase * 0.12 : 0.0005, rBase, len, tentacle ? 6 : 5, tentacle ? 10 : 1, true);
    g.translate(0, len / 2, 0);
    const m = new THREE.Matrix4().makeRotationFromQuaternion(new THREE.Quaternion().setFromUnitVectors(up, dir.clone().normalize()));
    g.applyMatrix4(m); g.translate(anchor.x, anchor.y, anchor.z);
    const ph = rn(r, 0, 6.28);
    const d = dir.clone().normalize();
    finish(fx, g, (c, x, y, z) => {
      const w = Math.max(0, Math.min(1, ((x - anchor.x) * d.x + (y - anchor.y) * d.y + (z - anchor.z) * d.z) / len));
      c.copy(darkC).lerp(tint, Math.pow(w, 1.4));
      if (tentacle && w > 0.8) c.copy(accC).multiplyScalar(1.9);        // glowing tentacle tips
    }, (x, y, z) => {
      if (!tentacle) return [0, 0, 0];
      const w = Math.max(0, Math.min(1, ((x - anchor.x) * d.x + (y - anchor.y) * d.y + (z - anchor.z) * d.z) / len));
      return [w, ph, 0];
    });
  }
  const tail = segs[n - 1];
  let sp = 0;
  if (role === 'lancer') for (const s of [-1, 1]) {   // 2 long forward lances
    limb(new THREE.Vector3(head.x + s * head.rx * 0.45, head.y, head.z - head.rz * 0.6), new THREE.Vector3(s * 0.05, 0.0, -1), 0.4 + R, 0.03, false, accC); sp++;
  }
  for (let i = 0; i < nTent; i++, sp++) {
    const a = (i / Math.max(1, nTent)) * 6.28 + rn(r, 0, 1), rear = i % 2 === 0;
    const sg = rear ? tail : segs[Math.max(1, n - 2 - (i % Math.max(1, n - 2)))];
    limb(new THREE.Vector3(sg.x + Math.cos(a) * sg.rx * 0.6, sg.y + Math.sin(a) * sg.ry * 0.6, sg.z),
      new THREE.Vector3(Math.cos(a) * 0.5, Math.sin(a) * 0.4, rear ? 1 : 0.6), rn(r, 0.28, 0.55), Math.max(0.012, R * 0.2), true, hullC);
  }
  for (; sp < P.spikes; sp++) {
    const sg = segs[ri(r, 0, n - 1)], a = rn(r, -1.2, 1.2) + (sp % 2 ? 0 : Math.PI);
    const dirv = new THREE.Vector3(Math.sin(a) * 0.7, Math.abs(Math.cos(a)) * 0.9 + 0.3, rn(r, 0.1, 0.9));
    limb(new THREE.Vector3(sg.x + Math.sin(a) * sg.rx * 0.8, sg.y + sg.ry * 0.7, sg.z), dirv, rn(r, 0.1, 0.24), Math.max(0.014, sg.rx * 0.22), false, accC);
  }
  // ── assemble: <= 3 merged meshes + eye sprites, under a length-scaled root ──
  const group = new THREE.Group(), root = new THREE.Group();
  root.scale.setScalar(length); group.add(root);
  const mat = makeEnemyMaterial(THREE); mat.uniforms.uPh.value = rn(r, 0, 6.28);
  const geoms = [];
  let tris = 0;
  for (const list of [body, fx, em]) {
    if (!list.length) continue;
    const g = mergeGeometries(list); g.computeBoundingSphere(); g.boundingSphere.radius *= 1.25;
    const m = new THREE.Mesh(g, mat); root.add(m); geoms.push(g); tris += g.attributes.position.count / 3;
    list.forEach((q) => q.dispose());
  }
  const smat = new THREE.SpriteMaterial({ map: glowTexture(THREE), color: eyeC, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true });
  const eyes = eyePos.map(([x, y, z], i) => {
    const sp2 = new THREE.Sprite(smat); sp2.position.set(x, y, z);
    const sz = eyeR * (i ? 0.75 : 1) * 7; sp2.scale.set(sz, sz, 1); sp2.userData.base = sz;
    sp2.userData.r = eyeR * 1.6 * length;   // hit radius, L units
    root.add(sp2); return sp2;
  });
  // ── stats ──
  const stats = {
    hp: Math.round(40 * Math.pow(1.9, tier - 1) * F.hp),
    speed: +(14 * Math.pow(0.9, tier - 1) * F.sp).toFixed(2),
    turn: +(1.6 * Math.pow(0.85, tier - 1) * F.tn).toFixed(2),
    dmg: Math.round(8 * Math.pow(1.4, tier - 1) * F.dm),
  };
  if (role === 'brood') stats.spawnOnDeath = 3;
  const attacks = F.atk.slice(0, tier >= 3 ? 2 : 1);
  const ph0 = mat.uniforms.uPh.value;
  return {
    group, length, hitR: length * 0.55, eye: eyes[0], eyes, stats, attacks, tier, role, seed,
    signature: P.segs + '-' + P.fins + '-' + P.spikes, tris,
    update(t, dt) {
      mat.uniforms.uTime.value = t;
      for (let i = 0; i < eyes.length; i++) { const k = 1 + 0.18 * Math.sin(t * 4.0 + ph0 + i * 1.7); eyes[i].scale.set(eyes[i].userData.base * k, eyes[i].userData.base * k, 1); }
    },
    dispose() { geoms.forEach((g) => g.dispose()); mat.dispose(); smat.dispose(); },
  };
}

// ── boss attack sets: ordered, telegraph seconds + damage grow with tier ──
const BASE_ATK = {
  beamSweep: { tele: 1.4, dmg: 14 }, orbRing: { tele: 1.2, dmg: 10 }, ramCharge: { tele: 1.6, dmg: 22 },
  mineField: { tele: 1.5, dmg: 12 }, gravityPull: { tele: 1.8, dmg: 6 },
  gravityWell: { tele: 3.5, dmg: 10 }, planetRam: { tele: 4.0, dmg: 40 }, terrainBeam: { tele: 3.0, dmg: 28 },
};
export function bossAttacksFor(seed, tier) {
  const r = mulberry((seed | 0) * 40503 + tier * 977 + 7);
  const pool = ['beamSweep', 'orbRing', 'ramCharge', 'mineField', 'gravityPull'];
  for (let i = pool.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); const q = pool[i]; pool[i] = pool[j]; pool[j] = q; }
  const ids = tier >= BOSS_TIERS.titan ? ['gravityWell', 'planetRam', 'terrainBeam', pool[0], pool[1]]
    : tier >= BOSS_TIERS.giant ? [pool[0], pool[1], pool[2], pool[3]]
    : tier >= BOSS_TIERS.mini ? [pool[0], pool[1]] : [pool[0]];
  const g = Math.pow(1.3, Math.max(0, tier - 4));
  return ids.map((id) => ({ id, tele: +(BASE_ATK[id].tele * (0.9 + 0.08 * tier)).toFixed(2), dmg: Math.round(BASE_ATK[id].dmg * g) }));
}

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
import { PARTS, compose, makeMaterials, mergeLit, mulberry as pmul } from './ship-parts.js';

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

// ── Rev 18: creatures are composed from js/ship-parts.js (spine + fins + spikes + tentacles + claws + shells + eyes), one lit mesh on a
//    per-creature instance of the shared flex material (same GL program as every boss). `lo` = lazily built low-LOD twin from the parts' lo variants.
function enemyBuild(THREE, seed, tier, role, lod) {
  const P = params(seed, role), r = P.r; role = P.role;
  const F = FAMILY[role];
  // palette
  const hue = (F.h + rn(r, -0.03, 0.03) + 1) % 1;
  const hullC = new THREE.Color().setHSL(hue, 0.7, 0.36), hull2C = new THREE.Color().setHSL((hue + 0.02) % 1, 0.75, 0.24);
  const darkC = new THREE.Color().setHSL(hue, 0.7, 0.07), accC = new THREE.Color().setHSL((hue + 0.03) % 1, 1, 0.56);
  const eyeC = new THREE.Color().setHSL(F.eyeH, 1, 0.72), metalC = new THREE.Color().setHSL(hue, 0.25, 0.42);
  const n = P.segs;
  let R = rn(r, 0.085, 0.15);
  if (role === 'lancer') R = rn(r, 0.035, 0.05);
  if (role === 'brood') R = rn(r, 0.18, 0.23);
  if (role === 'interceptor') R *= 0.85;
  const nEyes = role === 'interceptor' ? 1 : role === 'brood' ? 3 : ri(r, 1, 3);
  const eyeR = Math.max(0.03, R * 0.36) * (role === 'brood' ? 0.8 : 1);
  const kids = [];
  const nodeEye = (id, at, k, mirror) => kids.push({ part: 'eye', id, params: { r: eyeR * k, glowCol: eyeC }, at, mirror: mirror ? 'x' : undefined });
  nodeEye('eyeP', 'eyeC', 1.0, false);
  if (nEyes === 2) nodeEye('eyeS', 'eyeA', 0.75, false);
  if (nEyes === 3) nodeEye('eyeS', 'eyeA', 0.75, true);
  if (role === 'brood') for (let i = 0; i < 4; i++) kids.push({ part: 'eye', id: 'eyeB' + i, params: { r: eyeR * 0.36, glowCol: eyeC }, at: 'eyeC', offset: [rn(r, -1, 1) * R * 0.5, -R * 0.1, rn(r, -0.3, 0.9) * R * 0.5], rot: [rn(r, -0.4, 0.4), 0, rn(r, -0.5, 0.5)] });
  // mandibles
  kids.push({ part: 'spike', at: 'jawA', mirror: 'x', params: { len: rn(r, 0.07, 0.14), r: Math.max(0.014, R * 0.16), bend: 0.3, tip: accC } });
  // fin-wing pairs
  const flankIdx = (k) => Math.min(n - 1, Math.max(0, Math.round((0.2 + (P.fins === 1 ? 0.25 : k / (P.fins - 1) * 0.55)) * (n - 1))));
  for (let k = 0; k < P.fins; k++) {
    const span = rn(r, 0.2, 0.5) * (role === 'sniper' ? 0.8 : 1) * (1 - k * 0.12), chord = rn(r, 0.12, 0.3), sweep = rn(r, 0.05, 0.28) / span * 1.6, dih = rn(r, -0.35, 0.5);
    kids.push({ part: 'fin', at: 'flank' + flankIdx(k), mirror: 'x', rot: [0, 0, -dih], params: { len: span, w: chord, sweep: Math.min(1.1, sweep), thick: 0.016, taper: rn(r, 0.25, 0.5), flap: true, glow: k === 0, tip: accC } });
  }
  // spikes + tentacles
  const nTent = P.spikes ? Math.floor(P.spikes * (role === 'lancer' ? 0 : role === 'brood' ? 0.7 : rn(r, 0.2, 0.7))) : 0;
  let sp = 0;
  if (role === 'lancer') for (let s = 0; s < 1; s++) { kids.push({ part: 'spike', at: 'head', mirror: 'x', offset: [R * 0.45, 0, R * 0.2], params: { len: 0.4 + R, r: 0.03, bend: 0, tip: accC } }); sp++; }
  for (let i = 0; i < nTent; i++, sp++) {
    const rear = i % 2 === 0, segI = rear ? n - 1 : Math.max(1, n - 2 - (i % Math.max(1, n - 2)));
    const a = (i / Math.max(1, nTent)) * 6.28 + rn(r, 0, 1);
    kids.push({ part: 'tentacle', at: rear ? 'tail' : 'belly' + segI, rot: rear ? [0, 0, 0] : [0.5 * Math.sin(a), a, 0], params: { len: rn(r, 0.28, 0.55), r: Math.max(0.014, R * 0.22), tip: accC } });
  }
  for (; sp < P.spikes; sp++) kids.push({ part: 'spike', at: 'dorsal' + ri(r, 0, n - 1), rot: [rn(r, 0.1, 0.9), 0, rn(r, -0.6, 0.6)], params: { len: rn(r, 0.08, 0.2), r: Math.max(0.014, R * 0.2), bend: 0.25, tip: accC } });
  // role identity pieces
  if (role === 'spitter') kids.push({ part: 'pod', at: 'dorsal' + (n - 2), rot: [Math.PI / 2, 0, 0], offset: [0, 0, -R * 0.2], params: { len: R * 2.2, r: R * 0.9 } });
  if (role === 'brood') { kids.push({ part: 'shell', at: 'dorsal' + Math.max(1, Math.floor(n / 2)), offset: [0, -R * 0.35, 0], params: { rx: R * 0.9, ry: R * 0.55, rz: Math.min(0.3, R * 1.6) } }); kids.push({ part: 'claw', at: 'flank0', mirror: 'x', params: { r: 0.045, open: 0.5 } }); }
  if (role === 'sniper') kids.push({ part: 'barrel', at: 'dorsal' + Math.min(n - 1, 2), rot: [-Math.PI / 2, 0, 0], params: { len: 0.2, r: 0.012, glowCol: eyeC } });
  if (role === 'interceptor' && n > 3) kids.push({ part: 'plate', at: 'dorsal' + Math.floor(n / 2), params: { w: R * 0.9, d: 0.18, th: 0.02 } });
  const curveA = role === 'lancer' ? 0.01 : rn(r, 0.02, 0.09);
  const recipe = {
    part: 'spine', palette: { base: hullC, panel: hull2C, accent: accC, glow: eyeC, dark: darkC, metal: metalC }, lod,
    params: { segs: n, len: 0.96, r: R * 1.15, curve: curveA, wid: role === 'brood' ? 1.15 : 1, hgt: role === 'sniper' ? 0.85 : 1, bodyFx: 0.55, stretch: role === 'lancer' ? 0.95 : 0.78, glowCol: eyeC },
    paint: { lines: 0, stripes: 0, grime: 0.22, base: hullC, accent: accC, panel: hull2C },
    children: kids,
  };
  return { recipe, P, F, role, eyeC, eyeR, nEyes, phase: rn(r, 0, 6.28) };
}

export function generateEnemy(THREE, seed, tier, role) {
  tier = Math.max(1, tier | 0 || 1);
  const B = enemyBuild(THREE, seed, tier, role, 'hi'), P = B.P, F = B.F; role = B.role;
  const length = 6 * Math.pow(1.6, tier - 1);
  const res = compose(THREE, B.recipe, pmul((seed | 0) * 40503 + 11));
  const mats = makeMaterials(THREE, { flex: true });
  const mat = mats.cloneMaterial(mats.lit); mat.uniforms.uPh.value = B.phase;
  const group = new THREE.Group(), root = new THREE.Group();
  root.scale.setScalar(length); group.add(root);
  const geo = mergeLit(THREE, res, 2.0); geo.computeBoundingSphere(); geo.boundingSphere.radius *= 1.25;
  const mesh = new THREE.Mesh(geo, mat); root.add(mesh);
  const tris = geo.attributes.position.count / 3;
  const smat = new THREE.SpriteMaterial({ map: glowTexture(THREE), color: B.eyeC, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true });
  const eyeSock = ['eyeP.front', 'eyeS.front', 'eyeSM.front'].filter((k) => res.sockets[k]);
  const eyes = eyeSock.map((k, i) => {
    const q = res.sockets[k].pos, sp2 = new THREE.Sprite(smat); sp2.position.set(q.x, q.y, q.z);
    const sz = B.eyeR * (i ? 0.75 : 1) * 7; sp2.scale.set(sz, sz, 1); sp2.userData.base = sz;
    sp2.userData.r = B.eyeR * 1.6 * length;   // hit radius, L units
    root.add(sp2); return sp2;
  });
  const stats = {
    hp: Math.round(40 * Math.pow(1.9, tier - 1) * F.hp),
    speed: +(14 * Math.pow(0.9, tier - 1) * F.sp).toFixed(2),
    turn: +(1.6 * Math.pow(0.85, tier - 1) * F.tn).toFixed(2),
    dmg: Math.round(8 * Math.pow(1.4, tier - 1) * F.dm),
  };
  if (role === 'brood') stats.spawnOnDeath = 3;
  const attacks = F.atk.slice(0, tier >= 3 ? 2 : 1);
  const ph0 = mat.uniforms.uPh.value;
  let lo = null;
  const out = {
    group, length, hitR: length * 0.55, eye: eyes[0], eyes, stats, attacks, tier, role, seed,
    signature: P.segs + '-' + P.fins + '-' + P.spikes, tris,
    update(t, dt) {
      mat.uniforms.uTime.value = t;
      for (let i = 0; i < eyes.length; i++) { const k = 1 + 0.18 * Math.sin(t * 4.0 + ph0 + i * 1.7); eyes[i].scale.set(eyes[i].userData.base * k, eyes[i].userData.base * k, 1); }
    },
    dispose() { geo.dispose(); mat.dispose(); smat.dispose(); if (lo) lo.userData.geo.dispose(); },
  };
  // rev 18 LOD: low variant (parts' lo geometry, same material instance so hit flash / flex stay in sync), built on first access
  Object.defineProperty(out, 'lo', {
    enumerable: true,
    get() {
      if (!lo) {
        const b2 = enemyBuild(THREE, seed, tier, role, 'lo'), r2 = compose(THREE, b2.recipe, pmul((seed | 0) * 40503 + 11));
        const g2 = mergeLit(THREE, r2, 2.0); g2.computeBoundingSphere(); g2.boundingSphere.radius *= 1.25;
        lo = new THREE.Group(); const rt = new THREE.Group(); rt.scale.setScalar(length); lo.add(rt);
        rt.add(new THREE.Mesh(g2, mat)); lo.userData.geo = g2; lo.userData.tris = g2.attributes.position.count / 3;
      }
      return lo;
    },
  });
  return out;
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

// ═════════════════════════ Revision 14: boss body plans ═════════════════════════
// generateBoss(THREE, name, wave, { kind, refRL, plan? }) -> same shape as generateEnemy plus `plan`, `name`.
//   kind: 'mini'|'giant'|'titan' (titan forces 'leviathan', length 1.5 * refRL); a plan name in `kind`/`plan` forces that plan.
//   length (L units) = refRL * clamp(0.05 * wave, 0.15, 1.0). Plan = hash(name) % 5 unless forced.
//   Plans: serpent | crab | jelly | leviathan | hydra. <= 2 merged meshes (opaque + translucent jelly dome) + eye sprites.
//   attacks = bossAttacksFor(...) objects ({id, tele, dmg}). Nose -Z, up +Y, contents in L units (root scaled by length).
export const BOSS_PLANS = ['serpent', 'crab', 'jelly', 'leviathan', 'hydra'];
export function bossHash(name) {
  let h = 2166136261;
  for (let i = 0; i < name.length; i++) { h ^= name.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
// (rev 18: the boss shaders live in ship-parts.js makeMaterials({flex:true}) - ONE program for every creature and boss.)
// aFx = (sway weight, phase, mode, param). mode 1 jaw hinge about Y, 2 sway, 3 fin flap about Z, 4 bell pulse.
function bossBuild(THREE, name, wave, opts, LOD) {
  const LO = LOD === 'lo';
  if (typeof opts === 'string') opts = { kind: opts };
  opts = opts || {};
  const kindS = typeof opts.kind === 'string' ? opts.kind : (opts.kind && opts.kind.kind) || '';
  const refRL = opts.refRL || (opts.kind && opts.kind.refRL) || 0;
  wave = Math.max(1, wave | 0 || 1);
  const hsh = bossHash(String(name || 'BOSS'));
  const r = mulberry(hsh);
  let plan = BOSS_PLANS[hsh % BOSS_PLANS.length];
  if (BOSS_PLANS.includes(opts.plan)) plan = opts.plan; else if (BOSS_PLANS.includes(kindS)) plan = kindS;
  const titan = kindS === 'titan';
  if (titan) plan = 'leviathan';
  const tier = titan ? BOSS_TIERS.titan : (kindS === 'giant' || wave >= 12 ? BOSS_TIERS.giant : BOSS_TIERS.mini);
  const length = refRL
    ? (titan ? 1.5 * refRL : refRL * Math.min(1.0, Math.max(0.15, 0.05 * wave)))
    : 6 * Math.pow(1.6, tier - 1);

  const V3 = (x, y, z) => new THREE.Vector3(x, y, z);
  const up = V3(0, 1, 0);
  const solid = [], glass = [];
  const jit = (a) => rn(r, -a, a);
  let lmCur = null;      // rev 17: {id, w(x,y,z)} while a limb's geometry is being added
  function add(g, colorFn, fx, piv, list, boost) {
    if (g.index) g = g.toNonIndexed();
    g.deleteAttribute('normal'); g.deleteAttribute('uv');
    const p = g.attributes.position, n = p.count, col = new Float32Array(n * 3), f = new Float32Array(n * 4), pv = new Float32Array(n * 3), lmA = new Float32Array(n * 2), c = new THREE.Color();
    const keep = colorFn === null ? g.attributes.color.array : null;      // colorFn null = keep the part's own vertex colours (boost scales emissive ones past the self-lit threshold)
    for (let i = 0; i < n; i++) {
      const x = p.getX(i), y = p.getY(i), z = p.getZ(i);
      if (keep) { const k = boost || 1; col[i * 3] = keep[i * 3] * k; col[i * 3 + 1] = keep[i * 3 + 1] * k; col[i * 3 + 2] = keep[i * 3 + 2] * k; }
      else { colorFn(c, x, y, z); col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b; }
      const q = typeof fx === 'function' ? fx(x, y, z) : (fx || [0, 0, 0, 0]);
      f[i * 4] = q[0]; f[i * 4 + 1] = q[1]; f[i * 4 + 2] = q[2]; f[i * 4 + 3] = q[3];
      if (piv) { pv[i * 3] = piv.x; pv[i * 3 + 1] = piv.y; pv[i * 3 + 2] = piv.z; }
      if (lmCur) { lmA[i * 2] = lmCur.id + 1; lmA[i * 2 + 1] = Math.min(1, Math.max(0, lmCur.w(x, y, z))); }
    }
    g.setAttribute('color', new THREE.BufferAttribute(col, 3));
    g.setAttribute('aFx', new THREE.BufferAttribute(f, 4));
    g.setAttribute('aPiv', new THREE.BufferAttribute(pv, 3));
    g.setAttribute('aLm', new THREE.BufferAttribute(lmA, 2));
    (list || solid).push(g);
  }
  const col = (h, s, l) => new THREE.Color().setHSL(((h % 1) + 1) % 1, s, l);
  const flat = (c0) => (c) => c.copy(c0);
  // tapered cylinder between two points (top at b)
  const sd = (k) => LO ? Math.max(3, k >> 1) : k;      // low-LOD: half the radial / ring segments
  function tubeGeo(a, b, ra, rb, sides) {
    const d = b.clone().sub(a), len = d.length();
    const g = new THREE.CylinderGeometry(rb, ra, len, sd(sides || 6), 1, true);
    g.applyMatrix4(new THREE.Matrix4().makeRotationFromQuaternion(new THREE.Quaternion().setFromUnitVectors(up, d.clone().normalize())));
    g.translate((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2);
    return g;
  }
  // cone with tip along dir, base centred at pos
  function coneGeo(pos, dir, rad, len, sides, flatten) {
    const g = new THREE.ConeGeometry(rad, len, sd(sides || 5));
    g.translate(0, len / 2, 0);
    if (flatten) g.scale(flatten[0], 1, flatten[1]);
    g.applyMatrix4(new THREE.Matrix4().makeRotationFromQuaternion(new THREE.Quaternion().setFromUnitVectors(up, dir.clone().normalize())));
    g.translate(pos.x, pos.y, pos.z);
    return g;
  }
  const ball = (x, y, z, rx, ry, rz, w, h) => { const g = new THREE.SphereGeometry(1, sd(w || 12), sd(h || 8)); g.scale(rx, ry, rz); g.translate(x, y, z); return g; };
  const eyeRecs = [];
  let eyeC, P = {};     // per plan uniforms
  // eyes: emissive ball + record for the sprite
  // rev 18: ship-parts pieces dropped into the boss through the same add() pipeline (limb weights / flex fx / hinge pivots apply to them as to any geometry)
  const partRng = mulberry(hsh ^ 0x51ed270b);
  let partPal = null;
  function addPart(name, params, pos, dir, fx, piv) {
    const res = PARTS[name](THREE, Object.assign({ pal: partPal, lod: LO ? 'lo' : 'hi' }, params, LO ? { lod: 'lo' } : null), partRng);
    const m = new THREE.Matrix4().compose(pos, new THREE.Quaternion().setFromUnitVectors(up, dir.clone().normalize()), new THREE.Vector3(1, 1, 1));
    for (const k of ['geo', 'emissive']) {
      const g = res[k]; if (!g) continue;
      g.applyMatrix4(m); for (const a of ['aFx', 'aPiv', 'aLm']) g.deleteAttribute(a);
      add(g, null, fx || null, piv || null, null, k === 'emissive' ? 2.0 : 1);
    }
  }
  function eye(x, y, z, er, fx, dir) {
    const d = (dir || V3(0, 0.25, -1)).clone().normalize();
    addPart('eye', { r: er * 0.8, glowCol: eyeC }, V3(x - d.x * er * 0.55, y - d.y * er * 0.55, z - d.z * er * 0.55), d, fx || null);
    eyeRecs.push({ pos: [x, y, z], r: er, fx: fx && fx[2] === 2 ? fx : null, lm: lmCur ? lmCur.id : -1, lw: lmCur ? lmCur.w(x, y, z) : 0 });
  }
  const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
  const hue0 = { serpent: 0.38, crab: 0.025, jelly: 0.88, leviathan: 0.58, hydra: 0.11 }[plan] + jit(0.09);
  eyeC = col({ serpent: 0.13, crab: 0.5, jelly: 0.5, leviathan: 0.03, hydra: 0.78 }[plan], 1, 0.7);
  partPal = { base: col(hue0, 0.6, 0.32), panel: col(hue0, 0.65, 0.2), accent: col(hue0 + 0.04, 1, 0.56), glow: eyeC, dark: col(hue0, 0.7, 0.08), metal: col(hue0, 0.2, 0.45) };
  P = { amp: [0, 0, 0], sp: 1, k: 0, breath: 0, hsp: 1, pulse: 0 };
  let extra = [];
  // ── rev 17 limbs: rigid/progressive swing about a pivot, driven by shared uniform arrays (GPU) that the hit capsules read too (CPU) ──
  const limbs = [];
  const reachU = Math.min(0.5, 20 / length);
  function defLimb(o) {
    o.id = limbs.length; o.glow = 0; o.ax = V3(0, 1, 0); o.ang = 0; o.tz = 0; o.rest = false;
    o.r = o.r; o.A = o.A || 1.0; o.reach = o.reach || reachU;
    const f = o.back ? -1 : 1, atk = {};
    const k = {
      sweep: { wind: { yaw: f * o.A }, strike: { yaw: -f * o.A } },
      slam: { wind: { pitch: o.sl[0] }, strike: { pitch: o.sl[1] } },
      lunge: { wind: { tz: 0.06 * f, pitch: o.sl[0] * 0.3 }, strike: { tz: -o.reach } },
      whip: { wind: { yaw: f * 1.3 }, strike: { yaw: -f * 1.3 } },
    };
    for (const t of o.allow) atk[t] = (o.keys && o.keys[t]) || k[t];
    o.atk = atk;
    o.capsule = { a: V3(), b: V3(), r: o.r * length };
    o.a0 = o.a; o.b0 = o.b;
    limbs.push(o); return o.id;
  }
  const qY = new THREE.Quaternion(), qX = new THREE.Quaternion(), tmpV = V3(0, 0, 0), AXY = V3(0, 1, 0), AXX = V3(1, 0, 0);
  function xf(lm, w, src, out) {
    tmpV.copy(src).sub(lm.pivot); if (lm.ang) tmpV.applyAxisAngle(lm.ax, lm.ang * w);
    out.copy(lm.pivot).add(tmpV); out.z += lm.tz * w; return out;
  }
  const ez = (x) => x * x * (3 - 2 * x), lerp = (a, b, t) => a + (b - a) * t;
  function poseLimb(lm, att) {
    let yaw = 0, pitch = 0, tz = 0, glow = 0;
    const active = att.ph !== 'idle' && (att.limb === lm.id || att.limb < 0);
    const kk = active ? (att.type === 'spin' ? (lm.atk.sweep || lm.atk.whip) : lm.atk[att.type]) : null;
    if (kk) {
      const W = kk.wind, S = kk.strike, u = att.u, sd = att.side || 1, sc = att.type === 'spin' ? 0.5 : 1;
      let A, B, e;
      if (att.ph === 'tele') { A = null; B = W; e = ez(u); glow = 0.3 + 0.7 * u; }
      else if (att.ph === 'strike') { A = W; B = S; e = u * u; glow = 1; }
      else { A = S; B = null; e = ez(u); glow = 1 - u; }
      const g = (o, key) => (o ? (o[key] || 0) : 0);
      yaw = lerp(g(A, 'yaw'), g(B, 'yaw'), e) * sd * sc; pitch = lerp(g(A, 'pitch'), g(B, 'pitch'), e); tz = lerp(g(A, 'tz'), g(B, 'tz'), e);
    }
    if (!active || !kk) { if (lm.rest && lm.glow === 0) return; yaw = pitch = tz = 0; glow = 0; }
    lm.rest = !yaw && !pitch && !tz && !glow; lm.glow = glow;
    qY.setFromAxisAngle(AXY, yaw); qX.setFromAxisAngle(AXX, pitch); qY.multiply(qX);
    if (qY.w < 0) { qY.x = -qY.x; qY.y = -qY.y; qY.z = -qY.z; qY.w = -qY.w; }
    const sn = Math.sqrt(Math.max(0, 1 - qY.w * qY.w));
    if (sn > 1e-5) { lm.ax.set(qY.x / sn, qY.y / sn, qY.z / sn); lm.ang = 2 * Math.acos(Math.min(1, qY.w)); } else { lm.ax.set(0, 1, 0); lm.ang = 0; }
    lm.tz = tz;
    const cp = lm.capsule;
    xf(lm, lm.wa, lm.a0, cp.a).multiplyScalar(length); xf(lm, lm.wb, lm.b0, cp.b).multiplyScalar(length);
  }

  if (plan === 'serpent') {
    // long spine of 9-13 tapered segments, dense dorsal fins, pectoral fins, tail fan; travelling sine wave down the body
    P.amp = [0.1, 0.03, 0]; P.sp = 2.4; P.k = 7; P.hsp = 3.0;
    const hull = col(hue0, 0.7, 0.3), belly = col(hue0 - 0.06, 0.55, 0.55), band = col(hue0 + 0.03, 0.8, 0.18), fin = col(hue0 + 0.42, 0.85, 0.5), glowC = col(hue0 + 0.3, 1, 0.6);
    const n = ri(r, 9, 13), sp = 0.9 / (n - 1);
    const segs = [];
    for (let i = 0; i < n; i++) {
      const t = i / (n - 1);
      const rad = i === 0 ? 0.08 : (0.032 + 0.045 * Math.pow(1 - t, 0.9) + (i === 1 ? 0.012 : 0));
      segs.push({ t, z: -0.45 + i * sp, rad, rz: Math.max(sp * 0.8, rad * 0.9), fx: [i === 0 ? 0 : Math.pow(t, 0.9), 0, 2, t] });
    }
    const lHead = defLimb({ name: 'head', kind: 'head', pivot: V3(0, 0, segs[3].z), a: V3(0, 0, segs[2].z), b: V3(0, 0, segs[0].z - 0.17), r: 0.1, wa: 0.4, wb: 1, sl: [0.5, -0.6], allow: ['sweep', 'slam', 'lunge'] });
    const lTail = defLimb({ name: 'tail', kind: 'tail', pivot: V3(0, 0, segs[n - 4].z), a: V3(0, 0, segs[n - 3].z), b: V3(0, 0, segs[n - 1].z + 0.17), r: 0.09, wa: 0.3, wb: 1, back: true, sl: [-0.6, 0.5], allow: ['whip', 'slam'] });
    segs.forEach((s, i) => {
      lmCur = i <= 2 ? { id: lHead, w: () => [1, 0.75, 0.4][i] } : (i >= n - 3 ? { id: lTail, w: () => [0.3, 0.65, 1][i - (n - 3)] } : null);
      add(ball(0, 0, s.z, s.rad, s.rad * 0.95, s.rz, 12, 8), (c, x, y, z) => {
        const stripe = Math.sin((z - s.z) / s.rz * 3.0) > 0.35;
        c.copy(y < -s.rad * 0.25 ? belly : (stripe ? band : hull));
      }, s.fx);
      if (i > 0) {   // dorsal fin
        const h = 0.06 + 0.07 * Math.sin(Math.PI * Math.min(1, t01(s.t))) ;
        addPart('fin', { len: h + 0.03, w: 0.075, sweep: 0.55, thick: 0.012, taper: 0.4, base: fin, tip: glowC, lod: i % 3 === 1 ? 'hi' : 'lo' }, V3(0, s.rad * 0.7, s.z), V3(0, 1, 0.35), s.fx);
        if (i % 2 === 0) add(ball(0, s.rad * 1.0, s.z, 0.011, 0.011, 0.011, 5, 4), (c) => c.copy(glowC).multiplyScalar(1.9), s.fx);
      }
    });
    lmCur = null;
    function t01(t) { return t; }
    for (const i of [1, 2, 4]) for (const side of [-1, 1]) {   // pectoral fins: flap about the body axis
      const s = segs[Math.min(i, n - 1)], piv = V3(side * s.rad * 0.8, -s.rad * 0.1, s.z);
      add(coneGeo(piv, V3(side, -0.15, 0.4), 0.05, 0.22 - i * 0.02, 3, [0.18, 1.6]), (c, x) => c.copy(fin).lerp(hull, 0.35 + 0.4 * Math.max(0, 1 - Math.abs(x) / 0.25)), [0, side * i, 3, 0.55], piv);
    }
    const tl = segs[n - 1];
    lmCur = { id: lTail, w: () => 1 };
    for (const dy of [-1, 1]) add(coneGeo(V3(0, 0, tl.z), V3(0, dy * 0.5, 1), 0.07, 0.17, 4, [0.2, 1.8]), (c) => c.copy(fin), tl.fx);
    // head: snout, jaw, horns
    const hd = segs[0];
    lmCur = { id: lHead, w: () => 1 };
    add(coneGeo(V3(0, -0.01, hd.z - hd.rz * 0.5), V3(0, -0.05, -1), hd.rad * 0.75, 0.1, 6, [1, 0.7]), (c) => c.copy(hull).multiplyScalar(0.85), hd.fx);
    for (const side of [-1, 1]) { addPart('spike', { len: 0.11, r: 0.017, bend: 0.3, base: band, tip: glowC }, V3(side * hd.rad * 0.5, hd.rad * 0.7, hd.z + 0.01), V3(side * 0.5, 0.9, 0.7), hd.fx); addPart('spike', { len: 0.05, r: 0.008, lod: 'lo', base: band, tip: fin }, V3(side * hd.rad * 0.3, -hd.rad * 0.35, hd.z - hd.rz * 0.8), V3(side * 0.1, -0.5, -1), hd.fx); }
    for (const side of [-1, 1]) eye(side * hd.rad * 0.72, hd.rad * 0.3, hd.z - hd.rz * 0.55, 0.026, null, V3(side * 0.7, 0.3, -0.6));
    lmCur = null;
  }

  else if (plan === 'crab') {
    // flat wide shell, 6 articulated claws (2 big pincers + 4 small), eye stalks
    P.amp = [0.012, 0.02, 0]; P.sp = 2.0; P.hsp = 2.7;
    const shell = col(hue0, 0.8, 0.34), dome = col(hue0 + 0.02, 0.75, 0.46), dark = col(hue0, 0.8, 0.12), tip = col(0.1, 0.35, 0.82), claw = col(hue0 - 0.01, 0.85, 0.27);
    add(ball(0, 0, 0.04, 0.4, 0.1, 0.27, 18, 10), (c, x, y, z) => c.copy(y < -0.03 ? dark : shell).multiplyScalar(0.9 + 0.2 * Math.abs(Math.sin(x * 40) * Math.cos(z * 37))));
    add(ball(0, 0.06, 0.05, 0.27, 0.09, 0.19, 14, 8), (c) => c.copy(dome));
    addPart('shell', { rx: 0.17, ry: 0.07, rz: 0.12, base: dome, panel: shell, accent: tip }, V3(0, 0.12, 0.06), V3(0, 1, 0));
    for (let i = 0; i < 9; i++) {   // rim spikes
      const a = Math.PI * 0.12 + i * Math.PI * 0.095 + (i > 4 ? 0.02 : 0), ca = Math.cos(a), sa = Math.sin(a);
      for (const side of [-1, 1]) if (!(i === 0 && side === 1)) addPart('spike', { len: 0.1, r: 0.024, lod: 'lo', base: shell, tip: tip }, V3(side * 0.38 * sa * 0.95, 0.04, 0.04 + 0.26 * ca * 0.95), V3(side * sa, 0.6, ca));
    }
    function jaws(w, s, len, yaw, amp, ph) {
      add(ball(0, 0, 0, s * 0.9, s * 0.6, s * 0.9, 8, 6).translate(w.x, w.y, w.z), flat(claw));
      for (const side of [-1, 1]) {
        const g = new THREE.ConeGeometry(s * 0.5, len, 5);
        g.rotateX(-Math.PI / 2); g.translate(0, 0, -len / 2); g.scale(1, 0.65, 1);
        g.translate(side * s * 0.45, 0, 0); g.rotateY(side * 0.2 + yaw);
        g.translate(w.x, w.y, w.z);
        add(g, (c, x, y, z) => { const d = Math.hypot(x - w.x, z - w.z) / len; c.copy(claw).lerp(tip, smooth(0.55, 0.9, d)); }, [0, ph, 1, -side * amp], w);
      }
    }
    for (const side of [-1, 1]) {   // big front pincers
      const sh = V3(side * 0.3, 0, -0.1), el = V3(side * 0.46, 0.01, -0.2), wr = V3(side * 0.4, 0, -0.38);
      lmCur = { id: defLimb({ name: (side < 0 ? 'left' : 'right') + ' claw', kind: 'claw', pivot: sh, a: sh, b: V3(wr.x, 0, wr.z - 0.17), r: 0.1, wa: 1, wb: 1, A: 0.95, sl: [0.7, -0.5], reach: Math.min(0.4, 20 / length), allow: ['sweep', 'slam', 'lunge'] }), w: () => 1 };
      add(tubeGeo(sh, el, 0.05, 0.045, 6), flat(claw)); add(ball(el.x, el.y, el.z, 0.055, 0.055, 0.055, 8, 6), flat(shell));
      add(tubeGeo(el, wr, 0.045, 0.05, 6), flat(claw));
      jaws(wr, 0.055, 0.17, -side * 0.05, 0.6, rn(r, 0, 6.28));
      lmCur = null;
    }
    for (const side of [-1, 1]) for (const zz of [0.0, 0.2]) {   // 4 small claws
      const sh = V3(side * 0.34, -0.01, zz), wr = V3(side * 0.47, -0.02, zz - 0.07 + (zz > 0.1 ? 0.04 : 0));
      add(tubeGeo(sh, wr, 0.03, 0.025, 5), flat(claw));
      jaws(wr, 0.03, 0.09, -side * 0.7, 0.55, rn(r, 0, 6.28));
    }
    for (const side of [-1, 1]) {   // eye stalks (sway)
      const b = V3(side * 0.09, 0.08, -0.2), tp = V3(side * 0.1, 0.2, -0.24), fxS = [1, side * 1.3, 2, 0];
      add(tubeGeo(b, tp, 0.018, 0.014, 5), flat(shell), fxS);
      eye(tp.x, tp.y + 0.01, tp.z - 0.005, 0.03, fxS, V3(side * 0.25, 0.5, -0.8));
    }
  }

  else if (plan === 'jelly') {
    // translucent bell facing forward, glowing core, rim + ribs, 12 trailing tentacles + 4 oral arms
    P.amp = [0.05, 0.05, 0.03]; P.sp = 1.7; P.k = 4.5; P.hsp = 1.7; P.pulse = 0.09;
    const flesh = col(hue0, 0.6, 0.62), deep = col(hue0 - 0.05, 0.7, 0.3), glowC = col(0.52, 1, 0.65), zc = -0.18;
    const dome = new THREE.SphereGeometry(1, sd(26), sd(12), 0, Math.PI * 2, 0, Math.PI / 2);
    dome.scale(0.34, 0.3, 0.34); dome.rotateX(-Math.PI / 2); dome.translate(0, 0, zc);
    add(dome, (c, x, y, z) => c.copy(flesh).lerp(deep, 0.5 * (1 - Math.min(1, Math.hypot(x, y) / 0.34))), [0, 0, 4, 0], null, glass);
    const rim = new THREE.TorusGeometry(0.34, 0.013, sd(5), sd(36)); rim.translate(0, 0, zc);
    add(rim, (c) => c.copy(glowC).multiplyScalar(1.8), [0, 0, 4, 0]);
    for (let k = 0; k < 8; k++) {   // ribs
      const phi = k / 8 * Math.PI * 2, pts = [];
      for (let j = 0; j <= 6; j++) { const th = j / 6 * Math.PI / 2; pts.push(V3(0.345 * Math.sin(th) * Math.cos(phi), 0.345 * Math.sin(th) * Math.sin(phi), zc - 0.305 * Math.cos(th))); }
      add(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), sd(10), 0.005, sd(4)), (c) => c.copy(glowC).multiplyScalar(1.6), [0, 0, 4, 0]);
    }
    const nT = 12;
    for (let i = 0; i < nT; i++) {
      const a = i / nT * Math.PI * 2 + 0.2, len = rn(r, 0.55, 0.7), ph = rn(r, 0, 6.28), x0 = Math.cos(a) * 0.29, y0 = Math.sin(a) * 0.29;
      const g = new THREE.CylinderGeometry(0.003, 0.014, len, sd(5), sd(14), true); g.rotateX(Math.PI / 2); g.translate(x0, y0, zc + len / 2);
      const wf = (z) => Math.min(1, Math.max(0, (z - zc) / len));
      add(g, (c, x, y, z) => { const w = wf(z); c.copy(flesh).lerp(deep, w * 0.6); if (w > 0.82) c.copy(glowC).multiplyScalar(1.9); }, (x, y, z) => { const w = wf(z); return [w * w, ph, 2, w]; });
    }
    for (let i = 0; i < 4; i++) {   // thick oral arms
      const a = i / 4 * Math.PI * 2 + 0.6, len = rn(r, 0.7, 0.8), ph = rn(r, 0, 6.28), x0 = Math.cos(a) * 0.07, y0 = Math.sin(a) * 0.07;
      const g = new THREE.CylinderGeometry(0.008, 0.032, len, sd(6), sd(12), true); g.rotateX(Math.PI / 2); g.translate(x0, y0, zc + len / 2 - 0.03);
      const wf = (z) => Math.min(1, Math.max(0, (z - zc) / len));
      lmCur = { id: defLimb({ name: 'oral arm ' + (i + 1), kind: 'tentacle', pivot: V3(x0, y0, zc), a: V3(x0, y0, zc + 0.12), b: V3(x0, y0, zc + len), r: 0.075, wa: 0.15, wb: 1, sl: [-1.6, -3.0], allow: ['slam', 'sweep'],
        keys: { slam: { wind: { pitch: -1.6 }, strike: { pitch: -3.0 } }, sweep: { wind: { pitch: -2.4, yaw: 1.1 }, strike: { pitch: -2.7, yaw: -1.1 } } } }), w: (x, y, z) => wf(z) };
      add(g, (c, x, y, z) => { const w = wf(z); c.copy(deep).lerp(flesh, 0.4 + 0.4 * Math.abs(Math.sin(z * 60))); if (w > 0.9) c.copy(glowC).multiplyScalar(1.7); }, (x, y, z) => { const w = wf(z); return [w * w * 0.8, ph, 2, w]; });
      lmCur = null;
    }
    eye(0, 0, zc - 0.1, 0.075, null, V3(0, 0, -1));
    eye(-0.15, 0.05, zc - 0.12, 0.04, null, V3(-0.4, 0.1, -1)); eye(0.15, 0.05, zc - 0.12, 0.04, null, V3(0.4, 0.1, -1));
    extra.push({ pos: [0, 0, zc - 0.08], size: 0.6, color: col(0.52, 1, 0.55) });   // inner glow
  }

  else if (plan === 'leviathan') {
    // heavy straight armored hull of plates, 2 spike rows, 3 eyes, slow breathing scale, slow paddle fins
    P.breath = 0.035; P.hsp = 0.9;
    const steel = col(hue0, 0.28, 0.3), plate = col(hue0, 0.3, 0.2), bone = col(0.1, 0.25, 0.66), glowC = col(0.04, 1, 0.58);
    const m = 9, z0 = -0.28, z1 = 0.46, st = (z1 - z0) / m;
    const lH = defLimb({ name: 'ram prow', kind: 'head', pivot: V3(0, 0, z0 + 2 * st), a: V3(0, 0, z0 + st), b: V3(0, 0, -0.62), r: 0.17, wa: 0.6, wb: 1, sl: [0.25, -0.35], allow: ['lunge', 'slam', 'sweep'] });
    const lT = defLimb({ name: 'tail', kind: 'tail', pivot: V3(0, 0, z0 + (m - 3.5) * st), a: V3(0, 0, z0 + (m - 2.5) * st), b: V3(0, 0, z1 + 0.22), r: 0.17, wa: 0.3, wb: 1, back: true, sl: [-0.45, 0.4], allow: ['whip', 'slam'] });
    for (let i = 0; i < m; i++) {
      lmCur = i <= 1 ? { id: lH, w: () => [1, 0.5][i] } : (i >= m - 3 ? { id: lT, w: () => [0.3, 0.65, 1][i - (m - 3)] } : null);
      const t = i / (m - 1), rx = 0.2 * (0.6 + 0.4 * Math.sin(Math.PI * (0.15 + 0.7 * t))) * (1 - 0.5 * t * t), ry = rx * 0.82, zc = z0 + (i + 0.5) * st;
      const g = new THREE.CylinderGeometry(rx * 0.95, rx, st * 0.98, sd(12), 1, false); g.rotateX(Math.PI / 2); g.scale(1, ry / rx, 1); g.translate(0, 0, zc);
      add(g, (c, x, y, z) => c.copy(i % 2 ? plate : steel).multiplyScalar(0.85 + 0.3 * Math.abs(Math.sin(x * 50 + z * 20))));
      const tr = new THREE.TorusGeometry(rx * 1.0, 0.006, sd(4), sd(16)); tr.scale(1, ry / rx, 1); tr.translate(0, 0, z0 + i * st);
      add(tr, (c) => c.copy(glowC).multiplyScalar(1.8));
      if (i > 0) addPart('plate', { w: rx * 1.3, d: st * 0.85, th: 0.018, lod: 'lo', base: plate, panel: steel }, V3(0, ry * 0.97, zc), V3(0, 1, 0));
      for (const row of [-1, 0, 1]) {   // spike rows: dorsal ridge + two shoulder rows
        const a = Math.PI / 2 - row * 0.62, dir = V3(Math.cos(a) * 0.6 * (row ? 1 : 0), 1, -0.5).normalize();
        const sx = Math.cos(a) * rx * 0.95 * (row ? 1 : 0), sy = Math.sin(a) * ry * 0.95;
        addPart('spike', { len: (row ? 0.1 : 0.15) * (1 - 0.4 * t) + 0.03, r: 0.028 * (row ? 1 : 1.3), lod: 'lo', base: bone, tip: glowC }, V3(sx, sy, zc), row ? V3(row * 0.6, 1, 0.3) : V3(0, 1, 0.35));
      }
    }
    // armored prow wedge + brow plates
    lmCur = { id: lH, w: () => 1 };
    const prow = new THREE.ConeGeometry(0.2, 0.3, 6); prow.rotateX(-Math.PI / 2); prow.scale(1, 0.82, 1); prow.translate(0, 0, -0.43);
    add(prow, (c, x, y, z) => c.copy(steel).lerp(plate, smooth(-0.5, -0.3, z) * 0.0 + 0.2).multiplyScalar(0.9 + 0.2 * Math.abs(Math.sin(x * 70))));
    for (const side of [-1, 1]) {
      addPart('plate', { w: 0.1, d: 0.1, th: 0.02, base: plate, panel: steel }, V3(side * 0.11, 0.095, -0.33), V3(side * 0.3, 1, 0));      // brow plates
      addPart('spike', { len: 0.18, r: 0.033, bend: 0.15, base: bone, tip: bone }, V3(side * 0.17, -0.02, -0.34), V3(side * 0.4, -0.1, -1));   // tusks
    }
    lmCur = { id: lT, w: () => 1 };
    addPart('spike', { len: 0.22, r: 0.1, base: plate, tip: glowC }, V3(0, 0, z1), V3(0, 0.05, 1));      // tail spike
    lmCur = null;
    for (const [zi, side] of [[2, -1], [2, 1], [5, -1], [5, 1]]) {   // slow paddle fins
      const zc = z0 + (zi + 0.5) * st, piv = V3(side * 0.17, -0.02, zc);
      add(coneGeo(piv, V3(side, -0.25, 0.5), 0.07, 0.28, 4, [0.2, 1.8]), (c, x) => c.copy(plate).lerp(glowC, 0.12 + 0.2 * smooth(0.12, 0.4, Math.abs(x))), [0, side * zi, 3, 0.28], piv);
    }
    lmCur = { id: lH, w: () => 1 };
    eye(0, 0.065, -0.4, 0.04, null, V3(0, 0.2, -1)); eye(-0.1, 0.04, -0.34, 0.03, null, V3(-0.5, 0.1, -1)); eye(0.1, 0.04, -0.34, 0.03, null, V3(0.5, 0.1, -1));
    lmCur = null;
  }

  else {   // hydra
    // low body, 3 long necks (swaying independently) each ending in a horned head with an eye
    P.amp = [0.11, 0.07, 0.05]; P.sp = 1.3; P.k = 0; P.hsp = 1;
    const hide = col(hue0, 0.55, 0.3), belly = col(hue0, 0.45, 0.5), dark = col(hue0 + 0.02, 0.6, 0.12), horn = col(0.1, 0.2, 0.75), mouth = col(0.95, 0.8, 0.45), spot = col(0.78, 0.8, 0.5);
    add(ball(0, 0, 0.18, 0.27, 0.19, 0.3, 16, 10), (c, x, y, z) => c.copy(y < -0.06 ? belly : hide).multiplyScalar(0.88 + 0.24 * Math.abs(Math.sin(x * 35) * Math.cos(z * 31))));
    add(coneGeo(V3(0, 0, 0.4), V3(0, 0.12, 1), 0.12, 0.28, 6), flat(dark));
    addPart('shell', { rx: 0.2, ry: 0.07, rz: 0.2, base: hide, panel: dark, accent: spot }, V3(0, 0.15, 0.2), V3(0, 1, 0));
    for (let i = 0; i < 6; i++) addPart('spike', { len: 0.1, r: 0.027, lod: 'lo', base: horn, tip: horn }, V3((i % 2 - 0.5) * 0.14, 0.17, 0.05 + i * 0.07), V3(0, 1, 0.3));
    for (const [x, z] of [[-0.2, 0.0], [0.2, 0.0], [-0.2, 0.34], [0.2, 0.34]]) { add(tubeGeo(V3(x, -0.1, z), V3(x * 1.1, -0.26, z), 0.06, 0.07, 6), flat(dark)); add(ball(x * 1.1, -0.27, z - 0.02, 0.08, 0.025, 0.1, 8, 4), flat(dark)); }
    const heads = [[0, 0.25, -0.48], [-0.32, 0.09, -0.4], [0.32, 0.09, -0.4]];
    heads.forEach(([hx, hy, hz], k) => {
      const ph = k * 2.1 + rn(r, 0, 1), base = V3(hx * 0.3, 0.12, 0.0);
      const curve = new THREE.CatmullRomCurve3([base, V3(hx * 0.4, 0.24 + hy * 0.3, -0.06), V3(hx * 0.8 + (k ? (k === 1 ? -0.06 : 0.06) : 0), hy + 0.12, hz * 0.5), V3(hx, hy, hz)]);
      const pts = curve.getPoints(10);
      const lhd = defLimb({ name: 'head ' + (k + 1), kind: 'head', pivot: base, a: pts[3], b: V3(pts[10].x, pts[10].y - 0.02, pts[10].z - 0.15), r: 0.1, wa: 0.3, wb: 1, sl: [0.5, -0.6], allow: ['sweep', 'slam', 'lunge'] });
      for (let j = 0; j < 10; j++) {
        const a = pts[j], b = pts[j + 1], w0 = j / 10, w1 = (j + 1) / 10, ra = 0.075 - 0.04 * w0, rb = 0.075 - 0.04 * w1;
        const d = b.clone().sub(a), l2 = d.lengthSq();
        lmCur = { id: lhd, w: (x, y, z) => { const u = Math.min(1, Math.max(0, ((x - a.x) * d.x + (y - a.y) * d.y + (z - a.z) * d.z) / l2)); return w0 + (w1 - w0) * u; } };
        add(tubeGeo(a, b, ra, rb, 7), (c, x, y, z) => { c.copy(hide).lerp(belly, y < a.y - ra * 0.3 ? 0.7 : 0); if (j % 3 === 0) c.lerp(spot, 0.3); },
          (x, y, z) => { const u = Math.min(1, Math.max(0, ((x - a.x) * d.x + (y - a.y) * d.y + (z - a.z) * d.z) / l2)), w = w0 + (w1 - w0) * u; return [w * w, ph, 2, 0]; });
      }
      const fxH = [1, ph, 2, 0], H = pts[10];
      lmCur = { id: lhd, w: () => 1 };
      add(ball(H.x, H.y, H.z - 0.01, 0.07, 0.055, 0.1, 10, 7), flat(hide), fxH);
      add(coneGeo(V3(H.x, H.y - 0.005, H.z - 0.06), V3(0, -0.15, -1), 0.045, 0.09, 6, [1, 0.7]), flat(hide), fxH);                // snout
      add(coneGeo(V3(H.x, H.y - 0.045, H.z - 0.04), V3(0, -0.4, -1), 0.04, 0.1, 5, [1, 0.5]), flat(mouth), fxH);               // jaw
      for (const side of [-1, 1]) {
        addPart('spike', { len: 0.11, r: 0.017, bend: 0.3, base: horn, tip: horn }, V3(H.x + side * 0.04, H.y + 0.04, H.z + 0.02), V3(side * 0.5, 0.8, 0.9), fxH);
        addPart('spike', { len: 0.04, r: 0.009, lod: 'lo', base: horn, tip: horn }, V3(H.x + side * 0.02, H.y - 0.03, H.z - 0.1), V3(0, -1, -0.2), fxH);          // fangs
      }
      eye(H.x, H.y + 0.03, H.z - 0.07, 0.027, fxH, V3(0, 0.3, -1));
      lmCur = null;
    });
  }

  // ── assemble ──
  const group = new THREE.Group(), root = new THREE.Group();
  root.scale.setScalar(length); group.add(root);
  const mats0 = makeMaterials(THREE, { flex: true });
  const bmat = mats0.cloneMaterial(mats0.lit), uniforms = bmat.uniforms;      // per-boss instance of the shared flex material (own uniforms, same GL program as every creature)
  uniforms.uPh.value = rn(r, 0, 6.28); uniforms.uWave.value = 0;
  uniforms.uAmp.value.set(P.amp[0], P.amp[1], P.amp[2]); uniforms.uSp.value = P.sp; uniforms.uK.value = P.k;
  uniforms.uBreath.value = P.breath; uniforms.uHsp.value = P.hsp; uniforms.uPulse.value = P.pulse;
  for (let i = 0; i < 8; i++) if (limbs[i]) uniforms.uSwP.value[i].copy(limbs[i].pivot);
  // attack state (ship.js writes it every frame): which limb(s) move and how far through telegraph / strike / recover they are
  const att = { type: '', limb: -1, ph: 'idle', u: 0, side: 1 };
  // seeded combo: 2-4 moves picked from the limb set (sweep / slam / lunge / whip, plus a full-body spin when >= 2 limbs can swing)
  const DUR = { sweep: 0.6, slam: 0.45, lunge: 0.4, whip: 0.55, spin: 1.7 };
  const cand = [];
  for (const lm of limbs) for (const t of Object.keys(lm.atk)) cand.push({ type: t, limb: lm.id });
  if (limbs.filter((lm) => lm.atk.sweep || lm.atk.whip).length >= 2) cand.push({ type: 'spin', limb: -1 });
  for (let i = cand.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); const q = cand[i]; cand[i] = cand[j]; cand[j] = q; }
  const moves = cand.slice(0, Math.min(cand.length, ri(r, 2, 4))).map((c) => {
    let reach = 0;
    for (const lm of limbs) {
      if (c.limb >= 0 && lm.id !== c.limb) continue;
      reach = Math.max(reach, lm.b0.length(), lm.pivot.length() + lm.b0.distanceTo(lm.pivot) * lm.wb);
      if (c.type === 'lunge') reach = Math.max(reach, lm.b0.length() + lm.reach);
    }
    return { type: c.type, limb: c.limb, name: (c.limb >= 0 ? limbs[c.limb].name + ' ' : 'whole body ') + c.type, tele: +rn(r, 0.8, 1.5).toFixed(2), dmg: ri(r, 25, 45), cd: +rn(r, 2, 4).toFixed(2), dur: DUR[c.type], rec: 0.55, reachL: reach * length };
  });
  const mats = [bmat];
  const geoms = [];
  let tris = 0;
  const lists = [[solid, mats[0]]];
  if (glass.length) {
    const gm = mats0.cloneMaterial(mats0.glass); gm.uniforms = uniforms;
    mats.push(gm); lists.push([glass, gm]);
  }
  for (const [list, mt] of lists) {
    const g = mergeGeometries(list); g.computeBoundingSphere(); g.boundingSphere.radius *= 1.6;
    const mesh = new THREE.Mesh(g, mt); mesh.frustumCulled = false;
    if (mt.transparent) mesh.renderOrder = 2;
    root.add(mesh); geoms.push(g); tris += g.attributes.position.count / 3;
    list.forEach((q) => q.dispose());
  }
  const smat = new THREE.SpriteMaterial({ map: glowTexture(THREE), color: eyeC, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true });
  const eyes = eyeRecs.map((e, i) => {
    const sp2 = new THREE.Sprite(smat); sp2.position.set(e.pos[0], e.pos[1], e.pos[2]);
    const sz = e.r * 7; sp2.scale.set(sz, sz, 1); sp2.userData.base = sz;
    sp2.userData.r = e.r * 1.6 * length;
    root.add(sp2); return sp2;
  });
  const xmat = extra.length ? new THREE.SpriteMaterial({ map: glowTexture(THREE), color: extra[0].color, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0.55 }) : null;
  const extras = extra.map((e) => { const s = new THREE.Sprite(xmat); s.position.set(...e.pos); s.scale.set(e.size, e.size, 1); root.add(s); return s; });
  const ph0 = uniforms.uPh.value, A = P.amp;
  const tierMul = Math.pow(1.9, tier - 1);
  const stats = {
    hp: Math.round(40 * tierMul * 3 * (1 + 0.04 * wave)),
    speed: 0, turn: 0.4,
    dmg: Math.round(8 * Math.pow(1.4, tier - 1)),
  };
  const attacks = bossAttacksFor(hsh | 0, tier);
  return {
    group, length, hitR: length * (plan === 'crab' || plan === 'hydra' ? 0.6 : 0.55), eye: eyes[0], eyes, stats, attacks,
    tier, role: 'boss', seed: hsh | 0, name, plan, signature: plan, tris, mats,
    limbs, moves, att,
    setHit(v) { uniforms.uHit.value = v; },
    update(t, dt) {
      uniforms.uTime.value = t;
      for (let i = 0; i < limbs.length; i++) {
        const lm = limbs[i];
        poseLimb(lm, att);
        const a = uniforms.uSwA.value[i], b = uniforms.uSwT.value[i];
        a.set(lm.ax.x, lm.ax.y, lm.ax.z, lm.ang); b.set(0, 0, lm.tz, lm.glow);
      }
      const bk = 1 + P.breath * Math.sin(t * 0.9);
      for (let i = 0; i < eyes.length; i++) {
        const e = eyeRecs[i], s = eyes[i];
        let x = e.pos[0], y = e.pos[1], z = e.pos[2];
        if (e.fx) {
          const f = e.fx, w = f[0];
          x += Math.sin(t * P.sp + f[1] + f[3] * P.k) * A[0] * w;
          y += Math.cos(t * P.sp * 0.8 + f[1] * 1.3 + f[3] * P.k) * A[1] * w;
          z += Math.sin(t * P.sp * 0.6 + f[1]) * A[2] * w;
        }
        if (e.lm >= 0 && limbs[e.lm].ang + limbs[e.lm].tz !== 0) { const lm = limbs[e.lm]; tmpV.set(x, y, z).sub(lm.pivot).applyAxisAngle(lm.ax, lm.ang * e.lw).add(lm.pivot); x = tmpV.x; y = tmpV.y; z = tmpV.z + lm.tz * e.lw; }
        s.position.set(x * bk, y * bk, z * bk);
        const k = 1 + 0.18 * Math.sin(t * 4.0 + ph0 + i * 1.7);
        s.scale.set(s.userData.base * k, s.userData.base * k, 1);
      }
    },
    dispose() { geoms.forEach((g) => g.dispose()); mats.forEach((m) => m.dispose()); smat.dispose(); if (xmat) xmat.dispose(); },
  };
}

// generateBoss = the hi build + a lazily built `lo` Group (same limb ids / weights / materials, parts' lo variants + half-res primitives).
export function generateBoss(THREE, name, wave, opts) {
  const out = bossBuild(THREE, name, wave, opts, 'hi');
  let lo = null;
  Object.defineProperty(out, 'lo', {
    enumerable: true,
    get() {
      if (!lo) {
        const b = bossBuild(THREE, name, wave, opts, 'lo');
        lo = new THREE.Group(); const rt = new THREE.Group(); rt.scale.copy(out.group.children[0].scale); lo.add(rt);
        const meshes = b.group.children[0].children.filter((o) => o.isMesh);
        meshes.forEach((m, i) => { m.material = out.mats[i] || out.mats[0]; rt.add(m); });
        lo.userData.geoms = meshes.map((m) => m.geometry);
        lo.userData.tris = meshes.reduce((a, m) => a + m.geometry.attributes.position.count / 3, 0);
        b.mats.forEach((m) => m.dispose());
      }
      return lo;
    },
  });
  const dis = out.dispose;
  out.dispose = () => { dis(); if (lo) lo.userData.geoms.forEach((g) => g.dispose()); };
  return out;
}

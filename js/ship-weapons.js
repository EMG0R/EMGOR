// Procedural weapons for NO MANS GOR. Seeded: same (seed, tier) -> same weapon.
// Stats units: dmg per projectile, rate shots/s, spread degrees, count projectiles per shot, speed & range in hull-lengths (L)/s and L.
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

function mulberry(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
function hashStr(s) {
  let h = 2166136261; s = String(s);
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
const SYL = ['VOTH', 'RAEL', 'XAL', 'MOR', 'ZETH', 'VOR', 'KRUN', 'ILU', 'THAX', 'NEM', 'OBB', 'SKAR', 'YUL', 'DRAK', 'ESS', 'QOR', 'VHEL', 'UMBR', 'GOR', 'NYX'];
const CLASSES = ['C', 'B', 'A', 'S'];
export const SPECIALS = ['pierce', 'chain', 'burn', 'homingLite', 'splitOnHit', 'overcharge'];
const SHAPES = ['bolt', 'needle', 'orb', 'shard'];
// class word by shape, with a flavour from the special
const WORDS = {
  bolt: ['PULSE CANNON', 'PHASE LANCER', 'BOLT DRIVER', 'ION REPEATER'],
  needle: ['NEEDLE GUN', 'RAIL SPIKE', 'SLIVER LANCE', 'PIN CASTER'],
  orb: ['PLASMA LOBBER', 'ORB PROJECTOR', 'SPHERE CANNON', 'GLOW MORTAR'],
  shard: ['SHARD SCATTER', 'CRYSTAL FAN', 'SPLINTER ARRAY', 'RAZOR SPRAY'],
};
const COLORS = {
  pierce: 0x6FD8FF, chain: 0xFFE24A, burn: 0xFF6A2A, homingLite: 0xB86CFF, splitOnHit: 0x6CFF9A, overcharge: 0xFF4FA8,
};
const SPECIAL_DESC = {
  pierce: 'Shots pass through targets.', chain: 'Arcs to a nearby enemy.', burn: 'Sets targets alight.',
  homingLite: 'Shots curve gently toward targets.', splitOnHit: 'Splits into shards on impact.', overcharge: 'Hold fire to build a bigger shot.',
};

export const STARTER_WEAPON = {
  id: 'starter-twin-laser', name: 'TWIN LASERS', cls: 'C',
  stats: { dmg: 8, rate: 6, spread: 0, count: 2, speed: 270, range: 120, special: null },
  color: 0x7FE8FF, shape: 'bolt', price: 0, desc: 'Standard issue twin lasers.',
};

export function generateWeapon(seed, tier = 0) {
  tier = Math.max(0, Math.min(3, tier | 0));
  const sd = typeof seed === 'number' ? seed | 0 : hashStr(seed);
  const r = mulberry(sd * 2654435761 + tier * 9973 + 4242);
  const pick = a => a[Math.floor(r() * a.length)];
  const shape = pick(SHAPES), special = pick(SPECIALS);
  let name = SYL[Math.floor(r() * SYL.length)] + (r() < 0.45 ? "'" : '') + SYL[Math.floor(r() * SYL.length)];
  if (r() < 0.25) name += SYL[Math.floor(r() * SYL.length)];
  name += ' ' + pick(WORDS[shape]);
  const tm = 1 + tier * 0.55;                       // tier multiplier
  const j = () => 0.85 + r() * 0.3;                 // +-15 % jitter
  // shape archetypes trade rate / damage / count
  const A = {
    bolt:   { dmg: 1.0, rate: 1.0, count: 2, spread: 2,  speed: 1.0, range: 1.0 },
    needle: { dmg: 1.5, rate: 0.55, count: 1, spread: 0.3, speed: 1.6, range: 1.4 },
    orb:    { dmg: 2.2, rate: 0.4, count: 1, spread: 1,  speed: 0.55, range: 0.8 },
    shard:  { dmg: 0.5, rate: 0.9, count: 5, spread: 14, speed: 0.9, range: 0.6 },
  }[shape];
  const stats = {
    dmg: Math.round(8 * A.dmg * tm * j() * 10) / 10,
    rate: Math.round(6 * A.rate * (1 + tier * 0.12) * j() * 10) / 10,
    spread: Math.round(A.spread * j() * 10) / 10,
    count: A.count + (tier >= 3 && A.count > 1 ? 2 : 0) + (tier >= 2 && A.count === 1 && r() < 0.4 ? 1 : 0),
    speed: Math.round(270 * A.speed * (1 + tier * 0.1) * j()),
    range: Math.round(120 * A.range * (1 + tier * 0.15) * j()),
    special,
  };
  const dps = stats.dmg * stats.rate * stats.count;
  const price = Math.round((140 + dps * 5) * (1 + tier * 0.9) / 10) * 10;
  return {
    id: 'w' + (sd >>> 0).toString(36) + tier + shape[0],
    name, cls: CLASSES[tier], stats, color: COLORS[special], shape, price,
    desc: SPECIAL_DESC[special] + ' ' + Math.round(dps) + ' dps.',
  };
}

export function weaponSetFor(outpostId, n = 4) {
  const base = hashStr(outpostId), r = mulberry(base);
  const out = [], names = new Set(), shapes = new Set();
  let guard = 0;
  while (out.length < n && guard++ < 200) {
    // spread tiers across the shop: mostly C/B, with a chance at A/S
    const k = out.length, roll = r();
    const tier = k === n - 1 ? (roll < 0.35 ? 3 : 2) : k === 0 ? 0 : (roll < 0.5 ? 1 : (roll < 0.85 ? 2 : 0));
    const w = generateWeapon(base + guard * 7919 + k * 31, tier);
    if (names.has(w.name)) continue;
    if (shapes.size < SHAPES.length && shapes.has(w.shape) && guard < 40) continue;   // prefer variety
    names.add(w.name); shapes.add(w.shape); out.push(w);
  }
  return out;
}

const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec4 mv = modelViewMatrix * vec4(position, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = max(0.0, dot(n, L)) * 0.7 + 0.3;
  float rim = pow(1.0 - max(0.0, dot(n, normalize(-vV))), 3.0) * 0.28;
  gl_FragColor = vec4(vC * d + rim * vec3(0.5, 0.3, 0.85), 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// Tiny gun pod, nose -Z, ~0.12 long, origin at its mount point (top centre). <= 300 tris.
export function buildWeaponModel(THREE, weapon) {
  const lit = [], emi = [];
  const put = (geo, hex, e) => {
    geo = geo.index ? geo.toNonIndexed() : geo;
    geo.deleteAttribute('normal'); geo.deleteAttribute('uv');
    const c = new THREE.Color(hex), n = geo.attributes.position.count, a = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
    geo.setAttribute('color', new THREE.BufferAttribute(a, 3));
    (e ? emi : lit).push(geo);
  };
  const tint = new THREE.Color(weapon.color), dark = 0x2A2430, mid = 0x4A4256;
  const cylZ = (rt, rb, len, seg, x, y, z, hex, e) => { const g = new THREE.CylinderGeometry(rt, rb, len, seg); g.rotateX(Math.PI / 2); g.translate(x, y, z); put(g, hex, e); };
  const box = (w, h, d, x, y, z, hex, e, rx = 0, rz = 0) => {
    const g = new THREE.BoxGeometry(w, h, d);
    if (rx || rz) g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, 0, rz)));
    g.translate(x, y, z); put(g, hex, e);
  };
  // mount + body
  box(0.03, 0.012, 0.05, 0, 0, 0, dark);
  cylZ(0.022, 0.026, 0.09, 8, 0, -0.025, 0.0, mid);
  box(0.03, 0.006, 0.03, 0, -0.025, 0.04, tint.getHex());                    // tinted tail plate
  const tip = -0.045;
  switch (weapon.shape) {
    case 'needle':
      cylZ(0.006, 0.01, 0.10, 6, 0, -0.025, tip - 0.05, dark);
      cylZ(0.003, 0.003, 0.02, 6, 0, -0.025, tip - 0.11, tint.getHex(), true);
      break;
    case 'orb': {
      cylZ(0.012, 0.016, 0.03, 6, 0, -0.025, tip - 0.015, dark);
      const g = new THREE.SphereGeometry(0.02, 8, 6); g.translate(0, -0.025, tip - 0.045); put(g, tint.getHex(), true);
      break;
    }
    case 'shard':
      for (let i = -1; i <= 1; i++) box(0.006, 0.006, 0.07, i * 0.016, -0.025, tip - 0.035, dark, false, 0, i * 0.12);
      box(0.045, 0.004, 0.012, 0, -0.025, tip - 0.07, tint.getHex(), true);
      box(0.03, 0.025, 0.02, 0, -0.025, tip - 0.01, mid);
      break;
    default: // bolt: twin barrels
      cylZ(0.008, 0.01, 0.07, 6, -0.014, -0.025, tip - 0.035, dark);
      cylZ(0.008, 0.01, 0.07, 6, 0.014, -0.025, tip - 0.035, dark);
      cylZ(0.005, 0.005, 0.01, 6, -0.014, -0.025, tip - 0.075, tint.getHex(), true);
      cylZ(0.005, 0.005, 0.01, 6, 0.014, -0.025, tip - 0.075, tint.getHex(), true);
  }
  // side fins, glow strip
  box(0.05, 0.004, 0.03, 0, -0.025, 0.02, dark);
  box(0.004, 0.004, 0.05, 0, -0.049, -0.005, tint.getHex(), true);
  const group = new THREE.Group();
  const mat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, vertexColors: true });
  const emat = new THREE.MeshBasicMaterial({ vertexColors: true });
  const m1 = new THREE.Mesh(mergeGeometries(lit), mat), m2 = new THREE.Mesh(mergeGeometries(emi), emat);
  group.add(m1, m2);
  group.userData.dispose = () => { m1.geometry.dispose(); m2.geometry.dispose(); mat.dispose(); emat.dispose(); };
  return group;
}

// Procedural weapons for NO MANS GOR. Seeded: same (seed, tier) -> same weapon.
// Stats units: dmg per projectile, rate shots/s, spread degrees, count projectiles per shot, speed & range in hull-lengths (L)/s and L.
import { compose, makeMaterials, mulberry as pmul } from './ship-parts.js';

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

// rev 20: damage per second of a weapon (dmg x rate x projectiles per volley) and a one-line summary for chat / the /weapons list.
export function weaponDps(w) { const st = (w && w.stats) || STARTER_WEAPON.stats; return st.dmg * st.rate * st.count; }
export function weaponLine(w) { return String(w.cls || 'C') + ' ' + w.name + ' (' + Math.round(weaponDps(w)) + ' dps' + (w.stats && w.stats.special ? ', ' + w.stats.special : '') + ')'; }
// loot tier by wave: bosses drop one tier above what regular kills give; both cap at S (3)
export function lootTier(wave, boss) { return Math.max(0, Math.min(3, Math.floor((Math.max(1, wave) - (boss ? 0 : 2)) / 4))); }

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

const WPAL = (hex) => ({ base: 0x4A4256, panel: 0x2A2430, accent: hex, glow: hex, dark: 0x1B1722, metal: 0x6A6478 });
function weaponRecipe(weapon, lod) {
  const hex = weapon.color, tip = -0.045, Y = -0.025, kids = [];
  const n = (part, params, o) => kids.push(Object.assign({ part, params: Object.assign({ lod: 'lo' }, params) }, o));      // small attachments always use the cheap variant
  const fwd = [-Math.PI / 2, 0, 0];                                  // part +Y -> hull -Z
  n('pod', { len: 0.1, r: 0.02, glowCol: hex, lod }, { rot: fwd, offset: [0, Y, 0.05] });          // body
  n('plate', { w: 0.026, d: 0.034, th: 0.006, glow: false }, { offset: [0, Y + 0.02, 0.03] });  // tail plate
  n('fin', { len: 0.026, w: 0.034, sweep: 0.5, thick: 0.004, taper: 0.5, tip: hex }, { mirror: 'x', rot: [0, 0, -Math.PI / 2], offset: [0.016, Y, 0.022], id: 'wing' });
  switch (weapon.shape) {
    case 'needle':
      n('barrel', { len: 0.115, r: 0.0055, glowCol: hex }, { rot: fwd, offset: [0, Y, tip + 0.005] });
      break;
    case 'orb':
      n('barrel', { len: 0.026, r: 0.012, glowCol: hex }, { rot: fwd, offset: [0, Y, tip + 0.005] });
      n('eye', { r: 0.017, glowCol: hex }, { rot: fwd, offset: [0, Y, tip - 0.03] });
      break;
    case 'shard':
      for (let i = -1; i <= 1; i++) n('spike', { len: 0.075, r: 0.0075, tip: hex }, { rot: [-Math.PI / 2, 0, -i * 0.14], offset: [i * 0.015, Y, tip + 0.012], id: 'sh' + i });
      break;
    default:   // bolt: twin barrels
      n('barrel', { len: 0.075, r: 0.0085, glowCol: hex }, { rot: fwd, offset: [0.014, Y, tip + 0.005], mirror: 'x', id: 'bar' });
  }
  return { part: 'plate', params: { w: 0.03, d: 0.05, th: 0.012 }, palette: WPAL(hex), lod, flex: false, children: kids };
}
function weaponBuild(THREE, weapon, lod) {
  const res = compose(THREE, weaponRecipe(weapon, lod), pmul(7));
  const mats = makeMaterials(THREE, { flex: false });
  const group = new THREE.Group(), meshes = [];
  const m1 = new THREE.Mesh(res.geo, mats.lit); group.add(m1); meshes.push(m1);
  if (res.emissive) { const m2 = new THREE.Mesh(res.emissive, mats.emissive); group.add(m2); meshes.push(m2); }
  group.userData.dispose = () => { meshes.forEach((m) => m.geometry.dispose()); };     // materials are shared (ship-parts makeMaterials)
  group.userData.triangles = res.tris;
  return group;
}
export function buildWeaponModel(THREE, weapon) {
  const g = weaponBuild(THREE, weapon, 'hi');
  let lo = null;
  Object.defineProperty(g, 'lo', { enumerable: false, configurable: true, get() { return lo || (lo = weaponBuild(THREE, weapon, 'lo')); } });
  return g;
}

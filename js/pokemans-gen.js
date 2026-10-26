// pokemans-gen.js — pure seeded species / individual / type chart / levelling for POKEMANS (docs/next-stage-plan.md A1).
// No Math.random, no DOM, no THREE: every output is a function of its seeds so two clients agree.
//   species(seedPlanet, kind, opts?) -> { id, name, tier, types[2], base{hp,atk,def,spd,sp}, bodyRecipe, moves[4] }
//        opts: { biome: 'lush'|'icy'|'gas'|'rocky'|'lava'|'sea', family: infusion family, tier: 1..5 }  (else derived from the seed)
//   individual(sp, seed)             -> { speciesId, iv{...}, size 0.8-1.3, shiny (1/256), hueFlip }
//   typeChart(universeSeed)          -> { types[8], beats{type:[2]}, weak{type:[2]}, mult(att, def) }
//   xpToLevel(xp) / xpForLevel(lv)   -> level 1..50 (cubic curve)
//   statsAt(sp, ind, level)          -> { hp, atk, def, spd, sp } at that level (iv applied)
import { name as lingoName } from './ship-lingo.js';

export const TYPES = ['leaf', 'frost', 'vapor', 'stone', 'ember', 'brine', 'spark', 'void'];
export const BIOME_TYPE = { lush: 'leaf', icy: 'frost', gas: 'vapor', rocky: 'stone', lava: 'ember', sea: 'brine' };
const BIOMES = Object.keys(BIOME_TYPE);
// infusion family (ship-items.js) -> second type
export const FAMILY_TYPE = { psy: 'void', stim: 'spark', down: 'brine', dis: 'vapor', weird: 'void', cozy: 'ember', glow: 'spark' };
const FAMILIES = Object.keys(FAMILY_TYPE);
export const STATS = ['hp', 'atk', 'def', 'spd', 'sp'];
export const MAX_LEVEL = 50;
export const EFFECTS = ['burn', 'slow', 'shield', 'heal', 'flinch', 'launch', 'drain'];
const VERBS = ['bite', 'lash', 'burst', 'slam', 'rake', 'pulse', 'crash', 'sting', 'surge', 'snap', 'hex', 'rush'];
const TYPE_WORD = { leaf: 'Thorn', frost: 'Rime', vapor: 'Mist', stone: 'Grit', ember: 'Cinder', brine: 'Tide', spark: 'Volt', void: 'Null' };
const PLANS = ['skimmer', 'crawler', 'floater', 'strider', 'burrower', 'swarmer'];
const SIG_PARTS = ['fin', 'spike', 'shell', 'tail', 'horn', 'glow', 'tentacle', 'claw', 'eye'];

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const seedOf = (s) => (typeof s === 'number' ? (s >>> 0) : hashStr(s));
const mix = (a, b) => (Math.imul(a ^ 0x9E3779B9, 0x85EBCA6B) ^ Math.imul(b ^ 0xC2B2AE35, 0x27D4EB2F)) >>> 0;
const ri = (r, lo, hi) => lo + Math.floor(r() * (hi - lo + 1));

export function typeChart(universeSeed) {
  const r = mulberry(seedOf(universeSeed) ^ 0x7C4A11), n = TYPES.length;
  // Each type beats 2 and is weak to 2: take two distinct non-zero offsets o1,o2 from a seeded cyclic order. Type at order position p beats
  // positions p+o1, p+o2; since offsets are fixed per chart, every type is hit by exactly 2 attackers (p-o1, p-o2) -> symmetric by construction.
  const ord = TYPES.slice(); for (let i = n - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); const t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  const o1 = 1 + Math.floor(r() * 3), o2 = 1 + (o1 + Math.floor(r() * 2)) % 3; // two distinct offsets in 1..3: sums < 8, so no pair beats each way
  const beats = {}, weak = {};
  for (const t of TYPES) { beats[t] = []; weak[t] = []; }
  for (let p = 0; p < n; p++) for (const o of [o1, o2]) { const a = ord[p], d = ord[(p + o) % n]; beats[a].push(d); weak[d].push(a); }
  return { types: TYPES.slice(), beats, weak, mult(att, def) { return beats[att] && beats[att].indexOf(def) >= 0 ? 2 : (weak[att] && weak[att].indexOf(def) >= 0 ? 0.5 : 1); } };
}

export function species(seedPlanet, kind, opts) {
  opts = opts || {};
  const sd = mix(seedOf(seedPlanet), seedOf(kind || 'creature')), r = mulberry(sd ^ 0x5eed);
  const biome = BIOME_TYPE[opts.biome] ? opts.biome : BIOMES[ri(r, 0, BIOMES.length - 1)];
  const family = FAMILY_TYPE[opts.family] ? opts.family : FAMILIES[ri(r, 0, FAMILIES.length - 1)];
  const t1 = BIOME_TYPE[biome]; let t2 = FAMILY_TYPE[family]; if (t2 === t1) t2 = TYPES[(TYPES.indexOf(t1) + 1 + ri(r, 0, TYPES.length - 2)) % TYPES.length];
  const tier = Math.max(1, Math.min(5, (opts.tier | 0) || (1 + Math.floor(r() * r() * 5.999))));
  // base stats 20..120: tier sets the band, a seeded split moves points between stats (total budget ~ band)
  const lo = 20 + (tier - 1) * 8, hi = 60 + (tier - 1) * 15, base = {};
  for (const s of STATS) base[s] = Math.max(20, Math.min(120, Math.round(lo + (hi - lo) * Math.pow(r(), 0.8))));
  const nm = lingoName(sd ^ 0x0a11ce);
  const nSig = ri(r, 0, 2), sig = [];
  while (sig.length < nSig) { const p = SIG_PARTS[ri(r, 0, SIG_PARTS.length - 1)]; if (sig.indexOf(p) < 0) sig.push(p); }
  const bodyRecipe = { plan: PLANS[ri(r, 0, PLANS.length - 1)], seed: sd, segs: ri(r, 3, 7), limbs: ri(r, 0, 3) * 2, hue: Math.round(r() * 359), sig };
  // moves: 4 from a procedural pool; first is a STAB (own type) attack, rest mix own types / neutral spark..void
  const moves = [], used = new Set();
  for (let i = 0; i < 4; i++) {
    const ty = i === 0 ? t1 : i === 1 ? t2 : (r() < 0.6 ? (r() < 0.5 ? t1 : t2) : TYPES[ri(r, 0, TYPES.length - 1)]);
    let nmv, g = 0;
    do { nmv = TYPE_WORD[ty] + ' ' + VERBS[ri(r, 0, VERBS.length - 1)]; g++; } while (used.has(nmv) && g < 40);
    used.add(nmv);
    const power = ri(r, 30, 120), acc = Math.max(70, Math.min(100, 100 - Math.round((power - 30) * 0.33) + ri(r, -4, 6)));
    moves.push({ name: nmv, type: ty, power, accuracy: acc, effect: r() < 0.65 ? EFFECTS[ri(r, 0, EFFECTS.length - 1)] : null });
  }
  return { id: sd.toString(16).padStart(8, '0'), name: nm, tier, biome, family, types: [t1, t2], base, bodyRecipe, moves };
}

export function individual(sp, seed) {
  const r = mulberry(mix(seedOf(sp.id), seedOf(seed)) ^ 0x1d1d), iv = {};
  for (const s of STATS) iv[s] = Math.round((r() * 0.30 - 0.15) * 1000) / 1000;
  const size = Math.round((0.8 + r() * 0.5) * 1000) / 1000, shiny = ri(r, 0, 255) === 0;
  return { speciesId: sp.id, iv, size, shiny, hueFlip: shiny };
}

export const xpForLevel = (lv) => { lv = Math.max(1, Math.min(MAX_LEVEL, lv | 0)); return lv <= 1 ? 0 : Math.round(0.8 * lv * lv * lv); };
export function xpToLevel(xp) { let lv = 1; while (lv < MAX_LEVEL && xp >= xpForLevel(lv + 1)) lv++; return lv; }

export function statsAt(sp, ind, level) {
  level = Math.max(1, Math.min(MAX_LEVEL, level | 0)); const o = {}, iv = (ind && ind.iv) || {};
  for (const s of STATS) {
    const b = sp.base[s] * (1 + (iv[s] || 0));
    o[s] = s === 'hp' ? Math.floor(b * 2 * level / 50 + level + 10) : Math.floor(b * 2 * level / 50 + 5);
  }
  return o;
}

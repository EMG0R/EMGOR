// pokemans-fighter.js — pure: a POKEMANS mon as a GOR BRAWL fighter, trainers, and the wild-ring GOR BALL slot (docs/next-stage-plan.md A5). No DOM, no THREE, no Math.random.
//   creatureFighter(mon) -> { fid, def, name, color, hpPct, stocks:1 }   fid = the base sprite (atlas key), def = a full fighter def the engine takes as slot.def
//        body plan -> sprite + movement, ind.size / segs / limbs -> hurtbox + reach, 4 moves -> bN / bF / bD / ftilt (power -> dmg, hit colour = type, effect -> status-ish
//        hit modifier), spd -> run / jump / cooldowns, hp -> weight (knockback resistance) and hpPct (the "percent budget" the wild-ball gate reads)
//   isTrainer(role, seed) / trainerTeam(pid, seed, role, planetTier) -> 1-3 seeded mons (elders: always trainers, with grafts)
//   trainerSpec(team) -> { fid, def, name, color, stocks } the team lead's fighter with one stock per mon
//   ballMove(wild) / withBall(fid, wild) -> the player's def with a GOR BALL throw on down+special, gated to a wild under 50 % (hpPct)
import { FIGHTERS } from './smash-fighters.js';
import { species, individual, statsAt, xpForLevel, MAX_LEVEL, isTrainer } from './pokemans-gen.js';
import { makeMon, catchChance, catchRoll } from './pokemans-catch.js';
import { PARTS, applyGraft } from './pokemans-graft.js';

export const PLAN_BASE = { skimmer: 'twin', crawler: 'ranger', floater: 'jelly', strider: 'racer', burrower: 'sparklet', swarmer: 'hum' };
export const TYPE_COL = { leaf: [0.42, 0.85, 0.42], frost: [0.6, 0.9, 1], vapor: [0.8, 0.8, 0.95], stone: [0.75, 0.65, 0.5], ember: [1, 0.5, 0.2], brine: [0.3, 0.6, 1], spark: [1, 0.9, 0.3], void: [0.7, 0.35, 0.95] };
const RANGED = { spark: 1, vapor: 1 };                  // these types throw a bolt on bN, the rest swing
const SLOTS = ['bN', 'bF', 'bD', 'ftilt'];              // moves 0..3 -> special / side special / down special / forward tilt
const SLOT_ANG = { bN: 40, bF: 30, bD: 75, ftilt: 35 };
export const BALL_GATE = 0.5;                           // a wild can be balled below this fraction of its percent budget

const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const hex2 = (n) => Math.round(clamp(n, 0, 1) * 255).toString(16).padStart(2, '0');
function hsl(h, s, l) { const f = (n) => { const k = (n + h / 30) % 12; return l - s * Math.min(l, 1 - l) * Math.max(-1, Math.min(k - 3, 9 - k, 1)); }; return '#' + hex2(f(0)) + hex2(f(8)) + hex2(f(4)); }
function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const hit = (s, e, seg, r, dmg, bkb, gr, ang, x) => Object.assign({ s, e, seg, r, dmg, bkb, gr, ang }, x || {});

// one mon move -> one engine move (melee swing or bolt). `cdKey` makes the cooldown per slot; effect shapes the hit or adds a tick.
export function buildMove(mon, mv, slot, st) {
  const sp = mon.sp, rec = sp.bodyRecipe, power = clamp(mv.power | 0 || 50, 20, 130), atkF = clamp(st.atk / 55, 0.7, 1.7);
  let dmg = clamp(power / 9, 3, 14) * atkF, bkb = 6 + power * 0.18, gr = 8 + power * 0.45, flinch = true;
  const reach = clamp(22 + rec.segs * 3 + rec.limbs * 2, 24, 50), rad = clamp(6 + sp.tier + rec.limbs * 0.5, 7, 13);
  const s0 = 6 + Math.round((130 - power) / 12), len = 7 + Math.round(power / 14), dur = s0 + len + 8;
  const ef = mv.effect;
  if (ef === 'burn') dmg *= 1.25;
  else if (ef === 'slow') { dmg *= 1.1; bkb *= 0.4; gr *= 0.4; }          // pins them close instead of launching
  else if (ef === 'flinch') bkb = Math.max(bkb, 14);
  else if (ef === 'launch') { bkb *= 1.5; gr *= 1.4; }
  dmg = Math.round(dmg * 10) / 10; bkb = Math.round(bkb * 10) / 10; gr = Math.round(gr * 10) / 10;
  const col = TYPE_COL[mv.type] || null, ang = SLOT_ANG[slot], key = 'm' + slot;
  const cd = Math.round(60 * 60 / Math.max(20, st.spd + 20) * 0.5 * (0.6 + power / 150));        // frames
  const ranged = !!RANGED[mv.type] && slot !== 'ftilt';
  const m = { name: String(mv.name).toLowerCase(), dur, pose: ranged ? 'cast' : 'swing', mon: true, type: mv.type, effect: ef || null, power, cd, hits: [] };
  if (!ranged) m.hits = [hit(s0, s0 + len, [8, -24, 8 + reach, -22], rad, dmg, bkb, gr, ang, { col })];
  m.tick = function (sim, f, t) {
    if (t === 0) { if (f.cd[key] > 0) return true; f.cd[key] = cd; if (ef === 'shield') f.inv = Math.max(f.inv, 18); }
    if (t === s0) {
      if (ranged) { sim.proj(f, { kind: 'bolt', ox: 14, oy: -22, vx: 4.2, vy: 0, g: 0, life: 60, r: 4, dmg, bkb: flinch ? Math.max(bkb * 0.5, 6) : 0, gr: gr * 0.5, ang: 8, col }); sim.sfx('shot'); }
      if (ef === 'heal') f.pct = Math.max(0, f.pct - 5);
      else if (ef === 'drain') f.pct = Math.max(0, f.pct - dmg * 0.4);
    }
  };
  return m;
}

export function creatureFighter(mon) {
  const sp = mon.sp, rec = sp.bodyRecipe, st = statsAt(sp, mon.ind, mon.level), size = (mon.ind && mon.ind.size) || 1;
  const baseId = PLAN_BASE[rec.plan] || 'twin', base = FIGHTERS[baseId], moves = Object.assign({}, base.moves);
  for (let i = 0; i < SLOTS.length; i++) { const mv = mon.learned && mon.learned[i] || sp.moves[i]; if (mv) moves[SLOTS[i]] = buildMove(mon, mv, SLOTS[i], st); }
  const color = hsl(rec.hue, 0.65, 0.58);
  const def = Object.assign({}, base, {
    id: 'mon:' + sp.id, name: String(sp.name).toUpperCase().slice(0, 10), color, moves, mon: true,
    weight: Math.round(clamp(60 + st.hp * 0.6, 70, 130)), run: Math.round(clamp(0.9 + st.spd / 120, 0.9, 2.0) * 100) / 100,
    jump: Math.round(clamp(48 + st.spd / 8, 48, 66)), hurtR: Math.round(clamp(0.75 + rec.segs * 0.06 + (size - 1) * 0.4, 0.8, 1.35) * 100) / 100,
    blurb: sp.types.join(' / ').toUpperCase(), hint: { range: clamp(22 + rec.segs * 3, 24, 44), style: 'allround', ranged: !!RANGED[sp.moves[0] && sp.moves[0].type] },
  });
  return { fid: baseId, def, name: def.name, color, hpPct: Math.round(st.hp * 2.2), stocks: 1, mon };
}
export const hpFrac = (spec, pct) => clamp(1 - pct / Math.max(1, spec.hpPct), 0, 1);

// ── trainers ─────────────────────────────────────────────────────────────────
export { isTrainer };
export function trainerTeam(pid, seed, role, planetTier) {
  const r = mulberry(hashStr(pid + ':' + seed + ':team')), elder = role === 'elder', n = elder ? 3 : 1 + Math.floor(r() * 2.99);
  const BIOMES = ['lush', 'icy', 'rocky', 'gas', 'lava', 'sea'], team = [];
  for (let i = 0; i < n; i++) {
    const tier = clamp((planetTier | 0 || 1) + (elder ? 1 : 0) + (r() < 0.25 ? 1 : 0), 1, 5), lvl = Math.min(MAX_LEVEL, 3 + tier * 3 + (elder ? 3 : 0) + Math.floor(r() * 3));
    const sp = species(pid, 'trainer' + i, { biome: BIOMES[Math.floor(r() * BIOMES.length)], tier }), ind = individual(sp, seed + ':' + i);
    const mon = makeMon(sp, ind, lvl, xpForLevel(lvl), pid);
    if (elder) { const g = 1 + Math.floor(r() * 3); for (let k = 0; k < g; k++) applyGraft(mon, PARTS[Math.floor(r() * PARTS.length)], clamp(tier - 1, 1, 5)); }
    team.push(mon);
  }
  return team;
}
// the lead mon's fighter, one stock per mon (a faint = a KO of the lead)
export function trainerSpec(team) { const s = creatureFighter(team[0]); s.stocks = team.length; s.team = team; return s; }

// ── GOR BALL slot (wild ring fights) ──────────────────────────────────────────
// down+special on the player's def. Throws only when the wild's percent is past BALL_GATE of its budget; otherwise a dud.
// The throw is a request on the sim (sim.ballReq) that ship.js polls and rolls with pokemans-catch; the sim itself never rolls.
export function ballMove(wild) {
  return { name: 'gor ball', dur: 30, pose: 'cast', hits: [], ball: true,
    tick(sim, f, t) {
      if (t === 0 && f.cd.ball > 0) return true;
      if (t === 9) {
        f.cd.ball = 45; const foe = sim.fighters.find((o) => o !== f && o.state !== 'dead' && o.state !== 'out'); if (!foe) return;
        const frac = clamp(1 - foe.pct / Math.max(1, wild.hpPct), 0, 1);
        if (frac < BALL_GATE) { sim.ballReq = { who: f.i, frac, frame: sim.frame }; sim.sfx('shot'); } else sim.events.push({ t: 'puff', x: f.x + f.face * 14, y: f.y - 24 });
      }
    } };
}
export function withBall(fid, wild) { const base = FIGHTERS[fid]; return Object.assign({}, base, { moves: Object.assign({}, base.moves, { bD: ballMove(wild) }) }); }
// the roll for a sim.ballReq: wild = creatureFighter(mon) spec, u in [0,1) from the caller
export function ballRoll(wild, frac, u) { const c = catchChance(wild.mon.sp.tier, frac, 0); return { chance: c, caught: catchRoll(c, u).caught }; }

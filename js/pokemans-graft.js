// pokemans-graft.js — pure graft / food-tutor / bond maths for POKEMANS pass 2a (docs/next-stage-plan.md A4). No DOM, no THREE, no Math.random:
// every roll takes its uniform u in [0,1) from the caller.
//   PARTS                         fin +spd · spike +atk · shell +def · glow +sp · lungs +hp  (stat per part lives in pokemans-gen GRAFT_STAT)
//   partItem(part, tier)          -> inventory item (kind 'graft') ; partDrop(tier, source, u1, u2) -> { part, tier } | null
//   graftCost(mon, tier) / canGraft(mon, tier, gor) / applyGraft(mon, part, tier) -> { ok, why, cost }   max GRAFT_MAX per mon, never mutates sp
//   bodyOf(mon)                   -> bodyRecipe copy with graft parts added to sig (visible silhouette)
//   foodType(item) / teachRoll(mon, type, u1, u2) / learn(mon, prop)   foods teach moves: 30 % per matching type, replaces the weakest move
//   bondAdd(mon, amount) / bondTier(mon) / critChance(mon) / sigMove(mon)   bond 10 = +5 % crit, bond 30 = a 5th signature move
import { FAMILY_TYPE, GRAFT_STAT, GRAFT_MAX, makeMove } from './pokemans-gen.js';

export { GRAFT_MAX };
export const PARTS = Object.keys(GRAFT_STAT);               // fin spike shell glow lungs
export const PART_WORD = { fin: 'FIN', spike: 'SPIKE', shell: 'SHELL', glow: 'GLOW ORGAN', lungs: 'LUNGS' };
export const TEACH_CHANCE = 0.3;
export const BOND_T1 = 10, BOND_T2 = 30, BOND_CRIT = 0.05;
export const BOND_PER_MIN = 1, BOND_PER_WIN = 2;

export function partItem(part, tier) {
  tier = Math.max(1, Math.min(5, tier | 0 || 1));
  const id = 'graft-' + part + '-' + tier;
  return { id, seed: 3000 + PARTS.indexOf(part) * 10 + tier, name: PART_WORD[part] + ' T' + tier, kind: 'graft', base: part, infusion: null, family: null, modifier: null, part, tier, price: 0,
    blurb: '+' + (GRAFT_STAT[part] === 'hp' ? 'HP' : GRAFT_STAT[part].toUpperCase()) + ' · graft it onto a pokemans at a POKEMANS counter', color: 0xFF7A8A, absurdity: 0, dealer: false, blurbs: [], stackKey: id, category: 'graftpart', tags: ['graftpart'], iconHint: 'candy' };
}
// source: 'kill' 35 %, 'catch' 100 %, 'resource' 20 %. Tier of the part = creature tier (resource nodes pass tier 1..2). Which part: second uniform.
export function partDrop(tier, source, u1, u2) {
  const p = source === 'catch' ? 1 : source === 'resource' ? 0.2 : 0.35;
  if (u1 >= p) return null;
  return { part: PARTS[Math.min(PARTS.length - 1, Math.floor(u2 * PARTS.length))], tier: Math.max(1, Math.min(5, tier | 0 || 1)) };
}
export const graftCount = (mon) => (mon.ind && mon.ind.grafts ? mon.ind.grafts.length : 0);
export const graftCost = (mon, tier) => 10 + 12 * (graftCount(mon) + 1) + 8 * Math.max(1, tier | 0);
export function canGraft(mon, tier, gor) {
  if (!mon) return { ok: false, why: 'empty', cost: 0 };
  if (graftCount(mon) >= GRAFT_MAX) return { ok: false, why: 'full', cost: 0 };
  const cost = graftCost(mon, tier);
  return (gor | 0) < cost ? { ok: false, why: 'poor', cost } : { ok: true, why: '', cost };
}
// grafts live on the individual (ind.grafts = [{p, t}]) so two of one species diverge; gen.statsAt reads them. Does not charge: caller subtracts cost.
export function applyGraft(mon, part, tier) {
  if (!GRAFT_STAT[part]) return { ok: false, why: 'part', cost: 0 };
  const c = canGraft(mon, tier, 1e9); if (!c.ok) return c;
  if (!mon.ind.grafts) mon.ind.grafts = [];
  mon.ind.grafts.push({ p: part, t: Math.max(1, Math.min(5, tier | 0 || 1)) });
  return { ok: true, why: '', cost: c.cost };
}
export function bodyOf(mon) {
  const b = mon.sp.bodyRecipe, g = (mon.ind && mon.ind.grafts) || [], sig = b.sig.slice();
  for (let i = 0; i < g.length; i++) sig.push(g[i].p);
  return Object.assign({}, b, { sig, grafts: g.length });
}
// foods: item.family -> type. Only types the mon owns can teach. Returns a proposal { slot, move } or null; mon is untouched until learn().
export const foodType = (it) => (it && it.family && FAMILY_TYPE[it.family]) || null;
export function weakestSlot(mon) { let b = 0; for (let i = 1; i < mon.sp.moves.length; i++) if (mon.sp.moves[i].power < mon.sp.moves[b].power) b = i; return b; }
export function movesOf(mon) { return mon.learned ? mon.sp.moves.map((m, i) => mon.learned[i] || m) : mon.sp.moves; }
export function teachRoll(mon, type, u1, u2) {
  if (!type || mon.sp.types.indexOf(type) < 0 || u1 >= TEACH_CHANCE) return null;
  const slot = weakestSlot(mon), cur = movesOf(mon);
  return { slot, move: makeMove(type, Math.imul((mon.xp | 0) + 1, 2654435761) ^ Math.floor(u2 * 1e9) ^ (graftCount(mon) << 8)), old: cur[slot] };
}
// confirm: the learned move replaces slot k for this individual only (mon.learned[k]); species stay shared.
export function learn(mon, prop) { if (!prop) return false; if (!mon.learned) mon.learned = {}; mon.learned[prop.slot] = prop.move; return true; }
export function bondAdd(mon, amount) { const b0 = mon.bond || 0; mon.bond = Math.min(99, b0 + Math.max(0, amount)); return (mon.bond >= BOND_T1 && b0 < BOND_T1) ? 1 : (mon.bond >= BOND_T2 && b0 < BOND_T2) ? 2 : 0; }
export const bondTier = (mon) => ((mon.bond || 0) >= BOND_T2 ? 2 : (mon.bond || 0) >= BOND_T1 ? 1 : 0);
export const critChance = (mon) => (bondTier(mon) >= 1 ? BOND_CRIT : 0);
// the 5th signature move: seeded from the species, own first type, power in the top of the pool; null below bond 30
export function sigMove(mon) { return bondTier(mon) >= 2 ? makeMove(mon.sp.types[0], parseInt(mon.sp.id, 16) ^ 0x51671, 110) : null; }

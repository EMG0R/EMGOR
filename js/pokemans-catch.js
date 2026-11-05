// pokemans-catch.js — pure GOR BALL throw + catch maths for POKEMANS (docs/next-stage-plan.md A2). No DOM, no THREE, no Math.random.
//   catchChance(tier, hpFrac [, bonus])  -> 0.1..1   tier(1-3)·(1-hp%)·0.9 + 0.1 (+ lure bonus), clamped
//   catchRoll(chance, u)                 -> { shakes: 3, caught }   three shakes, then 'click' (caught) or 'burst' (escaped)
//   arcPoint(o, d, t, out) / previewDots(o, d, out)   ballistic arc in H (human heights) along d (unit) with up (0,1,0 in the caller's frame)
//   makeMon(sp, ind, level, xp)          -> the full-data record stored on pokemans[] / inventory stack (kind 'pokemans')
import { statsAt, xpToLevel } from './pokemans-gen.js';

export const BALL = { speed: 15, lift: 0.28, g: 12, reach: 12, dots: 8, tMax: 2.2, radius: 0.35 };     // H, H/s, H/s²: reach = max straight distance from the hand
export const PARTY_MAX = 6;
export const LURE_BONUS = 0.15;

export function catchChance(tier, hpFrac, bonus) {
  const t = Math.max(1, Math.min(3, tier | 0 || 1)), h = hpFrac === hpFrac ? Math.max(0, Math.min(1, hpFrac)) : 1;
  return Math.max(0.1, Math.min(1, t * (1 - h) * 0.9 + 0.1 + (bonus || 0)));
}
export function catchRoll(chance, u) { return { shakes: 3, caught: u < chance }; }

// Q42 ring-fight catch: after a wild mon is KO'd in the ring the player has KO_WINDOW seconds to throw a GOR BALL at it.
export const KO_WINDOW = 3;
export const koOpen = (tKo, now) => now >= tKo && now - tKo <= KO_WINDOW;
// remaining hp fraction of a ring fighter: pct = damage taken, budget = creatureFighter().hpPct (a KO has pct >= budget -> 0)
export const wildHpFrac = (pct, budget) => Math.max(0, Math.min(1, 1 - (pct || 0) / Math.max(1, budget || 1)));
export const ringCatchChance = (ballTier, hpFrac, bonus) => catchChance(ballTier, hpFrac, bonus);

// o = {x,y,z} hand, d = {x,y,z} unit aim, up = {x,y,z} unit. Position at time t of a ball thrown at BALL.speed with BALL.lift added along up.
export function arcPoint(o, d, up, t, out) {
  const vx = d.x * BALL.speed + up.x * BALL.lift * BALL.speed, vy = d.y * BALL.speed + up.y * BALL.lift * BALL.speed, vz = d.z * BALL.speed + up.z * BALL.lift * BALL.speed, h = 0.5 * BALL.g * t * t;
  out.x = o.x + vx * t - up.x * h; out.y = o.y + vy * t - up.y * h; out.z = o.z + vz * t - up.z * h;
  return out;
}
// 8 dots along the arc, evenly spaced in time up to the reach cut-off; `out` = array of >= BALL.dots {x,y,z}; returns the dot count used
export function previewDots(o, d, up, out) {
  const T = Math.min(BALL.tMax, BALL.reach / (BALL.speed * Math.sqrt(1 + BALL.lift * BALL.lift)));
  for (let i = 0; i < BALL.dots; i++) arcPoint(o, d, up, T * (i + 1) / BALL.dots, out[i]);
  return BALL.dots;
}
export const throwTime = () => Math.min(BALL.tMax, BALL.reach / (BALL.speed * Math.sqrt(1 + BALL.lift * BALL.lift)));

export function makeMon(sp, ind, level, xp, planet) {
  level = Math.max(1, level | 0 || 1); xp = xp | 0;
  const stats = statsAt(sp, ind, level);
  return { id: 'mon-' + sp.id + '-' + Math.round((ind.size || 1) * 1000) + (ind.shiny ? 's' : ''), sp, ind, level, xp, hp: stats.hp, dead: false, planet: String(planet || ''), at: 0 };
}
export const monLevel = (m) => xpToLevel(m && m.xp | 0);

// Q48: world entry for a wild ring fight. list = [{ id, lp:{x,y,z} planet-local }], pos = the player's planet-local position, range = 3 H in the same units.
// Returns the index of the nearest creature strictly inside range, or -1. Allocation-free.
export function wildEncounterIndex(list, pos, range) {
  let best = -1, bd = range * range;
  if (!list || !pos || !(range > 0)) return -1;
  for (let i = 0; i < list.length; i++) {
    const p = list[i] && list[i].lp; if (!p) continue;
    const dx = p.x - pos.x, dy = p.y - pos.y, dz = p.z - pos.z, d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < bd) { bd = d2; best = i; }
  }
  return best;
}

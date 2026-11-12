// pokemans-catch.js — pure GOR BALL throw + catch maths for POKEMANS (docs/next-stage-plan.md A2). No DOM, no THREE, no Math.random.
//   battleCatchChance(tier, hp, maxHp [, bonus])  the battle BALL odds (pokemans-battle.js); catchChance = legacy curve still read by pokemans-fighter
//   catchChance(tier, hpFrac [, bonus])  -> 0.1..1   tier(1-3)·(1-hp%)·0.9 + 0.1 (+ lure bonus), clamped
//   catchRoll(chance, u)                 -> { shakes: 3, caught }   three shakes, then 'click' (caught) or 'burst' (escaped)
//   arcPoint(o, d, t, out) / previewDots(o, d, out)   ballistic arc in H (human heights) along d (unit) with up (0,1,0 in the caller's frame)
//   makeMon(sp, ind, level, xp)          -> the full-data record stored on pokemans[] / inventory stack (kind 'pokemans')
import { statsAt, xpToLevel } from './pokemans-gen.js';

export const BALL = { speed: 15, lift: 0.28, g: 12, reach: 20, dots: 8, tMax: 2.2, radius: 0.35 };     // H, H/s, H/s²: reach = max straight distance from the hand
export const PARTY_MAX = 6;
export const LURE_BONUS = 0.15;

export function catchChance(tier, hpFrac, bonus) {
  const t = Math.max(1, Math.min(3, tier | 0 || 1)), h = hpFrac === hpFrac ? Math.max(0, Math.min(1, hpFrac)) : 1;
  return Math.max(0.1, Math.min(1, Math.max(t * (1 - h) * 0.9 + 0.1, FLOOR[t - 1]) + (bonus || 0)));
}
export const FLOOR = [0.35, 0.45, 0.55];      // full-hp floor per ball tier (grazers always roll at hp 1); lower hp still raises it
export const AIM_CONE = 20 * Math.PI / 180, AIM_REACH = 20, HINT_RANGE = 25;
export const CALM_SECS = 2, BODY_K = 0.5, GRAZER_K = 1.5;      // body hit radius = BODY_K x render size (x GRAZER_K for grazers)
export const CATCHABLE = { grazer: 1, jelly: 1, predator: 1 };
// pure encounter predicate: dist in H, kind 'grazer'|'jelly'|'predator' (villagers/npcs never), st = {tamed, held, dead}
export function encounterOk(dist, kind, st, range) {
  if (!CATCHABLE[kind] || !(dist >= 0)) return false;
  if (st && (st.tamed || st.held || st.dead)) return false;
  return dist < (range > 0 ? range : 3);
}
// ball-hit: segment a->b (cur - prev) vs sphere (c, r); all scalars, allocation-free
export function segSphere(ax, ay, az, bx, by, bz, cx, cy, cz, r) {
  const dx = bx - ax, dy = by - ay, dz = bz - az, l2 = dx * dx + dy * dy + dz * dz;
  let t = l2 > 1e-12 ? ((cx - ax) * dx + (cy - ay) * dy + (cz - az) * dz) / l2 : 0; t = t < 0 ? 0 : t > 1 ? 1 : t;
  const px = ax + dx * t - cx, py = ay + dy * t - cy, pz = az + dz * t - cz;
  return px * px + py * py + pz * pz < r * r;
}
// Aim solve: offset T (H, hand -> target body) and unit up; returns flight time t (s) so arcPoint(...,t) == T and writes the unit aim into out, or -1 (unreachable: out = straight at T).
export function solveAim(Tx, Ty, Tz, up, out) {
  const S = BALL.speed, Lf = BALL.lift, G = BALL.g, tEnd = Math.min(BALL.tMax, BALL.reach / (S * Math.sqrt(1 + Lf * Lf)) * 1.15);
  const f = (t) => {
    const k = 0.5 * G * t;
    const ax = (Tx / t + up.x * k) / S - Lf * up.x, ay = (Ty / t + up.y * k) / S - Lf * up.y, az = (Tz / t + up.z * k) / S - Lf * up.z;
    return Math.sqrt(ax * ax + ay * ay + az * az) - 1;
  };
  let t0 = 0.05, f0 = f(t0), t1 = -1;
  for (let t = 0.07; t <= tEnd; t += 0.02) { const f1 = f(t); if (f0 > 0 && f1 <= 0) { t1 = t; break; } t0 = t; f0 = f1; }
  if (t1 < 0) { const l = Math.sqrt(Tx * Tx + Ty * Ty + Tz * Tz) || 1; out.x = Tx / l; out.y = Ty / l; out.z = Tz / l; return -1; }
  let lo = t0, hi = t1; for (let i = 0; i < 24; i++) { const m = 0.5 * (lo + hi); if (f(m) > 0) lo = m; else hi = m; }
  const t = 0.5 * (lo + hi), k = 0.5 * G * t;
  let ax = (Tx / t + up.x * k) / S - Lf * up.x, ay = (Ty / t + up.y * k) / S - Lf * up.y, az = (Tz / t + up.z * k) / S - Lf * up.z;
  const l = Math.sqrt(ax * ax + ay * ay + az * az) || 1; out.x = ax / l; out.y = ay / l; out.z = az / l;
  return t;
}
export function catchRoll(chance, u) { return { shakes: 3, caught: u < chance }; }

// Battle catch (the only catch path): chance = tierFloor x (1 - hp/maxHp x 0.8) (+ lure bonus), shown as a percentage on the BALL option.
// tier 1..3 = ball tier. A full-hp wild keeps 20 % of the floor; a 1-hp wild keeps ~100 % of it.
export function battleCatchChance(tier, hp, maxHp, bonus) {
  const t = Math.max(1, Math.min(3, tier | 0 || 1)), f = maxHp > 0 ? Math.max(0, Math.min(1, hp / maxHp)) : 1;
  return Math.max(0, Math.min(1, FLOOR[t - 1] * (1 - f * 0.8) + (bonus || 0)));
}

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

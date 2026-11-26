// pokemans-wild.js — pure real-time wild-encounter rules for POKEMANS (no DOM, no THREE, no Math.random: callers pass u in 0..1).
//   One mechanism for wild fights: the player's blaster hurts a wild (wildHit), it aggros, fights back, flees only when nearly dead or after a
//   quiet spell; a GOR BALL then catches it with a chance that rises as its HP falls (liveCatchChance), resolved per shake (shakeRoll).
//   wildMaxHp(mon) / wildBlasterDamage / wildPlayerDamage / wildStyle / wildStep / partnerClamp / liveCatchChance / shakeRoll / aimLock
import { statsAt } from './pokemans-gen.js';

export const WILD = {
  hpBase: 60, hpPerStat: 6,            // wild max HP = 60 + 6 x species hp stat at its level (L3 ~ 150; a starter blaster shot is ~12)
  alertH: 8,                            // H: inside this a wild stops and watches (no flee, no attack)
  fleeBelow: 0.10,                      // HP fraction under which a wild runs
  calmAfter: 25,                        // s without being hit and it stands down
  fleeSecs: 8, chaseSecs: 6,            // mode lifetimes handed to the planet AI
  meleeH: 2.2, rangedH: 14,             // H from the body edge (melee) / from the body centre (ranged hold distance)
  atkCd: { melee: 1.6, ranged: 2.4 },   // s between attacks
  dmgBase: 4, dmgPerLevel: 2.2,         // player damage = base + level x per, x style, x (1 + 0.1 (tier-1))
  nameH: 25, nameRangeH: 25             // chip appears within this many H (or when aimed at)
};
export const AGGRO = { idle: 0, chase: 1, flee: 2 };

export function wildMaxHp(mon) { const hp = statsAt(mon.sp, mon.ind, mon.level).hp; return Math.round(WILD.hpBase + WILD.hpPerStat * hp); }
export const newWild = (mon) => { const max = wildMaxHp(mon); return { hp: max, max, aggro: false, flee: false, fleeT: 0, since: 99, cd: 1.0, hits: 0 }; };
export const hpFrac = (w) => (w.max > 0 ? Math.max(0, Math.min(1, w.hp / w.max)) : 0);

// species style: floaters / skitterers spit from range, the rest lunge. beh = planet behaviour name ('grazer'|'skitterer'|'floater'|'strider'), kind 0..3 (walker, jelly, ox, strider)
export function wildStyle(beh, kind) { return beh === 'floater' || beh === 'skitterer' || kind === 1 ? 'ranged' : 'melee'; }

// blaster dmg (what ship.js footFireTick computes) onto a wild; returns the damage dealt (>0) and wakes it up
export function wildHit(w, dmg, partner) {
  if (!(dmg > 0) || w.hp <= 0) return 0;
  let d = dmg;
  if (partner) d = partnerClamp(w, dmg);
  d = Math.min(d, Math.max(0, w.hp - 1));      // the blaster never kills: a wild bottoms out at 1 HP (catchable, flees under 10 %)
  w.hp -= d; w.since = 0; w.hits++;
  if (!partner || d > 0) { if (!w.flee) w.aggro = true; }
  if (hpFrac(w) < WILD.fleeBelow) { w.flee = true; w.aggro = false; w.fleeT = WILD.fleeSecs; }
  return d;
}
// a partner never finishes the job: it cannot take a wild below 5 % HP (the player's catch)
export function partnerClamp(w, dmg) { const floor = Math.ceil(w.max * 0.05); return Math.max(0, Math.min(dmg, w.hp - floor)); }

// damage a wild deals to the player (player HP ~100+): scales with the wild's level
export function wildPlayerDamage(level, tier, style) {
  const d = (WILD.dmgBase + WILD.dmgPerLevel * Math.max(1, level | 0)) * (1 + 0.1 * (Math.max(1, tier | 0) - 1));
  return Math.round(d * (style === 'ranged' ? 0.8 : 1));
}

// per-frame state step. distH = player distance in H from the body centre. Returns { mode: 0|1|2, attack: ''|'melee'|'ranged' }.
export function wildStep(w, dt, distH, style, edgeH) {
  w.since += dt; w.cd -= dt;
  const out = { mode: AGGRO.idle, attack: '' };
  if (w.flee) {      // low HP (< 10 %) or lost interest: run; stands down after calmAfter s unhit (still low, still catchable)
    if (w.fleeT > 0) w.fleeT -= dt;
    if (w.since > WILD.calmAfter + 5 || (w.fleeT <= 0 && hpFrac(w) >= WILD.fleeBelow * 1.5)) { w.flee = false; w.fleeT = 0; }
    else { out.mode = AGGRO.flee; return out; }
  }
  if (w.aggro && w.since > WILD.calmAfter) { w.aggro = false; w.flee = true; w.fleeT = WILD.fleeSecs; out.mode = AGGRO.flee; return out; }      // ~25 s without a hit: loses interest and runs off
  if (!w.aggro) return out;
  out.mode = AGGRO.chase;
  const reach = style === 'ranged' ? WILD.rangedH : (edgeH || 0) + WILD.meleeH;
  if (distH <= reach && w.cd <= 0) { out.attack = style; w.cd = WILD.atkCd[style] || 2; }
  return out;
}
export const wildStopH = (style, edgeH) => (style === 'ranged' ? WILD.rangedH * 0.7 : (edgeH || 0) + WILD.meleeH * 0.7);

// ── catch ──
export const BALL_FLOOR = [0.05, 0.09, 0.15], BALL_MUL = [1, 1.12, 1.25];
// ballTier floor + (1 - hp%)^1.5 x 0.9 x tier multiplier, a species-tier penalty and the lure bonus; full HP common ~5 %, < 20 % HP ~70-90 % on a basic ball
export function liveCatchChance(tier, hpf, speciesTier, bonus) {
  const t = Math.max(1, Math.min(3, tier | 0 || 1)), h = hpf === hpf ? Math.max(0, Math.min(1, hpf)) : 1;
  const pen = 1 - 0.06 * (Math.max(1, Math.min(5, speciesTier | 0 || 1)) - 1);
  return Math.max(0, Math.min(0.99, (BALL_FLOOR[t - 1] + Math.pow(1 - h, 1.5) * 0.9 * BALL_MUL[t - 1]) * pen + (bonus || 0)));
}
// the chance is resolved per shake: each shake holds with chance^(1/3). us = 3 uniform draws. Returns { shakes: 1..3, caught }.
export function shakeRoll(chance, us) {
  const p = Math.pow(Math.max(0, Math.min(1, chance)), 1 / 3);
  for (let i = 0; i < 3; i++) if (!((us && us[i] !== undefined ? us[i] : 1) < p)) return { shakes: i + 1, caught: false };
  return { shakes: 3, caught: true };
}
// aim lock: nearest in a cone. list [{dx,dy,dz (offset from the camera), id}], dir = unit camera ray, coneRad, reach (same units). Returns index or -1.
export function aimLock(list, dir, coneRad, reach) {
  let best = -1, bd = reach * reach, c = Math.cos(coneRad);
  for (let i = 0; i < list.length; i++) {
    const o = list[i], d2 = o.dx * o.dx + o.dy * o.dy + o.dz * o.dz; if (d2 >= bd || d2 < 1e-9) continue;
    if ((o.dx * dir.x + o.dy * dir.y + o.dz * dir.z) < c * Math.sqrt(d2)) continue;
    bd = d2; best = i;
  }
  return best;
}

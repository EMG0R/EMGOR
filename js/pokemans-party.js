// pokemans-party.js — party rules (pure, no THREE / DOM / Math.random): active follower, tiny fight AI, XP, healing, fainting.
//   active(party, mons)            -> the active mon (first living party mon unless profile.activeMon names one) or null
//   pickActive(pf, i)              -> swap to party slot i (0.4 s poof handled by the caller); refuses DEAD / empty slots
//   chooseMove(mon, foe, chart, cds, u) -> index of one of the 4 moves (best expected damage among those off cooldown) or -1
//   moveDamage(mon, move, foe, chart)  -> integer damage (level, atk/sp vs def, type chart, STAB)
//   moveCooldown(mon)              -> seconds; 1 / spd-scaled so faster mons swing more often
//   awardXp(mon, amount)           -> levels gained (hp rescaled, never heals a dead mon)
//   xpForKill(foeLevel, foeTier) / xpForCatch(tier, level)
//   healParty(pf) / faint(mon) / revive(mon) / reviveCost(mon) -> survive a battle = full heal; faint = DEAD until revived at the CVS
import { statsAt, xpToLevel, MAX_LEVEL } from './pokemans-gen.js';

export const SWAP_POOF = 0.4;
export const MOVE_CD_BASE = 1;
export const monById = (pf, id) => { for (let i = 0; i < pf.pokemans.length; i++) if (pf.pokemans[i].id === id) return pf.pokemans[i]; return null; };
export const maxHp = (mon) => statsAt(mon.sp, mon.ind, mon.level).hp;

export function active(pf) {
  const want = pf.activeMon && monById(pf, pf.activeMon);
  if (want && !want.dead && pf.party.indexOf(want.id) >= 0) return want;
  for (let i = 0; i < pf.party.length; i++) { const m = monById(pf, pf.party[i]); if (m && !m.dead) return m; }
  return null;
}
export function pickActive(pf, i) {
  const m = pf.party[i] ? monById(pf, pf.party[i]) : null;
  if (!m || m.dead) return null;
  pf.activeMon = m.id; return m;
}
export function moveCooldown(mon) { const s = statsAt(mon.sp, mon.ind, mon.level); return MOVE_CD_BASE * 60 / Math.max(20, s.spd + 20); }
export function moveDamage(mon, move, foe, chart) {
  const a = statsAt(mon.sp, mon.ind, mon.level), special = move.type === mon.sp.types[0] || move.type === mon.sp.types[1];
  const atk = special ? a.sp : a.atk, def = foe.def || 30, lv = mon.level;
  let mult = 1;
  if (chart && foe.types) { for (let i = 0; i < foe.types.length; i++) mult *= chart.mult(move.type, foe.types[i]); }
  const stab = special ? 1.25 : 1;
  const base = ((2 * lv / 5 + 2) * move.power * atk / Math.max(10, def)) / 20 + 2;
  return Math.max(1, Math.round(base * mult * stab * (move.accuracy >= 100 ? 1 : (0.9 + 0.1 * move.accuracy / 100))));
}
export function chooseMove(mon, foe, chart, cds, u) {
  let best = -1, bd = -1;
  for (let i = 0; i < mon.sp.moves.length; i++) {
    if (cds && cds[i] > 0) continue;
    const d = moveDamage(mon, mon.sp.moves[i], foe, chart) * (0.85 + 0.3 * ((u * 7 + i * 0.37) % 1));
    if (d > bd) { bd = d; best = i; }
  }
  return best;
}
export function awardXp(mon, amount) {
  if (mon.dead || amount <= 0) return 0;
  const before = mon.level, mh0 = maxHp(mon);
  mon.xp = (mon.xp | 0) + Math.round(amount); mon.level = Math.min(MAX_LEVEL, xpToLevel(mon.xp));
  if (mon.level > before) { const mh1 = maxHp(mon); mon.hp = Math.min(mh1, mon.hp + (mh1 - mh0)); }
  return mon.level - before;
}
export const xpForKill = (foeLevel, foeTier) => Math.round(8 + 6 * Math.max(1, foeLevel | 0) + 10 * Math.max(1, foeTier | 0));
export const xpForCatch = (tier, level) => Math.round(20 + 8 * Math.max(1, level | 0) + 12 * Math.max(1, tier | 0));
export function damage(mon, n) {            // returns true if this hit fainted it
  if (mon.dead) return false; mon.hp = Math.max(0, mon.hp - Math.round(n));
  if (mon.hp <= 0) { faint(mon); return true; } return false;
}
export function faint(mon) { mon.hp = 0; mon.dead = true; }
export const reviveCost = (mon) => 10 + 4 * (mon.level | 0);
export function revive(mon) { if (!mon.dead) return false; mon.dead = false; mon.hp = maxHp(mon); return true; }
// Q14 POKEMANS CVS: REVIVE counter. First DEAD mon (party first, then pen) is revived for reviveCost gorCoin (10 + 4·level). Mutates pf.gor + the mon.
// Returns { ok, why: ''|'none'|'poor', mon, cost }.
export function reviveAtCvs(pf) {
  let mon = null, i, m;
  for (i = 0; i < pf.party.length && !mon; i++) { m = monById(pf, pf.party[i]); if (m && m.dead) mon = m; }
  for (i = 0; pf.pen && i < pf.pen.length && !mon; i++) { m = monById(pf, pf.pen[i]); if (m && m.dead) mon = m; }
  if (!mon) return { ok: false, why: 'none', mon: null, cost: 0 };
  const cost = reviveCost(mon);
  if ((pf.gor | 0) < cost) return { ok: false, why: 'poor', mon, cost };
  pf.gor -= cost; revive(mon);
  return { ok: true, why: '', mon, cost };
}
export function healParty(pf) { let n = 0; for (let i = 0; i < pf.party.length; i++) { const m = monById(pf, pf.party[i]); if (m && !m.dead) { m.hp = maxHp(m); n++; } } return n; }
// summon / dismiss rules: DEAD can't be summoned; returns {ok, why}
export function canSummon(mon) { return !mon ? { ok: false, why: 'empty' } : mon.dead ? { ok: false, why: 'dead' } : { ok: true, why: '' }; }
// ─── Q13b: radial move pick, forced-move AI hand-off, party/pen manager ───
// radialPick(dx, dy, dead): 4-move radial, mouse delta since T went down. 0 = up, 1 = right, 2 = down, 3 = left; -1 inside the dead zone
export function radialPick(dx, dy, dead) {
  if (dx * dx + dy * dy < (dead === undefined ? 20 : dead) ** 2) return -1;
  return Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
}
export const FORCE_WAIT = 3;               // a player-picked move waits at most this long for its cooldown, then the AI resumes
// nextMove: the player's pick (forced, -1 = none) wins when off cooldown; while it cools the AI holds fire (age < FORCE_WAIT); otherwise the AI
// returns {k, keep}: k = move index or -1 (nothing this tick), keep = the forced pick is still pending
export function nextMove(mon, foe, chart, cds, u, forced, age) {
  if (forced >= 0 && forced < mon.sp.moves.length && age < FORCE_WAIT) {
    if (!(cds && cds[forced] > 0)) return { k: forced, keep: false };
    return { k: -1, keep: true };
  }
  return { k: chooseMove(mon, foe, chart, cds, u), keep: false };
}
// manager: move a mon between party and pen. Returns {ok, why, to}. A party must keep one living mon; the active mon is repaired after.
export function moveToggle(pf, id, partyMax) {
  const mon = monById(pf, id); if (!mon) return { ok: false, why: 'unknown' };
  const pi = pf.party.indexOf(id);
  if (pi >= 0) {
    const others = pf.party.filter((x) => x !== id).some((x) => { const m = monById(pf, x); return m && !m.dead; });
    if (!others && !mon.dead) return { ok: false, why: 'last' };
    pf.party.splice(pi, 1); pf.pen.push(id);
    if (pf.activeMon === id) pf.activeMon = '';
    return { ok: true, to: 'pen' };
  }
  const ni = pf.pen.indexOf(id); if (ni < 0) return { ok: false, why: 'unknown' };
  if (pf.party.length >= partyMax) return { ok: false, why: 'full' };
  pf.pen.splice(ni, 1); pf.party.push(id);
  return { ok: true, to: 'party' };
}
// setActiveId: make a specific party mon the active one (refuses DEAD / not in party)
export function setActiveId(pf, id) { const i = pf.party.indexOf(id); return i < 0 ? null : pickActive(pf, i); }

// Keep at most `max` mon records: drop the oldest that are NOT in the party or pen first; only if every record is in use does the oldest go,
// and its id is then removed from party/pen so no slot dangles on a missing record.
export function trimMons(pf, max) {
  const keep = new Set((pf.party || []).concat(pf.pen || []));
  while (pf.pokemans.length > max) {
    let i = 0;
    while (i < pf.pokemans.length && keep.has(pf.pokemans[i].id)) i++;
    if (i >= pf.pokemans.length) { i = 0; const id = pf.pokemans[0].id; pf.party = (pf.party || []).filter((x) => x !== id); pf.pen = (pf.pen || []).filter((x) => x !== id); keep.delete(id); }
    pf.pokemans.splice(i, 1);
  }
  return pf;
}

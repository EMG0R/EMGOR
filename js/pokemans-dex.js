// pokemans-dex.js — DEX + party view-models for POKEMANS (pure: no THREE, no DOM, no Math.random).
//   note(pf, sp, planetId)   -> true if this species was new to the dex (pf.dex.seen[id] = compact species record, first planet kept)
//   entries(pf)              -> [{ id, name, tier, types, base, moves, rec, planet, caught, count, best, grafts{part:n} }] sorted planet, tier, name
//                               (a caught mon is always seen: species of pf.pokemans appear even if note() never ran)
//   groups(list)             -> [{ planet, list }] in first-appearance order
//   partyRow(pf, mon, maxHp) -> { id, name, level, hp, max, frac, dead, bond, bondTier, grafts, active, types }
//   iconGrid(rec)            -> 256 chars (16x16): '.' clear, 'O' outline, 'd' 'b' 'l' 'h' body shades, 'e' eye, 'f' fin, 's' spike, 'c' shell, 'g' glow, 'p' lungs
// The grid is the same silhouette for the same recipe (seeded from rec.seed), so ship-icons only has to colour it.
import { bondTier } from './pokemans-graft.js';

const mulberry = (a) => () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const SEEN_MAX = 400;

const compact = (sp, planet) => ({
  id: sp.id, name: sp.name, tier: sp.tier, types: sp.types.slice(), base: Object.assign({}, sp.base),
  moves: sp.moves.map((m) => ({ name: m.name, type: m.type, power: m.power })),
  rec: { plan: sp.bodyRecipe.plan, seed: sp.bodyRecipe.seed, segs: sp.bodyRecipe.segs, limbs: sp.bodyRecipe.limbs, hue: sp.bodyRecipe.hue, sig: sp.bodyRecipe.sig.slice() },
  planet: String(planet == null ? '?' : planet),
});

export function note(pf, sp, planetId) {
  if (!pf.dex || typeof pf.dex !== 'object') pf.dex = { seen: {} };
  if (!pf.dex.seen) pf.dex.seen = {};
  if (pf.dex.seen[sp.id]) return false;
  if (Object.keys(pf.dex.seen).length >= SEEN_MAX) return false;
  pf.dex.seen[sp.id] = compact(sp, planetId);
  return true;
}

export function entries(pf) {
  const seen = (pf.dex && pf.dex.seen) || {}, by = {}, out = [];
  for (const id in seen) { const s = seen[id]; if (s && s.id && s.rec) by[id] = Object.assign({}, s, { caught: false, count: 0, best: 0, grafts: {} }); }
  const ms = pf.pokemans || [];
  for (let i = 0; i < ms.length; i++) {
    const m = ms[i]; if (!m || !m.sp) continue;
    let e = by[m.sp.id];
    if (!e) { e = by[m.sp.id] = Object.assign(compact(m.sp, m.planet || m.pid || '?'), { caught: false, count: 0, best: 0, grafts: {} }); }
    e.caught = true; e.count++; if ((m.level | 0) > e.best) e.best = m.level | 0;
    const g = (m.ind && m.ind.grafts) || [];
    for (let k = 0; k < g.length; k++) e.grafts[g[k].p] = (e.grafts[g[k].p] || 0) + 1;
  }
  for (const id in by) out.push(by[id]);
  out.sort((a, b) => (a.planet < b.planet ? -1 : a.planet > b.planet ? 1 : a.tier - b.tier || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0)));
  return out;
}

export function groups(list) {
  const out = [], at = {};
  for (let i = 0; i < list.length; i++) { const p = list[i].planet; if (!at[p]) { at[p] = { planet: p, list: [] }; out.push(at[p]); } at[p].list.push(list[i]); }
  return out;
}

export function partyRow(pf, mon, maxHp) {
  const g = (mon.ind && mon.ind.grafts) || [], mx = Math.max(1, maxHp | 0), hp = mon.dead ? 0 : Math.max(0, Math.min(mx, mon.hp | 0));
  return { id: mon.id, name: mon.sp.name, level: mon.level | 0, hp, max: mx, frac: hp / mx, dead: !!mon.dead, bond: mon.bond | 0, bondTier: bondTier(mon), grafts: g.length, active: pf.activeMon === mon.id, types: mon.sp.types.slice() };
}

// ── creature icon: plan silhouette + signature parts, 16x16 ──
export function iconGrid(rec) {
  const r = mulberry((rec.seed | 0) ^ 0x1c0de), m = new Array(256).fill(''), P = (x, y, c) => { x = Math.round(x); y = Math.round(y); if (x >= 0 && y >= 0 && x < 16 && y < 16) m[y * 16 + x] = c; };
  const disc = (cx, cy, rx, ry) => { for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++) for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) { const dx = (x - cx) / rx, dy = (y - cy) / ry; if (dx * dx + dy * dy <= 1.05) P(x, y, 'b'); } };
  const segs = Math.max(3, Math.min(7, rec.segs | 0 || 4)), limbs = Math.max(0, Math.min(6, rec.limbs | 0));
  let top = 4, mid = 8, eyeX = 10, eyeY = 7;
  switch (rec.plan) {
    case 'skimmer': { const rx = Math.min(6.5, 3 + segs * 0.5), ry = 2.6 + (r() < 0.5 ? 0 : 0.6); mid = 8; top = mid - ry; disc(7.5, mid, rx, ry); for (let i = 0; i < 3; i++) { P(7.5 - rx - 1 + i * 0.5, mid + 1 + i, 'b'); P(7.5 + rx + 1 - i * 0.5, mid + 1 + i, 'b'); } eyeX = 7.5 + rx - 2; eyeY = mid - 1; break; }
    case 'floater': { const ry = 3 + segs * 0.35, rx = ry + 0.8; mid = 6; top = mid - ry; disc(7.5, mid, rx, ry); const n = Math.max(2, limbs || 3); for (let i = 0; i < n; i++) { const x = 4 + Math.round((i * 7) / Math.max(1, n - 1)); for (let y = Math.round(mid + ry); y < 14; y++) P(x + (((y + i) & 2) ? 1 : 0), y, 'b'); } eyeX = 7.5 + 1.5; eyeY = mid; break; }
    case 'strider': { mid = 5; top = 3; disc(7.5, mid, 3.4, 2.4); const n = Math.max(2, limbs || 2); for (let i = 0; i < n; i++) { const x = 4 + Math.round((i * 7) / Math.max(1, n - 1)); const bend = i % 2 ? 1 : -1; for (let y = 7; y < 15; y++) P(x + (y < 10 ? bend * (y - 7) * 0.3 : 0), y, 'b'); } eyeX = 9; eyeY = 5; break; }
    case 'burrower': { const n = segs; for (let i = 0; i < n; i++) { const t = i / Math.max(1, n - 1), cx = 2.5 + t * 11, cy = 11 - Math.sin(t * Math.PI) * 5, rr = 1.3 + (1 - Math.abs(t - 0.7)) * 1.6; disc(cx, cy, rr, rr); } top = 5; mid = 8; eyeX = 13; eyeY = 10; break; }
    case 'swarmer': { const n = 4 + (segs >> 1); for (let i = 0; i < n; i++) disc(3 + r() * 10, 4 + r() * 8, 1.4 + r() * 0.8, 1.2 + r() * 0.6); top = 3; mid = 8; eyeX = 8; eyeY = 8; break; }
    default: { /* crawler */ const rx = Math.min(6, 3 + segs * 0.45), ry = 2.8; mid = 7; top = mid - ry; disc(7.5, mid, rx, ry); const n = Math.max(2, limbs || 4); for (let i = 0; i < n; i++) { const x = 7.5 - rx + 1 + Math.round((i * (2 * rx - 2)) / Math.max(1, n - 1)); for (let y = Math.round(mid + ry); y < 13; y++) P(x, y, 'b'); } eyeX = 7.5 + rx - 2; eyeY = mid - 1; }
  }
  // signature parts (own letter so the colourer can tint them)
  const sig = rec.sig || [];
  for (let i = 0; i < sig.length; i++) {
    const p = sig[i];
    if (p === 'fin') { for (let k = 0; k < 4; k++) { P(7.5 + k - 1, top - 1 - (k < 2 ? k : 3 - k), 'f'); } }
    else if (p === 'spike') { P(4, top, 's'); P(4, top - 1, 's'); P(7, top - 1, 's'); P(7, top - 2, 's'); P(10, top, 's'); P(10, top - 1, 's'); }
    else if (p === 'shell') { for (let q = 0; q < 256; q++) if (m[q] === 'b' && ((q / 16) | 0) <= Math.round(mid) - 1) m[q] = 'c'; }
    else if (p === 'glow') { P(Math.round(eyeX) - 3, mid, 'g'); P(Math.round(eyeX) - 4, mid, 'g'); P(Math.round(eyeX) - 3, mid + 1, 'g'); }
    else if (p === 'lungs') { P(5, mid + 1, 'p'); P(10, mid + 1, 'p'); }
  }
  // shade: light from the top-left; edges lose to the outline pass below
  const out = new Array(256).fill('.'), body = (c) => c !== '';
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const c = m[y * 16 + x]; if (!c) continue;
    if (c !== 'b') { out[y * 16 + x] = c; continue; }
    const up = y > 0 && body(m[(y - 1) * 16 + x]), lf = x > 0 && body(m[y * 16 + x - 1]), dn = y < 15 && body(m[(y + 1) * 16 + x]), rt = x < 15 && body(m[y * 16 + x + 1]);
    out[y * 16 + x] = !up && !lf ? 'h' : !up || !lf ? 'l' : !dn || !rt ? 'd' : 'b';
  }
  const ex = Math.round(eyeX), ey = Math.round(eyeY); if (ex >= 0 && ex < 16 && ey >= 0 && ey < 16 && out[ey * 16 + ex] !== '.') out[ey * 16 + ex] = 'e';
  const fin = out.slice();
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    if (out[y * 16 + x] !== '.') continue;
    if ((x > 0 && out[y * 16 + x - 1] !== '.' && out[y * 16 + x - 1] !== 'O') || (x < 15 && out[y * 16 + x + 1] !== '.' && out[y * 16 + x + 1] !== 'O') || (y > 0 && out[(y - 1) * 16 + x] !== '.' && out[(y - 1) * 16 + x] !== 'O') || (y < 15 && out[(y + 1) * 16 + x] !== '.' && out[(y + 1) * 16 + x] !== 'O')) fin[y * 16 + x] = 'O';
  }
  return fin;
}

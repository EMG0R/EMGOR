// GOR BRAWL CPU: a utility-based planner with a 9-level difficulty ladder (L1-L9). Pure sim-side (no DOM), deterministic (seeded rng on the bot record).
// It never reads anything a player could not: opponent state is a delayed copy (`react` frames old, 24 -> 1 across the ladder) and its output is an ordinary
// input mask (press bits are edge-only, see out()).
//
// Every behaviour is a per-tier feature flag in BOT_TIERS (no `lvl === n` literals in the logic):
//   shield   probability of defending a predicted hit            grab    shield / out-of-shield grabs      dodge   spot-dodge / roll mixups (0..1)
//   di       0 none, 1 toward centre, 2 reads the knockback vector and picks the survival DI by simulating the launch arc (smash-geom.js)
//   tech     probability of teching       air  short-hop / jump-in aerial approach   fh  full-hop approach   spec  uses specials (projectiles)
//   punish   0 none, 1 only on clear stuns / knockdowns, 2 also punishes whiff endlag on reaction            edge  0 none, 1 ground ledge guard, 2 offstage edgeguard
//   kc       kill-confirm: scores moves by a launch-arc KO test (1), plans kill percent (2)     ff  fast-fall      dd  dash-dance spacing      adapt  habit reading
//   rec      recovery reliability (1 = always makes it back when physically possible)    vary  seeded score noise so it does not spam one poke
//
// This file must not import smash-engine.js (the engine imports this one): geometry + the launch predictor come from smash-geom.js (BRAWL_API).
import { frameData } from './smash-moves.js';
import { PHYS, physOf, knockback, hitstunOf } from './smash-phys.js';
import { BRAWL_API as G } from './smash-geom.js';

const L = 1, R = 2, U = 4, D = 8, A = 16, K = 32, SHD = 64, GR = 128, J = 256, CL = 512, CR = 1024, CU = 2048, CD = 4096;
const T = (lvl, react, every, atk, miss, o) => Object.assign({ lvl, react, every, atk, miss }, o);
export const BOT_TIERS = [null,
  T(1, 24, 12, 0.15, 0.35, { shield: 0,    grab: 0, dodge: 0,    di: 0, tech: 0,    air: 0,    fh: 0,   spec: 0, punish: 0, edge: 0, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 0.8,  vary: 9 }),
  T(2, 18, 8,  0.25, 0.20, { shield: 0.3,  grab: 0, dodge: 0,    di: 0, tech: 0,    air: 0,    fh: 0,   spec: 0, punish: 0, edge: 0, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 0.92, vary: 7 }),
  T(3, 14, 5,  0.40, 0.12, { shield: 0.55, grab: 1, dodge: 0,    di: 0, tech: 0,    air: 0,    fh: 0,   spec: 0, punish: 0, edge: 0, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 1,    vary: 5 }),
  T(4, 10, 3,  0.55, 0.06, { shield: 0.7,  grab: 1, dodge: 0,    di: 1, tech: 0.5,  air: 0.35, fh: 0,   spec: 1, punish: 1, edge: 0, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 1,    vary: 4 }),
  T(5, 8,  2,  0.70, 0.03, { shield: 0.8,  grab: 1, dodge: 0.3,  di: 1, tech: 0.8,  air: 0.5,  fh: 0,   spec: 1, punish: 1, edge: 0, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 1,    vary: 3.5 }),
  T(6, 6,  1,  0.80, 0.01, { shield: 0.88, grab: 1, dodge: 0.5,  di: 2, tech: 1,    air: 0.6,  fh: 0.2, spec: 1, punish: 2, edge: 1, kc: 0, ff: 0, dd: 0, adapt: 0, rec: 1,    vary: 3 }),
  T(7, 4,  1,  0.90, 0,    { shield: 0.93, grab: 1, dodge: 0.6,  di: 2, tech: 1,    air: 0.7,  fh: 0.4, spec: 1, punish: 2, edge: 1, kc: 0, ff: 1, dd: 0, adapt: 0, rec: 1,    vary: 2.5 }),
  T(8, 2,  1,  0.95, 0,    { shield: 0.96, grab: 1, dodge: 0.7,  di: 2, tech: 1,    air: 0.8,  fh: 0.5, spec: 1, punish: 2, edge: 2, kc: 1, ff: 1, dd: 0, adapt: 0, rec: 1,    vary: 2 }),
  T(9, 1,  1,  1.0,  0,    { shield: 0.98, grab: 1, dodge: 0.75, di: 2, tech: 1,    air: 0.85, fh: 0.6, spec: 1, punish: 2, edge: 2, kc: 2, ff: 1, dd: 1, adapt: 1, rec: 1,    vary: 1.5 })];
export const BOT_LABELS = ['', 'L1', 'L2', 'L3', 'L4', 'L5', 'L6', 'L7', 'L8', 'L9'];

const RING = 32, MASK = 31;
const sgn = (v) => (v > 0 ? 1 : v < 0 ? -1 : 0);
const dm = (d) => (d > 0 ? R : d < 0 ? L : 0);
const clampT = (v) => Math.max(1, Math.min(9, v | 0));
const BTN = { L: 1, R: 2 };
void BTN;

// ---- own-move knowledge (cached per def, invalidated by a preset switch): reach, startup, endlag, damage, every hit ---------------------------------
let INFO = new WeakMap(), INFO_EPOCH = -1;
function infoOf(def) {
  if (INFO_EPOCH !== PHYS.epoch) { INFO = new WeakMap(); INFO_EPOCH = PHYS.epoch; }
  let I = INFO.get(def); if (I) return I;
  const mv = def.moves; I = { poke: [], smash: [], air: [], ranged: !!(def.hint && def.hint.ranged), best: 0, grab: 0, proj: false, spec: [] };
  const add = (key, list) => {
    const m = mv[key]; if (!m || !m.hits || !m.hits.length) return; const h = m.hits[0], fd = frameData(m);
    let x1 = -99, y0 = 99, y1 = -99; for (const q of m.hits) { x1 = Math.max(x1, q.seg[0], q.seg[2]); y0 = Math.min(y0, q.seg[1], q.seg[3]); y1 = Math.max(y1, q.seg[1], q.seg[3]); }
    list.push({ key, m, h, hs: m.hits, s: fd.s, e: fd.e, dur: m.dur, dmg: h.dmg, bkb: h.bkb, gr: h.gr, ang: h.ang, reach: x1 + h.r, top: y0 - h.r, bot: y1 + h.r, back: h.seg[0] < 0 && h.seg[2] < 0, smash: m.charge != null, dash: key === 'dashatk' });
  };
  for (const k of ['jab1', 'ftilt', 'utilt', 'dtilt', 'dashatk']) add(k, I.poke);
  for (const k of ['fsmash', 'usmash', 'dsmash']) add(k, I.smash);
  for (const k of ['nair', 'fair', 'bair', 'uair', 'dair']) add(k, I.air);
  for (const p of I.poke) if (p.key !== 'dashatk' && p.reach > I.best) I.best = p.reach;
  I.grab = mv.grab && mv.grab.grab ? mv.grab.grab.reach + 4 : 30;
  const bn = mv.bN; I.proj = !!(bn && (!bn.hits || !bn.hits.length) && bn.tick && /\.proj\(/.test(String(bn.tick)));      // neutral special that throws a projectile
  I.noUp = !!(def.hint && def.hint.noUp) || !(mv.bU && mv.bU.noGrav);      // a bU that does not carry the fighter upward (noGrav rise) is not a recovery move
  INFO.set(def, I); return I;
}
// frames of advantage if this move connects: victim hitstun - my remaining endlag (negative = they act first)
function advOf(q, E) { const kb = knockback(q.dmg, E.pct + q.dmg, E.w, q.gr, q.bkb); return Math.max(hitstunOf(kb), 0) - q.e - 3; }
function threatReach(def) { const I = infoOf(def); let r = I.best; for (const q of I.smash) r = Math.max(r, q.reach); return r; }

// ---- perception ring: each foe's state from `react` frames ago ---------------------------------------------------------------------------------
function snap(b, S) {
  const F = S.fighters;
  if (!b.ring) { b.ring = []; for (let i = 0; i < 4; i++) { const r = []; for (let k = 0; k < RING; k++) r.push({ x: 0, y: 0, vx: 0, vy: 0, state: '', move: null, mf: 0, face: 1, pct: 0, grounded: true, sh: 180, lag: 0, hitstun: 0, sst: 0, stunT: 0, downT: 0, st: 0, inv: 0, stocks: 0, ledge: -1, jumps: 0, w: 100, sleepT: 0, eggT: 0 }); b.ring.push(r); } b.seen = -1; }
  for (let i = 0; i < F.length && i < 4; i++) {
    const f = F[i], o = b.ring[i][S.frame & MASK];
    o.x = f.x; o.y = f.y; o.vx = f.vx; o.vy = f.vy; o.state = f.state; o.move = f.move; o.mf = f.mf; o.face = f.face; o.pct = f.pct; o.grounded = f.grounded; o.sh = f.sh; o.lag = f.lag;
    o.hitstun = f.hitstun; o.sst = f.sst; o.stunT = f.stunT; o.downT = f.downT; o.st = f.st; o.inv = f.inv; o.stocks = f.stocks; o.ledge = f.ledge; o.jumps = f.jumps; o.w = f.def.weight; o.sleepT = f.sleepT; o.eggT = f.eggT;
  }
}
const pastOf = (b, S, i, d) => b.ring[i][(S.frame - Math.min(d, RING - 1)) & MASK];

// ---- stage helpers -------------------------------------------------------------------------------------------------------------------------
function mainSolid(S) { let m = S.stage.solids[0]; for (const s of S.stage.solids) if (s.x1 - s.x0 > m.x1 - m.x0) m = s; return m; }
function onThin(S, f) { for (const t of S.thin) if (Math.abs(f.y - t.y) < 0.05 && f.x > t.x0 - 3 && f.x < t.x1 + 3) return true; return false; }
function onStage(S, f) { for (const s of S.solids) if (f.x > s.x0 - 4 && f.x < s.x1 + 4 && f.y < s.y0 + 30) return true; return false; }

// threat: frames until E's next hit connects with me (or -1). E is a perceived snapshot (extrapolated by `ex` frames)
function threatOf(E, me, ex) {
  const m = E.move; if (E.state !== 'attack' || !m || !m.hits) return -1;
  const mf = E.mf + ex; let best = -1;
  if (m.hits.length === 0) return -1;
  for (let i = 0; i < m.hits.length; i++) {
    const h = m.hits[i]; if (mf > h.e) continue;
    let tt = h.s - mf; if (m.charge != null && mf >= m.charge && mf <= m.charge + 1) tt = h.s - m.charge + 1;
    if (tt < 0) tt = 0;
    const xa = E.x + E.face * Math.min(h.seg[0], h.seg[2]), xb = E.x + E.face * Math.max(h.seg[0], h.seg[2]), lo = Math.min(xa, xb) - h.r - 8, hi = Math.max(xa, xb) + h.r + 8;
    const ya = E.y + Math.min(h.seg[1], h.seg[3]) - h.r - 4, yb = E.y + Math.max(h.seg[1], h.seg[3]) + h.r + 24;
    if (me.x > lo && me.x < hi && me.y - 18 > ya && me.y - 18 < yb && (best < 0 || tt < best)) best = tt;
  }
  return best;
}
// vulnerability: frames until E can act again, only when E is genuinely stuck (-1 = not vulnerable)
function vulnOf(E) {
  switch (E.state) {
    case 'attack': { const m = E.move; if (!m) return -1; let last = 0; for (const h of m.hits) if (h.e > last) last = h.e; if (!m.hits.length) return -1; return E.mf >= last ? m.dur - E.mf : -1; }
    case 'land': return E.lag;
    case 'stun': return E.stunT;
    case 'shield': return E.sst > 0 ? E.sst : -1;
    case 'down': return 30 - E.downT;
    case 'hit': return E.hitstun;
    case 'roll': return E.st > 19 ? 28 - E.st : -1;
    case 'dodge': return E.st > 16 ? 24 - E.st : -1;
    case 'grab': return E.st > 10 ? 24 - E.st : -1;
    case 'egg': case 'sleep': case 'held': return 40;
  }
  return -1;
}
// does vulnerability kind count for a tier? punish 1 = clear stuns only (not whiff endlag, not shield-stun)
function vulnOk(tr, E) { if (tr.punish >= 2) return true; if (!tr.punish) return false; const s = E.state; return s === 'stun' || s === 'down' || s === 'egg' || s === 'sleep' || s === 'held' || s === 'land' || s === 'hit'; }

// ---- execution layer: press bits cannot be held across frames (edges only) -----------------------------------------------------------------------
const TAPS = A | K | GR | J | CL | CR | CU | CD;
function out(b, hold, tap) { tap &= ~b.last; const m = hold | tap; b.last = m & (TAPS | SHD); return m; }

// ---- exact hit tests through the geometry API --------------------------------------------------------------------------------------------
const _pv = { x: 0, y: 0, hurtMul: 1, state: 'idle', def: null, fid: '', grounded: true }, _caps = [];
function capsOf(live, E, x, y) { _pv.x = x; _pv.y = y; _pv.hurtMul = live.hurtMul || 1; _pv.state = E.state; _pv.def = live.def; _pv.fid = live.fid; return G.hurtCaps(_pv, _caps); }
// would any hit of `q` thrown from (ax, ay) facing `face` touch the foe `caps`?
function touches(q, ax, ay, face, caps) { for (let i = 0; i < q.hs.length; i++) if (G.hitTouches(ax, ay, face, q.hs[i], caps)) return true; return false; }
// KO test: would this move, landing now, launch the foe out of the blast zone (no DI, no recovery)?
function koOf(S, live, E, q, dir) {
  _pv.x = E.x; _pv.y = E.y; _pv.def = live.def; _pv.grounded = E.grounded;
  const kb = knockback(q.dmg, E.pct + q.dmg, E.w, q.gr, q.bkb); const r = G.launchArc(S.stage, _pv, kb, q.ang, dir, 0, 0); return r.ko;
}
// move picked: remember the last 3 keys so the planner varies (tier `vary` = noise amplitude)
function remember(b, key) { b.h2 = b.h1; b.h1 = b.h0; b.h0 = key; }
const repPen = (b, key) => (b.h0 === key ? 4 : 0) + (b.h1 === key ? 2.5 : 0) + (b.h2 === key ? 1.5 : 0);

// ---- survival DI: bend the pending launch to the stick that avoids the KO / lands on the stage (simulated with the shared launch-arc predictor) ----------
const STICKS = [0, L, R, U, D, L | U, R | U, L | D, R | D];
function diPick(S, f, b, cx) {
  const p = f.pend; if (b.diFor === p) return b.diHold;
  b.diFor = p; let best = 0, bs = -1e9;
  for (let i = 0; i < STICKS.length; i++) {
    const m = STICKS[i], sx = (m & R ? 1 : 0) - (m & L ? 1 : 0), sy = (m & U ? 1 : 0) - (m & D ? 1 : 0);
    const a = G.launchArc(S.stage, f, p.kb, p.ang, p.dir, sx, sy);
    let sc = a.ko ? -1000 + a.t * 4 : a.land ? 400 - Math.abs(a.x - cx) * 0.2 : 100 - Math.abs(a.x - cx) * 0.3 - (a.y > 0 ? 40 : 0);
    if (!a.ko && !a.land && a.y < S.stage.blast.b * 0.3) sc += 30;
    if (sc > bs) { bs = sc; best = m; }
  }
  b.diHold = best; return best;
}

export function botInput(S, f) {
  const b = f.bot; if (!b) return 0;
  if (!b.tierInit) { b.tierInit = 1; b.last = 0; b.cool = 0; b.hold = 0; b.shT = 0; b.plan = 0; b.noSh = 0; b.planT = 0; b.sideT = 0; b.walkT = 0; b.idx = -1; b.lastAtk = -99; b.opt = 0; b.optDir = 0; b.jT = 0; b.spCd = 0; b.egT = 0; b.h0 = b.h1 = b.h2 = ''; b.shN = 0; b.atkN = 0; b.pEs = ''; b.tc = 0; b.tcSt = -1; b.ddT = 0; }
  snap(b, S);
  if (S.phase !== 'play') { b.last = 0; return 0; }
  const tr = BOT_TIERS[clampT(b.tier || b.lvl || 6)], R_ = b.rng, st = f.state, lvl = tr.lvl, react = tr.react;
  if (st === 'dead' || st === 'out') return out(b, 0, 0);
  const P = physOf(f.def), I = infoOf(f.def), ms = mainSolid(S), half = (ms.x1 - ms.x0) / 2, cx = (ms.x0 + ms.x1) / 2;
  const toC = f.x > cx ? -1 : 1;

  // --- states that need button mashing / special handling regardless of tier
  if (st === 'egg' || st === 'held' || st === 'sleep') { b.alt = !b.alt; return out(b, b.alt ? L : R, b.alt ? A : J); }
  if (st === 'grabHold') return grabHold(S, f, b, tr, R_, cx);
  if (st === 'respawn') { return out(b, 0, f.respT < 100 && R_() < 0.2 ? J : 0); }
  if (st === 'ledge') {
    const Lg = S.ledges[f.ledge];
    if (f.ledgeT > 12 + (lvl <= 2 ? 20 : 0)) {
      const r = R_();
      if (lvl >= 7) { if (r < 0.34) return out(b, 0, J); if (r < 0.58) return out(b, 0, A); if (r < 0.8) return out(b, dm(-Lg.side), 0); return out(b, 0, SHD); }      // mixed getup: jump / attack / stand / roll
      if (lvl >= 3 && r < 0.5) return out(b, dm(-Lg.side), 0);
      if (r < 0.4) return out(b, 0, A); return out(b, dm(-Lg.side), 0);
    }
    return out(b, 0, 0);
  }
  if (f.hitlag > 0) {
    if (f.pend && tr.di >= 2) return out(b, diPick(S, f, b, cx), 0);      // reads the pending knockback vector
    if (f.pend && tr.di === 1) return out(b, dm(toC) | (Math.abs(f.pend.kb) > 0 ? 0 : 0), 0);
    return out(b, b.last & ~TAPS, 0);
  }
  if (st === 'dodge' || st === 'roll' || st === 'getup' || st === 'tech') return out(b, 0, 0);
  if (st === 'squat') return out(b, (b.jT-- > 0 ? J : 0) | (b.keepHold | 0), 0);

  // --- target: nearest live foe by perceived (old) state
  let ei = -1, best = 1e9;
  for (let i = 0; i < S.fighters.length; i++) { if (i === f.i) continue; const o = S.fighters[i]; if (o.stocks <= 0 || o.state === 'dead' || o.state === 'out') continue; const d = Math.abs(o.x - f.x) + Math.abs(o.y - f.y) * 1.5; if (d < best) { best = d; ei = i; } }
  if (ei < 0) return out(b, 0, 0);
  const E = pastOf(b, S, ei, react), live = S.fighters[ei];
  const dx = E.x - f.x, dy = E.y - f.y, ad = Math.abs(dx), dirT = sgn(dx) || f.face, myThreatR = I.best;
  const eDef = live.def, eThreat = threatReach(eDef);
  const grounded = f.grounded;

  // --- habit reading (L9): count the foe's shield / attack starts
  if (tr.adapt) { if (E.state !== b.pEs) { if (E.state === 'shield') b.shN++; else if (E.state === 'attack') b.atkN++; b.pEs = E.state; } }
  const aShield = tr.adapt ? Math.min(1, b.shN / 6) : 0, aAtk = tr.adapt ? Math.min(1, b.atkN / 12) : 0;

  // --- in hitstun: tech near the floor, otherwise the launch DI was already applied during hitlag
  if (st === 'hit') {
    if (f.st <= 1) { b.tc = R_() < tr.tech ? 1 : 0; }
    let tap = 0;
    if (b.tc && f.tumble && !f.grounded && f.vy > 0 && f.y > -34 && f.y <= 4 && onStage(S, f)) tap = SHD;
    return out(b, 0, tap);
  }
  if (st === 'stun') return out(b, 0, 0);
  if (st === 'down') { if (f.downT < 10) return out(b, 0, 0); const r = R_(); if (lvl >= 4 && r < 0.3) return out(b, 0, A); if (lvl >= 4 && r < 0.45) return out(b, dm(dirT), 0); return out(b, 0, 0); }

  // --- offstage: recover (priority over everything unless an L8+ edgeguard excursion is under way)
  const offX = f.x < ms.x0 - 6 || f.x > ms.x1 + 6, underStage = f.y > ms.y0 + 4 && f.x > ms.x0 - 8 && f.x < ms.x1 + 8;
  const guarding = b.egT > S.frame && f.jumps > 0 && !f.upUsed && f.y < ms.y0 + 50 && Math.abs(f.x - cx) < half + 80;
  if (!grounded && st !== 'ledge' && (offX || underStage) && !guarding) {
    if (lvl < 3 && R_() > tr.rec) return out(b, dm(toC), 0);
    if (st === 'airdodge' || f.helpless) { const side0 = f.x < cx ? -1 : 1; return out(b, dm(side0 < 0 ? ms.x0 - f.x : ms.x1 - f.x) | (f.y > ms.y0 + 20 ? 0 : 0), 0); }
    // the ledge grab zone is 10..60 px below the ledge and up to 26 px outside it: aim for 40 below (hneed = climb still needed)
    const side = f.x < cx ? -1 : 1, lg = side < 0 ? ms.x0 : ms.x1, hneed = f.y - ms.y0 - 40;
    const tgt = lg + side * 10, tdx = tgt - f.x;
    let hold = Math.abs(tdx) > 5 ? dm(tdx) : 0, tap = 0;
    if (underStage) hold = dm(side);      // under the deck: get out from beneath it first
    if (st === 'attack') return out(b, hold, 0);      // up-B in flight: steer it
    const room = f.def.flutter ? 52 : P.vDbl * P.vDbl / (2 * P.g), canDJ = f.jumps > 0 && f.vy > -0.2, canUB = !f.upUsed && !I.noUp && st === 'air' && f.vy > -0.5;
    if (f.def.flutter && f.flutterT > 0) hold |= J;
    else if (hneed < -6 && f.vy > 0.5) hold = dm(toC);      // above ledge height: drift in
    else if (!underStage) {
      // double jump first (the up-B locks the fighter in its move for ~40 f, so it goes at the apex of the jump); up-B once the jump is spent
      if (canDJ && hneed > -6 && (f.vy > 0.3 || hneed > room * 0.5)) { tap = J; if (f.def.flutter) hold |= J; }
      else if (canUB && hneed > 8 && f.jumps === 0) { tap = K; hold = U | dm(tdx); }
      else if (canUB && hneed > 8 && f.vy > 2.5) { tap = K; hold = U | dm(tdx); }
    } else if (f.jumps > 0 && f.vy > 0 && Math.abs(f.x - lg) < 40) tap = J;
    return out(b, hold, tap);
  }

  // --- projectiles: shield / hop
  let pj = -1;
  if (tr.shield > 0) for (let i = 0; i < S.projs.length; i++) { const p = S.projs[i]; if (p.owner === f.i || p.noHit || !(p.dmg > 0)) continue; const rx = f.x - p.x; if (Math.abs(p.vx) > 0.3 && rx * p.vx > 0 && Math.abs(rx) < 14 * Math.abs(p.vx) && Math.abs(p.y - (f.y - 20)) < 26) { pj = Math.abs(rx) / Math.abs(p.vx); break; } }
  if (grounded && pj >= 0 && pj < 7 && R_() < tr.shield) return out(b, SHD, 0);

  // --- shielding state: decide when to release / grab out of it (evaluated every frame, no cadence)
  if (st === 'shield') {
    const thr = threatOf(E, f, react);
    if (f.sst > 0) return out(b, SHD, 0);
    if (b.opt) { const o = b.opt; b.opt = 0; if (o === 1) return out(b, SHD | D, 0); if (o === 2) return out(b, SHD | dm(b.optDir), 0); if (o === 3) return out(b, SHD, J); }      // spot dodge / roll / jump out of the bubble
    const vu = vulnOf(E);
    if (tr.grab && ad < I.grab + 4 && Math.abs(dy) < 30 && (E.state === 'attack' && vu >= 0 || E.state === 'shield' || E.state === 'land' || E.state === 'stun' || E.state === 'down')) return out(b, SHD, GR);
    if (thr >= 0 || (E.state === 'attack' && vu < 0 && ad < eThreat + 30)) return out(b, SHD, 0);
    if (b.shT-- > 0) return out(b, SHD, 0);
    return out(b, 0, 0);
  }

  const cad = tr.every;
  if (cad > 1 && (S.frame % cad) !== 0 && st !== 'attack' && st !== 'shield') return out(b, b.keepHold | 0, 0);
  b.keepHold = 0;

  // === combat ============================================================================================================================
  const thr = E.state === 'attack' ? threatOf(E, f, react) : -1;
  const vu = vulnOf(E);
  const eOff = (E.x < ms.x0 - 6 || E.x > ms.x1 + 6) || E.y > 14;
  const roll = R_();
  const rl = R_;

  // 1. defend: a hit lands within a few frames (shield / spot dodge / roll / step back / jump out / air dodge)
  if (thr >= 0 && thr <= 14 && tr.shield > 0 && grounded && st !== 'attack') {
    if (roll < tr.shield * (1 + aAtk * 0.05)) {
      const r2 = rl();
      if (tr.dodge > 0 && r2 < tr.dodge && thr >= 5 && thr <= 12) {
        b.shT = 8;
        if (rl() < 0.6) { b.opt = 1; } else { b.opt = 2; b.optDir = rl() < 0.5 ? dirT : -dirT; }      // spot dodge, or roll through / away
        return out(b, SHD, 0);
      }
      if (lvl >= 7 && thr > 5 && ad > 4 && Math.abs(f.x - cx) < half - 24 && rl() < 0.4) { b.keepHold = dm(-dirT); b.shT = 6; return out(b, dm(-dirT), 0); }
      b.shT = 8; if (lvl >= 8 && rl() < 0.25) b.opt = 3; return out(b, SHD, 0);
    }
  }
  if (thr >= 0 && thr <= 10 && !grounded && f.adUsed === 0 && tr.dodge > 0 && roll < tr.dodge * 0.7) return out(b, 0, SHD);

  // 2. punish: opponent stuck for `vu` frames
  if (vu >= 0 && st !== 'attack' && ad < 130 && Math.abs(dy) < 70 && vulnOk(tr, E)) {
    const choice = pickPunish(S, I, f, live, E, ad, dy, vu, tr, P, R_, b, dirT);
    if (choice) {
      if (choice.go) { if (R_() >= tr.miss) { b.lastAtk = S.frame; remember(b, choice.key); return out(b, choice.hold | 0, choice.tap | 0); } return out(b, dm(dirT), 0); }
      if (!grounded && dy < -30 && f.jumps > 0 && f.vy > 0) return out(b, dm(dirT), J);
      if (!grounded && tr.ff && dy > 10) return out(b, dm(dirT) | D, 0);
      b.keepHold = dm(dirT); return out(b, dm(dirT), 0);
    }
  }

  // 3. edgeguard
  if (tr.edge && eOff && E.state !== 'ledge') {
    const side = E.x < cx ? -1 : 1, ex = cx + side * (half - 14), edgeSide = Math.abs(E.x - cx) > half - 10;
    if (grounded && edgeSide && (E.y > -20 || Math.abs(dy) > 10)) {
      if (Math.abs(f.x - ex) > 10) { b.keepHold = dm(ex - f.x); return out(b, dm(ex - f.x), 0); }
      if (f.face !== side) { b.keepHold = dm(side); return out(b, dm(side), 0); }
      const inReach = Math.abs(E.x - f.x) < 62 && E.y > -30 && E.y < 70;
      if (inReach && E.vy > -2.5 && R_() < 0.35) return out(b, dm(side), side < 0 ? CL : CR);      // smash at the ledge
      // L8+: go out after them: ranged edge pressure, or a full-hop chase while the double jump + up-B are still in hand
      if (tr.edge >= 2 && f.jumps > 0 && !f.upUsed && E.y > -50 && Math.abs(E.x - f.x) < 120 && f.pct < 130 && R_() < 0.5) { b.egT = S.frame + 40; return out(b, dm(side), J); }
      if (tr.spec && I.proj && Math.abs(dy) < 40 && S.frame >= b.spCd && f.face === side) { b.spCd = S.frame + 40; return out(b, dm(side), K); }
      return out(b, 0, 0);
    }
  }
  if (eOff && E.state === 'ledge' && grounded && tr.edge) { const side = E.x < cx ? -1 : 1; const ex = cx + side * (half - 10); if (Math.abs(f.x - ex) > 8) return out(b, dm(ex - f.x), 0); if (R_() < 0.25) return out(b, dm(side), side < 0 ? CL : CR); return out(b, 0, 0); }

  // 4. air game
  if (!grounded && st !== 'attack') {
    const a = airPick(S, I, f, live, E, dx, dy, tr, b);
    if (a && R_() < tr.atk * 0.7 + 0.25 && R_() >= tr.miss * 0.5) { remember(b, a.key); b.lastAtk = S.frame; return out(b, a.hold, a.tap); }
    let hold = dm(dirT), tap = 0;
    if (dy < -50 && f.jumps > 0 && f.vy > 0.3) tap = J;
    const lim = tr.edge >= 2 && b.egT > S.frame ? half + 60 : half - 10;
    if (Math.abs(f.x - cx) > lim && f.x * dirT > 0) hold = dm(toC);
    if (tr.ff && f.vy > 0.5 && !f.ff && ad < 110 && dy > 0 && !(vu < 0 && E.state === 'attack' && thr >= 0)) hold |= D;      // fast-fall to the target / to land sooner
    return out(b, hold, tap);
  }
  if (st === 'attack') return out(b, b.keepHold | 0, 0);

  // 5. grounded neutral
  if (grounded) {
    // vertical games
    if (dy < -50 && ad < 110 && !eOff) { if (tr.fh && rl() < 0.5) b.jT = 4; return out(b, dm(dirT), J); }       // jump toward a higher opponent
    if (dy > 50 && onThin(S, f) && ad < 140) return out(b, st === 'crouch' ? 0 : D, 0);      // drop through: D needs a fresh edge, so release it while crouching
    // E shielding: grab (habit reading raises it)
    if (E.state === 'shield' && ad < I.grab && Math.abs(dy) < 30 && tr.grab && (rl() < 0.5 + aShield * 0.5 || lvl <= 5)) { if (f.face !== dirT) return out(b, dm(dirT), 0); return out(b, 0, GR); }
    // turn to face
    if (f.face !== dirT && ad < myThreatR + 50) { b.keepHold = dm(dirT); return out(b, dm(dirT), 0); }
    // preemptive guard: the opponent can start a move inside my reaction time -> have the bubble up (grab or punish the whiff afterwards)
    if (tr.shield >= 0.7 && E.state !== 'attack' && vu < 0 && ad < eThreat + 6 + react && Math.abs(dy) < 40 && f.sh > 50 && S.frame >= b.noSh && eThreat > myThreatR - 2 && R_() < tr.shield * 0.9) {
      b.shT = 8 + ((R_() * 14) | 0); b.noSh = S.frame + b.shT + 40; return out(b, SHD, 0);
    }
    // spacing: wanted distance = my best poke reach + a margin, but outside the foe's threat reach when I cannot out-range it
    let want = myThreatR + 2;
    const safe = eThreat + 14;
    if (lvl >= 7 && want < safe && E.state !== 'attack') want = Math.min(want, safe);       // out-ranged: wait outside its reach and punish the whiff
    const atEdge = Math.abs(f.x - cx) > half - 18 && f.x * dirT < 0 ? 0 : 1;
    if (ad < want + 2 && Math.abs(dy) < 34) {
      // in poke range: attack (varied pick), else shield-grab mixups
      if (roll < tr.atk) {
        if (tr.grab && R_() < 0.2 + aShield * 0.4 && E.state === 'shield') return out(b, 0, GR);
        if (R_() < tr.miss) return out(b, dm(dirT) * (ad > 20 ? 1 : 0), 0);
        const pk = pickPoke(S, I, f, live, E, ad, dy, tr, R_, b, dirT);
        if (pk) { b.lastAtk = S.frame; remember(b, pk.key); return out(b, pk.hold, pk.tap); }
        if (ad > 20) { b.keepHold = dm(dirT); return out(b, dm(dirT), 0); }      // at the edge of poke range: step in
      }
      return out(b, 0, 0);
    }
    if (E.state === 'attack') b.baitT = 0;
    if (lvl >= 7 && ad < safe && ad > want + 2 && E.state !== 'attack' && eThreat > myThreatR + 4 && (b.baitT = (b.baitT | 0) + 1) < 36) {
      // out-ranged: hold position just outside its reach for a moment and bait the whiff (a passive foe is approached after ~0.6 s)
      if (ad < eThreat + 8) { b.keepHold = dm(-dirT); return out(b, atEdge ? dm(-dirT) : 0, 0); }
      return out(b, 0, 0);
    }
    if (ad > safe + 20) b.baitT = 0;
    if (tr.punish >= 2 && E.state === 'attack' && thr < 0 && vu < 0 && ad < eThreat + 20 && ad > myThreatR) { return out(b, dm(-dirT) * (atEdge ? 1 : 0), 0); }
    // projectile (specials, L4+): a ranged neutral special from outside poke range
    if (tr.spec && I.proj && ad > myThreatR + 26 && ad < 230 && Math.abs(dy) < 34 && E.state !== 'attack' && S.frame >= b.spCd && R_() < 0.35) {
      if (f.face !== dirT) { b.keepHold = dm(dirT); return out(b, dm(dirT), 0); }
      b.spCd = S.frame + 50 + ((R_() * 40) | 0); return out(b, 0, K);
    }
    // approach: dash-dance spacing (L9), short-hop / full-hop aerial approach, dash attack, or just run in
    if (tr.dd && ad < want + 44 && ad > want + 8 && E.state !== 'attack') { b.ddT++; const ph = (b.ddT >> 2) & 1; const h = ph ? dm(dirT) : dm(-dirT); return out(b, atEdge || ph ? h : 0, 0); }
    if (tr.air > 0 && ad > want + 4 && ad < want + 92 && Math.abs(dy) < 60 && E.state !== 'attack' && R_() < tr.air * 0.45) {
      b.keepHold = dm(dirT);
      if (tr.fh && R_() < tr.fh) { b.jT = 4; return out(b, dm(dirT), J); }      // full hop: hold J through the jump squat
      b.jT = 0; return out(b, dm(dirT), J);      // short hop: tap only
    }
    if (tr.punish >= 2 && f.state === 'run' && f.dAgeX >= 15 && E.state !== 'attack' && E.state !== 'shield') {
      const q = I.poke.find((x) => x.dash);
      if (q && ad < q.reach + 34 && ad > q.reach - 4 && Math.abs(dy) < 30 && R_() < 0.5) { remember(b, 'dashatk'); return out(b, dm(dirT), A); }
    }
    let hold = dm(dirT), tap = 0;
    if (Math.abs(f.x - cx) > half - 12 && f.x * dirT > 0 && !(lvl === 1 && R_() < 0.2)) hold = 0;           // never run off the stage (L1 sometimes does)
    b.keepHold = hold; return out(b, hold, tap);
  }
  return out(b, 0, 0);
}

// ---- grab follow-ups: pummels then the throw that KOs (L8+) / toward the nearest blast zone -------------------------------------------------------------
function grabHold(S, f, b, tr, R_, cx) {
  const v = f.hold >= 0 ? S.fighters[f.hold] : null; b.pumN = (b.pumN | 0);
  if (!v) return out(b, 0, 0);
  const want = tr.lvl <= 2 ? 0 : v.pct > 70 ? 1 : 3;
  if (b.pumN < want && (S.frame & 7) === 0) { b.pumN++; return out(b, 0, A); }
  if (b.pumN < want) return out(b, 0, 0);
  b.pumN = 0;
  const T = f.def.throws;
  if (tr.kc && T) {      // kill-confirm: the throw whose launch arc leaves the blast zone; else the one that sends them farthest toward an edge
    const opts = [['f', 0, f.face], ['b', 0, -f.face], ['u', U, 0], ['d', D, 0]]; let bestH = -1, bs = -1e9;
    for (const o of opts) {
      const t = T[o[0]]; if (!t) continue;
      const kb = knockback(t.dmg, v.pct + t.dmg, v.def.weight, t.gr, t.bkb), a = G.launchArc(S.stage, v, kb, t.ang, f.face, 0, 0);
      const sc = a.ko ? 1000 - a.t : Math.abs(a.x - cx) * 0.5 - (a.land ? 100 : 0);
      if (sc > bs) { bs = sc; bestH = o[1] | (o[2] ? dm(o[2]) : 0); }
    }
    if (bestH >= 0) return out(b, bestH, 0);
  }
  const ed = sgn(v.x - cx) || f.face;
  if (v.pct > 110 && tr.lvl >= 3) return out(b, U, 0);
  return out(b, ed === f.face ? dm(f.face) : dm(-f.face), 0);
}

// ---- attack choices ------------------------------------------------------------------------------------------------------------------------------------
function moveScore(S, b, live, E, q, tr, dir, R_, base) {
  let sc = base + R_() * tr.vary;
  if (tr.lvl >= 3) sc -= repPen(b, q.key);
  if (tr.kc) {
    const ko = koOf(S, live, E, q, dir);
    if (ko) sc += 60; else if (tr.kc >= 2 && q.smash) sc -= 10;      // L9: do not waste a smash before it kills; build damage with the fast pokes
    if (tr.kc >= 2 && E.pct > 90 && q.smash) sc += 14;
  }
  return sc;
}
// grounded neutral poke: exact capsule test against the foe's hurtboxes
function pickPoke(S, I, f, live, E, ad, dy, tr, R_, b, dirT) {
  const caps = capsOf(live, E, E.x + E.vx * 3, E.y); let best = null, bs = -1e9;
  for (const q of I.poke) {
    if (q.dash) continue;
    if (!touches(q, f.x, f.y, dirT, caps)) continue;
    if (tr.lvl <= 2 && ad > q.reach + 4) continue;
    const base = q.dmg * 4 - q.s * 0.6 - q.e * 0.5 + Math.max(-30, Math.min(10, advOf(q, E))) * 1.2;
    const sc = tr.lvl === 1 ? R_() : moveScore(S, b, live, E, q, tr, dirT, R_, base);
    if (sc > bs) { bs = sc; best = q; }
  }
  if (!best) return null;
  const key = best.key, dir = dirT;
  if (key === 'jab1') return { key, hold: 0, tap: A };
  if (key === 'ftilt') return { key, hold: f.state === 'run' && f.dAgeX >= 15 ? 0 : dm(dir), tap: A };      // a held direction while running would be a dash attack
  if (key === 'utilt') return { key, hold: U, tap: A };
  if (key === 'dtilt') return { key, hold: D, tap: A };
  return { key, hold: dm(dir), tap: A };
}
function pickPunish(S, I, f, live, E, ad, dy, vu, tr, P, R_, b, dirT) {
  // travel time to get in range at run speed, then startup must fit inside the window
  let best = null, bs = -1e9;
  const lists = [I.poke, I.smash];
  const caps = capsOf(live, E, E.x, E.y);
  for (const L_ of lists) {
    for (const q of L_) {
      const gap = Math.max(0, ad - q.reach + 2), trav = gap / (P.run * 0.9);
      if (q.key === 'dashatk') { const t2 = Math.max(0, ad - q.reach - 8) / (P.run * 1.1); if (t2 + q.s + 2 > vu) continue; }
      else if (trav + q.s + 1 > vu) continue;
      let base = q.dmg * (1 + (E.pct * 0.01)) + (q.smash ? E.pct * 0.12 : 0) + Math.max(-30, Math.min(10, advOf(q, E))) * 0.9;
      if (!q.smash && E.pct > 110 && tr.lvl >= 7) base -= 2;
      const sc = moveScore(S, b, live, E, q, tr, dirT, R_, base);
      if (sc > bs) { bs = sc; best = q; }
    }
  }
  if (!best) return null;
  const q = best, dir = dirT;
  if (!touches(q, f.x, f.y, dir, caps)) return { go: false };
  const key = q.key;
  if (q.smash) return { go: true, key, hold: 0, tap: key === 'fsmash' ? (dir > 0 ? CR : CL) : key === 'usmash' ? CU : CD };
  if (key === 'utilt') return { go: true, key, hold: U, tap: A };
  if (key === 'dtilt') return { go: true, key, hold: D, tap: A };
  return { go: true, key, hold: dm(dir), tap: A };
}
// aerial: exact capsule test with my predicted position at the move's first active frame
function airPick(S, I, f, live, E, dx, dy, tr, b) {
  const R_ = b.rng; let best = null, bs = -1e9; const P = physOf(f.def);
  for (const q of I.air) {
    const s = q.s, px = f.x + f.vx * s, py = f.y + f.vy * s + 0.5 * P.g * s * s, caps = capsOf(live, E, E.x + E.vx * s * 0.7, E.y + (E.grounded ? 0 : E.vy * s * 0.7));
    if (!touches(q, px, py, f.face, caps)) continue;
    const base = q.dmg * 3 - q.s * 0.5 + (tr.kc && E.pct > 80 ? q.dmg : 0);
    const sc = moveScore(S, b, live, E, q, tr, sgn(dx) || f.face, R_, base);
    if (sc > bs) { bs = sc; best = q; }
  }
  if (!best) return null;
  const k = best.key;
  if (k === 'uair') return { key: k, hold: U, tap: A };
  if (k === 'dair') return { key: k, hold: D, tap: A };
  if (k === 'nair') return { key: k, hold: 0, tap: A };
  return { key: k, hold: k === 'bair' ? dm(-f.face) : dm(f.face), tap: A };
}

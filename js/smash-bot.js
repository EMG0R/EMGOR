// GOR BRAWL CPU: a utility-based planner. Pure sim-side (no DOM), deterministic (seeded rng on the bot record), allocation-free per frame.
// It never reads anything a player could not: opponent state is a delayed copy (react frames old; the hard tier's input read is one
// frame late by construction: it is computed before S.step applies the frame), and its output is an ordinary input mask.
//
// Actions scored every decision tick: recover, DI/tech, defend (shield / back-step on a predicted hit), punish (whiff / endlag / shield-stun
// window the move fits in), combo (opponent in hitstun), edgeguard (opponent offstage), space (hold the distance the best poke reaches), approach.
//
// This file must not import smash-engine.js (the engine imports this one): the few bit constants it needs are repeated here.
import { moveTable, MOVE_KEYS, frameData } from './smash-moves.js';
import { physOf, knockback, hitstunOf } from './smash-phys.js';

const L = 1, R = 2, U = 4, D = 8, A = 16, K = 32, SHD = 64, G = 128, J = 256, CL = 512, CR = 1024, CU = 2048, CD = 4096;
export const BOT_TIERS = [null,
  { lvl: 1, react: 20, atk: 0.22, edge: false, every: 12, miss: 0.3 },
  { lvl: 2, react: 8, atk: 0.6, edge: false, every: 2, miss: 0.04 },
  { lvl: 3, react: 3, atk: 0.9, edge: true, every: 1, miss: 0 }];

const RING = 32, MASK = 31;
const sgn = (v) => (v > 0 ? 1 : v < 0 ? -1 : 0);
const dm = (d) => (d > 0 ? R : d < 0 ? L : 0);

// ---- own-move knowledge (cached per def): reach, startup, endlag, damage -------------------------------------------------------
const INFO = new WeakMap();
function infoOf(def) {
  let I = INFO.get(def); if (I) return I;
  const mv = def.moves; I = { poke: [], smash: [], air: [], ranged: !!(def.hint && def.hint.ranged), best: 0, grab: 0 };
  const add = (key, list) => {
    const m = mv[key]; if (!m || !m.hits || !m.hits.length) return; const h = m.hits[0], fd = frameData(m);
    let x1 = -99, y0 = 99, y1 = -99; for (const q of m.hits) { x1 = Math.max(x1, q.seg[0], q.seg[2]); y0 = Math.min(y0, q.seg[1], q.seg[3]); y1 = Math.max(y1, q.seg[1], q.seg[3]); }
    list.push({ key, s: fd.s, e: fd.e, dur: m.dur, dmg: h.dmg, kb: h.bkb + h.gr * 0.5, bkb: h.bkb, gr: h.gr, reach: x1 + h.r, top: y0 - h.r, bot: y1 + h.r, back: h.seg[0] < 0 && h.seg[2] < 0, ang: h.ang, smash: m.charge != null });
  };
  for (const k of ['jab1', 'ftilt', 'utilt', 'dtilt', 'dashatk']) add(k, I.poke);
  for (const k of ['fsmash', 'usmash', 'dsmash']) add(k, I.smash);
  for (const k of ['nair', 'fair', 'bair', 'uair', 'dair']) add(k, I.air);
  for (const p of I.poke) if (p.key !== 'dashatk' && p.reach > I.best) I.best = p.reach;
  I.grab = mv.grab && mv.grab.grab ? mv.grab.grab.reach + 4 : 30;
  void moveTable; void MOVE_KEYS;
  INFO.set(def, I); return I;
}
// frames of advantage if this move connects: victim hitstun - my remaining endlag (negative = they act first)
function advOf(q, E) { const kb = knockback(q.dmg, E.pct + q.dmg, E.w, q.gr, q.bkb); return Math.max(hitstunOf(kb), 0) - q.e - 3; }
// opponent's maximum ground threat reach (any normal / smash)
function threatReach(def) { const I = infoOf(def); let r = I.best; for (const q of I.smash) r = Math.max(r, q.reach); return r; }

// ---- perception ring: each foe's state from `react` frames ago ----------------------------------------------------------------------------
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

// ---- stage helpers ------------------------------------------------------------------------------------------------------------------
function mainSolid(S) { let m = S.stage.solids[0]; for (const s of S.stage.solids) if (s.x1 - s.x0 > m.x1 - m.x0) m = s; return m; }
function onThin(S, f) { for (const t of S.thin) if (Math.abs(f.y - t.y) < 0.05 && f.x > t.x0 - 3 && f.x < t.x1 + 3) return true; return false; }

// threat: frames until E's next hit connects with me (or -1). E is a perceived snapshot (extrapolated by `ex` frames)
function threatOf(E, me, ex, def) {
  const m = E.move; if (E.state !== 'attack' || !m || !m.hits) return -1;
  const mf = E.mf + ex; let best = -1;
  if (m.hits.length === 0) { return -1; }
  for (let i = 0; i < m.hits.length; i++) {
    const h = m.hits[i]; if (mf > h.e) continue;
    let tt = h.s - mf; if (m.charge != null && mf >= m.charge && mf <= m.charge + 1) tt = h.s - m.charge + 1;
    if (tt < 0) tt = 0;
    const xa = E.x + E.face * Math.min(h.seg[0], h.seg[2]), xb = E.x + E.face * Math.max(h.seg[0], h.seg[2]), lo = Math.min(xa, xb) - h.r - 8, hi = Math.max(xa, xb) + h.r + 8;
    const ya = E.y + Math.min(h.seg[1], h.seg[3]) - h.r - 4, yb = E.y + Math.max(h.seg[1], h.seg[3]) + h.r + 24;
    if (me.x > lo && me.x < hi && me.y - 18 > ya && me.y - 18 < yb && (best < 0 || tt < best)) best = tt;
  }
  void def; return best;
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
    case 'getup': return -1;
  }
  return -1;
}

// ---- execution layer: press bits cannot be held across frames (edges only) --------------------------------------------------------
const TAPS = A | K | G | J | CL | CR | CU | CD;
function out(b, hold, tap) { tap &= ~b.last; const m = hold | tap; b.last = m & (TAPS | SHD); return m; }

export function botInput(S, f) {
  const b = f.bot; if (!b) return 0;
  if (!b.tierInit) { b.tierInit = 1; b.last = 0; b.cool = 0; b.hold = 0; b.shT = 0; b.plan = 0; b.noSh = 0; b.planT = 0; b.sideT = 0; b.walkT = 0; b.idx = -1; b.lastAtk = -99; }
  snap(b, S);
  if (S.phase !== 'play') { b.last = 0; return 0; }
  const tr = b.tier ? BOT_TIERS[b.tier] : BOT_TIERS[Math.max(1, Math.min(3, b.lvl || 2))], R_ = b.rng, st = f.state;
  const react = tr.react, lvl = tr.lvl;
  if (st === 'dead' || st === 'out') return out(b, 0, 0);
  const P = physOf(f.def), I = infoOf(f.def), ms = mainSolid(S), half = (ms.x1 - ms.x0) / 2, cx = (ms.x0 + ms.x1) / 2;
  const toC = f.x > cx ? -1 : 1;

  // --- states that need button mashing / special handling regardless of tier
  if (st === 'egg' || st === 'held' || st === 'sleep') { b.alt = !b.alt; return out(b, b.alt ? L : R, b.alt ? A : J); }
  if (st === 'grabHold') {      // pummel a couple of times, then throw toward the nearest blast zone (or up when they are high)
    const v = f.hold >= 0 ? S.fighters[f.hold] : null; b.pumN = (b.pumN | 0);
    if (!v) return out(b, 0, 0);
    const want = lvl === 1 ? 0 : v.pct > 70 ? 1 : 3;
    if (b.pumN < want && (S.frame & 7) === 0) { b.pumN++; return out(b, 0, A); }
    if (b.pumN < want) return out(b, 0, 0);
    b.pumN = 0;
    const ed = sgn(v.x - cx) || f.face;
    if (v.pct > 110 && lvl >= 2) return out(b, U, 0);
    return out(b, ed === f.face ? dm(f.face) : dm(-f.face), 0);
  }
  if (st === 'respawn') { return out(b, 0, f.respT < 100 && R_() < 0.2 ? J : 0); }
  if (st === 'ledge') {
    const Lg = S.ledges[f.ledge];
    if (f.ledgeT > 12 + (lvl === 1 ? 20 : 0)) { const r = R_(); if (lvl === 3 && r < 0.5) return out(b, dm(-Lg.side), 0); if (r < 0.4) return out(b, 0, A); return out(b, dm(-Lg.side), 0); }
    return out(b, 0, 0);
  }
  if (f.hitlag > 0) return out(b, b.last & ~TAPS, 0);

  // --- target: nearest live foe by perceived (old) state
  let ei = -1, best = 1e9;
  for (let i = 0; i < S.fighters.length; i++) { if (i === f.i) continue; const o = S.fighters[i]; if (o.stocks <= 0 || o.state === 'dead' || o.state === 'out') continue; const d = Math.abs(o.x - f.x) + Math.abs(o.y - f.y) * 1.5; if (d < best) { best = d; ei = i; } }
  if (ei < 0) return out(b, 0, 0);
  const E = pastOf(b, S, ei, react), live = S.fighters[ei];
  const dx = E.x - f.x, dy = E.y - f.y, ad = Math.abs(dx), dirT = sgn(dx) || f.face, myThreatR = I.best;
  const eDef = live.def, eThreat = threatReach(eDef);

  // --- in hitstun: DI toward safety, tech near the floor
  if (st === 'hit') {
    let hold = 0, tap = 0;
    if (lvl >= 2) {
      const fast = Math.abs(f.vx) > Math.abs(f.vy);
      hold = fast ? U : dm(toC);
      if (lvl === 3 && f.tumble && !f.grounded && f.vy > 0 && f.y > -26 && f.y <= 4 && onStage(S, f)) tap = SHD;
      if (lvl === 2 && f.tumble && !f.grounded && f.vy > 0 && f.y > -16 && f.y <= 4 && R_() < 0.5) tap = SHD;
    }
    return out(b, hold, tap);
  }
  if (st === 'stun') return out(b, 0, 0);

  // --- offstage: recover (priority over everything)
  const offX = f.x < ms.x0 - 6 || f.x > ms.x1 + 6, below = f.y > 8;
  if (!f.grounded && st !== 'ledge' && (offX || (below && f.x > ms.x0 - 40 && f.x < ms.x1 + 40 && false))) {
    if (st === 'airdodge' || f.helpless) return out(b, dm(toC), 0);
    const side = f.x < cx ? -1 : 1, lg = side < 0 ? ms.x0 : ms.x1, hneed = f.y - ms.y0 + 6;      // height to climb to hang on the ledge
    const hdist = Math.abs(f.x - lg);
    let hold = dm(toC), tap = 0;
    if (lvl === 1 && R_() < 0.2) return out(b, hold, 0);
    if (st === 'attack') return out(b, hold, 0);
    const room = f.def.flutter ? 52 : P.vDbl * P.vDbl / (2 * P.g);
    if (f.jumps > 0 && f.vy > -0.2 && hneed > 6 && hneed < room + 30 && (hneed > room * 0.55 || f.vy > 1.5)) { tap = J; if (f.def.flutter) hold |= J; }
    else if (f.def.flutter && f.flutterT > 0) hold |= J;
    else if (f.jumps === 0 && !f.upUsed && f.vy > 0 && st === 'air' && (hneed > 18 || hdist > 70) && !(f.def.hint && f.def.hint.noUp)) { tap = K; hold = U | dm(toC); }
    else if (f.vy > 0.5 && hneed < -2) hold = dm(toC);      // above ledge height: drift in
    return out(b, hold, tap);
  }
  // under the stage edge but not past x: just get back
  if (!f.grounded && f.y > 30 && st === 'air') return out(b, dm(toC), f.jumps > 0 && f.vy > 0 ? J : 0);


  const grounded = f.grounded;
  // --- projectiles: shield / hop
  let pj = -1;
  if (lvl >= 2) for (let i = 0; i < S.projs.length; i++) { const p = S.projs[i]; if (p.owner === f.i || p.noHit || !(p.dmg > 0)) continue; const rx = f.x - p.x; if (Math.abs(p.vx) > 0.3 && rx * p.vx > 0 && Math.abs(rx) < 14 * Math.abs(p.vx) && Math.abs(p.y - (f.y - 20)) < 26) { pj = Math.abs(rx) / Math.abs(p.vx); break; } }
  if (grounded && pj >= 0 && pj < 7 && lvl >= (R_() < 0.8 ? 2 : 4)) return out(b, SHD, 0);

  // --- shielding state: decide when to release / grab out of it (evaluated every frame, no cadence)
  if (st === 'shield') {
    const thr = threatOf(E, f, react, eDef);
    if (f.sst > 0) return out(b, SHD, 0);
    const vu = vulnOf(E);
    if (lvl >= 2 && ad < I.grab + 4 && Math.abs(dy) < 30 && (E.state === 'attack' && vu >= 0 || E.state === 'shield' || E.state === 'land' || E.state === 'stun' || E.state === 'down')) return out(b, SHD, G);
    if (thr >= 0 || (E.state === 'attack' && vu < 0 && ad < eThreat + 30)) return out(b, SHD, 0);
    if (b.shT-- > 0) return out(b, SHD, 0);
    return out(b, 0, 0);
  }

  const cad = tr.every;
  if (cad > 1 && (S.frame % cad) !== 0 && st !== 'attack' && st !== 'shield') return out(b, b.keepHold | 0, 0);
  b.keepHold = 0;

  // === combat ==========================================================================================================================
  const thr = E.state === 'attack' ? threatOf(E, f, react, eDef) : -1;
  const vu = vulnOf(E);
  const eOff = (E.x < ms.x0 - 6 || E.x > ms.x1 + 6) || E.y > 14;
  const roll = R_();

  // 1. defend: a hit lands within a few frames
  if (thr >= 0 && thr <= 14 && lvl >= 2 && grounded && st !== 'attack') {
    if (roll < (lvl === 3 ? 0.97 : 0.85)) {
      // hard: step out of range when there is time; else shield
      if (lvl === 3 && thr > 5 && ad > 4 && Math.abs(f.x - cx) < half - 24 && R_() < 0.5) { b.keepHold = dm(-dirT); b.shT = 6; return out(b, dm(-dirT), 0); }
      b.shT = 8; return out(b, SHD, 0);
    }
  }
  if (thr >= 0 && thr <= 10 && lvl >= 3 && !grounded && f.adUsed === 0 && roll < 0.5) return out(b, 0, SHD);

  // 2. punish: opponent stuck for `vu` frames
  if (vu >= 0 && st !== 'attack' && ad < 130 && Math.abs(dy) < 70) {
    const choice = pickPunish(I, f, E, ad, dy, vu, lvl, P, R_);
    if (choice) {
      if (choice.go) { const tapb = choice.tap | 0; if (R_() >= tr.miss) { b.lastAtk = S.frame; return out(b, choice.hold | 0, tapb); } return out(b, dm(dirT), 0); }
      if (!grounded && dy < -30 && f.jumps > 0 && f.vy > 0) return out(b, dm(dirT), J);
      b.keepHold = dm(dirT); return out(b, dm(dirT), 0);
    }
  }

  // 3. edgeguard
  if (tr.edge && eOff && E.state !== 'ledge' && grounded && Math.abs(E.x - cx) > half - 10) {
    const side = E.x < cx ? -1 : 1, ex = cx + side * (half - 14);
    if (E.y > -20 || Math.abs(dy) > 10) {
      if (Math.abs(f.x - ex) > 10) { b.keepHold = dm(ex - f.x); return out(b, dm(ex - f.x), 0); }
      f.face === side || (b.keepHold = dm(side));
      const inReach = Math.abs(E.x - f.x) < 62 && E.y > -30 && E.y < 70;
      if (inReach && E.vy > -2.5 && R_() < 0.35) return out(b, dm(side), CL * (side < 0 ? 1 : 0) | CR * (side > 0 ? 1 : 0));        // smash at the ledge
      return out(b, dm(side) * 0, 0);
    }
  }
  if (eOff && E.state === 'ledge' && grounded && lvl >= 2) { const side = E.x < cx ? -1 : 1; const ex = cx + side * (half - 10); if (Math.abs(f.x - ex) > 8) return out(b, dm(ex - f.x), 0); if (R_() < 0.25) return out(b, dm(side), CL * (side < 0 ? 1 : 0) | CR * (side > 0 ? 1 : 0)); return out(b, 0, 0); }

  // 4. air game: attack when close
  if (!grounded && st !== 'attack') {
    if (ad < 60 && Math.abs(dy) < 70 && R_() < tr.atk * 0.7 + 0.2) { const a = airAttack(I, f, dx, dy, R_); if (a) return out(b, a.hold, a.tap); }
    let hold = dm(dirT), tap = 0;
    if (dy < -50 && f.jumps > 0 && f.vy > 0.3) tap = J;
    if (Math.abs(f.x - cx) > half - 10 && f.x * dirT > 0) hold = dm(toC);
    if (f.vy > 0.5 && !f.ff && lvl >= 3 && ad < 100) tap |= 0;
    return out(b, hold, tap);
  }
  if (st === 'attack') { return out(b, b.keepHold | 0, 0); }

  // 5. grounded neutral
  if (grounded) {
    // vertical games
    if (dy < -50 && ad < 110 && !eOff) { return out(b, dm(dirT), J); }       // jump toward a higher opponent
    if (dy > 50 && onThin(S, f) && ad < 140) return out(b, 0, D);
    // E shielding: grab
    if (E.state === 'shield' && ad < I.grab && Math.abs(dy) < 30 && lvl >= 2) { if (f.face !== dirT) return out(b, dm(dirT), 0); return out(b, 0, G); }
    // turn to face
    if (f.face !== dirT && ad < myThreatR + 50) { b.keepHold = dm(dirT); return out(b, dm(dirT), 0); }
    // preemptive guard: the opponent can start a move inside my reaction time -> have the bubble up (grab or punish the whiff afterwards)
    if (lvl >= 2 && E.state !== 'attack' && vu < 0 && ad < eThreat + 6 + react && Math.abs(dy) < 40 && f.sh > 50 && S.frame >= b.noSh && eThreat > myThreatR - 2 && R_() < (lvl === 3 ? 0.85 : 0.75)) {
      b.shT = 8 + ((R_() * 14) | 0); b.noSh = S.frame + b.shT + 40; return out(b, SHD, 0);
    }
    // spacing: wanted distance = my best poke reach + a margin, but outside the foe's threat reach when I cannot out-range it
    let want = myThreatR + 2;
    const safe = eThreat + 14;
    if (lvl === 3 && want < safe && E.state !== 'attack') want = Math.min(want, safe);       // out-ranged: wait outside its reach and punish the whiff
    const atEdge = Math.abs(f.x - cx) > half - 18 && f.x * dirT < 0 ? 0 : 1;
    if (ad < want + 2 && Math.abs(dy) < 34 && lvl >= 1) {
      // in poke range: attack (fast poke first), else shield-grab mixups
      if (roll < tr.atk) {
        if (lvl >= 2 && R_() < 0.2 && E.state === 'shield') return out(b, 0, G);
        const pk = pickPoke(I, f, E, ad, dy, lvl, R_, vu, eThreat);
        if (pk) { b.lastAtk = S.frame; return out(b, pk.hold, pk.tap); }
        if (ad > 20) { b.keepHold = dm(dirT); return out(b, dm(dirT), 0); }      // at the edge of poke range: step in
      }
      return out(b, 0, 0);
    }
    if (lvl === 3 && ad < safe && ad > want + 2 && E.state !== 'attack' && eThreat > myThreatR + 4) {
      // out-ranged: hold position just outside its reach; bait the whiff
      if (ad < eThreat + 8) { b.keepHold = dm(-dirT); return out(b, atEdge ? dm(-dirT) : 0, 0); }
      return out(b, 0, 0);
    }
    if (lvl >= 2 && E.state === 'attack' && thr < 0 && vu < 0 && ad < eThreat + 20 && ad > myThreatR) { return out(b, dm(-dirT) * (atEdge ? 1 : 0), 0); }
    // approach
    let hold = dm(dirT), tap = 0;
    if (Math.abs(f.x - cx) > half - 12 && f.x * dirT > 0) hold = 0;           // never run off the stage
    b.keepHold = hold; return out(b, hold, tap);
  }
  return out(b, 0, 0);
}

function onStage(S, f) { for (const s of S.solids) if (f.x > s.x0 - 4 && f.x < s.x1 + 4 && f.y < s.y0 + 30) return true; return false; }

// ---- attack choices ------------------------------------------------------------------------------------------------------------------------
function inReach(q, ad, dy) { return ad <= q.reach + 6 && (dy - 40) < q.bot + 2 && dy > q.top - 2; }      // my hit's vertical band overlaps the opponent's body (feet at dy)
function pickPoke(I, f, E, ad, dy, lvl, R_, vu, eThreat) {
  let best = null, bs = -1;
  for (const q of I.poke) {
    if (q.key === 'dashatk') continue;
    if (!inReach(q, ad, dy)) continue;
    let sc = q.dmg * 4 - q.s * 0.6 - q.e * 0.5 + Math.max(-30, Math.min(10, advOf(q, E))) * 1.2 + R_() * 3;
    if (lvl === 1) sc = R_();
    if (sc > bs) { bs = sc; best = q; }
  }
  if (!best) return null;
  const dir = sgn(E.x - f.x);
  const key = best.key;
  if (key === 'jab1') return { hold: 0, tap: A };
  if (key === 'ftilt') return f.state === 'run' && f.dAgeX >= 15 ? { hold: 0, tap: A } : { hold: dm(dir), tap: A };      // a held direction while running would be a dash attack
  if (key === 'utilt') return { hold: U, tap: A };
  if (key === 'dtilt') return { hold: D, tap: A };
  return { hold: dm(dir), tap: A };
}
function pickPunish(I, f, E, ad, dy, vu, lvl, P, R_) {
  // travel time to get in range at run speed, then startup must fit inside the window
  let best = null, bs = -1;
  const lists = [I.poke, lvl >= 2 ? I.smash : null];
  for (const L_ of lists) {
    if (!L_) continue;
    for (const q of L_) {
      const gap = Math.max(0, ad - q.reach + 2), trav = gap / (P.run * 0.9);
      if (q.key === 'dashatk') { const t2 = Math.max(0, ad - q.reach - 8) / (P.run * 1.1); if (t2 + q.s + 2 > vu) continue; }
      else if (trav + q.s + 1 > vu) continue;
      let sc = q.dmg * (1 + (E.pct * 0.01)) + (q.smash ? E.pct * 0.12 : 0) + Math.max(-30, Math.min(10, advOf(q, E))) * 0.9 + R_();
      if (!q.smash && E.pct > 110 && lvl === 3) sc -= 2;
      if (sc > bs) { bs = sc; best = q; }
    }
  }
  if (!best) return null;
  const q = best, dir = sgn(E.x - f.x);
  if (!inReach(q, ad, dy)) return { go: false };
  const key = q.key;
  if (q.smash) return { go: true, hold: 0, tap: key === 'fsmash' ? (dir > 0 ? CR : CL) : key === 'usmash' ? CU : CD };
  if (key === 'utilt') return { go: true, hold: U, tap: A };
  if (key === 'dtilt') return { go: true, hold: D, tap: A };
  return { go: true, hold: dm(dir), tap: A };
}
function airAttack(I, f, dx, dy, R_) {
  const dir = sgn(dx) || f.face, ad = Math.abs(dx); let best = null, bs = -1;
  for (const q of I.air) {
    if (!inReach(q, ad, dy)) continue;
    if (q.back ? dir === f.face : dir !== f.face && ad > 8) continue;
    const sc = q.dmg * 3 - q.s * 0.5 + R_();
    if (sc > bs) { bs = sc; best = q; }
  }
  if (!best) return null;
  const k = best.key;
  if (k === 'uair') return { hold: U, tap: A };
  if (k === 'dair') return { hold: D, tap: A };
  if (k === 'nair') return { hold: 0, tap: A };
  return { hold: dm(dir), tap: A };
}

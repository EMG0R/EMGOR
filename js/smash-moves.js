// GOR BRAWL move tables: the data-driven normals / smashes / aerials / grabs / throws for every style, plus the special-move fills,
// the retimes that keep every move inside sane frame data, and upgradeFighters(), which gives every FIGHTERS def the physics contract
// (smash-phys.js) and a complete move table. Pure data + tiny tick closures; no DOM. Idempotent: safe to import from anywhere.
//
// Frame data per move (frameData(m)): s = startup (first active frame index), a = active frames, e = endlag (frames after the last active/spawn frame).
// Row fields in BASE: s a e (frames) | d dmg | b base knockback | g knockback scaling | ang | x:[from,to] reach px | y height px | r hit radius | pose.
import { FIGHTERS, FIGHTER_IDS, AUTO } from './smash-fighters.js';
import { PHYS, clampN } from './smash-phys.js';

const hit = (s, e, seg, r, dmg, bkb, gr, ang, x) => Object.assign({ s, e, seg, r, dmg, bkb, gr, ang }, x || {});
const dirX = (f) => (f.inp & 2 ? 1 : 0) - (f.inp & 1 ? 1 : 0);
const r1 = (v) => Math.round(v * 10) / 10;

// ---- generic frame table (profile k=q=r=b=g=1) --------------------------------------------------------------------
export const BASE = {
  jab1:   { sh: 'f', s: 3, a: 2, e: 9,  d: 3,   b: 3,  g: 20,  ang: AUTO, x: [8, 28], y: -24, r: 8,  pose: 'swing' },
  jab2:   { sh: 'f', s: 3, a: 2, e: 9,  d: 3,   b: 3,  g: 20,  ang: AUTO, x: [8, 28], y: -22, r: 8,  pose: 'swing' },
  jab3:   { sh: 'f', s: 4, a: 3, e: 15, d: 4.6, b: 12, g: 70,  ang: 45,   x: [8, 32], y: -24, r: 9,  pose: 'kick' },
  ftilt:  { sh: 'f', s: 7, a: 3, e: 15, d: 8.5, b: 10, g: 100, ang: 40,   x: [6, 31], y: -25, r: 10, pose: 'kick', slide: 1.4 },
  utilt:  { sh: 'u', s: 6, a: 4, e: 12, d: 7,   b: 12, g: 95,  ang: 85,   x: [-8, 12], y: -48, r: 10, pose: 'swingU' },
  dtilt:  { sh: 'd', s: 5, a: 3, e: 11, d: 6,   b: 8,  g: 60,  ang: 80,   x: [10, 30], y: -5, r: 7,  pose: 'swingD' },
  dashatk:{ sh: 'f', s: 7, a: 6, e: 20, d: 9,   b: 10, g: 80,  ang: 40,   x: [4, 28], y: -22, r: 11, pose: 'dive', dash: 2.6 },
  fsmash: { sh: 'f', s: 12, a: 3, e: 24, d: 15, b: 28, g: 95,  ang: 38,   x: [8, 36], y: -26, r: 12, pose: 'swing', smash: 1, slide: 0.8 },
  usmash: { sh: 'u', s: 10, a: 4, e: 26, d: 14, b: 28, g: 98,  ang: 88,   x: [-10, 14], y: -50, r: 12, pose: 'swingU', smash: 1 },
  dsmash: { sh: 'ds', s: 9, a: 3, e: 24, d: 13, b: 24, g: 82,  ang: 35,   x: [10, 32], y: -8, r: 9, pose: 'swingD', smash: 1 },
  nair:   { sh: 'n', s: 4, a: 10, e: 10, d: 7,   b: 8,  g: 100, ang: AUTO, x: [0, 14], y: -22, r: 12, pose: 'spin', air: 1, land: 6, ac: [3, 22] },
  fair:   { sh: 'f', s: 8, a: 3, e: 16, d: 11,  b: 16, g: 90,  ang: 40,   x: [10, 30], y: -22, r: 10, pose: 'swing', air: 1, land: 9, ac: [5, 24] },
  bair:   { sh: 'b', s: 6, a: 3, e: 14, d: 12,  b: 20, g: 95,  ang: 145,  x: [12, 30], y: -23, r: 10, pose: 'kick', air: 1, land: 8, back: 1, ac: [4, 20] },
  uair:   { sh: 'u', s: 5, a: 4, e: 14, d: 8,   b: 12, g: 100, ang: 90,   x: [-6, 10], y: -50, r: 11, pose: 'swingU', air: 1, land: 7, ac: [3, 20] },
  dair:   { sh: 'dn', s: 9, a: 6, e: 17, d: 10, b: 14, g: 70,  ang: 290,  x: [-6, 6], y: 2, r: 11, pose: 'dive', air: 1, land: 14, ac: [6, 26] },
};
export const THROWS = {       // [dmg, base kb, scaling, angle]
  f: [9, 28, 70, 40], b: [10, 30, 75, 140], u: [8, 30, 80, 88], d: [6, 20, 60, 80],
};
export const MOVE_KEYS = ['jab1', 'jab2', 'jab3', 'ftilt', 'utilt', 'dtilt', 'dashatk', 'fsmash', 'usmash', 'dsmash', 'nair', 'fair', 'bair', 'uair', 'dair',
  'grab', 'pummel', 'fthrow', 'bthrow', 'uthrow', 'dthrow', 'bN', 'bF', 'bU', 'bD'];

// ---- per-style profile: k damage, q speed (startup/endlag multiplier), r reach, b base kb, g scaling, gs grab speed
export const PROFILES = {
  pilot:   { k: 1.0,  q: 1.0,  r: 1.0,  b: 1.0,  g: 1.0 },
  twin:    { k: 0.95, q: 1.12, r: 1.0,  b: 1.0,  g: 1.0 },
  hauler:  { k: 1.25, q: 1.3,  r: 1.1,  b: 1.1,  g: 1.1 },
  ranger:  { k: 1.0,  q: 1.05, r: 1.3,  b: 1.0,  g: 1.0 },
  bounty:  { k: 1.1,  q: 1.15, r: 1.1,  b: 1.0,  g: 1.0 },
  hopper:  { k: 1.0,  q: 1.0,  r: 1.0,  b: 1.0,  g: 1.0 },
  jelly:   { k: 0.88, q: 0.95, r: 0.9,  b: 0.9,  g: 0.95 },
  swift:   { k: 0.88, q: 0.8,  r: 0.88, b: 0.95, g: 0.95 },
  sparklet:{ k: 0.82, q: 0.8,  r: 0.82, b: 0.9,  g: 0.9 },
  hum:     { k: 0.82, q: 0.95, r: 0.85, b: 0.9,  g: 0.9 },
  seer:    { k: 0.95, q: 1.0,  r: 1.1,  b: 1.0,  g: 1.0 },
  racer:   { k: 1.1,  q: 0.95, r: 1.0,  b: 1.05, g: 1.05 },
  builder: { k: 1.0,  q: 1.05, r: 1.1,  b: 1.0,  g: 1.0 },
};
export function profileOf(def) {
  const p = PROFILES[def.id];
  if (p) return p;
  const w = clampN(def.weight || 100, 55, 140); return { k: 0.85 + (w - 60) / 200, q: 0.8 + (w - 60) / 160, r: 1, b: 1, g: 1 };       // creatures: weight sets damage + speed
}

function shape(B, P, sw) {
  const rx0 = B.x[0], rx1 = B.x[1] * (B.sh === 'u' || B.sh === 'dn' ? 1 : P.r), y = B.y;
  switch (B.sh) {
    case 'f': return [rx0, y, rx1, y + (B.pose === 'swing' ? 2 : 0)];
    case 'u': return [rx0, y + 4, rx1, y - 6];
    case 'd': return [rx0, y, rx1, y];
    case 'n': return [-14 * P.r, y, 14 * P.r, y];
    case 'b': return [-rx1, y, -B.x[0], y + 2];
    case 'dn': return [rx0, y - 6, rx1, y + 6];
    default: return [rx0, y, rx1, y];
  }
}

// builds one data-driven move from a BASE row
export function buildMove(key, B, P) {
  const s = clampN(Math.round(B.s * P.q), 3, 25), e = Math.min(45, Math.round(B.e * P.q)), a = B.a;
  const dmg = r1(B.d * (key.startsWith('jab') ? 1 + (P.k - 1) * 0.5 : P.k)), bkb = r1(B.b * P.b), kbg = r1(B.g * P.g), seg = shape(B, P), r = B.r * (P.r > 1 ? 1 + (P.r - 1) * 0.3 : 1);
  const hits = [hit(s, s + a, seg, r, dmg, bkb, kbg, B.ang)];
  if (B.sh === 'ds') { hits[0].seg = [B.x[0], B.y, B.x[1] * P.r, B.y]; hits.push(hit(s + 4, s + 4 + a, [-B.x[0], B.y, -B.x[1] * P.r, B.y], r, dmg, bkb, kbg, B.ang)); }
  const m = { name: key, dur: s + a + e + (B.sh === 'ds' ? 4 : 0), pose: B.pose, hits, data: 1 };
  if (B.air) { m.air = true; m.landLag = Math.round(B.land * P.q); m.ac = [B.ac[0], Math.min(B.ac[1], m.dur)]; }
  if (B.back) m.back = true;
  if (B.smash) m.charge = s - 1;       // frame at which the move holds while the button is down (up to PHYS.smashCharge frames)
  const slide = B.slide, dash = B.dash;
  if (dash) m.tick = (sim, f, t) => { if (t < s + a) f.vx = f.face * dash * (0.75 + 0.25 * P.q); else f.vx *= 0.85; };
  else if (slide) m.tick = (sim, f, t) => { if (t >= s - 2 && t <= s + a) f.vx = f.face * slide; };
  return m;
}

function buildGrab(P) {
  const s = clampN(Math.round(6 * P.q), 4, 12), a = 4, e = Math.round(18 * P.q);
  return { name: 'grab', dur: s + a + e, pose: 'grab', hits: [], grab: { s, e: s + a, reach: Math.round(26 * P.r) }, data: 1, fd: { s, a, e } };
}
const buildPummel = () => ({ name: 'pummel', dur: 8, pose: 'swing', hits: [], pummel: { dmg: 2, lag: 3 }, data: 1, fd: { s: 3, a: 1, e: 4 } });
function buildThrow(k, P) { const t = THROWS[k]; return { name: k + 'throw', dur: 20, pose: 'swing', hits: [], throw: { dmg: r1(t[0] * P.k), bkb: r1(t[1] * P.b), gr: r1(t[2] * P.g), ang: t[3] }, data: 1, fd: { s: 3, a: 1, e: 16 } }; }

// generic fallback specials (used when a def has no move for a special slot)
function genSpecial(key, P) {
  switch (key) {
    case 'bN': return { name: 'bolt', dur: 26, pose: 'cast', hits: [], tick(sim, f, t) { if (t === 0 && (f.cd.gb > 0 || sim.countProj(f, 'plasma') >= 2)) return true; if (t === 9) { f.cd.gb = 20; sim.proj(f, { kind: 'plasma', ox: 16, oy: -22, vx: 3.4, vy: 0, g: 0, life: 70, r: 6, dmg: 5 * P.k, bkb: 6, gr: 8, ang: 8 }); sim.sfx('shot'); } } };
    case 'bF': return { name: 'lunge', dur: 34, pose: 'dive', hits: [hit(6, 18, [4, -22, 22, -22], 10, 7 * P.k, 14, 70, 40)], tick(sim, f, t) { if (t >= 6 && t <= 16) f.vx = f.face * 4.4; } };
    case 'bU': return { name: 'rise', dur: 40, pose: 'swingU', noGrav: [5, 16], hits: [hit(5, 12, [4, -30, 14, -46], 10, 8 * P.k, 20, 80, 85)],
      tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; } if (t >= 5 && t <= 16) { f.vy = -5.2; f.vx = dirX(f) * 1.4; } if (t === 17) f.vy = -1; } };
    default: return { name: 'stomp', dur: 34, pose: 'spin', hits: [hit(6, 16, [-14, -20, 14, -20], 13, 8 * P.k, 14, 70, AUTO)] };
  }
}

// ---- the named style specials (spec: MARO / SONIK / JOSHI / STEEV) --------------------------------------------------------
const multi = (s0, n, gap, mk) => Array.from({ length: n }, (_, i) => mk(s0 + i * gap, i));
function nearestFoe(sim, f) { let E = null, bd = 1e9; for (const o of sim.fighters) { if (o === f || o.state === 'dead' || o.state === 'out') continue; const d = Math.abs(o.x - f.x) + Math.abs(o.y - f.y); if (d < bd) { bd = d; E = o; } } return E; }

const STYLE_SPECIALS = {
  pilot(m) {       // MARO: fireball (bN, kept) / cape reflect (bF) / super jump punch (bU, kept) / tornado (bD)
    const cape = Object.assign({}, m.bD, { name: 'cape' }); m.bF = cape;
    m.bD = { name: 'tornado', dur: 46, pose: 'spin', hits: [...multi(6, 4, 5, (s) => hit(s, s + 3, [-14, -22, 14, -22], 14, 2.2, 3, 0, AUTO)), hit(26, 30, [-16, -24, 16, -26], 15, 4.5, 24, 85, 80)],
      tick(sim, f, t) { if (t >= 6 && t <= 26) { if (!f.grounded) f.vy = Math.min(f.vy, -1.1); f.vx = dirX(f) * 1.5; } } };
  },
  swift(m) {       // SONIK: homing attack / spin dash / spring / spin charge
    m.bN = { name: 'homing', dur: 40, pose: 'dive', noGrav: [8, 22], dash: true,
      hits: [hit(8, 22, [-8, -26, 16, -22], 11, 7, 12, 70, 45)],
      tick(sim, f, t) {
        if (t === 0) { if (f.cd.homing > 0) return true; f.cd.homing = 40; f.dashx = f.face; f.dashy = 0; }
        if (t >= 8 && t <= 22) { const E = nearestFoe(sim, f); let dx = f.face, dy = 0; if (E) { dx = E.x - f.x; dy = E.y - 22 - (f.y - 22); const L = Math.sqrt(dx * dx + dy * dy) || 1; dx /= L; dy /= L; if (Math.abs(dx) > 0.2) f.face = dx > 0 ? 1 : -1; } f.vx = dx * 6.4; f.vy = dy * 6.4; f.dashx = dx; f.dashy = dy; }
        if (t === 23) { f.vx *= 0.3; f.vy = -2.4; }
      } };
    m.bF = { name: 'spindash', dur: 38, pose: 'spin',
      hits: [...multi(6, 5, 4, (s) => hit(s, s + 4, [-10, -18, 18, -16], 11, 1.6, 4, 0, AUTO)), hit(26, 30, [-10, -18, 20, -16], 12, 4, 24, 80, 40)],
      tick(sim, f, t) { if (t >= 6 && t <= 28) f.vx = f.face * 5.4; if (t === 29) f.vx *= 0.3; } };
    m.bU = { name: 'spring', dur: 44, pose: 'swingU', noGrav: [6, 16], hits: [hit(6, 13, [-6, -10, 10, -46], 11, 6, 18, 80, 88)],
      tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); } if (t >= 6 && t <= 16) { f.vy = -6.4; f.vx = dirX(f) * 1.6; } if (t === 17) f.vy = -1; } };
    m.bD = { name: 'spincharge', dur: 62, pose: 'spin', hits: [hit(18, 34, [-10, -18, 20, -16], 12, 9, 20, 80, 40)],
      tick(sim, f, t) {
        if (t === 0) f.fx = 0;
        if (t === 6 && f.inp & 32 && f.fx < 30) { f.fx++; f.mf--; return; }
        if (t >= 18 && t <= 34) f.vx = f.face * (3.6 + f.fx * 0.09); if (t === 35) f.vx *= 0.3;
      } };
  },
  hopper(m) {      // JOSHI: egg throw / egg roll / flutter up / ground pound
    m.bN = { name: 'eggthrow', dur: 34, pose: 'cast', hits: [], tick(sim, f, t) { if (t === 0 && f.cd.eggt > 0) return true; if (t === 12) { f.cd.eggt = 30; sim.proj(f, { kind: 'egg', ox: 14, oy: -26, vx: 2.8, vy: -2.2, g: 0.09, bounce: 0.6, life: 150, r: 6, dmg: 12, bkb: 25, gr: 80, ang: AUTO, hits: 2 }); sim.sfx('shot'); } } };
    m.bF = { name: 'eggroll', dur: 40, pose: 'spin', hits: [hit(6, 30, [-8, -18, 14, -14], 12, 7, 12, 60, 40)], tick(sim, f, t) { if (t >= 4 && t <= 30) f.vx = f.face * 3.4; } };
    m.bU = { name: 'flutterup', dur: 46, pose: 'swingU', noGrav: [4, 28], hits: [hit(8, 26, [-8, -30, 10, -42], 11, 3.5, 6, 20, 88)],
      tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; } if (t >= 4 && t <= 26) { f.vy = -3.1; f.vx = dirX(f) * 1.3; } if (t === 27) f.vy = -0.8; } };
    m.bD = { name: 'groundpound', dur: 44, pose: 'dive', noGrav: [0, 12], hits: [hit(14, 20, [-12, -4, 12, 6], 14, 10, 28, 85, AUTO)],
      tick(sim, f, t) { if (t < 14) { f.vx = 0; if (!f.grounded) f.vy = t < 4 ? -1.2 : 0; } else if (!f.grounded) f.vy = 7; } };
  },
  builder(m) {     // STEEV: block place / minecart / elytra up / TNT
    m.bF = m.bU; m.bF.name = 'minecart';
    m.bU = { name: 'elytra', dur: 52, pose: 'swingU', noGrav: [6, 34], hits: [hit(8, 14, [-6, -28, 12, -40], 10, 4, 8, 20, 85)],
      tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; } if (t >= 6 && t <= 34) { f.vy = t < 18 ? -3.6 : -0.6; f.vx = f.face * 2.2 + dirX(f) * 0.6; } if (t === 35) f.vy = 0.4; } };
  },
};

// ---- retimes: keep long scripted moves inside the frame-data bounds without editing their scripts -------------------------
function retime(m, shift) {     // move the whole timeline earlier by `shift` frames (charge-up moves): hits, ticks, armor and noGrav windows
  const old = m.tick;
  for (const h of m.hits) { h.s -= shift; h.e -= shift; }
  if (m.noGrav) m.noGrav = [Math.max(0, m.noGrav[0] - shift), m.noGrav[1] - shift];
  m.dur -= shift;
  m.tick = (sim, f, t) => { if (t === 0) { const r = old && old(sim, f, 0); if (r) return r; } return old ? old(sim, f, t + shift) : undefined; };
}
function trimDur(m, last) { m.dur = Math.min(m.dur, last + 40); }

// scan a tick closure for the frame numbers it reacts to (t === 9, t >= 4 ...)
function tickFrames(m) {
  if (m._tf) return m._tf;
  const src = m.tick ? String(m.tick) : '', re = /\bt\s*(?:===?|>=?|<=?)\s*(\d+)/g, out = []; let g;
  while ((g = re.exec(src))) { const n = +g[1]; if (n <= m.dur && n >= 3) out.push(n); }
  m._tf = out.length ? { first: Math.min(...out), last: Math.max(...out) } : { first: Math.max(3, Math.round(m.dur * 0.3)), last: Math.max(4, Math.round(m.dur * 0.35)) };
  return m._tf;
}
export function frameData(m) {
  if (m.fd) return m.fd;
  const hs = m.hits || [];
  if (hs.length) { let s = 1e9, last = 0; for (const h of hs) { if (h.s < s) s = h.s; if (h.e > last) last = h.e; } return { s, a: last - s, e: m.dur - last }; }
  const tf = tickFrames(m); return { s: tf.first, a: Math.max(1, tf.last - tf.first + 1), e: m.dur - tf.last - 1 };
}

function normalize(def) {
  const mv = def.moves;
  for (const k of ['bN', 'bF', 'bU', 'bD', 'bDAir']) {
    const m = mv[k]; if (!m || m.data || m._norm) continue; m._norm = 1;
    for (const h of m.hits || []) if (h.s < 3) { const w = h.e - h.s; h.s = 3; h.e = 3 + Math.max(1, w); }
    const fd = frameData(m);
    if (fd.s > 25 && m.hits.length) retime(m, fd.s - 18);
    else if (frameData(m).e > 45) trimDur(m, m.hits.length ? Math.max(...m.hits.map((h) => h.e)) : tickFrames(m).last);
    if (m.dur < 8) m.dur = 8;
  }
}

// ---- the upgrade: physics contract + full move table ----------------------------------------------------------------------
export function ensureMoves(def) {
  const P = profileOf(def), mv = def.moves;
  for (const k of MOVE_KEYS) {
    if (mv[k]) continue;
    if (k === 'grab') mv[k] = buildGrab(P); else if (k === 'pummel') mv[k] = buildPummel();
    else if (k.endsWith('throw')) mv[k] = buildThrow(k[0], P);
    else if (BASE[k]) mv[k] = buildMove(k, BASE[k], P);
    else mv[k] = genSpecial(k, P);
  }
  if (!mv.getupAtk) mv.getupAtk = { name: 'getupatk', dur: 28, pose: 'swingD', hits: [hit(8, 11, [10, -8, 30 * P.r, -8], 8, r1(6 * P.k), 14, 70, 45), hit(12, 15, [-10, -8, -30 * P.r, -8], 8, r1(6 * P.k), 14, 70, 135)], data: 1 };
  if (!mv.nair.ac) mv.nair.ac = [3, 22];
  // the jab chain
  mv.jab1.next = 'jab2'; mv.jab2.next = 'jab3';
  for (const k of ['jab1', 'jab2']) { const h = mv[k].hits[0]; mv[k].cancelFrom = h.s + 1; }
  def.throws = { f: mv.fthrow.throw, b: mv.bthrow.throw, u: mv.uthrow.throw, d: mv.dthrow.throw };
}

let DONE = false;
export function upgradeFighters() {
  if (DONE) return; DONE = true;
  for (const id of FIGHTER_IDS) {
    const def = FIGHTERS[id], P = profileOf(def), m = def.moves;
    // data-driven normals replace the hand-set ones (specials, finals and the hand-tuned aerial specials stay)
    for (const k of MOVE_KEYS) if (BASE[k]) m[k] = buildMove(k, BASE[k], P);
    if (STYLE_SPECIALS[id]) STYLE_SPECIALS[id](m);
    normalize(def);
    m.jab1.name = 'jab1';
    def.shortHop = PHYS.hopShort / PHYS.hopFull;
    delete m.dash;
    ensureMoves(def);
    if (id === 'swift') def.hint.ranged = false;
    if (id === 'hopper') def.hint.ranged = true;
  }
}
upgradeFighters();
export function completeDef(def) { if (!def.moves.grab || !def.moves.dsmash) { ensureMoves(def); normalize(def); } return def; }

// frame data table for docs / tests / the bot: every move of a def as {key: {s,a,e,dmg}}
export function moveTable(def) {
  const out = {};
  for (const k of MOVE_KEYS) { const m = def.moves[k]; if (!m) continue; const fd = frameData(m); let dmg = 0; for (const h of m.hits || []) dmg = Math.max(dmg, h.dmg); out[k] = { s: fd.s, a: fd.a, e: fd.e, dmg: m.throw ? m.throw.dmg : m.pummel ? m.pummel.dmg : dmg, dur: m.dur }; }
  return out;
}

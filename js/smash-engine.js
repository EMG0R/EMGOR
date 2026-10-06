// GOR BRAWL engine: fixed 60 Hz deterministic sim, CPU bots, input (keyboard+gamepad), WebGL2 sprite batcher
// with Canvas2D fallback, camera + scene drawing, and the game loop glue. The sim has no DOM / canvas access.
// Determinism: only + - * / sqrt, own sin/cos table, seeded RNG, state snapped to 1/256 px each step.
import { FIGHTERS, FIGHTER_IDS, AUTO } from './smash-fighters.js';
import { STAGES, createStageVisual } from './smash-stages.js';
import { buildAtlas } from './smash-sprites.js';

export const sfx = { play() {}, hook: null }; // smash.html wires sfx.hook(name) -> ship-audio (guarded)
const play = (n) => { try { if (sfx.hook) sfx.hook(n); sfx.play(n); } catch (e) { /* audio is optional */ } };

// ---- input bits ----------------------------------------------------------------------------------
export const B = { L: 1, R: 2, U: 4, D: 8, A: 16, K: 32, S: 64, G: 128, J: 256 };
const FIX = 256, snap = (v) => Math.round(v * FIX) / FIX;
const sgn = (v) => (v > 0 ? 1 : v < 0 ? -1 : 0);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

// ---- deterministic math ------------------------------------------------------------------------
const SIN = new Float64Array(362);
(function () {
  for (let d = 0; d <= 361; d++) {
    const dd = d <= 90 ? d : d <= 270 ? 180 - d : d - 360, x = dd * 3.141592653589793 / 180, x2 = x * x;
    SIN[d] = x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110 * (1 - x2 / 156))))));
  }
})();
export function sinD(a) { a %= 360; if (a < 0) a += 360; const i = a | 0, fr = a - i; return SIN[i] + (SIN[i + 1] - SIN[i]) * fr; }
export function cosD(a) { return sinD(a + 90); }
function wscale(w) { const x = 100 / w - 1; return 1 + 0.8 * x - 0.08 * x * x + 0.032 * x * x * x; } // (100/w)^0.8
function mulberry(a) { return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

function segSeg2(p1x, p1y, q1x, q1y, p2x, p2y, q2x, q2y) {
  const dx1 = q1x - p1x, dy1 = q1y - p1y, dx2 = q2x - p2x, dy2 = q2y - p2y, rx = p1x - p2x, ry = p1y - p2y;
  const a = dx1 * dx1 + dy1 * dy1, e = dx2 * dx2 + dy2 * dy2, f = dx2 * rx + dy2 * ry; let s, t;
  if (a <= 1e-9 && e <= 1e-9) return rx * rx + ry * ry;
  if (a <= 1e-9) { s = 0; t = clamp(f / e, 0, 1); }
  else {
    const c = dx1 * rx + dy1 * ry;
    if (e <= 1e-9) { t = 0; s = clamp(-c / a, 0, 1); }
    else {
      const b = dx1 * dx2 + dy1 * dy2, den = a * e - b * b;
      s = den !== 0 ? clamp((b * f - c * e) / den, 0, 1) : 0; t = (b * s + f) / e;
      if (t < 0) { t = 0; s = clamp(-c / a, 0, 1); } else if (t > 1) { t = 1; s = clamp((b - c) / a, 0, 1); }
    }
  }
  const cx = rx + dx1 * s - dx2 * t, cy = ry + dy1 * s - dy2 * t; return cx * cx + cy * cy;
}

// ---- the match sim -----------------------------------------------------------------------------
const STOCKS = 3, TIME = 14400, COUNT = 150;
const GROUND_STATES = { idle: 1, run: 1, crouch: 1 };

function mkFighter(i, slot, stage, stocks) {
  const def = FIGHTERS[slot.fid];
  return {
    i, fid: slot.fid, def, ctrl: slot.ctrl || 'cpu', level: slot.level || 2,
    x: stage.start[i].x, y: stage.start[i].y, px: stage.start[i].x, py: stage.start[i].y, vx: 0, vy: 0, face: i & 1 ? -1 : 1, grounded: true, landed: false,
    state: 'idle', st: 0, animT: 0, pct: 0, stocks: stocks || STOCKS, hitlag: 0, pend: null, hitstun: 0, tumble: false, inv: 0, armorNow: 0,
    sh: 180, jumps: def.jumps, upUsed: false, flutterT: 0, jumpT: 99, ff: false, move: null, mf: 0, hitSet: 0, hbMove: null, hbT: 0, moveAir: false,
    bufA: 0, bufB: 0, bufG: 0, bufJ: 0, inp: 0, prev: 0, cd: {}, reflectT: 0, hold: -1, heldBy: -1, holdT: 0, ledge: -1, ledgeT: 0, ledgeLock: 0, dropT: 0,
    eggT: 0, stunT: 0, lag: 0, deadT: 0, respT: 0, dashx: 0, dashy: 0, res: def.res0 || 0, resT: 0, kos: 0, dealt: 0, taken: 0, eggShield: 1, rollDir: 1,
    dmgMult: 1, bot: null, flash: 0, koDir: 0, hurtMul: 1,
  };
}

export function createMatch(cfg) {
  cfg = cfg || {};
  const stage = cfg.stageDef || STAGES[cfg.stage || 'plateau']; // stageDef: runtime stage (terrain fights)
  const slots = (cfg.players || [{ fid: 'pilot', ctrl: 'p1' }, { fid: 'swift', ctrl: 'cpu' }]).filter((p) => p.ctrl !== 'off').slice(0, 4);
  const S = {
    stage, frame: 0, phase: 'count', count: COUNT, time: cfg.time || TIME, endT: 0, rng: mulberry((cfg.seed | 0) || 1234),
    fighters: slots.map((p, i) => mkFighter(i, p, stage, cfg.stocks)), projs: [], blocks: [], events: [], solids: stage.solids.slice(), nid: 1, result: null,
    ledgeOcc: [-1, -1],
  };
  S.ledges = [];
  stage.solids.forEach((s, k) => { S.ledges.push({ x: s.x0, y: s.y0, side: -1 }, { x: s.x1, y: s.y0, side: 1 }); });
  S.ledgeOcc = S.ledges.map(() => -1);
  S.fighters.forEach((f) => { if (f.ctrl === 'cpu') f.bot = { rng: mulberry(9000 + f.i * 77 + (cfg.seed | 0)), next: 0, tap: 0, hold: 0, shT: 0, lastB: 0, lvl: f.level, turn: 0 }; });
  // services used by the data tables
  S.sfx = (n) => S.events.push({ t: 'sfx', n });
  S.countProj = (f, kind) => S.projs.reduce((a, p) => a + (p.owner === f.i && p.kind === kind ? 1 : 0), 0);
  S.towardCenter = (f) => (f.x > 0 ? -1 : 1);
  S.proj = (f, o) => {
    const p = Object.assign({ id: S.nid++, owner: f.i, hits: 1, bounce: 0, g: 0, flinch: true, hitSet: 0, age: 0, radial: false }, o);
    p.x = f.x + f.face * o.ox; p.y = f.y + o.oy; p.vx = f.face * o.vx; p.vy = o.vy; p.face = f.face;
    if (o.kind === 'egg') { if (f.inp & 4) p.vy -= 0.9; if (f.inp & 8) p.vy += 0.9; }
    S.projs.push(p);
  };
  S.placeBlock = (f, kind) => placeBlock(S, f, kind);
  S.mineBlock = (f) => mineBlock(S, f);
  S.step = (inputs) => stepMatch(S, inputs);
  S.snapshot = () => JSON.stringify({ f: S.frame, ph: S.phase, t: S.time, fs: S.fighters.map((f) => [f.x, f.y, f.vx, f.vy, f.pct, f.stocks, f.state, f.st]), p: S.projs.length, b: S.blocks.length });
  S.hash = () => {
    let h = 2166136261; const mix = (v) => { h ^= (v * 256) | 0; h = Math.imul(h, 16777619); };
    S.fighters.forEach((f) => { mix(f.x); mix(f.y); mix(f.vx); mix(f.vy); mix(f.pct); mix(f.stocks); mix(f.sh); mix(f.st); });
    S.projs.forEach((p) => { mix(p.x); mix(p.y); }); mix(S.frame); return h >>> 0;
  };
  return S;
}

// ---- blocks ---------------------------------------------------------------------------------
function overlapFighter(S, r) {
  for (const o of S.fighters) { if (o.state === 'dead' || o.state === 'out') continue; if (o.x + 7 > r.x0 && o.x - 7 < r.x1 && o.y > r.y0 && o.y - 36 < r.y1) return true; }
  return false;
}
function placeBlock(S, f, kind) {
  const cx = f.x + f.face * 24, r = { kind, x0: cx - 16, x1: cx + 16, y0: f.y - 32, y1: f.y, life: 180, fuse: 90, owner: f.i, id: S.nid++ };
  if (overlapFighter(S, r)) { S.sfx('deny'); return; }
  if (kind === 'block') { const mine = S.blocks.filter((b) => b.kind === 'block' && b.owner === f.i); if (mine.length >= 3) S.blocks.splice(S.blocks.indexOf(mine[0]), 1); }
  S.blocks.push(r); S.sfx('place'); S.events.push({ t: 'puff', x: cx, y: f.y - 16 });
}
function mineBlock(S, f) {
  const x0 = f.x + (f.face > 0 ? 6 : -34), x1 = x0 + 28;
  for (let i = 0; i < S.blocks.length; i++) { const b = S.blocks[i]; if (b.x1 > x0 && b.x0 < x1 && b.y1 > f.y - 36 && b.y0 < f.y) { S.blocks.splice(i, 1); f.res = Math.min(12, f.res + 1); S.sfx('mine'); S.events.push({ t: 'puff', x: (b.x0 + b.x1) / 2, y: (b.y0 + b.y1) / 2 }); return; } }
}

// ---- hurt / hit shapes ----------------------------------------------------------------------
const HURT = [[0, -6, 0, -14, 7], [0, -20, 0, -28, 8], [0, -35, 0, -35, 6]];
function hurtCaps(f, out) {
  let k = f.hurtMul;
  const st = f.state;
  if (st === 'crouch' || st === 'roll' || st === 'dodge' || st === 'getup') k *= 0.62; else if (st === 'land') k *= 0.85;
  out.length = 0;
  for (let i = 0; i < 3; i++) { const c = HURT[i]; out.push([f.x + c[0], f.y + c[1] * k, f.x + c[2], f.y + c[3] * k, c[4] * (f.fid === 'swift' ? 0.9 : 1)]); }
  return out;
}
const _hc = [];

function supportAt(S, f) {
  for (const s of S.solids) if (Math.abs(f.y - s.y0) < 0.02 && f.x + 5 > s.x0 && f.x - 5 < s.x1) return 1;
  if (!f.dropT) for (const t of S.stage.thin) if (Math.abs(f.y - t.y) < 0.02 && f.x > t.x0 - 3 && f.x < t.x1 + 3) return 2;
  return 0;
}

function moveFighter(S, f) {
  f.landed = false;
  f.x += f.vx;
  for (const s of S.solids) {
    if (f.x + 7 > s.x0 && f.x - 7 < s.x1 && f.y > s.y0 + 0.5 && f.y - 34 < s.y1) {
      if (f.x < (s.x0 + s.x1) / 2) f.x = s.x0 - 7; else f.x = s.x1 + 7;
      f.vx = 0;
    }
  }
  const py = f.y; f.y += f.vy;
  if (f.vy >= 0) {
    for (const s of S.solids) if (f.x + 5 > s.x0 && f.x - 5 < s.x1 && py <= s.y0 + 0.02 && f.y >= s.y0) { f.y = s.y0; f.vy = 0; f.landed = true; }
    if (!f.landed && !f.dropT) for (const t of S.stage.thin) if (py <= t.y + 0.02 && f.y >= t.y && f.x > t.x0 - 3 && f.x < t.x1 + 3) { f.y = t.y; f.vy = 0; f.landed = true; }
  } else {
    for (const s of S.solids) if (f.x + 6 > s.x0 && f.x - 6 < s.x1 && py - 34 >= s.y1 - 0.02 && f.y - 34 < s.y1) { f.y = s.y1 + 34; f.vy = f.state === 'hit' ? -f.vy * 0.4 : 0; }
  }
  if (f.dropT > 0) f.dropT--;
  const was = f.grounded;
  f.grounded = f.vy >= 0 && (f.landed || (was && supportAt(S, f) > 0));
  if (f.grounded) f.vy = 0;
  f.x = snap(f.x); f.y = snap(f.y); f.vx = snap(f.vx); f.vy = snap(f.vy);
}

// ---- fighter logic --------------------------------------------------------------------------
const dirOf = (f) => (f.inp & 2 ? 1 : 0) - (f.inp & 1 ? 1 : 0);
const vJump = (f, apex) => Math.sqrt(2 * f.def.grav * apex);

function readInput(f, inp) {
  f.inp = inp; const ed = inp & ~f.prev; f.prev = inp; f.ed = ed;
  f.bufA = ed & B.A ? 6 : f.bufA > 0 ? f.bufA - 1 : 0;
  f.bufB = ed & B.K ? 6 : f.bufB > 0 ? f.bufB - 1 : 0;
  f.bufG = ed & B.G ? 6 : f.bufG > 0 ? f.bufG - 1 : 0;
  f.bufJ = ed & (B.U | B.J) ? 6 : f.bufJ > 0 ? f.bufJ - 1 : 0;
}

function startMove(S, f, m) {
  f.move = m; f.mf = 0; f.hitSet = 0; f.state = 'attack'; f.moveAir = !f.grounded; f.bufA = 0; f.bufB = 0; f.hbMove = null;
  if (f.grounded) f.vx *= 0.5;
}
function endMove(S, f) { f.move = null; f.hbMove = null; f.state = f.grounded ? 'idle' : 'air'; f.st = 0; }

function chooseA(f) {
  const mv = f.def.moves, d = dirOf(f), up = f.inp & 4, dn = f.inp & 8;
  if (!f.grounded) {
    if (up) return mv.uair; if (dn) return mv.dair;
    if (d && d === f.face) return mv.fair; if (d) return mv.bair; return mv.nair;
  }
  if (d) { f.face = d; return mv.ftilt; }
  if (up) return mv.utilt; if (dn) return mv.dtilt;
  return mv.jab1;
}
function chooseB(f) {
  const mv = f.def.moves, d = dirOf(f), air = !f.grounded;
  let key = 'bN';
  if (f.inp & 4) key = 'bU'; else if (f.inp & 8) key = 'bD'; else if (d) { key = 'bF'; f.face = d; }
  return (air && mv[key + 'Air']) || mv[key] || mv.bN;
}

function doJump(S, f, fromGround) {
  if (fromGround) { f.vy = -vJump(f, f.def.jump); f.grounded = false; f.jumpT = 0; f.state = 'air'; f.jumps = f.def.jumps; f.ff = false; S.sfx('jump'); }
  else {
    if (f.def.flutter) { f.vy = -2.4; f.flutterT = 45; f.jumps--; }
    else f.vy = -vJump(f, f.def.jump * 0.85), f.jumps--;
    f.jumpT = 99; f.ff = false; S.events.push({ t: 'puff', x: f.x, y: f.y }); S.sfx('jump');
  }
  f.bufJ = 0;
}

function airControl(f, mult) {
  const d = dirOf(f), a = f.def.air * (mult == null ? 1 : mult), cap = f.def.run * 1.0;
  if (d) { f.vx += d * a * 1.6; if (Math.abs(f.vx) > cap && sgn(f.vx) === d) f.vx = d * Math.max(cap, Math.abs(f.vx) - 0.04); }
  else f.vx *= 0.985;
}
function gravity(f, mult) {
  const d = f.def; let g = d.grav * (mult == null ? 1 : mult);
  if (f.flutterT > 0 && f.inp & (B.J | B.U) && f.vy > -1.5) { g *= 0.1; f.vy = Math.max(f.vy, -1.5); }
  f.vy += g; const cap = d.fall * (f.ff ? 1.6 : 1) * (f.fid === 'swift' ? 1 : 1);
  if (f.vy > cap) f.vy = cap;
}

function groundMove(f) {
  const d = dirOf(f), run = f.def.run;
  if (f.inp & 8) { f.vx *= 0.78; return; }
  if (d) { f.face = d; f.vx += d * 0.34; if (Math.abs(f.vx) > run) f.vx = d * run; }
  else { f.vx = Math.abs(f.vx) < 0.3 ? 0 : f.vx - sgn(f.vx) * 0.26; }
}

function enterAir(S, f) { f.state = 'air'; f.st = 0; f.ff = false; }

function stepFighter(S, f) {
  f.px = f.x; f.py = f.y; f.armorNow = 0; f.hbMove = null; f.animT++;
  if (f.inv > 0) f.inv--; if (f.reflectT > 0) f.reflectT--; if (f.ledgeLock > 0) f.ledgeLock--;
  for (const k in f.cd) if (f.cd[k] > 0) f.cd[k]--;
  if (f.def.res0 && ++f.resT >= 240) { f.resT = 0; if (f.res < 12) f.res++; }
  const st = f.state; f.st++;
  const play_ = S.phase === 'play';
  if (!play_) { f.inp = 0; f.bufA = f.bufB = f.bufG = f.bufJ = 0; }
  switch (st) {
    case 'idle': case 'run': case 'crouch': {
      if (!f.grounded) { enterAir(S, f); break; }
      f.jumps = f.def.jumps; f.upUsed = false; f.flutterT = 0;
      if (f.sh < 180) f.sh = Math.min(180, f.sh + 0.35);
      if (f.bufA > 0) { startMove(S, f, chooseA(f)); break; }
      if (f.bufB > 0) { startMove(S, f, chooseB(f)); break; }
      if (f.bufG > 0) { f.state = 'grab'; f.st = 0; f.bufG = 0; break; }
      if (f.inp & B.S && f.sh > 0) { f.state = 'shield'; f.st = 0; f.eggUsed = 0; break; }
      if (f.bufJ > 0) { doJump(S, f, true); break; }
      if (f.ed & B.D && supportAt(S, f) === 2) { f.dropT = 12; f.grounded = false; f.y += 1; f.vy = 0.5; enterAir(S, f); break; }
      groundMove(f);
      f.state = f.inp & 8 ? 'crouch' : Math.abs(f.vx) > 0.35 ? 'run' : 'idle';
      break;
    }
    case 'land': { if (f.grounded) f.vx *= 0.8; if (--f.lag <= 0) { f.state = 'idle'; f.st = 0; } else if (f.bufJ > 0) { doJump(S, f, true); } break; }
    case 'air': {
      if (f.landed || f.grounded) { f.state = 'land'; f.lag = 3; f.st = 0; break; }
      if (f.ledgeLock === 0 && f.vy >= 0 && tryLedge(S, f)) break;
      if (f.jumpT < 6) { f.jumpT++; if (!(f.inp & (B.U | B.J)) && f.vy < 0) { const sh = f.def.shortHop; f.vy = -vJump(f, f.def.jump * sh); f.jumpT = 99; } }
      if (f.bufA > 0) { startMove(S, f, chooseA(f)); break; }
      if (f.bufB > 0) { startMove(S, f, chooseB(f)); break; }
      if (f.bufJ > 0 && f.jumps > 0) doJump(S, f, false);
      if (f.flutterT > 0) { f.flutterT--; if (f.flutterT > 33) f.armorNow = 1; }
      if (f.ed & B.D && f.vy >= 0 && !(f.inp & (B.L | B.R))) f.ff = true; else if (f.ed & B.D && f.vy > 0) f.ff = true;
      airControl(f); gravity(f);
      break;
    }
    case 'attack': updateAttack(S, f); break;
    case 'hit': {
      if (f.grounded) { f.vx *= 0.86; } else { f.vx *= 0.995; gravity(f); }
      if (--f.hitstun <= 0) { f.tumble = false; f.state = f.grounded ? 'idle' : 'air'; f.st = 0; f.ff = false; }
      break;
    }
    case 'shield': {
      if (!(f.inp & B.S)) { f.state = 'idle'; f.st = 0; break; }
      f.sh -= 0.55; f.vx *= 0.6;
      if (f.sh <= 0) { shieldBreak(S, f); break; }
      if (f.fid === 'hopper') { f.sh = Math.max(f.sh + 0.55, 30); if (f.st > 120) { f.state = 'idle'; f.st = 0; f.lag = 0; f.cd.egg = Math.max(f.cd.egg || 0, 40); break; } } // egg shield: never breaks, 2 s
      if (f.bufG > 0 || f.bufA > 0) { f.state = 'grab'; f.st = 0; f.bufG = f.bufA = 0; break; }
      if (f.bufJ > 0) { doJump(S, f, true); break; }
      if (f.ed & (B.L | B.R)) { f.rollDir = f.ed & B.R ? 1 : -1; f.state = 'roll'; f.st = 0; f.inv = 12; S.sfx('roll'); break; }
      if (f.ed & B.D) { f.state = 'dodge'; f.st = 0; f.inv = 14; break; }
      if (!f.grounded) enterAir(S, f);
      break;
    }
    case 'roll': { f.vx = f.st < 20 ? f.rollDir * 2.1 : f.vx * 0.7; if (f.st >= 24) { f.state = 'idle'; f.st = 0; } if (!f.grounded) enterAir(S, f); break; }
    case 'dodge': { f.vx *= 0.7; if (f.st >= 20) { f.state = 'idle'; f.st = 0; } break; }
    case 'grab': {
      f.vx *= 0.6;
      if (f.st >= 6 && f.st <= 9) { const v = tryGrab(S, f); if (v) break; }
      if (f.st >= 26) { f.state = 'idle'; f.st = 0; }
      break;
    }
    case 'grabHold': {
      const v = S.fighters[f.hold]; if (!v || v.state !== 'held') { f.hold = -1; f.state = 'idle'; f.st = 0; break; }
      f.vx *= 0.6; v.x = f.x + f.face * 14; v.y = f.y; v.vx = 0; v.vy = 0;
      f.holdT--;
      let th = null;
      if (f.bufA > 0 && !(f.inp & 15)) { f.bufA = 0; v.pct += 2; f.dealt += 2; S.events.push({ t: 'hit', x: v.x, y: v.y - 22, dmg: 2, kb: 0, dir: f.face, who: v.i, by: f.i }); S.sfx('hit_light'); f.hitlag = 3; v.hitlag = 3; }
      const d = dirOf(f);
      if (f.bufA > 0 || f.ed & (B.L | B.R | B.U | B.D)) {
        if (f.inp & 4) th = { dmg: 8, bkb: 30, gr: 75, ang: 88 }; else if (f.inp & 8) th = { dmg: 6, bkb: 20, gr: 60, ang: 80 };
        else if (d && d !== f.face) th = { dmg: 10, bkb: 25, gr: 80, ang: 140 }; else if (d) th = { dmg: 9, bkb: 30, gr: 70, ang: 40 };
      }
      if (th) { v.state = 'idle'; v.heldBy = -1; f.hold = -1; releaseHeld(v); hitFighter(S, f, v, th, f.face); f.state = 'idle'; f.st = 0; f.lag = 0; break; }
      if (f.holdT <= 0) { v.state = 'air'; v.heldBy = -1; v.inv = 20; f.hold = -1; f.state = 'idle'; f.st = 0; }
      break;
    }
    case 'held': { f.inv = Math.max(f.inv, 2); break; }
    case 'egg': {
      f.vx = 0; if (!f.grounded) gravity(f);
      if (f.ed & (B.A | B.K | B.L | B.R | B.U | B.D)) f.eggT -= 2;
      if (--f.eggT <= 0) { f.pct += 3; f.state = 'air'; f.vy = -3; f.vx = -f.face * 1; f.inv = 10; S.sfx('hit_light'); S.events.push({ t: 'puff', x: f.x, y: f.y - 14 }); }
      break;
    }
    case 'stun': { f.vx *= 0.8; if (!f.grounded) gravity(f); if (--f.stunT <= 0) { f.state = 'idle'; f.st = 0; f.sh = 60; } break; }
    case 'ledge': {
      const L = S.ledges[f.ledge]; f.vx = 0; f.vy = 0; f.ledgeT++;
      f.x = L.x + L.side * 6; f.y = L.y + 36; f.face = -L.side; f.upUsed = false; f.jumps = f.def.jumps;
      if (f.ledgeT < 30) f.inv = Math.max(f.inv, 2);
      const toward = dirOf(f) === -L.side, away = dirOf(f) === L.side;
      if (f.bufJ > 0 && f.ledgeT > 6) { leaveLedge(S, f); f.vy = -vJump(f, f.def.jump * 0.9); f.vx = -L.side * 1.2; f.state = 'air'; f.jumpT = 99; f.jumps = f.def.jumps; f.ledgeLock = 20; }
      else if ((toward || f.bufA > 0) && f.ledgeT > 6) { leaveLedge(S, f); f.state = 'getup'; f.st = 0; f.inv = 18; f.sx = f.x; f.sy = f.y; f.tx = L.x - L.side * 12; f.ty = L.y; f.bufA = 0; }
      else if (((f.ed & B.D) || away || f.ledgeT >= 90) && f.ledgeT > 6) { leaveLedge(S, f); f.state = 'air'; f.vy = 0.5; f.ledgeLock = 40; f.ff = false; f.jumps = f.def.jumps; }
      break;
    }
    case 'getup': { const t = Math.min(1, f.st / 14); f.x = f.sx + (f.tx - f.sx) * t; f.y = f.sy + (f.ty - f.sy) * (t * t); f.vx = 0; f.vy = 0; if (f.st >= 14) { f.x = f.tx; f.y = f.ty; f.grounded = true; f.state = 'idle'; f.st = 0; } break; }
    case 'dead': { f.vx = f.vy = 0; if (--f.deadT <= 0) { if (f.stocks > 0) respawn(S, f); else f.state = 'out'; } break; }
    case 'respawn': {
      f.vx = dirOf(f) * 0.8; f.vy = 0; f.respT--; f.inv = Math.max(f.inv, 10);
      f.x = clamp(f.x + f.vx, -150, 150);
      if (play_ && ((f.respT < 100 && (f.bufA > 0 || f.bufB > 0 || f.bufJ > 0 || f.ed & B.D)) || f.respT <= 0)) { f.state = 'air'; f.st = 0; f.inv = 60; f.vy = 0.3; f.jumps = f.def.jumps; f.upUsed = false; f.eggShield = 1; f.ledgeLock = 10; }
      break;
    }
    case 'out': break;
  }
  if (f.state !== 'dead' && f.state !== 'out' && f.state !== 'respawn' && f.state !== 'ledge' && f.state !== 'held' && f.state !== 'getup') moveFighter(S, f);
  postPhysics(S, f);
}

function leaveLedge(S, f) { if (f.ledge >= 0) S.ledgeOcc[f.ledge] = -1; f.ledge = -1; f.ledgeT = 0; }
function releaseHeld(v) { v.heldBy = -1; }

function postPhysics(S, f) {
  const st = f.state;
  if (f.landed) {
    if (st === 'attack' && f.move && f.moveAir) { const ll = f.move.landLag || 8; f.move = null; f.hbMove = null; f.state = 'land'; f.lag = ll; f.st = 0; S.sfx('land'); }
    else if (st === 'hit' && f.hitstun > 0 && !f.tumble) { /* stays in hit */ }
  }
  if (!f.grounded && (st === 'idle' || st === 'run' || st === 'crouch' || st === 'land')) { f.state = 'air'; f.st = 0; }
  if (f.grounded && st === 'attack' && f.move && f.moveAir && !(f.move.noGrav)) { /* landed handled above */ }
}

function updateAttack(S, f) {
  const m = f.move, t = f.mf;
  if (f.grounded) f.vx *= 0.82; else { airControl(f, 0.3); if (!(m.noGrav && t >= m.noGrav[0] && t <= m.noGrav[1])) gravity(f); }
  if (m.tick && m.tick(S, f, t)) { endMove(S, f); return; }
  if (m.armor && t >= m.armor[0] && t <= m.armor[1]) f.armorNow = 1;
  f.hbMove = m; f.hbT = t;
  if (m.next && t >= m.cancelFrom && f.bufA > 0 && (m.next !== m.name || f.def.moves[m.next])) { const nm = f.def.moves[m.next]; if (nm) { startMove(S, f, nm); return; } }
  f.mf++;
  if (f.mf >= m.dur) endMove(S, f);
}

function tryLedge(S, f) {
  for (let i = 0; i < S.ledges.length; i++) {
    const L = S.ledges[i]; if (S.ledgeOcc[i] >= 0) continue;
    const dx = (f.x - L.x) * L.side, dy = f.y - L.y;
    if (dx > -2 && dx < 26 && dy > 10 && dy < 60) {
      if (dirOf(f) === -L.side * -1 && false) continue;
      S.ledgeOcc[i] = f.i; f.ledge = i; f.ledgeT = 0; f.state = 'ledge'; f.st = 0; f.vx = f.vy = 0; f.upUsed = false; f.flutterT = 0; S.sfx('ledge'); return true;
    }
  }
  return false;
}

function tryGrab(S, f) {
  const x0 = f.x + f.face * 6, x1 = f.x + f.face * 26, y = f.y - 22;
  for (const v of S.fighters) {
    if (v === f || v.inv > 0 || v.state === 'dead' || v.state === 'out' || v.state === 'respawn' || v.state === 'held' || v.state === 'ledge' || v.state === 'getup') continue;
    const caps = hurtCaps(v, _hc);
    for (const c of caps) if (segSeg2(x0, y, x1, y, c[0], c[1], c[2], c[3]) < (9 + c[4]) * (9 + c[4])) {
      releaseHold(S, v); v.state = 'held'; v.heldBy = f.i; v.move = null; v.st = 0; v.pend = null; v.hitlag = 0;
      f.state = 'grabHold'; f.st = 0; f.hold = v.i; f.holdT = Math.floor(100 + v.pct * 0.6); S.sfx('grab'); return true;
    }
  }
  return false;
}
function releaseHold(S, v) { // v is being hit or grabbed while holding someone
  if (v.hold >= 0) { const o = S.fighters[v.hold]; if (o && o.state === 'held') { o.state = 'air'; o.heldBy = -1; o.inv = 20; } v.hold = -1; }
  if (v.ledge >= 0) leaveLedge(S, v);
}

function shieldBreak(S, f) { f.state = 'stun'; f.stunT = 90; f.vy = -4; f.grounded = false; f.sh = 0; S.sfx('break'); S.events.push({ t: 'break', x: f.x, y: f.y - 24 }); }

function respawn(S, f) {
  const sp = S.stage.spawns[f.i];
  f.state = 'respawn'; f.st = 0; f.x = sp.x; f.y = sp.y; f.px = f.x; f.py = f.y; f.vx = f.vy = 0; f.respT = 120; f.inv = 120; f.pct = 0; f.sh = 180;
  f.grounded = false; f.move = null; f.hitstun = 0; f.pend = null; f.hitlag = 0; f.face = sp.x < 0 ? 1 : -1; f.eggShield = 1; f.upUsed = false; f.flutterT = 0; f.jumps = f.def.jumps;
  if (f.def.res0) f.res = f.def.res0;
}

function koFighter(S, f) {
  releaseHold(S, f); if (f.heldBy >= 0) { const g = S.fighters[f.heldBy]; if (g) { g.hold = -1; g.state = 'idle'; } f.heldBy = -1; }
  f.stocks--; f.kos++; f.state = 'dead'; f.deadT = 50; f.move = null; f.hitstun = 0; f.pend = null; f.hitlag = 0; f.vx = f.vy = 0;
  const b = S.stage.blast; const dx = f.x < b.l ? -1 : f.x > b.r ? 1 : 0, dy = f.y < b.t ? -1 : 1;
  const ex = clamp(f.x, b.l, b.r), ey = clamp(f.y, b.t, b.b);
  S.events.push({ t: 'ko', who: f.i, x: ex, y: ey, dx, dy, left: f.stocks });
  S.sfx('ko');
}

// ---- combat ---------------------------------------------------------------------------------
function launchDir(f, ang, dir) {
  if (ang === AUTO) ang = f.grounded ? 25 : 44;
  return [cosD(ang) * dir, sinD(ang)];
}

function hitFighter(S, att, vic, h, dir, src) {
  const dmgMul = att ? att.dmgMult : 1, dmg = h.dmg * dmgMul;
  if (vic.state === 'held' || vic.state === 'dead' || vic.state === 'respawn') return false;
  // shield
  if (vic.state === 'shield' && !h.egg && !h.unblock) {
    vic.sh -= dmg * 1.4 + 1; const lag = Math.floor(dmg / 3) + 2;
    if (att) att.hitlag = lag; vic.hitlag = lag; vic.vx = dir * (0.5 + dmg * 0.09); if (att && att.grounded && !src) att.vx = -dir * 0.8;
    S.events.push({ t: 'shield', x: vic.x, y: vic.y - 20, who: vic.i }); S.sfx('shield');
    if (vic.sh <= 0) shieldBreak(S, vic);
    return true;
  }
  if (vic.inv > 0) return false;
  releaseHold(S, vic);
  vic.pct += dmg; if (att) att.dealt += dmg; vic.taken += dmg;
  let lag = Math.floor(dmg / 3) + 3; if (h.flinch === false) lag = 2;
  // armor: damage only
  if (vic.armorNow) { vic.hitlag = lag; if (att) att.hitlag = lag; S.events.push({ t: 'hit', x: vic.x, y: vic.y - 22, dmg, kb: 0, dir, who: vic.i, by: att ? att.i : -1, armor: 1 }); S.sfx('hit_light'); return true; }
  if (h.egg) {
    vic.state = 'egg'; vic.eggT = 45 + Math.floor(Math.min(45, vic.pct * 0.45)); vic.move = null; vic.vx = 0; vic.hitlag = 4; if (att) att.hitlag = 4;
    S.sfx('egg'); S.events.push({ t: 'puff', x: vic.x, y: vic.y - 20 }); return true;
  }
  const kb = (h.bkb + h.gr * (vic.pct / 100)) * wscale(vic.def.weight);
  if (h.flinch === false || (h.bkb === 0 && h.gr === 0)) {
    vic.hitlag = lag; if (att) att.hitlag = lag;
    S.events.push({ t: 'hit', x: vic.x, y: vic.y - 22, dmg, kb: 0, dir, who: vic.i, by: att ? att.i : -1 }); S.sfx('hit_light'); return true;
  }
  vic.hitlag = lag; if (att) att.hitlag = lag;
  vic.pend = { kb, ang: h.ang, dir }; vic.move = null; vic.state = 'hit'; vic.st = 0; vic.hitstun = 999; vic.flash = 4;
  S.events.push({ t: 'hit', x: vic.x, y: vic.y - 22, dmg, kb, dir, who: vic.i, by: att ? att.i : -1 });
  S.sfx(kb > 60 ? 'hit_heavy' : 'hit_light');
  return true;
}

function applyLaunch(S, f) {
  const p = f.pend; f.pend = null; if (!p) return;
  let [lx, ly] = launchDir(f, p.ang, p.dir);
  // directional influence: bend up to 15 degrees by the stick component perpendicular to the launch
  const sx = dirOf(f), sy = (f.inp & 4 ? 1 : 0) - (f.inp & 8 ? 1 : 0);
  if (sx || sy) { const L = Math.sqrt(sx * sx + sy * sy), ux = sx / L, uy = sy / L, pp = ux * -ly + uy * lx, phi = 15 * pp, c = cosD(phi), s = sinD(phi); const nx = lx * c - ly * s, ny = lx * s + ly * c; lx = nx; ly = ny; }
  const speed = p.kb * 0.075;
  f.vx = lx * speed; f.vy = -ly * speed;
  if (f.grounded) { if (f.vy > 0) f.vy = -f.vy * 0.4; if (f.vy > -1.4) f.vy = 0; else f.grounded = false; }
  f.hitstun = Math.max(8, Math.floor(p.kb * 0.4)); f.tumble = p.kb >= 60; f.ff = false; f.upUsed = f.upUsed;
  if (!f.grounded) f.y -= 0.01;
}

function hitCapsuleWorld(f, h) {
  const d = f.face;
  return [f.x + d * h.seg[0], f.y + h.seg[1], f.x + d * h.seg[2], f.y + h.seg[3], h.r];
}

function detectHits(S) {
  const F = S.fighters;
  for (const a of F) {
    const m = a.hbMove; if (!m || a.state !== 'attack' || a.hitlag > 0) continue;
    for (let hi = 0; hi < m.hits.length; hi++) {
      const h = m.hits[hi]; if (a.hbT < h.s || a.hbT >= h.e) continue;
      const cap = hitCapsuleWorld(a, h);
      for (const v of F) {
        if (v === a || v.state === 'dead' || v.state === 'out') continue;
        const bit = 1 << (hi * 4 + v.i); if (a.hitSet & bit) continue;
        let hit = false;
        if (v.state === 'shield') { const rr = 12 + 14 * (v.sh / 180), cy = v.y - 20 * v.hurtMul; const dx = Math.max(0, 0); hit = segSeg2(cap[0], cap[1], cap[2], cap[3], v.x, cy, v.x, cy) < (cap[4] + rr) * (cap[4] + rr) + dx; }
        else { const caps = hurtCaps(v, _hc); for (const c of caps) if (segSeg2(cap[0], cap[1], cap[2], cap[3], c[0], c[1], c[2], c[3]) < (cap[4] + c[4]) * (cap[4] + c[4])) { hit = true; break; } }
        if (!hit) continue;
        if (hitFighter(S, a, v, h, a.face)) a.hitSet |= bit;
      }
    }
  }
}

function updateProjs(S) {
  const P = S.projs, F = S.fighters;
  for (let i = P.length - 1; i >= 0; i--) {
    const p = P[i]; p.age++; let dead = false;
    if (p.kind !== 'boom') {
      p.px = p.x; p.py = p.y; p.x += p.vx; p.y += p.vy; p.vy += p.g;
      for (const s of S.solids) {
        if (p.x > s.x0 - p.r && p.x < s.x1 + p.r && p.y > s.y0 && p.y < s.y1 + p.r) {
          if (p.bounce && p.py <= s.y0 + 0.5 && p.vy > 0) { p.y = s.y0 - 0.5; p.vy = -Math.abs(p.vy) * p.bounce; if (Math.abs(p.vy) < 0.8) p.vy = -0.8; }
          else if (p.bounce && p.px < s.x0 - p.r + 1 || p.bounce && p.px > s.x1 + p.r - 1) { p.vx = -p.vx; p.x = p.px; }
          else if (!p.bounce) dead = true;
        }
      }
      if (p.bounce) for (const t of S.stage.thin) if (p.vy > 0 && p.py <= t.y && p.y >= t.y && p.x > t.x0 && p.x < t.x1) { p.y = t.y - 0.5; p.vy = -Math.abs(p.vy) * p.bounce; if (Math.abs(p.vy) < 0.8) p.vy = -0.8; }
      const b = S.stage.blast; if (p.x < b.l || p.x > b.r || p.y < b.t - 40 || p.y > b.b) dead = true;
    }
    if (--p.life <= 0) dead = true;
    if (!dead || p.kind === 'boom') {
      for (const v of F) {
        if (v.state === 'dead' || v.state === 'out' || v.state === 'held') continue;
        if (p.owner === v.i && !p.selfHit && !p.reflected) continue;
        const bit = 1 << v.i; if (p.hitSet & bit) continue;
        if (v.reflectT > 0 && p.kind !== 'boom' && p.owner !== v.i) {
          if ((p.x - v.x) * (p.x - v.x) + (p.y - (v.y - 22)) * (p.y - (v.y - 22)) < 26 * 26) { p.owner = v.i; p.vx = -p.vx * 1.2; p.vy = -p.vy; p.dmg = p.dmg * 1.5; p.life += 20; p.hitSet = 0; p.reflected = true; S.sfx('reflect'); S.events.push({ t: 'spark', x: p.x, y: p.y }); continue; }
        }
        if (v.inv > 0 && v.state !== 'shield') continue;
        let hit = false;
        if (v.state === 'shield') hit = (p.x - v.x) * (p.x - v.x) + (p.y - (v.y - 20)) * (p.y - (v.y - 20)) < (p.r + 12 + 14 * (v.sh / 180)) ** 2;
        else { const caps = hurtCaps(v, _hc); for (const c of caps) if (segSeg2(p.x, p.y, p.x, p.y, c[0], c[1], c[2], c[3]) < (p.r + c[4]) ** 2) { hit = true; break; } }
        if (!hit) continue;
        const att = S.fighters[p.owner], dir = p.radial ? (sgn(v.x - p.x) || 1) : (sgn(p.vx) || p.face);
        const hh = { dmg: p.dmg * (p.owner === v.i ? 0.75 : 1), bkb: p.bkb, gr: p.gr, ang: p.ang, flinch: p.flinch, egg: p.egg };
        if (hitFighter(S, att, v, hh, dir, p)) { p.hitSet |= bit; if (--p.hits <= 0 && p.kind !== 'boom') { dead = true; break; } }
      }
    }
    if (dead) { if (p.kind === 'egg' || p.kind === 'plasma') S.events.push({ t: 'puff', x: p.x, y: p.y }); P.splice(i, 1); }
  }
}

function updateBlocks(S) {
  S.solids.length = S.stage.solids.length;
  for (let i = S.blocks.length - 1; i >= 0; i--) {
    const b = S.blocks[i]; b.life--;
    if (b.kind === 'tnt') { b.fuse--; if (b.fuse <= 0) { S.blocks.splice(i, 1); explode(S, b); continue; } }
    if (b.life <= 0 && b.kind === 'block') { S.blocks.splice(i, 1); S.events.push({ t: 'puff', x: (b.x0 + b.x1) / 2, y: (b.y0 + b.y1) / 2 }); continue; }
    S.solids.push(b);
  }
}
function explode(S, b) {
  S.projs.push({ id: S.nid++, owner: b.owner, kind: 'boom', x: (b.x0 + b.x1) / 2, y: (b.y0 + b.y1) / 2, vx: 0, vy: 0, g: 0, r: 44, life: 6, dmg: 18, bkb: 50, gr: 110, ang: 80, hits: 99, hitSet: 0, age: 0, radial: true, selfHit: true, face: 1, flinch: true });
  S.sfx('boom'); S.events.push({ t: 'boom', x: (b.x0 + b.x1) / 2, y: (b.y0 + b.y1) / 2 });
}

function results(S) {
  const rows = S.fighters.map((f) => ({ i: f.i, fid: f.fid, stocks: f.stocks, pct: Math.floor(f.pct), kos: f.kos, dealt: Math.floor(f.dealt), taken: Math.floor(f.taken), ctrl: f.ctrl }));
  rows.sort((a, b) => b.stocks - a.stocks || a.pct - b.pct);
  const tie = rows.length > 1 && rows[0].stocks === rows[1].stocks && rows[0].pct === rows[1].pct;
  return { winner: tie ? -1 : rows[0].i, rows, frames: S.frame, time: TIME - S.time };
}

function stepMatch(S, inputs) {
  S.events.length = 0; S.frame++;
  if (S.phase === 'over') return;
  const F = S.fighters;
  for (let i = 0; i < F.length; i++) readInput(F[i], S.phase === 'play' ? (inputs && inputs[i] | 0) : 0);
  for (const f of F) {
    if (f.hitlag > 0) { f.px = f.x; f.py = f.y; f.hbMove = f.hbMove; if (--f.hitlag === 0 && f.pend) applyLaunch(S, f); continue; }
    stepFighter(S, f);
  }
  updateBlocks(S); updateProjs(S); detectHits(S);
  // blast zones
  const bz = S.stage.blast;
  for (const f of F) {
    if (f.state === 'dead' || f.state === 'out' || f.state === 'respawn') continue;
    if (f.x < bz.l || f.x > bz.r || f.y < bz.t || f.y > bz.b) koFighter(S, f);
  }
  for (const f of F) if (f.flash > 0) f.flash--;
  if (S.phase === 'count') { if (--S.count <= 0) { S.phase = 'play'; S.events.push({ t: 'go' }); S.sfx('go'); } else if (S.count % 50 === 0) { S.events.push({ t: 'count', n: S.count / 50 }); S.sfx('count'); } }
  else if (S.phase === 'play') {
    if (S.frame % 1 === 0) S.time--;
    const alive = F.filter((f) => f.stocks > 0);
    if (alive.length <= 1 && F.length > 1 || S.time <= 0) {
      if (!S.endT) { S.endT = 1; S.events.push({ t: 'end' }); }
    }
    if (S.endT) { S.endT++; if (S.endT > 100) { S.phase = 'over'; S.result = results(S); S.events.push({ t: 'over', result: S.result }); S.sfx('victory'); } }
  }
}

// ---- input: keyboard + Gamepad API -------------------------------------------------------------
const KEYMAP = [
  { KeyA: B.L, KeyD: B.R, KeyW: B.U, KeyS: B.D, KeyJ: B.A, KeyK: B.K, KeyL: B.S, KeyI: B.G, Space: B.J },
  { ArrowLeft: B.L, ArrowRight: B.R, ArrowUp: B.U, ArrowDown: B.D, Numpad1: B.A, Numpad2: B.K, Numpad3: B.S, Numpad0: B.G, Comma: B.A, Period: B.K, Slash: B.S, Semicolon: B.G, Enter: B.J },
];
export class Input {
  constructor() {
    this.down = new Set(); this.enabled = false; this.pads = [];
    const kd = (e) => { const used = KEYMAP[0][e.code] || KEYMAP[1][e.code]; if (used && this.enabled) { e.preventDefault(); } this.down.add(e.code); };
    const ku = (e) => { this.down.delete(e.code); };
    window.addEventListener('keydown', kd); window.addEventListener('keyup', ku); window.addEventListener('blur', () => this.down.clear());
  }
  mask(pn) {
    let m = 0; const map = KEYMAP[pn];
    if (map) for (const k of this.down) m |= map[k] || 0;
    const gp = navigator.getGamepads ? navigator.getGamepads() : [], pads = []; for (const g of gp) if (g && g.connected) pads.push(g);
    const g = pads[pn];
    if (g) {
      const ax = g.axes[0] || 0, ay = g.axes[1] || 0, bt = (i) => g.buttons[i] && g.buttons[i].pressed;
      if (ax < -0.25 || bt(14)) m |= B.L; if (ax > 0.25 || bt(15)) m |= B.R; if (ay < -0.25 || bt(12)) m |= B.U; if (ay > 0.25 || bt(13)) m |= B.D;
      if (bt(2)) m |= B.A; if (bt(1)) m |= B.K; if (bt(0) || bt(3)) m |= B.J; if (bt(6) || bt(7)) m |= B.S; if (bt(4) || bt(5)) m |= B.G;
    }
    return m;
  }
}

// ---- CPU bots (3 levels: reaction 12/8/4 frames; approach / space / punish; recovery) ----------
const dm = (d) => (d > 0 ? B.R : d < 0 ? B.L : 0);
function botInput(S, f) {
  const b = f.bot; if (S.phase !== 'play') return 0;
  if (b.tap > 0) { b.tap--; return b.hold | b.tapMask; }
  const lvl = b.lvl, react = lvl === 1 ? 12 : lvl === 2 ? 8 : 4, miss = lvl === 1 ? 0.3 : lvl === 2 ? 0.12 : 0.03;
  if (S.frame < b.next) return b.hold;
  b.next = S.frame + react; b.hold = 0; b.tapMask = 0;
  let hold = 0, tap = 0; const R = b.rng, st = f.state;
  const fin = () => { b.hold = hold; b.tapMask = tap; if (tap) b.tap = 1; return hold | tap; };
  if (st === 'dead' || st === 'out') return fin();
  const toC = f.x > 0 ? -1 : 1;
  if (st === 'hit') { hold = dm(toC); return fin(); }
  if (st === 'egg' || st === 'held') { b.alt = !b.alt; tap = b.alt ? B.A : B.L; return fin(); }
  if (st === 'respawn') { if (f.respT < 90 && R() < 0.5) tap = B.J; hold = dm(-f.x > 0 ? 1 : -1) * 0; return fin(); }
  if (st === 'ledge') { const L = S.ledges[f.ledge]; if (f.ledgeT > 25) { hold = dm(-L.side); if (R() < 0.3) tap = B.J; } return fin(); }
  let E = null, best = 1e9;
  for (const o of S.fighters) { if (o === f || o.stocks <= 0 || o.state === 'dead' || o.state === 'out') continue; const d = Math.abs(o.x - f.x) + Math.abs(o.y - f.y) * 1.5; if (d < best) { best = d; E = o; } }
  if (!E) return fin();
  const edge = 180, dx = E.x - f.x, dy = E.y - f.y, ad = Math.abs(dx), dirT = sgn(dx) || f.face, hint = f.def.hint, range = hint.range;
  // recover
  if (!f.grounded && st !== 'attack' && (f.x < -edge - 2 || f.x > edge + 2 || f.y > 28)) {
    hold = dm(toC);
    if (lvl === 1 && R() < 0.25) return fin();
    if (f.vy > 0.3 && f.jumps > 0 && f.y > -5) { tap = B.J; if (f.def.flutter) hold |= B.J; }
    else if (f.def.flutter && f.flutterT > 0) hold |= B.J;
    else if (f.vy > 0 && f.jumps === 0 && !f.upUsed && (f.y > 30 || Math.abs(f.x) > edge + 50) && f.state === 'air') { tap = B.K; hold |= B.U | dm(toC); if (f.fid === 'swift') hold = B.U | dm(toC); }
    return fin();
  }
  if (R() < miss) { if (f.grounded && R() < 0.5) hold = dm(dirT); return fin(); }
  const eEnd = (E.state === 'attack' && E.move && E.mf > E.move.dur * 0.55) || E.state === 'land' || E.state === 'stun' || E.state === 'shield';
  if (b.shT > 0) { b.shT -= react; hold = B.S; if (lvl >= 3 && ad < 36 && E.state !== 'attack') { tap = B.G; hold = 0; } return fin(); }
  if (f.grounded && lvl >= 2 && E.state === 'attack' && ad < range + 40 && R() < (lvl === 2 ? 0.45 : 0.8)) { b.shT = 16; hold = B.S; return fin(); }
  if (!f.grounded) { // air game
    if (ad < 44 && Math.abs(dy) < 46) { tap = B.A; hold = dm(dirT); }
    else { hold = dm(dirT); if (dy < -40 && f.jumps > 0 && f.vy > 0.5) tap = B.J; }
    if (Math.abs(f.x) > edge - 20 && f.x * dirT > 0 && !(ad < 44)) hold = dm(toC);
    return fin();
  }
  const onThin = supportAt(S, f) === 2;
  if (dy < -40 && ad < 100) { hold = dm(dirT); tap = B.J; return fin(); }
  if (dy > 45 && onThin && ad < 120) { tap = B.D; hold = 0; return fin(); }
  if (f.face !== dirT && ad < range + 40 && Math.abs(dy) < 40) { hold = dm(dirT); return fin(); }
  if (ad < range + 8 && Math.abs(dy) < 40) {
    if (E.state === 'shield' && lvl >= 2) { tap = B.G; return fin(); }
    if (eEnd || (lvl >= 3 && R() < 0.5)) { tap = B.A; hold = dm(dirT); } else tap = B.A;
    return fin();
  }
  if (hint.ranged && ad > 70 && ad < 220 && Math.abs(dy) < 50 && R() < lvl * 0.22 && S.frame - b.lastB > 40 && f.face === dirT) { tap = B.K; b.lastB = S.frame; return fin(); }
  if (f.fid === 'builder' && lvl >= 2 && ad < 90 && R() < 0.14) { tap = B.K; hold = f.res >= 2 && R() < 0.5 ? B.D : 0; return fin(); }
  if (lvl >= 2 && E.state === 'attack' && !eEnd && ad < range + 35) { hold = dm(-dirT); return fin(); }
  hold = dm(dirT); if (ad > 150 && lvl >= 2 && R() < 0.2) tap = B.J;
  if (Math.abs(f.x) > edge - 12 && f.x * dirT > 0) hold = 0;
  return fin();
}

// ---- renderers ---------------------------------------------------------------------------------
class GLR {
  constructor(canvas, atlas, transparent) {
    const gl = this.gl = canvas.getContext('webgl2', { alpha: !!transparent, premultipliedAlpha: false, antialias: false, preserveDrawingBuffer: true }); this.transparent = !!transparent;
    if (!gl) throw new Error('no webgl2');
    const vs = '#version 300 es\nin vec2 a_p;in vec2 a_uv;in vec4 a_c;in vec3 a_a;uniform vec2 u_res;out vec2 v_uv;out vec4 v_c;out vec3 v_a;void main(){vec2 p=a_p/u_res*2.0-1.0;gl_Position=vec4(p.x,-p.y,0.0,1.0);v_uv=a_uv;v_c=a_c;v_a=a_a;}';
    const fs = '#version 300 es\nprecision mediump float;uniform sampler2D u_t;in vec2 v_uv;in vec4 v_c;in vec3 v_a;out vec4 o;void main(){vec4 t=texture(u_t,v_uv);o=vec4(t.rgb*v_c.rgb+v_a*t.a,t.a*v_c.a);}';
    const mk = (type, src) => { const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s); if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s)); return s; };
    const pr = this.pr = gl.createProgram(); gl.attachShader(pr, mk(gl.VERTEX_SHADER, vs)); gl.attachShader(pr, mk(gl.FRAGMENT_SHADER, fs)); gl.linkProgram(pr);
    if (!gl.getProgramParameter(pr, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(pr));
    gl.useProgram(pr); gl.uniform2f(gl.getUniformLocation(pr, 'u_res'), 480, 270);
    this.MAXQ = 2048; this.vb = new Float32Array(this.MAXQ * 4 * 11); this.n = 0;
    const idx = new Uint16Array(this.MAXQ * 6); for (let q = 0; q < this.MAXQ; q++) idx.set([q * 4, q * 4 + 1, q * 4 + 2, q * 4, q * 4 + 2, q * 4 + 3], q * 6);
    this.vao = gl.createVertexArray(); gl.bindVertexArray(this.vao);
    this.buf = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, this.buf); gl.bufferData(gl.ARRAY_BUFFER, this.vb.byteLength, gl.DYNAMIC_DRAW);
    const ib = gl.createBuffer(); gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ib); gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.STATIC_DRAW);
    const at = (name, size, off) => { const l = gl.getAttribLocation(pr, name); gl.enableVertexAttribArray(l); gl.vertexAttribPointer(l, size, gl.FLOAT, false, 44, off * 4); };
    at('a_p', 2, 0); at('a_uv', 2, 2); at('a_c', 4, 4); at('a_a', 3, 8);
    gl.enable(gl.BLEND); gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    this.texs = new Map(); this.cur = null; this.tint = [1, 1, 1, 1]; this.add = [0, 0, 0]; this.atlas = atlas; this.draws = 0; this.kind = 'webgl2';
    this.dot = atlas.fx.dot;
  }
  setTint(r, g, b, a) { const t = this.tint; t[0] = r; t[1] = g; t[2] = b; t[3] = a; }
  setAdd(r, g, b) { const t = this.add; t[0] = r; t[1] = g; t[2] = b; }
  begin() { const gl = this.gl; gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight); if (this.transparent) gl.clearColor(0, 0, 0, 0); else gl.clearColor(0.03, 0.01, 0.06, 1); gl.clear(gl.COLOR_BUFFER_BIT); this.n = 0; this.cur = null; this.draws = 0; }
  _tex(c) {
    let t = this.texs.get(c); if (t) return t; const gl = this.gl, h = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, h); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, c);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    t = { h, w: c.width, hh: c.height }; this.texs.set(c, t); return t;
  }
  flush() {
    if (!this.n) return; const gl = this.gl;
    gl.bindTexture(gl.TEXTURE_2D, this.cur.h); gl.bindBuffer(gl.ARRAY_BUFFER, this.buf); gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.vb, 0, this.n * 44);
    gl.drawElements(gl.TRIANGLES, this.n * 6, gl.UNSIGNED_SHORT, 0); this.n = 0; this.draws++;
  }
  img(c, sx, sy, sw, sh, dx, dy, dw, dh, flip) {
    if (dx > 480 || dy > 270 || dx + dw < 0 || dy + dh < 0) return;
    const t = this._tex(c); if (t !== this.cur) { this.flush(); this.cur = t; }
    if (this.n >= this.MAXQ) this.flush();
    let u0 = sx / t.w, u1 = (sx + sw) / t.w; const v0 = sy / t.hh, v1 = (sy + sh) / t.hh; if (flip) { const q = u0; u0 = u1; u1 = q; }
    const x0 = Math.round(dx), y0 = Math.round(dy), x1 = x0 + Math.round(dw), y1 = y0 + Math.round(dh), v = this.vb, T = this.tint, A = this.add; let o = this.n * 44;
    const put = (x, y, u, w) => { v[o++] = x; v[o++] = y; v[o++] = u; v[o++] = w; v[o++] = T[0]; v[o++] = T[1]; v[o++] = T[2]; v[o++] = T[3]; v[o++] = A[0]; v[o++] = A[1]; v[o++] = A[2]; };
    put(x0, y0, u0, v0); put(x1, y0, u1, v0); put(x1, y1, u1, v1); put(x0, y1, u0, v1); this.n++;
  }
  rect(x, y, w, h, r, g, b, a) {
    const T = this.tint, ta = T[0], tb = T[1], tc = T[2], td = T[3]; this.setTint(r, g, b, a);
    this.img(this.atlas.canvas, this.dot.x, this.dot.y, 1, 1, x, y, w, h, false); this.setTint(ta, tb, tc, td);
  }
  end() { this.flush(); }
}
class C2R {
  constructor(canvas, atlas, transparent) {
    this.transparent = !!transparent; this.ctx = canvas.getContext('2d', { alpha: !!transparent }); this.atlas = atlas; this.tint = [1, 1, 1, 1]; this.add = [0, 0, 0]; this.draws = 0; this.kind = 'canvas2d'; this.cv = canvas;
  }
  setTint(r, g, b, a) { const t = this.tint; t[0] = r; t[1] = g; t[2] = b; t[3] = a; }
  setAdd(r, g, b) { this.add[0] = r; }
  begin() { const c = this.ctx; c.imageSmoothingEnabled = false; c.globalAlpha = 1; if (this.transparent) c.clearRect(0, 0, 480, 270); else { c.fillStyle = '#08030f'; c.fillRect(0, 0, 480, 270); } this.draws = 0; }
  img(src, sx, sy, sw, sh, dx, dy, dw, dh, flip) {
    if (dx > 480 || dy > 270 || dx + dw < 0 || dy + dh < 0) return; const c = this.ctx; c.globalAlpha = this.tint[3];
    dx = Math.round(dx); dy = Math.round(dy); dw = Math.round(dw); dh = Math.round(dh);
    if (flip) { c.save(); c.translate(dx + dw, dy); c.scale(-1, 1); c.drawImage(src, sx, sy, sw, sh, 0, 0, dw, dh); c.restore(); }
    else c.drawImage(src, sx, sy, sw, sh, dx, dy, dw, dh);
    if (this.add[0] > 0.1) { c.globalAlpha = Math.min(1, this.add[0]) * 0.8 * this.tint[3]; c.globalCompositeOperation = 'lighter'; if (flip) { c.save(); c.translate(dx + dw, dy); c.scale(-1, 1); c.drawImage(src, sx, sy, sw, sh, 0, 0, dw, dh); c.restore(); } else c.drawImage(src, sx, sy, sw, sh, dx, dy, dw, dh); c.globalCompositeOperation = 'source-over'; }
    c.globalAlpha = 1; this.draws++;
  }
  rect(x, y, w, h, r, g, b, a) { const c = this.ctx; c.globalAlpha = 1; c.fillStyle = 'rgba(' + ((r * 255) | 0) + ',' + ((g * 255) | 0) + ',' + ((b * 255) | 0) + ',' + a + ')'; c.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h)); }
  end() {}
}
export function createRenderer(canvas, atlas, force2d, transparent) {
  if (!force2d) { try { return new GLR(canvas, atlas, transparent); } catch (e) { console.warn('smash: webgl2 unavailable, using canvas2d', e.message); } }
  return new C2R(canvas, atlas, transparent);
}

// ---- scene drawing -----------------------------------------------------------------------------
const PCOL = [[1, 0.35, 0.35], [0.35, 0.55, 1], [1, 0.85, 0.3], [0.35, 0.9, 0.5]];
export const PLAYER_COLORS = ['#ff5a5a', '#5a8cff', '#ffd84d', '#5ae680'];

function animOf(f) {
  const t = f.animT;
  switch (f.state) {
    case 'idle': return ['idle', (t / 10 | 0) % 6];
    case 'run': return ['run', (t / 3 | 0) % 8];
    case 'crouch': case 'dodge': case 'getup': return ['crouch', 0];
    case 'land': return ['land', Math.min(2, f.st >> 1)];
    case 'air': return f.flutterT > 0 ? ['fall', (t >> 2) & 3] : f.vy < 0 ? ['jump', f.vy < -2 ? 0 : f.vy < -0.6 ? 1 : 2] : ['fall', (t >> 3) & 3];
    case 'attack': {
      const m = f.move; if (!m) return ['idle', 0]; const tt = f.mf;
      if (m.dash) return ['dive', tt > 5 ? 1 : 0];
      let i; if (m.hits.length) { const s = m.hits[0].s, e = m.hits[m.hits.length - 1].e; i = tt < s / 2 ? 0 : tt < s ? 1 : tt < e ? 2 : 3; } else { const u = tt / m.dur; i = u < 0.3 ? 0 : u < 0.5 ? 1 : u < 0.75 ? 2 : 3; }
      return [m.pose || 'swing', i];
    }
    case 'hit': return f.tumble ? ['tumble', (t >> 1) & 7] : ['hit', f.hitstun > 10 ? 1 : 0];
    case 'shield': return ['shield', (f.st >> 3) & 1];
    case 'roll': return ['roll', (f.st >> 2) & 3];
    case 'grab': case 'grabHold': return ['grab', f.st > 3 ? 1 : 0];
    case 'held': case 'egg': return ['hit', 0];
    case 'ledge': return ['ledge', (t >> 4) & 1];
    case 'stun': return ['hit', 1];
    case 'respawn': return ['fall', (t >> 3) & 3];
  }
  return null;
}

function circleStrips(R, cx, cy, rad, r, g, b, a) { for (let y = -rad; y <= rad; y += 2) { const w = Math.sqrt(Math.max(0, rad * rad - y * y)); R.rect(cx - w, cy + y, w * 2, 2, r, g, b, a); } }
function ring(R, cx, cy, rad, r, g, b, a) { for (let k = 0; k < 20; k++) { const an = k * 18; R.rect(cx + cosD(an) * rad - 1, cy + sinD(an) * rad - 1, 2, 2, r, g, b, a); } }

export function newView() { return { x: 0, y: -30, z: 1, shake: 0, flash: 0, fx: [], t: 0, debug: false }; }

export function updateView(view, S) {
  let minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9, n = 0;
  for (const f of S.fighters) { if (f.state === 'dead' || f.state === 'out') continue; n++; minx = Math.min(minx, f.x); maxx = Math.max(maxx, f.x); miny = Math.min(miny, f.y - 50); maxy = Math.max(maxy, f.y + 10); }
  if (!n) { minx = -60; maxx = 60; miny = -80; maxy = 20; }
  const w = maxx - minx + 170, h = maxy - miny + 90, z = clamp(Math.min(480 / w, 270 / h), 0.75, 1.5);
  const b = S.stage.blast; let cx = (minx + maxx) / 2, cy = (miny + maxy) / 2 - 8;
  cx = clamp(cx, b.l + 240 / z, b.r - 240 / z); cy = clamp(cy, b.t + 135 / z, b.b - 135 / z);
  view.x += (cx - view.x) * 0.1; view.y += (cy - view.y) * 0.1; view.z += (z - view.z) * 0.08;
  if (view.shake > 0) view.shake *= 0.85; if (view.shake < 0.1) view.shake = 0; if (view.flash > 0) view.flash--;
  view.t++;
  for (let i = view.fx.length - 1; i >= 0; i--) { const p = view.fx[i]; p.x += p.vx; p.y += p.vy; p.vy += p.g; if (++p.a >= p.life) view.fx.splice(i, 1); }
}
export function viewEvent(view, e) {
  const burst = (x, y, n, sp, life, kind, col) => { for (let i = 0; i < n; i++) { const a = (i * 360 / n) + (i * 37 % 29), s = sp * (0.5 + ((i * 7) % 5) / 5); view.fx.push({ x, y, vx: cosD(a) * s, vy: -sinD(a) * s, g: 0.04, a: 0, life, kind, col }); } };
  if (e.t === 'hit') { burst(e.x, e.y, 5 + Math.min(8, e.dmg | 0), 2.6, 12, 'star', null); view.shake = Math.max(view.shake, Math.min(5, 1 + e.dmg / 4)); }
  else if (e.t === 'ko') { view.flash = 3; view.shake = 7; burst(e.x, e.y, 26, 5, 30, 'star', (view.pcol || PCOL)[e.who % 4]); }
  else if (e.t === 'puff') burst(e.x, e.y, 6, 1.1, 14, 'puff', null);
  else if (e.t === 'boom') { view.shake = 6; burst(e.x, e.y, 22, 3.5, 22, 'puff', [1, 0.6, 0.2]); view.fx.push({ x: e.x, y: e.y, vx: 0, vy: 0, g: 0, a: 0, life: 8, kind: 'blast' }); }
  else if (e.t === 'shield' || e.t === 'spark' || e.t === 'break') burst(e.x, e.y, 6, 1.8, 10, 'star', [0.5, 0.9, 1]);
}

export function drawScene(R, S, atlas, vis, view, alpha, debug) {
  const A = atlas.canvas, fx = atlas.fx, z = view.z;
  const sh = view.shake, shx = sh ? (((view.t * 7) % 5) - 2) * sh * 0.35 : 0, shy = sh ? (((view.t * 3) % 5) - 2) * sh * 0.35 : 0;
  const cam = { x: view.x + shx, y: view.y + shy, zoom: z };
  const X = (wx) => (wx - cam.x) * z + 240, Y = (wy) => (wy - cam.y) * z + 135;
  R.setTint(1, 1, 1, 1); R.setAdd(0, 0, 0);
  if (vis) vis.drawBack(R, cam, view.t); if (vis) vis.drawStage(R, cam);
  // respawn pads
  for (const f of S.fighters) if (f.state === 'respawn') R.img(A, fx.pad.x, fx.pad.y, 44, 10, X(f.x - 22), Y(f.y), 44 * z, 10 * z);
  // blocks
  for (const b of S.blocks) {
    const w = (b.x1 - b.x0) * z, h = (b.y1 - b.y0) * z;
    if (b.kind === 'tnt') { const fl = b.fuse < 40 && (b.fuse >> 2) & 1; R.setAdd(fl ? 0.7 : 0, fl ? 0.7 : 0, fl ? 0.7 : 0); R.img(A, fx.tnt.x, fx.tnt.y, 32, 32, X(b.x0), Y(b.y0), w, h); R.setAdd(0, 0, 0); }
    else { R.setTint(1, 1, 1, b.life < 40 && (b.life >> 2) & 1 ? 0.45 : 1); R.img(A, fx.block.x, fx.block.y, 32, 32, X(b.x0), Y(b.y0), w, h); R.setTint(1, 1, 1, 1); }
  }
  // fighters (back to front by index)
  for (const f of S.fighters) {
    if (f.state === 'dead' || f.state === 'out') continue;
    const an = animOf(f); if (!an) continue;
    let ix = f.x, iy = f.y; if (Math.abs(f.x - f.px) < 30 && Math.abs(f.y - f.py) < 30 && f.hitlag === 0) { ix = f.px + (f.x - f.px) * alpha; iy = f.py + (f.y - f.py) * alpha; }
    if (f.state === 'ledge') { ix = f.x; iy = f.y; }
    const fr = atlas.frame(f.fid, an[0], an[1]), blink = f.inv > 0 && f.state !== 'shield' && f.state !== 'ledge' && (view.t >> 2) & 1;
    if (f.move && f.move.cart && f.state === 'attack') R.img(A, fx.cart.x, fx.cart.y, 40, 22, X(ix - 20), Y(iy - 20), 40 * z, 22 * z, f.face < 0);
    { const tc = (view.tcol && view.tcol[f.i]) || [1, 1, 1]; R.setTint(tc[0], tc[1], tc[2], blink ? 0.45 : 1); }
    if (f.hitlag > 0 && f.flash > 0 || f.armorNow) R.setAdd(0.55, 0.55, 0.55);
    if (f.state === 'attack' && f.move && f.move.dash && f.mf >= 6 && f.mf < 14) { R.setTint(1, 0.8, 1, 0.35); for (let k = 1; k <= 3; k++) R.img(A, fr[0], fr[1], 32, 48, X(ix - 16 - f.dashx * 12 * k), Y(iy - 46 - f.dashy * 12 * k), 32 * z, 48 * z, f.face < 0); R.setTint(1, 1, 1, 1); }
    R.img(A, fr[0], fr[1], 32, 48, X(ix - 16), Y(iy - 46), 32 * z, 48 * z, f.face < 0);
    R.setTint(1, 1, 1, 1); R.setAdd(0, 0, 0);
    if (f.state === 'egg') R.img(A, fx.egg.x, fx.egg.y, 12, 15, X(ix - 15), Y(iy - 40), 30 * z, 40 * z);
    if (f.state === 'shield') { const D = (24 + 28 * (f.sh / 180)) * z; R.setTint(1, 1, 1, 0.85); R.img(A, fx.shield.x, fx.shield.y, 48, 48, X(ix) - D / 2, Y(iy - 20) - D / 2, D, D); R.setTint(1, 1, 1, 1); }
    if (f.reflectT > 0) { R.setTint(0.6, 1, 1, 0.7); const D = 52 * z; R.img(A, fx.shield.x, fx.shield.y, 48, 48, X(ix) - D / 2, Y(iy - 22) - D / 2, D, D); R.setTint(1, 1, 1, 1); }
    // player marker
    const pc = (view.pcol || PCOL)[f.i % 4]; if (f.state !== 'respawn' || true) { const mx = Math.round(X(ix)), my = Math.round(Y(iy - 54) - 2); R.rect(mx - 3, my - 3, 7, 1, pc[0], pc[1], pc[2], 1); R.rect(mx - 2, my - 2, 5, 1, pc[0], pc[1], pc[2], 1); R.rect(mx - 1, my - 1, 3, 1, pc[0], pc[1], pc[2], 1); R.rect(mx, my, 1, 1, pc[0], pc[1], pc[2], 1); }
  }
  // projectiles
  for (const p of S.projs) {
    if (p.kind === 'plasma') R.img(A, fx.plasma.x, fx.plasma.y, 12, 12, X(p.x - 6), Y(p.y - 6), 12 * z, 12 * z);
    else if (p.kind === 'egg') R.img(A, fx.egg.x, fx.egg.y, 12, 15, X(p.x - 6), Y(p.y - 7), 12 * z, 15 * z);
    else if (p.kind === 'bolt') R.img(A, fx.bolt.x, fx.bolt.y, 12, 5, X(p.x - 6), Y(p.y - 2), 12 * z, 5 * z, p.vx < 0);
    else if (p.kind === 'boom') { const u = 1 - p.life / 6; circleStrips(R, X(p.x), Y(p.y), (14 + 30 * u) * z, 1, 0.8 - u * 0.4, 0.3, 0.8 - u * 0.5); }
  }
  // particles
  for (const p of view.fx) {
    const u = p.a / p.life, c = p.col || [1, 1, 1];
    if (p.kind === 'blast') { circleStrips(R, X(p.x), Y(p.y), (10 + p.a * 6) * z, 1, 0.9, 0.5, 0.5 * (1 - u)); continue; }
    if (p.kind === 'puff') { R.setTint(c[0] * 0.9, c[1] * 0.9, c[2] * 0.9, 0.8 * (1 - u)); R.img(A, fx.dot.x, fx.dot.y, 2, 2, X(p.x), Y(p.y), (3 - u * 2) * z, (3 - u * 2) * z); R.setTint(1, 1, 1, 1); continue; }
    R.setTint(c[0], c[1], c[2], 1 - u * 0.6); const s = (1 + (u < 0.3 ? 1 : 0)) * z; R.img(A, fx.star.x, fx.star.y, 5, 5, X(p.x) - 2.5 * s, Y(p.y) - 2.5 * s, 5 * s, 5 * s); R.setTint(1, 1, 1, 1);
  }
  if (vis) vis.drawFront(R, cam);
  if (debug) {
    for (const f of S.fighters) {
      if (f.state === 'dead' || f.state === 'out') continue;
      for (const c of hurtCaps(f, [])) { ring(R, X(c[0]), Y(c[1]), c[4] * z, 0.2, 1, 0.3, 0.9); ring(R, X(c[2]), Y(c[3]), c[4] * z, 0.2, 1, 0.3, 0.9); }
      const m = f.hbMove; if (m && f.state === 'attack') for (const h of m.hits) if (f.hbT >= h.s && f.hbT < h.e) { const c = hitCapsuleWorld(f, h); ring(R, X(c[0]), Y(c[1]), c[4] * z, 1, 0.2, 0.2, 1); ring(R, X(c[2]), Y(c[3]), c[4] * z, 1, 0.2, 0.2, 1); }
    }
    for (const b of S.stage.blast ? [S.stage.blast] : []) { R.rect(X(b.l), Y(b.t), 1, (b.b - b.t) * z, 1, 0.3, 0.3, 0.5); R.rect(X(b.r), Y(b.t), 1, (b.b - b.t) * z, 1, 0.3, 0.3, 0.5); }
  }
  if (view.flash > 0) R.rect(0, 0, 480, 270, 1, 1, 1, view.flash > 1 ? 0.85 : 0.4);
}

// ---- game loop glue ---------------------------------------------------------------------------
export function createGame(o) {
  const canvas = o.canvas, ui = o.ui || null, input = new Input();
  const atlas = buildAtlas(FIGHTER_IDS), vis = createStageVisual(STAGES.plateau);
  const R = createRenderer(canvas, atlas, /[?&]canvas2d/.test(location.search));
  const g = { sim: null, view: newView(), input, atlas, R, inject: [null, null, null, null], paused: false, debug: /[?&]smashdebug/.test(location.search), cfg: null, simMs: 0, simSteps: 0, running: false };
  let acc = 0, last = 0;
  function masks() {
    const S = g.sim, out = [];
    for (const f of S.fighters) {
      const inj = g.inject[f.i];
      out.push(inj != null ? inj : f.ctrl === 'p1' ? input.mask(0) : f.ctrl === 'p2' ? input.mask(1) : f.bot ? botInput(S, f) : 0);
    }
    return out;
  }
  function drain() {
    for (const e of g.sim.events) {
      if (e.t === 'sfx') play(e.n); else viewEvent(g.view, e);
      if (ui) ui.onEvent(e, g);
    }
  }
  g.step = function (n) {
    n = n || 1;
    for (let k = 0; k < n; k++) {
      if (!g.sim || g.sim.phase === 'over' && g.sim.result && g.sim.frame > 0 && g._doneSent) break;
      const t0 = performance.now(); g.sim.step(masks()); g.simMs += performance.now() - t0; g.simSteps++;
      drain(); updateView(g.view, g.sim);
      if (g.sim.phase === 'over') g._doneSent = true;
    }
    g.render(1);
  };
  g.render = function (alpha) {
    if (!g.sim) return; R.begin(); drawScene(R, g.sim, atlas, vis, g.view, alpha, g.debug); R.end(); if (ui) ui.update(g.sim, g);
  };
  g.start = function (cfg) {
    g.cfg = cfg; g.sim = createMatch(cfg); g.view = newView(); g._doneSent = false; g.inject = [null, null, null, null]; g.paused = false; acc = 0;
    input.enabled = true; updateView(g.view, g.sim); g.view.x = (g.sim.fighters.reduce((a, f) => a + f.x, 0) / g.sim.fighters.length); if (ui) ui.onStart(g.sim, g); g.render(1);
    if (!g.running) { g.running = true; last = performance.now(); requestAnimationFrame(loop); }
  };
  g.stop = function () { g.sim = null; input.enabled = false; };
  g.pause = function (v) { g.paused = v == null ? !g.paused : v; if (ui) ui.onPause(g.paused); };
  function loop(ts) {
    requestAnimationFrame(loop);
    const dt = Math.min(100, ts - last); last = ts;
    if (!g.sim || g.paused) { if (g.sim) g.render(1); return; }
    acc += dt; let n = 0;
    while (acc >= 1000 / 60 && n < 5) {
      acc -= 1000 / 60; n++;
      if (g.sim.phase === 'over' && g._doneSent) { acc = 0; break; }
      const t0 = performance.now(); g.sim.step(masks()); g.simMs += performance.now() - t0; g.simSteps++; drain(); updateView(g.view, g.sim);
      if (g.sim.phase === 'over') g._doneSent = true;
    }
    if (n === 5) acc = 0;
    g.render(acc / (1000 / 60));
  }
  g.bench = function (steps) { // sim cost only (no render)
    const S = createMatch(g.cfg || { players: [{ fid: 'pilot', ctrl: 'cpu', level: 3 }, { fid: 'swift', ctrl: 'cpu', level: 3 }] }); let worst = 0; const t0 = performance.now();
    for (let i = 0; i < steps; i++) { const a = performance.now(); S.step(S.fighters.map((f) => (f.bot ? botInput(S, f) : 0))); worst = Math.max(worst, performance.now() - a); if (S.phase === 'over') break; }
    return { avgMs: (performance.now() - t0) / Math.max(1, S.frame), worstMs: worst, frames: S.frame };
  };
  return g;
}
export { botInput };

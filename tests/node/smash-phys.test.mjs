import test from 'node:test';
import assert from 'node:assert/strict';
import { PHYS, physOf, knockback, hitlagOf, hitstunOf, launchSpeed, diBend, locoStep, jumpSpeed } from '../../js/smash-phys.js';
import { FIGHTERS } from '../../js/smash-fighters.js';
import { createMatch, ringStage, sinD, cosD } from '../../js/smash-engine.js';
import { mk, run, B, SAMPLES } from './smash-arena.mjs';

const H = PHYS.H;
const near = (a, b, tol, msg) => assert.ok(Math.abs(a - b) <= tol, (msg || '') + ' got ' + a + ' want ' + b + ' +-' + tol);

test('constants: gravity 0.095 u/f^2 at REF scale, fall cap 1.7, fast fall 1.6x, air accel 0.04, air speed 0.95, traction 0.06', () => {
  near(PHYS.grav / PHYS.REF, 0.095, 1e-9); near(PHYS.fallCap / PHYS.REF, 1.7, 1e-9); assert.equal(PHYS.fastFall, 1.6);
  near(PHYS.airAccel / PHYS.REF, 0.04, 1e-9); near(PHYS.airSpeed / PHYS.REF, 0.95, 1e-9); near(PHYS.traction / PHYS.REF, 0.06, 1e-9);
  near(PHYS.runMin / PHYS.REF, 1.2, 1e-9); near(PHYS.runMax / PHYS.REF, 1.8, 1e-9);
  assert.equal(PHYS.squat, 3); assert.equal(PHYS.buffer, 6);
});
test('run speed 1.2..1.8 by weight (heavier = slower); one gravity for everyone within +-6 %', () => {
  const ids = Object.keys(FIGHTERS), runs = ids.map((i) => physOf(FIGHTERS[i]).run);
  assert.ok(Math.min(...runs) >= PHYS.runMin - 1e-9 && Math.max(...runs) <= PHYS.runMax + 1e-9);
  assert.ok(physOf(FIGHTERS.hum).run > physOf(FIGHTERS.hauler).run);
  for (const i of ids) near(physOf(FIGHTERS[i]).g / PHYS.grav, 1, 0.061);
});

// jump heights measured in the sim, standing fighter, hold / tap of the jump key
function apex(S, f, hold, from) {
  let top = f.y;
  for (let i = 0; i < 120; i++) { S.step([hold(i), 0]); if (f.y < top) top = f.y; if (i > 6 && f.vy > 0.5) break; }
  return from - top;
}
test('full hop 1.45 H, short hop 0.55 H (measured), jump-squat 3 frames', () => {
  const S = mk(); const f = S.fighters[0]; run(S, 30);
  f.x = 0; assert.ok(f.grounded);
  let sq = 0; const y0 = f.y;
  S.step([B.J, 0]); assert.equal(f.state, 'squat'); for (let i = 0; i < 6 && f.state === 'squat'; i++) { sq++; S.step([B.J, 0]); }
  assert.ok(sq >= 2 && sq <= 3, 'squat frames ' + sq);
  let top = f.y; for (let i = 0; i < 120; i++) { S.step([B.J, 0]); if (f.y < top) top = f.y; if (f.grounded) break; }
  near(y0 - top, 1.45 * H, 0.04 * 1.45 * H, 'full hop');
  run(S, 20); const y1 = f.y; S.step([B.J, 0]); S.step([0, 0]);
  let t2 = f.y; for (let i = 0; i < 120; i++) { S.step([0, 0]); if (f.y < t2) t2 = f.y; if (i > 4 && f.grounded) break; }
  near(y1 - t2, 0.55 * H, 0.1 * 0.55 * H, 'short hop');
});
test('double jump apex 1.3 H above the jump point', () => {
  const S = mk(); const f = S.fighters[0]; run(S, 30); f.x = 0;
  S.step([B.J, 0]); for (let i = 0; i < 4; i++) S.step([B.J, 0]);
  for (let i = 0; i < 80 && f.vy < -0.1; i++) S.step([B.J, 0]);        // coast to the first apex
  run(S, 1);
  const y0 = f.y; assert.equal(f.jumps, 1); S.step([0, 0]); S.step([B.J, 0]);        // fresh press = double jump
  let top = f.y; for (let i = 0; i < 60; i++) { S.step([B.J, 0]); if (f.y < top) top = f.y; if (f.vy > 0) break; }
  near(y0 - top, 1.3 * H, 0.06 * 1.3 * H, 'double jump');
});
test('fall speed capped at 1.7 u/f (x weight), fast fall 1.6x that', () => {
  const S = mk(); const f = S.fighters[0], P = physOf(f.def);
  f.y = -450; f.x = 0; f.vy = 0; f.grounded = false; f.landed = false; f.state = 'air'; let mx = 0;
  for (let i = 0; i < 100; i++) { S.step([0, 0]); mx = Math.max(mx, f.vy); }
  near(mx, P.fall, 0.01); near(P.fall, 1.7 * PHYS.REF, 0.1);
  f.y = -450; f.vy = 0; f.grounded = false; f.landed = false; f.state = 'air'; S.step([0, 0]); S.step([B.D, 0]); mx = 0;
  for (let i = 0; i < 200; i++) { S.step([B.D, 0]); mx = Math.max(mx, f.vy); if (f.y > 40) break; }
  near(mx, P.fall * 1.6, 0.01, 'fast fall');
});
test('air accel 0.04 u/f^2 and air speed cap 0.95 u/f', () => {
  const S = mk(); const f = S.fighters[0]; f.y = -450; f.x = 0; f.vx = 0; f.vy = 0; f.grounded = false; f.landed = false; f.state = 'air';
  S.step([B.R, 0]); near(f.vx, PHYS.airAccel, 0.004, 'one frame of drift');
  for (let i = 0; i < 40; i++) S.step([B.R, 0]); near(f.vx, PHYS.airSpeed, 0.004, 'cap');
});
test('external forces are ignored: wind / gravity / planet inputs change nothing', () => {
  const runIt = (poke) => {
    const cfg = { players: [{ fid: 'pilot', ctrl: 'net' }, { fid: 'swift', ctrl: 'net' }], stageDef: ringStage(SAMPLES, 'ext'), seed: 5, stocks: 2 };
    poke.cfg && Object.assign(cfg, poke.cfg);
    const S = createMatch(cfg); poke.S && Object.assign(S, poke.S);
    for (let i = 0; i < 400; i++) { if (poke.live && i === 200) Object.assign(S, poke.live); if (poke.def && i === 200) { S.fighters[0].windX = 9; } S.step([i % 90 < 40 ? B.R : B.L | (i % 7 === 0 ? B.J : 0), i % 120 < 60 ? B.L : B.R]); }
    return S.hash();
  };
  const base = runIt({});
  assert.equal(runIt({ cfg: { gravity: 0.9, wind: 7, windX: 5, planetGravity: 3, external: { x: 4, y: 4 } } }), base, 'cfg forces');
  assert.equal(runIt({ S: { wind: 6, windX: 3, gravity: 2, gust: 5, force: [3, 3], planetGravity: 9 } }), base, 'sim fields');
  assert.equal(runIt({ live: { wind: -8, gravity: 4, windX: 9, gust: 3 }, def: 1 }), base, 'mid-match');
});

test('knockback formula: ((d/10 + d*p/20) * 200/(w+100) * 1.4 + 18) * s + b', () => {
  const d = 12, p = 60, w = 100, s = 90, b = 20;
  near(knockback(d, p, w, s, b), ((d / 10 + d * p / 20) * (200 / (w + 100)) * 1.4 + 18) * 0.9 + 20, 1e-9);
  assert.ok(knockback(12, 60, 62, 90, 20) > knockback(12, 60, 135, 90, 20), 'light flies farther');
  assert.equal(hitlagOf(12), 8); assert.equal(hitlagOf(2), 4); assert.equal(hitlagOf(30), 14);
  assert.equal(hitstunOf(100), 40); near(launchSpeed(100), 3 * PHYS.REF, 1e-9);
});
test('DI bends a launch by at most 18 degrees', () => {
  const lx = cosD(40), ly = sinD(40);
  for (const [sx, sy] of [[0, 1], [0, -1], [1, 0], [-1, 0], [1, 1], [-1, 1]]) {
    const v = diBend(lx, ly, sx, sy, cosD, sinD), a0 = Math.atan2(ly, lx), a1 = Math.atan2(v[1], v[0]);
    assert.ok(Math.abs(a1 - a0) * 180 / Math.PI <= 18.01, 'bend ' + (Math.abs(a1 - a0) * 180 / Math.PI));
  }
  const none = diBend(lx, ly, 0, 0, cosD, sinD); assert.equal(none[0], lx);
});

test('state machine is pure: plain objects in, same transitions out', () => {
  const mkf = () => ({ state: 'idle', st: 0, grounded: true, landed: false, x: 0, y: 0, vx: 0, vy: 0, face: 1, inp: 0, ed: 0, bufA: 0, bufB: 0, bufG: 0, bufJ: 0, bufC: 0, jumps: 0, ff: false, jumpT: 99, adUsed: 0, flutterT: 0, sh: 180, dropT: 0, inv: 0, lag: 0, helpless: 0, def: FIGHTERS.pilot, upUsed: false });
  const f = mkf(), P = physOf(f.def);
  assert.equal(locoStep(f, P, null), ''); assert.equal(f.state, 'idle');
  f.bufJ = 6; f.inp = B.J; f.st++; locoStep(f, P, null); assert.equal(f.state, 'squat');
  let code = ''; for (let i = 0; i < 6 && !code; i++) { f.st++; code = locoStep(f, P, null); }
  assert.equal(code, 'JUMP'); assert.equal(f.state, 'air'); assert.equal(f.grounded, false); near(f.vy, -P.vFull, 1e-9);
  // run + pivot
  const g = mkf(); g.inp = B.R; for (let i = 0; i < 20; i++) { g.st++; locoStep(g, P, null); } assert.equal(g.state, 'run'); near(g.vx, P.run, 1e-9);
  g.inp = B.L; g.st++; locoStep(g, P, null); assert.equal(g.state, 'turn', 'pivot at run speed');
  for (let i = 0; i < PHYS.turn + 1; i++) { g.st++; locoStep(g, P, null); } assert.equal(g.face, -1);
  // request codes
  const h = mkf(); h.bufA = 3; assert.equal(locoStep(h, P, null), 'A'); h.bufA = 0; h.bufB = 2; assert.equal(locoStep(h, P, null), 'B'); h.bufB = 0; h.inp = B.S; assert.equal(locoStep(h, P, null), 'S');
  assert.equal(jumpSpeed(0.171, 52.2) > 3.9, true);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { maroTable, MARO_K as K, MA, FX } from '../../js/ship-styles.js';

const DT = 1 / 120;
const fwd = { wx: 0, wz: -1, m: 1 };
const none = { wx: 0, wz: 0, m: 0 };
const io = (...o) => Object.assign({}, ...o);
const run = (T, i, secs) => { for (let t = 0; t < secs; t += DT) T.step(i, DT); };
const air = (T, v = 9.75) => { T.s.grounded = false; T.s.y = 3; T.s.vy = 2; T.s.mode = MA.JUMP1; T.s.vz = -3; };
const land = (T, i = none) => { for (let n = 0; n < 2000 && !T.s.grounded; n++) T.step(i, DT); };

test('wall kick: inside the 0.17 s window only, 9.75 up, 4.5 out along the normal', () => {
  const T = maroTable(); air(T);
  T.step(io(fwd, { jumpP: true, wallT: 0.2, wallNx: 1, wallNz: 0 }), DT); assert.notEqual(T.s.mode, MA.WALLKICK);   // too late
  T.step(io(fwd, { jumpP: true, wallT: 0.16, wallNx: 1, wallNz: 0 }), DT);
  assert.equal(T.s.mode, MA.WALLKICK);
  assert.ok(Math.abs(T.s.vy - (K.WK_Y - K.G * DT)) < 0.05, 'vy ' + T.s.vy);
  assert.ok(Math.abs(T.s.vx - 4.5) < 0.6, 'vx ' + T.s.vx);   // steering may add a little
});

test('dive: 9 H/s forward, lands into belly slide with 0.55/s friction, hop-out on jump', () => {
  const T = maroTable(); air(T);
  T.step(io(fwd, { diveP: true }), DT); assert.equal(T.s.mode, MA.DIVE);
  assert.ok(Math.abs(Math.hypot(T.s.vx, T.s.vz) - 9) < 1e-6);
  land(T, fwd); assert.equal(T.s.mode, MA.BELLY);
  const v0 = Math.hypot(T.s.vx, T.s.vz); T.step(none, 1); const v1 = Math.hypot(T.s.vx, T.s.vz);
  assert.ok(Math.abs(v1 / v0 - K.BELLY_FR) < 0.01, 'ratio ' + v1 / v0);
  T.step(io(none, { jumpP: true }), DT); assert.equal(T.s.mode, MA.JUMP1); assert.ok(T.s.vy > 5); assert.ok(Math.hypot(T.s.vx, T.s.vz) > 3);
  const U = maroTable(); U.s.mode = MA.BELLY; U.s.vz = -3; run(U, none, 6); assert.equal(U.s.mode, MA.IDLE);
});

test('ground pound: 0.33 s hang, 56 H/s^2 fall, stomp fx 3 H / 12 dmg, then recovery', () => {
  const T = maroTable(); air(T); T.s.y = 6;
  T.step(io(none, { poundP: true }), DT); assert.equal(T.s.mode, MA.POUND);
  let t = 0; const y0 = T.s.y; while (t < 0.3) { T.step(none, DT); t += DT; } assert.ok(Math.abs(T.s.y - y0) < 1e-6, 'still hanging');
  run(T, none, 0.1); assert.ok(T.s.vy < -3, 'falling ' + T.s.vy);
  let fx = 0, n = 0; while (!T.s.grounded && n++ < 3000) { T.step(none, DT); }
  assert.equal(T.s.fx, FX.STOMP); assert.equal(T.s.fxR, 3); assert.equal(T.s.fxDmg, 12); assert.equal(T.s.mode, MA.POUNDL);
  T.step(none, DT); assert.equal(T.s.fx, FX.NONE);
  run(T, none, 0.4); assert.equal(T.s.mode, MA.IDLE);
  // fall acceleration is 56 not 22.5
  const U = maroTable(); air(U); U.s.y = 50; U.step(io(none, { poundP: true }), DT); run(U, none, 0.33 + 0.2); assert.ok(Math.abs(U.s.vy + 56 * 0.2) < 1.5, 'vy ' + U.s.vy);
});

test('ledge grab: within 0.19 H, speed < 7.5, auto; hop up on jump, no regrab right after', () => {
  const T = maroTable(); air(T); T.s.y = 3; T.s.vy = -1; T.s.vz = -2;
  T.step(io(none, { ledgeTop: 3 + K.HANDS + 0.25, ledgeNx: 0, ledgeNz: 1 }), DT); assert.notEqual(T.s.mode, MA.LEDGE);   // out of reach
  T.step(io(none, { ledgeTop: T.s.y + K.HANDS + 0.1, ledgeNx: 0, ledgeNz: 1 }), DT); assert.equal(T.s.mode, MA.LEDGE);
  const top = T.s.y + K.HANDS; T.step(none, 1); assert.equal(T.s.mode, MA.LEDGE); assert.equal(T.s.y, top - K.HANDS);   // hangs
  T.step(io(none, { jumpP: true }), DT); assert.equal(T.s.mode, MA.JUMP1); assert.ok(T.s.vy > 6); assert.ok(T.s.vz < 0);
  const U = maroTable(); air(U); U.s.vy = -1; U.s.vz = -8; U.step(io(none, { ledgeTop: U.s.y + K.HANDS }), DT); assert.notEqual(U.s.mode, MA.LEDGE);   // too fast
  const V = maroTable(); air(V); V.s.vy = 3; V.step(io(none, { ledgeTop: V.s.y + K.HANDS }), DT); assert.notEqual(V.s.mode, MA.LEDGE);                // still rising
});

test('steep slide: slope > 0.62 slides downhill and accelerates; 0.6 does not', () => {
  const T = maroTable(); run(T, io(none, { slope: 0.7, slopeX: 1, slopeZ: 0 }), 0.5);
  assert.equal(T.s.mode, MA.SLIDE); assert.ok(T.s.vx > 3); const v = T.s.vx; run(T, io(none, { slope: 0.7, slopeX: 1, slopeZ: 0 }), 2); assert.ok(T.s.vx > v); assert.ok(T.s.vx <= K.SLIDE_MAX + 1e-9);
  const U = maroTable(); run(U, io(none, { slope: 0.6, slopeX: 1, slopeZ: 0 }), 0.5); assert.notEqual(U.s.mode, MA.SLIDE); assert.equal(U.s.vx, 0);
  T.step(io(none, { slope: 0.7, slopeX: 1, slopeZ: 0, jumpP: true }), DT); assert.ok(T.s.vy > 7);   // can jump out
});

test('bonk: > 3 H/s into a wall on the ground, 0.5 s stun; slower or jumping does not', () => {
  const wall = { wallHit: true, wallNx: 0, wallNz: 1 };
  const T = maroTable(); run(T, fwd, 2); T.step(io(fwd, wall), DT);
  assert.equal(T.s.mode, MA.BONK); assert.equal(T.s.fx, FX.BONK); assert.ok(T.s.vz > 0);
  run(T, fwd, 0.45); assert.equal(T.s.mode, MA.BONK); assert.equal(T.s.vx, 0);   // controls ignored
  run(T, none, 0.1); assert.equal(T.s.mode, MA.IDLE);
  const U = maroTable(); U.s.vz = -2.5; U.step(io(fwd, wall), DT); assert.notEqual(U.s.mode, MA.BONK);
  const V = maroTable(); run(V, fwd, 2); V.step(io(fwd, wall, { jumpP: true }), DT); assert.equal(V.s.mode, MA.JUMP1);
});

test('punch chain: jab / jab / kick 6/6/10, 0.27 s window, kick ends the chain', () => {
  const P = { punchP: true }, dmg = [];
  const T = maroTable();
  T.step(io(none, P), DT); assert.equal(T.s.mode, MA.PUNCH1); assert.equal(T.s.fx, FX.HIT); dmg.push(T.s.fxDmg);
  run(T, none, 0.2); T.step(io(none, P), DT); assert.equal(T.s.mode, MA.PUNCH2); dmg.push(T.s.fxDmg);
  run(T, none, 0.2); T.step(io(none, P), DT); assert.equal(T.s.mode, MA.KICK); dmg.push(T.s.fxDmg);
  assert.deepEqual(dmg, [6, 6, 10]);
  run(T, none, 0.15); T.step(io(none, P), DT); assert.equal(T.s.mode, MA.KICK);   // no fourth hit
  run(T, none, 0.4); assert.equal(T.s.mode, MA.IDLE);
  // too late: window closed -> back to a jab
  const U = maroTable(); U.step(io(none, P), DT); run(U, none, 0.26); U.step(io(none, P), DT); assert.equal(U.s.mode, MA.PUNCH2);
  const W = maroTable(); W.step(io(none, P), DT); run(W, none, 0.3); assert.equal(W.s.mode, MA.IDLE); W.step(io(none, P), DT); assert.equal(W.s.mode, MA.PUNCH1);
  // jump cancels a swing
  const X = maroTable(); X.step(io(none, P), DT); X.step(io(none, { jumpP: true }), DT); assert.equal(X.s.mode, MA.JUMP1);
});

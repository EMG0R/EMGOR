import test from 'node:test';
import assert from 'node:assert/strict';
import { maroTable, MARO_K as K, MA } from '../../js/ship-styles.js';

const DT = 1 / 120;
const fwd = { wx: 0, wz: -1, m: 1, jumpP: false, crouch: false };
const none = { wx: 0, wz: 0, m: 0, jumpP: false, crouch: false };
const run = (T, io, secs) => { for (let t = 0; t < secs; t += DT) T.step(io, DT); };
const jump = (T, io) => { T.step(Object.assign({}, io, { jumpP: true }), DT); };
const apex = (T) => { let m = 0; for (let i = 0; i < 600 && !T.s.grounded; i++) { T.step(fwd, DT); if (T.s.y > m) m = T.s.y; } return m; };
const near3 = (a, b) => assert.ok(Math.abs(a - b) / b < 0.03, a + ' vs ' + b);

test('accelerates to 6.0 H/s at 6.2 H/s^2 and reads as run', () => {
  const T = maroTable(); run(T, fwd, 0.5);
  assert.ok(Math.abs(Math.hypot(T.s.vx, T.s.vz) - 3.1) < 0.1);
  run(T, fwd, 2); near3(Math.hypot(T.s.vx, T.s.vz), K.MAX); assert.equal(T.s.mode, MA.RUN);
});

test('apex of single / double / triple jump within 3% of v0^2/2g', () => {
  for (const [c, v0] of [[1, K.V1], [2, K.V2], [3, K.V3]]) {
    const T = maroTable(); run(T, fwd, 1.5);
    T.s.chain = c - 1; T.s.landAge = 0;                       // set up the chain so the next jump is level c
    jump(T, fwd); assert.equal(T.s.mode, MA.JUMP1 + c - 1);
    near3(apex(T), v0 * v0 / (2 * K.G));
  }
});

test('chain window: 0.17 s boundary, slow speed breaks it', () => {
  const T = maroTable(); run(T, fwd, 1.5); jump(T, fwd); apex(T);
  assert.equal(T.s.chain, 1); run(T, fwd, 0.12); jump(T, fwd); assert.equal(T.s.mode, MA.JUMP2);          // inside window
  apex(T); run(T, fwd, 0.25); jump(T, fwd); assert.equal(T.s.mode, MA.JUMP1);                              // outside window: back to 1
  const S = maroTable(); S.s.chain = 1; S.s.landAge = 0; S.s.vx = 0; S.s.vz = 0; jump(S, none); assert.equal(S.s.mode, MA.JUMP1);  // standing: no chain
});

test('triple needs speed; below it stays a double', () => {
  const T = maroTable(); T.s.chain = 2; T.s.landAge = 0.05; T.s.vz = -2; jump(T, none); assert.equal(T.s.mode, MA.JUMP2);
  const U = maroTable(); U.s.chain = 2; U.s.landAge = 0.05; U.s.vz = -5; jump(U, fwd); assert.equal(U.s.mode, MA.JUMP3);
});

test('skid: > 90 deg turn above 3 H/s only', () => {
  const rev = { wx: 0, wz: 1, m: 1, jumpP: false, crouch: false }, T = maroTable();
  T.s.vz = -5; T.step(rev, DT); assert.equal(T.s.mode, MA.SKID);
  const U = maroTable(); U.s.vz = -2.9; U.step(rev, DT); assert.notEqual(U.s.mode, MA.SKID);           // too slow
  const V = maroTable(); V.s.vz = -5; V.step({ wx: Math.sin(89 * Math.PI / 180), wz: -Math.cos(89 * Math.PI / 180), m: 1 }, DT); assert.notEqual(V.s.mode, MA.SKID);
  // decel 11 H/s^2
  const sp0 = Math.hypot(T.s.vx, T.s.vz); T.step(rev, DT); near3(sp0 - Math.hypot(T.s.vx, T.s.vz), K.SKID * DT);
});

test('side flip out of a skid: 11.6 up, 1.5 fwd toward the wish', () => {
  const rev = { wx: 0, wz: 1, m: 1, jumpP: false, crouch: false }, T = maroTable();
  T.s.vz = -5; T.step(rev, DT); jump(T, rev);
  assert.equal(T.s.mode, MA.SIDE); near3(T.s.vy + K.G * DT, K.SF_Y); assert.ok(Math.abs(T.s.vz - K.SF_F) < 0.1);
});

test('long jump: crouch + A at speed = 9.0 fwd cap, 5.6 up; slow = backflip 11.6 up, 0 fwd', () => {
  const T = maroTable(); run(T, fwd, 1.5); jump(T, Object.assign({}, fwd, { crouch: true }));
  assert.equal(T.s.mode, MA.LONG); near3(T.s.vy + K.G * DT, K.LJ_Y); near3(Math.hypot(T.s.vx, T.s.vz), K.LJ_F);
  let d = 0; while (!T.s.grounded) { T.step(none, DT); d += Math.hypot(T.s.vx, T.s.vz) * DT; } near3(d, 2 * K.LJ_Y / K.G * K.LJ_F);   // ~4.5 H with the spec constants
  const B = maroTable(); jump(B, { wx: 0, wz: 0, m: 0, crouch: true });
  assert.equal(B.s.mode, MA.BACK); near3(B.s.vy + K.G * DT, K.BF_Y); assert.equal(Math.hypot(B.s.vx, B.s.vz), 0);
});

test('crouch / crawl / idle states', () => {
  const T = maroTable(); T.step(none, DT); assert.equal(T.s.mode, MA.IDLE);
  T.step({ wx: 0, wz: 0, m: 0, crouch: true }, DT); assert.equal(T.s.mode, MA.CROUCH);
  T.step({ wx: 0, wz: -1, m: 1, crouch: true }, DT); assert.equal(T.s.mode, MA.CRAWL); assert.ok(Math.abs(T.s.vz + K.CRAWL) < 1e-9);
});

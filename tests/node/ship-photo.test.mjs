import test from 'node:test';
import assert from 'node:assert/strict';
import { clampFov, fovFromWheel, wrapT, stepT, sunAt, orbitStep, shareQuery, parseShare, FOV_MIN, FOV_MAX, PITCH_MIN, T_STEP, WATERMARK } from '../../js/ship-photo.js';

const len = (v) => Math.hypot(v[0], v[1], v[2]);
const angUp = (v, u) => Math.acos((v[0] * u[0] + v[1] * u[1] + v[2] * u[2]) / len(v));

test('fov: clamp, wheel direction, NaN safe', () => {
  assert.equal(clampFov(5), FOV_MIN); assert.equal(clampFov(500), FOV_MAX); assert.equal(clampFov(NaN), 60);
  assert.ok(fovFromWheel(60, -100) < 60 && fovFromWheel(60, 100) > 60 && fovFromWheel(60, 0) === 60);
  let f = 60; for (let i = 0; i < 200; i++) f = fovFromWheel(f, 1); assert.equal(f, FOV_MAX);
  for (let i = 0; i < 400; i++) f = fovFromWheel(f, -1); assert.equal(f, FOV_MIN);
});
test('time: wraps to [0,1), step is reversible', () => {
  assert.equal(wrapT(1), 0); assert.equal(wrapT(-0.25), 0.75); assert.equal(wrapT(NaN), 0); assert.ok(Math.abs(wrapT(2.5) - 0.5) < 1e-12);
  assert.ok(Math.abs(stepT(stepT(0.3, 1), -1) - 0.3) < 1e-12);
  assert.ok(stepT(0, -1) > 0.9 && stepT(0.99, 1) >= 0 && stepT(0.99, 1) < 1);
  let t = 0; for (let i = 0; i < Math.round(1 / T_STEP); i++) t = stepT(t, 1); assert.ok(Math.abs(t) < 1e-9 || Math.abs(t - 1) < 1e-9);
});
test('sun: t=0 is the base, half turn flips x/z, y and length kept', () => {
  const o = { x: 0, y: 0, z: 0 };
  sunAt(-0.62, 0.52, 0.4, 0, o); assert.ok(Math.abs(o.x + 0.62) < 1e-12 && Math.abs(o.z - 0.4) < 1e-12);
  sunAt(-0.62, 0.52, 0.4, 0.5, o); assert.ok(Math.abs(o.x - 0.62) < 1e-9 && Math.abs(o.z + 0.4) < 1e-9 && o.y === 0.52);
  for (let t = 0; t < 1; t += 0.07) { sunAt(-0.62, 0.52, 0.4, t, o); assert.ok(Math.abs(Math.hypot(o.x, o.y, o.z) - Math.hypot(0.62, 0.52, 0.4)) < 1e-9); }
});
test('orbit: length kept, yaw keeps the up angle, pitch clamps', () => {
  const up = [0, 1, 0], out = [0, 0, 0], off = [3, 2, 1], L = len(off), a0 = angUp(off, up);
  orbitStep(off, up, 1.1, 0, out);
  assert.ok(Math.abs(len(out) - L) < 1e-9 && Math.abs(angUp(out, up) - a0) < 1e-9);
  orbitStep(off, up, 0, 0.3, out); assert.ok(Math.abs(angUp(out, up) - (a0 + 0.3)) < 1e-9 && Math.abs(len(out) - L) < 1e-9);
  let v = off.slice(); for (let i = 0; i < 100; i++) v = orbitStep(v, up, 0, 0.2, [0, 0, 0]);
  assert.ok(Math.abs(angUp(v, up) - (Math.PI - PITCH_MIN)) < 1e-9);
  for (let i = 0; i < 100; i++) v = orbitStep(v, up, 0, -0.2, [0, 0, 0]);
  assert.ok(Math.abs(angUp(v, up) - PITCH_MIN) < 1e-9 && Math.abs(len(v) - L) < 1e-9);
  assert.deepEqual(orbitStep([0, 0, 0], up, 1, 1, out), [0, 0, 0]);
  const tilted = [0.3, 0.4, -0.8]; const u2 = [0.6, 0.8, 0]; const r = orbitStep(tilted, u2, 0.7, 0.2, [0, 0, 0]);
  assert.ok(Math.abs(len(r) - len(tilted)) < 1e-9 && r.every(Number.isFinite));
  const par = orbitStep([0, 5, 0], up, 0, 0.5, [0, 0, 0]); assert.ok(par.every(Number.isFinite) && Math.abs(len(par) - 5) < 1e-9);
});
test('share link: round trip, rejects junk', () => {
  const q = shareQuery('node 7', [0.1234567, -1.0004, 0.5], 0.3333, 'l');
  assert.equal(q, '?p=node%207&d=0.123,-1,0.5&t=0.333&m=l');
  const s = parseShare(q); assert.deepEqual(s, { id: 'node 7', d: [0.123, -1, 0.5], t: 0.333, mode: 'l' });
  assert.equal(parseShare('?p=a&d=1,0,0').t, 0); assert.equal(parseShare('?p=a&d=1,0,0').mode, 'f');
  assert.equal(shareQuery('', [1, 0, 0], 0, 'f'), ''); assert.equal(shareQuery('a', [NaN, 0, 0], 0, 'f'), ''); assert.equal(shareQuery('a', [1], 0, 'f'), '');
  for (const bad of ['', '?p=a', '?d=1,0,0', '?p=a&d=1,0', '?p=a&d=x,y,z', '?p=a&d=0,0,0', '?p=a&d=1000,0,0', '?p=a&d=Infinity,0,0', '?p=%E0%A4%A&d=1,0,0', null, undefined, 5, '?p=' + 'x'.repeat(500) + '&d=1,0,0'])
    assert.equal(parseShare(bad), null);
  assert.equal(parseShare('?p=a&d=1,0,0&t=1.25').t, 0.25);
  assert.equal(WATERMARK, 'emgor.online/nomansgor');
});

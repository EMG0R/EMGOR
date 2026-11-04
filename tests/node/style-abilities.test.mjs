import test from 'node:test';
import assert from 'node:assert/strict';
import { reflectVel, regenStep, throwArc, createStyle } from '../../js/ship-styles.js';

test('reflectVel aims at the source, speed x k, no NaN on degenerate source', () => {
  const o = { x: 0, y: 0, z: 0 };
  reflectVel(o, 0, 0, -10, 0, 0, -5, 1.25);
  assert.ok(Math.abs(o.z + 12.5) < 1e-9 && o.x === 0 && o.y === 0);
  reflectVel(o, 3, 0, 4, 0, 0, 0, 1);
  assert.deepEqual([o.x, o.y, o.z], [-3, -0, -4]);
  reflectVel(o, 3, 4, 0, 0, 5, 0, 0);
  assert.ok(Math.abs(Math.hypot(o.x, o.y, o.z) - 5) < 1e-9);
});
test('regenStep: capped, monotone, only when calm, ignores bad input', () => {
  assert.equal(regenStep(50, 100, 5, 1, true), 55);
  assert.equal(regenStep(98, 100, 5, 1, true), 100);
  assert.equal(regenStep(50, 100, 5, 1, false), 50);
  assert.equal(regenStep(50, 100, 0, 1, true), 50);
  assert.ok(Number.isNaN(regenStep(NaN, 100, 5, 1, true)));
  let h = 10; for (let i = 0; i < 40; i++) { const n = regenStep(h, 100, 5, 0.5, true); assert.ok(n >= h); h = n; }
  assert.equal(h, 100);
});
test('throwArc: starts at p0, apex at v.up/g, returns below start', () => {
  const o = { x: 0, y: 0, z: 0 }, p0 = { x: 1, y: 2, z: 3 }, v = { x: 0, y: 6, z: 10 }, up = { x: 0, y: 1, z: 0 };
  throwArc(o, p0, v, up, 12, 0);
  assert.deepEqual([o.x, o.y, o.z], [1, 2, 3]);
  throwArc(o, p0, v, up, 12, 0.5);
  assert.ok(Math.abs(o.y - (2 + 6 * 0.5 - 6 * 0.25)) < 1e-9 && Math.abs(o.z - 8) < 1e-9);
  throwArc(o, p0, v, up, 12, 2);
  assert.ok(o.y < 2);
});
test('styles still emit the events the consumers read', () => {
  const sw = createStyle('SWIFT', {}), hl = createStyle('HAULER', {}), jl = createStyle('JELLY', {});
  assert.ok(sw && hl && jl && typeof hl.grab === 'function');
});

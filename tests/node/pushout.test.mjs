import test from 'node:test';
import assert from 'node:assert/strict';
import { pushBoxes } from '../../js/ship-pushout.js';

const box = (x0, y0, z0, x1, y1, z1) => ({ min: { x: x0, y: y0, z: z0 }, max: { x: x1, y: y1, z: z1 } });
const inside = (p, q) => p.x > q.min.x && p.x < q.max.x && p.y > q.min.y && p.y < q.max.y && p.z > q.min.z && p.z < q.max.z;

test('outside by more than rs: untouched', () => {
  const p = { x: 5, y: 0.5, z: 0.5 };
  assert.equal(pushBoxes(p, [box(0, 0, 0, 1, 1, 1)], 0.2), false);
  assert.deepEqual(p, { x: 5, y: 0.5, z: 0.5 });
});
test('graze: pushed to exactly rs from the face', () => {
  const p = { x: 1.1, y: 0.5, z: 0.5 };
  assert.equal(pushBoxes(p, [box(0, 0, 0, 1, 1, 1)], 0.25), true);
  assert.ok(Math.abs(p.x - 1.25) < 1e-9 && p.y === 0.5 && p.z === 0.5);
});
test('centre inside: leaves by the shallowest face', () => {
  const p = { x: 0.9, y: 0.5, z: 0.5 };
  pushBoxes(p, [box(0, 0, 0, 1, 1, 1)], 0.2);
  assert.ok(Math.abs(p.x - 1.2) < 1e-9);
  const t = { x: 0.5, y: 0.95, z: 0.5 };      // top face is shallowest: flight over a shelf lands on it
  pushBoxes(t, [box(0, 0, 0, 1, 1, 1)], 0.2);
  assert.ok(Math.abs(t.y - 1.2) < 1e-9);
});
test('two touching boxes: later box pushing into the earlier one is resolved by the extra pass', () => {
  const a = box(0, 0, 0, 1, 1, 1), b = box(1, 0, 0, 1.3, 1, 1);      // thin wall b beside a
  const p = { x: 1.15, y: 0.5, z: 0.5 };
  assert.equal(pushBoxes(p, [a, b], 0.2), true);
  assert.ok(!inside(p, a) && !inside(p, b));
  const rs = 0.2;
  for (const q of [a, b]) {
    const cx = Math.min(Math.max(p.x, q.min.x), q.max.x), cy = Math.min(Math.max(p.y, q.min.y), q.max.y), cz = Math.min(Math.max(p.z, q.min.z), q.max.z);
    assert.ok(Math.hypot(p.x - cx, p.y - cy, p.z - cz) >= rs - 1e-6);
  }
});
test('corner fuzz: never ends inside any box, no NaN, stable', () => {
  let s = 12345; const rnd = () => (s = (s * 1664525 + 1013904223) >>> 0) / 4294967296;
  const boxes = [box(0, 0, 0, 4, 1, 0.3), box(0, 0, 0, 0.3, 1, 4), box(1, 0, 1, 2, 0.8, 2)];
  for (let i = 0; i < 2000; i++) {
    const p = { x: rnd() * 4, y: rnd(), z: rnd() * 4 };
    pushBoxes(p, boxes, 0.22);
    assert.ok(isFinite(p.x + p.y + p.z));
    for (const q of boxes) assert.ok(!inside(p, q));
  }
});
test('null list is a no-op', () => assert.equal(pushBoxes({ x: 0, y: 0, z: 0 }, null, 0.2), false));

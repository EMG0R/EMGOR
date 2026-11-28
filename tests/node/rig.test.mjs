import test from 'node:test';
import assert from 'node:assert/strict';
import { normRig, rigParts, GAITS } from '../../js/ship-styles.js';

test('PILOT rig = today: unit scales, no extras, gait 1, PILOT head 1.3x, helmet-top ~1.112', () => {
  const r = normRig(null);
  assert.deepEqual(r.ts, [1, 1, 1]); assert.deepEqual(r.hs, [1.3, 1.3, 1.3]);
  assert.equal(r.arm, 1); assert.equal(r.leg, 1); assert.equal(r.hipOff, 0); assert.deepEqual(r.extras, []);
  assert.deepEqual(r.gait, GAITS.PILOT); assert.ok(Math.abs(r.top - 1.112) < 1e-9);
});
test('bad input falls back to defaults / clamps', () => {
  const r = normRig({ headShape: 'dodecahedron', torso: 7, headScale: 'x', limbLen: { arm: 99, leg: -3 }, quills: 'a', tail: { len: 50, spikes: 99 } });
  assert.equal(r.headShape, 'round'); assert.equal(r.torso, 'normal'); assert.deepEqual(r.hs, [1, 1, 1]);
  assert.equal(r.arm, 1.6); assert.equal(r.leg, 0.5); assert.equal(r.quills, 0); assert.equal(r.tail.len, 3); assert.equal(r.tail.spikes, 6);
});
test('leg length lifts the hips so feet stay on y=0', () => {
  const r = normRig({ limbLen: { leg: 1.4 } });
  assert.ok(Math.abs(r.hipOff - 0.2) < 1e-9);
  assert.ok(r.hitbox.h > normRig(null).hitbox.h);
});
test('rigParts lists unique tokens; no NaN in a normalised rig', () => {
  const p = rigParts(normRig(null)); assert.ok(p.length >= 10); assert.equal(new Set(p).size, p.length);
  const r = normRig({ headScale: NaN, limbLen: { arm: NaN, leg: Infinity }, tail: { len: NaN }, quills: NaN });
  for (const k of ['arm', 'leg', 'quills']) assert.ok(Number.isFinite(r[k]), k);
});

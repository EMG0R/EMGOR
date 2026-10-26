import test from 'node:test';
import assert from 'node:assert/strict';
import * as D from '../../js/ship-depth.js';

test('microSeed is stable and differs by rock / parent', () => {
  assert.equal(D.microSeed('p1', 3), D.microSeed('p1', 3));
  assert.notEqual(D.microSeed('p1', 3), D.microSeed('p1', 4));
  assert.notEqual(D.microSeed('p1', 3), D.microSeed('p2', 3));
});
test('microSpec flags micro, moons 0-3, g scales 0.85^d', () => {
  const s = D.microSpec('p1', 7, 1);
  assert.equal(s.micro, true); assert.ok(s.moons >= 0 && s.moons <= 3);
  assert.ok(Math.abs(s.g - 0.85) < 1e-9); assert.deepEqual(D.microSpec('p1', 7, 1), s);
  assert.equal(D.scale(0).g, 1); assert.equal(D.scale(2).loot, 3);
});
test('dive: hold -> dive -> in, cancel only before in', () => {
  const d = D.createDive(); d.start(5); assert.equal(d.st.phase, 'hold');
  d.tick(0.3); assert.equal(d.cancel(), 'idle');
  d.start(5); d.tick(D.HOLD_T + 0.01); assert.equal(d.st.phase, 'dive');
  d.tick(1); assert.ok(d.st.k > 0 && d.st.k < 1);
  assert.equal(d.tick(1.01), 'in'); assert.equal(d.st.k, 1);
  assert.equal(d.cancel(), 'in');
});
test('climb: reverse ease back to idle, keeps rock id until reset', () => {
  const d = D.createDive(); d.start(2); d.tick(D.HOLD_T); d.tick(D.DIVE_T); assert.equal(d.st.phase, 'in');
  assert.equal(d.climb(), 'climb'); d.land(1); assert.ok(d.st.k < 1 && d.st.k > 0);
  assert.equal(d.land(1.01), 'idle'); assert.equal(d.st.k, 0);
});
test('start ignored when not idle; shouldClimb at 3 R', () => {
  const d = D.createDive(); d.start(1); d.start(9); assert.equal(d.st.rock, 1);
  assert.equal(D.shouldClimb(299, 100), false); assert.equal(D.shouldClimb(300, 100), true);
});
test('depthStack: cap at MAX_DEPTH, save round-trip, junk rejected', () => {
  const s = []; assert.equal(D.push(s, 'p1', 4), true); assert.equal(D.push(s, 'p2', 1), false);
  assert.deepEqual(D.fromSave(D.toSave(s)), s);
  assert.deepEqual(D.fromSave([{ planetId: 3, rockIndex: 1 }, null, { planetId: 'a', rockIndex: -1 }]), []);
  assert.deepEqual(D.pop(s), { planetId: 'p1', rockIndex: 4 }); assert.equal(D.pop(s), null);
});

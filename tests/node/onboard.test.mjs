import test from 'node:test';
import assert from 'node:assert/strict';
import * as OB from '../../js/ship-onboard.js';

test('fresh chain starts at 0, done profile is finished', () => {
  assert.equal(OB.fresh(false).i, 0); assert.ok(OB.finished(OB.fresh(true))); assert.ok(!OB.finished(OB.fresh(false)));
});
test('steps advance in order and never skip without evidence', () => {
  const c = OB.fresh(false);
  assert.equal(OB.advance(c, {}), 0); assert.equal(OB.current(c).id, 'planet');
  assert.equal(OB.advance(c, { planet: true }), 1); assert.equal(OB.current(c).id, 'land');
  assert.equal(OB.advance(c, { planet: true, landed: true }), 1); assert.equal(OB.current(c).id, 'beacon');
  assert.equal(OB.advance(c, { landed: true, beacon: true }), 1); assert.equal(OB.current(c).id, 'ball');
  assert.equal(OB.advance(c, { ball: true }), 1); assert.equal(OB.current(c).id, 'catch');
  assert.equal(OB.advance(c, { caught: true }), 1); assert.equal(OB.current(c).id, 'fight');
  assert.equal(OB.advance(c, { won: true }), 1); assert.equal(OB.current(c).id, 'station');
  assert.equal(OB.advance(c, { station: true }), 1); assert.ok(OB.finished(c));
});
test('a profile that already caught a mon skips ahead', () => {
  const c = OB.fresh(false); const n = OB.advance(c, { planet: true, landed: true, caught: true });
  assert.equal(n, 5); assert.equal(OB.current(c).id, 'fight');
});
test('entry + markKind', () => {
  const c = OB.fresh(false); assert.match(OB.entry(c)[0], /^FIRST STEPS 1\/7/);
  c.i = 5; assert.equal(OB.markKind(c), 'village'); c.i = 6; assert.equal(OB.markKind(c), 'station');
  c.i = 7; assert.equal(OB.entry(c), null); assert.equal(OB.markKind(c), '');
});
test('norm repairs junk and clamps', () => {
  assert.equal(OB.norm(null, false).i, 0); assert.equal(OB.norm({ i: 99 }, false).i, 7); assert.equal(OB.norm({ i: -3 }, false).i, 0);
  assert.equal(OB.START_CARD.length, 3);
});
test('no Math.random', async () => {
  const fs = await import('node:fs'); assert.ok(!/Math\.random/.test(fs.readFileSync(new URL('../../js/ship-onboard.js', import.meta.url), 'utf8')));
});

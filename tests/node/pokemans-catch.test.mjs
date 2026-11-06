import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKC from '../../js/pokemans-catch.js';

const up = { x: 0, y: 1, z: 0 };
for (const [dist, hy] of [[6, 0], [6, 3], [15, 0], [15, 4], [20, 2]]) {
  test('aim solve: ball reaches a body at ' + dist + ' H (height ' + hy + ')', () => {
    const T = { x: 0, y: hy, z: -dist }, d = { x: 0, y: 0, z: 0 }, t = PKC.solveAim(T.x, T.y, T.z, up, d);
    assert.ok(t > 0 && t <= PKC.BALL.tMax, 't ' + t);
    assert.ok(Math.abs(Math.hypot(d.x, d.y, d.z) - 1) < 1e-6);
    const p = PKC.arcPoint({ x: 0, y: 0, z: 0 }, d, up, t, { x: 0, y: 0, z: 0 });
    assert.ok(Math.hypot(p.x - T.x, p.y - T.y, p.z - T.z) < 0.05, JSON.stringify(p));
    // swept segment catches a 2 H body around the target
    const a = PKC.arcPoint({ x: 0, y: 0, z: 0 }, d, up, t - 0.02, { x: 0, y: 0, z: 0 }), b = PKC.arcPoint({ x: 0, y: 0, z: 0 }, d, up, t + 0.02, { x: 0, y: 0, z: 0 });
    assert.ok(PKC.segSphere(a.x, a.y, a.z, b.x, b.y, b.z, T.x, T.y, T.z, 1));
  });
}
test('segSphere: fast ball tunnelling through a thin body still hits; a miss does not', () => {
  assert.ok(PKC.segSphere(0, 0, -10, 0, 0, 10, 0.3, 0, 0, 0.5));
  assert.ok(!PKC.segSphere(0, 0, -10, 0, 0, 10, 2, 0, 0, 0.5));
});
test('catchChance: monotone in tier, inverse in hp, grazer floors 35/45/55', () => {
  for (const h of [1, 0.7, 0.4, 0.1, 0]) assert.ok(PKC.catchChance(1, h) <= PKC.catchChance(2, h) && PKC.catchChance(2, h) <= PKC.catchChance(3, h));
  for (const t of [1, 2, 3]) { let prev = 0; for (const h of [1, 0.8, 0.5, 0.2, 0]) { const c = PKC.catchChance(t, h); assert.ok(c >= prev - 1e-12); prev = c; } }
  assert.deepEqual([1, 2, 3].map((t) => Math.round(PKC.catchChance(t, 1) * 100)), [35, 45, 55]);
});
test('encounterOk: catchable kinds inside range only', () => {
  assert.equal(PKC.encounterOk(2, 'grazer', {}), true);
  assert.equal(PKC.encounterOk(2, 'jelly', null), true);
  assert.equal(PKC.encounterOk(2.9, 'predator', { held: false }), true);
  assert.equal(PKC.encounterOk(3.1, 'grazer', {}), false);
  assert.equal(PKC.encounterOk(1, 'villager', {}), false);
  assert.equal(PKC.encounterOk(1, 'grazer', { tamed: true }), false);
  assert.equal(PKC.encounterOk(20, 'grazer', {}, 25), true);
});

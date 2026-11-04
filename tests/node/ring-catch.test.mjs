import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKC from '../../js/pokemans-catch.js';

test('ring catch: chance monotone in missing hp and in ball tier', () => {
  for (let t = 1; t <= 3; t++) for (let h = 1; h > 0; h -= 0.1) assert.ok(PKC.ringCatchChance(t, h - 0.1, 0) >= PKC.ringCatchChance(t, h, 0) - 1e-12);
  for (let h = 0; h <= 1; h += 0.1) for (let t = 1; t < 3; t++) assert.ok(PKC.ringCatchChance(t + 1, h, 0) >= PKC.ringCatchChance(t, h, 0));
  assert.ok(PKC.ringCatchChance(1, 0, 0) <= 1 && PKC.ringCatchChance(3, 1, 0) >= 0.1);
});
test('ring catch: KO window is 3 s from the KO, hp frac clamps', () => {
  assert.ok(PKC.koOpen(10, 10) && PKC.koOpen(10, 13) && !PKC.koOpen(10, 13.01) && !PKC.koOpen(10, 9.9));
  assert.equal(PKC.wildHpFrac(120, 100), 0);
  assert.equal(PKC.wildHpFrac(0, 100), 1);
  assert.ok(PKC.wildHpFrac(50, 100) === 0.5 && PKC.wildHpFrac(NaN, 0) >= 0);
});

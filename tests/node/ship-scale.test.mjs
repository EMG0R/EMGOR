import test from 'node:test';
import assert from 'node:assert/strict';
import { H_RATIO, HUMAN_M, GM_PER_L, hOf } from '../../js/ship-scale.js';

test('hOf is H_RATIO * L and the ship is 3-6 humans long', () => {
  assert.equal(hOf(10), H_RATIO * 10);
  assert.ok(1 / H_RATIO >= 3 && 1 / H_RATIO <= 6);
});
test('GM_PER_L maps a human height in metres to H_RATIO L', () => {
  assert.ok(Math.abs(GM_PER_L * HUMAN_M - H_RATIO) < 1e-12);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKC from '../../js/pokemans-catch.js';

const c = (id, x, y, z) => ({ id, lp: { x, y, z } });
test('wild encounter: nearest inside range wins, boundary and empties give -1', () => {
  const p = { x: 0, y: 0, z: 0 };
  assert.equal(PKC.wildEncounterIndex([c('a', 2, 0, 0), c('b', 1, 0, 1)], p, 3), 1);
  assert.equal(PKC.wildEncounterIndex([c('a', 3, 0, 0)], p, 3), -1);
  assert.equal(PKC.wildEncounterIndex([c('a', 2.99, 0, 0)], p, 3), 0);
  assert.equal(PKC.wildEncounterIndex([], p, 3), -1);
  assert.equal(PKC.wildEncounterIndex(null, p, 3), -1);
  assert.equal(PKC.wildEncounterIndex([c('a', 1, 0, 0)], p, 0), -1);
  assert.equal(PKC.wildEncounterIndex([{ id: 'x' }, c('a', 1, 1, 1)], p, 3), 1);
});

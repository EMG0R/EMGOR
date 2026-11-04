import test from 'node:test';
import assert from 'node:assert/strict';
import * as ITM from '../../js/ship-items.js';

test('every 7/11 menu carries GOR BALL tiers 1-3 (any store id, any day)', () => {
  for (const id of ['station-711', 'planet-a', 'x']) for (const day of ['2026-11-04', '2027-01-01']) {
    const ids = ITM.storeMenu(id, 24, day).map((i) => i.id);
    for (const t of [1, 2, 3]) assert.ok(ids.includes('gorball-' + t), id + ' ' + day + ' missing gorball-' + t);
  }
});

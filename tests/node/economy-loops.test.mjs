import test from 'node:test';
import assert from 'node:assert/strict';
import * as CRF from '../../js/ship-craft.js';

// Craft loops: buy the parts, craft, sell the result must never profit. Cost model lives in ship-craft.js (recipeInputCost); this asserts it over the whole table.
test('one sell factor', () => {
  assert.equal(CRF.SELL_K, 0.4);
  for (const cat of ['junk', 'food', 'part', 'material']) assert.equal(CRF.sellPrice({ price: 100, category: cat, kind: cat === 'food' ? 'fries' : cat }), 40);
});
test('no recipe sells for more than 0.8x its inputs', () => {
  const bad = [];
  for (const r of CRF.RECIPES) {
    const count = r.count || 1, cost = CRF.recipeInputCost(r);
    const sell = CRF.sellPrice({ price: r.out.price || 40, category: r.out.cat }) * count;
    if (0.8 * cost < count) continue;      // 1 gC minimum sell on a near-free recipe
    if (sell > 0.8 * cost + 1e-9) bad.push(`${r.id}: sells ${sell} vs cost ${cost} (x${count})`);
  }
  assert.deepEqual(bad, []);
});
test('cost model sanity: dream cell and infusion dust are no longer x4 / x2', () => {
  const by = Object.fromEntries(CRF.RECIPES.map((r) => [r.id, r]));
  for (const id of ['dream-cell', 'infusion-dust', 'seasoning-salt']) {
    const r = by[id], sell = CRF.sellPrice({ price: r.out.price, category: r.out.cat }) * (r.count || 1);
    assert.ok(sell <= CRF.recipeInputCost(r), id);
  }
});

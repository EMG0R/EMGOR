import test from 'node:test';
import assert from 'node:assert/strict';
import * as CRF from '../../js/ship-craft.js';

test('applyUpgrade caps ship slots and suit kits', () => {
  let p = { upgrades: {} };
  for (let i = 0; i < 5; i++) { const r = CRF.applyUpgrade(p, { slot: 'shield', level: 1 }); assert.ok(!r.refused); p = r.profile; }
  const r6 = CRF.applyUpgrade(p, { slot: 'shield', level: 1 });
  assert.ok(r6.refused); assert.equal(p.upgrades.shield, 5);
  const s = CRF.applyUpgrade({ upgrades: {} }, { slot: 'suit-scanner', level: 4 });
  assert.equal(s.profile.upgrades['suit-scanner'], 3);
  assert.ok(CRF.applyUpgrade(s.profile, { slot: 'suit-scanner', level: 1 }).refused);
  assert.equal(CRF.applyUpgrade({ upgrades: {} }, { slot: 'pens', level: 1 }).profile.pens, 1);
});

test('bolt scope/coil and salted creature treat are craftable from crafted outputs', () => {
  const core = { id: 'cr:bolt-core:1', kind: 'material', category: 'material', name: 'Bolt Core', recipe: 'bolt-core', tags: [] };
  const lens = { id: 'lens', kind: 'part', base: 'lens', name: 'Lens', tags: ['part', 'lens'] };
  const bat = { id: 'bat', kind: 'part', base: 'battery', name: 'Battery', tags: ['part', 'battery'] };
  const salt = { id: 'salt', kind: 'material', category: 'material', name: 'Seasoning Salt', recipe: 'seasoning-salt', tags: [] };
  const jerky = { id: 'j', kind: 'snack', name: 'Beef Jerky', base: 'Beef Jerky', tags: [] };
  const g = (...its) => its.map((item) => ({ item, n: 1 }));
  assert.equal(CRF.match(g(lens, core), {}).recipe.id, 'scope-bolt');
  assert.equal(CRF.match(g(core, bat), {}).recipe.id, 'coil-bolt');
  assert.equal(CRF.match(g(salt, jerky), {}).recipe.id, 'creature-treat-salt');
});

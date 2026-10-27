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

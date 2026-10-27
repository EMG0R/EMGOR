import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKP from '../../js/pokemans-party.js';
import * as PKG from '../../js/pokemans-gen.js';
import * as PKC from '../../js/pokemans-catch.js';

function mk(n) { const sp = PKG.species(n, 'a'); return PKC.makeMon ? PKC.makeMon(sp, PKG.individual(sp, n), 5) : null; }
const prof = (ids) => { const pf = { pokemans: [], party: [], pen: [], activeMon: '' }; ids.forEach((i) => { const m = mk(i); pf.pokemans.push(m); pf.party.push(m.id); }); return pf; };

test('radialPick quadrants + dead zone', () => {
  assert.equal(PKP.radialPick(2, -3), -1);
  assert.equal(PKP.radialPick(0, -40), 0); assert.equal(PKP.radialPick(40, 5), 1);
  assert.equal(PKP.radialPick(-3, 50), 2); assert.equal(PKP.radialPick(-60, 10), 3);
});
test('nextMove: forced wins, waits on cooldown, expires to AI', () => {
  const pf = prof([1]), m = pf.pokemans[0], foe = { def: 30, types: [] };
  assert.deepEqual(PKP.nextMove(m, foe, null, [0, 0, 0, 0], 0.3, 2, 0), { k: 2, keep: false });
  assert.deepEqual(PKP.nextMove(m, foe, null, [0, 0, 1, 0], 0.3, 2, 1), { k: -1, keep: true });
  const r = PKP.nextMove(m, foe, null, [0, 0, 1, 0], 0.3, 2, PKP.FORCE_WAIT + 0.1);
  assert.ok(r.k >= 0 && r.k !== 2 && !r.keep);
  assert.ok(PKP.nextMove(m, foe, null, [0, 0, 0, 0], 0.3, -1, 0).k >= 0);
});
test('moveToggle party <-> pen, last living guard, full guard, active repair', () => {
  const pf = prof([1, 2, 3]);
  assert.equal(PKP.moveToggle(pf, 'nope', 6).ok, false);
  pf.activeMon = pf.party[0];
  let r = PKP.moveToggle(pf, pf.party[0], 6); assert.equal(r.to, 'pen'); assert.equal(pf.activeMon, ''); assert.equal(pf.party.length, 2);
  assert.ok(PKP.active(pf));
  PKP.moveToggle(pf, pf.party[0], 6); assert.equal(PKP.moveToggle(pf, pf.party[0], 6).why, 'last');
  assert.equal(pf.party.length, 1);
  r = PKP.moveToggle(pf, pf.pen[0], 1); assert.equal(r.why, 'full');
  r = PKP.moveToggle(pf, pf.pen[0], 6); assert.equal(r.to, 'party'); assert.equal(pf.party.length, 2);
});
test('setActiveId refuses dead / non-party', () => {
  const pf = prof([4, 5]); const a = pf.party[1];
  assert.equal(PKP.setActiveId(pf, a).id, a); assert.equal(pf.activeMon, a);
  PKP.faint(pf.pokemans[0]); assert.equal(PKP.setActiveId(pf, pf.party[0]), null);
  assert.equal(PKP.setActiveId(pf, 'zzz'), null);
});

test('trimMons drops unused mons first and never leaves a dangling party/pen id', async () => {
  const { trimMons } = await import('../../js/pokemans-party.js');
  const mk = (n) => Array.from({ length: n }, (_, i) => ({ id: 'm' + i }));
  const pf = { pokemans: mk(5), party: ['m0'], pen: ['m1'] };
  trimMons(pf, 3);
  assert.deepEqual(pf.pokemans.map((m) => m.id), ['m0', 'm1', 'm4']);
  const all = { pokemans: mk(3), party: ['m0', 'm1'], pen: ['m2'] };
  trimMons(all, 2);
  assert.deepEqual(all.party, ['m1']); assert.deepEqual(all.pen, ['m2']); assert.equal(all.pokemans.length, 2);
});

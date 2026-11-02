import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKG from '../../js/pokemans-gen.js';
import * as PKC from '../../js/pokemans-catch.js';
import * as P from '../../js/pokemans-party.js';
import * as G from '../../js/pokemans-graft.js';

const mk = (pid, lvl) => { const sp = PKG.species(pid, 'skimmer', { biome: 'lush', tier: 2 }), ind = PKG.individual(sp, pid); return PKC.makeMon(sp, ind, lvl, PKG.xpForLevel(lvl), pid); };

test('graft raises exactly its stat, only for this individual', () => {
  const a = mk('a', 20), b = mk('a', 20); b.ind = JSON.parse(JSON.stringify(a.ind));
  const s0 = PKG.statsAt(a.sp, a.ind, 20);
  assert.ok(G.applyGraft(a, 'fin', 3).ok);
  const s1 = PKG.statsAt(a.sp, a.ind, 20), sb = PKG.statsAt(b.sp, b.ind, 20);
  assert.ok(s1.spd > s0.spd); for (const k of ['hp', 'atk', 'def', 'sp']) assert.equal(s1[k], s0[k]);
  assert.deepEqual(sb, s0);
});
test('each part maps to its stat', () => {
  const want = { fin: 'spd', spike: 'atk', shell: 'def', glow: 'sp', lungs: 'hp' };
  for (const p of G.PARTS) { const m = mk('z', 30), s0 = PKG.statsAt(m.sp, m.ind, 30); G.applyGraft(m, p, 2); const s1 = PKG.statsAt(m.sp, m.ind, 30); assert.ok(s1[want[p]] > s0[want[p]], p); }
});
test('max 6 grafts, cost rises, poor refused, bad part refused', () => {
  const m = mk('c', 10); let last = 0;
  for (let i = 0; i < 6; i++) { const c = G.graftCost(m, 2); assert.ok(c > last); last = c; assert.ok(G.applyGraft(m, 'shell', 2).ok); }
  assert.equal(G.graftCount(m), 6); assert.equal(G.applyGraft(m, 'fin', 1).why, 'full'); assert.equal(G.canGraft(m, 1, 1e9).why, 'full');
  const n = mk('d', 10); assert.equal(G.canGraft(n, 2, 5).why, 'poor'); assert.equal(G.applyGraft(n, 'tail', 1).why, 'part');
});
test('bodyOf adds graft parts to sig without touching the species', () => {
  const m = mk('e', 5), n0 = m.sp.bodyRecipe.sig.length; G.applyGraft(m, 'spike', 1);
  const b = G.bodyOf(m); assert.equal(b.sig.length, n0 + 1); assert.equal(b.sig[n0], 'spike'); assert.equal(m.sp.bodyRecipe.sig.length, n0);
});
test('partDrop deterministic, rates by source, tier clamped', () => {
  assert.equal(G.partDrop(2, 'kill', 0.9, 0.5), null); assert.ok(G.partDrop(2, 'kill', 0.1, 0.5));
  assert.equal(G.partDrop(9, 'catch', 0.99, 0.0).tier, 5); assert.equal(G.partDrop(2, 'catch', 0.99, 0.999).part, 'lungs');
  assert.equal(G.partDrop(2, 'resource', 0.25, 0.5), null);
  let n = 0; for (let i = 0; i < 1000; i++) if (G.partDrop(1, 'kill', i / 1000, 0.5)) n++; assert.equal(n, 350);
  const it = G.partItem('glow', 3); assert.equal(it.kind, 'graft'); assert.equal(it.id, 'graft-glow-3');
});
test('food teaches: only own types, 30 %, replaces weakest, individual only', () => {
  const m = mk('f', 12), ty = m.sp.types[0], other = PKG.TYPES.find((t) => m.sp.types.indexOf(t) < 0);
  assert.equal(G.teachRoll(m, other, 0, 0), null); assert.equal(G.teachRoll(m, ty, 0.3, 0), null);
  const p = G.teachRoll(m, ty, 0.29, 0.4); assert.ok(p); assert.equal(p.move.type, ty);
  const w = G.weakestSlot(m); assert.equal(p.slot, w); for (const mv of m.sp.moves) assert.ok(mv.power >= m.sp.moves[w].power);
  assert.deepEqual(G.teachRoll(m, ty, 0.29, 0.4).move, p.move);
  const orig = m.sp.moves[w].name; G.learn(m, p);
  assert.equal(P.moveAt(m, w).name, p.move.name); assert.equal(m.sp.moves[w].name, orig); assert.equal(P.moveCount(m), 4);
  assert.equal(G.foodType({ family: 'stim' }), 'spark'); assert.equal(G.foodType({ family: null }), null);
});
test('bond thresholds: 10 crit, 30 signature move, 5th move used by the AI', () => {
  const m = mk('g', 15);
  assert.equal(G.critChance(m), 0); assert.equal(G.sigMove(m), null);
  assert.equal(G.bondAdd(m, 9), 0); assert.equal(G.bondAdd(m, 1), 1); assert.equal(G.critChance(m), 0.05); assert.equal(G.sigMove(m), null);
  assert.equal(G.bondAdd(m, 25), 2); assert.equal(G.bondTier(m), 2);
  const sm = G.sigMove(m); assert.ok(sm); assert.equal(sm.power, 110); assert.equal(sm.type, m.sp.types[0]); assert.deepEqual(G.sigMove(m), sm);
  m.sig = sm; assert.equal(P.moveCount(m), 5); assert.equal(P.moveAt(m, 4), sm);
  const cds = [1, 1, 1, 1, 0]; assert.equal(P.chooseMove(m, { def: 30, types: [] }, null, cds, 0.5), 4);
  assert.equal(P.nextMove(m, { def: 30, types: [] }, null, cds, 0.5, 4, 0).k, 4);
});

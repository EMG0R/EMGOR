import test from 'node:test';
import assert from 'node:assert/strict';
import * as Q from '../../js/ship-quests.js';

const ctx = { planetIds: ['id-a', 'id-b', 'id-c'], planetNames: { 'id-a': 'ALPHA', 'id-b': 'BETA', 'id-c': 'GAMMA' }, currentPlanetId: 'id-a', giverKind: 'npc', wave: 1 };
test('quests key planets by id, show names', () => {
  for (const kind of ['deliver', 'scan', 'retrieve']) {
    for (let s = 1; s < 40; s++) {
      const q = Q.generateQuest(s, ctx, kind);
      const st = q.steps[0];
      if (st.planetId == null) continue;
      assert.ok(ctx.planetIds.includes(st.planetId), 'step planetId is a node id');
      assert.equal(st.planetName, ctx.planetNames[st.planetId]);
      assert.ok(!q.title.includes('id-'), 'title shows the name: ' + q.title);
    }
  }
});
test('plant scan progress matches plant events', () => {
  let found = null;
  for (let s = 1; s < 400 && !found; s++) { const q = Q.generateQuest(s, ctx, 'scan'); if (q.steps[0].target === 'plant') found = q; }
  assert.ok(found, 'a plant scan quest exists');
  const st = found.steps[0];
  const r = Q.progress(found, { type: 'scan', planetId: st.planetId, target: 'plant' });
  const nq = r.quest || r;
  assert.equal(nq.steps[0].have, 1);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import * as PKP from '../../js/pokemans-party.js';
import * as PKG from '../../js/pokemans-gen.js';
import * as PKC from '../../js/pokemans-catch.js';

function mk(n, level) { const sp = PKG.species(n, 'a'), m = PKC.makeMon(sp, PKG.individual(sp, n), level || 5); return m; }
const prof = (gor, lv) => { const a = mk(1, lv), b = mk(2, lv), c = mk(3, lv); return { gor, pokemans: [a, b, c], party: [a.id, b.id], pen: [c.id], activeMon: '' }; };

test('revive: nobody dead -> none, no charge', () => {
  const pf = prof(500);
  assert.deepEqual([PKP.reviveAtCvs(pf).why, pf.gor], ['none', 500]);
});
test('revive: costs 10 + 4·level, restores full hp, clears dead', () => {
  const pf = prof(500, 12), m = pf.pokemans[0];
  PKP.faint(m); assert.equal(m.dead, true);
  const r = PKP.reviveAtCvs(pf);
  assert.equal(r.ok, true); assert.equal(r.cost, 10 + 4 * m.level); assert.equal(pf.gor, 500 - r.cost);
  assert.equal(m.dead, false); assert.equal(m.hp, PKP.maxHp(m));
});
test('revive: too poor -> refused, nothing changes', () => {
  const pf = prof(5, 10), m = pf.pokemans[1]; PKP.faint(m);
  const r = PKP.reviveAtCvs(pf);
  assert.equal(r.ok, false); assert.equal(r.why, 'poor'); assert.equal(pf.gor, 5); assert.equal(m.dead, true);
});
test('revive: party first, then pen; one per use', () => {
  const pf = prof(1000, 5); PKP.faint(pf.pokemans[2]); PKP.faint(pf.pokemans[1]);
  assert.equal(PKP.reviveAtCvs(pf).mon.id, pf.pokemans[1].id);
  assert.equal(pf.pokemans[2].dead, true);
  assert.equal(PKP.reviveAtCvs(pf).mon.id, pf.pokemans[2].id);
  assert.equal(PKP.reviveAtCvs(pf).why, 'none');
});
test('shader pad arrays: three GLSL copies, uniforms and NPADU agree on 6 slots (5 outposts+sites were 5)', () => {
  const src = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
  assert.equal((src.match(/uniform vec3 uPadD\[6\]; uniform vec3 uPadP\[6\];/g) || []).length, 3);
  assert.equal((src.match(/for \(int i = 0; i < 6; i\+\+\)/g) || []).length, 3);
  assert.match(src, /var NPADU = 6;/);
  assert.match(src, /var NPAD = 3;/);
  assert.match(src, /for \(var si = 0; si < 3; si\+\+\)/);
  const one = 'new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()]';
  assert.equal(src.split(one).length - 1, 2);
});

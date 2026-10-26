import test from 'node:test';
import assert from 'node:assert/strict';
import { normRig, GAITS, skinFor, STYLES, createStyle } from '../../js/ship-styles.js';

test('PILOT rig = today: unit scales, no extras, gait 1, helmet-top ~1.045', () => {
  const r = normRig(null);
  assert.deepEqual(r.ts, [1, 1, 1]); assert.deepEqual(r.hs, [1, 1, 1]);
  assert.equal(r.arm, 1); assert.equal(r.leg, 1); assert.equal(r.hipOff, 0); assert.deepEqual(r.extras, []);
  assert.deepEqual(r.gait, GAITS.PILOT); assert.ok(Math.abs(r.top - 1.045) < 1e-9);
});
test('bad input falls back to defaults / clamps', () => {
  const r = normRig({ headShape: 'dodecahedron', torso: 7, headScale: 'x', limbLen: { arm: 99, leg: -3 }, quills: 'a', tail: { len: 50, spikes: 99 } });
  assert.equal(r.headShape, 'round'); assert.equal(r.torso, 'normal'); assert.deepEqual(r.hs, [1, 1, 1]);
  assert.equal(r.arm, 1.6); assert.equal(r.leg, 0.5); assert.equal(r.quills, 0); assert.equal(r.tail.len, 3); assert.equal(r.tail.spikes, 6);
});
test('leg length lifts the hips so feet stay on y=0', () => {
  const r = normRig({ limbLen: { leg: 1.4 } });
  assert.ok(Math.abs(r.hipOff - 0.2) < 1e-9);
  assert.ok(r.hitbox.h > normRig(null).hitbox.h);
});
test('recipes: the five rigs are distinct', () => {
  const R = {}; for (const id of ['MARO', 'JOSHI', 'STEEV', 'SONIK']) R[id] = normRig(skinFor(id).rig, id);
  assert.equal(R.MARO.torso, 'stout'); assert.equal(R.MARO.feet, 'big'); assert.ok(R.MARO.cap); assert.ok(R.MARO.arm < 1); assert.ok(R.MARO.extras.includes('cap')); assert.ok(R.MARO.extras.includes('bigfeet'));
  assert.equal(R.JOSHI.torso, 'lizard'); assert.ok(R.JOSHI.tail.len > 1.5); assert.ok(R.JOSHI.extras.includes('snout') && R.JOSHI.extras.includes('tail') && R.JOSHI.extras.includes('crest'));
  assert.equal(R.STEEV.headShape, 'cube'); assert.equal(R.STEEV.hs[0], 2); assert.equal(R.STEEV.neck, false); assert.ok(R.STEEV.extras.includes('blocky'));
  assert.equal(R.SONIK.quills, 5); assert.ok(R.SONIK.leg > 1.3); assert.ok(R.SONIK.torso === 'slim'); assert.ok(R.SONIK.extras.includes('quills') && R.SONIK.extras.includes('visor'));
  const sigs = new Set(Object.values(R).map(r => JSON.stringify([r.ts, r.hs, r.arm, r.leg, r.gait])));
  assert.equal(sigs.size, 4);
});
test('gait table: every style has one, SONIK fastest stride, STEEV smallest bounce', () => {
  for (const id of ['PILOT', 'MARO', 'JOSHI', 'STEEV', 'SONIK']) assert.ok(GAITS[id] && GAITS[id].stride > 0);
  assert.ok(GAITS.SONIK.stride > GAITS.MARO.stride && GAITS.STEEV.bounce < GAITS.MARO.bounce);
  assert.deepEqual(skinFor('SONIK').rig.gait, GAITS.SONIK);
});
test('skinFor: extra derives from the recipe; unknown id has no rig; hitbox from rig', () => {
  const s = skinFor('maro'); assert.deepEqual(s.extra, normRig(s.rig, 'MARO').extras); assert.ok(s.hitbox.h > 0.8 && s.hitbox.r > 0.2);
  assert.equal(skinFor('nope').rig, undefined);
  assert.ok(createStyle('STEEV').skin.rig);
});

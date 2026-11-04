import test from 'node:test';
import assert from 'node:assert/strict';
import { normRig, rigParts, skinFor, createStyle } from '../../js/ship-styles.js';

const IDS = ['MARO', 'JOSHI', 'STEEV', 'SONIK'];
const recipes = { PILOT: normRig(null, 'PILOT') }; for (const id of IDS) recipes[id] = normRig(skinFor(id).rig, id);
const parts = {}; for (const id in recipes) parts[id] = rigParts(recipes[id]);
const overlap = (a, b) => { const B = new Set(b); return a.filter(x => B.has(x)).length / Math.max(a.length, b.length); };

test('5 recipes exist (PILOT + 4 skins), each with a non-empty unique part list', () => {
  assert.equal(Object.keys(parts).length, 5);
  for (const id in parts) { assert.ok(parts[id].length >= 10, id); assert.equal(new Set(parts[id]).size, parts[id].length, id + ' no dup tokens'); }
  assert.equal(new Set(Object.values(parts).map(p => p.join('|'))).size, 5);
});
test('no two styles share more than 60 % of parts', () => {
  const ids = Object.keys(parts);
  for (let i = 0; i < ids.length; i++) for (let j = i + 1; j < ids.length; j++) { const o = overlap(parts[ids[i]], parts[ids[j]]); assert.ok(o <= 0.6, `${ids[i]} vs ${ids[j]}: ${(o * 100).toFixed(0)} %`); }
});
test('distinguishing features: head shape, torso, limb lengths, feet, tail / quills', () => {
  const R = recipes;
  assert.equal(new Set(IDS.map(i => R[i].torso)).size, 4, 'four torsos');
  assert.ok(R.JOSHI.tail && !R.MARO.tail && !R.STEEV.tail && !R.SONIK.tail, 'tail is JOSHI only');
  assert.ok(R.SONIK.quills > 0 && IDS.filter(i => R[i].quills > 0).length === 1, 'quills are SONIK only');
  assert.equal(R.STEEV.headShape, 'cube'); assert.equal(R.JOSHI.headShape, 'snout'); assert.equal(R.MARO.headShape, 'round');
  assert.ok(R.SONIK.leg > R.MARO.leg && R.MARO.arm < R.STEEV.arm, 'limb lengths differ');
  assert.equal(R.MARO.feet, 'big'); assert.equal(R.SONIK.feet, 'big'); assert.equal(R.STEEV.feet, 'normal');
});
test('hitbox h/r match the recipe height (per style, skin + registry + createStyle)', () => {
  for (const id of IDS) {
    const r = recipes[id], hipY = 0.52 + 0.5 * (r.leg - 1), top = hipY + (0.93 - 0.52) * r.ts[1] - (r.neck ? 0 : 0.04) + 0.115 * r.hs[1];
    assert.ok(Math.abs(r.top - top) < 1e-9, id + ' top'); assert.ok(Math.abs(r.hitbox.h - top) < 1e-3, `${id} h ${r.hitbox.h} vs ${top}`);
    assert.ok(r.hitbox.r > 0.15 && r.hitbox.r < 0.6, id + ' r ' + r.hitbox.r);
    assert.deepEqual(skinFor(id).hitbox, r.hitbox);
    const live = createStyle(id, {}).hitbox; assert.equal(live.r, r.hitbox.r, id + ' live r'); assert.equal(id === 'SONIK' ? live.hStand : live.h, r.hitbox.h, id + ' live h');
  }
  assert.ok(recipes.SONIK.hitbox.h > recipes.MARO.hitbox.h, 'taller legs = taller capsule');
});
test('no NaN / Infinity anywhere in any normalised rig', () => {
  const walk = (v, path) => { if (typeof v === 'number') assert.ok(Number.isFinite(v), path + ' = ' + v); else if (Array.isArray(v)) v.forEach((x, i) => walk(x, path + '[' + i + ']')); else if (v && typeof v === 'object') for (const k in v) walk(v[k], path + '.' + k); };
  for (const id in recipes) walk(recipes[id], id);
  for (const bad of [{ headScale: NaN, limbLen: { arm: NaN, leg: Infinity }, tail: { len: NaN }, quills: NaN }, { headScale: { x: NaN } }, {}]) walk(normRig(bad), 'bad');
});

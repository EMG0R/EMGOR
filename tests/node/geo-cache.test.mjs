import test from 'node:test';
import assert from 'node:assert/strict';
import { createGeoCache, VILLAGER_TRI_BUDGET } from '../../js/ship-geo-cache.js';

const mk = () => { let freed = 0; return { free: () => { freed++; }, get freed() { return freed; } }; };

test('same key builds once, later acquires hit', () => {
  const c = createGeoCache(); let built = 0;
  for (let i = 0; i < 20; i++) c.acquire('b|farmer', () => ++built);
  assert.equal(built, 1);
  const s = c.stats(); assert.equal(s.hits, 19); assert.equal(s.misses, 1); assert.ok(s.rate >= 0.95);
});
test('village mix: 4 roles x 25 villagers -> 4 builds, hit rate >= 0.95', () => {
  const c = createGeoCache(); let built = 0;
  for (let i = 0; i < 100; i++) c.acquire('b|' + ['farmer', 'trader', 'elder', 'kid'][i % 4], () => ++built);
  assert.equal(built, 4); assert.ok(c.stats().rate >= 0.95, String(c.stats().rate));
});
test('release frees only at refs 0 and drops the entry', () => {
  const c = createGeoCache(), f = mk();
  c.acquire('k', () => 1); c.acquire('k', () => 2);
  c.release('k', f.free); assert.equal(f.freed, 0); assert.equal(c.refs('k'), 1);
  c.release('k', f.free); assert.equal(f.freed, 1); assert.equal(c.stats().size, 0);
  let built = 0; c.acquire('k', () => ++built); assert.equal(built, 1, 'rebuilt after full release');
});
test('release of an unknown key is a no-op', () => { const c = createGeoCache(), f = mk(); c.release('nope', f.free); assert.equal(f.freed, 0); });
test('villager budget constant', () => assert.equal(VILLAGER_TRI_BUDGET, 800));

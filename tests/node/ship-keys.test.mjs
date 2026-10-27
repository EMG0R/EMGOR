import test from 'node:test';
import assert from 'node:assert/strict';
import { roomKey, hash8, putRecent, cooldownMax } from '../../js/ship-keys.js';

test('roomKey: short ids unchanged, long ids <= 24 and relay-legal', () => {
  assert.equal(roomKey('emgor.home'), 'emgor.home');
  assert.equal(roomKey('a b/c'), 'a_b_c');
  const ids = ['emgor.papers.neptr-performance-system.hyperguitar', 'emgor.papers.neptr-performance-system.neuralgrid', 'emgor.papers.neptr-performance-system.hypertrumpet', 'emgor.archive.generative.a', 'emgor.archive.generative.b'];
  const keys = ids.map(roomKey);
  for (const k of keys) { assert.ok(k.length <= 24); assert.match(k, /^[A-Za-z0-9_.:-]{1,24}$/); }
  assert.equal(new Set(keys).size, ids.length, 'siblings with a shared 24-char prefix must not collide');
});
test('roomKey is deterministic and idempotent', () => {
  const id = 'emgor.papers.neptr-performance-system.pocket-opgorator';
  assert.equal(roomKey(id), roomKey(id));
  assert.equal(roomKey(roomKey(id)), roomKey(id));
  assert.equal(hash8('x').length, 8);
});
test('putRecent: evicts least-recently-written, never the key just written', () => {
  const m = {}; for (let i = 0; i < 4; i++) putRecent(m, 'k' + i, i, 4);
  putRecent(m, 'k0', 'new', 4);               // oldest-inserted key is rewritten: must survive
  assert.equal(m.k0, 'new');
  assert.deepEqual(Object.keys(m), ['k1', 'k2', 'k3', 'k0']);
  putRecent(m, 'k9', 9, 4);                    // evicts k1 (least recent), not k0
  assert.deepEqual(Object.keys(m), ['k2', 'k3', 'k0', 'k9']);
  const z = {}; putRecent(z, 'only', 1, 0); assert.equal(z.only, 1);
});
test('cooldownMax never shortens', () => {
  assert.equal(cooldownMax(1000, 500), 1000);
  assert.equal(cooldownMax(undefined, 500), 500);
  assert.equal(cooldownMax(0, 500), 500);
});

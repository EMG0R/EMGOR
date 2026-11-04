import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
const B = createRequire(import.meta.url)('../../server/nmg-relay/board.js');

test('bump/best/proto safety', () => {
  const b = {};
  assert.ok(B.bump(b, 'Ann', 'kills', 1, 5)); assert.ok(B.bump(b, 'ann', 'kills', 2, 6));
  assert.equal(b.ann.kills, 3); assert.equal(b.ann.name, 'Ann');
  assert.equal(B.bump(b, '__proto__', 'kills', 1, 1), false);
  assert.equal(B.bump(b, 'ann', 'bogus', 1, 1), false);
  assert.ok(B.best(b, 'ann', 'depth', 3, 7)); assert.equal(B.best(b, 'ann', 'depth', 2, 8), false); assert.equal(b.ann.depth, 3);
});
test('fromProfile reads dex size + depth stack, ignores junk', () => {
  const b = {};
  assert.ok(B.fromProfile(b, 'bo', { dex: { seen: { a: 1, b: 2 } }, depthStack: [1, 2, 3] }, 1));
  assert.equal(b.bo.catches, 2); assert.equal(b.bo.depth, 3);
  assert.equal(B.fromProfile(b, 'bo', null, 2), false);
  assert.equal(B.fromProfile(b, 'bo', { dex: { seen: [1, 2, 3] }, depthStack: 'x' }, 2), false);
});
test('winnerName maps slot to fighter order', () => {
  assert.equal(B.winnerName({ winner: 1 }, ['a', 'b']), 'b');
  assert.equal(B.winnerName({ winner: -1 }, ['a', 'b']), null);
  assert.equal(B.winnerName({ winner: 5 }, ['a', 'b']), null);
  assert.equal(B.winnerName(null, ['a']), null);
});
test('row cap evicts oldest; top sorts', () => {
  const b = {};
  for (let i = 0; i < B.MAX_ROWS + 5; i++) B.bump(b, 'p' + i, 'wins', 1, i + 1);
  assert.equal(Object.keys(b).length, B.MAX_ROWS);
  assert.ok(!b.p0);
  B.bump(b, 'p300', 'wins', 9, 9999);
  assert.equal(B.top(b, 1)[0].name, 'p300');
});

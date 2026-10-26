import test from 'node:test';
import assert from 'node:assert/strict';
import { wallNormal2 } from '../../js/ship-styles.js';

const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-9, a + ' vs ' + b);

test('head-on wall, slide along +x: normal opposes the move (z back = +)', () => {
  const o = { x: 0, z: 0 };
  assert.equal(wallNormal2(0, -1, 1, 0, o), true);   // moving forward (z=-1), wall runs along x
  near(o.x, 0); near(o.z, 1);
});

test('angled wall: normal is perpendicular to the slide and faces against the move', () => {
  const o = { x: 0, z: 0 };
  // moving +x, wall runs along the diagonal (1,-1)/sqrt2 slide; normal must have negative x
  assert.equal(wallNormal2(1, 0, 1, -1, o), true);
  near(Math.hypot(o.x, o.z), 1);
  near(o.x * 1 + o.z * -1, 0);
  assert.ok(o.x < 0);
});

test('slide flipped gives the same normal', () => {
  const a = { x: 0, z: 0 }, b = { x: 0, z: 0 };
  wallNormal2(0.3, -1, 1, 0.2, a); wallNormal2(0.3, -1, -1, -0.2, b);
  near(a.x, b.x); near(a.z, b.z);
});

test('degenerate: zero slide or slide parallel to the move', () => {
  const o = { x: 7, z: 7 };
  assert.equal(wallNormal2(0, -1, 0, 0, o), false);
  assert.equal(wallNormal2(0, -1, 0, -1, o), false);
  assert.equal(o.x, 7);
});

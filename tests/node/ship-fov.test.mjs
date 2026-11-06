import test from 'node:test';
import assert from 'node:assert/strict';
import { targetFov, FOV_FOOT, FOV_FLY_CAP } from '../../js/ship-fov.js';

test('targetFov: per-mode, capped, deterministic', () => {
  assert.equal(targetFov({ mode: 'foot', base: 42, pulse: 14, kick: 9 }), FOV_FOOT);
  assert.equal(targetFov({ mode: 'landed', base: 42, pulse: 14 }), 42);
  assert.equal(targetFov({ mode: 'sfoot', base: 42 }), 42);
  assert.equal(targetFov({ mode: 'fly', base: 42 }), 50);
  assert.equal(targetFov({ mode: 'fly', base: 42, boost: 4, pulse: 14, focus: 3, kick: 8, atm: 8, planet: 4, sty: 1.25 }), FOV_FLY_CAP);
  assert.equal(targetFov({ mode: 'fly', base: 42, kick: NaN }), 50);
  const a = { mode: 'fly', base: 42, pulse: 5 }; assert.equal(targetFov(a), targetFov(a));
});

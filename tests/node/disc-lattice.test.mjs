import test from 'node:test';
import assert from 'node:assert/strict';
import { discLattice, DISC_SKIRT_R, HEIGHT_GLSL } from '../../js/ship-planet.js';
import { readFileSync } from 'node:fs';

test('site disc lattice: counts, bounds, indices', () => {
  const n = 28, m = 72, L = discLattice(n, m), rings = n + 2, per = m + 1;
  assert.equal(L.pos.length, rings * per * 3);
  assert.equal(L.idx.length, (rings - 1) * m * 6);
  assert.ok(rings * per < 65536, 'fits Uint16');
  let maxI = 0; for (const i of L.idx) maxI = Math.max(maxI, i);
  assert.equal(maxI, rings * per - 1);
  for (let j = 0; j < rings; j++) for (let i = 0; i < per; i++) {
    const o = (j * per + i) * 3, r = Math.hypot(L.pos[o], L.pos[o + 1]);
    assert.ok(Math.abs(r - (j <= n ? j / n : DISC_SKIRT_R)) < 1e-5);
    assert.equal(L.pos[o + 2], j > n ? 1 : 0);
  }
});

test('site disc lattice: closed seam, centre ring degenerate, skirt only outside r = 1', () => {
  const L = discLattice(8, 12), per = 13;
  for (let j = 0; j < 10; j++) { const a = j * per * 3, b = (j * per + 12) * 3; assert.ok(Math.abs(L.pos[a] - L.pos[b]) < 1e-5 && Math.abs(L.pos[a + 1] - L.pos[b + 1]) < 1e-5); }
  for (let i = 0; i < per; i++) assert.equal(Math.hypot(L.pos[i * 3], L.pos[i * 3 + 1]), 0);
  assert.ok(DISC_SKIRT_R > 1);
});

test('patch and disc shaders share one pad-flattening function', () => {
  const src = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
  assert.equal((src.match(/float hfunP\(vec3 d\)/g) || []).length, 1);
  assert.ok(/HFUNP_GLSL,\s*\n\s*'vec3 dirAt/.test(src));
  assert.ok(/HEIGHT_GLSL,\s*\n\s*HFUNP_GLSL,/.test(src));
  assert.ok(HEIGHT_GLSL.includes('float hfun('));
});

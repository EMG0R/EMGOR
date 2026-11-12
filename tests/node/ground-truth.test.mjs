// ground truth (2026-11-11): the pure floor math behind ps.groundTruth. Structure slab = tangent plane family; pad terrain = the SAME plane; one query = max of the two.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { affInverse, makeFloorSet, slabRadius, growPad, STEP_WALK_H } from '../../js/ship-planet.js';

const src = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
function rng(a) { return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
const norm = (v) => { const l = Math.hypot(...v); return v.map((x) => x / l); };
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
// structure matrix: Y = up (unit dir d), position = d * rad, uniform scale K, yaw about up
function frame(d, rad, K, yaw) {
  const ref = Math.abs(d[1]) < 0.9 ? [0, 1, 0] : [1, 0, 0], x0 = norm(cross(ref, d)), z0 = cross(d, x0);
  const c = Math.cos(yaw), s = Math.sin(yaw), x = x0.map((v, i) => v * c + z0[i] * s), z = x0.map((v, i) => -v * s + z0[i] * c);
  const e = new Float64Array(16); [x, d, z].forEach((a, j) => { for (let i = 0; i < 3; i++) e[j * 4 + i] = a[i] * K; });
  e[12] = d[0] * rad; e[13] = d[1] * rad; e[14] = d[2] * rad; e[15] = 1; return e;
}
const R = 320;

test('affInverse inverts random rotation*scale+translation', () => {
  const r = rng(7);
  for (let k = 0; k < 20; k++) {
    const d = norm([r() - 0.5, r() - 0.5, r() - 0.5]), m = frame(d, 100 + r() * 300, 0.1 + r(), r() * 6.28), inv = affInverse(m);
    for (let t = 0; t < 4; t++) {
      const p = [r() * 9 - 4, r() * 9 - 4, r() * 9 - 4], w = [0, 1, 2].map((i) => m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i]);
      const b = [0, 1, 2].map((i) => inv[i] * w[0] + inv[4 + i] * w[1] + inv[8 + i] * w[2] + inv[12 + i]);
      for (let i = 0; i < 3; i++) assert.ok(Math.abs(b[i] - p[i]) < 1e-9, 'round trip');
    }
  }
});

test('slabRadius: plane radius inside the rect, 0 outside, edge step is the plane height only', () => {
  const d = norm([0.3, 0.8, -0.5]), K = 0.2, h = 0.04, rad = R * (1 + h), M = frame(d, rad, K, 0.7);
  const S = makeFloorSet(M, K, [{ x0: -10, x1: 10, z0: -6, z1: 6, top: 0.2 }]);
  const at = (lx, lz) => { const p = [0, 1, 2].map((i) => M[i] * lx + M[4 + i] * 0.2 + M[8 + i] * lz + M[12 + i]); return norm(p); };
  const c = slabRadius(S, ...at(0, 0), R);
  assert.ok(Math.abs(c - (rad + 0.2 * K)) < 1e-9, 'centre = plane top');
  // every inside point lies on the plane: radius * (dir . up) = const
  for (const [lx, lz] of [[9.9, 5.9], [-9.9, 5.9], [9.9, -5.9], [-9.9, -5.9], [3, -2]]) { const dir = at(lx, lz), rr = slabRadius(S, ...dir, R); assert.ok(rr > 0); assert.ok(Math.abs(rr * (dir[0] * d[0] + dir[1] * d[1] + dir[2] * d[2]) - (rad + 0.2 * K)) < 1e-6); }
  assert.equal(slabRadius(S, ...at(10.2, 0), R), 0); assert.equal(slabRadius(S, ...at(0, -6.2), R), 0);
  assert.equal(slabRadius(S, ...norm([-d[0], -d[1], -d[2]]), R), 0, 'far side');
});

test('pad plane == slab plane family: the terrain under a slab IS the slab (no gap, no buried step)', () => {
  // pad: tangent plane at the pad centre d, height h -> radius R(1+h)/cos(theta). Slab seated at top 0: same plane. Must agree to 1e-9 anywhere in the footprint.
  const d = norm([-0.6, 0.1, 0.79]), h = 0.03, rad = R * (1 + h), K = 0.35, M = frame(d, rad, K, 1.9);
  const S = makeFloorSet(M, K, [{ x0: -30, x1: 30, z0: -20, z1: 20, top: 0 }]);
  for (let i = 0; i < 200; i++) {
    const r = rng(i + 3), lx = (r() * 2 - 1) * 29, lz = (r() * 2 - 1) * 19, p = [0, 1, 2].map((q) => M[q] * lx + M[8 + q] * lz + M[12 + q]), dir = norm(p);
    const cos = dir[0] * d[0] + dir[1] * d[1] + dir[2] * d[2], padR = R * (1 + h) / Math.max(cos, 0.5), slab = slabRadius(S, ...dir, R);
    assert.ok(Math.abs(slab - padR) < 1e-9, 'slab ' + slab + ' pad ' + padR);
  }
});

test('growPad keeps the shoulder and only grows', () => {
  const pd = { flat: 0.01, blend: 0.03, sh: 0.02 };
  assert.equal(growPad(pd, 0.005), false); assert.equal(pd.flat, 0.01);
  assert.equal(growPad(pd, 0.05), true); assert.ok(Math.abs(pd.flat - 0.05) < 1e-12 && Math.abs(pd.blend - 0.07) < 1e-12);
});

test('single mechanism: pad height is the tangent plane in JS and in both shaders; step rule constant', () => {
  assert.equal((src.match(/\(1\.0 \+ uPadP\[i\]\.z\) \/ max\(dot\(d, uPadD\[i\]\), 0\.5\) - 1\.0/g) || []).length, 2, 'both shader copies');
  assert.ok(/\(1 \+ pd\.h\) \/ \(pdd > 0\.5 \? pdd : 0\.5\) - 1/.test(src), 'JS hFrac');
  assert.ok(!/mix\(h, uPadP\[i\]\.z,/.test(src), 'no sphere-cap pad left');
  assert.equal(STEP_WALK_H, 0.35);
});

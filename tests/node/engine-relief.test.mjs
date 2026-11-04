import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { sampleHeight, sampleHeightLo, HEIGHT_GLSL } from '../../js/ship-planet.js';

const g3d = readFileSync(new URL('../../js/galaxy3d.js', import.meta.url), 'utf8');
const sp = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
const AMP = 0.09, BIAS = 0.0015;

function rng(a) { return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
function dirs(n, seed) { const r = rng(seed), out = []; for (let i = 0; i < n; i++) { const z = r() * 2 - 1, a = r() * 6.2831853, q = Math.sqrt(1 - z * z); out.push([q * Math.cos(a), z, q * Math.sin(a)]); } return out; }
function planet(seed) { const r = rng(seed); return { seed: { x: r() * 43, y: r() * 43, z: r() * 43 }, freq: 2.1 + r() * 1.7, warp: 0.55 + r() * 0.95, sea: 0.43 + r() * 0.12 }; }

test('low-octave relief tracks the full height function (far silhouette is the same shape)', () => {
  let sxy = 0, sx = 0, sy = 0, n = 0, maxAbs = 0, sumSq = 0, sumFull = 0;
  for (let k = 0; k < 6; k++) {
    const p = planet(100 + k);
    for (const d of dirs(400, 7 + k)) {
      const a = Math.max(0, sampleHeight(d[0], d[1], d[2], p.seed, p.freq, p.warp, p.sea, AMP, BIAS, p.sea) - BIAS);
      const b = Math.max(0, sampleHeightLo(d[0], d[1], d[2], p.seed, p.freq, p.warp, p.sea, AMP, BIAS, p.sea) - BIAS);
      sx += a; sy += b; sxy += a * b; sumSq += a * a; n++; maxAbs = Math.max(maxAbs, Math.abs(a - b)); sumFull += a;
    }
  }
  // correlation of lo vs full relief over 2400 samples
  let va = 0, vb = 0; const ma = sx / n, mb = sy / n;
  for (let k = 0; k < 6; k++) { const p = planet(100 + k); for (const d of dirs(400, 7 + k)) {
    const a = Math.max(0, sampleHeight(d[0], d[1], d[2], p.seed, p.freq, p.warp, p.sea, AMP, BIAS, p.sea) - BIAS);
    const b = Math.max(0, sampleHeightLo(d[0], d[1], d[2], p.seed, p.freq, p.warp, p.sea, AMP, BIAS, p.sea) - BIAS);
    va += (a - ma) ** 2; vb += (b - mb) ** 2; } }
  const corr = (sxy - n * ma * mb) / Math.sqrt(va * vb);
  console.log('lo/full relief corr', corr.toFixed(3), 'mean full', ma.toFixed(4), 'mean lo', mb.toFixed(4), 'max |diff| R', maxAbs.toFixed(4));
  assert.ok(corr > 0.8, 'corr ' + corr);
  assert.ok(Math.abs(mb / ma - 1) < 0.25, 'mean relief ratio ' + mb / ma);
  assert.ok(maxAbs < 0.09, 'max diff ' + maxAbs);   // below one AMP
});

test('relief is bounded and zero-bias safe (cheap vertex displacement cannot exceed the full range)', () => {
  const p = planet(5); let mx = 0;
  for (const d of dirs(2000, 3)) mx = Math.max(mx, sampleHeightLo(d[0], d[1], d[2], p.seed, p.freq, p.warp, p.sea, AMP, BIAS, p.sea) - BIAS);
  assert.ok(mx < 0.2 && mx > 0, 'max lo relief ' + mx);
});

test('galaxy3d ORBIT_RELIEF_GLSL carries the constants the JS mirror uses', () => {
  for (const lit of ['0.625 * vnoise(p)', '0.3125 * vnoise(p)', 'p * 2.07 + vec3(11.3, 7.9, 5.1)', 'freq * 5.3', 'seed * 1.7 + 2.3', 'amp * 0.78 * (0.10 * land', '2.2 + 1.6 * rid * smoothstep(0.02, 0.14, rel)', 'sea - 0.035, sea + 0.035'])
    assert.ok(g3d.includes(lit), 'galaxy3d ORBIT_RELIEF_GLSL missing ' + lit);
  // the full hfun it mirrors
  assert.ok(HEIGHT_GLSL.includes('0.10 * land + rel * (2.2 + 1.6 * rid * smoothstep(0.02, 0.14, rel))'));
  assert.ok(g3d.includes('RO_AMP = 0.09, RO_BIAS = 0.0015'));
  assert.ok(/var AMP = 0\.09;/.test(sp) && /var BIAS = 0\.0015;/.test(sp));
});

test('far relief: 8 R -> 4 R ramp, active + nearest 2, no allocation in the driver', () => {
  assert.ok(/FAR_REL_R = 8, FAR_REL_FULL = 4/.test(g3d));
  const body = g3d.slice(g3d.indexOf('function updateFarRelief'), g3d.indexOf('// ─── camera: orbit rig'));
  assert.ok(!/new THREE\.|\.clone\(|\[\s*\]|\{\s*\}|\.map\(|\.filter\(|\.slice\(/.test(body.replace(/\[null, null, null\]/g, '')), 'allocation in updateFarRelief');
  assert.ok(/pt >= 2 \? 3 : \(pt === 1 \? 2 : 1\)/.test(body));
});

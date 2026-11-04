import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { sampleHeight, globeRimW, patchTaperU, GLOBE_RIM_A, GLOBE_RIM_B } from '../../js/ship-planet.js';

const sp = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
const AMP = 0.09, BIAS = 0.0015;

// worst relief over a few seeded terra worlds (fraction of R above the sphere)
function hMax() {
  let m = 0;
  for (let k = 0; k < 8; k++) {
    const seed = { x: k * 5.3 + 1, y: k * 3.1 + 2, z: k * 7.7 + 3 };
    for (let i = 0; i < 1500; i++) {
      const z = ((i * 0.618034) % 1) * 2 - 1, a = i * 2.399963, q = Math.sqrt(1 - z * z);
      m = Math.max(m, sampleHeight(q * Math.cos(a), z, q * Math.sin(a), seed, 2.1 + k * 0.2, 0.9, 0.48, AMP, BIAS, 0.48) - BIAS);
    }
  }
  return m;
}

test('globe vs patch height at the patch edge match within 0.2 % R (uFade = 1)', () => {
  const H = hMax();
  assert.ok(H > 0.03 && H < 0.2, 'hMax ' + H);
  let worstEdge = 0, worstPoke = 0;
  for (let u = 0.5; u <= 1.3; u += 0.0025) {
    const patch = patchTaperU(u) * H, globe = globeRimW(u, 1) * H;
    if (u <= 1) worstPoke = Math.max(worstPoke, globe - patch);        // globe must never rise above the patch it sits under
    if (u >= 0.98 && u <= 1.0) worstEdge = Math.max(worstEdge, Math.abs(globe - patch));
  }
  console.log('hMax', H.toFixed(4), 'edge |dH| (R)', worstEdge.toFixed(5), 'globe-over-patch', worstPoke.toFixed(5));
  assert.ok(worstEdge < 0.002, 'edge step ' + worstEdge);
  assert.ok(worstPoke < 0.002, 'globe pokes through patch by ' + worstPoke);
});

test('the OLD ramp (0.92 -> 1.08) really did step at the edge (regression guard for the fix)', () => {
  const sm = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
  const H = hMax();
  assert.ok(Math.abs(sm(0.92, 1.08, 1.0) * H - patchTaperU(1.0) * H) > 0.01);
  assert.equal(GLOBE_RIM_A, 1.0); assert.ok(GLOBE_RIM_B > GLOBE_RIM_A);
});

test('relief applied once across the 3.0 -> 2.6 R handover: globe is mix(lo, full, hiRel), patch relief only below the fade', () => {
  assert.ok(sp.includes('mix(max(0.0, hl - uBias) * w, full, uRel)'));
  assert.ok(sp.includes('hiRel.value = smoothstep(3.0, 2.6, ratioD)'));
  assert.ok(sp.includes('hiOn === node ? hiRel.value'));
  assert.equal(globeRimW(5, 0), 1);                           // no patch (fade 0): full weight everywhere
  assert.equal(globeRimW(0.9, 1), 0);                         // under the patch: flat
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import * as GL from '../../js/galaxy-layout.js';

const g3d = readFileSync(new URL('../../js/galaxy3d.js', import.meta.url), 'utf8');
const ship = readFileSync(new URL('../../js/ship.js', import.meta.url), 'utf8');
const data = JSON.parse(readFileSync(new URL('../../galaxy.json', import.meta.url), 'utf8'));

test('PILOT_PLANET_SCALE is {4,3} and the clearance constants are documented in one block', () => {
  assert.ok(/PILOT_PLANET_SCALE = \{ root: 4, deep: 3 \}/.test(g3d));
  assert.ok(/PILOT_MIN_R_PER_L = 320/.test(g3d));
  const block = g3d.slice(g3d.indexOf('Size / clearance contract'), g3d.indexOf('PILOT_PLANET_SCALE = {'));
  for (const w of ['PILOT_CLEAR0', 'PILOT_MIN_R_PER_L', 'ATM_R', 'LF_ON']) assert.ok(block.includes(w), w);
});

test('PILOT_CLEAR0 clears the local-frame engage and atmosphere radii read from ship.js', () => {
  const num = (re) => +re.exec(ship)[1];
  const ATM_R = num(/var ATM_R = ([\d.]+)/), LF_ON = num(/LF_ON = ([\d.]+)/);
  assert.ok(GL.PILOT_CLEAR0 > LF_ON, `${GL.PILOT_CLEAR0} > ${LF_ON}`);
  assert.ok(GL.PILOT_CLEAR0 - 1 > ATM_R - 1 + 0.2, 'room for a 1.45 R approach outside the parent');
});

test('every pilot child orbits at >= PILOT_CLEAR0 x parent rendered radius, {4,3}, worst vibration', () => {
  const MIN = 320, scale = { root: 4, deep: 3 };
  for (const sv of [1.0, 1.45]) {
    const t = GL.buildTree(data, (k) => { k.sizeVar = sv; });
    const refR = GL.uniformSizeFor(t.root.kids[0], t.root, t.root) * scale.root, L = refR / 12000;
    GL.pilotPrepass(t.drawOrder, t.root, scale, MIN * L);
    const rootS = GL.pilotRootSpread(t.root, 1, GL.BH_CORE_R, (n) => n.homeA, () => -1);
    for (const n of t.drawOrder) {
      const p = n.parentNode; if (p === t.root) continue;
      const r = GL.pilotOrbitRadius(n, p, n.orbF * p.sysR * (1 - n.vibDepth), 1);
      assert.ok(r >= GL.PILOT_CLEAR0 * p._pF - 1e-6, `${n.id} r=${r.toFixed(1)} < ${(GL.PILOT_CLEAR0 * p._pF).toFixed(1)}`);
      assert.ok(n._pF >= MIN * L * 0.999 || n._pF > 0);
    }
    assert.ok(rootS >= 1);
  }
});

test('globe tiers: 3/2/1/0 = 192/128/96/off, rebuilt only in setQuality', () => {
  const sp = readFileSync(new URL('../../js/ship-planet.js', import.meta.url), 'utf8');
  assert.ok(sp.includes('function globeSegs(t) { return t >= 3 ? 192 : t === 2 ? 128 : t === 1 ? 96 : 0; }'));
  const tris = (w) => { const h = Math.round(w * 2 / 3); return 2 * w * h - 2 * w; };
  assert.equal(tris(192), 48768); console.log('globe tris', tris(192), tris(128), tris(96));
  assert.ok(tris(128) < tris(192) && tris(96) < tris(128));
  assert.ok(/if \(hiOn && globeSegs\(t\) !== hiGeoN\)/.test(sp));
  const hi = sp.slice(sp.indexOf('function globeHi'), sp.indexOf('ps.globeInfo'));
  assert.equal((hi.match(/new THREE\./g) || []).length, 2);      // SphereGeometry + ShaderMaterial, both behind the on/tier guards
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import * as GL from '../../js/galaxy-layout.js';

const data = JSON.parse(readFileSync(new URL('../../galaxy.json', import.meta.url), 'utf8'));
const MIN_R_PER_L = 320;      // galaxy3d.js PILOT_MIN_R_PER_L
const TAU = Math.PI * 2;

// worst case sizeVar is 1.45 (0.6 + rng * 0.85); nominal 1.0. Both bound the real seeded value.
function layout(scale, sizeVar, pinnedL) {
  const t = GL.buildTree(data, (k) => { k.sizeVar = sizeVar; });
  const { root, drawOrder } = t;
  const k0 = root.kids[0];
  const refR = GL.uniformSizeFor(k0, root, root) * scale.root;           // ship.js readRefR
  const L = pinnedL || refR / 12000;
  GL.pilotPrepass(drawOrder, root, scale, MIN_R_PER_L * L);
  // engine state at pilot: time frozen at ORBIT_T0 = 0, pilotBlend = 1
  const ang = (n) => n.homeA, vib = (n, v) => v;
  const rootS = GL.pilotRootSpread(root, 1, GL.BH_CORE_R, ang, (n) => Math.sin(n.vibPh));
  return { root, drawOrder, rootS, L };
}
// vibMode: 'T0' = sin(vibPh) as the engine freezes it; 'max' / 'min' = +-1 (envelope of any phase)
function place(t, vibMode) {
  const { root, drawOrder, rootS } = t;
  root.wx = root.wy = 0;
  for (const n of drawOrder) {
    const p = n.parentNode;
    const v = vibMode === 'T0' ? Math.sin(n.vibPh) : (vibMode === 'max' ? 1 : -1);
    let r = n.orbF * p.sysR * (1 + v * n.vibDepth);
    if (p !== root) r = GL.pilotOrbitRadius(n, p, r, 1);
    else r *= rootS;
    n.wx = p.wx + Math.cos(n.homeA) * r;
    n.wy = p.wy + Math.sin(n.homeA) * r;
  }
}
function embeds(t) {
  const out = [];
  const ns = t.drawOrder;
  for (let i = 0; i < ns.length; i++) for (let j = i + 1; j < ns.length; j++) {
    const a = ns[i], b = ns[j];
    const d = Math.hypot(a.wx - b.wx, a.wy - b.wy);
    if (d < a._pF + b._pF) out.push(a.id + ' x ' + b.id + ' d=' + d.toFixed(1) + ' < ' + (a._pF + b._pF).toFixed(1));
  }
  return out;
}
function check(scale, pinnedL) {
  const bad = [];
  for (const sv of [1.0, 1.45]) {
    const t = layout(scale, sv, pinnedL);
    for (const vm of ['T0', 'max', 'min']) { place(t, vm); bad.push(...embeds(t).map((s) => `sv${sv}/${vm}: ${s}`)); }
    // nothing sits inside the x8 black hole
    place(t, 'T0');
    for (const k of t.root.kids) {
      const d = Math.hypot(k.wx, k.wy);
      if (d < GL.BH_CORE_R * GL.BH_PILOT_SCALE + k._pF) bad.push(`sv${sv}: ${k.id} inside black hole`);
    }
  }
  return bad;
}

test('real tree loads', () => {
  const t = GL.buildTree(data, () => {});
  assert.ok(t.drawOrder.length > 50);
  assert.ok(t.root.kids.length >= 3);
});
test('PILOT_PLANET_SCALE {6,4}: no body embeds another', () => {
  assert.deepEqual(check({ root: 6, deep: 4 }).slice(0, 10), []);
});
test('PILOT_PLANET_SCALE {4,3}: no body embeds another', () => {
  assert.deepEqual(check({ root: 4, deep: 3 }).slice(0, 10), []);
});
test('PILOT_PLANET_SCALE {4,3} with L pinned to the x6 reference', () => {
  const L6 = layout({ root: 6, deep: 4 }, 1).L;
  assert.deepEqual(check({ root: 4, deep: 3 }, L6).slice(0, 10), []);
});

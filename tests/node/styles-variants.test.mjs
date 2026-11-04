import test from 'node:test';
import assert from 'node:assert/strict';
import { createStyle, STYLES, normRig, rigParts, skinFor, GAITS } from '../../js/ship-styles.js';

const NEW = ['SWIFT', 'SPARKLET', 'JELLY', 'HAULER'], DT = 1 / 60;
const G = { onGround: true, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
const A = { onGround: false, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
const fxTypes = o => o.fx.map(f => f.type);
function mk(id) { return createStyle(id, { walk: 22, grav: 22, jump: 8.5 }); }

test('4 new styles registered with skin, gait, abilities, hitbox', () => {
  for (const id of NEW) {
    assert.ok(STYLES[id], id); assert.ok(GAITS[id], id + ' gait'); const st = mk(id);
    assert.ok(st.abilities.length >= 3, id); assert.ok(st.skin.rig && st.skin.rig.gait, id + ' rig'); assert.ok(st.hitbox.r > 0.1 && st.hitbox.h > 0.5, id + ' hitbox');
    assert.equal(st.hitbox.h, skinFor(id).hitbox.h, id + ' capsule = recipe');
  }
});

const IDS = ['PILOT', 'MARO', 'JOSHI', 'STEEV', 'SONIK', ...NEW];
const parts = {}; for (const id of IDS) parts[id] = rigParts(normRig(id === 'PILOT' ? null : skinFor(id).rig, id));
const overlap = (a, b) => { const B = new Set(b); return a.filter(x => B.has(x)).length / Math.max(a.length, b.length); };
test('rigParts: every new style shares <= 60 % of parts with every other style', () => {
  for (const n of NEW) for (const o of IDS) { if (o === n) continue; const v = overlap(parts[n], parts[o]); assert.ok(v <= 0.6, `${n} vs ${o}: ${(v * 100).toFixed(0)} %`); }
  assert.equal(new Set(IDS.map(i => parts[i].join('|'))).size, IDS.length);
});

test('determinism: same input stream, same trace', () => {
  for (const id of NEW) {
    const run = () => { const st = mk(id); let t = ''; for (let i = 0; i < 300; i++) { const o = st.step({ keys: { KeyW: i % 40 < 30, Space: i % 50 === 0, KeyC: i % 90 > 70 }, mouse: { l: i % 33 === 0, r: i % 77 < 5 }, camYaw: 0 }, i % 60 < 45 ? G : A, DT); t += o.vel.x.toFixed(5) + o.vel.y.toFixed(5) + o.vel.z.toFixed(5) + fxTypes(o).join(); } return t; };
    assert.equal(run(), run(), id);
  }
});

test('walk caps: speed ratios vs env.walk 22', () => {
  const peak = id => { const st = mk(id); let p = 0; for (let i = 0; i < 300; i++) { const o = st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, G, DT); p = Math.max(p, Math.hypot(o.vel.x, o.vel.z)); } return p; };
  const w = { HAULER: peak('HAULER'), JELLY: peak('JELLY'), SWIFT: peak('SWIFT'), SPARKLET: peak('SPARKLET') };
  assert.ok(Math.abs(w.HAULER - 22 * 0.7) < 0.1, 'HAULER ' + w.HAULER); assert.ok(Math.abs(w.SPARKLET - 22 * 1.4) < 0.1, 'SPARKLET ' + w.SPARKLET);
  assert.ok(w.HAULER < w.JELLY && w.JELLY < w.SWIFT && w.SWIFT < w.SPARKLET, JSON.stringify(w));
  for (const id of NEW) assert.ok(w[id] < 22 * 1.5, id + ' cap');
});

// double-tap W: press, release, press within 0.25 s
function tapTap(st, ctx, gap) {
  st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, ctx, DT);
  for (let i = 0; i < gap; i++) st.step({ keys: {}, mouse: {}, camYaw: 0 }, ctx, DT);
  return st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, ctx, DT);
}
test('SWIFT: double-tap dashes 0.35 s with i-frames, single tap does not, air dash once per airtime', () => {
  const st = mk('SWIFT'); let o = tapTap(st, G, 6);
  const d = o.fx.find(f => f.type === 'dash'); assert.ok(d && d.v === 0.15 && d.n === 0, 'ground dash');
  let steps = 0, peak = 0; while (steps < 60) { o = st.step({ keys: {}, mouse: {} }, G, DT); if (st.state.dashT <= 0) break; peak = Math.max(peak, Math.hypot(o.vel.x, o.vel.z)); steps++; }
  assert.ok(steps >= 19 && steps <= 22, 'dash length ' + steps); assert.ok(peak > 22 * 3, 'dash speed ' + peak);
  const s2 = mk('SWIFT'); o = tapTap(s2, G, 30); assert.ok(!fxTypes(o).includes('dash'), 'slow double press is not a dash');
  const s3 = mk('SWIFT'); let nd = 0;
  for (let k = 0; k < 3; k++) { o = tapTap(s3, A, 6); if (fxTypes(o).includes('dash')) nd++; for (let i = 0; i < 60; i++) { s3.step({ keys: {}, mouse: {} }, A, DT); } }
  assert.equal(nd, 1, 'one air dash until landing'); s3.step({ keys: {}, mouse: {} }, G, DT); s3.step({ keys: {}, mouse: {} }, A, DT);
  for (let i = 0; i < 40; i++) s3.step({ keys: {}, mouse: {} }, A, DT);
  assert.ok(fxTypes(tapTap(s3, A, 6)).includes('dash'), 'air dash restored after landing');
});
test('SWIFT: fast fall falls faster, right click reflects', () => {
  const fall = held => { const st = mk('SWIFT'); st.step({ keys: {}, mouse: {} }, A, DT); let v = 0; for (let i = 0; i < 20; i++) v = st.step({ keys: held, mouse: {} }, A, DT).vel.y; return v; };
  assert.ok(fall({ KeyC: true }) < fall({}) - 1, 'fast fall');
  const st = mk('SWIFT'); st.step({ keys: {}, mouse: {} }, G, DT);
  assert.ok(fxTypes(st.step({ keys: {}, mouse: { r: true } }, G, DT)).includes('reflect'));
});
test('SPARKLET: left click emits a zap that chains 2 for 5 dmg, rate limited', () => {
  const st = mk('SPARKLET'); st.step({ keys: {}, mouse: {} }, G, DT);
  const o = st.step({ keys: {}, mouse: { l: true } }, G, DT), z = o.fx.find(f => f.type === 'zap');
  assert.ok(z && z.n === 2 && z.dmg === 5 && z.r === 6);
  st.step({ keys: {}, mouse: {} }, G, DT); assert.ok(!fxTypes(st.step({ keys: {}, mouse: { l: true } }, G, DT)).includes('zap'), 'cooldown');
});
test('JELLY: 5 jumps total then glide; sleep heals and flags vulnerable', () => {
  const st = mk('JELLY'); let jumps = 0, o;
  for (let i = 0; i < 600; i++) {
    const air = i > 2; o = st.step({ keys: { Space: i % 6 === 1 }, mouse: {} }, air ? A : G, DT); if (o.fx.some(f => f.type === 'jump')) jumps++;
  }
  assert.equal(jumps, 5, 'mini-jumps');
  const gl = mk('JELLY'); for (let i = 0; i < 6; i++) gl.step({ keys: { Space: i === 1 }, mouse: {} }, i ? A : G, DT);
  for (let i = 0; i < 4; i++) { gl.step({ keys: { Space: true }, mouse: {} }, A, DT); gl.step({ keys: {}, mouse: {} }, A, DT); }
  let vy = 0; for (let i = 0; i < 120; i++) vy = gl.step({ keys: { Space: true }, mouse: {} }, A, DT).vel.y;
  assert.ok(vy >= -3.0001 && vy < 0, 'glide clamps fall ' + vy);
  const sl = mk('JELLY'); let heals = 0, vul = false;
  for (let i = 0; i < 130; i++) { o = sl.step({ keys: {}, mouse: { r: true } }, G, DT); if (o.fx.some(f => f.type === 'regen' && f.v === 5)) heals++; vul = o.vulnerable; }
  assert.equal(heals, 2); assert.ok(vul);
});
test('HAULER: quake pound hangs then lands with 14 dmg r 3; grab then throw', () => {
  const st = mk('HAULER'); st.step({ keys: {}, mouse: {} }, A, DT);
  assert.ok(fxTypes(st.step({ keys: { KeyC: true }, mouse: {} }, A, DT)).includes('poundStart'));
  let o; for (let i = 0; i < 80; i++) o = st.step({ keys: {}, mouse: {} }, A, DT);
  assert.ok(o.vel.y < -5, 'falling hard ' + o.vel.y);
  o = st.step({ keys: {}, mouse: {} }, G, DT); const p = o.fx.find(f => f.type === 'pound'); assert.ok(p && p.dmg === 14 && p.r === 3);
  const g = mk('HAULER'); g.step({ keys: {}, mouse: {} }, G, DT);
  assert.ok(fxTypes(g.step({ keys: {}, mouse: { r: true } }, G, DT)).includes('grab'));
  g.grab(1); g.step({ keys: {}, mouse: {} }, G, DT);
  const t = g.step({ keys: {}, mouse: { l: true } }, G, DT).fx.find(f => f.type === 'throw'); assert.ok(t && t.dmg === 10 && g.held === 0);
  g.step({ keys: {}, mouse: {} }, G, DT); assert.ok(!fxTypes(g.step({ keys: {}, mouse: { l: true } }, G, DT)).includes('throw'), 'nothing held');
});

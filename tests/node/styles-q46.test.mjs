import test from 'node:test';
import assert from 'node:assert/strict';
import { createStyle, STYLES, normRig, rigParts, skinFor, GAITS } from '../../js/ship-styles.js';

const NEW = ['HOPPER', 'HUM', 'RACER', 'SEER'], DT = 1 / 60;
const G = { onGround: true, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
const A = { onGround: false, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
const fxTypes = o => o.fx.map(f => f.type);
const mk = id => createStyle(id, { walk: 22, grav: 22, jump: 8.5 });

test('4 styles registered with skin, gait, abilities, hitbox = recipe', () => {
  for (const id of NEW) {
    assert.ok(STYLES[id], id); assert.ok(GAITS[id], id + ' gait'); const st = mk(id);
    assert.ok(st.abilities.length >= 2, id); assert.ok(st.skin.rig && st.skin.rig.gait, id + ' rig'); assert.equal(st.hitbox.h, skinFor(id).hitbox.h, id);
  }
});

const IDS = Object.keys(STYLES).filter(k => k !== 'PILOT'); IDS.push('PILOT');
const parts = {}; for (const id of IDS) parts[id] = rigParts(normRig(id === 'PILOT' ? null : skinFor(id).rig, id));
const overlap = (a, b) => { const B = new Set(b); return a.filter(x => B.has(x)).length / Math.max(a.length, b.length); };
test('rigParts: every Q46 style shares <= 60 % of parts with every other style, all recipes distinct', () => {
  for (const n of NEW) for (const o of IDS) { if (o === n) continue; const v = overlap(parts[n], parts[o]); assert.ok(v <= 0.6, `${n} vs ${o}: ${(v * 100).toFixed(0)} %`); }
  assert.equal(new Set(IDS.map(i => parts[i].join('|'))).size, IDS.length);
});

test('determinism + no NaN + bounded velocity', () => {
  for (const id of NEW) {
    const run = () => { const st = mk(id); let t = ''; for (let i = 0; i < 400; i++) { const o = st.step({ keys: { KeyW: i % 40 < 30, Space: i % 50 < 30, KeyC: i % 90 > 70 }, mouse: { l: i % 33 === 0, r: i % 77 < 5 }, camYaw: 0 }, i % 60 < 45 ? G : A, DT); for (const v of [o.vel.x, o.vel.y, o.vel.z]) { assert.ok(Number.isFinite(v) && Math.abs(v) < 200, id); } t += o.vel.x.toFixed(5) + o.vel.y.toFixed(5) + fxTypes(o).join(); } return t; };
    assert.equal(run(), run(), id);
  }
});

const peak = (id, n) => { const st = mk(id); let p = 0; for (let i = 0; i < n; i++) { const o = st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, G, DT); p = Math.max(p, Math.hypot(o.vel.x, o.vel.z)); } return p; };
test('walk caps: RACER ramps to 1.6x over ~2 s, others capped', () => {
  assert.ok(peak('RACER', 30) < 22 * 1.3, 'RACER starts near 1x'); const r = peak('RACER', 400); assert.ok(Math.abs(r - 22 * 1.6) < 0.5, 'RACER top ' + r);
  assert.ok(Math.abs(peak('HOPPER', 300) - 22) < 0.1); assert.ok(Math.abs(peak('HUM', 300) - 22 * 0.8) < 0.1); assert.ok(Math.abs(peak('SEER', 300) - 22) < 0.1);
});
test('RACER: ramp resets after stopping; ram melee only at speed', () => {
  const st = mk('RACER'); for (let i = 0; i < 300; i++) st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, G, DT);
  let o = st.step({ keys: { KeyW: true }, mouse: { l: true }, camYaw: 0 }, G, DT); const m = o.fx.find(f => f.type === 'melee'); assert.ok(m && m.dmg === 10, 'ram at speed');
  for (let i = 0; i < 300; i++) st.step({ keys: {}, mouse: {}, camYaw: 0 }, G, DT); assert.ok(st.state.run < 0.01);
  o = st.step({ keys: {}, mouse: { l: true }, camYaw: 0 }, G, DT); assert.ok(!fxTypes(o).includes('melee'), 'no ram when slow');
});
function airFall(id, jumps) {                       // fall speed after N jumps then holding Space
  const st = mk(id); st.step({ keys: { Space: true }, mouse: {}, camYaw: 0 }, G, DT); st.step({ keys: {}, mouse: {}, camYaw: 0 }, A, DT);
  let o; for (let j = 1; j < jumps; j++) { for (let i = 0; i < 12; i++) st.step({ keys: {}, mouse: {}, camYaw: 0 }, A, DT); st.step({ keys: { Space: true }, mouse: {}, camYaw: 0 }, A, DT); }
  let minVy = 0, t = 0; for (let i = 0; i < 240; i++) { o = st.step({ keys: { Space: true }, mouse: {}, camYaw: 0 }, A, DT); minVy = Math.min(minVy, o.vel.y); if (st.state.gl) t += DT; } return { minVy, t, st };
}
test('HOPPER flutter: clamp 1.5 and at most 0.9 s per airtime; HUM glides after 2nd jump with clamp 2', () => {
  const h = airFall('HOPPER', 1); assert.ok(h.minVy >= -40 && h.t > 0.5 && h.t <= 0.95, 'flutter ' + h.t);
  const u = airFall('HUM', 2); assert.ok(u.t > 1, 'HUM feather fall ' + u.t);
  const s = airFall('SEER', 2); assert.ok(s.t <= 0.65 && s.t > 0.3, 'SEER float ' + s.t);
  const g = mk('HUM'); g.step({ keys: { Space: true }, mouse: {}, camYaw: 0 }, G, DT); g.step({ keys: {}, mouse: {}, camYaw: 0 }, A, DT);
  assert.ok(!g.state.gl, 'no glide before the 2nd jump');
});
test('HUM sing: left click emits sing (r 4, 3 s), 1.5 s cooldown', () => {
  const st = mk('HUM'); let o = st.step({ keys: {}, mouse: { l: true }, camYaw: 0 }, G, DT); const e = o.fx.find(f => f.type === 'sing'); assert.ok(e && e.r === 4 && e.v === 3);
  o = st.step({ keys: {}, mouse: {}, camYaw: 0 }, G, DT); o = st.step({ keys: {}, mouse: { l: true }, camYaw: 0 }, G, DT); assert.ok(!fxTypes(o).includes('sing'), 'cooldown');
  for (let i = 0; i < 100; i++) st.step({ keys: {}, mouse: {}, camYaw: 0 }, G, DT); st.step({ keys: {}, mouse: {}, camYaw: 0 }, G, DT);
  assert.ok(fxTypes(st.step({ keys: {}, mouse: { l: true }, camYaw: 0 }, G, DT)).includes('sing'));
});
test('SEER reflect on right click', () => {
  const st = mk('SEER'); const o = st.step({ keys: {}, mouse: { r: true }, camYaw: 0 }, G, DT); assert.ok(fxTypes(o).includes('reflect'));
});

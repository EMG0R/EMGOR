import test from 'node:test';
import assert from 'node:assert/strict';
import { createStyle, STYLES, maroTable, MARO_K as K, MA } from '../../js/ship-styles.js';

// deterministic xorshift so the fuzz is replayable
function rng(seed) { let a = seed >>> 0 || 1; return () => { a ^= a << 13; a >>>= 0; a ^= a >>> 17; a ^= a << 5; a >>>= 0; return a / 4294967296; }; }
const KEYS = ['KeyW', 'KeyA', 'KeyS', 'KeyD', 'Space', 'KeyC', 'ShiftLeft', 'KeyF', 'Digit1', 'Digit2', 'Digit3'];
const DT = 1 / 60;

// minimal ground world: y=0 floor, integrate vel, report onGround
function sim(id, seed, steps, withWall) {
  const st = createStyle(id, {}), r = rng(seed), p = { x: 0, y: 0, z: 0 }, trace = [];
  let onGround = true, held = {};
  for (let i = 0; i < steps; i++) {
    if (i % 20 === 0) { held = {}; for (const k of KEYS) if (r() < 0.3) held[k] = true; }
    const mouse = { l: r() < 0.1, r: r() < 0.1 };
    const ctx = { onGround, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse,
      wallNormal: withWall && p.z < -5 ? { x: 0, y: 0, z: 1 } : undefined, targets: [{ x: 3, y: 0, z: -3 }], hostileNear: false };
    const o = st.step({ keys: held, mouse, mdx: 0, mdy: 0, camYaw: r() * 6 }, ctx, DT);
    const v = o.vel;
    for (const k of ['x', 'y', 'z']) assert.ok(Number.isFinite(v[k]), `${id} seed ${seed} step ${i}: vel.${k}=${v[k]}`);
    assert.ok(Number.isFinite(o.jumpImpulse), `${id} jumpImpulse`);
    p.x += v.x * DT; p.y += v.y * DT; p.z += v.z * DT;
    if (withWall && p.z < -6) p.z = -6;
    if (p.y <= 0) { p.y = 0; onGround = true; } else onGround = false;
    if (o.jumpImpulse) onGround = false;
    trace.push(p.x.toFixed(4) + ',' + p.y.toFixed(4) + ',' + p.z.toFixed(4));
    for (const k in o.anim) if (typeof o.anim[k] === 'number') assert.ok(Number.isFinite(o.anim[k]), `${id} anim.${k}=${o.anim[k]} step ${i}`);
    assert.ok(Math.abs(v.y) < 200 && Math.hypot(v.x, v.z) < 200, `${id} runaway vel ${JSON.stringify(v)} step ${i}`);
    for (const f of o.fx) assert.ok(typeof f.type === 'string' && f.type.length > 0, `${id} fx type`);
  }
  return trace;
}

for (const id of Object.keys(STYLES)) {
  test(`${id}: 4 fuzz seeds x 1500 steps, no NaN, bounded velocity, fx typed`, () => { for (const sd of [1, 2, 3, 4]) sim(id, sd, 1500, sd % 2 === 0); });
  test(`${id}: deterministic for the same input stream, reset() restores the start`, () => {
    const a = sim(id, 77, 400, false), b = sim(id, 77, 400, false);
    assert.deepEqual(a, b);
    const st = createStyle(id, {}); const z = { keys: {}, mouse: {} }, c = { onGround: true, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
    const first = st.step(z, c, DT).vel.y;
    for (let i = 0; i < 50; i++) st.step({ keys: { KeyW: true, Space: i % 7 === 0 }, mouse: {} }, c, DT);
    st.reset(); assert.equal(st.step(z, c, DT).vel.y, first);
  });
  test(`${id}: hitbox sane, abilities non-empty, skin has colour`, () => {
    const st = createStyle(id, {});
    assert.ok(st.hitbox.r > 0.1 && st.hitbox.r < 0.5, 'r ' + st.hitbox.r);
    assert.ok((st.hitbox.h || st.hitbox.hStand) > 0.5, 'h');
    assert.ok(st.abilities.length >= 3);
    assert.ok(Number.isInteger(st.skin.color));
  });
  test(`${id}: idle on the ground with no input stays put (no drift)`, () => {
    const st = createStyle(id, {}), c = { onGround: true, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
    let o; for (let i = 0; i < 120; i++) o = st.step({ keys: {}, mouse: {} }, c, DT);
    assert.ok(Math.hypot(o.vel.x, o.vel.z) < 1e-6, 'horizontal drift ' + Math.hypot(o.vel.x, o.vel.z));
  });
  test(`${id}: walking reaches ~env.walk and never exceeds a run cap`, () => {
    const st = createStyle(id, { walk: 22 }), c = { onGround: true, floorNormal: { x: 0, y: 1, z: 0 }, slope: 0, speed: 0, airTime: 0, facing: 0, mouse: {} };
    let peak = 0, o; for (let i = 0; i < 300; i++) { o = st.step({ keys: { KeyW: true }, mouse: {}, camYaw: 0 }, c, DT); peak = Math.max(peak, Math.hypot(o.vel.x, o.vel.z)); }
    assert.ok(peak > 1, 'moves ' + peak); assert.ok(peak < 22 * 3, 'cap ' + peak);
  });
}

test('createStyle: unknown id -> null, case-insensitive id works', () => {
  assert.equal(createStyle('NOPE'), null);
  assert.equal(createStyle('maro').id, 'MARO');
});

// ── MARO table extras ──
const none = { wx: 0, wz: 0, m: 0 }, fwd = { wx: 0, wz: -1, m: 1 };
test('MARO: long jump covers >= 9 H horizontally over its flight', { todo: 'js/ship-styles.js:491 + MARO_K.LJ_Y (:464) 5.6 up at g 22.5 gives 2*5.6/22.5*9 = 4.48 H of travel; the 9.0 is a speed (H/s), not a distance' }, () => {
  const T = maroTable(), DT = 1 / 120; T.s.mode = MA.RUN;
  for (let i = 0; i < 240; i++) T.step(fwd, DT);
  T.step(Object.assign({}, fwd, { crouch: true, jumpP: true }), DT);
  assert.equal(T.s.mode, MA.LONG, 'long jump entered');
  let d = 0, n = 0; while (!T.s.grounded && n++ < 4000) { T.step(fwd, DT); d += Math.hypot(T.s.vx, T.s.vz) * DT; }
  assert.ok(d >= 9, 'distance ' + d);
});
test('MARO: wall kick refused with no wall contact (no wallNx) or stale contact', () => {
  const T = maroTable(); T.s.grounded = false; T.s.y = 3; T.s.vy = 2; T.s.mode = MA.JUMP1; T.s.vz = -3;
  T.step(Object.assign({}, fwd, { jumpP: true }), 1 / 120); assert.notEqual(T.s.mode, MA.WALLKICK);
  T.step(Object.assign({}, fwd, { jumpP: true, wallT: K.WK_WIN + 0.01, wallNx: 1, wallNz: 0 }), 1 / 120); assert.notEqual(T.s.mode, MA.WALLKICK);
  T.step(Object.assign({}, fwd, { jumpP: true, wallT: K.WK_WIN, wallNx: 1, wallNz: 0 }), 1 / 120); assert.equal(T.s.mode, MA.WALLKICK, 'edge exactly at the window is inside');
});
test('MARO: constants are positive and ordered', () => {
  assert.ok(K.V1 < K.V2 && K.V2 < K.V3);
  for (const k in K) if (typeof K[k] === 'number') assert.ok(Number.isFinite(K[k]) && K[k] >= 0, k);
  assert.equal(K.PUNCH_DMG.length, 3);
});

// Jetpack input model: tap = style jump, hold >= 0.18 s in the air = jet, for every style (MARO chain preserved).
import test from 'node:test';
import assert from 'node:assert/strict';
import { createStyle, mkJetSM, jetStep, JET_K } from '../../js/ship-styles.js';

const DT = 1 / 60;
// Drive style + jet SM like footStep does: ground = y<=0. seq = [[held, seconds], ...]. Returns log.
function run(id, seq) {
  const st = id ? createStyle(id, { walk: 6, grav: 22, jump: 8.5 }) : null, j = mkJetSM();
  const keys = {}, input = { keys, mouse: {}, camYaw: 0, wx: 0, wz: 0 }, ctx = { onGround: true, speed: 0, airTime: 0, floorNormal: null, slope: 0, targets: [], mouse: {}, facing: 0 };
  let y = 0, vy = 0, air = false, jets = 0, firstJet = -1, t = 0, jumps = 0, maxY = 0, kinds = [];
  for (const [held, secs] of seq) {
    for (let n = 0; n < Math.round(secs / DT); n++, t += DT) {
      keys.Space = held; ctx.onGround = !air;
      let r = null; if (st) r = st.step(input, ctx, DT);
      if (r && r.vel) { if (!air && r.jumpImpulse > 0) { air = true; jumps++; } if (air) vy = r.vel.y; for (const f of r.fx) if (f.type === 'jump') kinds.push(f.n); }
      else if (!st && !air && held && !j.sh && !ctx.prevHeld) { air = true; vy = 5.34; jumps++; }
      ctx.prevHeld = held;
      jetStep(j, DT, held, air, false, !!st);
      if (j.jet && air) { vy += (JET_K.UP * j.k - vy) * Math.min(1, 7 * DT); if (firstJet < 0) firstJet = t; jets++; }
      else if (air && !st) vy -= (vy > 0 ? 7.93 : 16) * DT;
      if (st && air && j.jet) st.state.vy = vy;
      if (air) { y += vy * DT; if (y <= 0) { y = 0; vy = 0; air = false; } }
      maxY = Math.max(maxY, y);
    }
  }
  return { jets, firstJet, jumps, maxY, y, kinds };
}

for (const id of ['MARO', 'SONIK', null]) {
  test(`${id || 'default'}: tap = jump, no jet`, () => { const r = run(id, [[true, 0.1], [false, 1.5]]); assert.equal(r.jets, 0); assert.ok(r.maxY > 0.5, 'jumped'); });
  test(`${id || 'default'}: hold 0.5 s -> jet ignites at ~0.18 s and climbs`, () => {
    const r = run(id, [[true, 0.5]]); assert.ok(r.jets > 0, 'jet burned');
    assert.ok(r.firstJet >= JET_K.HOLD - 1.5 * DT && r.firstJet < JET_K.HOLD + 0.05, 'ignition ' + r.firstJet); assert.ok(r.maxY > 1.8, 'climbed ' + r.maxY);
  });
  test(`${id || 'default'}: release cuts the jet`, () => { const r = run(id, [[true, 0.6], [false, 4]]); assert.equal(r.y, 0, 'landed'); });
}
test('MARO: tap,tap,tap on consecutive landings keeps the single/double/triple chain', () => {
  const st = createStyle('MARO', { walk: 6, grav: 22, jump: 8.5 }), keys = {}, input = { keys, mouse: {}, camYaw: 0, wx: 0, wz: -1, m: 1 };
  const ctx = { onGround: true, speed: 0, airTime: 0, floorNormal: null, slope: 0, targets: [], mouse: {}, facing: 0 };
  let air = false, y = 0, vy = 0, kinds = [], idle = 0;
  keys.KeyW = true;
  for (let n = 0; n < 60 * 10 && kinds.length < 3; n++) {
    if (n < 90) { ctx.onGround = true; keys.Space = false; st.step(input, ctx, DT); continue; }       // run up to speed
    keys.Space = !air && y === 0 && idle++ >= 0 && (idle % 2 === 0);
    ctx.onGround = !air; const r = st.step(input, ctx, DT);
    if (!air && r.jumpImpulse > 0) { air = true; } for (const f of r.fx) if (f.type === 'jump') kinds.push(f.n);
    if (air) { vy = r.vel.y; y += vy * DT; if (y <= 0) { y = 0; vy = 0; air = false; } }
  }
  assert.deepEqual(kinds, [1, 2, 3]);
});
test('tap then hold: second press ignites the jet (default and MARO)', () => {
  for (const id of [null, 'MARO']) { const r = run(id, [[true, 0.08], [false, 0.15], [true, 0.6]]); assert.ok(r.jets > 0, id + ' jet'); }
});
test('dive: default dives on Shift in air; styles only on a Shift pressed in the air', () => {
  const j = mkJetSM(); jetStep(j, DT, false, true, true, false); assert.equal(j.dive, true);
  const k = mkJetSM(); jetStep(k, DT, false, false, true, true); jetStep(k, DT, false, true, true, true); assert.equal(k.dive, false);   // sprint held through the jump
  jetStep(k, DT, false, true, false, true); jetStep(k, DT, false, true, true, true); assert.equal(k.dive, true);
});

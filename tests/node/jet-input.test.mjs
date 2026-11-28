// Jetpack input model: tap = jump, hold >= 0.18 s in the air = jet, Shift in the air = dive. Default (only) walker.
import test from 'node:test';
import assert from 'node:assert/strict';
import { mkJetSM, jetStep, JET_K } from '../../js/ship-styles.js';

const DT = 1 / 60;
function run(seq) {
  const j = mkJetSM();
  let y = 0, vy = 0, air = false, jets = 0, firstJet = -1, t = 0, maxY = 0, prev = false;
  for (const [held, secs] of seq) {
    for (let n = 0; n < Math.round(secs / DT); n++, t += DT) {
      if (!air && held && !prev) { air = true; vy = 5.34; }
      prev = held;
      jetStep(j, DT, held, air, false, false);
      if (j.jet && air) { vy += (JET_K.UP * j.k - vy) * Math.min(1, 7 * DT); if (firstJet < 0) firstJet = t; jets++; }
      else if (air) vy -= (vy > 0 ? 7.93 : 16) * DT;
      if (air) { y += vy * DT; if (y <= 0) { y = 0; vy = 0; air = false; } }
      maxY = Math.max(maxY, y);
    }
  }
  return { jets, firstJet, maxY, y };
}
test('tap = jump, no jet', () => { const r = run([[true, 0.1], [false, 1.5]]); assert.equal(r.jets, 0); assert.ok(r.maxY > 0.5, 'jumped'); });
test('hold 0.5 s -> jet ignites at ~0.18 s and climbs', () => {
  const r = run([[true, 0.5]]); assert.ok(r.jets > 0, 'jet burned');
  assert.ok(r.firstJet >= JET_K.HOLD - 1.5 * DT && r.firstJet < JET_K.HOLD + 0.05, 'ignition ' + r.firstJet); assert.ok(r.maxY > 1.8, 'climbed ' + r.maxY);
});
test('release cuts the jet', () => { const r = run([[true, 0.6], [false, 4]]); assert.equal(r.y, 0, 'landed'); });
test('tap then hold: second press ignites the jet', () => { const r = run([[true, 0.08], [false, 0.15], [true, 0.6]]); assert.ok(r.jets > 0); });
test('dive: Shift in the air dives', () => {
  const j = mkJetSM(); jetStep(j, DT, false, true, true, false); assert.equal(j.dive, true);
});

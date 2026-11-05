import test from 'node:test';
import assert from 'node:assert/strict';
import { inCone, segHitsBox, placementReport } from '../../js/ship-npc-access.js';

const box = (x0, z0, x1, z1) => ({ min: { x: x0, y: 0, z: z0 }, max: { x: x1, y: 5, z: z1 } });

test('cone: ahead in reach yes, behind no, very close any angle', () => {
  assert.ok(inCone(0, 0, 0, 1, 0, 4, 5.25, 60, 1.75));
  assert.ok(!inCone(0, 0, 0, 1, 0, -4, 5.25, 60, 1.75));
  assert.ok(inCone(0, 0, 0, 1, 0, -1.5, 5.25, 60, 1.75));
  assert.ok(!inCone(0, 0, 0, 1, 4, 1, 5.25, 60, 1.75));   // ~76 deg off
  assert.ok(inCone(0, 0, 0, 1, 3, 4, 5.25, 60, 1.75));    // ~37 deg
  assert.ok(!inCone(0, 0, 0, 1, 0, 9, 5.25, 60, 1.75));   // out of reach
});

test('segment vs box', () => {
  assert.ok(segHitsBox(0, 0, 10, 0, box(4, -1, 6, 1)));
  assert.ok(!segHitsBox(0, 0, 10, 0, box(4, 2, 6, 3)));
  assert.ok(!segHitsBox(0, 0, 3, 0, box(4, -1, 6, 1)));
});

test('placement: outside >= 70 %, inside-collider and blocked walks fail', () => {
  const walls = [box(10, -2, 14, 2)];
  const ok = placementReport([{ x: 3, z: 3 }, { x: -4, z: 2 }, { x: 0, z: -6 }, { x: 1, z: 1, inside: true }], walls, [0, 0]);
  assert.equal(ok.outside, 3); assert.ok(ok.pass);
  const clipped = placementReport([{ x: 12, z: 0 }], walls, [0, 0]);
  assert.deepEqual(clipped.inside, [0]); assert.ok(!clipped.pass);
  const blocked = placementReport([{ x: 16, z: 0 }], walls, [0, 0]);
  assert.deepEqual(blocked.unreachable, [0]);
  const mostlyIn = placementReport([{ x: 1, z: 1, inside: true }, { x: 2, z: 1, inside: true }, { x: 3, z: 3 }], walls, [0, 0]);
  assert.ok(mostlyIn.outsideFrac < 0.7 && !mostlyIn.pass);
  const far = placementReport([{ x: 99, z: 0 }], [], [0, 0]);
  assert.deepEqual(far.badSpawn, [0]);
  const steep = placementReport([{ x: 3, z: 0 }], [], [0, 0], { slopeAt: () => 0.5 });
  assert.deepEqual(steep.badSpawn, [0]);
});

import { pickRelocation } from '../../js/ship-npc-access.js';
test('Q49 pickRelocation: nearest ok within range, else -1', () => {
  const c = [{ x: 1, z: 0, ok: false }, { x: 0, z: 2, ok: true }, { x: 3, z: 0, ok: true }, { x: 0.5, z: 0, ok: true }];
  assert.equal(pickRelocation(0, 0, c, 4), 3);
  assert.equal(pickRelocation(0, 0, c.slice(0, 3), 4), 1);
  assert.equal(pickRelocation(0, 0, c.slice(0, 1), 4), -1);
  assert.equal(pickRelocation(0, 0, [{ x: 5, z: 0, ok: true }], 4), -1);
  assert.equal(pickRelocation(0, 0, [], 4), -1);
  assert.equal(pickRelocation(0, 0, [{ x: 2, z: 0, ok: true }, { x: 0, z: 2, ok: true }], 2), 0);   // exactly at range counts; tie keeps first
});

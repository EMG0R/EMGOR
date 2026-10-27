import test from 'node:test';
import assert from 'node:assert/strict';
import * as ITM from '../../js/ship-items.js';
import * as PKC from '../../js/pokemans-catch.js';
import * as PKG from '../../js/pokemans-gen.js';

test('gorball tiers: prices, kind, stack keys, shelf + itemFromKey', () => {
  assert.deepEqual(ITM.GORBALL_TIERS.map((b) => b.price), [15, 60, 200]);
  assert.ok(ITM.GORBALL_TIERS.every((b, i) => b.kind === 'ball' && b.tier === i + 1 && b.stackKey === 'gorball-' + (i + 1)));
  const menu = ITM.storeMenu('s1', 24, '2026-10-26');
  for (const b of ITM.GORBALL_TIERS) assert.ok(menu.some((it) => it.id === b.id), b.id + ' on the shelf table');
  for (let i = 1; i < menu.length; i++) assert.ok(menu[i - 1].price <= menu[i].price);
  assert.equal(ITM.itemFromKey('gorball-2').name, 'GREAT GOR BALL');
});

test('lurefries is a Burger House tier with the lure extra and real-time glow', () => {
  const l = ITM.FRIES_TIERS.find((f) => f.id === 'lurefries');
  assert.ok(l && l.kind === 'fries' && l.effect.extras.lure === 1 && l.effect.realtime && l.effect.glow);
  assert.equal(ITM.itemFromKey('lurefries'), l);
  assert.equal(ITM.FRIES_TIERS.length, 4);
});

test('catchChance = tier*(1-hp)*0.9+0.1, clamped; lure adds', () => {
  assert.ok(Math.abs(PKC.catchChance(1, 1) - 0.1) < 1e-9);
  assert.ok(Math.abs(PKC.catchChance(1, 0.5) - 0.55) < 1e-9);
  assert.ok(Math.abs(PKC.catchChance(1, 0) - 1) < 1e-9);
  assert.ok(Math.abs(PKC.catchChance(3, 0.8) - (3 * 0.2 * 0.9 + 0.1)) < 1e-9);
  assert.equal(PKC.catchChance(3, 0), 1);
  assert.ok(Math.abs(PKC.catchChance(2, 1, PKC.LURE_BONUS) - 0.25) < 1e-9);
  assert.ok(PKC.catchChance(2, 0.3) > PKC.catchChance(1, 0.3));
});

test('catchRoll: always 3 shakes, caught iff u < chance; empirical rate matches', () => {
  assert.equal(PKC.catchRoll(0.4, 0.39).caught, true);
  assert.equal(PKC.catchRoll(0.4, 0.4).caught, false);
  assert.equal(PKC.catchRoll(0.4, 0.9).shakes, 3);
  let c = 0; const N = 20000; for (let i = 0; i < N; i++) if (PKC.catchRoll(0.3, (i + 0.5) / N).caught) c++;
  assert.ok(Math.abs(c / N - 0.3) < 0.01);
});

test('arc: 8 dots, within 12 H reach, descending under gravity, deterministic', () => {
  const d = { x: 0, y: 0, z: -1 }, up = { x: 0, y: 1, z: 0 }, dots = Array.from({ length: 8 }, () => ({ x: 0, y: 0, z: 0 }));
  assert.equal(PKC.previewDots({ x: 0, y: 0, z: 0 }, d, up, dots), 8);
  let prev = 0;
  for (const p of dots) { const dist = Math.hypot(p.x, p.y, p.z); assert.ok(dist > prev && dist <= PKC.BALL.reach + 1e-6, 'dist ' + dist); prev = dist; }
  assert.ok(dots[7].y < dots[3].y, 'falls late in the arc');
  const T = PKC.throwTime(), end = PKC.arcPoint({ x: 0, y: 0, z: 0 }, d, up, T, { x: 0, y: 0, z: 0 });
  assert.ok(Math.hypot(end.x, end.y, end.z) <= PKC.BALL.reach + 1e-6);
  assert.ok(Math.abs(dots[7].z - end.z) < 1e-9);
});

test('makeMon carries full data; ids differ per individual', () => {
  const sp = PKG.species('planetA', 'stalker', { biome: 'lush', tier: 2 }), a = PKG.individual(sp, 's1'), b = PKG.individual(sp, 's2');
  const m = PKC.makeMon(sp, a, 5, PKG.xpForLevel(5), 'planetA');
  assert.equal(m.level, 5); assert.equal(m.hp, PKG.statsAt(sp, a, 5).hp); assert.equal(m.sp, sp); assert.equal(m.dead, false);
  assert.notEqual(m.id, PKC.makeMon(sp, b, 5, 0, 'planetA').id);
  assert.equal(PKC.PARTY_MAX, 6);
});

test('catchChance with NaN hp fraction is finite', () => {
  assert.ok(Number.isFinite(PKC.catchChance(1, NaN, 0)));
});

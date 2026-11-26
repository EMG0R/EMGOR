import test from 'node:test';
import assert from 'node:assert/strict';
import * as W from '../../js/pokemans-wild.js';
import * as G from '../../js/pokemans-gen.js';
import * as C from '../../js/pokemans-catch.js';

const mk = (lvl, plan) => { const sp = G.species('w' + (plan || 'a'), 'grazer', { biome: 'lush', tier: 1 }), ind = G.individual(sp, 1); return C.makeMon(sp, ind, lvl, G.xpForLevel(lvl), 'p'); };

test('wild HP scales with level and the blaster eats it in several shots, never to 0', () => {
  const a = W.newWild(mk(3)), b = W.newWild(mk(15));
  assert.ok(b.max > a.max && a.max >= 100 && a.max <= 260, 'max ' + a.max);
  let n = 0; while (a.hp > 1 && n < 200) { W.wildHit(a, 12.8); n++; }
  assert.ok(n >= 8 && a.hp === 1, 'shots ' + n + ' hp ' + a.hp);
  assert.equal(W.wildHit(a, 50), 0);
});
test('hit -> aggro; flee only under 10 % HP; calm after 25 s unhit', () => {
  const w = W.newWild(mk(4)); assert.equal(W.wildStep(w, 0.1, 5, 'melee', 2).mode, W.AGGRO.idle);
  W.wildHit(w, 5); assert.equal(w.aggro, true);
  assert.equal(W.wildStep(w, 0.1, 30, 'melee', 2).mode, W.AGGRO.chase);
  W.wildHit(w, w.hp - Math.floor(w.max * 0.08)); assert.equal(w.flee, true);
  assert.equal(W.wildStep(w, 0.1, 5, 'melee', 2).mode, W.AGGRO.flee);
  const v = W.newWild(mk(4)); W.wildHit(v, 5); for (let i = 0; i < 400; i++) W.wildStep(v, 0.1, 40, 'melee', 2);
  assert.equal(v.aggro, false); assert.equal(v.flee, false); assert.equal(W.wildStep(v, 0.1, 40, 'melee', 2).mode, W.AGGRO.idle);      // lost interest, ran, then stood down
});
test('attacks respect range + cooldown, ranged spits from afar', () => {
  const w = W.newWild(mk(4)); W.wildHit(w, 5); w.cd = 0;
  assert.equal(W.wildStep(w, 0.1, 50, 'melee', 3).attack, '');
  w.cd = 0; assert.equal(W.wildStep(w, 0.1, 4, 'melee', 3).attack, 'melee');
  assert.equal(W.wildStep(w, 0.1, 4, 'melee', 3).attack, '');      // cooling
  const r = W.newWild(mk(4)); W.wildHit(r, 5); r.cd = 0; assert.equal(W.wildStep(r, 0.1, 12, 'ranged', 3).attack, 'ranged');
  assert.equal(W.wildStyle('floater', 1), 'ranged'); assert.equal(W.wildStyle('grazer', 0), 'melee');
  assert.ok(W.wildPlayerDamage(10, 1, 'melee') > W.wildPlayerDamage(2, 1, 'melee'));
});
test('partner cannot KO-steal: never below 5 % HP, and does not wake a clamped wild', () => {
  const w = W.newWild(mk(5)); W.wildHit(w, 1);
  for (let i = 0; i < 500; i++) W.wildHit(w, 30, true);
  assert.ok(w.hp >= Math.ceil(w.max * 0.05) && w.hp <= Math.ceil(w.max * 0.05) + 0, 'hp ' + w.hp);
  assert.equal(W.partnerClamp(w, 99), 0);
});
test('catch curve: full HP common ~5 %, under 20 % HP 65-95 % on a basic ball, rises with tier, falls with species tier', () => {
  assert.ok(Math.abs(W.liveCatchChance(1, 1, 1) - 0.05) < 0.01);
  for (const f of [0.19, 0.1, 0.05]) { const c = W.liveCatchChance(1, f, 1); assert.ok(c > 0.62 && c < 0.95, f + ' -> ' + c); }
  assert.ok(W.liveCatchChance(3, 0.5, 1) > W.liveCatchChance(1, 0.5, 1));
  assert.ok(W.liveCatchChance(1, 0.3, 5) < W.liveCatchChance(1, 0.3, 1));
  let prev = -1; for (let h = 1; h >= 0; h -= 0.1) { const c = W.liveCatchChance(1, h, 1); assert.ok(c >= prev); prev = c; }
});
test('shakes: 1-3, resolved per shake, product matches the chance', () => {
  assert.deepEqual(W.shakeRoll(0.8, [0, 0, 0]), { shakes: 3, caught: true });
  assert.deepEqual(W.shakeRoll(0.8, [0.99, 0, 0]), { shakes: 1, caught: false });
  assert.deepEqual(W.shakeRoll(0.8, [0, 0.99, 0]), { shakes: 2, caught: false });
  let caught = 0, n = 20000, s = 1; const u = () => ((s = (s * 16807) % 2147483647) / 2147483647);
  for (let i = 0; i < n; i++) if (W.shakeRoll(0.5, [u(), u(), u()]).caught) caught++;
  assert.ok(Math.abs(caught / n - 0.5) < 0.02, 'rate ' + caught / n);
});
test('aim lock: nearest inside the 12 deg cone within reach', () => {
  const dir = { x: 0, y: 0, z: -1 }, cone = 12 * Math.PI / 180;
  const list = [{ dx: 10, dy: 0, dz: -20 }, { dx: 0, dy: 1, dz: -30 }, { dx: 0, dy: 0, dz: -50 }, { dx: 2, dy: 0, dz: -15 }];
  assert.equal(W.aimLock(list, dir, cone, 40 * 1), 3);      // 2/15 = 7.6 deg
  assert.equal(W.aimLock([list[0]], dir, cone, 40), -1);      // 26 deg
  assert.equal(W.aimLock([list[2]], dir, cone, 40), -1);      // out of reach
});
test('species variety: six plans, sizes log-normal about 1 H, ~5 % rare giants, palettes in range', () => {
  const plans = {}, hs = []; let rare = 0;
  for (let i = 0; i < 3000; i++) {
    const sp = G.species('v' + i, 'grazer'), b = sp.bodyRecipe; plans[b.plan] = (plans[b.plan] || 0) + 1; hs.push(b.h); if (b.h > 2) rare++;
    assert.ok(b.h >= 0.5 && b.h <= 3.5); assert.ok(b.pal.length === 3 && b.pal.every((x) => x >= 0 && x < G.PALETTE_SLOTS));
    const spec = G.PLAN_SPEC[b.plan]; assert.ok(b.legs >= spec.legs[0] && b.legs <= spec.legs[1]);
  }
  assert.equal(Object.keys(plans).length, 6); hs.sort((a, c) => a - c);
  assert.ok(hs[1500] > 0.85 && hs[1500] < 1.2, 'median ' + hs[1500]); assert.ok(rare / 3000 > 0.03 && rare / 3000 < 0.08, 'rare ' + rare / 3000);
  assert.equal(JSON.stringify(G.species('v7', 'grazer')), JSON.stringify(G.species('v7', 'grazer')));
});

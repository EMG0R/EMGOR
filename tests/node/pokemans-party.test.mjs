import test from 'node:test';
import assert from 'node:assert/strict';
import * as PKG from '../../js/pokemans-gen.js';
import * as PKC from '../../js/pokemans-catch.js';
import * as P from '../../js/pokemans-party.js';

const mk = (pid, lvl) => { const sp = PKG.species(pid, 'skimmer', { biome: 'lush', tier: 2 }), ind = PKG.individual(sp, pid); return PKC.makeMon(sp, ind, lvl, PKG.xpForLevel(lvl), pid); };
const prof = () => { const a = mk('a', 5), b = mk('b', 8), c = mk('c', 3); return { pokemans: [a, b, c], party: [a.id, b.id, c.id], pen: [], activeMon: '' }; };

test('active: first living, swap, dead refused', () => {
  const pf = prof(); assert.equal(P.active(pf).id, pf.party[0]);
  assert.equal(P.pickActive(pf, 1).id, pf.party[1]); assert.equal(P.active(pf).id, pf.party[1]);
  P.faint(P.monById(pf, pf.party[1])); assert.equal(P.pickActive(pf, 1), null); assert.equal(P.active(pf).id, pf.party[0]);
  assert.equal(P.pickActive(pf, 5), null); assert.equal(P.canSummon(P.monById(pf, pf.party[1])).why, 'dead');
});
test('moveDamage: scales with level, type chart multiplies, min 1', () => {
  const ch = PKG.typeChart(7), lo = mk('x', 5), hi = mk('x', 40), mv = lo.sp.moves[0];
  const foe = { def: 50, types: ['stone'] };
  assert.ok(P.moveDamage(hi, mv, foe, ch) > P.moveDamage(lo, mv, foe, ch));
  const t = PKG.TYPES.find((x) => ch.beats[mv.type] && ch.beats[mv.type].length), w = ch.beats[mv.type][0], l = ch.weak[mv.type][0];
  assert.ok(P.moveDamage(lo, mv, { def: 50, types: [w] }, ch) > P.moveDamage(lo, mv, { def: 50, types: [l] }, ch));
  assert.equal(P.moveDamage(lo, { type: 'void', power: 1, accuracy: 70 }, { def: 9999, types: [] }, ch) >= 1, true);
});
test('chooseMove: respects cooldowns, -1 when all cooling, deterministic', () => {
  const m = mk('y', 10), ch = PKG.typeChart(3), foe = { def: 40, types: ['leaf'] };
  const a = P.chooseMove(m, foe, ch, [0, 0, 0, 0], 0.3); assert.ok(a >= 0 && a < 4); assert.equal(a, P.chooseMove(m, foe, ch, [0, 0, 0, 0], 0.3));
  const cd = [0, 0, 0, 0]; cd[a] = 1; assert.notEqual(P.chooseMove(m, foe, ch, cd, 0.3), a);
  assert.equal(P.chooseMove(m, foe, ch, [1, 1, 1, 1], 0.3), -1);
});
test('moveCooldown: faster mons cool down quicker (1/spd)', () => {
  const sp = PKG.species('z', 'skimmer', { biome: 'lush', tier: 2 }), ind = PKG.individual(sp, 'z');
  const slow = PKC.makeMon({ ...sp, base: { ...sp.base, spd: 20 } }, ind, 20, 0, 'z'), fast = PKC.makeMon({ ...sp, base: { ...sp.base, spd: 120 } }, ind, 20, 0, 'z');
  assert.ok(P.moveCooldown(fast) < P.moveCooldown(slow)); assert.ok(P.moveCooldown(slow) < 2 && P.moveCooldown(fast) > 0.2);
});
test('xp: levels up, caps at 50, hp grows by the gained max, dead gains nothing', () => {
  const m = mk('q', 1); m.hp = 3; const mh0 = P.maxHp(m);
  const g = P.awardXp(m, PKG.xpForLevel(6)); assert.equal(m.level, 6); assert.equal(g, 5);
  assert.equal(m.hp, 3 + P.maxHp(m) - mh0);
  P.awardXp(m, 1e9); assert.equal(m.level, PKG.MAX_LEVEL);
  const d = mk('q2', 4); P.faint(d); assert.equal(P.awardXp(d, 1000), 0); assert.equal(d.level, 4);
  assert.ok(P.xpForKill(10, 3) > P.xpForKill(2, 1)); assert.ok(P.xpForCatch(3, 10) > P.xpForCatch(1, 2));
});
test('healing + fainting + revive: survive = full heal, faint = DEAD until revived, cost 10+4L', () => {
  const pf = prof(), [a, b] = pf.party.map((id) => P.monById(pf, id));
  a.hp = 1; assert.equal(P.damage(b, 1e6), true); assert.ok(b.dead && b.hp === 0);
  assert.equal(P.healParty(pf), 2); assert.equal(a.hp, P.maxHp(a)); assert.ok(b.dead && b.hp === 0, 'heal never revives');
  assert.equal(P.reviveCost(b), 10 + 4 * b.level);
  assert.equal(P.revive(b), true); assert.equal(b.hp, P.maxHp(b)); assert.equal(P.revive(b), false);
  assert.equal(P.damage(a, 1), false);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { FIGHTERS, FIGHTER_IDS } from '../../js/smash-fighters.js';
import { MOVE_KEYS, frameData, moveTable, BASE, THROWS } from '../../js/smash-moves.js';
import * as PX from '../../js/pokemans-fighter.js';
import { createMatch, ringStage } from '../../js/smash-engine.js';
import { SAMPLES, mk, run, B } from './smash-arena.mjs';

test('every style has all 25 moves with sane frame data (startup 3-25, active >= 1, endlag <= 45)', () => {
  assert.equal(MOVE_KEYS.length, 25);
  for (const id of FIGHTER_IDS) {
    const d = FIGHTERS[id];
    for (const k of MOVE_KEYS) {
      const m = d.moves[k]; assert.ok(m, id + '.' + k + ' missing');
      const fd = frameData(m);
      assert.ok(fd.s >= 3 && fd.s <= 25, id + '.' + k + ' startup ' + fd.s);
      assert.ok(fd.a >= 1, id + '.' + k + ' active ' + fd.a);
      assert.ok(fd.e <= 45, id + '.' + k + ' endlag ' + fd.e);
    }
  }
});
test('jab chain is 3 hits and totals 8-12 damage for every style', () => {
  for (const id of FIGHTER_IDS) {
    const mv = FIGHTERS[id].moves; assert.equal(mv.jab1.next, 'jab2'); assert.equal(mv.jab2.next, 'jab3'); assert.ok(!mv.jab3.next);
    const tot = ['jab1', 'jab2', 'jab3'].reduce((a, k) => a + mv[k].hits[0].dmg, 0);
    assert.ok(tot >= 8 && tot <= 12, id + ' jab chain ' + tot);
  }
});
test('smashes charge (hold frame set), aerials have landing lag, throws exist in 4 directions', () => {
  for (const id of FIGHTER_IDS) {
    const d = FIGHTERS[id], mv = d.moves;
    for (const k of ['fsmash', 'usmash', 'dsmash']) assert.equal(mv[k].charge, mv[k].hits[0].s - 1, id + '.' + k);
    for (const k of ['nair', 'fair', 'bair', 'uair', 'dair']) { assert.ok(mv[k].air && mv[k].landLag >= 3, id + '.' + k + ' landLag'); }
    assert.ok(d.throws.f && d.throws.b && d.throws.u && d.throws.d);
    assert.ok(mv.grab.grab.reach > 0 && mv.pummel.pummel.dmg > 0);
    for (const k of ['fsmash', 'usmash', 'dsmash']) assert.ok(mv[k].hits[0].dmg > mv.ftilt.hits[0].dmg, id + ' smash > tilt');
  }
});
test('the four named styles carry their identity specials', () => {
  const N = (id) => ['bN', 'bF', 'bU', 'bD'].map((k) => FIGHTERS[id].moves[k].name);
  assert.deepEqual(N('pilot'), ['plasma', 'cape', 'boost', 'tornado']);       // MARO: fireball / cape reflect / super jump punch / tornado
  assert.deepEqual(N('swift'), ['homing', 'spindash', 'spring', 'spincharge']);
  assert.deepEqual(N('hopper'), ['eggthrow', 'eggroll', 'flutterup', 'groundpound']);
  assert.deepEqual(N('builder'), ['place', 'minecart', 'elytra', 'tnt']);
  assert.ok(FIGHTERS.pilot.moves.bF.tick, 'cape reflects'); 
});
test('stronger styles hit harder and slower (data profile)', () => {
  const t = (id) => moveTable(FIGHTERS[id]);
  assert.ok(t('hauler').fsmash.dmg > t('pilot').fsmash.dmg && t('pilot').fsmash.dmg > t('sparklet').fsmash.dmg);
  assert.ok(t('hauler').fsmash.s > t('pilot').fsmash.s && t('pilot').fsmash.s > t('swift').fsmash.s);
});
test('creature fighters load against the new table (missing moves filled, own moves kept)', () => {
  const mon = PX.trainerTeam('p1', 'a', 'elder', 3)[0], spec = PX.creatureFighter(mon);
  for (const k of MOVE_KEYS) assert.ok(spec.def.moves[k], 'creature ' + k);
  assert.ok(spec.def.moves.ftilt.mon && spec.def.moves.bN.mon);
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net' }, { fid: spec.fid, def: spec.def, ctrl: 'cpu', tier: 2 }], stageDef: ringStage(SAMPLES, 'cr'), seed: 3, stocks: 1 });
  for (let i = 0; i < 900; i++) S.step([0, 0]);
  assert.ok(S.frame > 800);
});
test('data-driven: every normal comes from BASE rows (no per-move code needed)', () => {
  for (const k of Object.keys(BASE)) assert.ok(MOVE_KEYS.includes(k), k);
  assert.deepEqual(Object.keys(THROWS).sort(), ['b', 'd', 'f', 'u']);
});
test('a fighter can run each move end to end (no throw, move ends, returns to a free state)', () => {
  for (const id of FIGHTER_IDS) for (const k of MOVE_KEYS) {
    if (k === 'grab' || k === 'pummel' || k.endsWith('throw')) continue;
    const S = mk({ fid: id }, { fid: 'pilot' }); const f = S.fighters[0]; S.fighters[1].x = 90; f.x = 0;
    const mv = f.def.moves[k]; if (!mv) continue;
    f.grounded = !mv.air; if (mv.air) { f.y = -200; f.state = 'air'; f.vy = 0; }
    f.cd = {}; f.upUsed = false; f.res = 12;
    f.move = mv; f.mf = 0; f.state = 'attack'; f.moveAir = !f.grounded; f.hitSet = 0;
    let n = 0; for (; n < 260 && f.state === 'attack'; n++) { try { S.step([0, 0]); } catch (e) { assert.fail(id + '.' + k + ' threw ' + e.message); } }
    assert.ok(n < 260, id + '.' + k + ' never ended (' + f.state + ')');
  }
});

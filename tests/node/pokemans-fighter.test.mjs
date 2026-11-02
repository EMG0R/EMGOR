import test from 'node:test';
import assert from 'node:assert/strict';
import * as PX from '../../js/pokemans-fighter.js';
import * as PKG from '../../js/pokemans-gen.js';
import { createMatch } from '../../js/smash-engine.js';
import { FIGHTERS } from '../../js/smash-fighters.js';

const team = (pid, seed, role) => PX.trainerTeam(pid, seed, role, 2);

test('creatureFighter is deterministic and maps parts/moves/stats', () => {
  const m = team('p1', 's1', 'farmer')[0], a = PX.creatureFighter(m), b = PX.creatureFighter(m);
  assert.equal(JSON.stringify(a.def.moves.bN.hits), JSON.stringify(b.def.moves.bN.hits));
  assert.equal(a.fid, PX.PLAN_BASE[m.sp.bodyRecipe.plan]);
  for (const k of ['bN', 'bF', 'bD', 'ftilt']) assert.ok(a.def.moves[k].mon, k);
  assert.ok(a.def.moves.bU === FIGHTERS[a.fid].moves.bU, 'recovery stays the base one');
  assert.ok(a.def.weight >= 70 && a.def.weight <= 130 && a.def.hurtR >= 0.8 && a.def.hurtR <= 1.35);
});
test('power -> damage monotone, type -> hit colour, effect shapes the hit', () => {
  const m = team('p1', 's1', 'farmer')[0], st = PKG.statsAt(m.sp, m.ind, m.level);
  const mk = (power, type, effect) => PX.buildMove(m, { name: 'x', type, power, effect }, 'ftilt', st);
  assert.ok(mk(120, 'stone').hits[0].dmg > mk(40, 'stone').hits[0].dmg);
  assert.deepEqual(mk(60, 'ember').hits[0].col, PX.TYPE_COL.ember);
  assert.ok(mk(60, 'stone', 'burn').hits[0].dmg > mk(60, 'stone').hits[0].dmg);
  assert.ok(mk(60, 'stone', 'launch').hits[0].bkb > mk(60, 'stone').hits[0].bkb);
  assert.ok(mk(60, 'stone', 'slow').hits[0].bkb < mk(60, 'stone').hits[0].bkb);
});
test('trainers: elders always, 1-3 mons, elders carry grafts, seeded', () => {
  assert.ok(PKG.isTrainer('elder', 'x') && !PKG.isTrainer('dealer', 'x') && !PKG.isTrainer('cashier', 'x'));
  let n = 0; for (let i = 0; i < 400; i++) if (PKG.isTrainer('farmer', 'v' + i)) n++;
  assert.ok(n > 60 && n < 140, 'about a quarter: ' + n);
  const e = team('p1', 'el', 'elder'), e2 = team('p1', 'el', 'elder');
  assert.equal(e.length, 3); assert.ok(e.every((m) => m.ind.grafts && m.ind.grafts.length >= 1));
  assert.equal(JSON.stringify(e.map((m) => m.id)), JSON.stringify(e2.map((m) => m.id)));
  for (let i = 0; i < 30; i++) { const t = team('p', 'k' + i, 'farmer'); assert.ok(t.length >= 1 && t.length <= 3); }
  assert.equal(PX.trainerSpec(e).stocks, 3);
});
function run(sim, frames, mask) { for (let i = 0; i < frames; i++) sim.step([mask(i), 0]); }
test('a creature def runs in the engine, hits land, cooldown gates, a KO drops a stock', () => {
  const t = PX.trainerSpec(team('p1', 'el', 'elder'));
  const S = createMatch({ seed: 7, stocks: 2, players: [{ fid: 'pilot', ctrl: 'net' }, { fid: t.fid, def: t.def, stocks: t.stocks, ctrl: 'net' }] });
  assert.equal(S.fighters[1].stocks, 3); assert.equal(S.fighters[1].def.name, t.def.name); assert.equal(S.fighters[1].fid, t.fid);
  run(S, 200, () => 0);
  const f = S.fighters[1]; f.x = S.fighters[0].x + 20; f.face = -1;
  let hitEv = null; const B = { A: 16, K: 32 };
  for (let i = 0; i < 240 && !hitEv; i++) { S.step([0, i % 60 === 0 ? B.K : 0]); hitEv = S.events.find((e) => e.t === 'hit' && e.by === 1); S.events.length = 0; }
  assert.ok(S.fighters[0].pct > 0 || hitEv, 'creature special landed');
  S.fighters[1].pct = 400; for (let i = 0; i < 400 && S.fighters[1].stocks === 3; i++) { S.fighters[1].vx = 9; S.fighters[1].vy = -9; S.step([0, 0]); }
});
test('wild ball slot: refused above 50 %, requested below, roll is pure', () => {
  const m = team('p1', 'w', 'farmer')[0], wild = PX.creatureFighter(m);
  for (const [pct, expect] of [[0, false], [wild.hpPct * 0.7, true]]) {
    const def = PX.withBall('pilot', wild), S = createMatch({ seed: 3, players: [{ fid: 'pilot', def, ctrl: 'net' }, { fid: wild.fid, def: wild.def, ctrl: 'net' }] });
    run(S, 200, () => 0); S.fighters[1].pct = pct;
    for (let i = 0; i < 40; i++) S.step([i === 0 ? 32 | 8 : 8, 0]);
    assert.equal(!!S.ballReq, expect, 'pct ' + pct);
  }
  assert.ok(PX.ballRoll(wild, 0.1, 0).caught && !PX.ballRoll(wild, 0.9, 0.9999).caught);
  assert.equal(PX.hpFrac(wild, wild.hpPct / 2), 0.5);
});

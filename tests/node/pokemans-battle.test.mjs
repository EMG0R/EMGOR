import test from 'node:test';
import assert from 'node:assert/strict';
import * as B from '../../js/pokemans-battle.js';
import * as PKC from '../../js/pokemans-catch.js';
import * as PKG from '../../js/pokemans-gen.js';

const sp = PKG.species('planetB', 'grazer', { biome: 'lush', tier: 2 }), ind = PKG.individual(sp, 'x');
const chart = PKG.typeChart(7);
const mk = (lv) => PKC.makeMon(sp, ind, lv, PKG.xpForLevel(lv), 'planetB');
const seq = (...v) => { let i = 0; return () => v[i++ % v.length]; };

test('battleCatchChance = floor x (1 - hp/max x 0.8), per ball tier', () => {
  const c = PKC.battleCatchChance;
  assert.ok(Math.abs(c(1, 100, 100) - 0.35 * 0.2) < 1e-9);
  assert.ok(Math.abs(c(2, 50, 100) - 0.45 * 0.6) < 1e-9);
  assert.ok(Math.abs(c(3, 0, 100) - 0.55) < 1e-9);
  assert.ok(c(1, 10, 100) > c(1, 90, 100) && c(3, 40, 100) > c(1, 40, 100));
  assert.ok(Math.abs(c(1, 100, 100, PKC.LURE_BONUS) - (0.07 + 0.15)) < 1e-9);
  assert.ok(c(3, 0, 100, 9) <= 1 && Number.isFinite(c(1, 5, 0)));
});

test('damage: Gen-1 formula, roll 0.85..1.0, crit x2 at 1/16, STAB 1.5, type chart', () => {
  const att = { level: 10, types: ['leaf'], st: { atk: 40, sp: 50, def: 30, spd: 30, hp: 50 } }, def = { level: 10, types: ['stone'], st: { atk: 30, sp: 30, def: 35, spd: 30, hp: 50 } };
  const mv = { name: 'x', type: 'leaf', power: 60, accuracy: 100 }, mvN = { name: 'y', type: 'void', power: 60, accuracy: 100 };
  const base = Math.floor(Math.floor((2 * 10 / 5 + 2) * 60 * 50 / 35) / 50) + 2, m = chart.mult('leaf', 'stone');
  const lo = B.calcDamage(att, mv, def, 0, 0.99, chart), hi = B.calcDamage(att, mv, def, 0.999999, 0.99, chart);
  assert.equal(lo.dmg, Math.max(1, Math.floor(base * 1.5 * m * 0.85)));
  assert.equal(hi.dmg, Math.max(1, Math.floor(base * 1.5 * m * 0.999999)));
  assert.ok(hi.dmg >= lo.dmg && lo.stab && !lo.crit);
  const cr = B.calcDamage(att, mv, def, 0.5, 1 / 16 - 1e-9, chart), nc = B.calcDamage(att, mv, def, 0.5, 1 / 16, chart);
  assert.ok(cr.crit && !nc.crit && cr.dmg >= nc.dmg * 2 - 2);
  const ns = B.calcDamage(att, mvN, def, 0.5, 0.99, chart); assert.ok(!ns.stab);
  assert.ok(B.calcDamage(att, mv, { level: 1, types: [], st: { def: 9999 } }, 0, 0.9).dmg >= 1);
});

test('turn order: speed decides, tie by roll; run odds reduced when the wild is faster', () => {
  assert.equal(B.turnOrder(50, 40, 0.9), 'me'); assert.equal(B.turnOrder(30, 40, 0.1), 'foe');
  assert.equal(B.turnOrder(40, 40, 0.1), 'me'); assert.equal(B.turnOrder(40, 40, 0.9), 'foe');
  assert.equal(B.runChance(50, 40, 0), 1);
  const r0 = B.runChance(20, 40, 0); assert.ok(r0 < 1 && r0 >= 0.1); assert.ok(B.runChance(20, 40, 3) > r0);
});

test('core: faster side hits first; a KO ends the battle before the slower side acts', () => {
  const me = B.youFighter(), foe = B.fighterOf(mk(3)); foe.hp = 1; foe.st.spd = 1; me.st.spd = 99;
  const c = B.createBattleCore({ mine: me, wild: foe, chart, rng: seq(0.1, 0.5, 0.5, 0.5) }), ev = c.act({ k: 'fight', i: 0 });
  assert.equal(c.result, 'win'); assert.ok(ev.some((e) => e.k === 'msg' && /fainted/.test(e.s)));
  assert.ok(!ev.some((e) => e.k === 'msg' && /^Wild .* used/.test(e.s)));
  const me2 = B.youFighter(), foe2 = B.fighterOf(mk(3)); me2.st.spd = 1; foe2.st.spd = 99;
  const c2 = B.createBattleCore({ mine: me2, wild: foe2, chart, rng: seq(0.1, 0.5, 0.5, 0.5) }), ev2 = c2.act({ k: 'fight', i: 0 });
  assert.match(ev2.find((e) => e.k === 'msg').s, /^Wild /);
});

test('core: ball odds on the option, forced catch / break-free, shakes 1..3', () => {
  let n = 2; const balls = { tier: () => (n > 0 ? 1 : 0), use: () => { n--; }, n: () => n };
  const foe = B.fighterOf(mk(5)); foe.hp = Math.floor(foe.max * 0.3);
  const c = B.createBattleCore({ mine: B.youFighter(), wild: foe, chart, rng: seq(0.5), catchU: () => 0.99, balls });
  assert.equal(c.ballOdds().pct, Math.round(PKC.battleCatchChance(1, foe.hp, foe.max) * 100));
  const e1 = c.act({ k: 'ball' }); const b1 = e1.find((e) => e.k === 'ball');
  assert.ok(!b1.caught && b1.shakes >= 1 && b1.shakes <= 3 && !c.over && n === 1);
  const c2 = B.createBattleCore({ mine: B.youFighter(), wild: foe, chart, rng: seq(0.5), catchU: () => 0, balls });
  const e2 = c2.act({ k: 'ball' }); assert.equal(c2.result, 'caught'); assert.equal(e2.find((e) => e.k === 'ball').shakes, 3); assert.equal(n, 0);
  assert.equal(c2.act({ k: 'ball' }).length, 0);
  const c3 = B.createBattleCore({ mine: B.youFighter(), wild: foe, chart, rng: seq(0.5), balls }); assert.equal(c3.ballOdds().tier, 0);
});

test('core: wild flees after 3 unanswered turns; RUN works at 100 % when faster', () => {
  const me = B.youFighter(), foe = B.fighterOf(mk(3)); me.st.spd = 1; foe.st.spd = 1; foe.moves.forEach((m) => { m.accuracy = 0; });
  const c = B.createBattleCore({ mine: me, wild: foe, chart, rng: seq(0.3), balls: { tier: () => 0, use() {}, n: () => 0 } });
  c.act({ k: 'ball' }); assert.equal(c.idle, 0);      // no ball: no turn spent
  me.moves.forEach((m) => { m.accuracy = 0; });
  c.act({ k: 'fight', i: 0 }); c.act({ k: 'fight', i: 0 }); assert.ok(!c.over); c.act({ k: 'fight', i: 0 }); assert.equal(c.result, 'fled');
  const fast = B.youFighter(); fast.st.spd = 99; const c2 = B.createBattleCore({ mine: fast, wild: B.fighterOf(mk(3)), chart, rng: seq(0.99) });
  c2.act({ k: 'run' }); assert.equal(c2.result, 'run');
});

test('win levels the party fighter: +1 level, HP +8 %, power +4 %, cap 30', () => {
  const m = mk(5), h0 = B.battleStats(m).hp, a0 = B.battleStats(m).atk; assert.ok(B.levelUp(m)); assert.equal(m.level, 6); assert.equal(m.bl, 1);
  const raw = PKG.statsAt(sp, ind, 6); assert.equal(B.battleStats(m).hp, Math.round(raw.hp * 1.08)); assert.equal(B.battleStats(m).atk, Math.round(raw.atk * 1.04)); assert.ok(B.battleStats(m).hp > h0 && B.battleStats(m).atk >= a0);
  const t = mk(30); assert.ok(!B.levelUp(t)); assert.equal(t.level, 30);
  const me = B.fighterOf(m), foe = B.fighterOf(mk(2)); foe.hp = 1; me.st.spd = 99;
  const c = B.createBattleCore({ mine: me, wild: foe, chart, rng: seq(0.1, 0.5, 0.9) }); c.act({ k: 'fight', i: 0 }); assert.equal(c.result, 'win'); assert.equal(m.level, 7);
});

test('module stays DOM-free at import and the ring KO catch is gone', () => {
  assert.equal(PKC.KO_WINDOW, undefined); assert.equal(PKC.koOpen, undefined); assert.equal(PKC.ringCatchChance, undefined);
});

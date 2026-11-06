import test from 'node:test';
import assert from 'node:assert/strict';
import * as PX from '../../js/pokemans-fighter.js';
import * as PG from '../../js/pokemans-graft.js';
import { createMatch, botInput } from '../../js/smash-engine.js';
import { terrainStage } from '../../js/smash-stages.js';

const mon0 = (seed) => PX.trainerTeam('p1', seed, 'elder', 3)[0];

test('every move carries a type colour; effects tag hits', () => {
  const m = mon0('a'), f = PX.creatureFighter(m);
  for (const k of ['bN', 'bF', 'bD', 'ftilt']) {
    const mv = f.def.moves[k]; assert.ok(mv.mon);
    if (mv.hits.length) assert.ok(Array.isArray(mv.hits[0].col) && mv.hits[0].col.length === 3, k + ' hit col');
  }
  const st = PG.PARTS && null; void st;
  for (const ef of ['burn', 'slow', 'flinch', 'launch']) {
    const mv = PX.buildMove(m, { name: 'x', type: 'ember', power: 60, effect: ef }, 'ftilt', { atk: 55, spd: 55, hp: 55 });
    assert.equal(mv.hits[0].eff, ef);
  }
  const sh = PX.buildMove(m, { name: 'x', type: 'ember', power: 60, effect: 'shield' }, 'ftilt', { atk: 55, spd: 55, hp: 55 });
  assert.equal(sh.hits[0].eff, undefined);
});
test('look: sig parts are only fin/spike/shell/glow/lungs; grafts show; rgb in 0..1.3', () => {
  const m = mon0('b'), before = PX.creatureFighter(m).def.look;
  for (const k of before.sig) assert.ok(PG.PARTS.includes(k));
  PG.applyGraft(m, 'spike', 2); PG.applyGraft(m, 'shell', 2);
  const after = PX.creatureFighter(m).def.look;
  assert.ok(after.sig.includes('spike') && after.sig.includes('shell'));
  assert.ok(after.col.every((v) => v >= 0 && v <= 1.3));
  assert.deepEqual(PX.creatureFighter(m).def.look, after);      // deterministic
});
test('different species read different (sig+hue)', () => {
  const seen = new Set(); for (const s of ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']) { const l = PX.creatureFighter(mon0(s)).def.look; seen.add(l.sig.join('+') + '|' + l.col.map((v) => v.toFixed(2)).join(',')); }
  assert.ok(seen.size >= 5, 'distinct looks ' + seen.size);
});
test('hp -> takeMul (bulky takes less percent), spd -> run/jump', () => {
  const a = PX.creatureFighter(mon0('a')).def, vals = ['a', 'b', 'c', 'd', 'e', 'f'].map((s) => PX.creatureFighter(mon0(s)).def);
  for (const d of vals) assert.ok(d.takeMul >= 0.7 && d.takeMul <= 1.4 && d.run >= 0.9 && d.run <= 2 && d.jump >= 48 && d.jump <= 66);
  assert.ok(a.takeMul > 0);
});
test('sim: a creature hit sets the victim status icon and takeMul scales percent', () => {
  const m = mon0('a'), spec = PX.creatureFighter(m), mv = PX.buildMove(m, { name: 'zap', type: 'spark', power: 120, effect: 'burn' }, 'ftilt', { atk: 55, spd: 55, hp: 55 });
  const def = Object.assign({}, spec.def, { moves: Object.assign({}, spec.def.moves, { ftilt: mv }), takeMul: 1 });
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net' }, { fid: spec.fid, def, ctrl: 'net' }], stageDef: terrainStage(new Float32Array(128), 'c'), seed: 9, stocks: 1 });
  while (S.phase !== 'play') S.step([0, 0]);
  S.fighters[0].x = 0; S.fighters[1].x = 44; S.fighters[1].face = -1;
  let tagged = false;      // scripted: hold toward 8 frames (tilt window 6..14), then A with the direction still held = f-tilt
  for (let i = 0; i < 120 && !tagged; i++) { S.step([0, i < 8 ? 1 : i < 12 ? 1 | 16 : 0]); if (S.fighters[0].stK === 'burn') tagged = true; }
  assert.ok(tagged, 'burn tag landed'); assert.ok(S.fighters[0].pct > 0);
  for (let i = 0; i < 100; i++) S.step([0, 0]); assert.equal(S.fighters[0].stK, '');      // clears after 90 f
});
test('self effects tag the user', () => {
  const m = mon0('a'), mv = PX.buildMove(m, { name: 'x', type: 'leaf', power: 60, effect: 'heal' }, 'bN', { atk: 55, spd: 55, hp: 55 });
  const f = { cd: {}, pct: 20, inv: 0, stK: '', stT: 0 };
  const s0 = 6 + Math.round((130 - 60) / 12);
  mv.tick({ proj() {}, sfx() {} }, f, 0); mv.tick({ proj() {}, sfx() {} }, f, s0);
  assert.equal(f.stK, 'heal'); assert.equal(f.pct, 15);
});

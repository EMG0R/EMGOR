import test from 'node:test';
import assert from 'node:assert/strict';
import { species, individual, typeChart, xpToLevel, xpForLevel, statsAt, TYPES, STATS, EFFECTS } from '../../js/pokemans-gen.js';

test('determinism: species, individual, chart', () => {
  assert.deepEqual(species(1234, 'skimmer'), species(1234, 'skimmer'));
  assert.notDeepEqual(species(1234, 'skimmer'), species(1235, 'skimmer'));
  const sp = species(9, 'x'); assert.deepEqual(individual(sp, 5), individual(sp, 5));
  const a = typeChart(77), b = typeChart(77); assert.deepEqual([a.beats, a.weak], [b.beats, b.weak]);
});
test('ranges over 300 species', () => {
  for (let i = 0; i < 300; i++) {
    const sp = species(i * 7919, 'k' + (i % 5));
    for (const s of STATS) assert.ok(sp.base[s] >= 20 && sp.base[s] <= 120, s);
    assert.equal(sp.moves.length, 4); assert.equal(sp.types.length, 2); assert.notEqual(sp.types[0], sp.types[1]);
    assert.ok(sp.types.every((t) => TYPES.includes(t))); assert.ok(sp.bodyRecipe.sig.length <= 2);
    for (const m of sp.moves) { assert.ok(m.power >= 30 && m.power <= 120); assert.ok(m.accuracy >= 70 && m.accuracy <= 100); assert.ok(m.effect === null || EFFECTS.includes(m.effect)); assert.ok(TYPES.includes(m.type)); }
    const ind = individual(sp, i);
    assert.ok(ind.size >= 0.8 && ind.size <= 1.3); for (const s of STATS) assert.ok(Math.abs(ind.iv[s]) <= 0.15);
  }
});
test('biome + family drive types', () => {
  const sp = species(5, 'a', { biome: 'lava', family: 'cozy' }); assert.equal(sp.types[0], 'ember');
  assert.notEqual(sp.types[1], 'ember');
  assert.equal(species(5, 'a', { biome: 'icy', family: 'stim' }).types.join(), 'frost,spark');
});
test('shiny rate ~1/256', () => {
  const sp = species(1, 'a'); let n = 0; const N = 51200;
  for (let i = 0; i < N; i++) if (individual(sp, i).shiny) n++;
  assert.ok(n > 120 && n < 290, 'shiny ' + n);
});
test('type chart: 8 types, each beats 2 / weak to 2, symmetric, no self', () => {
  for (const u of [0, 1, 42, 99999, 123456789]) {
    const c = typeChart(u); assert.equal(c.types.length, 8);
    for (const t of TYPES) {
      assert.equal(c.beats[t].length, 2); assert.equal(c.weak[t].length, 2); assert.equal(new Set(c.beats[t]).size, 2);
      assert.ok(!c.beats[t].includes(t) && !c.weak[t].includes(t));
      for (const d of c.beats[t]) { assert.ok(c.weak[d].includes(t)); assert.equal(c.mult(t, d), 2); assert.equal(c.mult(d, t), 0.5); }
      for (const a of c.weak[t]) assert.ok(c.beats[a].includes(t));
      for (const d of c.beats[t]) assert.ok(!c.beats[d].includes(t), 'no mutual beats');
    }
  }
});
test('xp curve and stats', () => {
  assert.equal(xpToLevel(0), 1); assert.equal(xpToLevel(xpForLevel(50)), 50); assert.equal(xpToLevel(1e9), 50);
  for (let l = 2; l <= 50; l++) { assert.equal(xpToLevel(xpForLevel(l)), l); assert.equal(xpToLevel(xpForLevel(l) - 1), l - 1); }
  const sp = species(3, 'z'), ind = individual(sp, 1), a = statsAt(sp, ind, 5), b = statsAt(sp, ind, 50);
  for (const s of STATS) assert.ok(b[s] > a[s]);
});
test('name uniqueness over 500 species >= 98 %', () => {
  const names = new Set(); for (let i = 0; i < 500; i++) names.add(species(i * 104729 + 13, 'k' + (i % 9)).name);
  assert.ok(names.size >= 490, 'unique ' + names.size);
});

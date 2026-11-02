import test from 'node:test';
import assert from 'node:assert/strict';
import * as DX from '../../js/pokemans-dex.js';
import * as G from '../../js/pokemans-gen.js';
import * as C from '../../js/pokemans-catch.js';

const sp = (p, k, t) => G.species(p, k, { biome: 'lush', tier: t || 2 });
const mkMon = (s, lvl, pid) => C.makeMon(s, G.individual(s, 'x:' + pid), lvl, G.xpForLevel(lvl), pid);

test('note: new once, keeps first planet, compact record', () => {
  const pf = {}, s = sp('P1', 'dog');
  assert.equal(DX.note(pf, s, 'P1'), true);
  assert.equal(DX.note(pf, s, 'P2'), false);
  assert.equal(pf.dex.seen[s.id].planet, 'P1');
  assert.equal(pf.dex.seen[s.id].moves.length, 4);
  JSON.stringify(pf);
});

test('entries: seen vs caught, counts, best level, grafts, caught implies seen', () => {
  const s1 = sp('P1', 'dog'), s2 = sp('P2', 'cat', 3), pf = { pokemans: [] };
  DX.note(pf, s1, 'P1'); DX.note(pf, s2, 'P2');
  const m1 = mkMon(s1, 5, 'P1'), m2 = mkMon(s1, 9, 'P1'); m2.id += '#1'; m2.ind.grafts = [{ p: 'fin', t: 1 }, { p: 'fin', t: 2 }];
  pf.pokemans.push(m1, m2);
  const s3 = sp('P3', 'bird'); pf.pokemans.push(mkMon(s3, 3, 'P3'));
  const e = DX.entries(pf), by = Object.fromEntries(e.map((x) => [x.id, x]));
  assert.equal(e.length, 3);
  assert.equal(by[s1.id].caught, true); assert.equal(by[s1.id].count, 2); assert.equal(by[s1.id].best, 9); assert.equal(by[s1.id].grafts.fin, 2);
  assert.equal(by[s2.id].caught, false); assert.equal(by[s2.id].count, 0);
  assert.equal(by[s3.id].caught, true);
  const g = DX.groups(e); assert.equal(g.reduce((n, x) => n + x.list.length, 0), 3);
  for (let i = 1; i < e.length; i++) assert.ok(e[i - 1].planet <= e[i].planet);
});

test('partyRow: dead is 0 hp, frac, bond tier, active flag', () => {
  const s = sp('P1', 'dog'), m = mkMon(s, 4, 'P1'), pf = { activeMon: m.id };
  const mx = G.statsAt(s, m.ind, 4).hp;
  m.hp = mx - 3; let r = DX.partyRow(pf, m, mx);
  assert.equal(r.hp, mx - 3); assert.ok(r.frac < 1 && r.frac > 0); assert.equal(r.active, true); assert.equal(r.dead, false);
  m.dead = true; r = DX.partyRow(pf, m, mx); assert.equal(r.hp, 0); assert.equal(r.dead, true);
  m.bond = 12; assert.equal(DX.partyRow(pf, m, mx).bondTier, 1);
});

test('iconGrid: deterministic, 256 cells, non-empty, outlined, every plan + sig', () => {
  const plans = ['skimmer', 'crawler', 'floater', 'strider', 'burrower', 'swarmer'];
  for (const plan of plans) {
    const rec = { plan, seed: 12345, segs: 5, limbs: 4, hue: 100, sig: ['fin', 'spike', 'shell', 'glow', 'lungs'] };
    const a = DX.iconGrid(rec), b = DX.iconGrid(rec);
    assert.equal(a.length, 256); assert.deepEqual(a, b);
    assert.ok(a.filter((c) => c !== '.').length > 20, plan);
    assert.ok(a.includes('O'), plan);
    assert.ok(a.every((c) => '.ObdlhefscgP'.toLowerCase().includes(c.toLowerCase())), plan);
  }
  const x = DX.iconGrid({ plan: 'crawler', seed: 1, segs: 4, limbs: 4, hue: 0, sig: [] }), y = DX.iconGrid({ plan: 'crawler', seed: 1, segs: 4, limbs: 4, hue: 0, sig: ['fin'] });
  assert.notDeepEqual(x, y);
});

test('no Math.random in the module', async () => {
  const fs = await import('node:fs');
  assert.ok(!/Math\.random/.test(fs.readFileSync(new URL('../../js/pokemans-dex.js', import.meta.url), 'utf8').replace(/\/\/.*$/gm, '')));
});

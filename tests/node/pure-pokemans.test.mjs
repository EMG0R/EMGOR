import test from 'node:test';
import assert from 'node:assert/strict';
import * as G from '../../js/pokemans-gen.js';
import * as P from '../../js/pokemans-party.js';
import * as C from '../../js/pokemans-catch.js';

const mkPf = (...mons) => ({ pokemans: mons, party: mons.map((m) => m.id), pen: [], gor: 0 });
const mkMon = (seed, lvl = 10, tier) => { const sp = G.species(seed, 'creature', tier ? { tier } : {}), ind = G.individual(sp, seed); return C.makeMon(sp, ind, lvl, G.xpForLevel(lvl), 'P'); };

test('species: stats in 20..120, 4 distinct moves, 2 types, tier 1..5, no NaN, 1000 seeds', () => {
  for (let i = 0; i < 1000; i++) {
    const sp = G.species('seed' + i, 'k' + (i % 7));
    assert.equal(sp.moves.length, 4); assert.equal(sp.types.length, 2);
    assert.ok(sp.tier >= 1 && sp.tier <= 5);
    for (const s of G.STATS) assert.ok(sp.base[s] >= 20 && sp.base[s] <= 120 && Number.isInteger(sp.base[s]), `${sp.id} ${s}`);
    assert.equal(sp.moves[0].type, sp.types[0], 'move 0 is STAB 1'); assert.equal(sp.moves[1].type, sp.types[1], 'move 1 is STAB 2');
    assert.equal(new Set(sp.moves.map((m) => m.name)).size, 4, 'move names distinct ' + sp.id);
    for (const m of sp.moves) { assert.ok(m.power >= 30 && m.power <= 120); assert.ok(m.accuracy >= 70 && m.accuracy <= 100); assert.ok(m.effect === null || G.EFFECTS.includes(m.effect)); assert.ok(G.TYPES.includes(m.type)); }
    assert.ok(sp.bodyRecipe.segs >= 3 && sp.bodyRecipe.segs <= 7 && sp.bodyRecipe.limbs % 2 === 0 && sp.bodyRecipe.hue >= 0 && sp.bodyRecipe.hue < 360);
    assert.ok(sp.bodyRecipe.sig.length <= 2 && new Set(sp.bodyRecipe.sig).size === sp.bodyRecipe.sig.length);
    assert.match(sp.id, /^[0-9a-f]{8}$/);
  }
});
test('species: tier forces the base-stat band', () => {
  for (let t = 1; t <= 5; t++) for (let i = 0; i < 60; i++) {
    const sp = G.species('b' + i, 'x', { tier: t }), lo = 20 + (t - 1) * 8, hi = Math.min(120, 60 + (t - 1) * 15);
    assert.equal(sp.tier, t); for (const s of G.STATS) assert.ok(sp.base[s] >= lo && sp.base[s] <= hi, `t${t} ${s}=${sp.base[s]}`);
  }
});
test('individual: iv within +-15 %, size 0.8..1.3, deterministic, shiny implies hueFlip', () => {
  const sp = G.species('a', 'b');
  for (let i = 0; i < 500; i++) {
    const a = G.individual(sp, i), b = G.individual(sp, i);
    assert.deepEqual(a, b); assert.equal(a.speciesId, sp.id);
    for (const s of G.STATS) assert.ok(Math.abs(a.iv[s]) <= 0.15 + 1e-9);
    assert.ok(a.size >= 0.8 && a.size <= 1.3); assert.equal(a.shiny, a.hueFlip);
  }
});
test('statsAt: positive ints, monotone in level, hp >= level + 10, grafts only raise, level clamps', () => {
  const sp = G.species('s', 'c'), ind = G.individual(sp, 1); let prev = null;
  for (let lv = 1; lv <= G.MAX_LEVEL; lv++) {
    const st = G.statsAt(sp, ind, lv);
    for (const s of G.STATS) { assert.ok(Number.isInteger(st[s]) && st[s] > 0, `${s}@${lv}`); if (prev) assert.ok(st[s] >= prev[s], `${s} monotone @${lv}`); }
    assert.ok(st.hp >= lv + 10); prev = st;
  }
  assert.deepEqual(G.statsAt(sp, ind, 999), G.statsAt(sp, ind, 50)); assert.deepEqual(G.statsAt(sp, ind, -5), G.statsAt(sp, ind, 1));
  for (const [part, stat] of Object.entries(G.GRAFT_STAT)) {
    const g = Object.assign({}, ind, { grafts: [{ p: part, t: 2 }] }), a = G.statsAt(sp, ind, 20), b = G.statsAt(sp, g, 20);
    for (const s of G.STATS) s === stat ? assert.ok(b[s] > a[s], part) : assert.equal(b[s], a[s], part + ' leaks into ' + s);
  }
  assert.ok(G.statsAt(sp, { iv: null }, 5).hp > 0, 'missing iv ok');
});
test('xp curve: strictly increasing to 50, round trip, xpToLevel caps', () => {
  for (let lv = 2; lv <= G.MAX_LEVEL; lv++) { assert.ok(G.xpForLevel(lv) > G.xpForLevel(lv - 1)); assert.equal(G.xpToLevel(G.xpForLevel(lv)), lv); assert.equal(G.xpToLevel(G.xpForLevel(lv) - 1), lv - 1); }
  assert.equal(G.xpToLevel(1e12), G.MAX_LEVEL); assert.equal(G.xpToLevel(0), 1); assert.equal(G.xpToLevel(-50), 1); assert.equal(G.xpForLevel(1), 0);
});
test('typeChart: deterministic per universe, differs across universes, mult only 0.5/1/2, no pair super-effective both ways', () => {
  const a = G.typeChart('u1'), b = G.typeChart('u1'); assert.deepEqual(a.beats, b.beats);
  let differ = 0; for (let i = 0; i < 20; i++) if (JSON.stringify(G.typeChart('u' + i).beats) !== JSON.stringify(a.beats)) differ++;
  assert.ok(differ >= 15, 'charts vary: ' + differ);
  for (let u = 0; u < 100; u++) {
    const c = G.typeChart(u);
    for (const x of G.TYPES) {
      assert.equal(c.mult(x, x), 1);
      for (const y of G.TYPES) {
        const m = c.mult(x, y); assert.ok([0.5, 1, 2].includes(m)); if (m === 2) assert.equal(c.mult(y, x), 0.5, `${x}>${y} u${u}`); if (m === 0.5) assert.equal(c.mult(y, x), 2);
      }
      assert.equal(c.beats[x].length, 2); assert.equal(c.weak[x].length, 2);
    }
  }
});
test('makeMove: deterministic, power honoured, accuracy falls with power', () => {
  assert.deepEqual(G.makeMove('ember', 5), G.makeMove('ember', 5));
  assert.equal(G.makeMove('leaf', 1, 77).power, 77);
  for (let i = 0; i < 200; i++) { const m = G.makeMove(G.TYPES[i % 8], i); assert.ok(m.power >= 40 && m.power <= 100 && m.accuracy >= 70 && m.accuracy <= 100 && m.type === G.TYPES[i % 8]); }
  assert.ok(G.makeMove('void', 1, 120).accuracy <= G.makeMove('void', 1, 40).accuracy);
});
test('isTrainer: elders always, dealers/cashiers/clerks never, ~25 % of others', () => {
  assert.ok(G.isTrainer('elder', 1)); for (const r of ['dealer', 'cashier', 'clerk']) for (let i = 0; i < 50; i++) assert.ok(!G.isTrainer(r, i));
  let n = 0; for (let i = 0; i < 2000; i++) if (G.isTrainer('villager', i)) n++; assert.ok(n > 400 && n < 600, 'rate ' + n / 2000);
});
test('GRAFT_MAX is 6 and GRAFT_STAT covers five parts onto five stats', () => {
  assert.equal(G.GRAFT_MAX, 6); assert.equal(Object.keys(G.GRAFT_STAT).length, 5); for (const s of Object.values(G.GRAFT_STAT)) assert.ok(G.STATS.includes(s));
});

// ── party ──
test('damage: hp floors at 0, faint = dead, dead takes no damage, returns true only on the killing hit', () => {
  const m = mkMon('d', 10), mh = P.maxHp(m);
  assert.equal(m.hp, mh); assert.equal(P.damage(m, 1), false); assert.equal(m.hp, mh - 1); assert.ok(!m.dead);
  assert.equal(P.damage(m, 1e9), true); assert.equal(m.hp, 0); assert.ok(m.dead);
  assert.equal(P.damage(m, 5), false); assert.equal(m.hp, 0);
});
test('heal/faint/revive rules: survivors heal to full, dead stay dead through healParty, revive restores full once', () => {
  const a = mkMon('h1', 12), b = mkMon('h2', 12), c = mkMon('h3', 12); a.id = 'a'; b.id = 'b'; c.id = 'c';
  const pf = mkPf(a, b, c); P.damage(a, 3); P.damage(b, 1e9); P.damage(c, 1);
  assert.equal(P.healParty(pf), 2); assert.equal(a.hp, P.maxHp(a)); assert.equal(c.hp, P.maxHp(c)); assert.ok(b.dead); assert.equal(b.hp, 0);
  assert.equal(P.revive(a), false, 'living mon: revive refused'); assert.equal(P.revive(b), true); assert.ok(!b.dead); assert.equal(b.hp, P.maxHp(b)); assert.equal(P.revive(b), false);
  assert.deepEqual(P.canSummon(b), { ok: true, why: '' }); P.faint(b); assert.deepEqual(P.canSummon(b), { ok: false, why: 'dead' }); assert.deepEqual(P.canSummon(null), { ok: false, why: 'empty' });
});
test('reviveAtCvs: party first, then pen; charges 10+4L; poor/none refused without mutation', () => {
  const a = mkMon('r1', 10), b = mkMon('r2', 20); a.id = 'a'; b.id = 'b'; P.faint(a); P.faint(b);
  const pf = { pokemans: [a, b], party: ['b'], pen: ['a'], gor: 5 };
  let r = P.reviveAtCvs(pf); assert.deepEqual([r.ok, r.why, r.cost], [false, 'poor', 90]); assert.equal(pf.gor, 5); assert.ok(b.dead);
  pf.gor = 1000; r = P.reviveAtCvs(pf); assert.ok(r.ok); assert.equal(r.mon, b); assert.equal(pf.gor, 1000 - 90); assert.ok(!b.dead);
  r = P.reviveAtCvs(pf); assert.equal(r.mon, a); assert.equal(pf.gor, 1000 - 90 - 50);
  r = P.reviveAtCvs(pf); assert.deepEqual([r.ok, r.why], [false, 'none']);
  assert.equal(P.reviveCost({ level: 1 }), 14); assert.equal(P.reviveCost({ level: 50 }), 210);
});
test('moveCount / moveAt: sp moves + signature; learned overrides; sig only at the tail', () => {
  const m = mkMon('mv', 10); assert.equal(P.moveCount(m), 4); m.sig = { name: 'S', type: 'void', power: 50, accuracy: 90, effect: null };
  assert.equal(P.moveCount(m), 5); assert.equal(P.moveAt(m, 4), m.sig); assert.equal(P.moveAt(m, 0), m.sp.moves[0]);
  const L = { name: 'L', type: 'leaf', power: 40, accuracy: 90, effect: null }; m.learned = { 1: L }; assert.equal(P.moveAt(m, 1), L); assert.equal(P.moveAt(m, 2), m.sp.moves[2]);
});
test('moveDamage / cooldown: finite, >= 1, faster spd -> shorter cd, cd bounded', () => {
  const chart = G.typeChart('c'); let n = 0;
  for (let i = 0; i < 60; i++) { const a = mkMon('x' + i, 5 + (i % 40)), d = mkMon('y' + i, 5 + ((i * 7) % 40)); for (let k = 0; k < P.moveCount(a); k++) { const dmg = P.moveDamage(a, P.moveAt(a, k), d, chart); assert.ok(Number.isFinite(dmg) && dmg >= 1, 'dmg ' + dmg); n++; } const cd = P.moveCooldown(a); assert.ok(cd > 0 && cd <= 3 && Number.isFinite(cd)); }
  assert.ok(n > 200);
});
test('awardXp: never negative, dead/zero/negative award is a no-op, level cap 50, hp never exceeds max', () => {
  const m = mkMon('xp', 1); assert.equal(P.awardXp(m, 0), 0); assert.equal(P.awardXp(m, -9), 0);
  let gained = 0; for (let i = 0; i < 400; i++) { gained += P.awardXp(m, 500); assert.ok(m.hp <= P.maxHp(m)); }
  assert.equal(m.level, G.MAX_LEVEL); assert.ok(gained >= 49); P.faint(m); const xp = m.xp; assert.equal(P.awardXp(m, 1000), 0); assert.equal(m.xp, xp);
  for (let t = 1; t <= 5; t++) for (let l = 1; l <= 50; l += 7) { assert.ok(P.xpForKill(l, t) > 0 && P.xpForCatch(t, l) > 0); }
  assert.ok(P.xpForKill(10, 3) > P.xpForKill(5, 3)); assert.ok(P.xpForKill(5, 4) > P.xpForKill(5, 3));
});
test('active / pickActive: dead mons are never the active one', () => {
  const a = mkMon('p1', 5), b = mkMon('p2', 5); a.id = 'a'; b.id = 'b'; const pf = mkPf(a, b); pf.active = 0;
  P.faint(a); const act = P.active(pf); assert.ok(!act || !act.dead);
  P.faint(b); const none = P.active(pf); assert.ok(!none || !none.dead);
});

// ── catch ──
test('catchChance: monotone in tier and in damage, in [0.1,1], bonus adds then clamps', () => {
  for (let t = 1; t <= 3; t++) { let prev = 0; for (let h = 1; h >= 0; h -= 0.05) { const c = C.catchChance(t, h); assert.ok(c >= 0.1 && c <= 1 && c >= prev - 1e-12); prev = c; } }
  for (let h = 0; h <= 1; h += 0.1) { assert.ok(C.catchChance(2, h) >= C.catchChance(1, h)); assert.ok(C.catchChance(3, h) >= C.catchChance(2, h)); }
  assert.equal(C.catchChance(1, 1), 0.1); assert.ok(Math.abs(C.catchChance(1, 1, C.LURE_BONUS) - 0.25) < 1e-12); assert.equal(C.catchChance(3, 0, 5), 1);
  assert.equal(C.catchChance(99, 0.5), C.catchChance(3, 0.5)); assert.equal(C.catchChance(0, 0.5), C.catchChance(1, 0.5)); assert.equal(C.catchChance(2, -3), C.catchChance(2, 0)); assert.equal(C.catchChance(2, 9), C.catchChance(2, 1));
});
test('catchRoll boundary: u == chance escapes, u just below catches', () => {
  assert.equal(C.catchRoll(0.5, 0.5).caught, false); assert.equal(C.catchRoll(0.5, 0.4999999).caught, true); assert.equal(C.catchRoll(1, 0.999999).caught, true);
});
test('arcPoint: t=0 is the hand; throwTime within tMax; dots stay finite and move away', () => {
  const o = { x: 1, y: 2, z: 3 }, d = { x: 0, y: 0, z: -1 }, up = { x: 0, y: 1, z: 0 }, out = { x: 0, y: 0, z: 0 };
  C.arcPoint(o, d, up, 0, out); assert.deepEqual(out, o);
  assert.ok(C.throwTime() > 0 && C.throwTime() <= C.BALL.tMax);
  const dots = Array.from({ length: C.BALL.dots }, () => ({ x: 0, y: 0, z: 0 })); assert.equal(C.previewDots(o, d, up, dots), C.BALL.dots);
  let prev = 0; for (const p of dots) { const dist = Math.hypot(p.x - o.x, p.z - o.z); assert.ok(Number.isFinite(p.y) && dist > prev); prev = dist; }
  assert.ok(prev <= C.BALL.reach + 1e-6, 'reach ' + prev);
});
test('makeMon: level >= 1, hp = full max, not dead, monLevel matches xp, junk args do not NaN', () => {
  const sp = G.species('m', 'n'), ind = G.individual(sp, 3);
  const m = C.makeMon(sp, ind, 0, undefined); assert.equal(m.level, 1); assert.equal(m.hp, G.statsAt(sp, ind, 1).hp); assert.equal(m.dead, false); assert.equal(m.xp, 0);
  assert.equal(C.monLevel(C.makeMon(sp, ind, 20, G.xpForLevel(20))), 20); assert.equal(C.monLevel(null), 1); assert.equal(C.PARTY_MAX, 6);
});

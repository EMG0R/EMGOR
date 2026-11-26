import test from 'node:test';
import assert from 'node:assert/strict';
import { createMatch, botInput, BOT_TIERS, ringStage } from '../../js/smash-engine.js';
import { BRAWL_API } from '../../js/smash-geom.js';
import { SAMPLES, rate, match, mk, run, B } from './smash-arena.mjs';

// the CPU ladder is L1-L9 (smash-bot.js BOT_TIERS); tests run under the default FAST preset
// headless: idle player (ctrl 'net', input 0) vs a tiered bot on the ring stage, fixed 60 Hz steps
function idleRun(tier, seed, fid, seconds) {
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net', stocks: 1 }, { fid: fid || 'swift', ctrl: 'cpu', tier, stocks: 1 }], stageDef: ringStage(SAMPLES, 'bt' + seed), seed, stocks: 1, time: 180 * 60 });
  let koFrame = -1, play0 = -1; const n = seconds * 60;
  for (let i = 0; i < n && S.phase !== 'over'; i++) { if (play0 < 0 && S.phase === 'play') play0 = S.frame; S.step(S.fighters.map((f) => (f.bot ? botInput(S, f) : 0))); if (koFrame < 0 && S.fighters[0].stocks <= 0) koFrame = S.frame; }
  return { koFrame, rel: koFrame < 0 ? -1 : koFrame - play0, winner: S.result ? S.result.winner : -1, dealt: S.fighters[1].dealt, S };
}
const SEEDS = [11, 222, 3333, 44444, 555555];
const pooled = (a, b, n, secs) => { let w = 0, l = 0; for (const fid of ['pilot', 'swift', 'hauler', 'ranger']) { const r = rate(a, b, n, fid, secs || 120); w += r.w; l += r.l; } return { w, l, n: 4 * n, rate: w / (4 * n) }; };

test('ladder table: 9 tiers, reaction 24 -> 1 frames, attack rate and shield / DI / edge flags only ever grow', () => {
  const T = BOT_TIERS.slice(1); assert.equal(T.length, 9);
  assert.deepEqual(T.map((t) => t.lvl), [1, 2, 3, 4, 5, 6, 7, 8, 9]);
  assert.deepEqual(T.map((t) => t.react), [24, 18, 14, 10, 8, 6, 4, 2, 1]);
  assert.deepEqual(T.map((t) => t.every), [12, 8, 5, 3, 2, 1, 1, 1, 1]);
  for (let i = 1; i < 9; i++) { const a = T[i - 1], b = T[i]; assert.ok(b.atk >= a.atk && b.miss <= a.miss && b.react < a.react && b.shield >= a.shield && b.di >= a.di && b.edge >= a.edge && b.kc >= a.kc && b.punish >= a.punish, 'L' + (i + 1)); }
  assert.deepEqual(T.map((t) => t.atk), [0.15, 0.25, 0.4, 0.55, 0.7, 0.8, 0.9, 0.95, 1]);
  assert.ok(T[3].air > 0 && T[3].spec && T[3].punish === 1 && T[5].punish === 2 && T[5].di === 2 && T[6].edge === 1 && T[7].edge === 2 && T[7].kc && T[8].adapt && T[4].dodge > 0 && T[2].grab && T[2].rec === 1);
});
test('idle player never wins vs L1 / L3 / L5 / L9 over 60 s', () => {
  for (const tier of [1, 3, 5, 9]) for (const seed of SEEDS.slice(0, 3)) {
    const r = idleRun(tier, seed, 'swift', 60);
    assert.ok(r.winner !== 0, 'L' + tier + ' seed ' + seed + ' idle player won');
    assert.ok(r.S.fighters[0].dealt < 1, 'idle player dealt damage');
  }
});
test('HARNESS: L9 KOs an idle player within 600 frames of the go (5 seeds x 3 fighters)', () => {
  for (const seed of SEEDS) for (const fid of ['pilot', 'swift', 'ranger']) { const r = idleRun(9, seed, fid, 60); assert.ok(r.rel > 0 && r.rel <= 600, 'L9 ' + fid + ' seed ' + seed + ' ko after ' + r.rel + ' f'); }
});
test('low tiers are slower than high tiers to KO an idle player (average over seeds)', () => {
  let e = 0, h = 0; for (const seed of SEEDS) { const a = idleRun(1, seed, 'pilot', 90).koFrame, b = idleRun(9, seed, 'pilot', 90).koFrame; e += a < 0 ? 90 * 60 : a; h += b; }
  assert.ok(e > h * 1.5, 'L1 ' + e + ' L9 ' + h);
});
test('determinism: same seed -> same KO frame and same hash', () => {
  assert.equal(idleRun(6, 77, 'swift', 60).koFrame, idleRun(6, 77, 'swift', 60).koFrame);
  assert.equal(match(9, 5, 5, 'pilot', 60).hash(), match(9, 5, 5, 'pilot', 60).hash());
});
test('the bot only sees the opponent through its reaction delay (perceived state is old)', () => {
  const S = mk({ ctrl: 'net' }, { ctrl: 'cpu', tier: 1 }); const b = S.fighters[1].bot;
  S.fighters[0].x = -150; run(S, 40, (i, s) => [0, botInput(s, s.fighters[1])]);
  S.fighters[0].x = 150; for (let i = 0; i < 5; i++) S.step([0, botInput(S, S.fighters[1])]);
  const old = b.ring[0][(S.frame - BOT_TIERS[1].react) & 31]; assert.ok(old.x < 0, 'delayed view still sees the old spot ' + old.x);
});

// ---- win rates: the CPU has to actually beat people ----
test('HARNESS: L9 beats L1 >= 90 % over 10 seeded matches', () => {
  const r = rate(9, 1, 10, 'pilot'); assert.ok(r.rate >= 0.9, JSON.stringify(r));
  const s = pooled(9, 1, 5); assert.ok(s.rate >= 0.9, 'pooled ' + JSON.stringify(s));
});
test('HARNESS: L5 vs L5 is roughly even (40 matches over 4 fighters)', () => {
  const r = pooled(5, 5, 10); assert.ok(r.rate >= 0.3 && r.rate <= 0.7, JSON.stringify(r));
});
test('the ladder is ordered: L9 > L5, L7 > L3, L5 > L2 (pooled over 4 fighters)', () => {
  const a = pooled(9, 5, 5), b = pooled(7, 3, 5), c = pooled(5, 2, 5);
  assert.ok(a.rate >= 0.55, 'L9 v L5 ' + JSON.stringify(a)); assert.ok(b.rate >= 0.8, 'L7 v L3 ' + JSON.stringify(b)); assert.ok(c.rate >= 0.8, 'L5 v L2 ' + JSON.stringify(c));
});
test('L7+ beats a scripted mash-forward-smash dummy >= 90 % with MARO; L9 >= 60 % with SONIK', () => {
  const a = rate(7, 'dummy', 20, 'pilot'), b = rate(9, 'dummy', 20, 'swift'); assert.ok(a.rate >= 0.9, JSON.stringify(a)); assert.ok(b.rate >= 0.6, 'swift ' + JSON.stringify(b));
});
test('a human-style player who never defends loses to L9 even when it mashes every attack', () => {
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net', stocks: 2 }, { fid: 'pilot', ctrl: 'cpu', tier: 9, stocks: 2 }], stageDef: ringStage(SAMPLES, 'mash'), seed: 9, stocks: 2, time: 90 * 60 });
  while (S.phase !== 'over' && S.frame < 90 * 60 + 300) S.step([(S.frame % 6 < 3 ? B.A : 0) | (S.fighters[0].x < S.fighters[1].x ? B.R : B.L), botInput(S, S.fighters[1])]);
  assert.equal(S.result.winner, 1);
});

// ---- recovery: L3+ always makes it back when it physically can ----
test('recovery: L3+ gets back to the ledge / stage from offstage and below the ledge (MARO, SONIK, HUM, JOSHI)', () => {
  for (const tier of [3, 9]) for (const fid of ['pilot', 'swift', 'hum', 'hopper']) for (const side of [-1, 1]) for (const dxo of [10, 60]) for (const y of [0, 40, 85]) {
    const S = createMatch({ players: [{ fid, ctrl: 'cpu', tier, stocks: 2 }, { fid: 'pilot', ctrl: 'net', stocks: 2 }], stageDef: ringStage(SAMPLES, 'r'), seed: 5, stocks: 2, time: 7200 });
    while (S.phase !== 'play') S.step([0, 0]);
    const b = S.fighters[0]; S.fighters[1].x = 0; b.x = side * (216 + dxo); b.y = y; b.vy = 0; b.grounded = false; b.state = 'air';
    let back = false; for (let i = 0; i < 300 && b.stocks === 2; i++) { S.step([botInput(S, b), 0]); if (b.state === 'ledge' || b.grounded) { back = true; break; } }
    assert.ok(back, 'L' + tier + ' ' + fid + ' side ' + side + ' dx ' + dxo + ' y ' + y + ' ended ' + b.state + ' ' + (b.x | 0) + ',' + (b.y | 0));
  }
});

// ---- geometry API (smash-geom.js): what the bot reads instead of importing the engine ----
test('BRAWL_API: hurt capsules, hit capsules, launch-arc KO predictor, kill percent', () => {
  const S = mk(); const f = S.fighters[0], v = S.fighters[1];
  const caps = BRAWL_API.hurtCaps(v, []); assert.equal(caps.length, 3); assert.ok(caps[0][4] > 0);
  const h = f.def.moves.fsmash.hits[0], cap = BRAWL_API.hitCapsuleWorld(f, h); assert.equal(cap.length, 5);
  assert.ok(BRAWL_API.hitTouches(0, 0, 1, { seg: [0, -20, 40, -20], r: 8 }, BRAWL_API.hurtCaps({ x: 30, y: 0, hurtMul: 1, state: 'idle', def: f.def }, [])), 'a swing through the body touches');
  assert.ok(!BRAWL_API.hitTouches(0, 0, 1, { seg: [0, -20, 10, -20], r: 4 }, BRAWL_API.hurtCaps({ x: 90, y: 0, hurtMul: 1, state: 'idle', def: f.def }, [])), 'a short swing does not');
  v.x = 150; v.y = 0; v.grounded = true;
  assert.equal(BRAWL_API.launchArc(S.stage, v, 20, 40, 1, 0, 0).ko, false, 'a weak hit never KOs');
  assert.equal(BRAWL_API.launchArc(S.stage, v, 300, 40, 1, 0, 0).ko, true, 'a huge hit KOs');
  const kp = BRAWL_API.koPct(S.stage, v, { dmg: 15, bkb: 28, gr: 95, ang: 38 }, 1, 1); assert.ok(kp > 20 && kp < 300, 'fsmash kill percent ' + kp);
});

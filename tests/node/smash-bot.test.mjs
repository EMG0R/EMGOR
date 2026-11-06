import test from 'node:test';
import assert from 'node:assert/strict';
import { createMatch, botInput, BOT_TIERS, ringStage } from '../../js/smash-engine.js';
import { SAMPLES, rate, match, mk, run, B } from './smash-arena.mjs';

// headless: idle player (ctrl 'net', input 0) vs a tiered bot on the ring stage, fixed 60 Hz steps
function idleRun(tier, seed, fid, seconds) {
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net', stocks: 1 }, { fid: fid || 'swift', ctrl: 'cpu', tier, level: 2, stocks: 1 }], stageDef: ringStage(SAMPLES, 'bt' + seed), seed, stocks: 1, time: 180 * 60 });
  let koFrame = -1; const n = seconds * 60;
  for (let i = 0; i < n && S.phase !== 'over'; i++) { S.step(S.fighters.map((f) => (f.bot ? botInput(S, f) : 0))); if (koFrame < 0 && S.fighters[0].stocks <= 0) koFrame = S.frame; }
  return { koFrame, winner: S.result ? S.result.winner : -1, dealt: S.fighters[1].dealt, S };
}
const SEEDS = [11, 222, 3333, 44444, 555555];

test('idle player never wins vs any tier over 60 s', () => {
  for (const tier of [1, 2, 3]) for (const seed of SEEDS) {
    const r = idleRun(tier, seed, 'swift', 60);
    assert.ok(r.winner !== 0, 'tier ' + tier + ' seed ' + seed + ' idle player won');
    assert.ok(r.S.fighters[0].dealt < 1, 'idle player dealt damage');
  }
});
test('hard bot KOs an idle player within 20 s (count-in included); every tier does it inside 40 s', () => {
  for (const seed of SEEDS) {
    const r = idleRun(3, seed, 'swift', 60); assert.ok(r.koFrame > 0 && r.koFrame <= 20 * 60, 'hard seed ' + seed + ' ko frame ' + r.koFrame);
    for (const tier of [1, 2]) { const q = idleRun(tier, seed, 'pilot', 60); assert.ok(q.koFrame > 0 && q.koFrame <= 40 * 60, 'tier ' + tier + ' seed ' + seed + ' ko frame ' + q.koFrame); }
  }
});
test('easy is slower than hard to KO an idle player (average over seeds)', () => {
  let e = 0, h = 0; for (const seed of SEEDS) { e += idleRun(1, seed, 'pilot', 60).koFrame; h += idleRun(3, seed, 'pilot', 60).koFrame; }
  assert.ok(e > h, 'easy ' + e + ' hard ' + h);
});
test('bot tier params: reaction 20 / 8 / 3 frames, edge-guard hard only', () => {
  const T = BOT_TIERS.slice(1);
  assert.deepEqual(T.map((t) => t.react), [20, 8, 3]);
  assert.deepEqual(T.map((t) => t.edge), [false, false, true]);
  assert.ok(T[0].atk < T[1].atk && T[1].atk < T[2].atk);
});
test('determinism: same seed -> same KO frame and same hash', () => {
  assert.equal(idleRun(2, 77, 'swift', 60).koFrame, idleRun(2, 77, 'swift', 60).koFrame);
  assert.equal(match(3, 2, 5, 'pilot', 60).hash(), match(3, 2, 5, 'pilot', 60).hash());
});
test('the bot only sees the opponent through its reaction delay (perceived state is old)', () => {
  const S = mk({ ctrl: 'net' }, { ctrl: 'cpu', tier: 1 }); const b = S.fighters[1].bot;
  S.fighters[0].x = -150; run(S, 30, (i, s) => [0, botInput(s, s.fighters[1])]);
  // teleport the player: the easy bot's ring still says the old position for ~20 frames
  S.fighters[0].x = 150; for (let i = 0; i < 5; i++) S.step([0, botInput(S, S.fighters[1])]);
  const old = b.ring[0][(S.frame - BOT_TIERS[1].react) & 31]; assert.ok(old.x < 0, 'delayed view still sees the old spot ' + old.x);
});

// ---- win rates: the CPU has to actually beat people ----
test('hard beats easy >= 90 % over 20 seeded matches', () => {
  const r = rate(3, 1, 20, 'pilot'); assert.ok(r.rate >= 0.9, JSON.stringify(r));
});
test('hard beats a scripted mash-forward-smash dummy >= 95 % (20 matches, MARO + SONIK)', () => {
  const a = rate(3, 'dummy', 20, 'pilot'), b = rate(3, 'dummy', 20, 'swift'); assert.ok(a.rate >= 0.95, JSON.stringify(a)); assert.ok(b.rate >= 0.9, 'swift ' + JSON.stringify(b));
});
test('medium beats the same dummy >= 70 % (20 matches, MARO + SONIK)', () => {
  const a = rate(2, 'dummy', 20, 'pilot'), b = rate(2, 'dummy', 20, 'swift'); assert.ok(a.rate >= 0.7, JSON.stringify(a)); assert.ok(b.rate >= 0.7, 'swift ' + JSON.stringify(b));
});
test('tiers are ordered by strength: hard > medium > easy against the dummy', () => {
  const e = rate(1, 'dummy', 20, 'pilot').rate, m = rate(2, 'dummy', 20, 'pilot').rate, h = rate(3, 'dummy', 20, 'pilot').rate;
  assert.ok(h >= m && m > e, 'easy ' + e + ' med ' + m + ' hard ' + h);
});
test('a human-style player who never defends loses to hard even when it mashes every attack', () => {
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net', stocks: 2 }, { fid: 'pilot', ctrl: 'cpu', tier: 3, stocks: 2 }], stageDef: ringStage(SAMPLES, 'mash'), seed: 9, stocks: 2, time: 90 * 60 });
  while (S.phase !== 'over' && S.frame < 90 * 60 + 300) S.step([(S.frame % 6 < 3 ? B.A : 0) | (S.fighters[0].x < S.fighters[1].x ? B.R : B.L), botInput(S, S.fighters[1])]);
  assert.equal(S.result.winner, 1);
});

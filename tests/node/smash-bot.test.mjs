import test from 'node:test';
import assert from 'node:assert/strict';
import { createMatch, botInput, BOT_TIERS } from '../../js/smash-engine.js';
import { terrainStage } from '../../js/smash-stages.js';

// headless: idle player (ctrl 'net', input 0) vs a tiered bot, fixed 60 Hz steps. Returns { koFrame, winner, dealt, frames }.
function run(tier, seed, fid, seconds) {
  const samples = new Float32Array(128);
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net', stocks: 1 }, { fid: fid || 'swift', ctrl: 'cpu', tier, level: 2, stocks: 1 }], stageDef: terrainStage(samples, 'bt' + seed), seed, stocks: 1, time: 180 * 60 });
  let koFrame = -1; const n = seconds * 60;
  for (let i = 0; i < n && S.phase !== 'over'; i++) {
    S.step(S.fighters.map((f) => (f.bot ? botInput(S, f) : 0)));
    if (koFrame < 0 && S.fighters[0].stocks <= 0) koFrame = S.frame;
  }
  return { koFrame, winner: S.result ? S.result.winner : -1, dealt: S.fighters[1].dealt, frames: S.frame, S };
}
const SEEDS = [11, 222, 3333, 44444, 555555];

test('idle player never wins vs any tier over 60 s', () => {
  for (const tier of [1, 2, 3]) for (const seed of SEEDS) {
    const r = run(tier, seed, 'swift', 60);
    assert.ok(r.winner !== 0, 'tier ' + tier + ' seed ' + seed + ' idle player won');
    assert.ok(r.S.fighters[0].dealt < 1, 'idle player dealt damage');
  }
});
test('hard bot KOs an idle player within 40 s (count-in included)', () => {
  for (const seed of SEEDS) {
    const r = run(3, seed, 'swift', 60);
    assert.ok(r.koFrame > 0 && r.koFrame <= 40 * 60, 'seed ' + seed + ' ko frame ' + r.koFrame);
  }
});
test('easy bot takes more than 20 s to KO an idle player', () => {
  for (const seed of SEEDS) {
    const r = run(1, seed, 'swift', 60);
    assert.ok(r.koFrame < 0 || r.koFrame > 20 * 60, 'seed ' + seed + ' ko frame ' + r.koFrame);
  }
});
test('tiers are ordered: damage dealt in 30 s easy < hard', () => {
  let e = 0, h = 0; for (const seed of SEEDS) { e += run(1, seed, 'swift', 30).dealt; h += run(3, seed, 'swift', 30).dealt; }
  assert.ok(e < h, 'easy ' + e + ' hard ' + h);
});
test('bot tier params: reaction 0.5/0.3/0.15 s, attack 30/60/90 %/s, edge-guard hard only', () => {
  const T = BOT_TIERS.slice(1);
  assert.deepEqual(T.map((t) => t.react / 60), [0.5, 0.3, 0.15].map((x) => Math.round(x * 60) / 60));
  assert.deepEqual(T.map((t) => t.atk), [0.3, 0.6, 0.9]);
  assert.deepEqual(T.map((t) => t.edge), [false, false, true]);
});
test('determinism: same seed -> same KO frame', () => {
  assert.equal(run(2, 77, 'swift', 60).koFrame, run(2, 77, 'swift', 60).koFrame);
});

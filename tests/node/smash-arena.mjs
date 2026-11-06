// shared headless harness for the smash tests (not a test file: the runner only picks *.test.mjs)
import { createMatch, botInput, B, ringStage } from '../../js/smash-engine.js';

export const SAMPLES = new Float32Array(128);
export function mk(a, b, opt) {
  opt = opt || {};
  const S = createMatch({ players: [Object.assign({ fid: 'pilot', ctrl: 'net', stocks: 3 }, a || {}), Object.assign({ fid: 'pilot', ctrl: 'net', stocks: 3 }, b || {})], stageDef: ringStage(SAMPLES, 'ts' + (opt.seed || 1)), seed: opt.seed || 1, stocks: 3, time: (opt.secs || 180) * 60 });
  while (S.phase !== 'play') S.step([0, 0]);
  return S;
}
export const run = (S, n, f) => { for (let i = 0; i < n; i++) S.step(f ? f(i, S) : [0, 0]); };
// "mash forward smash": walks at the nearest foe, taps the forward c-stick when close; naive recovery (toward centre + jump)
export function dummyInput(S, f) {
  const o = S.fighters[1 - f.i]; if (!o || o.stocks <= 0 || S.phase !== 'play') return 0;
  const dx = o.x - f.x, d = dx > 0 ? 1 : -1;
  if (!f.grounded && (Math.abs(f.x) > 200 || f.y > 20)) return (f.x > 0 ? B.L : B.R) | (f.vy > 0 && f.jumps > 0 && (S.frame & 1) ? B.J : 0);
  if (Math.abs(dx) > 46 || Math.abs(o.y - f.y) > 60) return d > 0 ? B.R : B.L;
  return (S.frame & 3) < 2 ? (d > 0 ? B.CR : B.CL) : 0;
}
// kinds: 1/2/3 = CPU tier, 'dummy'. Returns the finished sim.
export function match(kindA, kindB, seed, fid, secs) {
  const slot = (k) => (k === 'dummy' ? { fid, ctrl: 'net', stocks: 2 } : { fid, ctrl: 'cpu', tier: k, stocks: 2 });
  const S = createMatch({ players: [slot(kindA), slot(kindB)], stageDef: ringStage(SAMPLES, 'm' + seed), seed, stocks: 2, time: (secs || 120) * 60 });
  const kinds = [kindA, kindB];
  while (S.phase !== 'over' && S.frame < (secs || 120) * 60 + 400) S.step(S.fighters.map((f, i) => (kinds[i] === 'dummy' ? dummyInput(S, f) : botInput(S, f))));
  return S;
}
export function rate(a, b, n, fid, secs) {
  let w = 0, l = 0, d = 0;
  for (let i = 0; i < n; i++) { const r = match(a, b, 100 + i * 37, fid, secs).result; if (!r || r.winner < 0) d++; else if (r.winner === 0) w++; else l++; }
  return { w, l, d, n, rate: w / n };
}
export { B };

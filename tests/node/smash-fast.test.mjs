import test from 'node:test';
import assert from 'node:assert/strict';
import { PHYS, PRESETS, setPreset, physOf, hitlagOf } from '../../js/smash-phys.js';
import { FIGHTERS } from '../../js/smash-fighters.js';
import { newView, updateView, viewEvent, createMatch, ringStage } from '../../js/smash-engine.js';
import { frameData } from '../../js/smash-moves.js';
import { launchArc } from '../../js/smash-geom.js';
import { mk, run, B, SAMPLES } from './smash-arena.mjs';

// default preset = FAST (this file runs without setPreset); CLASSIC is selectable and restores the old numbers
const H = PHYS.H;
const near = (a, b, tol, msg) => assert.ok(Math.abs(a - b) <= tol, (msg || '') + ' got ' + a + ' want ' + b + ' +-' + tol);
function duel(d, a, b) { const S = mk(a, b); run(S, 40); const f = S.fighters[0], v = S.fighters[1]; f.x = 0; v.x = d; f.face = 1; v.face = -1; f.vx = v.vx = 0; run(S, 5); return { S, f, v }; }

test('FAST is the default preset; classic is selectable and switches back losslessly', () => {
  assert.equal(PHYS.preset, 'fast'); near(PHYS.grav, 0.14 * PHYS.REF, 1e-9); near(PHYS.fallCap, 2.4 * PHYS.REF, 1e-9); assert.equal(PHYS.fastFall, 1.7); assert.equal(PHYS.speed, 1.15); assert.equal(PHYS.buffer, 8);
  const mv = FIGHTERS.pilot.moves.fair, fastDur = mv.dur, fastLand = mv.landLag;
  setPreset('classic'); assert.equal(PHYS.preset, 'classic'); near(PHYS.grav, 0.095 * PHYS.REF, 1e-9); assert.equal(PHYS.speed, 1);
  const cm = FIGHTERS.pilot.moves.fair; assert.ok(cm.dur > fastDur, 'classic endlag is longer: ' + cm.dur + ' vs ' + fastDur); assert.ok(cm.landLag >= fastLand);
  setPreset('fast'); assert.equal(FIGHTERS.pilot.moves.fair.dur, fastDur); assert.equal(PRESETS.fast.camMin, 5 * H);
});
test('full hop is about 38-40 frames (was 49), apex still 1.45 H', () => {
  const S = mk(); const f = S.fighters[0]; run(S, 30); f.x = 0; const y0 = f.y; let top = f.y, n = 0;
  S.step([B.J, 0]); while (f.state === 'squat' || f.grounded) { S.step([B.J, 0]); if (++n > 6) break; }
  let air = 0; for (let i = 0; i < 200; i++) { S.step([B.J, 0]); if (f.y < top) top = f.y; if (f.grounded) break; air++; }
  assert.ok(air >= 36 && air <= 42, 'air frames ' + air); near(y0 - top, 1.45 * H, 0.05 * 1.45 * H, 'apex');
});
test('per-fighter physics: swift is fast + heavy-falling, hum floats, hauler is slow and heavy (physOf honours def.run / grav / fall / air)', () => {
  const p = (id) => physOf(FIGHTERS[id]);
  assert.ok(p('swift').run > p('pilot').run && p('pilot').run > p('hauler').run, 'run');
  assert.ok(p('swift').fall > p('hum').fall * 1.8 && p('hum').g < p('pilot').g < p('swift').g || p('hum').g < p('swift').g, 'gravity / fall');
  assert.ok(p('hum').air > p('hauler').air * 1.5, 'air accel');
  const runs = new Set(Object.keys(FIGHTERS).map((id) => p(id).run.toFixed(2))); assert.ok(runs.size >= 8, 'distinct run speeds: ' + runs.size);
  for (const id of Object.keys(FIGHTERS)) { assert.ok(p(id).run >= PHYS.runMin - 1e-9 && p(id).run <= PHYS.runMax + 1e-9, id + ' run in range'); }
});
test('dash: run start bursts above run speed, then settles; reversal inside the first 8 frames is a dash dance (no turn state), later it pivots', () => {
  const S = mk(); const f = S.fighters[0]; run(S, 30); f.x = -50; const P = physOf(f.def); let mx = 0;
  for (let i = 0; i < 4; i++) { S.step([B.R, 0]); mx = Math.max(mx, f.vx); } assert.ok(f.vx > 2.5 && f.state === 'run', 'fast ramp: vx ' + f.vx + ' after 4 f');
  for (let i = 0; i < 6; i++) { S.step([B.R, 0]); mx = Math.max(mx, f.vx); } assert.ok(mx > P.run + 0.1, 'burst ' + mx + ' > run ' + P.run);
  const t = mk(); const g = t.fighters[0]; run(t, 30); g.x = -50;
  for (let i = 0; i < 4; i++) t.step([B.R, 0]); t.step([B.L, 0]); assert.notEqual(g.state, 'turn', 'dash dance: no turn state'); assert.equal(g.face, -1); assert.ok(g.vx < -1.5, 'reversed at once ' + g.vx);
  const u = mk(); const h = u.fighters[0]; run(u, 30); h.x = -50; for (let i = 0; i < 30; i++) u.step([B.R, 0]); u.step([B.L, 0]); assert.equal(h.state, 'turn', 'pivot after a long run');
});
test('fast fall actually works: holding down after the apex snaps to 1.7x the cap at once', () => {
  const S = mk(); const f = S.fighters[0], P = physOf(f.def); f.y = -300; f.x = 0; f.vy = 0.5; f.grounded = false; f.landed = false; f.state = 'air';
  S.step([B.D, 0]); near(f.vy, P.fall * PHYS.fastFall, 0.01, 'vy after one frame'); assert.ok(f.ff);
});
test('landing lag is short: generic landing 2 f, air-dodge landing 4 f; aerial landing lag x0.75', () => {
  const S = mk(); const f = S.fighters[0]; f.y = -3; f.vy = 2; f.grounded = false; f.landed = false; f.state = 'air'; S.step([0, 0]); S.step([0, 0]); S.step([0, 0]); assert.equal(f.state, 'land'); assert.ok(f.lag <= PHYS.landLag, 'lag ' + f.lag);
  const nair = FIGHTERS.pilot.moves.nair; assert.ok(nair.landLag <= 5, 'nair landing lag ' + nair.landLag);
  const jab = FIGHTERS.pilot.moves.fsmash, fd = frameData(jab); assert.ok(fd.e <= Math.round(24 * 0.8) + 1, 'fsmash endlag ' + fd.e);
});
test('impact: hitlag scales with damage, +30 % on kb>=60, and a KO-range hit freezes 14-18 f', () => {
  assert.equal(hitlagOf(12), 8); assert.equal(hitlagOf(12, 80, false), 10); assert.ok(hitlagOf(12, 80, true) >= 14 && hitlagOf(12, 400, true) <= 18, 'ko ' + hitlagOf(12, 80, true) + '..' + hitlagOf(12, 400, true));
  const lo = duel(30); lo.S.step([B.CR, 0]); let evLo = null; for (let i = 0; i < 40 && !evLo; i++) { lo.S.step([0, 0]); evLo = lo.S.events.find((e) => e.t === 'hit'); }
  assert.ok(evLo && !evLo.ko && evLo.str >= 1, 'a 0 % fsmash is a hit but not a KO');
  const hi = duel(30); hi.v.pct = 170; hi.S.step([B.CR, 0]); let evHi = null, lagHi = 0; for (let i = 0; i < 40 && !evHi; i++) { hi.S.step([0, 0]); evHi = hi.S.events.find((e) => e.t === 'hit'); if (evHi) lagHi = hi.v.hitlag; }
  assert.ok(evHi && evHi.ko === 1 && evHi.str === 2, 'KO-range hit flagged'); assert.ok(lagHi >= 14 && lagHi <= 18, 'freeze ' + lagHi); assert.ok(lagHi > evLo.lag, 'longer than a normal hit');
});
test('3 hit sfx strengths layered: light = 1 cue, medium = 2 cues, heavy / KO = hit_heavy + hit_crit', () => {
  const grab = (pct) => { const d = duel(30); d.v.pct = pct; d.S.step([B.CR, 0]); const sf = []; for (let i = 0; i < 40; i++) { d.S.step([0, 0]); for (const e of d.S.events) if (e.t === 'sfx' && /^hit_/.test(e.n)) sf.push(e.n); if (sf.length) break; } return sf; };
  const j = duel(30); j.S.step([B.A, 0]); let light = []; for (let i = 0; i < 20 && !light.length; i++) { j.S.step([0, 0]); for (const e of j.S.events) if (e.t === 'sfx' && /^hit_/.test(e.n)) light.push(e.n); }
  assert.deepEqual(light, ['hit_light'], 'jab'); const heavy = grab(170); assert.deepEqual(heavy, ['hit_heavy', 'hit_crit']);
});
test('view: hit sparks scale with strength, KO-range hit zooms in (punch), shake cap 14, launch trail behind a fast victim', () => {
  const v0 = newView(); viewEvent(v0, { t: 'hit', x: 0, y: 0, dmg: 3, kb: 10, lag: 5, dir: 1, str: 0, lx: 1, ly: 0.5 });
  const v2 = newView(); viewEvent(v2, { t: 'hit', x: 0, y: 0, dmg: 15, kb: 200, lag: 16, dir: 1, str: 2, ko: 1, lx: 1, ly: 0.5 });
  assert.ok(v2.fx.length > v0.fx.length + 8, 'more particles'); assert.ok(v2.fx.some((p) => p.kind === 'streak') && !v0.fx.some((p) => p.kind === 'streak')); assert.ok(v2.fx.some((p) => p.kind === 'blast'), 'impact flash');
  assert.ok(v2.punch > 0.1 && v0.punch === 0, 'zoom punch only on a KO-range hit'); assert.ok(v2.shake > v0.shake && v2.shake <= 14);
  assert.equal(v2.hold, 16);
  const S = mk(); const v = newView(); const f = S.fighters[1]; f.state = 'hit'; f.hitlag = 0; f.vx = 7; f.vy = -3; for (let i = 0; i < 4; i++) updateView(v, S); assert.ok(v.fx.some((p) => p.kind === 'trail'), 'launch trail');
  const dec = v2.punch; for (let i = 0; i < 20; i++) updateView(v2, S); assert.ok(v2.punch < dec * 0.2, 'punch decays');
});
test('camera: width clamp 5..12 H, zoom lerp 0.12, fighters fill 22-25 % of the screen height at mid range', () => {
  const S = mk(); const settle = (a, b) => { const v = newView(); S.fighters[0].x = a; S.fighters[1].x = b; for (let i = 0; i < 300; i++) updateView(v, S); return v; };
  near(480 / settle(-10, 10).z, 5 * H, 2, 'min 5 H wide (the 1.5 H vertical margin makes it ~5.04 H)'); near(480 / settle(-500, 500).z, 12 * H, 0.5, 'max 12 H wide');
  const mid = settle(-2.25 * H, 2.25 * H), frac = H * mid.z / 270; assert.ok(frac >= 0.21 && frac <= 0.27, 'H / frame at 4.5 H apart ' + frac);
  const v = newView(); v.snap = true; S.fighters[0].x = -100; S.fighters[1].x = 100; updateView(v, S); v.snap = false; const z0 = v.z; S.fighters[0].x = -300; S.fighters[1].x = 300; updateView(v, S);
  const target = 480 / Math.min(12 * H, 600 + 3 * H); near(v.z - z0, (target - z0) * 0.12, 0.01, 'one step moves 12 % of the way');
});
test('stage still gives seconds of launch: a KO fsmash at 140 % flies >= 1.2 s before the blast zone; weak hits never KO', () => {
  const S = mk(); const v = S.fighters[1]; v.x = 0; v.y = 0; v.grounded = true; v.def = FIGHTERS.pilot;
  const a = launchArc(S.stage, v, 202, 38, 1, 0, 0); assert.ok(a.ko && a.t >= 72, 'flight ' + a.t + ' f'); assert.equal(launchArc(S.stage, v, 60, 38, 1, 0, 0).ko, false);
});
test('sim speed multiplier is a preset field (1.15 FAST local / bot / NPC fights; net lockstep never reads it); the sim itself is speed-agnostic (same hash)', () => {
  assert.equal(PHYS.speed, 1.15);
  const h = () => { const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net' }, { fid: 'swift', ctrl: 'net' }], stageDef: ringStage(SAMPLES, 'sp'), seed: 3 }); for (let i = 0; i < 300; i++) S.step([i % 70 < 30 ? B.R : B.L | B.A, i % 50 < 25 ? B.L : B.R | B.J]); return S.hash(); };
  assert.equal(h(), h());
});

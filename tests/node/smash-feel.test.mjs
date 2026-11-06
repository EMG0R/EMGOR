import test from 'node:test';
import assert from 'node:assert/strict';
import { PHYS, hitlagOf, knockback } from '../../js/smash-phys.js';
import { newView, updateView, viewEvent, ringStage, createMatch, hitPitch } from '../../js/smash-engine.js';
import { FIGHTERS } from '../../js/smash-fighters.js';
import { mk, run, B, SAMPLES } from './smash-arena.mjs';

const H = PHYS.H;
// put fighter 0 at x=0 on the ground facing right and fighter 1 `d` px in front
function duel(d, a, b) { const S = mk(a, b); run(S, 40); const f = S.fighters[0], v = S.fighters[1]; f.x = 0; v.x = d; f.face = 1; v.face = -1; f.vx = v.vx = 0; run(S, 5); return { S, f, v }; }

test('input buffer: a press up to 6 frames before the fighter is free still fires; 8 frames early does not', () => {
  for (const [early, expect] of [[4, 'attack'], [9, 'idle']]) {
    const { S, f } = duel(200);
    f.state = 'land'; f.lag = 10;      // busy for 10 frames
    // press A `early` frames before the lag ends
    let sawAttack = false;
    for (let i = 0; i < 16; i++) { S.step([i === 10 - early ? B.A : 0, 0]); if (f.state === 'attack') sawAttack = true; }
    assert.equal(sawAttack ? 'attack' : 'idle', expect, 'early=' + early);
  }
});
test('c-stick / flick = smash, short hold = tilt, long run = dash attack, no direction = jab', () => {
  const name = (setup) => { const { S, f } = duel(300); const out = setup(S, f); return out || (f.move && f.move.name); };
  assert.equal(name((S, f) => { S.step([B.CR, 0]); return f.move.name; }), 'fsmash');
  assert.equal(name((S, f) => { S.step([B.CU, 0]); return f.move.name; }), 'usmash');
  assert.equal(name((S, f) => { S.step([B.CD, 0]); return f.move.name; }), 'dsmash');
  assert.equal(name((S, f) => { S.step([B.R, 0]); S.step([B.R | B.A, 0]); return f.move.name; }), 'fsmash');       // flick
  assert.equal(name((S, f) => { for (let i = 0; i < 9; i++) S.step([B.R, 0]); S.step([B.R | B.A, 0]); return f.move.name; }), 'ftilt');
  assert.equal(name((S, f) => { for (let i = 0; i < 24; i++) S.step([B.R, 0]); S.step([B.R | B.A, 0]); return f.move.name; }), 'dashatk');
  assert.equal(name((S, f) => { S.step([B.A, 0]); return f.move.name; }), 'jab1');
  assert.equal(name((S, f) => { S.step([B.U | B.A, 0]); return f.move ? f.move.name : 'none'; }), 'utilt');
});
test('smash charge: holding the button freezes the move up to 60 frames, damage x1.4 at full', () => {
  const { S, f, v } = duel(30);
  S.step([B.CR, 0]); const m = f.move; assert.equal(m.name, 'fsmash');
  for (let i = 0; i < 100; i++) S.step([B.CR, 0]);
  assert.equal(f.chgT, PHYS.smashCharge); assert.ok(Math.abs(f.chgMul - PHYS.smashMul) < 1e-9);
  run(S, 40); assert.ok(Math.abs(v.pct - m.hits[0].dmg * 1.4) < 0.01, 'charged dmg ' + v.pct);
  const d2 = duel(30); d2.S.step([B.CR, 0]); for (let i = 0; i < 20; i++) d2.S.step([B.CR, 0]); run(d2.S, 40);
  assert.ok(d2.v.pct > m.hits[0].dmg && d2.v.pct < m.hits[0].dmg * 1.4, 'partial charge ' + d2.v.pct);
});
test('hitlag = floor(dmg/3)+4 on both fighters, shown in the hit event', () => {
  const { S, f, v } = duel(30);
  S.step([B.CR, 0]); let ev = null, lagA = 0, lagV = 0;
  for (let i = 0; i < 40 && !ev; i++) { S.step([0, 0]); ev = S.events.find((e) => e.t === 'hit'); if (ev) { lagA = f.hitlag; lagV = v.hitlag; } }
  assert.ok(ev, 'hit'); assert.equal(ev.lag, hitlagOf(ev.dmg)); assert.equal(lagA, ev.lag); assert.equal(lagV, ev.lag);
  assert.ok(Math.abs(ev.kb - knockback(ev.dmg, v.pct, v.def.weight, f.def.moves.fsmash.hits[0].gr, f.def.moves.fsmash.hits[0].bkb)) < 1e-6, 'kb from the formula');
});
test('freeze: both fighters hold position during hitlag, then the victim launches', () => {
  const { S, f, v } = duel(30);
  S.step([B.CR, 0]); for (let i = 0; i < 40 && !(v.hitlag > 0); i++) S.step([0, 0]);
  const x0 = v.x, y0 = v.y; assert.ok(v.pend, 'pending launch'); const n = v.hitlag;
  for (let i = 0; i < n - 1; i++) { S.step([0, 0]); assert.equal(v.x, x0); assert.equal(v.y, y0); }
  run(S, 3); assert.ok(Math.abs(v.vx) > 0.5 || v.x !== x0, 'launched');
});
test('shield: shieldstun after a blocked hit, shield-break stuns, grab from shield', () => {
  const { S, f, v } = duel(26);
  v.state = 'shield'; v.sh = 180; v.st = 0; S.step([B.A, B.S]); run(S, 12, () => [0, B.S]);
  assert.ok(v.sh < 180, 'shield took damage'); assert.equal(v.pct, 0, 'no damage through the bubble');
  const w = duel(26); w.v.state = 'shield'; w.v.sh = 1; w.S.step([B.CR, B.S]); run(w.S, 40, () => [0, B.S]);
  assert.ok(['stun', 'idle'].includes(w.v.state) && w.v.sh <= 60, 'shield break -> stun');
});
test('sst: shieldstun scales with damage (can not drop the shield while stunned)', () => {
  const { S, f, v } = duel(26);
  v.state = 'shield'; v.sh = 180; S.step([B.CR, B.S]); let max = 0;
  for (let i = 0; i < 30; i++) { S.step([0, B.S]); max = Math.max(max, v.sst); }
  assert.ok(max >= 6, 'smash shieldstun ' + max);
});
test('dodges: spot dodge intangible 2-16, roll 4-19, air dodge 3-30', () => {
  const a = duel(300); a.f.state = 'shield'; a.f.sh = 180; a.f.st = 0;
  a.S.step([B.S | B.D, 0]); assert.equal(a.f.state, 'dodge'); const inv = [];
  for (let i = 0; i < 26; i++) { a.S.step([B.S, 0]); inv.push(a.f.state === 'dodge' ? a.f.inv > 0 : null); }
  assert.ok(inv.slice(2, 15).every(Boolean), 'intangible 2..16'); assert.equal(inv[0], false); assert.ok(inv[24] == null || inv[24] === false);
  const r = duel(300); r.f.state = 'shield'; r.f.sh = 180; r.S.step([B.S | B.R, 0]); assert.equal(r.f.state, 'roll'); const x0 = r.f.x; run(r.S, 20, () => [B.S, 0]); assert.ok(r.f.x - x0 > 30, 'rolled ' + (r.f.x - x0));
  const d = duel(300); d.f.y = -150; d.f.grounded = false; d.f.landed = false; d.f.state = 'air'; d.f.vy = 0; d.S.step([0, 0]); d.S.step([B.S | B.R, 0]);
  assert.equal(d.f.state, 'airdodge'); let iv = []; for (let i = 0; i < 40; i++) { d.S.step([0, 0]); iv.push(d.f.inv > 0); }
  assert.ok(iv.slice(3, 28).every(Boolean), 'air dodge intangible 3..30'); assert.equal(iv[38], false); assert.equal(d.f.helpless, 1, 'special fall after');
});
test('tech: shield pressed before landing in tumble = tech; otherwise knockdown then get up', () => {
  for (const [tech, expect] of [[true, 'tech'], [false, 'down']]) {
    const { S, f, v } = duel(300);
    v.state = 'hit'; v.tumble = true; v.hitstun = 200; v.grounded = false; v.landed = false; v.y = -20; v.vy = 3; v.vx = 0; v.x = 100;
    if (tech) S.step([0, B.S]);
    let seen = ''; for (let i = 0; i < 30; i++) { S.step([0, 0]); if (v.state === 'tech' || v.state === 'down') { seen = v.state; break; } }
    assert.equal(seen, expect);
    run(S, 45); assert.ok(['idle', 'run'].includes(v.state), 'back on feet ' + v.state);
  }
});
test('ledge: grab, then jump / neutral getup / roll / attack / drop', () => {
  const opts = { jump: [B.J], getup: [B.L], roll: [B.S], attack: [B.A], drop: [B.D] };
  for (const o of Object.keys(opts)) {
    const S = mk(); const f = S.fighters[0], L0 = S.ledges.find((l) => l.side === 1);
    S.fighters[1].x = -200; f.x = L0.x + 12; f.y = L0.y + 30; f.vy = 0.5; f.vx = 0; f.grounded = false; f.landed = false; f.state = 'air'; f.jumps = 0;
    let grabbed = false; for (let i = 0; i < 20 && !grabbed; i++) { S.step([0, 0]); grabbed = f.state === 'ledge'; } assert.ok(grabbed, 'grabs the ledge');
    run(S, 10);
    S.step([opts[o][0], 0]); S.step([0, 0]);
    const r = o === 'jump' ? ['air'] : o === 'drop' ? ['air'] : ['getup', 'attack', 'idle'];
    assert.ok(r.includes(f.state), o + ' -> ' + f.state);
    if (o === 'attack') { let atk = false; for (let i = 0; i < 40; i++) { S.step([0, 0]); if (f.move && f.move.name === 'getupatk') atk = true; } assert.ok(atk, 'getup attack'); }
    if (o === 'roll') { run(S, 40); assert.ok(f.x < L0.x - 30, 'rolled onto the stage ' + f.x); }
    if (o === 'getup') { run(S, 40); assert.ok(f.grounded && f.x < L0.x, 'stood up'); }
  }
});
test('ledge grab has intangibility, hanging is capped', () => {
  const S = mk(); const f = S.fighters[0], L0 = S.ledges.find((l) => l.side === -1);
  S.fighters[1].x = 200; f.x = L0.x - 10; f.y = L0.y + 30; f.vy = 0.5; f.grounded = false; f.landed = false; f.state = 'air'; f.jumps = 0;
  for (let i = 0; i < 20 && f.state !== 'ledge'; i++) S.step([0, 0]); run(S, 5); assert.ok(f.inv > 0, 'ledge invuln'); run(S, 140); assert.notEqual(f.state, 'ledge', 'forced drop');
});
test('stage contract: main platform 12 H, soft platforms at 2.2 H, blast +-16 H / +14 H up / -7 H down', () => {
  const st = ringStage(SAMPLES, 'k'), s = st.solids[0];
  assert.equal(s.x1 - s.x0, 12 * H); assert.equal(st.thin.length, 2);
  for (const t of st.thin) { assert.ok(Math.abs(t.y + 2.2 * H) < 0.01); assert.ok(t.x1 - t.x0 >= 2 * H); }
  assert.deepEqual([st.blast.l, st.blast.r, st.blast.t, st.blast.b], [-16 * H, 16 * H, -14 * H, 7 * H]);
  const S = createMatch({ players: [{ fid: 'pilot', ctrl: 'net' }, { fid: 'swift', ctrl: 'net' }], stageDef: st, seed: 1 });
  assert.equal(S.stage.blast.b, 7 * H);
});
test('camera: frames both fighters with 1.5 H margin, width clamps 7..20 H, gentle zoom', () => {
  const S = mk(); const settle = (a, b) => { const v = newView(); S.fighters[0].x = a; S.fighters[1].x = b; for (let i = 0; i < 400; i++) updateView(v, S); return v; };
  const mid = settle(-100, 100); const w = 480 / mid.z; assert.ok(Math.abs(w - (200 + 3 * H)) < 4, 'visible width ' + w);
  assert.ok(Math.abs(480 / settle(-10, 10).z - 7 * H) < 0.5, 'min 7 H wide');
  assert.ok(Math.abs(480 / settle(-500, 500).z - 20 * H) < 0.5, 'max 20 H wide');
  const v = newView(); S.fighters[0].x = -100; S.fighters[1].x = 100; updateView(v, S); const z1 = v.z; S.fighters[1].x = 400; updateView(v, S); assert.ok(Math.abs(v.z - z1) < 0.02, 'zoom eases, no jump');
  // character height vs frame at mid zoom: ~1/8 of the 270 px frame
  const midz = settle(-189, 189).z, frac = H * midz / 270; assert.ok(frac > 0.11 && frac < 0.15, 'H / frame at mid zoom (13.5 H wide) ' + frac);
});
test('hitstop-synced particles, screen shake scaled by knockback, KO flash + 20 f slow', () => {
  const S = mk(); const v = newView();
  viewEvent(v, { t: 'hit', x: 0, y: 0, dmg: 10, kb: 40, lag: 7, dir: 1 }); const sh1 = v.shake;
  assert.equal(v.hold, 7); const p0 = v.fx.map((p) => p.x + ':' + p.y).join(); for (let i = 0; i < 6; i++) updateView(v, S);
  assert.equal(v.fx.map((p) => p.x + ':' + p.y).join(), p0, 'particles frozen during hitlag'); updateView(v, S); updateView(v, S); assert.notEqual(v.fx.map((p) => p.x + ':' + p.y).join(), p0);
  const v2 = newView(); viewEvent(v2, { t: 'hit', x: 0, y: 0, dmg: 10, kb: 160, lag: 7, dir: 1 }); assert.ok(v2.shake > sh1, 'harder hit shakes more');
  const v3 = newView(); viewEvent(v3, { t: 'ko', who: 0, x: 500, y: 0, dx: 1, dy: 0 }); assert.equal(v3.slow, 20); assert.ok(v3.flash > 0); assert.ok(v3.fx.some((p) => p.kind === 'bz'), 'blast-zone flash');
});
test('hit sound pitch falls with damage', () => { assert.ok(hitPitch(2) > hitPitch(8) && hitPitch(8) > hitPitch(25)); });
test('knockback launches heavier fighters less far (same hit, same percent)', () => {
  const dist = (fid) => { const { S, f, v } = duel(30, {}, { fid }); v.pct = 80; S.step([B.CR, 0]); for (let i = 0; i < 160; i++) S.step([0, 0]); return Math.abs(v.x - 30); };
  assert.ok(dist('hum') > dist('hauler') + 20, 'light ' + dist('hum') + ' heavy ' + dist('hauler'));
});
test('KO: leaving the blast zone costs a stock and respawns on the pad', () => {
  const { S, f, v } = duel(300); const st = v.stocks; v.x = 600; run(S, 2); assert.equal(v.stocks, st - 1); assert.equal(v.state, 'dead'); run(S, 60); assert.equal(v.state, 'respawn');
});

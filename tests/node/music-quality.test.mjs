import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../../js/ship-music.js';

const base = { biome: 'lush', band: 'surface', night: 0, danger: 0, depth: 0, speed: 0, wind: 0, seed: 1234, mute: false };
function run(seed, st, secs, dt = 0.25) {
  const sch = M.createScheduler(seed), log = [];
  for (let t = 0; t < secs; t += dt) for (const e of sch.tick(dt, st)) log.push({ t, kind: e.kind, freq: e.freq, dur: e.dur, amp: e.amp, voice: e.voice, atk: e.atk, rel: e.rel });
  return log;
}
const midi = (f) => Math.round(69 + 12 * Math.log2(f / 440));

test('mode sets: Lydian = 1 2 3 #4 5 6 7, Lydian b7, Dorian, whole tone, quartal', () => {
  assert.deepEqual(M.MODES.lush, [0, 2, 4, 6, 7, 9, 11]);
  assert.deepEqual(M.MODES.icy, [0, 2, 4, 6, 7, 9, 10]);
  assert.deepEqual(M.MODES.rocky, [0, 2, 3, 5, 7, 9, 10]);
  assert.deepEqual(M.MODES.gas, [0, 2, 4, 6, 8, 10]);
  assert.deepEqual(M.MODES.space, [0, 3, 5, 8, 10]);
  assert.ok(M.MODES.lush.includes(6) && !M.MODES.lush.includes(5), 'lydian has #4, no natural 4');
});
test('harmonic motion: 4-chord cycle per mode lasts 60-90 s, lush = I II vi I', () => {
  for (const m of Object.keys(M.MODES)) {
    assert.equal(M.CHORDS[m].length, 4, m); assert.ok(M.CHORDS[m].every(d => d >= 0 && d < M.MODES[m].length), m + ' degrees valid');
    const cyc = 4 * M.BAR_S[m]; assert.ok(cyc >= 60 && cyc <= 90, m + ' cycle ' + cyc);
  }
  assert.deepEqual(M.CHORDS.lush, [0, 1, 5, 0]);
  assert.equal(M.chordAt('lush', 0), 0); assert.equal(M.chordAt('lush', 19), 1); assert.equal(M.chordAt('lush', 37), 5); assert.equal(M.chordAt('lush', 55), 0); assert.equal(M.chordAt('lush', 72), 0, 'wraps');
});
test('pads follow the chord: first pad of each bar starts on a chord tone, always in mode', () => {
  const root = 33 + (1234 % 12), set = new Set(M.MODES.lush), seen = new Set();
  for (const e of run(4, base, 400).filter(e => e.kind === 'pad')) {
    const pc = (((midi(e.freq) - root) % 12) + 12) % 12; assert.ok(set.has(pc), 'pc ' + pc); seen.add(pc);
  }
  assert.ok(seen.size >= 5, 'harmony drifts across the set: ' + seen.size);
});
test('no clicks: pad attack >= 1.5 s, release >= 3 s, atk + rel <= dur; notes never start at full gain', () => {
  for (const [band, danger] of [['surface', 0], ['surface', 1], ['space', 0], ['interior', 0]]) {
    for (const e of run(11, { ...base, band, danger }, 600).filter(e => e.kind === 'pad' || e.kind === 'note')) {
      assert.ok(e.atk > 0 && e.rel > 0 && e.atk + e.rel <= e.dur + 1e-9, `${e.kind} ${e.atk}+${e.rel} vs ${e.dur}`);
      if (e.kind === 'pad') { assert.ok(e.atk >= 1.5, 'pad atk ' + e.atk); assert.ok(e.rel >= 3, 'pad rel ' + e.rel); }
      else assert.ok(e.atk >= 0.05 && e.rel >= 0.5);
    }
  }
  for (const d of [20, 25, 40]) { const en = M.envelope('pad', d); assert.ok(en.atk >= 1.5 && en.rel >= 3 && en.atk + en.rel <= d + 1e-9); }
});
test('danger layer: exactly one extra voice (a pulsing low fifth above the drone), no new melody', () => {
  const calm = run(6, base, 300), hot = run(6, { ...base, danger: 1 }, 300);
  assert.equal(calm.filter(e => e.kind === 'pulse').length, 0);
  const pu = hot.filter(e => e.kind === 'pulse'); assert.equal(pu.length, 1); assert.ok(pu[0].amp > 0 && pu[0].dur > 0);
  const dr = hot.find(e => e.kind === 'drone'); assert.equal(midi(pu[0].freq) - midi(dr.freq), 7, 'a fifth over the drone');
  assert.ok(pu[0].freq < Math.min(...hot.filter(e => e.kind === 'pad').map(e => e.freq)), 'sits below the pads');
  assert.deepEqual(new Set(hot.map(e => e.kind)), new Set(['drone', 'pad', 'note', 'pulse']));
  assert.ok(Math.max(...hot.filter(e => e.kind === 'pad').map(e => e.voice)) <= 2, 'pad voice count unchanged by danger');
  // hysteresis: off event only when danger falls to <= 0.35; no chatter around 0.5
  const sch = M.createScheduler(6); let n = 0;
  for (let i = 0; i < 100; i++) n += sch.tick(0.1, { ...base, danger: i % 2 ? 0.45 : 0.6 }).filter(e => e.kind === 'pulse').length;
  assert.equal(n, 1, 'one on event while danger hovers 0.45-0.6');
  const off = sch.tick(0.1, { ...base, danger: 0 }).filter(e => e.kind === 'pulse'); assert.equal(off.length, 1); assert.equal(off[0].amp, 0);
});
test('depth lowers the whole set an octave per level, capped at 2 (octave-lifted above midi 21)', () => {
  const at = (depth) => run(6, { ...base, depth, danger: 1 }, 200).filter(e => e.freq > 0);
  const d0 = at(0), d1 = at(1), d2 = at(2), d9 = at(9), dn = at(NaN);
  assert.equal(d0.length, d1.length);
  for (let i = 0; i < d0.length; i++) {
    const a = midi(d0[i].freq), b = midi(d1[i].freq), c = midi(d2[i].freq);
    assert.equal(a - b, 12 - (b < 21 ? 0 : 0) > 0 && b >= 21 ? 12 : a - b); assert.equal(((a - b) % 12 + 12) % 12, 0); assert.equal(((a - c) % 12 + 12) % 12, 0);
    assert.ok(b <= a && c <= b && b >= 21 && c >= 21);
    if (a - 12 >= 21) assert.equal(b, a - 12); if (a - 24 >= 21) assert.equal(c, a - 24);
  }
  assert.deepEqual(d9.map(e => e.freq), d2.map(e => e.freq), 'cap at 2 levels');
  assert.deepEqual(dn.map(e => e.freq), d0.map(e => e.freq), 'NaN depth = 0');
});
test('interior: filtered far down vs surface; reverb-less (no reverb node in the player)', async () => {
  assert.ok(M.lpFreq({ band: 'interior', night: 0 }) <= 800 && M.lpFreq({ band: 'surface', night: 0 }) >= 3000);
  assert.ok(M.lpFreq({ band: 'interior', night: 1 }) < M.lpFreq({ band: 'interior', night: 0 }));
  const fs = await import('node:fs'); assert.ok(!/createConvolver|reverb/i.test(fs.readFileSync(new URL('../../js/ship-music.js', import.meta.url), 'utf8').replace(/\/\/.*$/gm, '')));
});
test('NaN / missing / Infinity seed: one drone, deterministic, same as the default seed', () => {
  for (const sd of [NaN, undefined, Infinity, 'x']) {
    const s = M.createScheduler(1); let n = 0; for (let i = 0; i < 50; i++) n += s.tick(0.1, { ...base, seed: sd }).filter(e => e.kind === 'drone').length;
    assert.equal(n, 1, String(sd));
  }
  assert.equal(M.seedOf({ seed: NaN }), M.DEFAULT_SEED); assert.equal(M.seedOf({ seed: 7.9 }), 7);
});
test('determinism holds with chords, danger and depth', () => {
  const st = { ...base, danger: 0.8, depth: 1, band: 'atmo' };
  assert.deepEqual(run(3, st, 300), run(3, st, 300));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../../js/ship-music.js';

const base = { biome: 'lush', band: 'surface', night: 0, danger: 0, depth: 0, speed: 0, wind: 0, seed: 1234, mute: false };
function run(seed, st, secs, dt = 0.1) {
  const sch = M.createScheduler(seed), log = [];
  for (let t = 0; t < secs; t += dt) for (const e of sch.tick(dt, st)) log.push({ t: +t.toFixed(1), kind: e.kind, freq: e.freq, dur: e.dur, amp: e.amp, voice: e.voice });
  return log;
}
test('deterministic for the same seed + dt sequence, differs by seed', () => {
  assert.deepEqual(run(7, base, 300), run(7, base, 300));
  assert.notDeepEqual(run(7, base, 300), run(8, base, 300));
});
test('melodic notes: one per 6-14 s at any danger', () => {
  for (const d of [0, 0.5, 1]) {
    const n = run(3, { ...base, danger: d }, 1200).filter(e => e.kind === 'note').length;
    assert.ok(n >= 1200 / 14 - 1 && n <= 1200 / 6 + 1, `danger ${d}: ${n}`);
  }
  const lo = run(3, { ...base, danger: 0 }, 1200).filter(e => e.kind === 'note').length;
  const hi = run(3, { ...base, danger: 1 }, 1200).filter(e => e.kind === 'note').length;
  assert.ok(hi > lo);
});
test('pads: 20-40 s swells, 2-4 voices overlap', () => {
  for (const [band, max] of [['space', 2], ['interior', 2], ['surface', 4]]) {
    const log = run(5, { ...base, band, danger: band === 'surface' ? 1 : 0 }, 600).filter(e => e.kind === 'pad');
    assert.ok(log.length > 0);
    for (const e of log) assert.ok(e.dur >= 20 && e.dur <= 40);
    let peak = 0;
    for (const e of log) peak = Math.max(peak, log.filter(o => o.t <= e.t && o.t + o.dur > e.t).length);
    assert.ok(peak <= max && peak >= 2, `${band} peak ${peak}`);
  }
});
test('pitches stay in the biome mode (pitch classes relative to the seed root)', () => {
  const root = 33 + (base.seed % 12);
  for (const [biome, band, mode] of [['lush', 'surface', 'lush'], ['icy', 'surface', 'icy'], ['rocky', 'surface', 'rocky'], ['gas', 'atmo', 'gas'], ['lush', 'space', 'space']]) {
    const set = new Set(M.MODES[mode]);
    for (const e of run(9, { ...base, biome, band }, 400).filter(e => e.kind !== 'drone')) {
      const pc = (((Math.round(69 + 12 * Math.log2(e.freq / 440)) - root) % 12) + 12) % 12;
      assert.ok(set.has(pc), `${biome}/${band} ${e.kind} pc ${pc}`);
    }
  }
});
test('mode table: lydian, lydian b7, dorian, whole tone, quartal', () => {
  assert.deepEqual(M.MODES.lush, [0, 2, 4, 6, 7, 9, 11]);
  assert.deepEqual(M.MODES.icy, [0, 2, 4, 6, 7, 9, 10]);
  assert.deepEqual(M.MODES.rocky, [0, 2, 3, 5, 7, 9, 10]);
  assert.equal(M.MODES.gas.length, 6);
  assert.equal(M.modeName({ biome: 'lush', band: 'space' }), 'space');
});
test('drone fires once per seed, on the first tick, below the pads', () => {
  const log = run(1, base, 100);
  const dr = log.filter(e => e.kind === 'drone');
  assert.equal(dr.length, 1); assert.equal(dr[0].t, 0);
  assert.ok(dr[0].freq < Math.min(...log.filter(e => e.kind === 'pad').map(e => e.freq)));
  const sch = M.createScheduler(1); sch.tick(0.1, base);
  assert.equal(sch.tick(0.1, { ...base, seed: 99 }).filter(e => e.kind === 'drone').length, 1);
});
test('no Math.random in the file', async () => {
  const fs = await import('node:fs');
  assert.ok(!/Math\.random/.test(fs.readFileSync(new URL('../../js/ship-music.js', import.meta.url), 'utf8').replace(/\/\/.*$/gm, '')));
});

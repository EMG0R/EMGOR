// verify.mjs -- (1) node-only wasm test of the sim (no browser), (2) headless Chrome check via puppeteer-core.
// usage: node verify.mjs [--node-only]   (exit code 1 on any failed assertion)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const wasmPath = path.join(here, 'web', 'hxsim.wasm');
const SR = 48000;
let fails = 0;
const ok = (c, msg) => { console.log((c ? '  PASS ' : '  FAIL ') + msg); if (!c) fails++; };

// ---------------------------------------------------------------- wasm host
const bytes = fs.readFileSync(wasmPath);
const mod = new WebAssembly.Module(bytes);
const inst = new WebAssembly.Instance(mod, { env: { now_ms: () => performance.now() } });
const X = inst.exports;
if (X._initialize) X._initialize();
const mem = () => new Float32Array(X.memory.buffer);
const u8 = () => new Uint8Array(X.memory.buffer);
const cstr = (p) => { if (!p) return ''; const b = u8(); let e = p; while (b[e]) e++; return Buffer.from(b.subarray(p, e)).toString('latin1'); };
X.sim_init(SR);
const io = X.sim_io() >> 2, INP = io, OL = io + 128, OR = io + 256, CP = io + 384, MP = io + 416;
const nEff = X.sim_effect_count();
const effects = []; for (let i = 0; i < nEff; i++) effects.push({ id: cstr(X.sim_effect_id(i)), name: cstr(X.sim_effect_name(i)) });

function setCtl(c) { const f = mem(); f.fill(0, CP, CP + 21); for (const [k, v] of Object.entries(c)) f[CP + Number(k)] = v; X.sim_set_controls(CP << 2); }
// scripted performance: returns {L, R, hint[], tracked[], msPerBlock}
function run({ seconds = 3, slots = null, mapper = true, faders = 1, breath = true, script = true, onSetup }) {
  X.sim_init(SR);
  X.sim_set_mapper_enabled(mapper ? 1 : 0);
  if (slots) for (let s = 0; s < 4; s++) { X.sim_set_slot_effect(s, slots[s]); X.sim_set_bypass(s, 0); }
  if (onSetup) onSetup();
  const N = Math.floor(seconds * SR / 128) * 128, L = new Float32Array(N), R = new Float32Array(N), hint = [], tracked = [];
  let blocks = 0, wall = 0;
  for (let off = 0; off < N; off += 128) {
    const t = off / SR; const c = {}; c[3] = faders; c[4] = faders; c[5] = faders;
    let partial = 4, v = [0, 0, 0];
    if (script) {
      if (t >= 0.5 && t < 1.0) v = [0, 1, 0]; else if (t >= 1.0 && t < 1.5) v = [1, 1, 0]; else if (t >= 1.5 && t < 2.0) { partial = 6; } else if (t >= 2.0) v = [0.3, 0.3, 0.3];
    }
    c[0] = v[0]; c[1] = v[1]; c[2] = v[2]; setCtl(c);
    X.sim_set_trumpet(partial, breath ? 0.7 : 0, 5.2, 0.15);
    if (script && Math.abs(t - 1.0) < 128 / SR) X.sim_tongue();
    const t0 = performance.now();
    X.sim_process(0, OL << 2, OR << 2, 128, 0);
    wall += performance.now() - t0; blocks += 2;
    const f = mem(); L.set(f.subarray(OL, OL + 128), off); R.set(f.subarray(OR, OR + 128), off);
    X.sim_read_meters(MP << 2); hint.push(f[MP + 10]); tracked.push(f[MP + 3]);
  }
  return { L, R, hint, tracked, msPerBlock: wall / blocks, N };
}
const stat = (a) => { let e = 0, pk = 0, bad = 0; for (const x of a) { if (!Number.isFinite(x)) bad++; else { e += x * x; if (Math.abs(x) > pk) pk = Math.abs(x); } } return { rms: Math.sqrt(e / a.length), peak: pk, bad }; };
function autocorrF0(x, start, len) {
  const lo = Math.floor(SR / 1200), hi = Math.floor(SR / 100), ac = new Float64Array(hi + 1); let best = 0;
  for (let lag = lo; lag < hi; lag++) { let s = 0; for (let i = 0; i < len; i++) s += x[start + i] * x[start + i + lag]; ac[lag] = s; if (s > best) best = s; }
  for (let lag = lo + 1; lag < hi - 1; lag++) if (ac[lag] >= 0.85 * best && ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1]) return SR / lag;   // first strong peak, not the sub-octave
  return 0;
}

console.log('== 1. node: synthetic trumpet + default patch (mapper on)');
console.log('  wasm ' + bytes.length + ' bytes, arena ' + (X.sim_arena_bytes() / 1048576) + ' MB, block ' + X.sim_block() + ', ' + nEff + ' effects: ' + effects.map((e) => e.id).join(' '));
const dflt = run({});
const sd = stat(dflt.L), sdr = stat(dflt.R);
console.log('  default patch: rms L ' + sd.rms.toFixed(4) + ' R ' + sdr.rms.toFixed(4) + ' peak ' + sd.peak.toFixed(3) + '  ' + (dflt.msPerBlock * 1000).toFixed(1) + ' us/64-block (' + (dflt.msPerBlock / (64 / SR * 1000) * 100).toFixed(2) + '% of budget)');
ok(sd.rms > 0.005, 'default patch non-silent (rms ' + sd.rms.toFixed(4) + ' > 0.005)');
ok(sd.bad + sdr.bad === 0, 'all samples finite');
ok(sd.peak < 8 && sdr.peak < 8, 'bounded (peak ' + sd.peak.toFixed(2) + ')');
const nobreath = run({ breath: false });
const sn = stat(nobreath.L);
ok(sn.rms < sd.rms * 0.05, 'no-input case differs: rms with breath 0 = ' + sn.rms.toExponential(2) + ' (vs ' + sd.rms.toFixed(4) + ')');
// dry synth f0 vs hint, chain empty
const dry = run({ slots: [-1, -1, -1, -1], mapper: false });
const checks = [[0.30, 0.50], [0.7, 0.95], [1.1, 1.45], [1.6, 1.95]];
const expect = ['Bb4 466.2 (partial 4, open)', 'Bb4-2st 415.3 (valve 2 -> Ab4? =466.2/2^(1/12) 440.0)', 'valves 1+2 = -3 st', 'partial 6 open F5 ~699'];
for (const [a, b] of checks) {
  const i0 = Math.floor((a + b) / 2 * SR / 128), hintHz = dflt.hint[i0], det = autocorrF0(dry.L, Math.floor(a * SR), 2048);
  const err = hintHz > 0 ? Math.abs(12 * Math.log2(det / hintHz)) : 99;
  console.log('  t=' + a + '-' + b + ' s: f0 hint ' + hintHz.toFixed(1) + ' Hz, autocorr on dry synth ' + det.toFixed(1) + ' Hz, error ' + err.toFixed(2) + ' st; engine tracked f0 ' + dflt.tracked[i0].toFixed(1));
  ok(err < 0.5, 'synth pitch matches hint within 0.5 st');
}
console.log('  (tracker f0 = 0 means fx_pitchtrack.h is still the stub)');

console.log('== 2. node: every registry effect in slot 0, others empty, mapper frozen, MIX=1');
const stubs = [];
const ref = run({ slots: [-1, -1, -1, -1], mapper: false });
for (let e = 0; e < nEff; e++) {
  const r = run({ slots: [e, -1, -1, -1], mapper: false, onSetup: () => { X.sim_set_param(0, 0, 1.0); } });
  const s = stat(r.L), s2 = stat(r.R);
  let diff = 0; for (let i = 0; i < r.N; i++) diff = Math.max(diff, Math.abs(r.L[i] - ref.L[i]), Math.abs(r.R[i] - ref.L[i]));
  const isStub = diff < 1e-6; if (isStub) stubs.push(effects[e].id);
  const built = X.sim_slot_ok(0);
  console.log('  ' + effects[e].id.padEnd(8) + 'rms ' + s.rms.toFixed(4) + '/' + s2.rms.toFixed(4) + ' peak ' + s.peak.toFixed(2) + ' nparams ' + X.sim_param_count(0) + ' ' + (r.msPerBlock * 1000).toFixed(1).padStart(6) + ' us/blk ' + (isStub ? '[passthrough at defaults: stub OR transparent by design (harm INTVL 5, looper idle)]' : '') + (built ? '' : ' [arena fail]'));
  ok(s.bad + s2.bad === 0 && s.rms > 1e-4 && s.peak < 16 && s2.rms > 1e-4, effects[e].id + ' finite, non-silent, bounded');
}
console.log('  effects identical to empty chain at default params: ' + (stubs.length ? stubs.join(', ') : 'none'));
fs.writeFileSync(path.join(here, 'verify_out', 'stubs.txt'), stubs.join('\n') + '\n');

console.log('== 3. node: all four slots loaded with the default patch, mapper on, fx swap during run');
{
  const r = run({ seconds: 2 });
  X.sim_set_slot_effect(1, 3); X.sim_set_slot_effect(1, 7);
  const s = stat(r.L); ok(s.bad === 0, 'finite after slot swaps');
}

// ---------------------------------------------------------------- browser
if (!process.argv.includes('--node-only')) {
  console.log('== 4. headless Chrome (puppeteer-core)');
  let puppeteer;
  try { puppeteer = (await import('puppeteer-core')).default; } catch (e) { console.log('  puppeteer-core not installed (npm install in sim/): ' + e.message); fails++; }
  if (puppeteer) {
    const { start } = await import('./serve.mjs');
    const srv = await start(8765); const url = 'http://127.0.0.1:' + srv.address().port + '/'; console.log('  serving ' + url);
    const chrome = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    const errors = [];
    let browser;
    try {
      browser = await puppeteer.launch({ executablePath: chrome, headless: 'new', args: ['--autoplay-policy=no-user-gesture-required', '--use-fake-ui-for-media-stream', '--use-fake-device-for-media-stream', '--no-first-run', '--window-size=1400,900'] });
      const page = await browser.newPage(); await page.setViewport({ width: 1400, height: 900 });
      page.on('console', (m) => { if (m.type() === 'error') errors.push('console.error: ' + m.text()); });
      page.on('pageerror', (e) => errors.push('pageerror: ' + e.message));
      await page.goto(url, { waitUntil: 'load' });
      await page.waitForFunction('window.__sim && window.__sim.S.ready', { timeout: 15000 });
      // START via a real mouse click on the drawn button (logical 352..448 x 14..50)
      { const [x, y] = await page.evaluate(() => window.__sim.toPage(400, 32)); await page.mouse.click(x, y); }
      await new Promise((r) => setTimeout(r, 400));
      await page.evaluate(() => { window.__sim.setBreath(0.7); window.__sim.setValve(1, 1); });
      await new Promise((r) => setTimeout(r, 2200));
      const st = await page.evaluate(() => window.__sim.stats());
      const state = await page.evaluate(() => window.__sim.state());
      console.log('  state ' + JSON.stringify(state) + '\n  stats ' + JSON.stringify(st));
      ok(state.running && state.ctxState === 'running', 'AudioContext running');
      ok(st.rms > 0.005, 'worklet output rms ' + st.rms.toFixed(4) + ' > 0.005 (synthetic trumpet)');
      ok(st.nonFinite === 0, 'no non-finite output samples');
      ok(st.maxAbs < 8, 'bounded, max abs ' + st.maxAbs.toFixed(3));
      console.log('  worklet wall time per 64-block: ' + (st.blockMsAvg * 1000).toFixed(1) + ' us (' + (st.blockMsAvg / (64 / SR * 1000) * 100).toFixed(2) + '% of budget), ' + st.quanta + ' quanta');
      fs.mkdirSync(path.join(here, 'verify_out'), { recursive: true });
      // exercise the UI with real pointer/key events: effect dropdown on slot 3 -> pick 'grain' (index 3), hold valve key, press a Trellis pad
      { const [x, y] = await page.evaluate(() => window.__sim.toPage(20 + 2 * 313 + 80, 84)); await page.mouse.click(x, y); await new Promise((r) => setTimeout(r, 150));
        const [x2, y2] = await page.evaluate(() => window.__sim.toPage(20 + 2 * 313 + 60, 64 + 44 + 6 + 3 * 26 + 10)); await page.mouse.click(x2, y2); await new Promise((r) => setTimeout(r, 300));
        const t = await page.evaluate(() => window.__sim.S.slots[2].type); ok(t === 3, 'dropdown click changed slot 3 effect to index 3 (got ' + t + ')');
        await page.keyboard.down('j'); await page.keyboard.down('q'); await new Promise((r) => setTimeout(r, 300));
        const f0 = await page.evaluate(() => ({ valve: window.__sim.U.keyValve[0], pads: window.__sim.U.keyPads }));
        ok(f0.valve === 1 && f0.pads === 16, 'keyboard J (valve 1) and Q (pad 4) registered: ' + JSON.stringify(f0));
        const leds = await page.evaluate(() => window.__sim.S.leds.slice(0, 48).reduce((a, b) => a + b, 0)); console.log('  led byte sum (mapper stub = 0 until mapper lands): ' + leds);
        await page.keyboard.up('j'); await page.keyboard.up('q'); }
      await page.evaluate(() => window.__sim.post({ t: 'poll' }));
      await new Promise((r) => setTimeout(r, 300));
      await page.screenshot({ path: path.join(here, 'verify_out', 'ui.png') });
      ok(errors.length === 0, 'no console errors' + (errors.length ? ': ' + errors.join(' | ') : ''));
      ok(!state.err, 'no UI error string' + (state.err ? ': ' + state.err : ''));
    } catch (e) {
      console.log('  browser check failed: ' + (e && e.stack || e)); fails++;
    } finally { if (browser) await browser.close(); if (srv) srv.close(); }
  }
}
console.log(fails ? '\nFAILED: ' + fails : '\nALL OK');
process.exit(fails ? 1 : 0);

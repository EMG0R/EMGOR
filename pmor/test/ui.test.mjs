// PMOR UI tests: state machine against a stub canvas context and a fake Engine.
// Run: node pmor/test/ui.test.mjs
import {
  createPmorUI, renderOled, ledFrame, lavaColor, makeFb, text, blankTick, computeLayout, phaseAt, MENU,
} from '../ui.js';

let passed = 0, failed = 0;
const ok = (name, cond, extra) => {
  if (cond) { passed++; console.log('ok   ' + name); } else { failed++; console.log('FAIL ' + name + (extra !== undefined ? '  ' + JSON.stringify(extra) : '')); }
};
const eq = (name, a, b) => ok(name, JSON.stringify(a) === JSON.stringify(b), { got: a, want: b });

// ---- fakes
function makeCtx() {
  const calls = [];
  const ctx = new Proxy({}, {
    get(t, k) { if (k === 'calls') return calls; if (k in t) return t[k]; return (...a) => { calls.push([k, ...a]); }; },
    set(t, k, v) { t[k] = v; return true; },
  });
  return ctx;
}
function makeEngine() {
  const calls = [];
  let n = 0;
  const e = {
    onTick: null, calls,
    start: () => { calls.push(['start']); return Promise.resolve(); },
    getPersist: () => { calls.push(['getPersist']); return Promise.resolve({ snap: ++n }); },
    defaults: () => ({ def: true }),
  };
  for (const m of ['noteOn', 'noteOff', 'drumHit', 'samplerPress', 'samplerRelease', 'fxBank', 'fxHeld', 'fxRecord', 'fxClearAuto', 'synMode', 'synArm', 'synPlace',
    'synLiveWrite', 'synClear', 'toggleStep', 'clearPattern', 'selectDrum', 'selectSlot', 'set', 'adjust', 'tap', 'allKeysOff', 'setPersist', 'suspend', 'resume']) {
    e[m] = (...a) => { calls.push([m, ...a]); };
  }
  e.of = (name) => calls.filter((c) => c[0] === name);
  e.clear = () => { calls.length = 0; };
  return e;
}
function makeEnv(storage) {
  let t = 100000;
  const timerList = new Map();
  let tid = 0;
  const timers = { set: (f, ms) => { timerList.set(++tid, { f, at: t + ms }); return tid; }, clear: (id) => timerList.delete(id) };
  const engine = makeEngine();
  const ctx = makeCtx();
  const canvas = { getContext: () => ctx };
  const store = storage || (() => { const m = new Map(); return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, v), m }; })();
  const ui = createPmorUI({ canvas, engine, storage: store, now: () => t, timers });
  ui.resize(390, 844, 3);
  const env = {
    ui, engine, ctx, store, timers: timerList,
    adv: (ms) => { t += ms; ui.update(); },
    set time(v) { t = v; }, get time() { return t; },
    tick: (o = {}) => { const k = Object.assign(blankTick(), o); ui.onTick(k); return k; },
  };
  return env;
}
const flush = async (ui) => { for (let i = 0; i < 6; i++) { await Promise.resolve(); await ui.flush(); } };

async function started(storage) {
  const env = makeEnv(storage);
  await env.ui.tapStart();
  await flush(env.ui);
  env.adv(300);
  env.tick();
  return env;
}

// ---- gesture helpers (CSS px = device px / 3)
const D = 3;
const padXY = (ui, k) => { const p = ui.layout().pads[k]; return [(p.x + p.w / 2) / D, (p.y + p.h / 2) / D]; };
const encXY = (ui, i) => { const e = ui.layout().enc[i]; return [e.cx / D, e.cy / D]; };
let pid = 100;
function padTap(env, k, holdMs = 20, info) {
  const id = ++pid, [x, y] = padXY(env.ui, k);
  env.ui.pointerDown(id, x, y, info); env.adv(holdMs); env.ui.pointerUp(id);
}
function padDown(env, k, info) { const id = ++pid, [x, y] = padXY(env.ui, k); env.ui.pointerDown(id, x, y, info); return id; }
function rotate(env, i, detents) {
  const id = ++pid, [x, y] = encXY(env.ui, i), dir = detents > 0 ? -1 : 1; // finger up = +
  env.ui.pointerDown(id, x, y); env.ui.pointerMove(id, x, y + dir * 10);
  let yy = y + dir * 10;
  for (let d = 0; d < Math.abs(detents); d++) { yy += dir * 20; env.ui.pointerMove(id, x, yy); }
  env.ui.pointerUp(id);
}
function encTap(env, i) { // returns after the 130 ms right-click deferral
  const id = ++pid, [x, y] = encXY(env.ui, i);
  env.ui.pointerDown(id, x, y); env.adv(30); env.ui.pointerUp(id);
  env.adv(140);
}
const gotoPage = (env, p) => { const d = (p - env.ui.S.menu + 7) % 7; rotate(env, 0, d); env.adv(500); };

// text assertions on the 128x64 buffer
function hasText(fb, x, y, str, inverted = false) {
  const ref = makeFb(); text(ref, x, y, str, true);
  for (let yy = y; yy < y + 8; yy++) for (let xx = x; xx < x + str.length * 6; xx++) {
    const a = fb[yy * 128 + xx], b = ref[yy * 128 + xx];
    if (inverted ? a === b : a !== b) return false;
  }
  return true;
}
const view = (env) => env.ui.view();
const oled = (env) => renderOled(makeFb(), view(env));

// =============================================================================================
console.log('-- primitives');
{
  const fb = makeFb(); text(fb, 0, 0, '!', true);
  ok('font: "!" is the libDaisy glyph (column 2, rows 0-4 and 6)', [0, 1, 2, 3, 4, 6].every((r) => fb[r * 128 + 2] === 1) && fb[5 * 128 + 2] === 0 && fb[7 * 128 + 2] === 0);
  const f2 = makeFb(); text(f2, 0, 0, 'A\x11B', true);
  ok('text aborts on a char below 32 (libDaisy WriteString)', f2[6 * 0 + 2] === 1 && !hasText(f2, 6, 0, 'B') );
  const f3 = makeFb(); text(f3, 124, 0, 'AB', true);
  ok('text refuses a glyph that does not fit', f3.every((v) => v === 0));
  eq('lava key 0', lavaColor(0), [70, 150, 255]);
  eq('lava key 3 (warm yellow)', lavaColor(96), [255, 215, 65]);
  eq('lava midway 0->1 (f=16)', lavaColor(16), [70 + ((75 - 70) * 16 >> 5), 150 + ((175 - 150) * 16 >> 5), 255 + ((240 - 255) * 16 >> 5)]);
  eq('pad phase init 16*i, drift per 80 ms tick', [phaseAt(3, 0), phaseAt(3, 10), phaseAt(0, 10)], [48, 48 + 20, 10]);
}

console.log('-- layout');
{
  for (const [w, h, dpr] of [[390, 844, 3], [360, 640, 2], [820, 1180, 2], [1280, 720, 1], [320, 480, 2]]) {
    const L = computeLayout(w, h, dpr);
    const inside = L.pads.every((p) => p.x >= 0 && p.y >= 0 && p.x + p.w <= L.W && p.y + p.h <= L.H);
    ok('layout ' + w + 'x' + h + ': OLED integer scale ' + L.scale + ', pads fit', Number.isInteger(L.scale) && L.scale >= 1 && inside && L.glass.w === 128 * L.scale && L.glass.h === 64 * L.scale,
      { y: L.pads[15].y + L.pads[15].h, H: L.H });
    ok('layout ' + w + 'x' + h + ': stack order strip < oled < encoders < pads', L.strip.y < L.oledBlock.y && L.oledBlock.y + L.oledBlock.h < L.enc[0].cy - L.enc[0].r && L.enc[0].cy + L.enc[0].r < L.pads[0].y);
  }
}

console.log('-- boot');
{
  const env = makeEnv();
  ok('idle splash: no timers, nothing started', env.timers.size === 0 && env.engine.of('start').length === 0 && env.ui.phase() === 'idle');
  const sp = oled(env);
  ok('splash shows TAP TO START', hasText(sp, 28, 34, 'TAP TO START'));
  const p = env.ui.tapStart();
  ok('engine.start() called synchronously inside the gesture', env.engine.of('start').length === 1);
  await p; await flush(env.ui);
  ok('boot intro phase after start', env.ui.phase() === 'boot');
  ok('mvol/lid/ax initialised', env.engine.of('set').some((c) => c[1] === 'mvol_idx' && c[2] === 9));
  const L1 = env.ui.view(); ok('intro frame index', L1.bootFrame === 0);
  env.adv(250);
  ok('run phase after 240 ms of intro', env.ui.phase() === 'run');
  const intro = makeFb();
  renderOled(intro, Object.assign(env.ui.view(), { bootFrame: 6, splash: null, animStep: 0, vol_mode: false }));
  ok('intro draws POCKETGORATOR from frame 4', hasText(intro, 25, 29, 'POCKETGORATOR'));
}

console.log('-- patches storage');
{
  const env = await started();
  const st = JSON.parse(env.store.m.get('pmor.patches.v1'));
  ok('storage key written, 16 slots', st.v === 1 && st.slots.length === 16);
  ok('example is captured with the master muted', env.engine.calls.findIndex((c) => c[0] === 'set' && c[1] === 'mvol_idx' && c[2] === 0) < env.engine.calls.findIndex((c) => c[0] === 'toggleStep'));
  ok('example planted in slot 1 as DRUMS page', st.slots[1] && st.slots[1].page === MENU.DRUM && st.exists === 3 && st.planted === true);
  const tg = env.engine.of('toggleStep');
  ok('example: kick 0,4,8,12 on voice 0; voice 1 steps 4,12; voice 2 even steps', JSON.stringify(tg.slice(0, 4).map((c) => c[3])) === '[0,4,8,12]' && tg.slice(4, 6).every((c) => c[2] === 1) && tg.length === 14);
  const env2 = await started(env.store);
  ok('second boot with same storage plants nothing', env2.engine.of('toggleStep').length === 0);
  ok('second boot loads the stored active slot', env2.engine.of('setPersist').length === 1);
  const bad = { getItem: () => '{nope', setItem() {} };
  const env3 = await started(bad);
  ok('corrupt storage is survivable', env3.ui.phase() === 'run');
}

console.log('-- navigation');
{
  const env = await started();
  const seen = [env.ui.S.menu];
  for (let i = 0; i < 7; i++) { rotate(env, 0, 1); env.adv(500); seen.push(env.ui.S.menu); }
  eq('left-rotate visits all 7 pages and wraps', seen, [0, 1, 2, 3, 4, 5, 6, 0]);
  rotate(env, 0, -1); env.adv(500);
  eq('left-rotate backwards wraps to GLOBALS', env.ui.S.menu, 6);
  gotoPage(env, 0);

  // dive animation
  rotate(env, 0, 1);
  ok('dive animation starts on page change', env.ui.state().animStart > 0);
  let v = view(env);
  ok('anim frame 1 draws the page name centred at y=29', v.animStep === 1 && hasText(renderOled(makeFb(), v), 64 - 5 * 3, 29, 'DRUMS'));
  env.adv(70); ok('anim step 2 after 66 ms', view(env).animStep === 2);
  env.adv(66 * 5); ok('anim over after 6 frames', view(env).animStep === 0 && env.ui.state().animStart < 0);
  ok('anim loop stops: no timer once idle (save debounce aside)', [...env.timers.values()].length <= 1);

  // 4 counts per detent
  const id = ++pid, [x, y] = encXY(env.ui, 0);
  const m0 = env.ui.S.menu;
  env.ui.pointerDown(id, x, y); env.ui.pointerMove(id, x, y - 10);
  env.ui.pointerMove(id, x, y - 10 - 14);
  eq('3 counts: no page change yet', env.ui.S.menu, m0);
  env.ui.pointerMove(id, x, y - 10 - 14 - 5);
  eq('4th count (accumulated remainder): one page step', env.ui.S.menu, (m0 + 1) % 7);
  env.ui.pointerUp(id); env.adv(500);

  // sub-state reset on rotate
  gotoPage(env, 1); encTap(env, 1);
  ok('right click on the SEQ EDIT tile opens seq_view', env.ui.S.seq_view === true);
  rotate(env, 0, 1); env.adv(500);
  ok('left rotate clears seq_view / seq_place / param_edit / mode_cur', !env.ui.S.seq_view && !env.ui.S.seq_place && !env.ui.S.param_edit && env.ui.S.mode_cur === 0);
}

console.log('-- left click / hold');
{
  const env = await started();
  gotoPage(env, 1);
  encTap(env, 1);
  ok('seq_view on', env.ui.S.seq_view);
  const id = ++pid, [x, y] = encXY(env.ui, 0);
  env.ui.pointerDown(id, x, y); env.adv(100); env.ui.pointerUp(id);
  ok('left click backs out of seq_view at release', !env.ui.S.seq_view);
  rotate(env, 1, 1);
  encTap(env, 1);
  ok('param_edit toggles on a param row', env.ui.S.param_edit === true && env.ui.S.mode_cur === 1);
  env.ui.pointerDown(id + 1, x, y); env.adv(100); env.ui.pointerUp(id + 1);
  ok('left click exits param_edit first', !env.ui.S.param_edit);

  // hold clear
  rotate(env, 1, -1); encTap(env, 1); ok('seq_view again', env.ui.S.seq_view);
  env.engine.clear();
  const h = ++pid; env.ui.pointerDown(h, x, y);
  env.adv(650); ok('no clear before 700 ms', env.engine.of('clearPattern').length === 0);
  env.adv(60); eq('hold >= 700 ms in drum seq_view: clearPattern(drums, sel_drum)', env.engine.of('clearPattern'), [['clearPattern', 1, 0]]);
  env.ui.pointerUp(h);
  ok('the release click is swallowed (still in seq_view)', env.ui.S.seq_view);
  const hs = ++pid; env.ui.pointerDown(hs, x, y); env.adv(30); env.ui.pointerUp(hs);
  ok('next short click backs out normally', !env.ui.S.seq_view);

  gotoPage(env, 2); encTap(env, 1); env.engine.clear();
  const h2 = ++pid; env.ui.pointerDown(h2, x, y); env.adv(710); env.ui.pointerUp(h2);
  eq('sampler seq_view hold: clearPattern(sampler, sel_smp)', env.engine.of('clearPattern'), [['clearPattern', 2, 0]]);

  gotoPage(env, 3); env.engine.clear();
  const h3 = ++pid; env.ui.pointerDown(h3, x, y); env.adv(710); env.ui.pointerUp(h3);
  eq('FX page hold: fxClearAuto', env.engine.of('fxClearAuto').length, 1);

  gotoPage(env, 0); env.engine.clear();
  const h4 = ++pid; env.ui.pointerDown(h4, x, y); env.adv(710); env.ui.pointerUp(h4);
  eq('SYNTH SEQ hold: synClear', env.engine.of('synClear').length, 1);
  gotoPage(env, 4); env.engine.clear();
  const h5 = ++pid; env.ui.pointerDown(h5, x, y); env.adv(710); env.ui.pointerUp(h5);
  ok('PATCHES hold clears nothing', env.engine.of('synClear').length + env.engine.of('clearPattern').length + env.engine.of('fxClearAuto').length === 0);
}

console.log('-- right click timing and per-page encoder behaviour');
{
  const env = await started();
  gotoPage(env, 1);
  const id = ++pid, [x, y] = encXY(env.ui, 1);
  env.ui.pointerDown(id, x, y); env.adv(40); env.ui.pointerUp(id);
  ok('right click is deferred (nothing at 40 ms)', !env.ui.S.seq_view);
  env.adv(100);
  ok('right click lands at 130 ms', env.ui.S.seq_view);
  gotoPage(env, 0); env.engine.clear();
  rotate(env, 1, 1);
  eq('SYNTH: cursor moves from tile 0 to tile 1', env.ui.S.mode_cur, 1);
  encTap(env, 1);
  eq('SYNTH: clicking another mode tile calls synMode(1)', env.engine.of('synMode'), [['synMode', 1]]);
  rotate(env, 1, -1); encTap(env, 1);
  ok('SYNTH: clicking SEQ again (it is active) opens the step editor', env.engine.of('synMode').length === 2 && env.ui.S.seq_view === false);
  encTap(env, 1);
  ok('SYNTH: re-click on the active SEQ tile enters seq_view', env.ui.S.seq_view === true);

  // param editing, SYNTH: tile x3 then params
  gotoPage(env, 1); gotoPage(env, 0); env.engine.clear();
  rotate(env, 1, 3); encTap(env, 1);
  ok('cursor 3 is WAVE; click enters param_edit', env.ui.S.mode_cur === 3 && env.ui.S.param_edit);
  rotate(env, 1, 1);
  eq('turn in param_edit calls adjust(wave, +1)', env.engine.of('adjust'), [['adjust', 'wave', 1]]);
  rotate(env, 1, -2);
  eq('and negative turns', env.engine.of('adjust').slice(1), [['adjust', 'wave', -1], ['adjust', 'wave', -1]]);
  encTap(env, 1); rotate(env, 1, 5); // cursor 3 -> 8 = PRISE
  eq('SYNTH cursor 8 is PRISE', env.ui.S.mode_cur, 8);
  encTap(env, 1); env.engine.clear(); rotate(env, 1, 1);
  eq('PRISE edit maps to adjust(prise)', env.engine.of('adjust'), [['adjust', 'prise', 1]]);
  encTap(env, 1); rotate(env, 1, 1);
  eq('SYNTH cursor 9 is BARS (shown as BARS)', env.ui.S.mode_cur, 9);
  rotate(env, 1, 3);
  eq('cursor 12 is TAP (3 tiles + 8 params + TAP = 12 items, wraps at 12)', env.ui.S.mode_cur, 0);
  rotate(env, 1, -1); env.engine.clear(); encTap(env, 1);
  eq('click on TAP sends tap()', env.engine.of('tap').length, 1);

  // DRUMS param map
  gotoPage(env, 1); env.engine.clear();
  rotate(env, 1, 1); encTap(env, 1); rotate(env, 1, 1);
  eq('DRUMS cursor 1 = KIT', env.engine.of('adjust'), [['adjust', 'kit', 1]]);
  // SAMPLER param map
  gotoPage(env, 2); env.engine.clear();
  rotate(env, 1, 1); encTap(env, 1); rotate(env, 1, 1);
  eq('SAMPLER cursor 1 = CHANCE', env.engine.of('adjust'), [['adjust', 'csmp', 1]]);

  // GLOBALS
  gotoPage(env, 6); env.engine.clear();
  encTap(env, 1); rotate(env, 1, 1);
  eq('GLOBALS BPM row: 120 -> 121 sets step_ms = 15000/121', env.engine.of('set').filter((c) => c[1] === 'step_ms'), [['set', 'step_ms', 123]]);
  env.tick({ bpm: 121 }); encTap(env, 1); rotate(env, 1, 1); encTap(env, 1); rotate(env, 1, 1);
  eq('GLOBALS KEY row uses adjust(key)', env.engine.of('adjust'), [['adjust', 'key', 1]]);

  // FX bank cycling
  gotoPage(env, 3); env.engine.clear();
  rotate(env, 1, 1); rotate(env, 1, -2);
  eq('FX page: right turn cycles banks (wrap)', env.engine.of('fxBank'), [['fxBank', 1], ['fxBank', 0], ['fxBank', 7]]);
}

console.log('-- pad precedence (SPEC 2.4)');
{
  const env = await started();
  env.engine.clear();
  padTap(env, 5);
  eq('SYNTH page: noteOn / noteOff', env.engine.calls.filter((c) => c[0].startsWith('note')), [['noteOn', 5], ['noteOff', 5]]);

  // synth seq_view: arm toggles
  encTap(env, 1); ok('synth seq_view', env.ui.S.seq_view);
  env.engine.clear(); padTap(env, 2); padTap(env, 7); padTap(env, 2);
  eq('SYNTH seq_view: pads also toggle syn_arm', env.engine.of('synArm').map((c) => c[1]), [4, 4 | 128, 128]);
  ok('and still play the note', env.engine.of('noteOn').length === 3);
  // place chord with right click
  env.engine.clear(); env.tick({ songLen: 16 });
  encTap(env, 1);
  eq('right click places the armed chord at the cursor then clears arm', env.engine.calls.map((c) => c[0] + (c[1] !== undefined ? ':' + c[1] : '')).filter((s) => s.startsWith('syn')), ['synArm:128', 'synPlace:0', 'synArm:0']);
  eq('cursor advanced', env.ui.S.seq_edit, 1);
  // LED view of the editor
  env.ui.pointerUp; padTap(env, 3);
  const led = env.ui.leds();
  eq('armed note is (200,200,255)', led[3], [200, 200, 255]);
  rotate(env, 0, 1); env.adv(500);

  // DRUMS
  env.engine.clear(); padTap(env, 6);
  eq('DRUMS page: drumHit only', env.engine.calls.filter((c) => /^(note|drum|sampler|fx)/.test(c[0])), [['drumHit', 6]]);
  eq('hit retargets the drum row optimistically (sel_drum = 3 - row)', [env.ui.tick().sel_drum, env.ui.tick().dr_pad[2]], [2, 6]);
  // place mode
  encTap(env, 1); encTap(env, 1);
  ok('seq_view, then place on right click', env.ui.S.seq_view && env.ui.S.seq_place);
  env.tick({ gstep: 20, songLen: 32 });
  env.engine.clear(); padTap(env, 3);
  eq('place mode: pad toggles step view_page*16+k of the selected voice', env.engine.of('toggleStep'), [['toggleStep', 1, 0, 16 + 3]]);
  env.tick({ gstep: 20, songLen: 18 }); env.engine.clear(); padTap(env, 3);
  eq('place mode: ignored beyond song length (step 19 >= 18)', env.engine.of('toggleStep').length, 0);
  env.tick({ gstep: 3, songLen: 16, sel_drum: 2 });
  encTap(env, 1);
  ok('right click toggles place off', env.ui.S.seq_place === false);
  env.engine.clear(); rotate(env, 1, 1);
  eq('seq_view right turn cycles the write target', env.engine.of('selectDrum'), [['selectDrum', 3]]);
  rotate(env, 0, 1); env.adv(500);

  // SAMPLER
  env.engine.clear(); padTap(env, 9, 500);
  eq('SAMPLER page: samplerPress/Release (engine owns the 300 ms rule)', env.engine.calls.filter((c) => /^sampler/.test(c[0])), [['samplerPress', 9], ['samplerRelease', 9]]);
  encTap(env, 1); encTap(env, 1);
  env.tick({ gstep: 0, songLen: 16, sel_smp: 4 }); env.engine.clear(); padTap(env, 1);
  eq('SAMPLER seq_view+place: toggles on slot sel_smp', env.engine.of('toggleStep'), [['toggleStep', 2, 4, 1]]);
  rotate(env, 0, 1); env.adv(500);

  // FX
  eq('now on FX', env.ui.S.menu, 3);
  env.engine.clear();
  const id = padDown(env, 6);
  eq('FX page: held bit of the current bank', env.engine.of('fxHeld'), [['fxHeld', 0, 64]]);
  ok('no note/drum/sampler on FX', env.engine.calls.every((c) => !/^(note|drum|sampler)/.test(c[0])));
  const [px, py] = padXY(env.ui, 7); env.ui.pointerMove(id, px + 200, py);
  eq('sliding off the pad releases it', env.engine.of('fxHeld').slice(1), [['fxHeld', 0, 0]]);
  rotate(env, 1, 1); env.engine.clear(); padTap(env, 1);
  eq('FX bank 1 pads address bank 1', env.engine.of('fxHeld'), [['fxHeld', 1, 2], ['fxHeld', 1, 0]]);
  // FX held cleared on left rotate
  const id2 = padDown(env, 4); env.engine.clear(); rotate(env, 0, 1); env.adv(500);
  eq('left rotate kills held FX', env.engine.of('fxHeld'), [['fxHeld', 1, 0]]);
  env.ui.pointerUp(id2);

  // PATCHES intercepts pads
  eq('now on PATCHES', env.ui.S.menu, 4);
  env.engine.clear(); padTap(env, 0, 50); await flush(env.ui);
  ok('PATCHES: pads do not sound anything', env.engine.calls.every((c) => !/^(note|drum|sampler|fxHeld)/.test(c[0])));

  // DASH and GLOBALS play the focused engine (focus was SAMPLER from the last engine page)
  rotate(env, 0, 1); env.adv(500); eq('now on DASH', env.ui.S.menu, 5);
  env.engine.clear(); padTap(env, 2, 400);
  ok('DASH: pads play the focused engine (SAMPLER)', env.engine.of('samplerPress').length === 1);
  gotoPage(env, 0);
  rotate(env, 0, -1); env.adv(500);
  eq('left-rotate back from SYNTH lands on GLOBALS', env.ui.S.menu, 6);
  env.engine.clear(); padTap(env, 2);
  ok('GLOBALS: focus follows last engine page (SYNTH)', env.engine.of('noteOn').length === 1);
}

console.log('-- volume mode');
{
  const env = await started();
  const a = ++pid, b = ++pid, [ax, ay] = encXY(env.ui, 0), [bx, by] = encXY(env.ui, 1);
  env.ui.pointerDown(a, ax, ay); env.ui.pointerDown(b, bx, by);
  ok('both encoders touched: master volume mode on', env.ui.S.vol_mode === true);
  env.ui.pointerUp(a); env.ui.pointerUp(b); env.adv(300);
  ok('stays on after release', env.ui.S.vol_mode === true);
  env.engine.clear(); padTap(env, 12);
  eq('pad k sets mvol_idx', env.engine.of('set').filter((c) => c[1] === 'mvol_idx'), [['set', 'mvol_idx', 12]]);
  ok('pads do not play in volume mode', env.engine.of('noteOn').length === 0);
  const fb = oled(env);
  ok('overlay: MASTER VOL, 100%', hasText(fb, 31, 6, 'MASTER VOL') && hasText(fb, 64 - 12, 20, '100%') && hasText(fb, 7, 56, 'PADS SET  BOTH=EXIT'));
  const led = env.ui.leds();
  eq('LEDs: green up to 9, amber to 12, set pad brighter', [led[0], led[9], led[10], led[12], led[13]], [[30, 120, 20], [30, 120, 20], [120, 90, 10], [180, 150, 40], [0, 0, 0]]);
  // modal: encoder rotation ignored
  const m0 = env.ui.S.menu; rotate(env, 0, 1); ok('left rotate ignored in volume mode', env.ui.S.menu === m0);
  env.engine.clear();
  const a2 = ++pid, b2 = ++pid; env.ui.pointerDown(a2, ax, ay); env.ui.pointerDown(b2, bx, by);
  env.ui.pointerUp(a2); env.ui.pointerUp(b2);
  ok('both again: exit', env.ui.S.vol_mode === false);
  // pct text
  const v15 = view(env); v15.vol_mode = true; v15.vol_pidx = 15;
  ok('index 15 = 125% TAN ZONE', hasText(renderOled(makeFb(), v15), 31, 6, 'MASTER VOL') && hasText(renderOled(makeFb(), v15), 64 - 12, 20, '125%') && hasText(renderOled(makeFb(), v15), 7, 56, 'TAN ZONE  BOTH=EXIT'));
}

console.log('-- hold-effects (FX record, synth live write)');
{
  const env = await started();
  gotoPage(env, 3); env.engine.clear();
  const id = ++pid, [x, y] = encXY(env.ui, 1);
  env.ui.pointerDown(id, x, y);
  ok('REC badge immediately', view(env).recBadge === true && hasText(renderOled(makeFb(), view(env)), 2, 1, 'REC', true));
  env.adv(60); ok('not recording before the 90 ms debounce', env.engine.of('fxRecord').length === 0);
  env.adv(40); eq('right held on FX page: fxRecord(true)', env.engine.of('fxRecord'), [['fxRecord', true]]);
  env.ui.pointerUp(id);
  eq('release: fxRecord(false)', env.engine.of('fxRecord')[1], ['fxRecord', false]);
  env.engine.clear();
  const id2 = ++pid; env.ui.pointerDown(id2, x, y); env.ui.pointerMove(id2, x, y - 12); env.adv(150);
  ok('a drag never records automation', env.engine.of('fxRecord').length === 0);
  env.ui.pointerUp(id2);
  // left held cancels REC
  gotoPage(env, 0); encTap(env, 1); encTap(env, 1); env.engine.clear();
  const id3 = ++pid; env.ui.pointerDown(id3, x, y); env.adv(100);
  eq('synth SEQ seq_view, right held: synLiveWrite(true)', env.engine.of('synLiveWrite'), [['synLiveWrite', true]]);
  env.ui.pointerUp(id3);
  ok('release ends live write', env.engine.of('synLiveWrite')[1][1] === false);
}

console.log('-- OLED screens (pixel positions from SPEC 3)');
{
  const env = await started();
  let fb = oled(env);
  ok('SYNTH: title (49,0) and mode tiles', hasText(fb, 49, 0, 'SYNTH') && hasText(fb, 13, 11, 'SEQ', true) && hasText(fb, 56, 11, 'ARP') && hasText(fb, 99, 11, 'GEN'));
  ok('SYNTH: SEQ tile filled, cursor outline around it', fb[10 * 128 + 5] === 1 && fb[8 * 128 + 0] === 1 && fb[22 * 128 + 41] === 1);
  ok('SYNTH rows: WAVE THIEF / DENS / POLY / VOL', hasText(fb, 0, 25, ' WAVE   THIEF') && hasText(fb, 0, 35, ' DENS   6') && hasText(fb, 0, 45, ' POLY   5') && hasText(fb, 0, 55, ' VOL    100%'));
  ok('page pips: current filled, others hollow', fb[1 * 128 + 93] === 1 && fb[1 * 128 + 97] === 0 && fb[0 * 128 + 96] === 1);
  ok('save star at rest', fb[1 * 128 + 123] === 1 && fb[3 * 128 + 123] === 1);
  // label fix: scroll to PRISE
  const v = view(env); v.mode_cur = 3 + 5;
  fb = renderOled(makeFb(), v);
  ok('SYNTH row labels are correct: PRISE row exists and is the selected one', hasText(fb, 0, 35, ' PRISE  0', true) && hasText(fb, 0, 25, ' SCAN   0') && hasText(fb, 0, 45, ' BARS   1') && hasText(fb, 0, 55, ' BEATS  16'));
  v.mode_cur = 3 + 8; fb = renderOled(makeFb(), v);
  ok('SYNTH TAP row reachable and highlighted', hasText(fb, 0, 55, ' TAP    120', true) && hasText(fb, 0, 25, ' PRISE  0'));
  v.mode_cur = 4; v.param_edit = true; fb = renderOled(makeFb(), v);
  ok('param_edit shows a visible dark marker on the selected bar at x=120', fb[(35 + 3) * 128 + 120] === 0 && fb[(35 + 3) * 128 + 118] === 1 && fb[(35 + 3) * 128 + 122] === 0);
  // synth editor screen
  const e2 = Object.assign(view(env), { seq_view: true, syn: { arm: 0b101, cache: new Uint16Array(128), seq_edit: 4 } });
  fb = renderOled(makeFb(), e2);
  ok('synth seq_view shows the EDIT/PLAY/ARM screen (not DRUM MACHINE)', hasText(fb, 0, 25, 'EDIT 05/16 BAR1') && hasText(fb, 0, 35, 'PLAY 01     BAR1') && hasText(fb, 0, 45, 'ARM:2  HOLD R=REC') && hasText(fb, 0, 55, 'RCLK=PLACE  L=BACK'));

  gotoPage(env, 1); fb = oled(env);
  ok('DRUMS: title, SEQ EDIT tile, rows', hasText(fb, 49, 0, 'DRUMS') && hasText(fb, 8, 11, 'SEQ E') && hasText(fb, 0, 25, ' KIT    CLASS') && hasText(fb, 0, 35, ' CHANCE 9') && hasText(fb, 0, 45, ' VOL    100%') && hasText(fb, 0, 55, ' BARS   1'));
  encTap(env, 1); fb = oled(env);
  ok('DRUMS seq_view shows the DRUM MACHINE screen (the page-level BAR line is dead code in the firmware)', hasText(fb, 0, 0, 'DRUM MACHINE'));
  gotoPage(env, 2); fb = oled(env);
  ok('SAMPLER: title, slots line', hasText(fb, 43, 0, 'SAMPLER') && hasText(fb, 0, 23, 'SLOTS:00 HOLD=REC') && hasText(fb, 0, 33, ' CHANCE 9'));
  env.tick({ rec_slot: 3 }); ok('SAMPLER recording line', hasText(oled(env), 0, 23, 'REC PAD 04...'));
  env.tick({ micState: 'denied' }); ok('SAMPLER mic denied note', hasText(oled(env), 0, 23, 'MIC DENIED'));
  gotoPage(env, 3); fb = oled(env);
  ok('FX: header, idle hint, FSR line', hasText(fb, 46, 0, '<FX1>') && hasText(fb, 0, 11, 'HOLD PADS  RT=BANK') && hasText(fb, 0, 56, 'FSR  0%   BPM:120'));
  const fx = blankTick(); fx.fx.eff[0] = (1 << 2) | (1 << 5) | (1 << 9) | (1 << 12);
  env.ui.onTick(fx); fb = oled(env);
  ok('FX: first three active names ascending', hasText(fb, 0, 11, 'STUT 16') && hasText(fb, 0, 21, 'CHORUS') && hasText(fb, 0, 31, 'DOUBLE'));
  gotoPage(env, 4); fb = oled(env);
  ok('PATCHES: title, grid, footer', hasText(fb, 31, 0, 'PATCHES') && hasText(fb, 118, 12, 'A') && hasText(fb, 118, 42, 'D') && hasText(fb, 0, 56, 'PGA.1 TAP:GO HOLD:NEW') && fb[12 * 128 + 2] === 1 && fb[13 * 128 + 3] === 1 && fb[13 * 128 + 17] === 1);
  gotoPage(env, 6); fb = oled(env);
  ok('GLOBALS: rows with the first highlighted', hasText(fb, 40, 0, 'GLOBALS') && hasText(fb, 0, 16, ' BPM   120', true) && hasText(fb, 0, 27, ' KEY   +0') && hasText(fb, 0, 38, ' MODE  LYDIAN') && hasText(fb, 0, 49, ' SWING 0'));
  gotoPage(env, 5); env.adv(500); fb = oled(env);
  ok('DASH rows', hasText(fb, 0, 0, 'A:+0.0 +0.0 +1.0') && hasText(fb, 0, 9, 'G:+0.0 +0.0 +0.0') && hasText(fb, 0, 27, 'D:   0mm S:00 O:+0') && hasText(fb, 0, 36, 'E1:--  E2:-- ') && hasText(fb, 0, 45, '[...................]') && hasText(fb, 0, 54, 'F:   0 SD:OK K:00 :D'));
  ok('DASH: no pips (not animating)', fb[1 * 128 + 93] === 0);
  env.tick({ out_amp: 0.5 }); fb = oled(env);
  ok('DASH level bar from out_amp', hasText(fb, 6, 45, '#########..........'));
  // drum machine screens
  const dm = Object.assign(view(env), { menu: 1, seq_view: true, seq_place: true, animStep: 0 });
  dm.T = Object.assign(blankTick(), { gstep: 4, dr_counts: [4, 2, 8, 0], sel_drum: 2 });
  fb = renderOled(makeFb(), dm);
  ok('DRUM MACHINE screen', hasText(fb, 0, 0, 'DRUM MACHINE') && hasText(fb, 0, 11, 'SND: SNARE SNP1') && hasText(fb, 0, 21, '>PLACING<  STEP 05') && hasText(fb, 0, 31, 'K:04 P:02 S:08 H:00') && hasText(fb, 0, 41, 'BPM 120') && hasText(fb, 0, 54, 'L=BACK R=PLAY RT=SND'));
  const sm = Object.assign(view(env), { menu: 2, seq_view: true, seq_place: false, animStep: 0 });
  sm.T = Object.assign(blankTick(), { gstep: 0, sel_smp: 4, smp_len: Object.assign(new Float32Array(16), { 4: 1.55 }), sm_counts: Object.assign(new Array(16).fill(0), { 4: 3 }) });
  fb = renderOled(makeFb(), sm);
  ok('SAMPLE SEQ screen', hasText(fb, 0, 0, 'SAMPLE SEQ') && hasText(fb, 0, 11, 'SLOT:05  1.5s') && hasText(fb, 0, 21, 'PLAYING  STEP 01') && hasText(fb, 0, 31, 'STEPS:03') && hasText(fb, 0, 54, 'L=BACK R=PLACE RT=SLOT'));
  // particles
  const quiet = renderOled(makeFb(), Object.assign(view(env), { menu: 0, animStep: 0, vol_mode: false }));
  const loud = view(env); loud.menu = 0; loud.animStep = 0; loud.T = Object.assign(blankTick(), { out_amp: 1 });
  const lf = renderOled(makeFb(), loud);
  let edgePx = 0; for (let i = 0; i < 128 * 64; i++) { const x = i % 128, y = (i / 128) | 0; if ((x < 3 || x > 124 || y < 3 || y > 60) && lf[i] && !quiet[i]) edgePx++; }
  ok('border particles appear with output level', edgePx >= 10, edgePx);
  const busy = view(env); busy.saveBusy = true; busy.spin = 0; busy.menu = 0; busy.animStep = 0;
  const bf = renderOled(makeFb(), busy);
  ok('save spinner replaces the star', bf[4 * 128 + 126] === 1 && bf[4 * 128 + 124] === 1 && bf[1 * 128 + 123] === 0);
}

console.log('-- LED frames (SPEC 4)');
{
  const env = await started();
  const base = () => Object.assign(view(env), { vol_mode: false, menu: 0, eng_focus: 0, seq_view: false });
  let v = base(); v.phases = new Array(16).fill(0); v.held_mask = 0;
  eq('idle lava is >>5 (about 3%)', ledFrame(v)[0], [2, 4, 7]);
  v.held_mask = 1; v.fsr = 0; eq('held, light touch: >>2', ledFrame(v)[0], [17, 37, 63]);
  v.fsr = 2600; eq('held, full lean: full colour', ledFrame(v)[0], [70, 150, 255]);
  v.fsr = 1900; const mid = ledFrame(v)[0]; ok('held, between 1200 and 2600 blends', mid[0] > 17 && mid[0] < 70 && mid[2] > 63 && mid[2] < 255, mid);
  v = base(); v.menu = 3; v.fx_bank = 1; v.T = blankTick(); v.T.fx.eff[1] = 1 << 5;
  eq('FX: row palette, held bit full, idle >>4', [ledFrame(v)[5], ledFrame(v)[0]], [[80, 255, 80], [2, 14, 11]]);
  v = base(); v.menu = 4; v.sess = { exists: 0b1011, cur: 1, warn: 3 };
  eq('PATCHES: warn red, active white, existing amber, empty off', [ledFrame(v)[3], ledFrame(v)[1], ledFrame(v)[0], ledFrame(v)[2]], [[255, 0, 0], [200, 200, 200], [120, 70, 10], [0, 0, 0]]);
  v = base(); v.eng_focus = 2; v.T = blankTick(); v.T.rec_slot = 2; v.T.smp_len[5] = 1;
  eq('SAMPLER slot map: recording red, filled teal', [ledFrame(v)[2], ledFrame(v)[5]], [[220, 30, 20], [10, 55, 42]]);
  v = base(); v.eng_focus = 1; v.seq_view = true; v.T = blankTick(); v.T.gstep = 1; v.T.songLen = 8; v.T.dr_page[0] = 0b1001; v.T.sel_drum = 0;
  eq('DRUM step grid: playhead, placed (kick colour >>2), past song end off', [ledFrame(v)[1], ledFrame(v)[0], ledFrame(v)[3], ledFrame(v)[9]], [[140, 140, 140], [63, 15, 10], [63, 15, 10], [0, 0, 0]]);
  v.seq_place = true; eq('placing brightens (>>1)', ledFrame(v)[0], [127, 30, 20]);
  v = base(); v.eng_focus = 1; v.seq_view = true; v.T = blankTick(); v.T.gstep = 17; v.T.songLen = 32; v.T.dr_page[0] = 0b10;
  eq('page follows the playhead (gstep 17 -> page 1; pad 1 is the playhead)', ledFrame(v)[1], [140, 140, 140]);
  v = base(); v.eng_focus = 0; v.seq_view = true; v.T = blankTick(); v.T.playing.syn = 1 << 3; v.syn = { arm: 1 << 1, cache: Object.assign(new Uint16Array(128), { 2: 1 << 6 }), seq_edit: 2 };
  eq('SYNTH editor: armed / sounding / on-edit-step', [ledFrame(v)[1], ledFrame(v)[3], ledFrame(v)[6]], [[200, 200, 255], [120, 180, 255], [20, 40, 120]]);
  // live: lava drifts, and the frame changes while ticks arrive with time
  const a = env.ui.leds(); env.adv(2000); env.tick(); const b = env.ui.leds();
  ok('lava drifts with time', JSON.stringify(a) !== JSON.stringify(b));
}

console.log('-- patches (tap / hold timings)');
{
  const env = await started();
  gotoPage(env, 4);
  env.tick();
  eq('PATCHES page, example in slot 1', [env.ui.state().exists, env.ui.state().cur], [3, 0]);
  env.engine.clear();
  padTap(env, 1, 300); await flush(env.ui);
  eq('tap on an existing slot loads it', env.engine.of('setPersist').length, 1);
  eq('and jumps to the patch page (DRUMS) and becomes active', [env.ui.S.menu, env.ui.state().cur], [1, 1]);
  gotoPage(env, 4); env.engine.clear();
  padTap(env, 7, 300); await flush(env.ui);
  ok('tap on an empty slot does nothing', env.engine.of('setPersist').length === 0 && env.ui.state().exists === 3);
  // hold on empty slot creates
  const id = padDown(env, 7); env.adv(2900); await flush(env.ui);
  ok('empty slot not created before 3 s', env.ui.state().exists === 3);
  env.adv(150); await flush(env.ui);
  ok('hold > 3 s on an empty slot creates a new patch there', (env.ui.state().exists >> 7 & 1) === 1 && env.ui.state().cur === 7 && env.engine.of('setPersist').length === 1);
  env.ui.pointerUp(id); await flush(env.ui);
  eq('new patch opens SYNTH page', env.ui.S.menu, 0);
  gotoPage(env, 4); env.tick();
  // hold existing: 5 s warn, 10 s delete
  const idw = padDown(env, 1); env.adv(4900); ok('no warning before 5 s', env.ui.state().sessWarn === -1);
  env.adv(200); eq('warning after 5 s', env.ui.state().sessWarn, 1);
  eq('LED goes red', env.ui.leds()[1], [255, 0, 0]);
  ok('footer text RELEASE=KEEP', hasText(oled(env), 0, 56, 'RELEASE=KEEP P02!'));
  env.ui.pointerUp(idw); await flush(env.ui);
  ok('release keeps the patch', (env.ui.state().exists >> 1 & 1) === 1 && env.ui.state().sessWarn === -1);
  const idd = padDown(env, 1); env.adv(10100); await flush(env.ui);
  ok('hold >= 10 s deletes', (env.ui.state().exists >> 1 & 1) === 0);
  env.ui.pointerUp(idd); await flush(env.ui);
  const id0 = padDown(env, 0); env.adv(10500); await flush(env.ui); env.ui.pointerUp(id0);
  ok('slot 0 can never be deleted', (env.ui.state().exists & 1) === 1);
  // persistence of the deletion
  env.adv(3500); await flush(env.ui); await flush(env.ui);
  const st = JSON.parse(env.store.m.get('pmor.patches.v1'));
  ok('autosave wrote the deletion (slot 1 null)', st.slots[1] === null && (st.exists & 2) === 0 && st.slots[0] !== null);
}

console.log('-- screensaver, idle, visibility');
{
  const env = await started();
  gotoPage(env, 1);
  env.adv(3500); await flush(env.ui); env.tick();
  ok('no saver at 3.5 s', view(env).saver === false);
  env.adv(22000); env.tick();
  ok('saver after 25 s idle', view(env).saver === true);
  const fb = oled(env);
  let lit = 0; for (const p of fb) lit += p;
  ok('harmonograph drawn (lit pixels, no pips)', lit > 60 && fb[1 * 128 + 93] === 0 && fb[1 * 128 + 123] === 0, lit);
  ok('saver loops at 15 fps via a timer', env.timers.size === 1);
  padTap(env, 3); ok('a pad press wakes the saver and still sounds', view(env).saver === false && env.engine.of('drumHit').length >= 1);
  // idle: no timers
  env.adv(3500); await flush(env.ui); env.adv(10);
  eq('static page, nothing pending: no timers', env.timers.size, 0);
  ok('saved: icon back to star', env.ui.state().saveBusy === false);
  // hidden
  const id = padDown(env, 2); env.engine.clear();
  env.ui.setHidden(true);
  ok('hidden: held pad released, context suspended, no timers', env.engine.of('suspend').length === 1 && env.timers.size === 0 && env.ui.state().heldMask === 0);
  env.ui.setHidden(false);
  ok('visible: resume requested', env.engine.of('resume').length >= 1);
}

console.log('-- sensors');
{
  const env = await started();
  gotoPage(env, 1); env.engine.clear();
  padTap(env, 4, 20, { pressure: 0.8, width: 1, height: 1 });
  ok('real pointer pressure maps to FSR raw 400 + p*2200', env.engine.of('set').some((c) => c[1] === 'fsr' && c[2] === 400 + 1760));
  ok('release drops FSR to 0', env.engine.of('set').filter((c) => c[1] === 'fsr').pop()[2] === 0);
  env.engine.clear();
  padTap(env, 4, 20, { pressure: 0.5, width: 1, height: 1, type: 'mouse' });
  ok('unreal pressure falls back to the lean slider (default 0.5 -> 1500)', env.engine.of('set').some((c) => c[1] === 'fsr' && c[2] === 1500));
  env.engine.clear();
  padTap(env, 4, 20, { pressure: 0, width: 40, height: 40 });
  ok('contact size is used when pressure is absent', env.engine.of('set').some((c) => c[1] === 'fsr' && c[2] > 400 + 1000));
  gotoPage(env, 5); env.adv(500);
  env.engine.clear();
  const L = env.ui.layout(), q = L.sliders[1];
  const sid = ++pid;
  env.ui.pointerDown(sid, (q.tx + q.tw * 0.6) / D, (q.y + q.h / 2) / D);
  eq('DASH lidar slider -> set(lid, mm)', env.engine.of('set').filter((c) => c[1] === 'lid'), [['set', 'lid', 360]]);
  env.ui.pointerUp(sid);
  const q2 = L.sliders[2], s2 = ++pid;
  env.ui.pointerDown(s2, (q2.tx + q2.tw * 1.0) / D, (q2.y + q2.h / 2) / D); env.ui.pointerUp(s2);
  eq('DASH tilt slider -> set(ax, g)', env.engine.of('set').filter((c) => c[1] === 'ax'), [['set', 'ax', 1]]);
  ok('DASH shows lidar and octave zone', hasText(oled(env), 0, 27, 'D: 360mm S:00 O:+2'));
}

console.log('-- audio interruption');
{
  const env = await started();
  env.engine.clear(); padTap(env, 1);
  ok('no resume while ticks are flowing', env.engine.of('resume').length === 0);
  env.adv(1000);
  env.engine.clear(); padTap(env, 1);
  ok('resume requested on a release after ticks stopped', env.engine.of('resume').length >= 1);
}

console.log('-- rendering cost');
{
  const env = await started();
  env.ctx.calls.length = 0;
  env.adv(5); env.tick();
  const n1 = env.ctx.calls.length;
  ok('identical frame repaints nothing on the OLED (diffed before painting)', env.ctx.calls.filter((c) => c[0] === 'fillRect').length <= 40, n1);
  env.ctx.calls.length = 0; env.adv(300);
  const fillsBefore = env.ctx.calls.length; env.adv(100); env.tick();
  ok('quiet static page: frames cost little', env.ctx.calls.length - fillsBefore < 200);
}

console.log('\n' + passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);

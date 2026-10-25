// PMOR integration test: the real worklet (pmor-worklet.js) under a fake AudioContext, the real Engine
// (engine.js) and the real UI (ui.js) with a stub canvas, a DOM-event layer and an IndexedDB stub.
// Time is virtual: audio frames drive the clock, setTimeout/performance.now follow it.
// Run: node pmor/test/integration.test.mjs [sampleRate]
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { readFileSync, existsSync } from 'node:fs';

const here = dirname(fileURLToPath(import.meta.url));
const SR = Number(process.argv[2] || 48000);
const Q = 128;

// ---------------------------------------------------------------- worklet scope shim (before the import)
let Proc = null;
globalThis.sampleRate = SR;
globalThis.currentFrame = 0;
globalThis.AudioWorkletProcessor = class { constructor() { this.port = { onmessage: null, postMessage() {} }; } };
globalThis.registerProcessor = (name, cls) => { if (name === 'pmor') Proc = cls; };
const emitWarning = process.emitWarning;
process.emitWarning = (w, ...a) => { if (!/MODULE_TYPELESS/.test(String((a[0] && a[0].code) || a[1] || ''))) emitWarning.call(process, w, ...a); };

// ui.js and engine.js are imported while no `document` exists, so ui.js does not auto-boot
const UI = await import(join(here, '..', 'ui.js'));
const { createPmorUI, attachDom, openStorage, renderOled, makeFb, text, MENU } = UI;
const { Engine } = await import(join(here, '..', 'engine.js'));
await import(join(here, '..', 'pmor-worklet.js'));
if (!Proc) throw new Error('worklet did not register "pmor"');

let passed = 0, failed = 0;
const ok = (name, cond, extra) => {
  if (cond) { passed++; console.log('ok   ' + name); } else { failed++; console.log('FAIL ' + name + (extra !== undefined ? '  ' + JSON.stringify(extra) : '')); }
};

// ---------------------------------------------------------------- virtual clock
let frame = 0, vnow = 1000, tid = 0, timers = [];
const realImmediate = setImmediate;
globalThis.setTimeout = (fn, ms, ...a) => { const id = ++tid; timers.push({ id, at: vnow + (ms || 0), fn, a }); return id; };
globalThis.clearTimeout = (id) => { timers = timers.filter((t) => t.id !== id); };
Object.defineProperty(globalThis, 'performance', { value: { now: () => vnow }, configurable: true });
function fireDue() {
  for (;;) {
    timers.sort((a, b) => a.at - b.at);
    const t = timers[0];
    if (!t || t.at > vnow) break;
    timers.shift(); t.fn(...t.a);
  }
}

// ---------------------------------------------------------------- fake Web Audio
let gesture = false;                     // true only while a synthetic touch event is being dispatched (iOS rule)
let iosRules = true;                     // resume() outside a gesture never completes
let curNode = null, micTarget = null, micGen = null, mediaState = 'ok';
const created = [];
let addModuleUrl = null;
const nodeLog = [];                      // every main -> worklet message
class FakeNode {
  constructor(ctx, name, opts) {
    if (name !== 'pmor') throw new Error('unknown processor ' + name);
    this.ctx = ctx; this.connected = false; this.opts = opts; this.inbox = [];
    globalThis.sampleRate = ctx.sampleRate; globalThis.currentFrame = frame;
    this.proc = new Proc();
    this.port = {
      onmessage: null,
      postMessage: (m) => { nodeLog.push(m); this.inbox.push(structuredClone(m)); },
    };
    this.proc.port.postMessage = (m) => { if (this.port.onmessage) this.port.onmessage({ data: structuredClone(m) }); };
    curNode = this;
  }
  connect() { this.connected = true; }
}
class FakeAC {
  constructor() {
    this.sampleRate = SR; this.state = 'suspended'; this.destination = {}; this.onstatechange = null;
    this.audioWorklet = { addModule: async (url) => { addModuleUrl = String(url); if (!existsSync(fileURLToPath(new URL(addModuleUrl)))) throw new Error('worklet 404'); } };
    created.push(this); this.createdInGesture = gesture;
  }
  _set(s) { if (this.state !== s) { this.state = s; if (this.onstatechange) this.onstatechange(); } }
  resume() {
    if (iosRules && !gesture) return new Promise(() => {});
    this._set('running'); return Promise.resolve();
  }
  suspend() { this._set('suspended'); return Promise.resolve(); }
  close() { this._set('closed'); return Promise.resolve(); }
  createBuffer() { return {}; }
  createBufferSource() { return { connect() {}, start() {} }; }
  createMediaStreamSource() { return { connect: (n) => { micTarget = n; }, disconnect() { micTarget = null; } }; }
}
globalThis.AudioContext = FakeAC;
globalThis.AudioWorkletNode = FakeNode;
Object.defineProperty(globalThis, 'navigator', {
  configurable: true,
  value: {
    mediaDevices: {
      getUserMedia: async () => {
        if (mediaState !== 'ok') throw new Error('NotAllowedError');
        return { getAudioTracks: () => [{ onended: null }] };
      },
    },
  },
});

// ---------------------------------------------------------------- IndexedDB stub (promise-free, event style like the real thing)
function makeFakeIdb() {
  const dbs = new Map();                 // name -> Map(store -> Map(key -> clone))
  const idb = {
    fail: null,                          // 'quota' makes every readwrite transaction abort
    open(name) {
      const req = { result: null, error: null, onsuccess: null, onerror: null, onupgradeneeded: null, onblocked: null };
      queueMicrotask(() => {
        const fresh = !dbs.has(name);
        if (fresh) dbs.set(name, new Map());
        const data = dbs.get(name);
        const db = {
          createObjectStore(s) { data.set(s, new Map()); },
          transaction(storeName, mode) {
            if (!data.has(storeName)) throw new Error('NotFoundError');
            const tx = { oncomplete: null, onerror: null, onabort: null, error: null };
            const pending = [];
            tx.objectStore = () => {
              const mk = (fn) => { const r = { result: undefined, error: null }; pending.push(() => fn(r)); return r; };
              const st = data.get(storeName);
              return {
                get: (k) => mk((r) => { r.result = st.has(k) ? structuredClone(st.get(k)) : undefined; }),
                put: (v, k) => { if (mode !== 'readwrite') throw new Error('ReadOnlyError'); return mk((r) => { st.set(k, structuredClone(v)); r.result = k; }); },
                delete: (k) => { if (mode !== 'readwrite') throw new Error('ReadOnlyError'); return mk((r) => { st.delete(k); }); },
              };
            };
            queueMicrotask(() => {
              if (mode === 'readwrite' && idb.fail) { tx.error = { name: 'QuotaExceededError' }; if (tx.onabort) tx.onabort(); return; }
              for (const p of pending) p();
              if (tx.oncomplete) tx.oncomplete();
            });
            return tx;
          },
        };
        req.result = db;
        if (fresh && req.onupgradeneeded) req.onupgradeneeded();
        if (req.onsuccess) req.onsuccess();
      });
      return req;
    },
    dump(storeName = 'patches') { return dbs.get('pmor') && dbs.get('pmor').get(storeName); },
  };
  return idb;
}

// ---------------------------------------------------------------- pages (a "page load" = canvas + window + document + engine + UI)
function makeTarget() {
  const h = {};
  return { h, addEventListener(t, fn, o) { (h[t] = h[t] || []).push({ fn, cap: !!(o && (o === true || o.capture)) }); },
    removeEventListener(t, fn) { if (h[t]) h[t] = h[t].filter((x) => x.fn !== fn); } };
}
function stubCtx() {
  const calls = [];
  return new Proxy({}, {
    get(t, k) { if (k === 'calls') return calls; if (k in t) return t[k]; return (...a) => { calls.push(k); }; },
    set(t, k, v) { t[k] = v; return true; },
  });
}
function newPage(idb) {
  const win = makeTarget(); win.devicePixelRatio = 3;
  const doc = makeTarget(); doc.hidden = false;
  const canvas = makeTarget();
  const g = stubCtx();
  canvas.getContext = () => g; canvas.setPointerCapture = () => {};
  canvas.getBoundingClientRect = () => ({ left: 0, top: 0, width: 390, height: 844 });
  globalThis.window = win; globalThis.document = doc;
  const engine = new Engine();
  const storage = openStorage(idb, null);
  const ui = createPmorUI({ canvas, engine, storage, now: () => vnow });
  attachDom(ui, canvas, win, doc);
  const page = { win, doc, canvas, engine, ui, ticks: 0, lastTickAt: -1 };
  const orig = engine.onTick;
  engine.onTick = (t) => { page.ticks++; page.lastTickAt = vnow; page.maxDr = Math.max(page.maxDr || 0, t.dr_counts[0] + t.dr_counts[1] + t.dr_counts[2] + t.dr_counts[3]); orig(t); };
  page.fire = (target, type, ev) => {
    gesture = /^(pointer|touch|click|key)/.test(type);
    try {
      ev = Object.assign({ type, preventDefault() {} }, ev);
      const w = win.h[type] || [];
      if (target === canvas) {
        for (const x of w) if (x.cap) x.fn(ev);
        for (const x of canvas.h[type] || []) x.fn(ev);
        for (const x of w) if (!x.cap) x.fn(ev);
      } else for (const x of (target.h[type] || [])) x.fn(ev);
    } finally { gesture = false; }
  };
  return page;
}

// ---------------------------------------------------------------- rendering
const L = new Float32Array(Q), R = new Float32Array(Q), ZERO = new Float32Array(Q), MIC = new Float32Array(Q);
let cap = null;
function quantum() {
  const n = curNode;
  vnow += Q * 1000 / SR;
  if (!n || !n.connected || n.ctx.state !== 'running') return;
  globalThis.currentFrame = frame; globalThis.sampleRate = SR;
  while (n.inbox.length) n.proc.port.onmessage({ data: n.inbox.shift() });   // messages land between render quanta
  let inp = ZERO;
  if (micTarget === n && micGen) { for (let i = 0; i < Q; i++) MIC[i] = micGen(frame + i); inp = MIC; }
  n.proc.process([[inp]], [[L, R]]);
  if (cap) { cap.L.push(Float32Array.from(L)); cap.R.push(Float32Array.from(R)); }
  frame += Q;
}
async function adv(sec) {
  const n = Math.round(sec * SR / Q);
  for (let i = 0; i < n; i += 4) {
    for (let j = 0; j < 4 && i + j < n; j++) quantum();
    fireDue();
    await new Promise((r) => realImmediate(r));
  }
}
function startCap() { cap = { L: [], R: [] }; }
function stopCap() {
  const c = cap; cap = null;
  const len = c.L.length * Q, l = new Float32Array(len), r = new Float32Array(len);
  c.L.forEach((a, i) => l.set(a, i * Q)); c.R.forEach((a, i) => r.set(a, i * Q));
  let peak = 0, ss = 0, bad = 0;
  for (let i = 0; i < len; i++) {
    if (!Number.isFinite(l[i]) || !Number.isFinite(r[i])) { bad++; continue; }
    peak = Math.max(peak, Math.abs(l[i]), Math.abs(r[i])); ss += l[i] * l[i] + r[i] * r[i];
  }
  return { L: l, R: r, peak, rms: Math.sqrt(ss / Math.max(1, 2 * len)), bad, len };
}
async function capture(sec) { startCap(); await adv(sec); return stopCap(); }
const rmsOf = (a, i0, i1) => { let s = 0; for (let i = i0; i < i1; i++) s += a[i] * a[i]; return Math.sqrt(s / Math.max(1, i1 - i0)); };
function onsets(x, minGapMs = 150) {            // envelope (5 ms RMS) rising through a threshold
  const w = Math.round(SR * 0.005), env = [];
  for (let i = 0; i + w <= x.length; i += w) env.push(rmsOf(x, i, i + w));
  const mx = Math.max(...env), th = mx * 0.25, out = [];
  let last = -1e9;
  for (let i = 1; i < env.length; i++) {
    if (env[i] >= th && env[i - 1] < th && (i - last) * 5 >= minGapMs) { out.push(i * 5); last = i; }
  }
  return out;
}
function freqEstimate(x, i0, i1) {
  let first = -1, last = -1, n = 0;
  for (let i = i0 + 1; i < i1; i++) if (x[i - 1] <= 0 && x[i] > 0) { const f = i - x[i] / (x[i] - x[i - 1]); if (first < 0) first = f; last = f; n++; }
  return n > 1 ? (n - 1) * SR / (last - first) : 0;
}

// ---------------------------------------------------------------- gestures through the DOM layer
let pid = 0;
const D = 3;
const padXY = (p, k) => { const q = p.ui.layout().pads[k]; return [(q.x + q.w / 2) / D, (q.y + q.h / 2) / D]; };
const encXY = (p, i) => { const e = p.ui.layout().enc[i]; return [e.cx / D, e.cy / D]; };
const ptr = (p, type, id, x, y, extra) => p.fire(p.canvas, type, Object.assign({ pointerId: id, clientX: x, clientY: y, pressure: 0.5, width: 1, height: 1, pointerType: 'touch' }, extra));
function padDown(p, k) { const id = ++pid, [x, y] = padXY(p, k); ptr(p, 'pointerdown', id, x, y); return { id, x, y }; }
function up(p, h) { ptr(p, 'pointerup', h.id, h.x, h.y); p.fire(p.canvas, 'touchend', {}); }
async function tapPad(p, k, ms = 30) { const h = padDown(p, k); await adv(ms / 1000); up(p, h); await adv(0.02); }
async function rotate(p, i, detents) {
  const id = ++pid, [x, y] = encXY(p, i), dir = detents > 0 ? -1 : 1;
  ptr(p, 'pointerdown', id, x, y); ptr(p, 'pointermove', id, x, y + dir * 10);
  let yy = y + dir * 10;
  for (let d = 0; d < Math.abs(detents); d++) { yy += dir * 20; ptr(p, 'pointermove', id, x, yy); }
  ptr(p, 'pointerup', id, x, yy);
  await adv(0.05);
}
async function encTap(p, i) {
  const id = ++pid, [x, y] = encXY(p, i);
  ptr(p, 'pointerdown', id, x, y); await adv(0.03); ptr(p, 'pointerup', id, x, y); await adv(0.2);
}
async function gotoPage(p, n) { const d = (n - p.ui.S.menu + 7) % 7; if (d) { await rotate(p, 0, d); await adv(0.5); } }
const T = (p) => p.ui.tick();
const oledFb = (p) => renderOled(makeFb(), p.ui.view());
function hasText(fb, x, y, str, inv = false) {
  const ref = makeFb(); text(ref, x, y, str, true);
  for (let yy = y; yy < y + 8; yy++) for (let xx = x; xx < x + str.length * 6; xx++) if (inv ? fb[yy * 128 + xx] === ref[yy * 128 + xx] : fb[yy * 128 + xx] !== ref[yy * 128 + xx]) return false;
  return true;
}
const lit = (fb) => { let n = 0; for (const v of fb) n += v; return n; };

// ================================================================ platform checks (static)
console.log('-- platform / paths');
{
  const root = join(here, '..', '..');
  const html = readFileSync(join(root, 'PMOR.html'), 'utf8');
  const wk = readFileSync(join(here, '..', 'pmor-worklet.js'), 'utf8');
  const uiSrc = readFileSync(join(here, '..', 'ui.js'), 'utf8');
  const engSrc = readFileSync(join(here, '..', 'engine.js'), 'utf8');
  const page = new URL('https://emgor.online/PMOR.html');
  const uiUrl = new URL(/<script[^>]*src="([^"]+)"/.exec(html)[1], page);
  const engUrl = new URL('./engine.js', uiUrl), wkUrl = new URL('./pmor-worklet.js', engUrl);
  ok('PMOR.html resolves pmor/ui.js at /pmor/ui.js, ui imports /pmor/engine.js, engine adds /pmor/pmor-worklet.js',
    uiUrl.pathname === '/pmor/ui.js' && engUrl.pathname === '/pmor/engine.js' && wkUrl.pathname === '/pmor/pmor-worklet.js' && !/<base\b/i.test(html));
  ok('script is type=module and there is no inline top-level await', /<script type="module"/.test(html) && !/await/.test(html.replace(/<script[^>]*src[^>]*><\/script>/, '')));
  ok('worklet file is self-contained (no import/export statements)', !/^\s*(import|export)\s/m.test(wk) && !/\bimport\s*\(/.test(wk));
  ok('no SharedArrayBuffer / OffscreenCanvas / top-level await anywhere', ![wk, uiSrc, engSrc].some((s) => /SharedArrayBuffer|OffscreenCanvas|^await\s/m.test(s)));
  ok('viewport: no zoom, safe-area, touch-action none, user-select none, 100dvh',
    /user-scalable=no/.test(html) && /viewport-fit=cover/.test(html) && /env\(safe-area-inset-top\)/.test(html) && /touch-action: none/.test(html) && /user-select: none/.test(html) && /100dvh/.test(html));
  ok('no console.log in production paths', ![wk, uiSrc, engSrc].some((s) => /console\.log/.test(s)));
}

// ================================================================ scenario 1: tap to start, boot, SYNTH
const idb = makeFakeIdb();
let P = newPage(idb);
console.log('-- tap-to-start');
{
  ok('before the tap: nothing created, splash phase idle', created.length === 0 && P.ui.phase() === 'idle');
  const [x, y] = padXY(P, 5);
  ptr(P, 'pointerdown', 900, x, y);
  ok('pointerdown alone does not start audio', created.length === 0);
  startCap();
  ptr(P, 'pointerup', 900, x, y);
  ok('AudioContext created synchronously inside the touch handler (iOS unlock)', created.length === 1 && created[0].createdInGesture === true && created[0].state === 'running');
  P.fire(P.canvas, 'touchend', {}); P.fire(P.canvas, 'click', {});
  ok('touchend/click after the pointerup do not start a second context', created.length === 1);
  ok('phase starting right after the tap', P.ui.phase() === 'starting');
  await adv(0.5);
  ok('worklet module URL is /pmor/pmor-worklet.js next to engine.js', /\/pmor\/pmor-worklet\.js$/.test(addModuleUrl), addModuleUrl);
  ok('boot intro phase reached with the engine ready', (P.ui.phase() === 'boot' || P.ui.phase() === 'run') && P.engine.ready === true);
  await adv(0.5);
  const c = stopCap();
  ok('run phase after the intro', P.ui.phase() === 'run');
  ok('example beat planted while the master was muted: output was silent from tap to run', c.peak < 1e-6 && c.bad === 0, { peak: c.peak });
  const mv = nodeLog.filter((m) => m.c === 'set' && m.name === 'mvol_idx').map((m) => m.value);
  const iMute = nodeLog.findIndex((m) => m.c === 'set' && m.name === 'mvol_idx' && m.value === 0);
  const iTog = nodeLog.findIndex((m) => m.c === 'tog');
  ok('mvol_idx 0 sent before the first toggleStep and restored to 9 after', iMute >= 0 && iMute < iTog && mv[mv.length - 1] === 9 && T(P).params.mvol_idx === 9, mv);
  ok('example planted exactly once: 14 toggles, exists slots 0 and 1', nodeLog.filter((m) => m.c === 'tog').length === 14 && P.ui.state().exists === 3 && P.ui.state().cur === 0);
  ok('lands on the SYNTH page', P.ui.S.menu === MENU.SYNTH);
  ok('IndexedDB holds meta + slots 0,1 after boot', idb.dump() && idb.dump().has('meta') && idb.dump().has('s0') && idb.dump().has('s1') && idb.dump().get('meta').planted === true);
  ok('UI never sent ax outside the FX page', nodeLog.filter((m) => m.c === 'set' && m.name === 'ax').length === 0);
  const d = P.engine.defaults();
  ok('defaults() is synchronous and JSON-safe', JSON.stringify(JSON.parse(JSON.stringify(d))) === JSON.stringify(d) && !(d instanceof Promise));
  const ppP = P.engine.getPersist(); await adv(0.05); const pp = await ppP;
  ok('getPersist() is JSON-safe (no typed arrays, audio as base64)', (() => { try { return JSON.stringify(pp).length > 100 && !JSON.stringify(pp, (k, v) => (ArrayBuffer.isView(v) ? '#TYPED' : v)).includes('#TYPED'); } catch (e) { return false; } })());
}

// ================================================================ scenario 2: synth pad
console.log('-- synth pad 12');
{
  await adv(1.0);
  const base = await capture(0.3);
  ok('quiet before the press', base.peak < 1e-4, base.peak);
  const h = padDown(P, 12);                       // bottom-left
  startCap(); await adv(Q / SR);                  // exactly one 128-frame quantum
  const one = stopCap();
  ok('energy within one block of the press', one.peak > 1e-4, one.peak);
  startCap(); await adv(0.6); const held = stopCap();
  const heldRms = rmsOf(held.L, 0, held.len);
  up(P, h);
  startCap(); await adv(2.0); const rel = stopCap();
  const tail = rmsOf(rel.L, rel.len - Math.round(0.2 * SR), rel.len);
  ok('held note sustains (rms > 1e-3) and the finger pan puts it on the left', heldRms > 1e-3 && rmsOf(held.L, 0, held.len) > rmsOf(held.R, 0, held.len) * 1.05, { heldRms, L: rmsOf(held.L, 0, held.len), R: rmsOf(held.R, 0, held.len) });
  ok('release decays (tail < 5% of held, < 1e-3)', tail < heldRms * 0.05 && tail < 1e-3, { tail, heldRms });
  ok('tick.playing.held mask follows the press and release', (T(P).playing.held & (1 << 12)) === 0);
}

// ================================================================ scenario 3: every page by left rotate
console.log('-- 7 pages');
{
  let thrown = 0, dark = 0;
  const seen = [P.ui.S.menu];
  for (let i = 0; i < 7; i++) {
    try {
      await rotate(P, 0, 1); await adv(0.5);
      seen.push(P.ui.S.menu);
      const fb = oledFb(P); if (lit(fb) < 20) dark++;
      const leds = P.ui.leds(); if (leds.length !== 16 || leds.some((c) => c.length !== 3 || c.some((v) => !(v >= 0 && v <= 255)))) dark++;
      if (P.ui.S.menu === MENU.DASH) await adv(0.2);
    } catch (e) { thrown++; console.log('   ' + e.stack); }
  }
  ok('left-rotate visits 1,2,3,4,5,6,0 with real ticks', JSON.stringify(seen) === '[0,1,2,3,4,5,6,0]', seen);
  ok('no exception; every OLED screen and LED frame renders', thrown === 0 && dark === 0, { thrown, dark });
  ok('ax was sent exactly once during the sweep (on entering FX), never on DASH', nodeLog.filter((m) => m.c === 'set' && m.name === 'ax').length === 1);
}

// ================================================================ scenario 4: drums
console.log('-- drums');
{
  await gotoPage(P, MENU.DRUM);
  startCap(); await tapPad(P, 12, 40); await adv(0.8); const hit = stopCap();
  ok('DRUMS: pad 12 is audible', hit.peak > 0.01 && T(P).sel_drum === 0 && T(P).dr_pad[0] === 12, hit.peak);
  await encTap(P, 1);                              // cursor 0 = SEQ EDIT -> open
  ok('SEQ EDIT opened', P.ui.S.seq_view === true && P.ui.S.seq_place === false);
  await encTap(P, 1);                              // click again = PLACING
  ok('placing mode on', P.ui.S.seq_place === true);
  await adv(0.3);
  const page = T(P).gstep >> 4;
  for (const k of [0, 4, 8, 12]) await tapPad(P, k, 30);
  await adv(0.3);
  ok('4 steps stored in the engine for the kick (tick.dr_counts[0] == 4)', T(P).dr_counts[0] === 4 && page === 0, { counts: T(P).dr_counts, page });
  await adv(2.2);
  const c = await capture(6.0);
  const on = onsets(c.L, 300);
  const gaps = on.slice(1).map((v, i) => v - on[i]);
  ok('4 hits per bar: 12 kicks in 3 bars, 500 ms apart', on.length >= 11 && on.length <= 13 && gaps.every((g) => Math.abs(g - 500) <= 30), { n: on.length, gaps });
  ok('drum output finite and below full scale', c.bad === 0 && c.peak <= 1.0 && c.peak > 0.01, c.peak);
  P.drumRms = c.rms;
}

// ================================================================ scenario 5: FX
console.log('-- FX page');
{
  // leave placing mode and go to FX
  await encTap(P, 0);
  await gotoPage(P, MENU.FX);
  await rotate(P, 1, 1); await adv(0.3);                    // right encoder: bank 1
  ok('FX bank 1 selected', P.ui.S.fx_bank === 1 && T(P).fx.bank === 1, { ui: P.ui.S.fx_bank, tick: T(P).fx.bank });
  ok('second FX entry sent ax once more (2 total)', nodeLog.filter((m) => m.c === 'set' && m.name === 'ax').length === 2);
  await adv(0.5);
  const ctrlA = await capture(2.0);
  const ctrlB = await capture(2.0);
  const ratioCtl = ctrlB.rms / ctrlA.rms;
  const h = padDown(P, 8);                                  // ISO-LO: takes the kick body out
  await adv(0.5);
  ok('tick.fx.held shows the held pad on bank 1', (T(P).fx.held[1] & (1 << 8)) !== 0 && (T(P).fx.eff[1] & (1 << 8)) !== 0, T(P).fx);
  const fxc = await capture(1.0);
  const ratioFx = fxc.rms / (ctrlB.rms || 1e-9);
  up(P, h); await adv(0.3);
  ok('FX hold changes the output vs bypass (rms ratio differs from the no-FX bar ratio by > 25%)', Math.abs(ratioFx - 1) > 0.25 && Math.abs(ratioCtl - 1) < 0.1, { ratioCtl, ratioFx });
  ok('release clears tick.fx.held and the UI mask', T(P).fx.held.every((v) => v === 0) && P.ui.state().heldMask === 0);
  const after = await capture(2.0);
  ok('output returns toward bypass after release (within 20% of control rms)', Math.abs(after.rms / ctrlB.rms - 1) < 0.2, { r: after.rms / ctrlB.rms });
  // engine's 20 s stuck-FX cap on bank 0, and the UI following it
  await rotate(P, 1, -1); await adv(0.3);
  const hs = padDown(P, 13); await adv(1.0);
  ok('bank 0 hold registered in the engine', P.ui.S.fx_bank === 0 && (T(P).fx.held[0] & (1 << 13)) !== 0);
  await adv(20.5);
  const sentBefore = nodeLog.filter((m) => m.c === 'fxHeld').length;
  ok('engine dropped the stuck hold after 20 s (tick.fx.held[0] clear) while the finger is still down', T(P).fx.held[0] === 0 && P.ui.state().heldMask === (1 << 13));
  up(P, hs); await adv(0.2);
  ok('release after the cap sends no stale fxHeld', nodeLog.filter((m) => m.c === 'fxHeld').length === sentBefore);
  const hs2 = padDown(P, 13); await adv(0.4);
  ok('a new press after the cap holds again', (T(P).fx.held[0] & (1 << 13)) !== 0);
  up(P, hs2); await adv(0.3);
  ok('and releases', T(P).fx.held[0] === 0);
  await gotoPage(P, MENU.SYNTH);
}

// ================================================================ scenario 6: volume mode
console.log('-- volume mode');
{
  await gotoPage(P, MENU.DRUM);
  await adv(0.3);
  const a = await capture(2.0);
  const e0 = padDownEnc(P, 0), e1 = padDownEnc(P, 1);
  await adv(0.3);
  ok('both encoders held -> volume mode', P.ui.state().vol_mode === true);
  ptr(P, 'pointerup', e0.id, e0.x, e0.y); ptr(P, 'pointerup', e1.id, e1.x, e1.y);
  await adv(0.1);
  ok('volume mode stays after releasing both', P.ui.state().vol_mode === true);
  await tapPad(P, 6, 30); await adv(0.2);
  ok('pad 6 -> mvol_idx 6 in the tick', T(P).params.mvol_idx === 6, T(P).params.mvol_idx);
  const b = await capture(2.0);
  const want = 0.081 / 0.2848, got = b.rms / a.rms;
  ok('master level follows kVolTable (pad 6 vs 9: ratio ~0.285)', got > want * 0.7 && got < want * 1.3, { got, want });
  await tapPad(P, 15, 30); await adv(0.2);
  const c = await capture(2.0);
  ok('pad 15 raises the level above pad 9 and stays finite and clipped to <= 1.0', T(P).params.mvol_idx === 15 && c.rms > a.rms * 1.2 && c.peak <= 1.0 && c.bad === 0, { rms: c.rms, base: a.rms, peak: c.peak });
  await tapPad(P, 9, 30);
  const e2 = padDownEnc(P, 0), e3 = padDownEnc(P, 1);
  await adv(0.3); ptr(P, 'pointerup', e2.id, e2.x, e2.y); ptr(P, 'pointerup', e3.id, e3.x, e3.y); await adv(0.2);
  ok('both again exits volume mode', P.ui.state().vol_mode === false);
  const d = await capture(2.0);
  ok('exit at pad 9 restores the original level', Math.abs(d.rms / a.rms - 1) < 0.15, d.rms / a.rms);
}
function padDownEnc(p, i) { const id = ++pid, [x, y] = encXY(p, i); ptr(p, 'pointerdown', id, x, y); return { id, x, y }; }

// ================================================================ scenario 7: patches + round trip
console.log('-- patches');
let recSlotLen = 0, keyBefore = 0;
{
  // distinctive edit in slot 0: KEY +3 on GLOBALS
  await gotoPage(P, MENU.GLOB);
  await rotate(P, 1, 1); await encTap(P, 1);                 // cursor -> KEY, enter edit
  await rotate(P, 1, 3); await adv(0.2);
  keyBefore = T(P).params.key;
  ok('GLOBALS KEY edited to +3 through adjust', keyBefore === 3, keyBefore);
  await encTap(P, 1);
  // BPM goes through adjust('bpm'): the OLED shows the engine's value
  await rotate(P, 1, -1); await adv(0.2);                    // cursor back to BPM
  await encTap(P, 1); await rotate(P, 1, 2); await adv(0.3);
  ok('GLOBALS BPM: engine owns the step, OLED shows 122 from the tick', T(P).bpm === 122 && hasText(oledFb(P), 0, 16, ' BPM   122', true), T(P).bpm);
  await rotate(P, 1, -2); await adv(0.2); await encTap(P, 1);
  ok('BPM back to 120', T(P).bpm === 120, T(P).bpm);

  // record 1 s of a 440 Hz sine into sampler slot 5
  micGen = (i) => 0.5 * Math.sin(2 * Math.PI * 440 * i / SR);
  await gotoPage(P, MENU.SMP);
  const h = padDown(P, 5);
  await adv(0.5);
  ok('mic asked lazily on the 300 ms hold and granted', P.engine.micState === 'ok', P.engine.micState);
  await adv(0.9);
  ok('recording in slot 5 while held', T(P).rec_slot === 5, T(P).rec_slot);
  up(P, h); await adv(0.4);
  recSlotLen = T(P).smp_len[5];
  ok('slot 5 holds about 1.2-1.5 s after release (hold 1.4 s minus the 0.3 s arm)', recSlotLen > 0.9 && recSlotLen < 1.5, recSlotLen);
  startCap(); await tapPad(P, 5, 40); await adv(0.6); const play = stopCap();
  const f = freqEstimate(play.L, Math.round(0.05 * SR), Math.round(0.4 * SR));
  ok('recorded slot plays back near 440 Hz', play.peak > 0.01 && Math.abs(f - 440) < 440 * 0.06, { f, peak: play.peak });

  // PATCHES
  await gotoPage(P, MENU.SESS);
  await adv(4.0);                                            // autosave
  ok('autosave wrote slot 0 with the sampler audio to IndexedDB', (() => { const s0 = idb.dump().get('s0'); return s0 && s0.p.audio && s0.p.audio[5] && s0.p.audio[5].b64.length > SR * 2 * 0.9 * 1.3; })());
  await tapPad(P, 1, 40); await adv(0.5);
  const ex = await capture(2.0);
  ok('slot 1 is the planted example beat and it is audible, on the DRUMS page', P.ui.state().cur === 1 && P.ui.S.menu === MENU.DRUM && T(P).dr_counts[0] === 4 && ex.rms > 0.002, { rms: ex.rms, menu: P.ui.S.menu });
  await gotoPage(P, MENU.SESS);
  await tapPad(P, 0, 40); await adv(0.5);
  const hh = padDown(P, 3); await adv(2.9);
  ok('empty slot 3: nothing before 3 s', (P.ui.state().exists >> 3 & 1) === 0);
  await adv(0.3); up(P, hh); await adv(0.5);
  ok('hold 3 s on empty slot creates a new patch at slot 3 with defaults', P.ui.state().cur === 3 && T(P).params.key === 0 && T(P).smp_len[5] === 0 && T(P).bpm === 120, { cur: P.ui.state().cur, key: T(P).params.key });
  ok('new patch lands on SYNTH', P.ui.S.menu === MENU.SYNTH);
  await gotoPage(P, MENU.SESS);
  // mvol is global: loading a patch must not move it
  const ve = padDownEnc(P, 0), ve2 = padDownEnc(P, 1); await adv(0.3); ptr(P, 'pointerup', ve.id, ve.x, ve.y); ptr(P, 'pointerup', ve2.id, ve2.x, ve2.y);
  await tapPad(P, 7, 30);
  const ve3 = padDownEnc(P, 0), ve4 = padDownEnc(P, 1); await adv(0.3); ptr(P, 'pointerup', ve3.id, ve3.x, ve3.y); ptr(P, 'pointerup', ve4.id, ve4.x, ve4.y); await adv(0.2);
  ok('mvol set to index 7 while on slot 3', T(P).params.mvol_idx === 7 && P.ui.state().vol_mode === false, T(P).params.mvol_idx);
  await tapPad(P, 0, 40); await adv(0.6);
  ok('tap slot 0 loads it: key +3, sampler slot 5 restored, drum steps back', P.ui.state().cur === 0 && T(P).params.key === 3 && Math.abs(T(P).smp_len[5] - recSlotLen) < 0.01 && T(P).dr_counts[0] === 4, { cur: P.ui.state().cur, key: T(P).params.key, len: T(P).smp_len[5], dr: T(P).dr_counts });
  ok('setPersist did not touch mvol_idx', T(P).params.mvol_idx === 7, T(P).params.mvol_idx);
  ok('loading a patch keeps the playhead running (gstep not forced to 0)', T(P).songLen === 16);
  await tapPad(P, 3, 40); await adv(0.4);
  await tapPad(P, 0, 40); await adv(0.4);
  // restore volume
  const v5 = padDownEnc(P, 0), v6 = padDownEnc(P, 1); await adv(0.3); ptr(P, 'pointerup', v5.id, v5.x, v5.y); ptr(P, 'pointerup', v6.id, v6.x, v6.y);
  await tapPad(P, 9, 30);
  const v7 = padDownEnc(P, 0), v8 = padDownEnc(P, 1); await adv(0.3); ptr(P, 'pointerup', v7.id, v7.x, v7.y); ptr(P, 'pointerup', v8.id, v8.x, v8.y); await adv(0.2);
  await adv(4.0);
}

// ================================================================ scenario 8: reload from IndexedDB (a new page load)
console.log('-- reload from IndexedDB');
{
  nodeLog.length = 0;
  const P2 = newPage(idb); P = P2;
  const [x, y] = padXY(P, 5);
  ptr(P, 'pointerdown', 901, x, y); ptr(P, 'pointerup', 901, x, y);
  await adv(1.2);
  ok('second page load reaches run', P.ui.phase() === 'run' && P.engine.ready);
  ok('second boot plants nothing (planted flag in storage)', nodeLog.filter((m) => m.c === 'tog').length === 0);
  await adv(0.3);
  ok('params restored from IndexedDB: key +3, sampler slot 5 present, 4 kick steps, slot 3 and 1 exist', T(P).params.key === 3 && Math.abs(T(P).smp_len[5] - recSlotLen) < 0.01 && T(P).dr_counts[0] === 4 && (P.ui.state().exists & 0b1011) === 0b1011, { key: T(P).params.key, len: T(P).smp_len[5], exists: P.ui.state().exists });
  await gotoPage(P, MENU.SMP);
  startCap(); await tapPad(P, 5, 40); await adv(0.6); const play = stopCap();
  const f = freqEstimate(play.L, Math.round(0.05 * SR), Math.round(0.4 * SR));
  ok('the reloaded sampler slot still plays a 440 Hz tone', play.peak > 0.01 && Math.abs(f - 440) < 440 * 0.06, { f, peak: play.peak });

  // storage failure is visible, not a hung spinner
  idb.fail = 'quota';
  await rotate(P, 1, 1); await adv(0.1);
  await tapPad(P, 2, 40);
  await adv(5.0);
  ok('storage failure: icon flashes (saveFail) instead of a stuck spinner', P.ui.state().saveFailed === true && P.ui.view().saveFail === true);
  const f1 = renderOled(makeFb(), Object.assign(P.ui.view(), { spin: 0 })), f2 = renderOled(makeFb(), Object.assign(P.ui.view(), { spin: 8 }));
  let dif = 0; for (let yy = 0; yy < 9; yy++) for (let xx = 119; xx < 128; xx++) dif += f1[yy * 128 + xx] ^ f2[yy * 128 + xx];
  ok('the flash has two distinct phases', dif > 20, dif);
  idb.fail = null;
  await adv(11.0);
  ok('retry after the failure clears the icon', P.ui.state().saveFailed === false && P.ui.view().saveFail === false);
}

// ================================================================ scenario 9: screensaver keeps audio running
console.log('-- screensaver');
{
  await gotoPage(P, MENU.DRUM);
  await adv(0.5);
  startCap(); await adv(26);
  const c = stopCap();
  ok('screensaver on after 25 s idle', P.ui.view().saver === true);
  ok('audio kept running under the screensaver (drum loop rms in the last 4 s)', rmsOf(c.L, c.len - 4 * SR, c.len) > 0.002 && c.bad === 0, rmsOf(c.L, c.len - 4 * SR, c.len));
  const t0 = P.ticks; await adv(0.5);
  ok('ticks keep flowing while the context runs', P.ticks - t0 >= 5, P.ticks - t0);
  ok('screensaver animates (lit pixels)', lit(oledFb(P)) > 60);
  await tapPad(P, 1, 30);
  ok('a pad press wakes the screensaver', P.ui.view().saver === false);
}

// ================================================================ scenario 10: hidden -> suspend, touch -> resume
console.log('-- visibility');
{
  const ctx = created[created.length - 1];
  ok('context running before hide', ctx.state === 'running');
  const f0 = frame;
  P.doc.hidden = true; P.fire(P.doc, 'visibilitychange', {});
  await adv(0.5);
  ok('hidden: context suspended and the audio clock stopped', ctx.state === 'suspended' && frame === f0, { state: ctx.state, d: frame - f0 });
  const tk = P.ticks; await adv(1.0);
  ok('no ticks while hidden', P.ticks === tk);
  P.doc.hidden = false; P.fire(P.doc, 'visibilitychange', {});
  await adv(0.2);
  ok('becoming visible alone cannot resume on iOS (needs a gesture)', ctx.state === 'suspended');
  const h = padDown(P, 14); await adv(0.05); up(P, h);
  await adv(0.5);
  ok('a touch resumes the context and the sequencer position is kept', ctx.state === 'running' && frame > f0 && T(P).songLen === 16);
  const t1 = P.ticks; await adv(0.5);
  ok('ticks flow again after resume', P.ticks - t1 >= 5);
}

console.log('-- setPersist keeps the playhead');
{
  for (let i = 0; i < 200 && !(T(P).gstep >= 6 && T(P).gstep <= 10); i++) await adv(0.02);
  const g = T(P).gstep;
  P.engine.setPersist(JSON.parse(JSON.stringify(P.engine.defaults())));
  await adv(0.08);
  ok('setPersist does not reset gstep (firmware persistApply keeps it)', g >= 6 && T(P).gstep >= g && T(P).gstep <= g + 2, { g, now: T(P).gstep });
}

console.log('\n' + passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);

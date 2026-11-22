// app.js -- HYPERTRUMPET V2 simulator UI. One <canvas>, custom drawn, no frameworks, no rAF loop:
// every redraw is event driven (pointer/key/worklet state message) and coalesced into one requestAnimationFrame.
// All control values go through setCtl()/sendCtl(): one packed Float32Array posted to the worklet (<= 100 Hz while touched).
'use strict';
(function () {
const LW = 1280, LH = 900;                       // logical canvas size, scaled to fit the window
const C = { bg: '#0d0618', panel: '#160a29', panel2: '#1f1038', line: '#34205c', violet: '#8a4dff', violet2: '#b28cff', magenta: '#ff3fa8',
  text: '#f0e8ff', dim: '#8d7bb3', dim2: '#5a4880', ok: '#52e0a0', warn: '#ffb84d' };
const FONT = 'system-ui,-apple-system,"Helvetica Neue",Arial,sans-serif', MONO = 'ui-monospace,Menlo,Consolas,monospace';
const ROLE = ['AMT', 'TIME', 'TONE', 'MOD', 'FB', 'PITCH'];

// ------------------------------------------------------------------------------------------------ state
const S = { effects: [], slots: [0, 1, 2, 3].map(() => ({ type: -1, byp: 0, vals: [], desc: null })), met: new Array(11).fill(0), leds: new Array(48).fill(0),
  status: '', page: 0, sel: 0, selParam: 0, cpuMs: 0, rms: 0, peak: 0, ready: false, perf: false, sr: 48000, block: 64, err: '' };
const U = {   // UI state
  running: false, mode: 0, fileName: '', micOn: false, dropdown: -1, help: false, motion: false, latch: true, gate: false,
  partial: 4, breath: 0.6, vibRate: 5.2, vibDepth: 0.15,
  valve: [0, 0, 0], fader: [0.8, 0.6, 0.4], pot: [0.5, 0.5], fsr: 0, joy: [0, 0], joyClick: 0, enc: [0, 0], encPos: [0, 0], encClick: [0, 0],
  tilt: [0, 0], shake: 0, lidar: 0, pads: 0, keyPads: 0, keyValve: [0, 0, 0], dragValve: [-1, -1, -1], cpuHist: [], inPk: 0, outPk: 0, note: '',
};
try { const j = JSON.parse(localStorage.getItem('hxsim') || 'null'); if (j) { for (const k of ['partial', 'breath', 'vibRate', 'vibDepth', 'latch']) if (j[k] !== undefined) U[k] = j[k]; if (j.fader) U.fader = j.fader; if (j.pot) U.pot = j.pot; } } catch (e) {}
function saveUI() { try { localStorage.setItem('hxsim', JSON.stringify({ partial: U.partial, breath: U.breath, vibRate: U.vibRate, vibDepth: U.vibDepth, latch: U.latch, fader: U.fader, pot: U.pot })); } catch (e) {} }

// ------------------------------------------------------------------------------------------------ audio plumbing
let ctx = null, node = null, micStream = null, micSrc = null, statsResolvers = [];
function post(m, tr) { if (node) node.port.postMessage(m, tr || []); }

async function setupAudio() {
  try {
    ctx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 48000, latencyHint: 'interactive' });
    await ctx.audioWorklet.addModule('worklet.js');
    node = new AudioWorkletNode(ctx, 'hx-sim', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2], channelCount: 1, channelCountMode: 'explicit' });
    node.connect(ctx.destination);
    node.port.onmessage = onWorkletMsg;
    const bytes = await (await fetch('hxsim.wasm')).arrayBuffer();
    post({ t: 'wasm', bytes }, [bytes]);
  } catch (e) { S.err = 'audio setup failed: ' + (e && e.message || e); redraw(); console.warn(S.err); }
}
function onWorkletMsg(ev) {
  const m = ev.data;
  if (m.t === 'ready') { S.effects = m.effects; S.ready = true; S.perf = m.perf; S.sr = m.sampleRate; S.block = m.block; sendCtl(true); sendTrumpet(); redraw(); }
  else if (m.t === 'state') {
    for (let s = 0; s < 4; s++) {
      const a = S.slots[s], b = m.slots[s];
      if (b.desc) a.desc = b.desc;
      a.type = b.type; a.byp = b.byp;
      if (drag && drag.kind === 'param' && drag.slot === s) { const keep = a.vals[drag.idx]; a.vals = b.vals; a.vals[drag.idx] = keep; } else a.vals = b.vals;
    }
    S.met = m.met; S.leds = m.leds; S.status = m.status; S.page = m.page; S.sel = m.sel; S.selParam = m.selParam; S.cpuMs = m.cpuBlockMs; S.rms = m.rms; S.peak = Math.max(S.peak * 0.9, m.peak);
    U.inPk = Math.max(U.inPk * 0.85, m.met[0]); U.outPk = Math.max(U.outPk * 0.85, m.met[1]);
    if (U.running) { U.cpuHist.push(m.cpuBlockMs); if (U.cpuHist.length > 100) U.cpuHist.shift(); }
    redraw();
  } else if (m.t === 'stats') { for (const r of statsResolvers) r(m); statsResolvers = []; }
  else if (m.t === 'error') { S.err = m.msg; console.error('worklet:', m.msg); redraw(); }
}

async function startStop() {
  if (!ctx) return;
  if (U.running) { await ctx.suspend(); U.running = false; U.cpuHist.length = 0; }
  else { await ctx.resume(); U.running = true; }
  redraw();
}
async function setMode(m) {
  if (m === 2) {
    try {
      if (!micStream) micStream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1 } });
      if (!micSrc) { micSrc = ctx.createMediaStreamSource(micStream); micSrc.connect(node); }
      U.micOn = true;
    } catch (e) { S.err = 'mic: ' + (e.message || e); redraw(); return; }
  }
  U.mode = m; post({ t: 'mode', mode: m }); redraw();
}
async function loadFile(file) {
  try {
    const buf = await file.arrayBuffer();
    const ab = await ctx.decodeAudioData(buf);
    const n = ab.length, mono = new Float32Array(n), chs = ab.numberOfChannels;
    for (let c = 0; c < chs; c++) { const d = ab.getChannelData(c); for (let i = 0; i < n; i++) mono[i] += d[i] / chs; }
    U.fileName = file.name; post({ t: 'file', data: mono }, [mono.buffer]); await setMode(1);
  } catch (e) { S.err = 'file: ' + (e.message || e); redraw(); }
}
function pickFile() { const i = document.createElement('input'); i.type = 'file'; i.accept = 'audio/*,.wav,.mp3'; i.onchange = () => { if (i.files[0]) loadFile(i.files[0]); }; i.click(); }

// ------------------------------------------------------------------------------------------------ control packing / posting
let ctlDirty = false, ctlTimer = 0, lastSend = 0; const paramQ = new Map();
function packCtl() {
  const a = new Float32Array(21);
  for (let i = 0; i < 3; i++) { a[i] = Math.max(U.valve[i], U.keyValve[i]); a[3 + i] = U.fader[i]; }
  a[6] = U.pot[0]; a[7] = U.pot[1]; a[8] = U.fsr; a[9] = U.joy[0]; a[10] = U.joy[1]; a[11] = U.joyClick;
  a[12] = U.enc[0]; a[13] = U.enc[1]; a[14] = U.encClick[0]; a[15] = U.encClick[1];
  a[16] = U.tilt[0]; a[17] = U.tilt[1]; a[18] = U.shake; a[19] = U.lidar; a[20] = (U.pads | U.keyPads) & 0xffff;
  return a;
}
function sendCtl(force) { if (!node) return; const a = packCtl(); U.enc[0] = U.enc[1] = 0; post({ t: 'ctl', a }); lastSend = performance.now(); ctlDirty = false; }
function sendTrumpet() { post({ t: 'trumpet', a: [U.partial, U.breath, U.vibRate, U.vibDepth], latch: U.latch, gate: U.gate }); }
function flush() {
  ctlTimer = 0; if (ctlDirty) sendCtl();
  for (const [k, v] of paramQ) post({ t: 'param', slot: v.slot, idx: v.idx, v: v.v }); paramQ.clear();
}
function touchCtl() {   // mark dirty; coalesce to <= 100 Hz, no timer when nothing is touched
  ctlDirty = true; if (!ctlTimer) { const dt = performance.now() - lastSend; ctlTimer = setTimeout(flush, Math.max(0, 10 - dt)); }
}
function queueParam(slot, idx, v) { paramQ.set(slot + ':' + idx, { slot, idx, v }); if (!ctlTimer) ctlTimer = setTimeout(flush, 10); }

// ------------------------------------------------------------------------------------------------ canvas + draw helpers
const cv = document.getElementById('c'), g = cv.getContext('2d');
let scale = 1, ox = 0, oy = 0, dpr = 1, rafPending = false;
function resize() {
  dpr = window.devicePixelRatio || 1;
  const w = window.innerWidth, h = window.innerHeight;
  scale = Math.min(w / LW, h / LH); const cw = LW * scale, ch = LH * scale; ox = (w - cw) / 2; oy = (h - ch) / 2;
  cv.style.width = w + 'px'; cv.style.height = h + 'px'; cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
  redraw();
}
function redraw() { if (!rafPending) { rafPending = true; requestAnimationFrame(() => { rafPending = false; draw(); }); } }

function rr(x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
function txt(s, x, y, size, col, align, weight, mono) { g.font = (weight || 700) + ' ' + size + 'px ' + (mono ? MONO : FONT); g.fillStyle = col || C.text; g.textAlign = align || 'left'; g.textBaseline = 'alphabetic'; g.fillText(s, x, y); }
function panel(x, y, w, h, title, col) { rr(x, y, w, h, 12); g.fillStyle = col || C.panel; g.fill(); g.strokeStyle = C.line; g.lineWidth = 1; g.stroke(); if (title) txt(title, x + 14, y + 22, 11, C.dim, 'left', 800); }
function lerpCol(t) { const a = [138, 77, 255], b = [255, 63, 168]; return 'rgb(' + a.map((v, i) => Math.round(v + (b[i] - v) * t)).join(',') + ')'; }
function btn(r, label, on, col) {
  rr(r[0], r[1], r[2], r[3], 8); g.fillStyle = on ? (col || C.violet) : C.panel2; g.fill(); g.strokeStyle = on ? (col || C.violet2) : C.line; g.stroke();
  txt(label, r[0] + r[2] / 2, r[1] + r[3] / 2 + 4, 11, on ? '#fff' : C.dim, 'center', 800);
}

// ------------------------------------------------------------------------------------------------ widgets (hit regions)
const hits = [];   // {r:[x,y,w,h], z, down(x,y), move(x,y), up(), wheel(dy), kind}
function hit(r, o) { o.r = r; hits.push(o); return o; }
function inR(r, x, y) { return x >= r[0] && x <= r[0] + r[2] && y >= r[1] && y <= r[3] + r[1]; }
let drag = null;
const clamp = (v, a, b) => v < a ? a : (v > b ? b : v);

function vslider(r, val, label, o) {      // vertical slider. val 0..1
  o = o || {};
  const [x, y, w, h] = r, cx = x + w / 2;
  rr(cx - 5, y, 10, h, 5); g.fillStyle = '#0d0618'; g.fill(); g.strokeStyle = C.line; g.stroke();
  const ty = y + h - val * h;
  rr(cx - 5, ty, 10, y + h - ty, 5); g.fillStyle = o.col || C.violet; g.fill();
  rr(x + 2, ty - 8, w - 4, 16, 6); g.fillStyle = val > 0.001 ? (o.col2 || C.violet2) : C.dim2; g.fill();
  if (label) txt(label, cx, y + h + 18, 11, C.dim, 'center', 800);
  if (o.sub) txt(o.sub, cx, y + h + 32, 10, C.dim2, 'center', 700, true);
  if (o.value) txt(o.value, cx, y - 6, 10, C.text, 'center', 700, true);
  const setv = (py) => o.set(clamp(1 - (py - y) / h, 0, 1));
  hit([x - 4, y - 8, w + 8, h + 16], { down: (px, py) => { drag = { kind: 'v' }; setv(py); }, move: (px, py) => setv(py), up: () => { if (o.spring) o.set(0, true); }, wheel: (d) => o.set(clamp(val - d * 0.0015, 0, 1)) });
}
function knob(cx, cy, rad, val, label, o) {
  const a0 = Math.PI * 0.75, a1 = Math.PI * 2.25;
  g.lineWidth = 6; g.lineCap = 'round';
  g.beginPath(); g.arc(cx, cy, rad, a0, a1); g.strokeStyle = '#0d0618'; g.stroke();
  g.beginPath(); g.arc(cx, cy, rad, a0, a0 + (a1 - a0) * val); g.strokeStyle = C.violet; g.stroke();
  const a = a0 + (a1 - a0) * val; g.beginPath(); g.moveTo(cx + Math.cos(a) * rad * 0.4, cy + Math.sin(a) * rad * 0.4); g.lineTo(cx + Math.cos(a) * rad * 0.9, cy + Math.sin(a) * rad * 0.9); g.strokeStyle = C.magenta; g.lineWidth = 3; g.stroke();
  g.lineCap = 'butt';
  txt(label, cx, cy + rad + 26, 11, C.dim, 'center', 800); txt(val.toFixed(2), cx, cy + 4, 11, C.text, 'center', 700, true);
  let y0 = 0, v0 = 0;
  hit([cx - rad - 8, cy - rad - 8, rad * 2 + 16, rad * 2 + 16], { down: (px, py) => { drag = { kind: 'knob' }; y0 = py; v0 = val; }, move: (px, py) => o.set(clamp(v0 + (y0 - py) / 150, 0, 1)), wheel: (d) => o.set(clamp(val - d * 0.001, 0, 1)) });
}
function pad2d(r, vx, vy, label, o) {
  const [x, y, w, h] = r;
  rr(x, y, w, h, 10); g.fillStyle = '#0d0618'; g.fill(); g.strokeStyle = C.line; g.stroke();
  g.strokeStyle = C.line; g.beginPath(); g.moveTo(x + w / 2, y + 8); g.lineTo(x + w / 2, y + h - 8); g.moveTo(x + 8, y + h / 2); g.lineTo(x + w - 8, y + h / 2); g.stroke();
  const px = x + (vx + 1) / 2 * w, py = y + (1 - (vy + 1) / 2) * h;
  g.beginPath(); g.arc(px, py, 14, 0, 7); g.fillStyle = o.click ? C.magenta : C.violet; g.fill(); g.strokeStyle = C.violet2; g.lineWidth = 2; g.stroke(); g.lineWidth = 1;
  txt(label, x + 8, y + h - 8, 10, C.dim2, 'left', 800);
  txt(vx.toFixed(2) + ' ' + vy.toFixed(2), x + w - 8, y + h - 8, 10, C.dim, 'right', 700, true);
  const setp = (qx, qy) => o.set(clamp((qx - x) / w * 2 - 1, -1, 1), clamp(1 - (qy - y) / h * 2, -1, 1));
  hit(r, { down: (qx, qy, ev) => { drag = { kind: 'pad' }; setp(qx, qy); }, move: setp, up: () => { if (o.spring !== false) o.set(0, 0, true); },
    dbl: () => o.dbl && o.dbl() });
}
function encoder(cx, cy, rad, i, label) {
  const ang = U.encPos[i] * 0.45;
  g.beginPath(); g.arc(cx, cy, rad, 0, 7); g.fillStyle = '#0d0618'; g.fill(); g.strokeStyle = C.line; g.lineWidth = 2; g.stroke();
  for (let k = 0; k < 24; k++) { const a = k / 24 * 6.2832 + ang; g.beginPath(); g.moveTo(cx + Math.cos(a) * (rad - 6), cy + Math.sin(a) * (rad - 6)); g.lineTo(cx + Math.cos(a) * rad, cy + Math.sin(a) * rad); g.strokeStyle = k === 0 ? C.magenta : C.dim2; g.stroke(); }
  g.lineWidth = 1;
  g.beginPath(); g.arc(cx, cy, rad * 0.45, 0, 7); g.fillStyle = U.encClick[i] ? C.magenta : C.panel2; g.fill(); g.strokeStyle = C.violet; g.stroke();
  txt('CLICK', cx, cy + 3, 9, U.encClick[i] ? '#fff' : C.dim, 'center', 800);
  txt(label, cx, cy + rad + 20, 11, C.dim, 'center', 800);
  let y0 = 0, acc = 0, mode = 0;
  hit([cx - rad - 4, cy - rad - 4, rad * 2 + 8, rad * 2 + 8], {
    down: (px, py) => { y0 = py; acc = 0; mode = (Math.hypot(px - cx, py - cy) < rad * 0.45) ? 1 : 0; drag = { kind: 'enc' }; if (mode) { U.encClick[i] = 1; touchCtl(); redraw(); } },
    move: (px, py) => { if (mode) return; acc += (y0 - py); y0 = py; const d = Math.trunc(acc / 14); if (d) { acc -= d * 14; encStep(i, d); } },
    up: () => { if (mode) { U.encClick[i] = 0; touchCtl(); redraw(); } },
    wheel: (d) => encStep(i, d > 0 ? -1 : 1) });
}
function encStep(i, d) { U.enc[i] += d; U.encPos[i] += d; touchCtl(); redraw(); }

// ------------------------------------------------------------------------------------------------ param formatting
function pNorm(p, v) { if ((p.flags & 128) && p.min > 0) return clamp(Math.log(v / p.min) / Math.log(p.max / p.min), 0, 1); return clamp((v - p.min) / ((p.max - p.min) || 1), 0, 1); }
function pDenorm(p, n) {
  let v = ((p.flags & 128) && p.min > 0) ? p.min * Math.pow(p.max / p.min, n) : p.min + n * (p.max - p.min);
  if (p.flags & (1 | 8 | 64)) { const q = (p.flags & (8 | 64)) ? 1 : (p.step > 0 ? p.step : 1); v = Math.round(v / q) * q; }
  return clamp(v, p.min, p.max);
}
function pFmt(p, v) {
  if (p.flags & 8 && p.names) return p.names[clamp(Math.round(v - p.min), 0, p.names.length - 1)] || String(Math.round(v));
  if (p.flags & 4 && p.max <= 1.001) return Math.round(v * 100) + '%';
  if (p.flags & (64)) return String(Math.round(v));
  const a = Math.abs(v); return a >= 1000 ? v.toFixed(0) : a >= 100 ? v.toFixed(0) : a >= 10 ? v.toFixed(1) : v.toFixed(2);
}

// ------------------------------------------------------------------------------------------------ slot panel
const SLOT_X = (i) => 20 + i * 313, SLOT_Y = 64, SLOT_W = 301, SLOT_H = 296;
function drawSlot(i) {
  const x = SLOT_X(i), y = SLOT_Y, w = SLOT_W, h = SLOT_H, sl = S.slots[i], d = sl.desc;
  const sel = S.sel === i;
  rr(x, y, w, h, 14); g.fillStyle = C.panel; g.fill(); g.strokeStyle = sel ? C.magenta : C.line; g.lineWidth = sel ? 2 : 1; g.stroke(); g.lineWidth = 1;
  // header
  hit([x, y, 44, 44], { down: () => { post({ t: 'select', slot: i }); } });
  txt(String(i + 1), x + 14, y + 33, 28, sel ? C.magenta : C.violet, 'left', 900);
  const eff = S.effects[sl.type];
  hit([x + 44, y + 4, w - 130, 36], { down: () => { U.dropdown = U.dropdown === i ? -1 : i; redraw(); } });
  txt(eff ? eff.name : '(empty)', x + 50, y + 26, 18, sl.byp ? C.dim2 : C.text, 'left', 900);
  txt((eff ? eff.id : '') + '  ▾', x + 50, y + 38, 10, C.dim, 'left', 700, true);
  const br = [x + w - 74, y + 10, 62, 26];
  btn(br, sl.byp ? 'BYPASS' : 'ON', !sl.byp, sl.byp ? null : C.violet);
  hit(br, { down: () => { sl.byp = sl.byp ? 0 : 1; post({ t: 'bypass', slot: i, b: sl.byp }); redraw(); } });
  // cpu bar
  const ns = S.met[5 + i], budget = (S.block / S.sr) * 1e9, frac = clamp(ns / budget, 0, 1);
  g.fillStyle = '#0d0618'; g.fillRect(x + 12, y + 46, w - 24, 3); g.fillStyle = lerpCol(frac); g.fillRect(x + 12, y + 46, (w - 24) * frac, 3);
  if (!d) return;
  // params
  const ry = y + 55, rh = 17;
  for (let p = 0; p < d.params.length; p++) {
    const q = d.params[p], v = sl.vals[p] !== undefined ? sl.vals[p] : q.min, yy = ry + p * rh;
    const bx = x + 78, bw = 142;
    txt(q.name, x + 12, yy + 13, 11, q.flags & 32 ? C.dim2 : C.violet2, 'left', 800, true);
    const rl = q.role; if (rl >= 0) txt(ROLE[rl][0] + (ROLE[rl] === 'TIME' || ROLE[rl] === 'TONE' ? ROLE[rl][1] : ''), x + 62, yy + 13, 8, C.magenta, 'right', 800);
    const nrm = pNorm(q, v);
    rr(bx, yy + 3, bw, 12, 6); g.fillStyle = '#0d0618'; g.fill();
    g.save(); rr(bx, yy + 3, bw, 12, 6); g.clip();
    g.fillStyle = (q.flags & 16) ? C.magenta : (p === 0 ? C.violet2 : C.violet);
    if (q.flags & 16) { const c0 = bx + bw * pNorm(q, 0 < q.min ? q.min : (0 > q.max ? q.max : 0)); g.fillRect(Math.min(c0, bx + bw * nrm), yy + 3, Math.abs(bx + bw * nrm - c0), 12); }
    else g.fillRect(bx, yy + 3, bw * nrm, 12);
    g.restore();
    txt(pFmt(q, v), x + w - 10, yy + 13, 11, C.text, 'right', 700, true);
    const setv = (px) => { const n = clamp((px - bx) / bw, 0, 1); const nv = pDenorm(q, n); sl.vals[p] = nv; queueParam(i, p, nv); redraw(); };
    hit([x + 70, yy, w - 74, rh], { down: (px) => { drag = { kind: 'param', slot: i, idx: p }; setv(px); }, move: (px) => setv(px),
      up: () => { drag = null; }, wheel: (dy) => { const n = clamp(pNorm(q, sl.vals[p]) - dy * 0.001, 0, 1); const nv = pDenorm(q, n); sl.vals[p] = nv; queueParam(i, p, nv); redraw(); } });
  }
  // actions
  const names = ['MAIN', 'ALT', 'CLEAR', 'HOLD'], ay = y + h - 30;
  for (let a = 0; a < 4; a++) {
    const r = [x + 12 + a * 71, ay, 66, 22]; btn(r, names[a], U.actDown === i * 4 + a, C.magenta);
    hit(r, { down: () => { U.actDown = i * 4 + a; post({ t: 'action', slot: i, id: a, v: 1 }); redraw(); }, up: () => { if (a === 3) post({ t: 'action', slot: i, id: 3, v: 0 }); U.actDown = -1; redraw(); } });
  }
}

function drawDropdown() {
  const i = U.dropdown; if (i < 0) return;
  const x = SLOT_X(i), y = SLOT_Y + 44, w = SLOT_W, n = S.effects.length, rowH = 26, h = n * rowH + 12;
  hit([0, 0, LW, LH], { z: 5, down: () => { U.dropdown = -1; redraw(); } });
  rr(x, y, w, h, 12); g.fillStyle = '#10061f'; g.fill(); g.strokeStyle = C.violet; g.lineWidth = 2; g.stroke(); g.lineWidth = 1;
  for (let k = 0; k < n; k++) {
    const e = S.effects[k], yy = y + 6 + k * rowH, cur = S.slots[i].type === k;
    if (cur) { rr(x + 6, yy, w - 12, rowH - 2, 6); g.fillStyle = C.violet; g.fill(); }
    txt(e.name, x + 16, yy + 17, 12, cur ? '#fff' : C.text, 'left', 800); txt(e.id, x + 110, yy + 17, 10, C.dim, 'left', 700, true);
    hit([x + 6, yy, w - 12, rowH - 2], { z: 6, down: () => { U.dropdown = -1; post({ t: 'fx', slot: i, id: k }); redraw(); } });
  }
}

// ------------------------------------------------------------------------------------------------ header
function drawHeader() {
  txt('HYPERTRUMPET', 20, 38, 24, C.violet, 'left', 900); g.font = '900 24px ' + FONT; const tw = g.measureText('HYPERTRUMPET').width; txt('V2 SIM', 20 + tw + 10, 38, 24, C.magenta, 'left', 900);
  const sb = [352, 14, 96, 36]; btn(sb, U.running ? 'STOP' : (S.ready ? 'START' : '...'), U.running, U.running ? C.magenta : C.violet);
  hit(sb, { down: () => { startStop(); } });
  const labs = ['SYNTH', 'FILE', 'MIC'];
  for (let k = 0; k < 3; k++) { const r = [462 + k * 66, 14, 60, 36]; btn(r, labs[k], U.mode === k); hit(r, { down: () => { if (k === 1 && !U.fileName) pickFile(); else setMode(k); } }); }
  const lr = [664, 14, 54, 36]; btn(lr, 'LOAD', false); hit(lr, { down: () => pickFile() });
  txt(U.fileName ? U.fileName.slice(0, 22) : 'or drop a WAV/MP3', 724, 36, 10, C.dim, 'left', 700, true);
  // in/out meters
  const mt = (lab, v, x, y) => { txt(lab, x, y + 9, 10, C.dim, 'left', 800); g.fillStyle = '#0d0618'; g.fillRect(x + 26, y, 110, 10); const f = clamp(Math.sqrt(v), 0, 1); g.fillStyle = lerpCol(f); g.fillRect(x + 26, y, 110 * f, 10); };
  mt('IN', U.inPk, 900, 14); mt('OUT', U.outPk, 900, 34);
  // CPU box
  const bx = 1048, by = 8, bw = 190, bh = 48; rr(bx, by, bw, bh, 8); g.fillStyle = C.panel; g.fill(); g.strokeStyle = C.line; g.stroke();
  const budgetMs = S.block / S.sr * 1000, pct = S.cpuMs / budgetMs * 100, eng = S.met[2] / 1e6, engPct = eng / budgetMs * 100;
  txt('WASM DSP ' + (U.running ? pct.toFixed(1) : '--') + '%', bx + 8, by + 17, 13, pct > 80 ? C.warn : C.text, 'left', 800, true);
  txt(S.cpuMs.toFixed(3) + ' ms / ' + budgetMs.toFixed(2) + ' ms blk', bx + 8, by + 30, 9, C.dim, 'left', 700, true);
  txt('NOT SEED TIMING' + (S.perf ? '' : ' (1ms clk)'), bx + 8, by + 42, 8, C.warn, 'left', 800);
  const hh = U.cpuHist; if (hh.length > 1) { g.beginPath(); for (let k = 0; k < hh.length; k++) { const px = bx + 120 + k * 0.66, py = by + bh - 6 - clamp(hh[k] / budgetMs, 0, 1) * (bh - 14); k ? g.lineTo(px, py) : g.moveTo(px, py); } g.strokeStyle = C.magenta; g.stroke(); }
  const hb = [1250, 14, 30, 36]; btn(hb, '?', U.help, C.violet); hit(hb, { down: () => { U.help = !U.help; redraw(); } });
}

// ------------------------------------------------------------------------------------------------ controls
const NOTE = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];
function noteName(f) { if (!(f > 20)) return '--'; const m = 69 + 12 * Math.log2(f / 440), r = Math.round(m); return NOTE[((r % 12) + 12) % 12] + (Math.floor(r / 12) - 1) + (Math.abs(m - r) > 0.15 ? (m > r ? '+' : '-') + Math.round(Math.abs(m - r) * 100) + 'c' : ''); }

function drawControls() {
  const A = 390;
  // TRUMPET
  panel(20, A, 300, 250, 'TRUMPET  (synthetic input)');
  txt('PARTIAL', 34, A + 46, 10, C.dim, 'left', 800);
  for (let k = 0; k < 8; k++) { const p = k + 2, r = [34 + (k % 4) * 64, A + 54 + Math.floor(k / 4) * 34, 58, 28]; btn(r, String(p), U.partial === p); hit(r, { down: () => { U.partial = p; sendTrumpet(); saveUI(); redraw(); } }); }
  const bh = 92, by = A + 134;
  vslider([40, by, 44, bh], U.breath, 'BREATH', { value: U.breath.toFixed(2), set: (v) => { U.breath = v; sendTrumpet(); saveUI(); redraw(); } });
  vslider([116, by, 30, bh - 20], clamp((U.vibRate - 2) / 8, 0, 1), 'VIB Hz', { value: U.vibRate.toFixed(1), col: C.dim2, set: (v) => { U.vibRate = 2 + v * 8; sendTrumpet(); saveUI(); redraw(); } });
  vslider([166, by, 30, bh - 20], U.vibDepth, 'VIB DEP', { value: U.vibDepth.toFixed(2), col: C.dim2, set: (v) => { U.vibDepth = v; sendTrumpet(); saveUI(); redraw(); } });
  const lb = [212, A + 130, 96, 28]; btn(lb, 'LATCH', U.latch); hit(lb, { down: () => { U.latch = !U.latch; sendTrumpet(); saveUI(); redraw(); } });
  const tb = [212, A + 164, 96, 28]; btn(tb, 'TONGUE [T]', U.tongueDown, C.magenta); hit(tb, { down: () => { U.tongueDown = 1; post({ t: 'tongue' }); redraw(); }, up: () => { U.tongueDown = 0; redraw(); } });
  const gb = [212, A + 198, 96, 28]; btn(gb, 'GATE [SPC]', U.gate, C.magenta); hit(gb, { down: () => { U.gate = true; sendTrumpet(); redraw(); }, up: () => { U.gate = false; sendTrumpet(); redraw(); } });
  hit([20, A, 300, 250], { z: -1, wheel: (d) => { U.breath = clamp(U.breath - d * 0.0015, 0, 1); sendTrumpet(); redraw(); } });
  // VALVES
  panel(332, A, 180, 250, 'VALVES  (Hall depth)');
  const vk = ['J', 'K', 'L'], vs = ['-2 st', '-1 st', '-3 st'], vn = ['1', '2', '3'];
  for (let i = 0; i < 3; i++) {
    const vv = Math.max(U.valve[i], U.keyValve[i]);
    vslider([350 + i * 52, A + 52, 40, 150], vv, 'V' + vn[i] + ' [' + vk[i] + ']', { sub: vs[i], value: vv.toFixed(2), spring: true, col: vv >= 0.5 ? C.magenta : C.violet,
      set: (v, rel) => { U.valve[i] = rel ? 0 : v; touchCtl(); redraw(); } });
  }
  // FADERS
  panel(524, A, 180, 250, 'FADERS');
  for (let i = 0; i < 3; i++) vslider([542 + i * 52, A + 52, 40, 150], U.fader[i], 'F' + (i + 1), { value: U.fader[i].toFixed(2), set: (v) => { U.fader[i] = v; touchCtl(); saveUI(); redraw(); } });
  // POTS + LIDAR
  panel(716, A, 200, 250, 'POTS  /  LIDAR');
  for (let i = 0; i < 2; i++) knob(766 + i * 100, A + 78, 30, U.pot[i], 'POT ' + (i + 1), { set: (v) => { U.pot[i] = v; touchCtl(); saveUI(); redraw(); } });
  txt('LIDAR  (hand near bell = right)', 728, A + 152, 10, C.dim, 'left', 800);
  const lx = 730, lw = 170; rr(lx, A + 162, lw, 12, 6); g.fillStyle = '#0d0618'; g.fill(); g.strokeStyle = C.line; g.stroke(); rr(lx, A + 162, Math.max(12, lw * U.lidar), 12, 6); g.fillStyle = C.violet; g.fill();
  txt(U.lidar.toFixed(2), lx + lw, A + 192, 11, C.text, 'right', 700, true);
  hit([lx - 6, A + 150, lw + 12, 40], { down: (px) => { drag = { kind: 'lidar' }; U.lidar = clamp((px - lx) / lw, 0, 1); touchCtl(); redraw(); }, move: (px) => { U.lidar = clamp((px - lx) / lw, 0, 1); touchCtl(); redraw(); } });
  const sb2 = [730, A + 204, 170, 26]; btn(sb2, 'SHAKE (hold)', U.shake > 0, C.magenta); hit(sb2, { down: () => { U.shake = 1; touchCtl(); redraw(); }, up: () => { U.shake = 0; touchCtl(); redraw(); } });
  // FSR
  panel(928, A, 84, 250, 'FSR');
  vslider([950, A + 52, 40, 150], U.fsr, 'PRESS', { value: U.fsr.toFixed(2), spring: true, col: C.magenta, set: (v, rel) => { U.fsr = rel ? 0 : v; touchCtl(); redraw(); } });
  // ENCODERS
  panel(1024, A, 236, 250, 'ENCODERS  (drag / wheel = detents)');
  encoder(1084, A + 100, 38, 0, 'ENC 1  [ ] B'); encoder(1200, A + 100, 38, 1, 'ENC 2  - = N');
  txt('detents: ' + U.encPos[0] + '   ' + U.encPos[1], 1142, A + 190, 10, C.dim, 'center', 700, true);

  // row B
  const B = 650;
  panel(20, B, 230, 250, 'JOYSTICK  (click = G / dbl-click)');
  pad2d([34, B + 34, 202, 202], U.joy[0], U.joy[1], 'JOY', { click: U.joyClick, set: (x, y, rel) => { U.joy = [x, y]; touchCtl(); redraw(); }, dbl: () => { U.joyClick = 1; touchCtl(); redraw(); setTimeout(() => { U.joyClick = 0; touchCtl(); redraw(); }, 150); } });
  panel(262, B, 230, 250, 'TILT  (roll X / pitch Y)');
  pad2d([276, B + 34, 202, 170], U.tilt[0], U.tilt[1], 'TILT', { spring: !U.motion, set: (x, y) => { U.tilt = [x, y]; touchCtl(); redraw(); } });
  const mb = [276, B + 214, 100, 24]; btn(mb, 'MOTION', U.motion); hit(mb, { down: () => enableMotion() });
  txt(U.motion ? 'device orientation on' : 'phone: tap MOTION', 386, B + 230, 9, C.dim, 'left', 700, true);
  // TRELLIS
  panel(504, B, 250, 250, 'TRELLIS  (1234 QWER ASDF ZXCV)');
  const cs = 48, gp = 4;
  for (let k = 0; k < 16; k++) {
    const px = 527 + (k % 4) * (cs + gp), py = B + 38 + Math.floor(k / 4) * (cs + gp);
    const r = S.leds[k * 3], gg = S.leds[k * 3 + 1], b = S.leds[k * 3 + 2], lit = r + gg + b > 8, pr = ((U.pads | U.keyPads) >> k) & 1;
    rr(px, py, cs, cs, 8); g.fillStyle = lit ? 'rgb(' + r + ',' + gg + ',' + b + ')' : '#0d0618'; g.fill(); g.strokeStyle = pr ? '#fff' : C.line; g.lineWidth = pr ? 3 : 1; g.stroke(); g.lineWidth = 1;
    txt('1234QWERASDFZXCV'[k], px + 6, py + 14, 9, lit ? 'rgba(255,255,255,.7)' : C.dim2, 'left', 800, true);
    hit([px, py, cs, cs], { down: () => { drag = { kind: 'pad' }; U.pads |= 1 << k; touchCtl(); redraw(); }, up: () => { U.pads &= ~(1 << k); touchCtl(); redraw(); } });
  }
  // MAPPER / READOUT
  panel(766, B, 494, 250, 'MAPPER / INPUT');
  txt('PAGE ' + S.page + '   SLOT ' + (S.sel + 1) + '   PARAM ' + S.selParam, 782, B + 52, 16, C.text, 'left', 900, true);
  txt(S.status || '(mapper status empty)', 782, B + 76, 13, C.violet2, 'left', 700, true);
  const f0h = S.met[10], f0t = S.met[3];
  txt('f0 hint (valves)  ' + (f0h > 0 ? f0h.toFixed(1) + ' Hz  ' + noteName(f0h) : '--'), 782, B + 106, 12, C.text, 'left', 700, true);
  txt('f0 tracked       ' + (f0t > 0 ? f0t.toFixed(1) + ' Hz  ' + noteName(f0t) : '--'), 782, B + 124, 12, C.text, 'left', 700, true);
  txt('env ' + S.met[4].toFixed(2) + '   out rms ' + S.rms.toFixed(3) + '   peak ' + S.peak.toFixed(2), 782, B + 142, 12, C.dim, 'left', 700, true);
  txt('synth ' + (S.met[9] / 1e3).toFixed(1) + ' us/blk   engine EMA ' + (S.met[2] / 1e3).toFixed(1) + ' us/blk', 782, B + 160, 11, C.dim, 'left', 700, true);
  txt('keys: J K L valves  SPACE breath  T tongue  G joy-click  [ ] enc1  - = enc2  ↑↓ partial  ? help', 782, B + 190, 10, C.dim2, 'left', 700, true);
  const pb = [782, B + 202, 120, 24]; btn(pb, 'DEFAULT PATCH', false); hit(pb, { down: () => post({ t: 'patch' }) });
  if (S.err) { g.fillStyle = 'rgba(255,63,168,.18)'; g.fillRect(0, LH - 24, LW, 24); txt(S.err.slice(0, 160), 14, LH - 8, 11, C.magenta, 'left', 700, true); }
}

function drawHelp() {
  hit([0, 0, LW, LH], { z: 7, down: () => { U.help = false; redraw(); } });
  g.fillStyle = 'rgba(8,3,16,.92)'; g.fillRect(0, 0, LW, LH);
  txt('KEYBOARD', 80, 100, 34, C.violet, 'left', 900);
  const rows = [['J K L', 'valves 1 2 3 (hold). A S D are Trellis keys, so valves live on J K L'], ['SPACE / SHIFT', 'breath gate (hold) when LATCH is off'], ['T', 'tongue: retrigger the attack'],
    ['↑ ↓', 'embouchure partial (2..9)'], ['1234 QWER ASDF ZXCV', 'Trellis 4x4 pads, row by row'], ['G', 'joystick click (held)'], ['B  N', 'encoder 1 / 2 click (held)'], ['[  ]', 'encoder 1 detent - / +'], ['-  =', 'encoder 2 detent - / +'],
    ['H  or  ?', 'this help'], ['ESC', 'close overlays'], ['mouse wheel', 'on a param bar: nudge. on TRUMPET panel: breath. on a fader/knob/encoder: change']];
  rows.forEach((r, i) => { txt(r[0], 80, 160 + i * 36, 15, C.magenta, 'left', 900, true); txt(r[1], 360, 160 + i * 36, 14, C.text, 'left', 600); });
  txt('Mouse: drag valves/faders/knobs. FSR and joystick/tilt spring back on release. Click a slot name for the effect list.', 80, 640, 14, C.dim, 'left', 600);
  txt('The DSP is the firmware C++ compiled to WebAssembly. CPU % is laptop WASM, not Seed timing.', 80, 666, 14, C.dim, 'left', 600);
  txt('click anywhere to close', 80, 720, 12, C.dim2, 'left', 700);
}

function draw() {
  g.setTransform(dpr, 0, 0, dpr, 0, 0); g.fillStyle = C.bg; g.fillRect(0, 0, cv.width / dpr, cv.height / dpr);
  g.setTransform(dpr * scale, 0, 0, dpr * scale, dpr * ox, dpr * oy);
  hits.length = 0;
  drawHeader();
  for (let i = 0; i < 4; i++) drawSlot(i);
  if (S.ready) txt('mapper: ' + (S.status || '-'), 24, 376, 11, C.dim, 'left', 700, true);
  drawControls();
  drawDropdown();
  if (U.help) drawHelp();
  if (!S.ready && !S.err) txt('loading wasm...', LW / 2, LH / 2, 16, C.dim, 'center', 800);
}

// ------------------------------------------------------------------------------------------------ pointer routing
function lp(e) { const r = cv.getBoundingClientRect(); return [(e.clientX - r.left - ox) / scale, (e.clientY - r.top - oy) / scale]; }
const active = new Map();
function topHit(x, y, key) {
  let best = null;
  for (const h of hits) { if (!h[key] && key !== 'any') continue; if (inR(h.r, x, y) && (!best || (h.z || 0) >= (best.z || 0))) best = h; }
  return best;
}
cv.addEventListener('pointerdown', (e) => {
  const [x, y] = lp(e); const h = topHit(x, y, 'down'); if (!h) return;
  try { cv.setPointerCapture(e.pointerId); } catch (_) {}
  active.set(e.pointerId, h); drag = null; h.down(x, y, e);
  if (e.detail === 2 && h.dbl) h.dbl();
  e.preventDefault();
});
cv.addEventListener('pointermove', (e) => { const h = active.get(e.pointerId); if (h && h.move) { const [x, y] = lp(e); h.move(x, y); } });
function pend(e) { const h = active.get(e.pointerId); if (h) { active.delete(e.pointerId); if (h.up) h.up(); drag = null; redraw(); } }
cv.addEventListener('pointerup', pend); cv.addEventListener('pointercancel', pend);
cv.addEventListener('wheel', (e) => { const [x, y] = lp(e); const h = topHit(x, y, 'wheel'); if (h) { h.wheel(e.deltaY); e.preventDefault(); } }, { passive: false });
cv.addEventListener('contextmenu', (e) => e.preventDefault());

// ------------------------------------------------------------------------------------------------ keyboard
const PADKEYS = '1234qwerasdfzxcv';
window.addEventListener('keydown', (e) => {
  if (e.repeat || e.metaKey || e.ctrlKey || e.altKey) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const vi = 'jkl'.indexOf(k);
  if (vi >= 0) { U.keyValve[vi] = 1; touchCtl(); redraw(); return; }
  const pi = PADKEYS.indexOf(k); if (pi >= 0 && k.length === 1) { U.keyPads |= 1 << pi; touchCtl(); redraw(); return; }
  if (k === ' ' || k === 'Shift') { U.gate = true; sendTrumpet(); redraw(); if (k === ' ') e.preventDefault(); return; }
  if (k === 't') { post({ t: 'tongue' }); return; }
  if (k === 'g') { U.joyClick = 1; touchCtl(); redraw(); return; }
  if (k === 'b') { U.encClick[0] = 1; touchCtl(); redraw(); return; }
  if (k === 'n') { U.encClick[1] = 1; touchCtl(); redraw(); return; }
  if (k === '[') return encStep(0, -1); if (k === ']') return encStep(0, 1); if (k === '-') return encStep(1, -1); if (k === '=') return encStep(1, 1);
  if (k === 'ArrowUp') { U.partial = Math.min(9, U.partial + 1); sendTrumpet(); redraw(); e.preventDefault(); return; }
  if (k === 'ArrowDown') { U.partial = Math.max(2, U.partial - 1); sendTrumpet(); redraw(); e.preventDefault(); return; }
  if (k === 'h' || k === '?' || k === '/') { U.help = !U.help; redraw(); return; }
  if (k === 'Escape') { U.help = false; U.dropdown = -1; redraw(); }
});
window.addEventListener('keyup', (e) => {
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const vi = 'jkl'.indexOf(k); if (vi >= 0) { U.keyValve[vi] = 0; touchCtl(); redraw(); return; }
  const pi = PADKEYS.indexOf(k); if (pi >= 0 && k.length === 1) { U.keyPads &= ~(1 << pi); touchCtl(); redraw(); return; }
  if (k === ' ' || k === 'Shift') { U.gate = false; sendTrumpet(); redraw(); }
  else if (k === 'g') { U.joyClick = 0; touchCtl(); redraw(); } else if (k === 'b') { U.encClick[0] = 0; touchCtl(); redraw(); } else if (k === 'n') { U.encClick[1] = 0; touchCtl(); redraw(); }
});
window.addEventListener('blur', () => { U.keyValve = [0, 0, 0]; U.keyPads = 0; U.gate = false; touchCtl(); sendTrumpet(); });

// ------------------------------------------------------------------------------------------------ device orientation, drop
async function enableMotion() {
  try {
    if (typeof DeviceOrientationEvent !== 'undefined' && typeof DeviceOrientationEvent.requestPermission === 'function') { const r = await DeviceOrientationEvent.requestPermission(); if (r !== 'granted') return; }
    if (!U.motion) window.addEventListener('deviceorientation', (e) => { if (!U.motion) return; U.tilt = [clamp((e.gamma || 0) / 45, -1, 1), clamp(-((e.beta || 0) - 45) / 45, -1, 1)]; touchCtl(); redraw(); });
    U.motion = !U.motion; if (!U.motion) { U.tilt = [0, 0]; touchCtl(); } redraw();
  } catch (e) { S.err = 'motion: ' + (e.message || e); redraw(); }
}
window.addEventListener('dragover', (e) => e.preventDefault());
window.addEventListener('drop', (e) => { e.preventDefault(); const f = e.dataTransfer && e.dataTransfer.files[0]; if (f) loadFile(f); });
window.addEventListener('resize', resize);

// ------------------------------------------------------------------------------------------------ debug hook (verify.mjs + console)
window.__sim = {
  S, U,
  stats: () => new Promise((res) => { statsResolvers.push(res); post({ t: 'stats' }); }),
  state: () => ({ running: U.running, ready: S.ready, err: S.err, ctxState: ctx && ctx.state }),
  start: startStop, setMode, post,
  toPage: (x, y) => { const r = cv.getBoundingClientRect(); return [r.left + ox + x * scale, r.top + oy + y * scale]; },
  setBreath: (v) => { U.breath = v; U.latch = true; sendTrumpet(); },
  setValve: (i, v) => { U.valve[i] = v; touchCtl(); },
  setFader: (i, v) => { U.fader[i] = v; touchCtl(); },
};

resize();
setupAudio();
})();

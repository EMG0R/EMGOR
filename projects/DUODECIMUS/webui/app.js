/* DUODECIMUS bench monitor
   Vanilla JS. Web MIDI in, layout mapping out.
   Firmware contract: twelve knobs straight through, one channel, no pages
   and no banks. Everything is on MIDI channel 7, and every CC number sits in
   undefined MIDI space so nothing a host auto-maps can collide with it.

     CC 20+i   = encoder i rotation   (i = 0..11, so CC 20..31)
     CC 102+i  = encoder i button     (CC 102..113) — MOMENTARY: 127 press,
                 0 release. There is no maintained latch. The pixel on the
                 hardware still toggles per press, but that state is
                 device-local and is not reported over MIDI, so this page
                 cannot and must not claim to know which knobs are "selected".
     CC 116    = held 5.0 s  -> value is the encoder number, 1..12  (capture)
     CC 117    = held 8.0 s  -> value is the encoder number, 1..12  (delete)
     CC 14     = jack A expression      CC 114 = jack A switch
     CC 15     = jack B expression      CC 115 = jack B switch

   Anything on another channel, or on a CC outside those ranges, is logged
   raw and never decoded into a tile.
*/
'use strict';

const NUM_ENC  = 12;
const MIDI_CH  = 7;    // the only channel DUODECIMUS speaks on
const CC_ROT   = 20;   // .. 31
const CC_BTN   = 102;  // .. 113, momentary
/* Press-and-hold gestures. The value is an ENCODER NUMBER (1..12), never a
   level, so it is never rendered as a 0-127 bar anywhere. */
const HOLDS = {
  116: { key: 'capture', label: 'capture', secs: '5.0 s' },
  117: { key: 'delete',  label: 'delete',  secs: '8.0 s' }
};
const HOLD_FLASH_MS = 1400;
const LOG_MAX  = 200;

/* The two TRS jacks are fully independent. Each is set to one type at a time
   on the hardware, so jack A emits CC 14 or CC 114 but never both.
   A jack acting as a footswitch is a "Butt". */
const JACKS = [
  { key: 'A', name: 'Jack A', pin: 'pin 39', exp: 14, sw: 114 },
  { key: 'B', name: 'Jack B', pin: 'pin 40', exp: 15, sw: 115 }
];
// A momentary switch can be down for less than one animation frame, so the
// lamp holds for this long after a press. The reported value is untouched.
const LATCH_MS = 220;

const $ = s => document.querySelector(s);

/* ── persisted settings ───────────────────────────────── */
const LS_PREFIX = 'duodecimus.';
const LS_LEGACY = 'ofoots2.';      // what this tool stored before the rename

/* One-time migration so the rename does not cost anyone their saved grid
   layout and LED map. Copies every legacy key that has no new-name
   counterpart, then marks itself done. The old keys are left alone. */
(function migrateStorage() {
  try {
    if (localStorage.getItem(LS_PREFIX + 'migrated')) return;
    const carry = [];
    for (let i = 0; i < localStorage.length; i++) {
      const k = localStorage.key(i);
      if (k && k.startsWith(LS_LEGACY)) carry.push(k);
    }
    carry.forEach(k => {
      const target = LS_PREFIX + k.slice(LS_LEGACY.length);
      if (localStorage.getItem(target) === null) {
        localStorage.setItem(target, localStorage.getItem(k));
      }
    });
    localStorage.setItem(LS_PREFIX + 'migrated', 'true');
  } catch (e) { /* private mode, blocked storage — nothing to carry over */ }
})();

const LS = {
  get(k, d) { try { const v = localStorage.getItem(LS_PREFIX + k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem(LS_PREFIX + k, JSON.stringify(v)); } catch (e) {} }
};

const state = {
  deviceId : LS.get('deviceId', ''),
  polarity : LS.get('polarity', 'normal'),      // normal | inverted
  modeSel  : LS.get('modeSel', 'auto'),         // auto | relative | accumulator
  detected : 'relative',                        // what auto-detect concluded
  zeroBased: LS.get('zeroBased', true),
  physView : LS.get('physView', false),
  enc      : Array.from({ length: NUM_ENC }, () => ({
               pos: null, last: null, dir: 0, down: false, btnVal: null,
               tRot: -1e9, tBtn: -1e9, btnLatchUntil: -1e9, btnTimer: null,
               turns: 0, sim: false, holdUntil: -1e9, holdKind: null, holdTimer: null
             })),
  deviceName: LS.get('deviceName', ''),
  ped      : {
    A: { role: null, val: null, on: false, presses: 0, t: -1e9, latchUntil: -1e9 },
    B: { role: null, val: null, on: false, presses: 0, t: -1e9, latchUntil: -1e9 }
  },
  lastHold : null,             // { label, enc, at } — the most recent gesture
  layout   : LS.get('layout', { rows: 4, cols: 3, enc: Array(12).fill(null), led: Array(12).fill(null) }),
  assign   : false,
  armed    : null,
  simOn    : false,
  paused   : false,
  logs     : []
};

// One-time repair: earlier builds could latch accumulator on a single stray
// value and persist a forced choice, so a board running relative came up
// reading accumulator on every reload. Drop that stale override once.
if (!LS.get('modeFix1', false)) {
  if (state.modeSel === 'accumulator') state.modeSel = 'auto';
  LS.set('modeSel', state.modeSel);
  LS.set('modeFix1', true);
}

// guard against an older/corrupt stored layout
if (!Array.isArray(state.layout.enc) || state.layout.enc.length !== 12) state.layout.enc = Array(12).fill(null);
if (!Array.isArray(state.layout.led) || state.layout.led.length !== 12) state.layout.led = Array(12).fill(null);

const saveLayout = () => LS.set('layout', state.layout);

/* ── encoder rack ─────────────────────────────────────── */
const rackEl = $('#rack');
const tiles  = [];

for (let i = 0; i < NUM_ENC; i++) {
  const el = document.createElement('div');
  el.className = 'enc';
  el.innerHTML =
    `<div class="enc-top">
       <span class="enc-idx">ENC <b>${i + 1}</b></span>
       <span class="enc-cc">cc${CC_ROT + i} / cc${CC_BTN + i}</span>
     </div>
     <div class="enc-val"><span class="enc-num">···</span><span class="enc-dir"></span></div>
     <div class="enc-sub">no traffic</div>
     <div class="enc-bar"><i></i></div>
     <div class="enc-foot">
       <span class="btn-pill"><i></i><span class="bs">up</span></span>
       <span class="act"><i></i></span>
     </div>
     <span class="hold-badge" hidden></span>`;
  el.addEventListener('click', ev => {
    if (!state.simOn) return;
    if (ev.shiftKey) simButton(i);
    else simRotate(i, Math.random() < .5 ? -1 : 1);
  });
  rackEl.appendChild(el);
  tiles.push({
    root: el,
    num : el.querySelector('.enc-num'),
    dir : el.querySelector('.enc-dir'),
    sub : el.querySelector('.enc-sub'),
    bar : el.querySelector('.enc-bar i'),
    pill: el.querySelector('.btn-pill'),
    bs  : el.querySelector('.bs'),
    act : el.querySelector('.act i'),
    hold: el.querySelector('.hold-badge'),
    tag : null
  });
}

function paintEncoder(i) {
  const e = state.enc[i], t = tiles[i];
  t.num.textContent = e.pos === null ? '···' : e.pos;
  t.dir.textContent = e.dir > 0 ? '↻' : e.dir < 0 ? '↺' : '';
  t.dir.className   = 'enc-dir' + (e.dir > 0 ? ' cw' : e.dir < 0 ? ' ccw' : '');
  t.bar.style.width = e.pos === null ? '0' : (e.pos / 127 * 100).toFixed(1) + '%';
  t.sub.textContent = e.last === null
    ? 'no traffic'
    : `last ${e.last} · ${e.turns} msg${e.turns === 1 ? '' : 's'}`;
  t.bs.textContent = e.down ? 'down' : 'up';
  refreshPill(i);
}

/* The button is momentary, so a tap can begin and end inside one frame. The
   pill lights the instant the press lands and holds BTN_LATCH_MS past the
   release. e.down and the value shown stay the true reported state — no latch
   is reconstructed, because the device's own pixel state is not on the wire. */
const BTN_LATCH_MS = 200;

/* Applied synchronously when a gesture lands, and again from the frame loop so
   it clears on time — the flash must never depend on a frame arriving. */
function refreshHold(i) {
  const e = state.enc[i], t = tiles[i];
  const holding = performance.now() < e.holdUntil;
  t.root.classList.toggle('hold-capture', holding && e.holdKind === 'capture');
  t.root.classList.toggle('hold-delete',  holding && e.holdKind === 'delete');
  t.hold.hidden = !holding;
}
function refreshPill(i) {
  const e = state.enc[i], t = tiles[i];
  const lit = e.down || performance.now() < e.btnLatchUntil;
  t.pill.classList.toggle('on', lit);
  t.pill.classList.toggle('latched', lit && !e.down);
}
for (let i = 0; i < NUM_ENC; i++) paintEncoder(i);

/* ── pedal jacks ──────────────────────────────────────── */
const pedTiles = {};
const pedalsEl = $('#pedals');

JACKS.forEach(j => {
  const el = document.createElement('div');
  el.className = 'pedal';
  el.dataset.role = 'none';
  el.innerHTML =
    `<div class="pedal-top">
       <span class="pedal-name">${j.name}</span>
       <span class="pedal-role">not seen yet</span>
     </div>
     <div class="pedal-cc">cc${j.exp} expression · cc${j.sw} butt · ch ${MIDI_CH} · ${j.pin}</div>
     <div class="pedal-val"><span class="pv">—</span></div>
     <div class="pedal-exp"><div class="meter"><div class="meter-fill"></div></div></div>
     <div class="pedal-sw"><span class="lamp"><i></i><span class="ls">UP</span></span></div>
     <div class="pedal-foot">waiting for this jack</div>`;
  pedalsEl.appendChild(el);
  pedTiles[j.key] = {
    root: el,
    role: el.querySelector('.pedal-role'),
    val : el.querySelector('.pv'),
    fill: el.querySelector('.meter-fill'),
    lamp: el.querySelector('.lamp'),
    ls  : el.querySelector('.ls'),
    foot: el.querySelector('.pedal-foot')
  };
});

/* The lamp is driven from two places on purpose: synchronously the instant a
   press arrives, so the trace never depends on a frame landing inside the
   latch window, and continuously from the frame loop so it clears on time. */
function refreshLamp(key) {
  const p = state.ped[key], t = pedTiles[key];
  const lit = p.on || performance.now() < p.latchUntil;
  t.lamp.classList.toggle('on', lit);
  t.lamp.classList.toggle('latched', lit && !p.on);
}

function paintPedal(key) {
  const j = JACKS.find(x => x.key === key);
  const p = state.ped[key], t = pedTiles[key];
  t.root.dataset.role = p.role || 'none';

  if (p.role === 'exp') {
    t.role.textContent = `Expression · cc${j.exp}`;
    t.val.textContent  = p.val;
    t.fill.style.width = (p.val / 127 * 100).toFixed(1) + '%';
    t.foot.textContent = `${(p.val / 127 * 100).toFixed(0)}% of sweep`;
  } else if (p.role === 'sw') {
    t.role.textContent = `Butt · cc${j.sw}`;
    t.val.textContent  = p.val;                       // true reported value
    t.ls.textContent   = p.on ? 'DOWN' : 'UP';
    t.foot.textContent = `${p.presses} press${p.presses === 1 ? '' : 'es'} · momentary`;
  }
}
JACKS.forEach(j => paintPedal(j.key));

/* ── decay / pulse animation ──────────────────────────── */
const HIT_MS = 260, ACT_MS = 1100;
function frame() {
  const now = performance.now();
  for (let i = 0; i < NUM_ENC; i++) {
    const e = state.enc[i], t = tiles[i];
    const last = Math.max(e.tRot, e.tBtn);
    const age  = now - last;
    t.root.classList.toggle('hit', age < HIT_MS);
    t.root.classList.toggle('simhit', age < HIT_MS && e.sim);
    const k = age < ACT_MS ? 1 - age / ACT_MS : 0;
    t.act.style.transform = `scaleX(${k.toFixed(3)})`;
    refreshPill(i);
    refreshHold(i);
  }
  JACKS.forEach(j => {
    const p = state.ped[j.key], t = pedTiles[j.key];
    t.root.classList.toggle('live', now - p.t < 500);
    // A momentary stomp can start and end inside a single frame. The lamp
    // holds for LATCH_MS past the press so it is always perceivable; p.on
    // itself — and the number shown — stay the true reported state.
    refreshLamp(j.key);
  });
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

/* ── message handling ─────────────────────────────────── */
function onCC(ch1, cc, val, sim) {          // ch1 is 1-based
  const now = performance.now();
  let kind = 'other', meaning = `CC ${cc} — not part of the DUODECIMUS map`;

  // DUODECIMUS speaks only on channel 7. Anything else is somebody else's
  // traffic: show it in the raw log so it is never invisible, decode nothing.
  if (ch1 !== MIDI_CH) {
    pushLog({ ch: ch1, cc, val, kind: 'other',
              meaning: `channel ${ch1} — DUODECIMUS is on channel ${MIDI_CH}`, sim: !!sim });
    return;
  }

  if (cc >= CC_ROT && cc < CC_ROT + NUM_ENC) {
    const i = cc - CC_ROT, e = state.enc[i];
    kind = 'rot';
    e.sim = !!sim; e.tRot = now; e.last = val; e.turns++;

    feedDetector(val);
    if (activeMode() === 'accumulator') {
      e.pos = val;
      e.dir = 0;
    } else {
      const step = val === 127 ? 1 : val === 0 ? -1 : 0;
      e.dir = step;
      e.pos = Math.max(0, Math.min(127, (e.pos === null ? 64 : e.pos) + step));
    }
    paintEncoder(i);
    meaning = `enc ${i + 1} ` + (activeMode() === 'accumulator'
      ? `→ ${val}`
      : (val === 127 ? 'CW' : val === 0 ? 'CCW' : `? ${val}`));
    if (state.assign && state.armed !== null) captureAssign(state.armed, i);

  } else if (cc >= CC_BTN && cc < CC_BTN + NUM_ENC) {
    const i = cc - CC_BTN, e = state.enc[i];
    kind = 'btn';
    e.sim = !!sim; e.tBtn = now; e.btnVal = val;
    const wasDown = e.down;
    e.down = state.polarity === 'inverted' ? val === 0 : val === 127;
    if (e.down && !wasDown) {
      e.btnLatchUntil = now + BTN_LATCH_MS;
      clearTimeout(e.btnTimer);
      e.btnTimer = setTimeout(() => refreshPill(i), BTN_LATCH_MS + 20);
    }
    paintEncoder(i);
    meaning = `enc ${i + 1} button ${e.down ? 'PRESS' : 'RELEASE'}`;

  } else if (HOLDS[cc]) {
    // Value is an encoder number, 1..12 — not a level.
    const h = HOLDS[cc];
    const n = val;
    if (n >= 1 && n <= NUM_ENC) {
      kind = 'hold';
      const e = state.enc[n - 1];
      e.holdUntil = now + HOLD_FLASH_MS;
      e.holdKind = h.key;
      tiles[n - 1].hold.textContent = h.label;
      refreshHold(n - 1);
      clearTimeout(e.holdTimer);
      e.holdTimer = setTimeout(() => refreshHold(n - 1), HOLD_FLASH_MS + 20);
      state.lastHold = { label: h.label, enc: n, secs: h.secs, at: new Date() };
      paintLastHold();
      meaning = `${h.label} · encoder ${n}`;
    } else {
      meaning = `CC ${cc} ${h.label} — value ${val} is not an encoder number (1-12)`;
    }

  } else {
    const jack = JACKS.find(j => j.exp === cc || j.sw === cc);
    if (jack) {
      kind = 'ped';
      const p = state.ped[jack.key];
      p.val = val; p.t = now;
      // The CC that arrived is what tells us which pedal type is plugged in.
      p.role = (cc === jack.exp) ? 'exp' : 'sw';
      if (p.role === 'sw') {
        const wasOn = p.on;
        p.on = val >= 64;
        // Hold the lamp so a stomp shorter than one frame still reads.
        if (p.on && !wasOn) {
          p.presses++;
          p.latchUntil = now + LATCH_MS;
          clearTimeout(p.latchTimer);
          p.latchTimer = setTimeout(() => refreshLamp(jack.key), LATCH_MS + 20);
        }
        refreshLamp(jack.key);
        meaning = `${jack.name} butt ${p.on ? 'DOWN' : 'UP'} (cc${cc})`;
      } else {
        meaning = `${jack.name} expression ${val} (cc${cc})`;
      }
      paintPedal(jack.key);
    }
  }

  // Only real traffic counts as decoded — simulated messages must never make
  // the diagnostic counters look healthy.
  if (!sim && kind !== 'other') { rx.mapped++; updateRxCounter(); }
  pushLog({ ch: ch1, cc, val, kind, meaning, sim: !!sim });
}

function onMidi(ev) {
  const d = ev.data;
  if (!d || d.length === 0) return;

  // Count EVERY inbound byte sequence before any parsing or filtering, so a
  // dead counter always means "not connected" and a climbing counter with dead
  // tiles always means "connected but not decoding".
  rx.raw++;
  rx.lastAt = performance.now();
  const port = ev.target;
  if (port && port.name) rx.lastName = port.name;

  // A port filter must never make traffic vanish without saying so.
  if (state.deviceId && port && port.id && port.id !== state.deviceId) {
    rx.offPort++;
    updateRxCounter();
    return;
  }
  updateRxCounter();

  const status = d[0];
  if (status >= 0xF8) return;                       // clock / active sensing / reset
  if ((status & 0xF0) === 0xB0) {
    onCC((status & 0x0F) + 1, d[1], d[2], false);
  } else {
    pushLog({ ch: (status & 0x0F) + 1, cc: '—', val: d[2] === undefined ? '' : d[2],
              kind: 'other', meaning: `status 0x${status.toString(16).toUpperCase()} — not a CC`, sim: false });
  }
}

/* ── last hold gesture ────────────────────────────────────
   These are rare, deliberate gestures, so the last one stays on screen rather
   than scrolling out of the log. */
function paintLastHold() {
  const el = $('#holdRead');
  const h = state.lastHold;
  if (!h) { el.className = 'hold-read'; el.textContent = 'none yet'; return; }
  el.className = 'hold-read is-' + h.label;
  el.textContent = '';
  const what = document.createElement('b');
  what.textContent = h.label;
  const who = document.createElement('span');
  who.textContent = ` · encoder ${h.enc}`;
  const when = document.createElement('i');
  when.textContent = ' ' + h.at.toTimeString().slice(0, 8);
  el.append(what, who, when);
}

/* ── mode readout ─────────────────────────────────────── */
function activeMode() {
  // A manual choice always wins. Auto only ever falls back to what the
  // detector currently believes, and the detector starts at relative.
  return state.modeSel === 'auto' ? state.detected : state.modeSel;
}

/* Rotation-mode detector.
   Firmware powers up in relative mode, where every rotation CC is exactly 127
   (CW) or exactly 0 (CCW). Accumulator mode is only reachable by SHIFT +
   press encoder 2 mid-session, and then values run the whole 0..127 range.

   So: relative is the resting state, and one stray mid-range byte — a garbled
   message, a half-seated connector — must not flip the panel. Require
   DET_TRIP off-detent values inside a DET_WINDOW-message window to switch,
   and fall back the moment DET_CLEAR consecutive messages are all 0 or 127.
   Nothing here is persisted: a reload always comes up relative. */
const DET_WINDOW = 12;   // how many recent rotation messages we weigh
const DET_TRIP   = 3;    // off-detent values needed to call it accumulator
const DET_CLEAR  = 8;    // consecutive clean detents that drop us back
let rotWindow = [];

function resetDetector() {
  rotWindow = [];
  state.detected = 'relative';
  updateModeChip();
}

function feedDetector(val) {
  const offDetent = (val !== 0 && val !== 127);
  rotWindow.push(offDetent);
  if (rotWindow.length > DET_WINDOW) rotWindow.shift();

  if (state.detected === 'relative') {
    if (rotWindow.filter(Boolean).length >= DET_TRIP) state.detected = 'accumulator';
  } else {
    const recent = rotWindow.slice(-DET_CLEAR);
    if (recent.length >= DET_CLEAR && !recent.some(Boolean)) {
      state.detected = 'relative';
      // Falling back is a fresh verdict: drop the old off-detent evidence,
      // or it is still inside the window and re-trips on the very next message.
      rotWindow = [];
    }
  }
  updateModeChip();
}

function updateModeChip() {
  const chip = $('#modeChip');
  const m = activeMode();
  const off = rotWindow.filter(Boolean).length;
  if (state.modeSel === 'auto') {
    chip.textContent = m + ' · detected';
    chip.title = `Auto-detect: ${off} of the last ${rotWindow.length || 0} rotation messages were off-detent. `
      + `${DET_TRIP} in ${DET_WINDOW} switches to accumulator; ${DET_CLEAR} clean detents switch back. `
      + `Pick Relative or Accumulator to override.`;
  } else {
    chip.textContent = m + ' · forced';
    chip.title = 'You set this by hand — auto-detect is ignored until you switch back to Auto-detect.';
  }
  chip.classList.toggle('chip-forced', state.modeSel !== 'auto');
  chip.classList.toggle('chip-accum', m === 'accumulator');
}

/* ── raw MIDI log ─────────────────────────────────────── */
const logEl = $('#log');
function pushLog(rec) {
  rec.time = new Date();
  state.logs.push(rec);
  if (state.logs.length > LOG_MAX) state.logs.shift();
  $('#logCount').textContent = state.logs.length;
  if (state.paused) return;
  const empty = logEl.querySelector('.log-empty');
  if (empty) empty.remove();
  renderLogRow(rec, true);
  while (logEl.children.length > LOG_MAX) logEl.removeChild(logEl.lastChild);
}
function rowText(r) {
  return `ch${r.ch} cc${r.cc} ${r.val} ${r.kind} ${r.meaning}`.toLowerCase();
}
function renderLogRow(r, prepend) {
  const d = r.time;
  const stamp = `${String(d.getMinutes()).padStart(2, '0')}:${String(d.getSeconds()).padStart(2, '0')}.${String(d.getMilliseconds()).padStart(3, '0')}`;
  const row = document.createElement('div');
  row.className = 'log-row k-' + r.kind + (r.sim ? ' sim' : '');
  row.innerHTML = `<span class="t">${stamp}</span><span>${r.ch}</span><span>${r.cc}</span><span class="v">${r.val}</span><span class="m"></span>`;
  row.lastChild.textContent = r.meaning;
  row.dataset.text = rowText(r);
  applyFilterTo(row);
  if (prepend && logEl.firstChild) logEl.insertBefore(row, logEl.firstChild);
  else if (prepend) logEl.appendChild(row);
  else logEl.appendChild(row);
}
function applyFilterTo(row) {
  const f = $('#logFilter').value.trim().toLowerCase();
  row.hidden = f !== '' && !row.dataset.text.includes(f);
}
function rebuildLog() {
  logEl.textContent = '';
  const list = state.logs.slice().reverse();
  if (!list.length) {
    const p = document.createElement('div');
    p.className = 'log-empty';
    p.textContent = 'Nothing received yet. Select a MIDI input above, or hit Simulate to generate traffic.';
    logEl.appendChild(p);
    return;
  }
  list.forEach(r => renderLogRow(r, false));
}
rebuildLog();

$('#logFilter').addEventListener('input', () => {
  [...logEl.querySelectorAll('.log-row')].forEach(applyFilterTo);
});
$('#logPause').addEventListener('click', () => {
  state.paused = !state.paused;
  $('#logPause').classList.toggle('on', state.paused);
  $('#logPause').textContent = state.paused ? 'Resume' : 'Pause';
  if (!state.paused) rebuildLog();
});
$('#logClear').addEventListener('click', () => {
  state.logs = []; $('#logCount').textContent = '0'; rebuildLog();
});

/* ── layout grid ──────────────────────────────────────── */
const gridEl = $('#grid');

function renderGrid() {
  const { rows, cols } = state.layout;
  gridEl.style.setProperty('--cols', cols);
  gridEl.textContent = '';
  for (let p = 0; p < rows * cols; p++) {
    const r = Math.floor(p / cols) + 1, c = (p % cols) + 1;
    const encIdx = state.layout.enc[p];
    const cell = document.createElement('div');
    cell.className = 'cell' + (state.assign ? ' clickable' : '') + (state.armed === p ? ' armed' : '');
    cell.tabIndex = state.assign ? 0 : -1;
    cell.innerHTML =
      `<div class="cell-pos">R${r} C${c}</div>
       <div class="cell-enc ${encIdx === null ? 'empty' : ''}">${encIdx === null ? '––' : 'ENC ' + (encIdx + 1)}</div>
       <div class="cell-row"><label>LED</label><input type="number" min="1" max="12" step="1" placeholder="–"></div>
       ${encIdx === null ? '' : '<button class="cell-x" title="Clear this cell">×</button>'}`;
    const input = cell.querySelector('input');
    input.value = state.layout.led[p] === null ? '' : state.layout.led[p];
    input.addEventListener('input', () => {
      const v = parseInt(input.value, 10);
      state.layout.led[p] = (Number.isFinite(v) && v >= 1 && v <= 12) ? v : null;
      saveLayout(); renderExport();
    });
    input.addEventListener('click', ev => ev.stopPropagation());
    const x = cell.querySelector('.cell-x');
    if (x) x.addEventListener('click', ev => {
      ev.stopPropagation();
      state.layout.enc[p] = null; saveLayout(); renderGrid(); renderExport(); applyPhysView();
    });
    cell.addEventListener('click', () => { if (state.assign) armCell(p); });
    cell.addEventListener('keydown', ev => {
      if (state.assign && (ev.key === 'Enter' || ev.key === ' ')) { ev.preventDefault(); armCell(p); }
    });
    gridEl.appendChild(cell);
  }
}

function armCell(p) {
  state.armed = state.armed === p ? null : p;
  renderGrid();
  if (state.armed !== null) {
    const el = gridEl.children[state.armed];
    if (el) el.focus({ preventScroll: true });
  }
  updateAssignHelp();
}

function captureAssign(pos, encIdx) {
  const prev = state.layout.enc.indexOf(encIdx);
  if (prev !== -1 && prev !== pos) state.layout.enc[prev] = null;
  state.layout.enc[pos] = encIdx;
  saveLayout();

  // next empty cell, wrapping, so the user can keep both hands on the board
  let next = null;
  if ($('#autoAdvance').checked) {
    const n = state.layout.rows * state.layout.cols;
    for (let k = 1; k <= n; k++) {
      const cand = (pos + k) % n;
      if (state.layout.enc[cand] === null) { next = cand; break; }
    }
  }
  state.armed = next;
  renderGrid();
  const cell = gridEl.children[pos];
  if (cell) { cell.classList.add('captured'); setTimeout(() => cell.classList.remove('captured'), 520); }
  renderExport();
  applyPhysView();
  updateAssignHelp();
}

function updateAssignHelp() {
  const h = $('#assignHelp');
  h.classList.toggle('armed', state.assign && state.armed !== null);
  if (!state.assign) {
    h.textContent = 'Encoder numbering is aligned in firmware, so encoder 1 really is the top-left knob. Use this to arrange the tiles to match your panel: turn on assign mode, click a cell, then turn the encoder you want there.';
  } else if (state.armed === null) {
    const left = state.layout.enc.filter(v => v === null).length;
    h.textContent = `Assign mode is live — click a cell to arm it. ${left} of ${state.layout.rows * state.layout.cols} cells still empty.`;
  } else {
    const cols = state.layout.cols;
    h.textContent = `Armed: row ${Math.floor(state.armed / cols) + 1}, column ${(state.armed % cols) + 1}. Now turn the encoder that belongs there.`;
  }
  if (state.assign && state.simOn) {
    h.textContent += ' (Random simulation pauses during assign — click a rack tile to fire that encoder.)';
  }
}

$('#assignToggle').addEventListener('click', () => {
  state.assign = !state.assign;
  state.armed = null;
  $('#assignToggle').classList.toggle('on', state.assign);
  $('#assignToggle').textContent = state.assign ? 'Assign: LIVE' : 'Assign mode';
  renderGrid(); updateAssignHelp();
});
$('#clearAll').addEventListener('click', () => {
  state.layout.enc = Array(12).fill(null);
  state.layout.led = Array(12).fill(null);
  state.armed = null;
  saveLayout(); renderGrid(); renderExport(); applyPhysView(); updateAssignHelp();
});
$('#gridSel').addEventListener('change', () => {
  const [r, c] = $('#gridSel').value.split('x').map(Number);
  state.layout.rows = r; state.layout.cols = c;
  state.armed = null;
  saveLayout(); renderGrid(); renderExport(); applyPhysView(); updateAssignHelp();
});

/* ── rack ordering by physical layout ─────────────────── */
function applyPhysView() {
  const on = $('#physView').checked;
  rackEl.classList.toggle('phys', on);
  rackEl.style.setProperty('--cols', state.layout.cols);
  $('#rackHint').textContent = on
    ? `tiles ordered by grid position (${state.layout.rows}×${state.layout.cols})`
    : 'tiles ordered by firmware index';
  for (let i = 0; i < NUM_ENC; i++) {
    const pos = state.layout.enc.indexOf(i);
    tiles[i].root.style.order = on ? (pos === -1 ? 100 + i : pos) : 0;
    let tag = tiles[i].root.querySelector('.enc-tag');
    if (on && pos !== -1) {
      if (!tag) { tag = document.createElement('span'); tag.className = 'enc-tag'; tiles[i].root.appendChild(tag); }
      tag.textContent = `R${Math.floor(pos / state.layout.cols) + 1}C${pos % state.layout.cols + 1}`;
    } else if (tag) tag.remove();
  }
}
$('#physView').addEventListener('change', () => { LS.set('physView', $('#physView').checked); applyPhysView(); });

/* ── export ───────────────────────────────────────────── */
function exportArrays() {
  const zb = state.zeroBased;
  const enc = state.layout.enc.map(v => v === null ? null : (zb ? v : v + 1));
  const led = state.layout.led.map(v => v === null ? null : (zb ? v - 1 : v));
  return { enc, led };
}
function cSource() {
  const { rows, cols } = state.layout;
  const { enc, led } = exportArrays();
  const zb = state.zeroBased;
  const n = rows * cols;
  const fmt = arr => {
    let out = '';
    for (let r = 0; r < rows; r++) {
      const slice = arr.slice(r * cols, r * cols + cols)
        .map(v => String(v === null ? 255 : v).padStart(3, ' ')).join(',');
      out += `  /* row ${r + 1} */ ${slice}${r === rows - 1 ? '' : ','}\n`;
    }
    return out;
  };
  const base = zb ? '0-based, as the sketch indexes them' : '1-based, as labelled on the panel';
  return `// DUODECIMUS physical layout
// ${rows} rows x ${cols} cols, row-major from the top-left.
// Values are ${base}.
// 255 marks a position that was never assigned.

// grid position -> firmware encoder index
static const uint8_t ENC_LAYOUT[${n}] = {
${fmt(enc)}};

// grid position -> NeoPixel index
static const uint8_t LED_LAYOUT[${n}] = {
${fmt(led)}};`;
}
function renderExport() {
  $('#exportOut').textContent = cSource();
  const missEnc = state.layout.enc.filter(v => v === null).length;
  const missLed = state.layout.led.filter(v => v === null).length;
  const msg = $('#exportMsg');
  if (missEnc || missLed) {
    msg.className = 'export-msg warn';
    msg.textContent = `${missEnc} encoder + ${missLed} LED positions unassigned — exported as 255`;
  } else {
    msg.className = 'export-msg';
    msg.textContent = 'All 12 positions mapped';
  }
}
$('#zeroBased').addEventListener('change', () => {
  state.zeroBased = $('#zeroBased').checked; LS.set('zeroBased', state.zeroBased); renderExport();
});
$('#copyC').addEventListener('click', async () => {
  const txt = cSource();
  try {
    await navigator.clipboard.writeText(txt);
    flash('Copied to clipboard');
  } catch (e) {
    const ta = document.createElement('textarea');
    ta.value = txt; document.body.appendChild(ta); ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    flash(ok ? 'Copied to clipboard' : 'Copy blocked — select the text above', !ok);
  }
});
function flash(text, warn) {
  const msg = $('#exportMsg');
  msg.className = 'export-msg' + (warn ? ' warn' : '');
  msg.textContent = text;
  setTimeout(renderExport, 2200);
}
$('#saveJson').addEventListener('click', () => {
  const blob = new Blob([JSON.stringify({
    app: 'DUODECIMUS layout', version: 1,
    rows: state.layout.rows, cols: state.layout.cols,
    note: 'encLayout / ledLayout are 1-based, null = unassigned',
    encLayout: state.layout.enc.map(v => v === null ? null : v + 1),
    ledLayout: state.layout.led.slice()
  }, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `duodecimus-layout-${state.layout.rows}x${state.layout.cols}.json`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  flash('Layout saved');
});
$('#loadJson').addEventListener('click', () => $('#jsonFile').click());
$('#jsonFile').addEventListener('change', ev => {
  const f = ev.target.files[0];
  if (!f) return;
  const rd = new FileReader();
  rd.onload = () => {
    try {
      const j = JSON.parse(rd.result);
      const n = (j.rows || 4) * (j.cols || 3);
      if (n !== 12) throw new Error('grid must hold 12 positions');
      state.layout.rows = j.rows; state.layout.cols = j.cols;
      state.layout.enc = (j.encLayout || []).map(v => (typeof v === 'number' && v >= 1 && v <= 12) ? v - 1 : null);
      state.layout.led = (j.ledLayout || []).map(v => (typeof v === 'number' && v >= 1 && v <= 12) ? v : null);
      while (state.layout.enc.length < 12) state.layout.enc.push(null);
      while (state.layout.led.length < 12) state.layout.led.push(null);
      $('#gridSel').value = `${j.rows}x${j.cols}`;
      saveLayout(); renderGrid(); renderExport(); applyPhysView(); updateAssignHelp();
      flash('Layout loaded from ' + f.name);
    } catch (e) {
      flash('Could not read that file: ' + e.message, true);
    }
    ev.target.value = '';
  };
  rd.readAsText(f);
});

/* ── simulate ─────────────────────────────────────────── */
let simTimer = null;
const simPos = Array(NUM_ENC).fill(64);
let simExp = 40, simExpDir = 3;
function simRotate(i, step) {
  if (activeMode() === 'accumulator') {
    simPos[i] = Math.max(0, Math.min(127, simPos[i] + step * 3));
    onCC(MIDI_CH, CC_ROT + i, simPos[i], true);
  } else {
    onCC(MIDI_CH, CC_ROT + i, step > 0 ? 127 : 0, true);
  }
}
function simButton(i) {
  // Momentary: press then release, the way the firmware now reports it.
  const press   = state.polarity === 'inverted' ? 0 : 127;
  const release = state.polarity === 'inverted' ? 127 : 0;
  onCC(MIDI_CH, CC_BTN + i, press, true);
  setTimeout(() => onCC(MIDI_CH, CC_BTN + i, release, true), 70);
}

function simHold(cc) {
  onCC(MIDI_CH, cc, 1 + Math.floor(Math.random() * NUM_ENC), true);
}
function simTick() {
  // Random traffic would hijack an armed cell, so it holds off while you map.
  // Clicking a tile still fires that exact encoder.
  if (state.assign) return;
  const roll = Math.random();
  if (roll < .60) {
    simRotate(Math.floor(Math.random() * NUM_ENC), Math.random() < .5 ? -1 : 1);
  } else if (roll < .72) {
    simButton(Math.floor(Math.random() * NUM_ENC));
  } else if (roll < .74) {
    simHold(Math.random() < .6 ? 116 : 117);
  } else if (roll < .93) {
    simExp += simExpDir;
    if (simExp >= 127) { simExp = 127; simExpDir = -3; }
    if (simExp <= 0)   { simExp = 0;   simExpDir = 3; }
    // Mirrors the user's rig: jack A is an expression treadle, jack B a
    // momentary butt. A brief stomp is 127 then 0 back to back.
    onCC(MIDI_CH, JACKS[0].exp, simExp, true);
  } else if (roll < .98) {
    onCC(MIDI_CH, JACKS[1].sw, 127, true);
    setTimeout(() => onCC(MIDI_CH, JACKS[1].sw, 0, true), 40);
  }
}
function setSim(on) {
  state.simOn = on;
  $('#simToggle').classList.toggle('on', on);
  $('#simToggle').textContent = on ? 'Simulating' : 'Simulate';
  tiles.forEach(t => t.root.classList.toggle('simclick', on));
  clearInterval(simTimer);
  simTimer = on ? setInterval(simTick, 340) : null;
  // Simulated traffic must never leave a verdict behind for real traffic to
  // inherit — starting or stopping the simulator wipes the detector.
  resetDetector();
  updateLink();
  updateAssignHelp();
}
$('#simToggle').addEventListener('click', () => setSim(!state.simOn));

/* ── Web MIDI ─────────────────────────────────────────────
   Everything here is built around one rule: never silently drop traffic.

   The previous version bound onmidimessage to a single port picked by a name
   regex, and unbound it whenever the stored port id stopped matching. Since
   Chrome hands out a fresh port id every time the Teensy re-enumerates — which
   is every single reflash — the page came up permanently deaf with nothing on
   screen to say so. Now every input is bound, always, and the dropdown is only
   a filter over already-flowing traffic. */

let access = null;
let midiState = navigator.requestMIDIAccess ? 'pending' : 'unsupported';
let listening = [];            // inputs we currently hold open

const rx = { raw: 0, mapped: 0, offPort: 0, lastAt: -1e9, lastName: '' };

function updateRxCounter() {
  $('#rxCount').textContent = rx.raw;
  $('#rxMapped').textContent = rx.mapped;
  const note = $('#rxNote');
  if (rx.raw === 0) {
    note.textContent = 'no messages yet';
    note.className = 'rx-note';
  } else if (rx.mapped === 0) {
    note.textContent = 'arriving but not decoding — check the CC map';
    note.className = 'rx-note warn';
  } else if (rx.offPort > 0 && state.deviceId) {
    note.textContent = `${rx.offPort} on other ports — try All inputs`;
    note.className = 'rx-note warn';
  } else {
    note.textContent = rx.lastName ? 'from ' + rx.lastName : 'receiving';
    note.className = 'rx-note ok';
  }
}

function updateLink() {
  const chip = $('#linkChip'), txt = $('#linkText');
  const set = (cls, t) => { chip.className = 'chip ' + cls; txt.textContent = t; };
  if (midiState === 'unsupported') return set('chip-err', 'no web midi');
  if (midiState === 'refused')     return set('chip-err', 'midi refused');
  if (midiState === 'pending')     return set('chip-off', 'awaiting midi permission');
  if (listening.length === 0)      return set('chip-err', 'no midi inputs found');
  const live = rx.raw > 0;
  set(live ? 'chip-live' : 'chip-off',
      `listening to ${listening.length} input${listening.length === 1 ? '' : 's'}`);
  if (state.simOn) set('chip-sim', `simulating + ${listening.length} input${listening.length === 1 ? '' : 's'}`);
}

/* Bind EVERY input, every time. Setting onmidimessage implicitly opens the
   port, and re-setting it on an already-bound input is harmless, so this is
   safe to call on any statechange. */
function bindAllInputs() {
  listening = [];
  if (!access) return;
  for (const inp of access.inputs.values()) {
    inp.onmidimessage = onMidi;
    if (inp.open) { try { inp.open(); } catch (e) {} }
    listening.push(inp);
  }
}

/* The USB descriptor now reports product "DUODECIMUS", manufacturer "EMGOR".
   teensy and foots stay in the alternation so a board still running older
   firmware is recognised too. */
const DEVICE_RE = /duodecimus|teensy|foots/i;
const looksLikeDevice = inp => DEVICE_RE.test(inp.name || '') || DEVICE_RE.test(inp.manufacturer || '');

function inputLabel(inp) {
  return inp.name + (inp.manufacturer ? ` · ${inp.manufacturer}` : '');
}

function refreshPorts() {
  const sel = $('#midiIn');
  bindAllInputs();

  sel.textContent = '';
  const all = document.createElement('option');
  all.value = '';
  all.textContent = listening.length ? `All inputs (${listening.length})` : 'All inputs';
  sel.appendChild(all);
  sel.disabled = !access;

  let match = '';
  for (const inp of listening) {
    const o = document.createElement('option');
    o.value = inp.id;
    o.textContent = inputLabel(inp);
    sel.appendChild(o);
    // Port ids churn on every reflash, so fall back to matching by name.
    if (inp.id === state.deviceId) match = inp.id;
    else if (!match && inp.name && inp.name === state.deviceName) match = inp.id;
  }
  // Last resort when a pinned port is gone and its exact name is gone too —
  // which is what a firmware rename looks like ("Teensy MIDI" -> DUODECIMUS).
  // Re-pin to whatever now identifies as the board rather than losing the pin.
  if (state.deviceId && !match) {
    const guess = listening.find(looksLikeDevice);
    if (guess) {
      match = guess.id;
      state.deviceName = guess.name;
      LS.set('deviceName', state.deviceName);
    }
  }

  // A filter that no longer resolves must fall back to All inputs, never to
  // silence — that fallback is the whole reason the page went dead before.
  if (state.deviceId && !match) {
    state.deviceId = '';
    LS.set('deviceId', '');
  } else {
    state.deviceId = match;
  }
  sel.value = state.deviceId;
  updateLink();
  updateRxCounter();
  announcePorts();
}

let lastPortSig = null;
function announcePorts() {
  const sig = listening.map(i => i.id + ':' + i.name).join('|');
  if (sig === lastPortSig) return;
  lastPortSig = sig;
  // The off-port warning describes the current set of ports, so a changed
  // port list retires it rather than leaving a stale complaint on screen.
  rx.offPort = 0;
  updateRxCounter();
  const b = $('#banner');
  if (midiState !== 'ready') return;
  if (listening.length === 0) {
    b.hidden = false;
    b.className = 'banner banner-warn';
    b.innerHTML = '<b>No MIDI inputs found.</b> Plug the board in — the page picks it up on its own, no reload needed.';
  } else if (listening.length === 1) {
    b.hidden = false;
    b.className = 'banner banner-ok';
    b.innerHTML = `<b>Listening to ${listening[0].name}.</b> It was the only input, so it was selected automatically. Stomp something — the RX counter in the status rail climbs on every message received.`;
  } else {
    const board = listening.find(looksLikeDevice);
    b.hidden = false;
    b.className = 'banner banner-ok';
    b.innerHTML = `<b>Listening to all ${listening.length} MIDI inputs.</b> `
      + (board ? `${board.name} is among them. ` : '')
      + 'Narrow it with the dropdown only if something else is chattering.';
  }
}

$('#midiIn').addEventListener('change', () => {
  state.deviceId = $('#midiIn').value;
  const hit = listening.find(i => i.id === state.deviceId);
  state.deviceName = hit ? hit.name : '';
  LS.set('deviceId', state.deviceId);
  LS.set('deviceName', state.deviceName);
  rx.offPort = 0;
  resetDetector();
  updateLink();
  updateRxCounter();
});
$('#rescan').addEventListener('click', () => { lastPortSig = null; refreshPorts(); });
$('#rxReset').addEventListener('click', () => {
  rx.raw = 0; rx.mapped = 0; rx.offPort = 0; rx.lastName = '';
  updateRxCounter(); updateLink();
});

function noMidiBanner(msg) {
  const b = $('#banner');
  b.hidden = false;
  b.className = 'banner';
  b.innerHTML = msg;
  $('#midiIn').disabled = true;
  updateLink();
}

if (midiState === 'unsupported') {
  noMidiBanner('<b>This browser has no Web MIDI API.</b> Safari and stock Firefox cannot talk to the pedalboard. Open this page in <b>Chrome</b> or <b>Edge</b> to monitor real hardware — everything else here, including Simulate, works as-is.');
} else {
  const slow = setTimeout(() => {
    if (midiState === 'pending') $('#linkText').textContent = 'allow the midi prompt';
  }, 4000);
  navigator.requestMIDIAccess({ sysex: false }).then(a => {
    clearTimeout(slow);
    midiState = 'ready';
    access = a;
    // Fires when the board disappears on upload and again when it comes back.
    a.onstatechange = () => refreshPorts();
    refreshPorts();
  }).catch(err => {
    clearTimeout(slow);
    midiState = 'refused';
    noMidiBanner('<b>Web MIDI was refused:</b> ' + err.message + '. Chrome asks for MIDI permission on a secure origin — reload over <b>http://localhost</b> and allow the prompt. Simulate mode still works.');
  });
}

/* ── settings wiring + boot ───────────────────────────── */
$('#polSel').value = state.polarity;
$('#polSel').addEventListener('change', () => {
  state.polarity = $('#polSel').value;
  LS.set('polarity', state.polarity);
});
$('#modeSel').value = state.modeSel;
$('#modeSel').addEventListener('change', () => {
  state.modeSel = $('#modeSel').value;
  LS.set('modeSel', state.modeSel);
  // Clearing the override hands control back to a detector with no history,
  // so it starts from relative rather than resuming an old verdict.
  if (state.modeSel === 'auto') resetDetector();
  else updateModeChip();
});
$('#zeroBased').checked = state.zeroBased;
$('#physView').checked  = state.physView;
$('#gridSel').value     = `${state.layout.rows}x${state.layout.cols}`;
if (!$('#gridSel').value || $('#gridSel').selectedIndex < 0) $('#gridSel').value = '4x3';

updateModeChip();
renderGrid();
renderExport();
applyPhysView();
updateAssignHelp();
updateRxCounter();
paintLastHold();
updateLink();

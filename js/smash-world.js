// GOR BRAWL world fights: a fight session you can drop over the 3D page (proximity fights).
// createFightSession(opts) -> { start, stop, update(dt), setInput(mask), el, onResult(fn), state }
// Net model (relay sm.*): host-authoritative. Host sims 60 Hz with guest input masks (sm.in), sends sm.snap at 30 Hz.
// Guest = full local sim restored from each snapshot, then re-simulated with its own unacked inputs (prediction);
// remote fighters are drawn from snapshot interpolation. Same seed + same inputs => same engine.hash().
import { createMatch, botInput, drawScene, newView, updateView, viewEvent, createRenderer, PLAYER_COLORS } from './smash-engine.js';
import { FIGHTER_IDS } from './smash-fighters.js';
import { buildAtlas } from './smash-sprites.js';
import { terrainStage, createTerrainVisual } from './smash-stages.js';
import { createUI } from './smash-ui.js';

// style id (3D world) -> smash fighter. Unknown ids fall to a stable pick by string hash.
const STYLE_MAP = { MARO: 'pilot', JOSHI: 'hopper', STEEV: 'builder', SONIK: 'swift', PILOT: 'pilot', HOPPER: 'hopper', BUILDER: 'builder', SWIFT: 'swift',
  TWIN: 'swift', RACER: 'swift', SPARKLET: 'swift', JELLY: 'hopper', HUM: 'hopper', HAULER: 'builder', RANGER: 'pilot', BOUNTY: 'pilot', SEER: 'pilot' };
export function styleToFighter(id) {
  const k = String(id == null ? '' : id).toUpperCase(); if (STYLE_MAP[k]) return STYLE_MAP[k];
  let h = 0; for (let i = 0; i < k.length; i++) h = (h * 31 + k.charCodeAt(i)) | 0; return FIGHTER_IDS[Math.abs(h) % FIGHTER_IDS.length];
}

let ATLAS = null; const atlas = () => ATLAS || (ATLAS = buildAtlas(FIGHTER_IDS));
const SKIP = new Set(['def', 'bot', 'move', 'hbMove', 'pend', 'cd', 'i', 'fid', 'ctrl', 'level']);
const CSS = '.smw{position:fixed;inset:0;z-index:60;pointer-events:none}.smw .sm-root{background:transparent}.smw .sm-wrap{background:transparent}.smw .sm-rbtn{display:none}';
function parseCol(c) {
  if (typeof c === 'number') c = '#' + (c & 0xffffff).toString(16).padStart(6, '0');
  const m = /^#?([0-9a-f]{6})$/i.exec(String(c || '').trim()); const v = m ? parseInt(m[1], 16) : 0xffffff;
  return { hex: '#' + (m ? m[1] : 'ffffff'), rgb: [((v >> 16) & 255) / 255, ((v >> 8) & 255) / 255, (v & 255) / 255] };
}
const mkSnapNum = (v) => (typeof v === 'number' && !Number.isInteger(v) ? Math.round(v * 1e4) / 1e4 : v);

export function createFightSession(opts) {
  opts = opts || {};
  const mode = opts.mode || 'local', fighters = opts.fighters || [], net = opts.net || null, room = opts.roomId;
  const seed = (opts.seed | 0) || 1234, N = fighters.length;
  const samples = opts.stageSamples || new Float32Array(128);
  const stageDef = terrainStage(samples, 'terrain_' + (seed >>> 0).toString(16));
  const cols = fighters.map((f, i) => parseCol((opts.playerColors && opts.playerColors[i]) || f.color || PLAYER_COLORS[i % 4]));
  const cfg = {
    players: fighters.map((f) => ({ fid: styleToFighter(f.styleId), ctrl: f.isBot ? 'cpu' : 'net', level: f.level || 2 })),
    stageDef, seed, stocks: opts.stocks || 2, time: (opts.time || 180) * 60,
  };
  const ownIdx = mode === 'guest' ? fighters.findIndex((f) => f.isLocal) : -1;
  const localIdx = []; fighters.forEach((f, i) => { if (f.isLocal && !f.isBot) localIdx.push(i); });
  const localMask = [0, 0, 0, 0]; let remoteMask = fighters.map(() => 0), lastMasks = fighters.map(() => 0), ack = fighters.map(() => 0);
  const idToIdx = {}; fighters.forEach((f, i) => { if (f.id != null) idToIdx[f.id] = i; });

  // ---- DOM / renderer
  const el = document.createElement('div'); el.className = 'smw'; el.style.display = 'none';
  if (!document.getElementById('smw-css')) { const s = document.createElement('style'); s.id = 'smw-css'; s.textContent = CSS; document.head.appendChild(s); }
  const ui = createUI(el); const at = atlas();
  const R = createRenderer(ui.canvas, at, /[?&]canvas2d/.test(location.search), true);
  const vis = createTerrainVisual(stageDef, opts.skyPalette, seed, opts.skyAlpha);
  let view = newView(); view.pcol = cols.map((c) => c.rgb); view.tcol = cols.map((c) => c.rgb.map((v) => 0.72 + 0.28 * v));
  const savedColors = PLAYER_COLORS.slice();

  // ---- sim
  let S = createMatch(cfg), V = S, running = false, clock = 0, acc = 0, done = false, resultCbs = [], ksSig = '', ks = null, snapN = 0;
  const known = S.fighters.map((f) => f.stocks);
  const state = { mode, phase: 'idle', frame: 0, kos: 0, koList: [], result: null, hashOk: 0, hashBad: 0, snaps: 0, view: [], pct: [], stocks: [], overlay: false };
  const gameLike = { get sim() { return V; }, atlas: at, cfg, stop() {}, pause() {} };
  const hist = []; let gf = 0; // guest input history (index = guest tick)
  const snaps = []; let lastSnapFrame = -1;
  const disp = {}; // own fighter smoothed display offset
  const hostId = (m) => (m.id != null ? idToIdx[m.id] : undefined);

  function noteStocks(sim, synth) {
    sim.fighters.forEach((f, i) => {
      if (f.stocks < known[i]) {
        known[i] = f.stocks; state.kos++; state.koList.push({ slot: fighters[i].slot, frame: sim.frame });
        if (synth) { const e = { t: 'ko', who: i, x: f.x, y: f.y }; viewEvent(view, e); ui.onEvent(e, gameLike); }
      }
    });
  }
  function drainEvents(sim, skipKo) {
    for (const e of sim.events) {
      if (e.t === 'sfx') { if (opts.onSfx) opts.onSfx(e.n); continue; }
      if (e.t === 'ko' && skipKo) continue;
      if (e.t === 'over') { if (mode !== 'guest') finishLocal(); }
      viewEvent(view, e); ui.onEvent(e, gameLike);
    }
  }
  function resultOf(sim) {
    const idx = sim.result ? sim.result.winner : -1, rows = sim.fighters;
    return { winner: idx >= 0 ? fighters[idx].slot : -1, slots: fighters.map((f) => f.slot), stocks: rows.map((f) => f.stocks), damage: rows.map((f) => Math.floor(f.pct)),
      dealt: rows.map((f) => Math.floor(f.dealt)), kos: rows.map((f) => f.kos), frames: sim.frame };
  }
  function fire(res) {
    if (done) return; done = true; state.result = res; state.phase = 'over';
    for (const fn of resultCbs) { try { fn(res); } catch (e) { console.error(e); } }
  }
  function finishLocal() {
    if (done) return; const res = resultOf(S);
    if (mode === 'host' && net) net.send({ t: 'sm.end', room, result: res });
    fire(res);
  }

  // ---- snapshot (host -> guests/spectators)
  function moveKey(f, m) { if (!m) return 0; for (const k in f.def.moves) if (f.def.moves[k] === m) return k; return 0; }
  function sendSnap() {
    const keys = Object.keys(S.fighters[0]).filter((k) => !SKIP.has(k)), sig = keys.join(',');
    const fs = S.fighters.map((f) => { const o = { v: keys.map((k) => f[k]), mv: moveKey(f, f.move), hm: moveKey(f, f.hbMove) }; if (f.pend) o.pd = f.pend; for (const k in f.cd) { o.cd = f.cd; break; } return o; });
    const data = { fr: S.frame, ph: S.phase, tm: S.time, ct: S.count, et: S.endT, nid: S.nid, h: S.hash(), m: lastMasks, ak: ack, fs, pj: S.projs, bl: S.blocks, lo: S.ledgeOcc };
    if (sig !== ksSig || (snapN % 60) === 0) { ksSig = sig; data.ks = keys; }
    let msg = { t: 'sm.snap', room, f: S.frame, data }, len = JSON.stringify(msg).length;
    if (len > 1900) { // tighten floats before giving up the frame
      data.fs.forEach((o) => { o.v = o.v.map(mkSnapNum); }); data.pj = data.pj.map((p) => { const q = {}; for (const k in p) q[k] = mkSnapNum(p[k]); return q; });
    }
    snapN++; net.send(msg);
  }
  function restore(d) {
    if (d.ks) ks = d.ks; if (!ks) return false;
    S.frame = d.fr; S.phase = d.ph; S.time = d.tm; S.count = d.ct; S.endT = d.et; S.nid = d.nid;
    d.fs.forEach((o, i) => {
      const f = S.fighters[i]; if (!f) return; ks.forEach((k, j) => { f[k] = o.v[j]; });
      f.move = o.mv ? f.def.moves[o.mv] : null; f.hbMove = o.hm ? f.def.moves[o.hm] : null; f.pend = o.pd || null; f.cd = o.cd ? Object.assign({}, o.cd) : {};
    });
    S.projs = d.pj.map((p) => Object.assign({}, p)); S.blocks = d.bl.map((b) => Object.assign({}, b)); S.ledgeOcc = d.lo.slice();
    S.solids.length = S.stage.solids.length; for (const b of S.blocks) S.solids.push(b);
    return true;
  }
  function onSnap(msg) {
    const d = msg.data; if (!d || d.fr <= lastSnapFrame) return; lastSnapFrame = d.fr;
    if (!restore(d)) return;
    state.snaps++; if (S.hash() === d.h) state.hashOk++; else state.hashBad++;
    snaps.push({ c: clock, f: d.fr, p: S.fighters.map((f) => [f.x, f.y]) }); if (snaps.length > 12) snaps.shift();
    lastMasks = d.m.slice(); noteStocks(S, true);
    if (ownIdx >= 0) { for (let g = (d.ak[ownIdx] | 0) + 1; g <= gf; g++) { const m = lastMasks.slice(); m[ownIdx] = hist[g] | 0; S.step(m); } }
    S.events.length = 0;
  }
  function onMsg(m) {
    if (!m || (m.room != null && room != null && m.room !== room)) return;
    if (m.t === 'sm.in' && mode === 'host') {
      let i = hostId(m); if (i == null) { i = fighters.findIndex((f, k) => !f.isLocal && !f.isBot && !f._bound); if (i >= 0) { fighters[i]._bound = true; if (m.id != null) idToIdx[m.id] = i; } }
      if (i >= 0 && i != null && m.f >= ack[i]) { remoteMask[i] = m.m | 0; ack[i] = m.f | 0; }
    } else if (m.t === 'sm.snap' && mode !== 'host' && mode !== 'local') onSnap(m);
    else if (m.t === 'sm.end') { const res = m.result || null; if (res) { if (mode === 'guest' || mode === 'spec') showFinal(res); fire(res); } else if (!done) fire({ winner: -1, slots: fighters.map((f) => f.slot), reason: m.reason || 'host' }); }
  }
  function showFinal(res) { // make sure the results card shows even if the local sim has not reached 'over' yet
    if (V.phase === 'over' && V.result) return;
    const rows = V.fighters.map((f) => ({ i: f.i, fid: f.fid, stocks: f.stocks, pct: Math.floor(f.pct), kos: f.kos, dealt: Math.floor(f.dealt), taken: Math.floor(f.taken), ctrl: f.ctrl }));
    rows.sort((a, b) => b.stocks - a.stocks || a.pct - b.pct);
    const wi = res.winner >= 0 ? fighters.findIndex((f) => f.slot === res.winner) : -1;
    ui.onEvent({ t: 'over', result: { winner: wi, rows, frames: V.frame, time: 0 } }, gameLike);
  }

  // ---- stepping
  function maskFor(i) {
    const f = S.fighters[i]; if (f.bot) return botInput(S, f);
    const li = localIdx.indexOf(i); if (li >= 0) return localMask[li] | 0;
    return remoteMask[i] | 0;
  }
  function tick() {
    if (mode === 'guest' || mode === 'spec') {
      gf++; const own = ownIdx >= 0 ? localMask[0] | 0 : 0; hist[gf] = own;
      if (ownIdx >= 0 && net) net.send({ t: 'sm.in', room, f: gf, m: own });
      const m = lastMasks.slice(); if (ownIdx >= 0) m[ownIdx] = own; S.step(m); drainEvents(S, true);
    } else {
      const masks = S.fighters.map((f, i) => maskFor(i)); lastMasks = masks; S.step(masks);
      if (opts.history) opts.history[S.frame] = S.fighters.map((f) => [f.x, f.y]);
      noteStocks(S, false); drainEvents(S, false);
      if (mode === 'host' && net && (S.frame & 1) === 0) sendSnap();
    }
    updateView(view, V === S ? S : V); state.frame = S.frame; state.phase = S.phase === 'over' ? 'over' : done ? 'over' : S.phase;
  }

  // guest/spec display state: P with interpolated remote positions + smoothed own position
  function buildView() {
    if (mode !== 'guest' && mode !== 'spec') { V = S; return 1; }
    const rt = clock - 0.1; let a = null, b = null;
    for (let k = 0; k < snaps.length; k++) { if (snaps[k].c <= rt) a = snaps[k]; else { b = snaps[k]; break; } }
    const fighters2 = S.fighters.map((f, i) => {
      const o = Object.create(f); let x = f.x, y = f.y, fr = S.frame;
      if (i !== ownIdx && (a || b)) {
        let p; if (a && b) { const u = (rt - a.c) / Math.max(1e-6, b.c - a.c); p = [a.p[i][0] + (b.p[i][0] - a.p[i][0]) * u, a.p[i][1] + (b.p[i][1] - a.p[i][1]) * u]; fr = a.f + (b.f - a.f) * u; } else { p = (a || b).p[i]; fr = (a || b).f; }
        if (Math.abs(p[0] - f.x) < 80 || !snaps.length) { x = p[0]; y = p[1]; } else { x = f.x; y = f.y; }
      } else if (i === ownIdx) {
        let d = disp[i]; if (!d) d = disp[i] = { x: f.x, y: f.y };
        const ex = f.x - d.x, ey = f.y - d.y;
        if (Math.abs(ex) > 24 || Math.abs(ey) > 24) { d.x = f.x; d.y = f.y; } else { d.x += ex * 0.5; d.y += ey * 0.5; if (Math.abs(f.x - d.x) < 0.01) d.x = f.x; if (Math.abs(f.y - d.y) < 0.01) d.y = f.y; }
        x = d.x; y = d.y;
      }
      o.x = o.px = x; o.y = o.py = y; o._vf = fr; return o;
    });
    V = Object.create(S); V.fighters = fighters2; return 1;
  }
  function refreshState() {
    state.view = V.fighters.map((f, i) => ({ slot: fighters[i].slot, x: f.x, y: f.y, f: f._vf != null ? f._vf : S.frame }));
    state.pct = S.fighters.map((f) => Math.floor(f.pct)); state.stocks = S.fighters.map((f) => f.stocks);
  }
  let bb = null, bbLast = -1;
  function render(alpha) {
    buildView(); R.begin(); drawScene(R, V, at, vis, view, alpha, false); R.end(); ui.update(V, gameLike); refreshState();
    if (bb && clock - bbLast >= 1 / 15) { bbLast = clock; drawBillboard(bb); }
  }
  function drawBillboard(c) {
    const g = c.getContext('2d'), w = c.width, h = c.height; g.imageSmoothingEnabled = false;
    const top = (opts.skyPalette && opts.skyPalette.top) || '#12082a', hor = (opts.skyPalette && opts.skyPalette.horizon) || '#5a2a8a';
    const gr = g.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, typeof top === 'number' ? '#' + top.toString(16).padStart(6, '0') : top); gr.addColorStop(1, typeof hor === 'number' ? '#' + hor.toString(16).padStart(6, '0') : hor);
    g.fillStyle = gr; g.fillRect(0, 0, w, h); g.drawImage(ui.canvas, 0, 0, 480, 270, 0, 0, w, h);
    const sc = w / 480; g.font = 'bold ' + Math.max(9, 12 * sc) + 'px monospace'; g.textBaseline = 'top';
    V.fighters.forEach((f, i) => { const x = 6 + i * (w / Math.max(2, N)); g.fillStyle = cols[i].hex; g.fillText((fighters[i].name || 'P' + (i + 1)).slice(0, 8) + ' ' + Math.floor(f.pct) + '%  ' + '●'.repeat(Math.max(0, f.stocks)), x, h - 16 * sc - 4); });
    if (V.phase === 'over') { g.fillStyle = '#fff'; g.fillText('GAME!', w / 2 - 20, 6); }
  }

  const sess = {
    el, state, stageDef, cfg,
    start() {
      if (running) return; running = true; el.style.display = ''; state.overlay = true; state.phase = 'count';
      PLAYER_COLORS.forEach((_, i) => { if (cols[i]) PLAYER_COLORS[i] = cols[i].hex; });
      if (net) net.onMessage(onMsg);
      updateView(view, S); view.x = S.fighters.reduce((a, f) => a + f.x, 0) / Math.max(1, N); ui.onStart(S, gameLike);
      el.querySelectorAll('.sm-card .nm').forEach((nm, i) => { const t = [...nm.childNodes].find((n) => n.nodeType === 3); if (t && fighters[i] && fighters[i].name) t.data = ' ' + String(fighters[i].name).slice(0, 10).toUpperCase() + ' '; });
      render(1);
    },
    stop() {
      if (!running && !el.parentNode) return; running = false; el.style.display = 'none'; state.overlay = false;
      PLAYER_COLORS.splice(0, PLAYER_COLORS.length, ...savedColors); if (el.parentNode) el.parentNode.removeChild(el);
      if (net && net.offMessage) net.offMessage(onMsg);
    },
    update(dt) {
      if (!running) return; dt = Math.min(0.1, dt || 0); clock += dt; acc += dt; let n = 0;
      while (acc >= 1 / 60 && n < 5) { acc -= 1 / 60; n++; if (!(S.phase === 'over' && (mode === 'host' || mode === 'local' ? done : false))) tick(); }
      if (n === 5) acc = 0;
      render(acc * 60);
    },
    setInput(mask, which) { localMask[which | 0] = mask | 0; },
    onResult(fn) { resultCbs.push(fn); if (done && state.result) fn(state.result); },
    hash() { return S.hash(); },
    sim: S, _onMsg: onMsg, _billboard(c) { bb = c; bbLast = -1; drawBillboard(c); },
  };
  Object.defineProperty(sess, 'sim', { get() { return S; } });
  return sess;
}

// Canvas the 3D side can texture onto a sprite for spectators; redrawn by the session at 15 Hz (while it updates).
export function renderBillboard(session, w, h) {
  const c = document.createElement('canvas'); c.width = w || 256; c.height = h || 144; session._billboard(c); return c;
}

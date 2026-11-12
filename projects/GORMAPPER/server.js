#!/usr/bin/env node
// GORMAPPER server — Node built-ins only. No dependencies, no build step.
//
//   node server.js            → http://localhost:8711
//
// Routes:
//   GET  /                        index.html
//   GET  /editor                  editor.html
//   GET  /output                  output.html
//   GET  /api/scenes              ["default", ...]
//   GET  /api/scene/active        { name, scene }           (current active scene)
//   GET  /api/scene/:name         scene JSON
//   POST /api/scene/:name         save scene JSON body → scenes/:name.json
//   GET  /api/scene/:name/activate  set active scene (output picks it up)
//   POST /api/upload?name=f.mp4   raw body → media/f.mp4, returns { url }
//   GET  /media/*                 uploaded media
//   GET  /api/patterns            [{ id, name, params }] from patterns/*.glsl
//   GET  /patterns/:id.glsl       pattern source (hot-editable)
//   GET  /api/events              SSE: pattern file changes + scene activation
//   POST /api/live                editor pushes live edit state; SSE-broadcast
//                                 to every output/overlay window (real-time
//                                 editing without BroadcastChannel, e.g. the
//                                 Electron overlay). GET returns last state.
//   GET  /api/draft/:name         { draft: {ts,scene}|null, sceneMtime: ms|null }
//   POST /api/draft/:name         autosave draft → scenes/.drafts/:name.json
//   DELETE /api/draft/:name       discard draft
//
// Every POST /api/scene/:name first copies the previous version to
// scenes/.history/<name>@<stamp>.json (last 20 kept per scene), then clears
// that scene's draft — an explicit save is always recoverable and final.

'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');
const dgram = require('dgram');

const PORT = 8711;
const RING_UDP_PORT = 21325;   // lightserver canvas-tap frames land here
const RING_META_PORT = 21326;  // lightserver semantic packets (tracks/stats)
const RING_PIXELS = 1920;      // smartGOR room ring length
const LIGHTSERVER = { host: 'gorcave.local', port: 8800 };
const ROOT = __dirname;
const PUBLIC_DIR = path.join(ROOT, 'public');
const SCENES_DIR = path.join(ROOT, 'scenes');
const MEDIA_DIR = path.join(ROOT, 'media');
const PATTERNS_DIR = path.join(ROOT, 'patterns');
const DRAFTS_DIR = path.join(SCENES_DIR, '.drafts');
const HISTORY_DIR = path.join(SCENES_DIR, '.history');
const ACTIVE_FILE = path.join(SCENES_DIR, '.active');
const HISTORY_KEEP = 20;

for (const d of [PUBLIC_DIR, SCENES_DIR, MEDIA_DIR, PATTERNS_DIR, DRAFTS_DIR, HISTORY_DIR]) {
  if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true });
}

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.webp': 'image/webp',
  '.svg': 'image/svg+xml',
  '.mp4': 'video/mp4',
  '.webm': 'video/webm',
  '.mov': 'video/quicktime',
  '.txt': 'text/plain; charset=utf-8',
  '.glsl': 'text/plain; charset=utf-8',
};

// ---------- helpers ----------------------------------------------------------

function send(res, status, body, type) {
  const buf = typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body);
  res.writeHead(status, {
    'Content-Type': type || 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(buf),
    'Cache-Control': 'no-store',
  });
  res.end(buf);
}

function sendError(res, status, msg) {
  send(res, status, { error: msg });
}

// Only allow simple names: letters, digits, dash, underscore, dot (no traversal).
function safeName(name) {
  if (typeof name !== 'string') return null;
  const clean = name.replace(/[^a-zA-Z0-9._ -]/g, '_').replace(/\.\.+/g, '.').trim();
  if (!clean || clean.startsWith('.')) return null;
  return clean;
}

function readBody(req, limit, cb) {
  const chunks = [];
  let size = 0;
  let done = false;
  req.on('data', (c) => {
    size += c.length;
    if (size > limit) {
      done = true;
      req.destroy();
      cb(new Error('body too large'));
      return;
    }
    chunks.push(c);
  });
  req.on('end', () => { if (!done) cb(null, Buffer.concat(chunks)); });
  req.on('error', (e) => { if (!done) { done = true; cb(e); } });
}

function serveFile(res, filePath) {
  fs.stat(filePath, (err, st) => {
    if (err || !st.isFile()) return sendError(res, 404, 'not found');
    const type = MIME[path.extname(filePath).toLowerCase()] || 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': type, 'Content-Length': st.size, 'Cache-Control': 'no-store' });
    fs.createReadStream(filePath).pipe(res);
  });
}

// ---------- active scene -----------------------------------------------------

function getActiveName() {
  let saved = null;
  try { saved = safeName(fs.readFileSync(ACTIVE_FILE, 'utf8').trim()); } catch (e) { /* none */ }
  // last active → room-baseline → default → whatever exists
  for (const n of [saved, 'room-baseline', 'default']) {
    if (n && fs.existsSync(path.join(SCENES_DIR, n + '.json'))) return n;
  }
  return 'default';
}

function setActiveName(name) {
  fs.writeFileSync(ACTIVE_FILE, name + '\n');
}

// ---------- SSE (/api/events) + patterns/ hot reload -------------------------

const sseClients = new Set();
let lastLive = null;   // most recent editor live-edit state (in-memory only)

// ---- LED-ring canvas tap (UDP from the smartGOR lightserver) ----------------
// Frame Protocol v1: 'F','C', version, flags(bit0=SHOW), offset u16le,
// count u16le, then count*3 RGB bytes. Frames arrive chunked; SHOW on the
// final chunk bumps the sequence so /api/ring always serves whole frames.
const ringBuf = new Uint8Array(RING_PIXELS * 3);
let ringSeq = 0;
let ringLastMs = 0;
let ringSock = null;
let metaSock = null;
let ringMeta = null;      // latest semantic packet (tracks/stats/projector)
let ringMetaMs = 0;

// ---- clock sync + latency (perfect wall/strip sync) -------------------------
// NTP-lite against the Pi's GET /time: 10 probes, keep the min-RTT sample.
// offsetMs = piClock - localClock. Each semantic packet carries the Pi's
// send time; latencyMs = how old the packet is when it lands here. The
// renderer extrapolates track positions forward by latency + user trim.
let clockOffsetMs = null;
let latencyMs = 0;        // EMA-smoothed pipeline latency
const TRIM_FILE = path.join(SCENES_DIR, '.latency-trim');
let trimMs = 0;
try { trimMs = +fs.readFileSync(TRIM_FILE, 'utf8') || 0; } catch (e) { /* default 0 */ }
let calib = null;         // last room calibration fetched from the lightserver
let wallStatus = { ok: true, error: null, using: 'style', at: null };   // last glsl compile result

function fetchPiJson(pathname, cb) {
  const req = http.request({ host: LIGHTSERVER.host, port: LIGHTSERVER.port, path: pathname, timeout: 2000 },
    (res) => {
      let b = '';
      res.on('data', (d) => { b += d; });
      res.on('end', () => { try { cb(null, JSON.parse(b)); } catch (e) { cb(e); } });
    });
  req.on('error', cb);
  req.on('timeout', () => { req.destroy(); cb(new Error('timeout')); });
  req.end();
}

function syncClock(done) {
  let best = { rtt: Infinity, offset: null };
  let i = 0;
  (function probe() {
    if (i++ >= 10) {
      if (best.offset !== null) clockOffsetMs = best.offset;
      if (done) done();
      return;
    }
    const t0 = Date.now();
    fetchPiJson('/time', (err, j) => {
      const t1 = Date.now();
      if (!err && j && typeof j.unix === 'number') {
        const rtt = t1 - t0;
        if (rtt < best.rtt) best = { rtt, offset: j.unix * 1000 - (t0 + t1) / 2 };
      }
      setTimeout(probe, 40);
    });
  })();
}

function refreshCalibration() {
  fetchPiJson('/calibration', (err, j) => { if (!err && j) calib = j.points || j; });
}

function startRingListener() {
  if (ringSock) return;
  ringSock = dgram.createSocket('udp4');
  ringSock.on('message', (msg) => {
    if (msg.length < 8 || msg[0] !== 0x46 || msg[1] !== 0x43) return;   // 'F','C'
    const flags = msg[3];
    const offset = msg.readUInt16LE(4);
    const count = msg.readUInt16LE(6);
    if (offset + count > RING_PIXELS || msg.length < 8 + count * 3) return;
    ringBuf.set(msg.subarray(8, 8 + count * 3), offset * 3);
    if (flags & 1) { ringSeq++; ringLastMs = Date.now(); }
  });
  ringSock.on('error', () => { try { ringSock.close(); } catch (e) {} ringSock = null; });
  ringSock.bind(RING_UDP_PORT);
  ringSock.unref();   // never keep a process alive just for the tap

  // semantic side-channel: 10Hz JSON datagrams with ground-truth object
  // tracks, room stats, and the scene's verbatim "projector" block
  metaSock = dgram.createSocket('udp4');
  metaSock.on('message', (msg) => {
    if (msg[0] !== 0x7b) return;   // '{'
    try {
      ringMeta = JSON.parse(msg.toString('utf8'));
      ringMetaMs = Date.now();
      if (typeof ringMeta.ts === 'number' && clockOffsetMs !== null) {
        const lag = ringMetaMs - (ringMeta.ts * 1000 - clockOffsetMs);
        if (lag > -500 && lag < 2000) latencyMs += (lag - latencyMs) * 0.1;
      }
    } catch (e) { /* malformed packet — ignore */ }
  });
  metaSock.on('error', () => { try { metaSock.close(); } catch (e) {} metaSock = null; });
  metaSock.bind(RING_META_PORT);
  metaSock.unref();
}

function sseBroadcast(event) {
  const data = 'data: ' + JSON.stringify(event) + '\n\n';
  for (const res of sseClients) res.write(data);
}

// parse the //! metadata header of a pattern file
function patternMeta(id, src) {
  const meta = { id, name: id, params: ['speed', 'color1', 'color2', 'brightness', 'opacity'] };
  for (const line of src.split('\n').slice(0, 12)) {
    const m = /^\s*\/\/!\s*(\w+)\s*:\s*(.+)$/.exec(line);
    if (!m) continue;
    if (m[1] === 'name') meta.name = m[2].trim();
    else if (m[1] === 'params') meta.params = m[2].split(',').map((s) => s.trim()).filter(Boolean);
  }
  return meta;
}

function listPatterns() {
  const out = [];
  for (const f of fs.readdirSync(PATTERNS_DIR).sort()) {
    if (!f.endsWith('.glsl')) continue;
    const id = f.slice(0, -5);
    let src = '';
    try { src = fs.readFileSync(path.join(PATTERNS_DIR, f), 'utf8'); } catch (e) { continue; }
    out.push(patternMeta(id, src));
  }
  return out;
}

// watch patterns/ — debounce per file (editors fire several events per save)
const watchTimers = new Map();
try {
  const watcher = fs.watch(PATTERNS_DIR, (eventType, filename) => {
    if (!filename || !filename.endsWith('.glsl')) return;
    const id = filename.slice(0, -5);
    clearTimeout(watchTimers.get(id));
    watchTimers.set(id, setTimeout(() => {
      watchTimers.delete(id);
      const exists = fs.existsSync(path.join(PATTERNS_DIR, filename));
      sseBroadcast({ type: 'pattern', id, exists });
      console.log('pattern ' + (exists ? 'changed' : 'removed') + ': ' + id);
    }, 120));
  });
  // don't keep a process alive just for the watcher (matters when this file
  // is require()d — e.g. the Electron app, or a quick getActiveName() probe)
  watcher.unref();
} catch (e) {
  console.warn('fs.watch on patterns/ unavailable:', e.message);
}

// ---------- server -----------------------------------------------------------

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const p = decodeURIComponent(url.pathname);

  // -- API ------------------------------------------------------------------
  if (p === '/api/events' && req.method === 'GET') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-store',
      'Connection': 'keep-alive',
    });
    res.write('data: {"type":"hello"}\n\n');
    sseClients.add(res);
    const ping = setInterval(() => res.write(': ping\n\n'), 25000);
    req.on('close', () => { clearInterval(ping); sseClients.delete(res); });
    return;
  }

  if (p === '/api/live') {
    if (req.method === 'POST') {
      readBody(req, 8 * 1024 * 1024, (err, buf) => {
        if (err) return sendError(res, 413, err.message);
        let state;
        try { state = JSON.parse(buf.toString('utf8')); } catch (e) { return sendError(res, 400, 'invalid JSON'); }
        lastLive = state;
        sseBroadcast({
          type: 'live',
          name: state.name,
          scene: state.scene,
          editMode: state.editMode,
          selectedId: state.selectedId,
        });
        send(res, 200, { ok: true });
      });
      return;
    }
    if (req.method === 'GET') {
      send(res, 200, lastLive || null);
      return;
    }
    return sendError(res, 405, 'method not allowed');
  }

  // latest LED-ring frame from the smartGOR lightserver canvas tap (UDP :21325)
  if (p === '/api/ring' && req.method === 'GET') {
    const age = Date.now() - ringLastMs;
    if (!ringSeq || age > 3000) { res.writeHead(204, { 'X-Ring-Age-Ms': String(age) }); res.end(); return; }
    res.writeHead(200, {
      'Content-Type': 'application/octet-stream',
      'Cache-Control': 'no-store',
      'X-Ring-Seq': String(ringSeq),
      'X-Ring-Pixels': String(RING_PIXELS),
      'X-Ring-Age-Ms': String(age),
    });
    res.end(Buffer.from(ringBuf));
    return;
  }

  // latest semantic packet (object tracks, stats, projector block) — 204 when
  // stale — wrapped with sync info (measured latency + user trim) and the
  // room calibration so the renderer gets everything in one poll
  if (p === '/api/ring/meta' && req.method === 'GET') {
    if (!ringMeta || Date.now() - ringMetaMs > 2000) { res.writeHead(204); res.end(); return; }
    send(res, 200, Object.assign({}, ringMeta, {
      // received_at: server wall-clock when the packet landed — same clock as
      // the browser (same machine), so the renderer can extrapolate by the
      // TRUE age of the data at each rendered frame instead of a fixed guess
      _sync: {
        latency_ms: Math.round(latencyMs * 10) / 10,
        trim_ms: trimMs,
        offset_ms: clockOffsetMs,
        received_at: ringMetaMs,
      },
      _calib: calib,
    }));
    return;
  }

  // wall-program compile status — pages POST results after compiling a
  // Pi-authored projector.glsl; the Pi-side generator/agents poll the GET
  // (reachable from the Pi at http://<tap host>:8711/api/wall/status)
  if (p === '/api/wall/status') {
    if (req.method === 'GET') { send(res, 200, wallStatus); return; }
    if (req.method === 'POST') {
      readBody(req, 64 * 1024, (err, buf) => {
        if (err) return sendError(res, 413, err.message);
        try { wallStatus = Object.assign(JSON.parse(buf.toString('utf8')), { at: new Date().toISOString() }); }
        catch (e) { return sendError(res, 400, 'invalid JSON'); }
        send(res, 200, { ok: true });
      });
      return;
    }
    return sendError(res, 405, 'method not allowed');
  }

  // user latency trim (± ms), persisted — the by-eye sync fine-tune
  if (p === '/api/latency') {
    if (req.method === 'GET') { send(res, 200, { latency_ms: latencyMs, trim_ms: trimMs }); return; }
    if (req.method === 'POST') {
      readBody(req, 1024, (err, buf) => {
        if (err) return sendError(res, 413, err.message);
        try {
          const t = JSON.parse(buf.toString('utf8')).trim_ms;
          if (typeof t !== 'number' || t < -1000 || t > 1000) return sendError(res, 400, 'trim_ms out of range');
          trimMs = t;
          fs.writeFile(TRIM_FILE, String(trimMs), () => {});
          send(res, 200, { ok: true, trim_ms: trimMs });
        } catch (e) { sendError(res, 400, 'invalid JSON'); }
      });
      return;
    }
    return sendError(res, 405, 'method not allowed');
  }

  // proxy the lightserver's room calibration (named ring points, e.g.
  // projL/projR) so the browser editor can read it without CORS trouble
  if (p === '/api/calibration' && req.method === 'GET') {
    const preq = http.request({
      host: LIGHTSERVER.host, port: LIGHTSERVER.port, path: '/calibration',
      method: 'GET', timeout: 3000,
    }, (pres) => {
      let body = '';
      pres.on('data', (d) => { body += d; });
      pres.on('end', () => {
        res.writeHead(pres.statusCode, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
        res.end(body);
      });
    });
    preq.on('error', () => sendError(res, 502, 'lightserver unreachable'));
    preq.on('timeout', () => { preq.destroy(); sendError(res, 502, 'lightserver timeout'); });
    preq.end();
    return;
  }

  if (p === '/api/patterns' && req.method === 'GET') {
    try { send(res, 200, listPatterns()); } catch (e) { sendError(res, 500, e.message); }
    return;
  }

  if (p === '/api/scenes' && req.method === 'GET') {
    fs.readdir(SCENES_DIR, (err, files) => {
      if (err) return sendError(res, 500, err.message);
      const names = files.filter((f) => f.endsWith('.json')).map((f) => f.slice(0, -5)).sort();
      send(res, 200, names);
    });
    return;
  }

  if (p === '/api/scene/active' && req.method === 'GET') {
    const name = getActiveName();
    fs.readFile(path.join(SCENES_DIR, name + '.json'), 'utf8', (err, data) => {
      if (err) return sendError(res, 404, 'active scene "' + name + '" not found');
      let scene;
      try { scene = JSON.parse(data); } catch (e) { return sendError(res, 500, 'bad scene JSON on disk'); }
      // tolerate a draft-shaped file ({ts, scene:{...}}) saved as a scene
      if (scene && scene.scene && !Array.isArray(scene.layers) && Array.isArray(scene.scene.layers)) {
        scene = scene.scene;
      }
      send(res, 200, { name, scene });
    });
    return;
  }

  let m = p.match(/^\/api\/scene\/([^/]+)\/activate$/);
  if (m && req.method === 'GET') {
    const name = safeName(m[1]);
    if (!name) return sendError(res, 400, 'bad scene name');
    if (!fs.existsSync(path.join(SCENES_DIR, name + '.json'))) return sendError(res, 404, 'no such scene');
    setActiveName(name);
    sseBroadcast({ type: 'active', name });   // output reloads instantly
    send(res, 200, { ok: true, active: name });
    return;
  }

  m = p.match(/^\/api\/draft\/([^/]+)$/);
  if (m) {
    const name = safeName(m[1]);
    if (!name) return sendError(res, 400, 'bad scene name');
    const draftFile = path.join(DRAFTS_DIR, name + '.json');
    if (req.method === 'GET') {
      let draft = null;
      try { draft = JSON.parse(fs.readFileSync(draftFile, 'utf8')); } catch (e) { /* none */ }
      let sceneMtime = null;
      try { sceneMtime = fs.statSync(path.join(SCENES_DIR, name + '.json')).mtimeMs; } catch (e) { /* none */ }
      send(res, 200, { draft, sceneMtime });
      return;
    }
    if (req.method === 'POST') {
      readBody(req, 8 * 1024 * 1024, (err, buf) => {
        if (err) return sendError(res, 413, err.message);
        let draft;
        try { draft = JSON.parse(buf.toString('utf8')); } catch (e) { return sendError(res, 400, 'invalid JSON'); }
        fs.writeFile(draftFile, JSON.stringify(draft) + '\n', (werr) => {
          if (werr) return sendError(res, 500, werr.message);
          send(res, 200, { ok: true, name });
        });
      });
      return;
    }
    if (req.method === 'DELETE') {
      fs.unlink(draftFile, () => send(res, 200, { ok: true }));
      return;
    }
    return sendError(res, 405, 'method not allowed');
  }

  m = p.match(/^\/api\/scene\/([^/]+)$/);
  if (m) {
    const name = safeName(m[1]);
    if (!name || name === 'active') {
      if (req.method === 'GET' && name === 'active') { /* handled above */ }
      return sendError(res, 400, 'bad scene name');
    }
    const file = path.join(SCENES_DIR, name + '.json');
    if (req.method === 'GET') {
      serveFile(res, file);
      return;
    }
    if (req.method === 'POST') {
      readBody(req, 8 * 1024 * 1024, (err, buf) => {
        if (err) return sendError(res, 413, err.message);
        let scene;
        try { scene = JSON.parse(buf.toString('utf8')); } catch (e) { return sendError(res, 400, 'invalid JSON'); }
        // keep a history copy of the previous version, prune to the last N
        try {
          if (fs.existsSync(file)) {
            const stamp = new Date().toISOString().replace(/[:.]/g, '-');
            fs.copyFileSync(file, path.join(HISTORY_DIR, name + '@' + stamp + '.json'));
            const old = fs.readdirSync(HISTORY_DIR).filter((f) => f.startsWith(name + '@')).sort();
            while (old.length > HISTORY_KEEP) fs.unlinkSync(path.join(HISTORY_DIR, old.shift()));
          }
        } catch (e) { console.warn('history copy failed:', e.message); }
        fs.writeFile(file, JSON.stringify(scene, null, 2) + '\n', (werr) => {
          if (werr) return sendError(res, 500, werr.message);
          // an explicit save supersedes any autosaved draft
          fs.unlink(path.join(DRAFTS_DIR, name + '.json'), () => {});
          send(res, 200, { ok: true, name });
        });
      });
      return;
    }
    return sendError(res, 405, 'method not allowed');
  }

  if (p === '/api/upload' && req.method === 'POST') {
    const raw = url.searchParams.get('name') || req.headers['x-filename'] || 'upload.bin';
    const name = safeName(path.basename(raw));
    if (!name) return sendError(res, 400, 'bad file name');
    readBody(req, 512 * 1024 * 1024, (err, buf) => {
      if (err) return sendError(res, 413, err.message);
      if (!buf.length) return sendError(res, 400, 'empty body');
      fs.writeFile(path.join(MEDIA_DIR, name), buf, (werr) => {
        if (werr) return sendError(res, 500, werr.message);
        send(res, 200, { ok: true, url: '/media/' + encodeURIComponent(name) });
      });
    });
    return;
  }

  if (p.startsWith('/api/')) return sendError(res, 404, 'no such endpoint');

  // -- media / patterns ------------------------------------------------------
  if (p.startsWith('/media/')) {
    const name = safeName(path.basename(p));
    if (!name) return sendError(res, 404, 'not found');
    serveFile(res, path.join(MEDIA_DIR, name));
    return;
  }

  if (p.startsWith('/patterns/')) {
    const name = safeName(path.basename(p));
    if (!name || !name.endsWith('.glsl')) return sendError(res, 404, 'not found');
    serveFile(res, path.join(PATTERNS_DIR, name));
    return;
  }

  // -- static ---------------------------------------------------------------
  if (req.method !== 'GET') return sendError(res, 405, 'method not allowed');
  let rel = p;
  if (rel === '/') rel = '/index.html';
  if (rel === '/editor') rel = '/editor.html';
  if (rel === '/output') rel = '/output.html';
  const filePath = path.normalize(path.join(PUBLIC_DIR, rel));
  if (!filePath.startsWith(PUBLIC_DIR + path.sep)) return sendError(res, 403, 'forbidden');
  serveFile(res, filePath);
});

// ---------- start / embed ----------------------------------------------------
// `node server.js` runs standalone; the Electron app requires this file and
// calls start() — if another GORMAPPER already owns the port, it is detected
// and reused instead of crashing (cb gets mode 'reused').

function probeExistingGormapper(port, cb) {
  const req = http.get({ host: '127.0.0.1', port, path: '/api/scenes', timeout: 1500 }, (res) => {
    let body = '';
    res.on('data', (c) => { body += c; });
    res.on('end', () => {
      let ok = false;
      try { ok = res.statusCode === 200 && Array.isArray(JSON.parse(body)); } catch (e) { /* not ours */ }
      cb(ok);
    });
  });
  req.on('error', () => cb(false));
  req.on('timeout', () => { req.destroy(); cb(false); });
}

function start(port, cb) {
  port = port || PORT;
  server.once('error', (err) => {
    if (err.code === 'EADDRINUSE') {
      probeExistingGormapper(port, (isOurs) => {
        if (isOurs) {
          console.log('GORMAPPER already running on port ' + port + ' — reusing it');
          if (cb) cb(null, 'reused');
        } else if (cb) cb(new Error('port ' + port + ' is in use by something else'));
      });
      return;
    }
    if (cb) cb(err);
  });
  server.listen(port, () => {
    console.log('GORMAPPER  http://localhost:' + port);
    console.log('  editor   http://localhost:' + port + '/editor');
    console.log('  output   http://localhost:' + port + '/output   (browser mode)');
    console.log('  active scene: ' + getActiveName());
    startRingListener();
    console.log('  ring tap  udp :' + RING_UDP_PORT + ' -> GET /api/ring');
    syncClock(() => console.log('  clock offset vs Pi: '
      + (clockOffsetMs === null ? 'unmeasured (Pi unreachable)' : clockOffsetMs.toFixed(1) + 'ms')));
    refreshCalibration();
    const resync = setInterval(() => { syncClock(); refreshCalibration(); }, 300000);
    resync.unref();
    if (cb) cb(null, 'started');
  });
}

function stop(cb) {
  for (const res of sseClients) { try { res.end(); } catch (e) { /* ignore */ } }
  sseClients.clear();
  if (ringSock) { try { ringSock.close(); } catch (e) { /* ignore */ } ringSock = null; }
  if (metaSock) { try { metaSock.close(); } catch (e) { /* ignore */ } metaSock = null; }
  server.close(cb);
}

if (require.main === module) {
  start(PORT, (err) => {
    if (err) { console.error(err.message); process.exit(1); }
  });
}

module.exports = { start, stop, PORT, getActiveName, ROOT };

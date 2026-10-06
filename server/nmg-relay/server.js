'use strict';
/* NO MANS GOR relay. Dumb fan-out + presence, persists only profiles/discoveries/events via store.js; nothing logged.
   Protocol and limits: docs/multiplayer-plan.md. Only dependency: ws. */
const http = require('http');
const { WebSocketServer } = require('ws');
const crypto = require('crypto');
const { handleGor } = require('./gor');
const store = require('./store');

const PORT = +process.env.PORT || 8796;
const HOST = process.env.HOST || '127.0.0.1';
const MAX_CLIENTS = 32, MAX_PER_IP = 4, MAX_PAYLOAD = 768, PROF_MAX = 65536, DISC_MAX = 2000;
const BUCKET_RATE = 30, BUCKET_BURST = 60, OVER_CLOSE_MS = 5000;
const FIRE_MAX = 12;                 // per second per connection
const SILENCE_MS = +process.env.SILENCE_MS || 15000, SWEEP_MS = 5000, STRIKES_MAX = 5;
const EPOCH = Date.now();            // constant until restart
const ALLOW_NO_ORIGIN = process.env.ALLOW_NO_ORIGIN === '1';   // test bots only
const ORIGIN_RE = /^(https:\/\/(www\.)?emgor\.online|http:\/\/(localhost|127\.0\.0\.1)(:\d+)?)$/;

const players = new Map();           // id -> { ws, id, name, color, s, last, pos }
const perIp = new Map();             // ip -> open sockets

function ipOf(req) {
  const xf = req.headers['x-forwarded-for'];
  if (xf) return String(xf).split(',')[0].trim();
  return req.socket.remoteAddress || '?';
}
function num(v, lim) { return typeof v === 'number' && isFinite(v) && Math.abs(v) < (lim || 1e7); }
function cleanName(s, id) {
  s = String(s == null ? '' : s).replace(/[^\x20-\x7e]/g, '').trim().slice(0, 16);
  return s || ('PILOT-' + id.slice(-4));
}
function cleanColor(c) {
  c = +c;
  return isFinite(c) ? (Math.floor(c) & 0xffffff) : 0x8a5cff;
}
function send(ws, obj) {
  if (ws.readyState === 1) { try { ws.send(JSON.stringify(obj)); } catch (e) { /* gone */ } }
}
function shipRec(p) {
  const o = { id: p.id, name: p.name, color: p.color, s: p.s, hs: p.hs || '' };
  if (p.pos) Object.assign(o, p.pos);
  return o;
}
function rosterFor(id) {
  const ships = [];
  players.forEach(function (p) { if (p.id !== id) ships.push(shipRec(p)); });
  return { t: 'roster', you: id, epoch: EPOCH, ships: ships };
}
function sendRoster(p) { send(p.ws, rosterFor(p.id)); }
function broadcastRoster() { players.forEach(sendRoster); }
function broadcast(obj, exceptId) {
  const txt = JSON.stringify(obj);
  players.forEach(function (p) { if (p.id !== exceptId && p.ws.readyState === 1) { try { p.ws.send(txt); } catch (e) { /* gone */ } } });
}

function cleanProfName(s) { s = typeof s === 'string' ? s.trim().toLowerCase() : ''; return /^[a-z0-9_-]{2,20}$/.test(s) ? s : ''; }
function cleanText(s, n) { return String(s == null ? '' : s).replace(/[^\x20-\x7e]/g, '').replace(/[<>]/g, '').trim().slice(0, n); }
function hashKey(k) { return crypto.createHash('sha256').update('nmg:' + k).digest('hex'); }
function keyOk(a, b) { return a.length === b.length && crypto.timingSafeEqual(Buffer.from(a), Buffer.from(b)); }

// ---- events: titan / meteor / friesSale / blockade every 20-40 min, persisted as { cur, seq, nextAt }
const EVENT_EVERY = +process.env.NMG_EVENT_EVERY_MS || 0;     // test override: fixed interval
const EVENT_KINDS = ['titan', 'meteor', 'friesSale', 'blockade'];
function rng(seed) { let a = seed >>> 0; return function () { a = (a + 0x6d2b79f5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function activeEvent(now) { const e = store.get('events').cur; return e && now < e.at + e.minutes * 60000 ? e : null; }
function eventMsg(e, now) {
  const o = { t: 'event', kind: e.kind, minutes: e.minutes, left: Math.max(0, Math.round((e.at + e.minutes * 60000 - now) / 1000)) };
  if (e.planetId != null) o.planetId = e.planetId;
  return o;
}
function fireEvent(now) {
  const ev = store.get('events');
  ev.seq = (ev.seq || 0) + 1;
  const r = rng(Math.floor(EPOCH / 1000) ^ Math.imul(ev.seq, 2654435761));
  const kind = EVENT_KINDS[Math.floor(r() * EVENT_KINDS.length)];
  const e = { kind: kind, minutes: kind === 'friesSale' ? 10 : 5 + Math.floor(r() * 6), at: now };
  if (kind === 'meteor' || kind === 'blockade') e.planetId = Math.floor(r() * 1000);   // client maps this to a planet index
  ev.cur = e;
  ev.nextAt = now + (EVENT_EVERY || (20 + r() * 20) * 60000);
  store.save('events');
  broadcast(eventMsg(e, now));
  scheduleEvent();
}
let evTimer = null;
function scheduleEvent() {
  clearTimeout(evTimer);
  const ev = store.get('events'), now = Date.now();
  if (!ev.nextAt) ev.nextAt = now + (EVENT_EVERY || 20 * 60000);
  evTimer = setTimeout(function () { fireEvent(Date.now()); }, Math.max(0, Math.min(ev.nextAt - now, 2 ** 31 - 1)));
  evTimer.unref();
}

const server = http.createServer(function (req, res) {
  const local = /^(::1|127\.0\.0\.1|::ffff:127\.0\.0\.1)$/.test(req.socket.remoteAddress || '');
  if (req.url === '/healthz' && local && !req.headers['x-forwarded-for']) {
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ n: players.size }));
    return;
  }
  res.writeHead(404); res.end();
});

const wss = new WebSocketServer({ noServer: true, maxPayload: PROF_MAX + 1024 })   // only prof.save may exceed MAX_PAYLOAD (checked in onmessage);

server.on('upgrade', function (req, socket, head) {
  const origin = req.headers.origin;
  const okOrigin = origin ? ORIGIN_RE.test(origin) : ALLOW_NO_ORIGIN;
  if (!okOrigin) { socket.write('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n'); socket.destroy(); return; }
  wss.handleUpgrade(req, socket, head, function (ws) { onConnection(ws, req); });
});

function onConnection(ws, req) {
  const ip = ipOf(req);
  if (wss.clients.size > MAX_CLIENTS) { ws.close(4001, 'full'); return; }     // counts every open socket, not just ones that said hi
  const n = perIp.get(ip) || 0;
  if (n >= MAX_PER_IP) { ws.close(4003, 'ip'); return; }
  perIp.set(ip, n + 1);

  const c = { profAt: 0, id: null, strikes: 0, tokens: BUCKET_BURST, tAt: Date.now(), overSince: 0, fireTokens: FIRE_MAX, fireAt: Date.now() };
  ws.on('error', function () { /* close follows */ });
  setTimeout(function () { if (!c.id) ws.close(4006, 'no hi'); }, 10000).unref();
  ws.on('close', function () {
    const k = (perIp.get(ip) || 1) - 1;
    if (k <= 0) perIp.delete(ip); else perIp.set(ip, k);
    const p = c.id && players.get(c.id);
    if (p && p.ws === ws) {
      players.delete(c.id);
      broadcast({ t: 'bye', id: c.id });
      broadcastRoster();
    }
  });
  ws.on('message', function (data, isBinary) {
    const now = Date.now();
    c.tokens = Math.min(BUCKET_BURST, c.tokens + (now - c.tAt) / 1000 * BUCKET_RATE); c.tAt = now;
    if (c.tokens < 1) {
      if (!c.overSince) c.overSince = now;
      else if (now - c.overSince > OVER_CLOSE_MS) { ws.close(4002, 'rate'); }
      return;
    }
    c.tokens -= 1; c.overSince = 0;
    let m = null;
    if (!isBinary && data.length > MAX_PAYLOAD && data.slice(0, 40).toString().indexOf('"prof.save"') < 0) { if (++c.strikes >= STRIKES_MAX) ws.close(4004, 'bad'); return; }
    if (!isBinary) { try { m = JSON.parse(data.toString()); } catch (e) { m = null; } }
    if (!m || typeof m !== 'object' || typeof m.t !== 'string' || !handle(ws, c, m, now)) {
      if (++c.strikes >= STRIKES_MAX) ws.close(4004, 'bad');
    }
  });
}

// returns false for a bad frame (counts as a strike)
function handle(ws, c, m, now) {
  if (m.t === 'hi') {
    if (typeof m.id !== 'string' || !/^[A-Za-z0-9]{6,16}$/.test(m.id)) return false;
    if (c.id && c.id !== m.id) return false;
    let p = players.get(m.id);
    if (!c.id) {
      if (p && p.ws !== ws) { const old = p.ws; players.delete(m.id); try { old.close(4000, 'replaced'); } catch (e) { /* gone */ } p = null; }
      c.id = m.id;
      p = { ws: ws, id: m.id, name: cleanName(m.name, m.id), color: cleanColor(m.color), s: 'docked', last: now, pos: null, hs: (typeof m.hs === 'string' ? m.hs.slice(0, 32) : '') };
      players.set(m.id, p);
    } else {
      p = players.get(c.id);
      if (!p || p.ws !== ws) return false;
      p.name = cleanName(m.name, c.id); p.color = cleanColor(m.color); p.last = now;
    }
    broadcastRoster();
    return true;
  }
  const p = c.id && players.get(c.id);
  if (!p || p.ws !== ws) return false;      // everything else needs a hi first
  p.last = now;
  switch (m.t) {
    case 'hb': return true;
    case 'ping': send(ws, { t: 'pong', c: num(m.c, 1e15) ? m.c : 0, now: now }); return true;
    case 'pos': {
      const keys = ['x', 'y', 'z', 'qx', 'qy', 'qz', 'qw', 'v', 'st', 'hp'];
      for (let i = 0; i < keys.length; i++) if (!num(m[keys[i]])) return false;
      const pos = { x: m.x, y: m.y, z: m.z, qx: m.qx, qy: m.qy, qz: m.qz, qw: m.qw, v: Math.round(m.v * 10) / 10, st: m.st & 15, hp: Math.max(0, Math.min(255, Math.round(m.hp))) };
      // mode: 'landed' | 'foot' (on-foot players send their human's pose; st bits 1/2/4 = moving/running/airborne)
      if (m.mode === 'landed' || m.mode === 'foot' || m.mode === 'docked') pos.mode = m.mode;
      // frame: 'station' = pose is station-local (docked / on the station deck)
      if (m.frame === 'station') pos.frame = 'station';
      p.pos = pos;
      broadcast(Object.assign({ t: 'pos', id: p.id }, pos), p.id);
      if (p.s !== 'piloting') { p.s = 'piloting'; broadcastRoster(); }
      return true;
    }
    case 'st':
      if (m.s !== 'docked' && m.s !== 'away') return false;
      if (p.s !== m.s) { p.s = m.s; broadcastRoster(); }
      return true;
    case 'fire': {
      const keys = ['w', 'x', 'y', 'z', 'dx', 'dy', 'dz'];
      for (let i = 0; i < keys.length; i++) if (!num(m[keys[i]])) return false;
      c.fireTokens = Math.min(FIRE_MAX, c.fireTokens + (now - c.fireAt) / 1000 * FIRE_MAX); c.fireAt = now;
      if (c.fireTokens < 1) return true;                 // over the fire cap: drop quietly
      c.fireTokens -= 1;
      broadcast({ t: 'fire', id: p.id, w: m.w | 0, x: m.x, y: m.y, z: m.z, dx: m.dx, dy: m.dy, dz: m.dz }, p.id);
      return true;
    }
    case 'kill':
      if (typeof m.by !== 'string' || !/^[A-Za-z0-9]{6,16}$/.test(m.by)) return false;
      broadcast({ t: 'kill', id: p.id, by: m.by });
      return true;
    case 'chat': {                       // plain multiplayer chat, fanned out to everyone (incl. sender)
      if (typeof m.text !== 'string') return false;
      const text = m.text.replace(/[^\x20-\x7e\u00a0-\uffff]/g, ' ').replace(/[<>]/g, '').trim().slice(0, 200);
      if (text) broadcast({ t: 'chat', from: p.name, text: text });
      return true;
    }
    case 'gor':                          // /gorcave: lobby neptr (gor.js). Replies async; never blocks the relay
      if (typeof m.text !== 'string') return false;
      handleGor(c, m.text, { name: p.name, reply: function (o) { send(ws, o); }, broadcast: function (o) { broadcast(o); } });
      return true;
    case 'prof.save': case 'prof.load': {
      if (now - c.profAt < 10000) { send(ws, { t: 'prof.err', why: 'rate' }); return true; }
      c.profAt = now;
      const nm = cleanProfName(m.name), key = typeof m.key === 'string' ? m.key : '';
      if (!nm || key.length < 4 || key.length > 64) { send(ws, { t: 'prof.err', why: 'bad' }); return true; }
      const profs = store.get('profiles'), rec = profs[nm], h = hashKey(key);
      if (m.t === 'prof.load') {
        if (!rec || !keyOk(rec.h, h)) send(ws, { t: 'prof.err', why: rec ? 'key' : 'none' });
        else send(ws, { t: 'prof', name: nm, data: rec.data });
        return true;
      }
      if (!m.data || typeof m.data !== 'object' || Array.isArray(m.data)) { send(ws, { t: 'prof.err', why: 'bad' }); return true; }
      const txt = JSON.stringify(m.data);
      if (txt.length > PROF_MAX) { send(ws, { t: 'prof.err', why: 'big' }); return true; }
      if (rec && !keyOk(rec.h, h)) { send(ws, { t: 'prof.err', why: 'key' }); return true; }
      if (!rec && store.size('profiles') + txt.length > store.MAX_BYTES - 4096) { send(ws, { t: 'prof.err', why: 'full' }); return true; }
      profs[nm] = { h: h, data: m.data, at: now };
      store.save('profiles');
      send(ws, { t: 'prof.ok', name: nm });
      return true;
    }
    case 'disc.claim': {
      if (m.kind !== 'planet' && m.kind !== 'creature' && m.kind !== 'resource') return false;
      if (typeof m.id !== 'string' || !/^[A-Za-z0-9_.:-]{1,48}$/.test(m.id)) return false;
      const reg = store.get('discoveries'), key = m.kind + ':' + m.id;
      const dn = cleanText(m.name, 24), by = cleanText(m.by, 16) || p.name;
      if (!dn) return false;
      if (reg[key]) { send(ws, { t: 'disc.no', kind: m.kind, id: m.id, name: reg[key].name, by: reg[key].by }); return true; }
      if (Object.keys(reg).length >= DISC_MAX) { send(ws, { t: 'disc.no', kind: m.kind, id: m.id, why: 'full' }); return true; }
      reg[key] = { kind: m.kind, id: m.id, name: dn, by: by, at: now };
      store.save('discoveries');
      broadcast({ t: 'disc', kind: m.kind, id: m.id, name: dn, by: by, at: now });
      return true;
    }
    case 'disc.list': {
      if (now - (c.listAt || 0) < 2000) return true;
      c.listAt = now;
      send(ws, { t: 'disc.all', list: Object.values(store.get('discoveries')) });
      return true;
    }
    case 'event.now': {
      const e = activeEvent(now);
      send(ws, e ? eventMsg(e, now) : { t: 'event.none' });
      return true;
    }
    default: return false;
  }
}

setInterval(function () {
  const now = Date.now();
  players.forEach(function (p) { if (now - p.last > SILENCE_MS) { try { p.ws.close(4005, 'silent'); } catch (e) { /* gone */ } p.ws.terminate(); } });
}, SWEEP_MS).unref();

scheduleEvent();
server.listen(PORT, HOST, function () { console.log('nmg-relay listening on ' + HOST + ':' + PORT + ' epoch ' + EPOCH); });
process.on('SIGTERM', function () { store.flushAll(); process.exit(0); });
process.on('SIGINT', function () { store.flushAll(); process.exit(0); });

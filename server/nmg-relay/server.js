'use strict';
/* NO MANS GOR relay. Dumb fan-out + presence, nothing persisted, nothing logged.
   Protocol and limits: docs/multiplayer-plan.md. Only dependency: ws. */
const http = require('http');
const { WebSocketServer } = require('ws');
const { handleGor } = require('./gor');

const PORT = +process.env.PORT || 8796;
const HOST = process.env.HOST || '127.0.0.1';
const MAX_CLIENTS = 32, MAX_PER_IP = 4, MAX_PAYLOAD = 768;
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
  const o = { id: p.id, name: p.name, color: p.color, s: p.s };
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

const server = http.createServer(function (req, res) {
  const local = /^(::1|127\.0\.0\.1|::ffff:127\.0\.0\.1)$/.test(req.socket.remoteAddress || '');
  if (req.url === '/healthz' && local && !req.headers['x-forwarded-for']) {
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ n: players.size }));
    return;
  }
  res.writeHead(404); res.end();
});

const wss = new WebSocketServer({ noServer: true, maxPayload: MAX_PAYLOAD });

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

  const c = { id: null, strikes: 0, tokens: BUCKET_BURST, tAt: Date.now(), overSince: 0, fireTokens: FIRE_MAX, fireAt: Date.now() };
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
      p = { ws: ws, id: m.id, name: cleanName(m.name, m.id), color: cleanColor(m.color), s: 'docked', last: now, pos: null };
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
      const pos = { x: m.x, y: m.y, z: m.z, qx: m.qx, qy: m.qy, qz: m.qz, qw: m.qw, v: Math.round(m.v * 10) / 10, st: m.st & 7, hp: Math.max(0, Math.min(255, Math.round(m.hp))) };
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
    default: return false;
  }
}

setInterval(function () {
  const now = Date.now();
  players.forEach(function (p) { if (now - p.last > SILENCE_MS) { try { p.ws.close(4005, 'silent'); } catch (e) { /* gone */ } p.ws.terminate(); } });
}, SWEEP_MS).unref();

server.listen(PORT, HOST, function () { console.log('nmg-relay listening on ' + HOST + ':' + PORT + ' epoch ' + EPOCH); });
process.on('SIGTERM', function () { process.exit(0); });

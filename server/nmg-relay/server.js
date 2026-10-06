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
const ROOM_RATE = 150, ROOM_BURST = 150;   // separate bucket for sm.in / sm.snap / pl.snap / pl.ev only
const SM_MAX_FIGHTERS = 4, SM_MAX_SPEC = 8, SM_SNAP_MAX = 2048, PL_SNAP_MAX = 4096, PL_EV_MAX = 512;
const ROOM_RE = /"t"\s*:\s*"(sm\.in|sm\.snap|pl\.snap|pl\.ev)"/;
const FIRE_MAX = 12;                 // per second per connection
const SILENCE_MS = +process.env.SILENCE_MS || 15000, SWEEP_MS = 5000, STRIKES_MAX = 5;
const EPOCH = Date.now();            // constant until restart
const ALLOW_NO_ORIGIN = process.env.ALLOW_NO_ORIGIN === '1';   // test bots only
const ORIGIN_RE = /^(https:\/\/(www\.)?emgor\.online|http:\/\/(localhost|127\.0\.0\.1)(:\d+)?)$/;

const players = new Map();           // id -> { ws, id, name, color, s, last, pos }
const rooms = new Map();             // 'sm_xxxxxx' -> { id, host, fighters:Set, spec:Set, seed }
const planets = new Map();           // planet key -> { members:Set ids, host }
const challenges = new Map();        // 'from>to' -> expiry ms
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
  const o = { id: p.id, name: p.name, color: p.color, s: p.s, hs: p.hs || '', style: p.style || '', pet: p.pet | 0, crew: p.crew | 0 };
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

function lim(c, k, rate, now) {      // per-type token bucket, burst = rate
  const b = c.lim[k] || (c.lim[k] = { t: rate, at: now });
  b.t = Math.min(rate, b.t + (now - b.at) / 1000 * rate); b.at = now;
  if (b.t < 1) return false;
  b.t -= 1; return true;
}
function toIds(ids, obj, except) { ids.forEach(function (id) { const q = players.get(id); if (q && id !== except) send(q.ws, obj); }); }
function roomAll(r) { return new Set([...r.fighters, ...r.spec]); }
function startMsg(r) { return { t: 'sm.start', room: r.id, host: r.host, members: [...r.fighters], spec: [...r.spec], seed: r.seed }; }
function endRoom(r, result, reason) {
  if (!rooms.delete(r.id)) return;
  const o = { t: 'sm.end', room: r.id, result: result == null ? null : result };
  if (reason) o.reason = reason;
  roomAll(r).forEach(function (id) { const q = players.get(id); if (q) { q.room = null; send(q.ws, o); } });
}
function leaveRoom(p) {
  const r = p.room && rooms.get(p.room);
  p.room = null;
  if (!r) return;
  if (r.host === p.id) { endRoom(r, null, 'host'); return; }
  const role = r.fighters.has(p.id) ? 'fighter' : 'spec';
  r.fighters.delete(p.id); r.spec.delete(p.id);
  toIds(roomAll(r), { t: 'sm.member', room: r.id, id: p.id, role: 'left', was: role });
}
function planetKey(v) {
  if (typeof v === 'number' && isFinite(v) && Math.abs(v) < 1e9) return String(Math.floor(v));
  return typeof v === 'string' && /^[A-Za-z0-9_.:-]{1,24}$/.test(v) ? v : '';
}
function electHost(key, force) {
  const pl = planets.get(key);
  if (!pl) return;
  if (!pl.members.size) { planets.delete(key); return; }
  const h = [...pl.members].sort()[0];
  if (h !== pl.host || force) { pl.host = h; toIds(pl.members, { t: 'pl.host', planet: key, host: h }); return true; }
  return false;
}
function leavePlanet(p) {
  const key = p.planet; p.planet = null;
  const pl = key && planets.get(key);
  if (!pl) return;
  pl.members.delete(p.id);
  electHost(key, false);
}
function dropPlayer(p) { leaveRoom(p); leavePlanet(p); }

function cnt(v) { return num(v, 1000) ? Math.max(0, Math.min(99, Math.floor(v))) : 0; }
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

  const c = { profAt: 0, id: null, strikes: 0, tokens: BUCKET_BURST, tAt: Date.now(), overSince: 0, lim: {}, rtokens: ROOM_BURST, rAt: Date.now(), rOver: 0, fireTokens: FIRE_MAX, fireAt: Date.now() };
  ws.on('error', function () { /* close follows */ });
  setTimeout(function () { if (!c.id) ws.close(4006, 'no hi'); }, 10000).unref();
  ws.on('close', function () {
    const k = (perIp.get(ip) || 1) - 1;
    if (k <= 0) perIp.delete(ip); else perIp.set(ip, k);
    const p = c.id && players.get(c.id);
    if (p && p.ws === ws) {
      dropPlayer(p);
      players.delete(c.id);
      broadcast({ t: 'bye', id: c.id });
      broadcastRoster();
    }
  });
  ws.on('message', function (data, isBinary) {
    const now = Date.now();
    const head = isBinary ? '' : data.slice(0, 48).toString();
    const roomMsg = ROOM_RE.test(head);      // room traffic: own bucket (the 't' key must come first, like prof.save)
    if (roomMsg) {
      c.rtokens = Math.min(ROOM_BURST, c.rtokens + (now - c.rAt) / 1000 * ROOM_RATE); c.rAt = now;
      if (c.rtokens < 1) {
        if (!c.rOver) c.rOver = now; else if (now - c.rOver > OVER_CLOSE_MS) ws.close(4002, 'rate');
        return;
      }
      c.rtokens -= 1; c.rOver = 0;
    } else {
      c.tokens = Math.min(BUCKET_BURST, c.tokens + (now - c.tAt) / 1000 * BUCKET_RATE); c.tAt = now;
      if (c.tokens < 1) {
        if (!c.overSince) c.overSince = now;
        else if (now - c.overSince > OVER_CLOSE_MS) { ws.close(4002, 'rate'); }
        return;
      }
      c.tokens -= 1; c.overSince = 0;
    }
    let m = null;
    const maxLen = roomMsg ? (head.indexOf('pl.snap') > 0 ? PL_SNAP_MAX : SM_SNAP_MAX) + 256 : MAX_PAYLOAD;
    if (!isBinary && data.length > maxLen && head.indexOf('"prof.save"') < 0) { if (++c.strikes >= STRIKES_MAX) ws.close(4004, 'bad'); return; }
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
      p = { ws: ws, id: m.id, name: cleanName(m.name, m.id), color: cleanColor(m.color), s: 'docked', last: now, pos: null, hs: (typeof m.hs === 'string' ? m.hs.slice(0, 32) : ''), style: cleanText(m.style, 16), pet: cnt(m.pet), crew: cnt(m.crew), room: null, planet: null };
      players.set(m.id, p);
    } else {
      p = players.get(c.id);
      if (!p || p.ws !== ws) return false;
      p.name = cleanName(m.name, c.id); p.color = cleanColor(m.color); p.last = now; p.style = cleanText(m.style, 16); p.pet = cnt(m.pet); p.crew = cnt(m.crew);
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
      const pos = { x: m.x, y: m.y, z: m.z, qx: m.qx, qy: m.qy, qz: m.qz, qw: m.qw, v: Math.round(m.v * 10) / 10, st: m.st & 31, hp: Math.max(0, Math.min(255, Math.round(m.hp))) };
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
    case 'sm.challenge': {
      if (typeof m.to !== 'string' || m.to === p.id) return false;
      const q = players.get(m.to);
      if (!q) { send(ws, { t: 'sm.no', why: 'gone', to: m.to }); return true; }
      if (!lim(c, 'ch', 2, now)) return true;
      challenges.set(p.id + '>' + m.to, now + 30000);
      send(q.ws, { t: 'sm.challenge', from: p.id, name: p.name });
      return true;
    }
    case 'sm.accept': {
      if (typeof m.from !== 'string') return false;
      const k = m.from + '>' + p.id, exp = challenges.get(k), h = players.get(m.from);
      challenges.delete(k);
      if (!exp || exp < now || !h) { send(ws, { t: 'sm.no', why: 'expired', from: m.from }); return true; }
      if (h.room || p.room) { send(ws, { t: 'sm.no', why: 'busy', from: m.from }); return true; }
      const r = { id: 'sm_' + crypto.randomBytes(4).toString('hex'), host: h.id, fighters: new Set([h.id, p.id]), spec: new Set(), seed: crypto.randomBytes(4).readUInt32BE(0) };
      rooms.set(r.id, r); h.room = p.room = r.id;
      toIds(r.fighters, startMsg(r));
      return true;
    }
    case 'sm.join': {
      const r = typeof m.room === 'string' && rooms.get(m.room);
      if (!r) { send(ws, { t: 'sm.no', why: 'none', room: m.room }); return true; }
      if (p.room) return true;
      const fight = m.fight === true && r.fighters.size < SM_MAX_FIGHTERS;
      if (!fight && r.spec.size >= SM_MAX_SPEC) { send(ws, { t: 'sm.no', why: 'full', room: r.id }); return true; }
      (fight ? r.fighters : r.spec).add(p.id); p.room = r.id;
      send(ws, startMsg(r));
      toIds(roomAll(r), { t: 'sm.member', room: r.id, id: p.id, name: p.name, role: fight ? 'fighter' : 'spec' }, p.id);
      return true;
    }
    case 'sm.leave': leaveRoom(p); return true;
    case 'sm.in': {                      // guest -> host only
      const r = p.room && p.room === m.room && rooms.get(m.room);
      if (!r || r.host === p.id || !r.fighters.has(p.id) || !num(m.f, 1e9) || !num(m.m, 65536)) return false;
      if (!lim(c, 'in', 60, now)) return true;
      const h = players.get(r.host);
      if (h) send(h.ws, { t: 'sm.in', room: r.id, id: p.id, f: m.f, m: m.m });
      return true;
    }
    case 'sm.snap': {                    // host -> everyone else in the room
      const r = p.room && p.room === m.room && rooms.get(m.room);
      if (!r || r.host !== p.id || !num(m.f, 1e9) || m.data == null) return false;
      const txt = JSON.stringify(m.data);
      if (txt.length > SM_SNAP_MAX) return false;
      if (!lim(c, 'snap', 30, now)) return true;
      toIds(roomAll(r), { t: 'sm.snap', room: r.id, f: m.f, data: m.data }, p.id);
      return true;
    }
    case 'sm.end': {                     // host only
      const r = p.room && p.room === m.room && rooms.get(m.room);
      if (!r || r.host !== p.id) return false;
      const res = m.result === undefined ? null : m.result;
      if (JSON.stringify(res === undefined ? null : res).length > 512) return false;
      endRoom(r, res);
      return true;
    }
    case 'pl.enter': {
      const key = planetKey(m.planet);
      if (!key) return false;
      if (p.planet === key) return true;
      if (!lim(c, 'ple', 4, now)) return true;
      leavePlanet(p);
      let pl = planets.get(key);
      if (!pl) planets.set(key, pl = { members: new Set(), host: null });
      pl.members.add(p.id); p.planet = key;
      electHost(key, false);
      send(ws, { t: 'pl.host', planet: key, host: pl.host });     // joiner always learns the host
      return true;
    }
    case 'pl.leave': leavePlanet(p); return true;
    case 'pl.snap': {                    // host only, <= 10/s, <= 4 KB
      const pl = p.planet && planets.get(p.planet);
      if (!pl || pl.host !== p.id || planetKey(m.planet) !== p.planet || m.data == null) return false;
      if (JSON.stringify(m.data).length > PL_SNAP_MAX) return false;
      if (!lim(c, 'pls', 10, now)) return true;
      toIds(pl.members, { t: 'pl.snap', planet: p.planet, data: m.data }, p.id);
      return true;
    }
    case 'pl.ev': {                      // any member, <= 5/s, <= 512 B
      const pl = p.planet && planets.get(p.planet);
      if (!pl || planetKey(m.planet) !== p.planet || m.data == null) return false;
      if (JSON.stringify(m.data).length > PL_EV_MAX) return false;
      if (!lim(c, 'ple2', 5, now)) return true;
      toIds(pl.members, { t: 'pl.ev', planet: p.planet, id: p.id, data: m.data }, p.id);
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

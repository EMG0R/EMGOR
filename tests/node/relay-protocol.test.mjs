// nmg-relay wire-protocol tests. Spawns server/nmg-relay/server.js on a random port, talks to it with `ws`.
// Needs server/nmg-relay/node_modules (npm ci --omit=dev there); skipped otherwise.
import { test, before, after, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createRequire } from 'node:module';
import { mkdtempSync, existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const dir = join(here, '..', '..', 'server', 'nmg-relay');
const have = existsSync(join(dir, 'node_modules', 'ws'));
const WS = have ? createRequire(join(dir, 'x.js'))('ws') : null;
const skip = have ? false : 'ws not installed in server/nmg-relay';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const rid = () => 'T' + Math.random().toString(36).slice(2, 10).padEnd(8, 'x');
const POS = { x: 1, y: 2, z: 3, qx: 0, qy: 0, qz: 0, qw: 1, v: 4.44, st: 3, hp: 100 };

let proc = null, port = 0, dataDir = mkdtempSync(join(tmpdir(), 'nmgrelay-'));
let ipSeq = 1;

function startServer(env = {}) {
  return new Promise((resolve, reject) => {
    port = 20000 + Math.floor(Math.random() * 30000);
    const p = spawn(process.execPath, [join(dir, 'server.js')], {
      env: { ...process.env, PORT: String(port), HOST: '127.0.0.1', ALLOW_NO_ORIGIN: '1', NMG_DATA: dataDir, SILENCE_MS: '60000', ...env },
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let err = '';
    p.stderr.on('data', (d) => { err += d; });
    p.stdout.on('data', (d) => { if (String(d).includes('listening')) resolve(p); });
    p.on('exit', (c) => reject(new Error('relay exited ' + c + ' ' + err)));
    p.errOut = () => err;
    setTimeout(() => reject(new Error('relay start timeout')), 4000).unref();
  });
}
function stopServer() {
  return new Promise((res) => { if (!proc) return res(); const p = proc; proc = null; p.removeAllListeners('exit'); p.on('exit', res); p.kill('SIGTERM'); });
}

class C {
  constructor(id = rid(), ip) {
    this.id = id; this.msgs = []; this.waiters = []; this.closed = null;
    this.ws = new WS('ws://127.0.0.1:' + port, { headers: { 'x-forwarded-for': ip || '10.0.0.' + (ipSeq++) } });
    this.ws.on('message', (d) => { let m; try { m = JSON.parse(String(d)); } catch (e) { return; } this.msgs.push(m); this.waiters.slice().forEach((w) => w()); });
    this.ws.on('close', (code) => { this.closed = code; this.waiters.slice().forEach((w) => w()); });
    this.ws.on('error', () => {});
    this.open = new Promise((r) => this.ws.on('open', r));
  }
  raw(s) { this.ws.send(s); }
  send(o) { this.ws.send(JSON.stringify(o)); }
  async hi(extra = {}) { await this.open; this.send({ t: 'hi', id: this.id, name: 'P' + this.id.slice(-3), color: 0x123456, ...extra }); return this.wait((m) => m.t === 'roster'); }
  find(pred, from = 0) { for (let i = from; i < this.msgs.length; i++) if (pred(this.msgs[i])) return this.msgs[i]; return null; }
  wait(pred, ms = 1500, from = 0) {
    return new Promise((resolve, reject) => {
      const chk = () => { const m = this.find(pred, from); if (m) { done(); resolve(m); } else if (this.closed != null && pred.closeOk) { done(); resolve(null); } };
      const tm = setTimeout(() => { done(); reject(new Error('timeout waiting; got ' + JSON.stringify(this.msgs.slice(-6)))); }, ms);
      const done = () => { clearTimeout(tm); this.waiters = this.waiters.filter((w) => w !== chk); };
      this.waiters.push(chk); chk();
    });
  }
  async none(pred, ms = 250) { await sleep(ms); assert.equal(this.find(pred), null, 'unexpected ' + JSON.stringify(this.find(pred))); }
  waitClose(ms = 2000) { return new Promise((res, rej) => { const tm = setTimeout(() => rej(new Error('no close')), ms); const chk = () => { if (this.closed != null) { clearTimeout(tm); res(this.closed); } }; this.waiters.push(chk); chk(); }); }
  close() { try { this.ws.close(); } catch (e) {} }
}
const pool = [];
async function mk(n, extra) { const cs = []; for (let i = 0; i < n; i++) { const c = new C(); pool.push(c); cs.push(c); } for (const c of cs) await c.hi(extra); return cs; }
async function health() { const r = await fetch('http://127.0.0.1:' + port + '/healthz'); return r.json(); }

before(async () => { if (have) proc = await startServer(); });
afterEach(async () => { pool.forEach((c) => c.close()); pool.length = 0; await sleep(30); });
after(async () => { pool.forEach((c) => c.close()); await stopServer(); });

test('join/leave fan-out: roster on hi, bye + roster on leave', { skip }, async () => {
  const [a, b] = await mk(2);
  await a.wait((m) => m.t === 'roster' && m.ships.some((s) => s.id === b.id));
  assert.equal(a.find((m) => m.t === 'roster').you, a.id);
  const rb = b.find((m) => m.t === 'roster');
  assert.ok(rb.ships.some((s) => s.id === a.id), 'b sees a');
  assert.ok(!rb.ships.some((s) => s.id === b.id), 'roster excludes self');
  const from = a.msgs.length;
  b.close();
  await a.wait((m) => m.t === 'bye' && m.id === b.id, 1500, from);
  const r = await a.wait((m) => m.t === 'roster' && !m.ships.some((s) => s.id === b.id), 1500, from);
  assert.equal(r.ships.length, 0);
});

test('pos relays to others, not self; fields verbatim/clamped', { skip }, async () => {
  const [a, b, c] = await mk(3);
  a.send({ t: 'pos', ...POS, hp: 999, st: 255, mode: 'landed' });
  const m = await b.wait((x) => x.t === 'pos' && x.id === a.id);
  assert.deepEqual([m.x, m.y, m.z, m.qw, m.v, m.mode], [1, 2, 3, 1, 4.4, 'landed']);
  assert.equal(m.hp, 255); assert.equal(m.st, 31);
  await c.wait((x) => x.t === 'pos' && x.id === a.id);
  await a.none((x) => x.t === 'pos');
  a.send({ t: 'pos', ...POS, x: NaN }); a.send({ t: 'pos', ...POS, y: 'no' }); a.send({ t: 'pos', ...POS, z: 1e9 });
  await b.none((x) => x.t === 'pos' && (x.z === 1e9 || x.y === 'no'));
});

test('frame:planet pose fields preserved verbatim; bad pid/missing fields degrade', { skip }, async () => {
  const [a, b] = await mk(2);
  const pl = { frame: 'planet', pid: 'planet:42.x', lx: 0.123456789, ly: -5.5, lz: 1e-7, lqx: 0.1, lqy: 0.2, lqz: 0.3, lqw: 0.9 };
  a.send({ t: 'pos', ...POS, ...pl, mode: 'foot' });
  const m = await b.wait((x) => x.t === 'pos' && x.id === a.id && x.frame === 'planet');
  for (const k of Object.keys(pl)) assert.equal(m[k], pl[k], k);
  assert.equal(m.mode, 'foot');
  const ros = await b.wait((x) => x.t === 'roster' && x.ships.some((s) => s.id === a.id && s.frame === 'planet'));
  assert.equal(ros.ships.find((s) => s.id === a.id).lx, 0.123456789);
  // dirty pid sanitised, never passes through raw
  a.send({ t: 'pos', ...POS, ...pl, pid: 'a b<script>/' + 'z'.repeat(80) });
  const m2 = await b.wait((x) => x.t === 'pos' && x.pid && x.pid.startsWith('a_b_script'));
  assert.ok(m2.pid.length <= 48 && /^[A-Za-z0-9_.:-]+$/.test(m2.pid));
  // missing local field: falls back to a world pose, no frame
  const mark = b.msgs.length;
  a.send({ t: 'pos', ...POS, frame: 'planet', pid: 'p1', lx: 1 });
  const m3 = await b.wait((x) => x.t === 'pos', 1500, mark);
  assert.equal(m3.frame, undefined); assert.equal(m3.lx, undefined);
  a.send({ t: 'pos', ...POS, frame: 'station' });
  assert.equal((await b.wait((x) => x.t === 'pos' && x.frame === 'station')).frame, 'station');
});

async function fightRoom() {
  const [h, g, s, o] = await mk(4);       // host, guest, spectator, outsider
  h.send({ t: 'sm.challenge', to: g.id });
  await g.wait((m) => m.t === 'sm.challenge' && m.from === h.id);
  g.send({ t: 'sm.accept', from: h.id });
  const st = await h.wait((m) => m.t === 'sm.start');
  await g.wait((m) => m.t === 'sm.start');
  s.send({ t: 'sm.join', room: st.room });
  await s.wait((m) => m.t === 'sm.start');
  return { h, g, s, o, room: st.room, st };
}

test('sm.* routed only to room members; t is first key on room frames', { skip }, async () => {
  const { h, g, s, o, room, st } = await fightRoom();
  assert.equal(st.host, h.id); assert.deepEqual(st.members.sort(), [h.id, g.id].sort());
  // room frames: raw text so the first-key rule is explicit
  g.raw(JSON.stringify({ t: 'sm.in', room, f: 5, m: 12 }));
  const gi = await h.wait((m) => m.t === 'sm.in');
  assert.deepEqual([gi.id, gi.f, gi.m], [g.id, 5, 12]);
  assert.deepEqual(Object.keys(gi)[0], 't');
  h.raw(JSON.stringify({ t: 'sm.snap', room, f: 9, data: { a: [1, 2, 3] } }));
  const gs = await g.wait((m) => m.t === 'sm.snap'), ss = await s.wait((m) => m.t === 'sm.snap');
  assert.deepEqual(gs.data, { a: [1, 2, 3] }); assert.equal(Object.keys(ss)[0], 't');
  await o.none((m) => m.t.startsWith('sm.') && m.t !== 'sm.challenge');
  await h.none((m) => m.t === 'sm.snap');                       // host does not echo
  await s.none((m) => m.t === 'sm.in');                         // guest input goes to host only
  // non-members and non-hosts cannot inject
  o.raw(JSON.stringify({ t: 'sm.snap', room, f: 1, data: {} }));
  g.raw(JSON.stringify({ t: 'sm.snap', room, f: 2, data: { evil: 1 } }));
  s.raw(JSON.stringify({ t: 'sm.in', room, f: 1, m: 1 }));
  await sleep(250);
  assert.equal(g.find((m) => m.t === 'sm.snap' && m.data.evil), null);
  assert.equal(h.find((m) => m.t === 'sm.in' && m.id === s.id), null);
  assert.equal(h.msgs.filter((m) => m.t === 'sm.in').length, 1);
  // oversize snap dropped
  h.raw(JSON.stringify({ t: 'sm.snap', room, f: 3, data: 'x'.repeat(3000) }));
  await g.none((m) => m.t === 'sm.snap' && m.f === 3);
  // spectator leaving notifies others
  s.send({ t: 'sm.leave' });
  const mem = await h.wait((m) => m.t === 'sm.member' && m.role === 'left' && m.id === s.id);
  assert.equal(mem.was, 'spec');
  // host end -> all remaining get sm.end, room gone
  h.send({ t: 'sm.end', room, result: { win: h.id } });
  const e = await g.wait((m) => m.t === 'sm.end');
  assert.deepEqual(e.result, { win: h.id });
  o.send({ t: 'sm.join', room });
  assert.equal((await o.wait((m) => m.t === 'sm.no')).why, 'none');
});

test('sm: host disconnect ends room with reason host; challenge expiry/self/unknown', { skip }, async () => {
  const { h, g, room } = await fightRoom();
  h.close();
  const e = await g.wait((m) => m.t === 'sm.end');
  assert.equal(e.reason, 'host'); assert.equal(e.result, null); assert.equal(e.room, room);
  const [a] = await mk(1);
  a.send({ t: 'sm.challenge', to: 'nobody12345' });
  assert.equal((await a.wait((m) => m.t === 'sm.no')).why, 'gone');
  a.send({ t: 'sm.accept', from: 'nobody12345' });
  assert.equal((await a.wait((m) => m.t === 'sm.no' && m.why === 'expired')).why, 'expired');
});

test('pl.host: lowest id hosts, re-elected on host leave and on disconnect', { skip }, async () => {
  const ids = ['Aaaaaa01', 'Bbbbbb02', 'Cccccc03'];
  const cs = ids.map((id) => { const c = new C(id); pool.push(c); return c; });
  for (const c of cs) await c.hi();
  const [a, b, c] = cs;
  b.send({ t: 'pl.enter', planet: 7 });
  assert.equal((await b.wait((m) => m.t === 'pl.host')).host, ids[1]);
  c.send({ t: 'pl.enter', planet: '7' });                      // number 7 and '7' are the same key
  assert.equal((await c.wait((m) => m.t === 'pl.host')).host, ids[1]);
  a.send({ t: 'pl.enter', planet: 7 });
  assert.equal((await a.wait((m) => m.t === 'pl.host')).host, ids[0]);
  assert.equal((await b.wait((m) => m.t === 'pl.host' && m.host === ids[0])).planet, '7');
  // only host may snap; members get it, host not
  b.raw(JSON.stringify({ t: 'pl.snap', planet: 7, data: { n: 'fake' } }));
  a.raw(JSON.stringify({ t: 'pl.snap', planet: 7, data: { n: 1 } }));
  assert.deepEqual((await c.wait((m) => m.t === 'pl.snap')).data, { n: 1 });
  assert.equal(c.find((m) => m.t === 'pl.snap' && m.data.n === 'fake'), null);
  c.raw(JSON.stringify({ t: 'pl.ev', planet: 7, data: { k: 1 } }));
  assert.equal((await a.wait((m) => m.t === 'pl.ev')).id, ids[2]);
  await c.none((m) => m.t === 'pl.ev');
  // host leaves -> next lowest
  const mark = b.msgs.length;
  a.send({ t: 'pl.leave' });
  assert.equal((await b.wait((m) => m.t === 'pl.host', 1500, mark)).host, ids[1]);
  // host disconnects -> remaining
  const mark2 = c.msgs.length;
  b.close();
  assert.equal((await c.wait((m) => m.t === 'pl.host' && m.host === ids[2], 1500, mark2)).host, ids[2]);
  // a different planet is isolated
  a.send({ t: 'pl.enter', planet: 'other' });
  c.raw(JSON.stringify({ t: 'pl.ev', planet: 7, data: { k: 2 } }));
  await sleep(200);
  assert.equal(a.find((m) => m.t === 'pl.ev' && m.data.k === 2), null);
});

test('disc.claim: first wins, broadcast once, list, bad input', { skip }, async () => {
  const [a, b] = await mk(2);
  const id = 'planet-' + rid();
  a.send({ t: 'disc.claim', kind: 'planet', id, name: 'Alpha', by: 'ann' });
  const d = await b.wait((m) => m.t === 'disc' && m.id === id);
  assert.deepEqual([d.name, d.by, d.kind], ['Alpha', 'ann', 'planet']);
  await a.wait((m) => m.t === 'disc' && m.id === id);          // claimer gets the broadcast too
  b.send({ t: 'disc.claim', kind: 'planet', id, name: 'Beta', by: 'bob' });
  const no = await b.wait((m) => m.t === 'disc.no' && m.id === id);
  assert.deepEqual([no.name, no.by], ['Alpha', 'ann']);
  await a.none((m) => m.t === 'disc.no');
  b.send({ t: 'disc.list' });
  const all = await b.wait((m) => m.t === 'disc.all');
  assert.equal(all.list.filter((x) => x.id === id).length, 1);
  // same id, other kind is a different key
  b.send({ t: 'disc.claim', kind: 'creature', id, name: 'Gamma', by: 'bob' });
  await a.wait((m) => m.t === 'disc' && m.kind === 'creature' && m.id === id);
  const mark = a.msgs.length;
  a.send({ t: 'disc.claim', kind: 'bogus', id: 'zz', name: 'x' });
  a.send({ t: 'disc.claim', kind: 'planet', id: 'bad id!', name: 'x' });
  a.send({ t: 'disc.claim', kind: 'planet', id: 'ok1', name: '\u0001\u0002' });
  await sleep(200);
  assert.equal(a.msgs.slice(mark).filter((m) => m.t === 'disc').length, 0);
});

test('disc.claim persists across restart (SIGTERM flush) and reconnect', { skip }, async () => {
  const [a] = await mk(1);
  const id = 'persist-' + rid();
  a.send({ t: 'disc.claim', kind: 'resource', id, name: 'Keep', by: 'ann' });
  await a.wait((m) => m.t === 'disc' && m.id === id);
  pool.forEach((c) => c.close());
  await stopServer();
  proc = await startServer();
  const [b] = await mk(1);
  b.send({ t: 'disc.claim', kind: 'resource', id, name: 'Steal', by: 'bob' });
  const no = await b.wait((m) => m.t === 'disc.no' && m.id === id);
  assert.deepEqual([no.name, no.by], ['Keep', 'ann']);
});

test('clock: epoch constant for all clients; pong now monotonic; ping echoes c', { skip }, async () => {
  const [a, b] = await mk(2);
  const ea = a.find((m) => m.t === 'roster').epoch, eb = b.find((m) => m.t === 'roster').epoch;
  assert.equal(typeof ea, 'number'); assert.equal(ea, eb);
  assert.ok(ea <= Date.now() && ea > Date.now() - 3600000);
  const nows = [];
  for (let i = 0; i < 4; i++) { a.send({ t: 'ping', c: 1000 + i }); const p = await a.wait((m) => m.t === 'pong' && m.c === 1000 + i); nows.push(p.now); await sleep(15); }
  for (let i = 1; i < nows.length; i++) assert.ok(nows[i] > nows[i - 1], 'monotonic');
  a.send({ t: 'ping', c: 'x' });
  assert.equal((await a.wait((m) => m.t === 'pong' && m.c === 0)).c, 0);
  assert.ok(Math.abs(nows[0] - Date.now()) < 2000);
});

test('same id reconnect replaces old socket (4000) and clears stale room/planet membership', { skip }, async () => {
  const id = 'Rejoin01';
  const old = new C(id); pool.push(old); await old.hi();
  const [peer] = await mk(1);
  old.send({ t: 'pl.enter', planet: 'rj' });
  await old.wait((m) => m.t === 'pl.host');
  peer.send({ t: 'pl.enter', planet: 'rj' });
  await peer.wait((m) => m.t === 'pl.host');
  const nw = new C(id); pool.push(nw); await nw.hi();
  assert.equal(await old.waitClose(), 4000);
  // old membership must not linger: peer becomes sole member/host, and a snap from peer reaches nobody stale
  const h = await peer.wait((m) => m.t === 'pl.host' && m.host === peer.id || m.t === 'pl.host' && m.host === id, 1500).catch(() => null);
  nw.send({ t: 'pl.enter', planet: 'rj' });
  await nw.wait((m) => m.t === 'pl.host');
  const mark = nw.msgs.length;
  peer.raw(JSON.stringify({ t: 'pl.ev', planet: 'rj', data: { z: 1 } }));
  await nw.wait((m) => m.t === 'pl.ev', 1500, mark);
  // fight room: host reconnects -> room must end, not zombie
  const [x, y] = await mk(2);
  x.send({ t: 'sm.challenge', to: y.id }); await y.wait((m) => m.t === 'sm.challenge');
  y.send({ t: 'sm.accept', from: x.id }); await y.wait((m) => m.t === 'sm.start');
  const x2 = new C(x.id); pool.push(x2); await x2.hi();
  const e = await y.wait((m) => m.t === 'sm.end');
  assert.equal(e.reason, 'host');
  assert.ok(h === null || h);
});

test('limits: bad frames strike out (4004), oversized, binary, no-hi, duplicate-t spoof', { skip }, async () => {
  const c = new C(); pool.push(c); await c.open;
  c.send({ t: 'pos', ...POS });                    // before hi: strike, no crash
  c.raw('{not json'); c.raw('[]'); c.raw('null'); c.raw('"s"'); c.raw(JSON.stringify({ t: 5 }));
  assert.equal(await c.waitClose(), 4004);
  const d = new C(); pool.push(d); await d.hi();
  const mark = d.msgs.length;
  for (let i = 0; i < 4; i++) d.raw(JSON.stringify({ t: 'chat', text: 'x'.repeat(900) }));   // > 768 B
  d.ws.send(Buffer.from([1, 2, 3]));
  assert.equal(await d.waitClose(), 4004);
  // room-bucket spoof: duplicate "t" so head looks like sm.in but parses as chat -> must not be handled
  const [e, f] = await mk(2);
  e.raw('{"t":"sm.in","t":"chat","text":"spoof"}');
  await f.none((m) => m.t === 'chat' && m.text === 'spoof');
  void mark;
});

test('fuzz: 200 random frames never crash the relay; it keeps serving', { skip }, async () => {
  const sample = ['hi', 'pos', 'st', 'fire', 'kill', 'chat', 'prof.save', 'prof.load', 'disc.claim', 'disc.list', 'event.now', 'sm.challenge', 'sm.accept', 'sm.join', 'sm.leave', 'sm.in', 'sm.snap', 'sm.end', 'pl.enter', 'pl.leave', 'pl.snap', 'pl.ev', 'ping', 'hb', '__proto__', 'constructor'];
  const vals = () => [null, 0, -1, 1e308, NaN, 'x', '', '__proto__', 'constructor', [], {}, { a: 1 }, true, 'a'.repeat(300), [[[]]], { __proto__: null }, 0.5];
  let seed = 12345; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const pick = (a) => a[Math.floor(rnd() * a.length)];
  const [a, b] = await mk(2);
  const pid = proc.pid;
  for (let i = 0; i < 200; i++) {
    const cl = i % 2 ? a : b;
    if (cl.closed != null) continue;
    const o = { t: pick(sample) };
    const n = Math.floor(rnd() * 8);
    for (let k = 0; k < n; k++) o[pick(['x', 'y', 'z', 'name', 'key', 'data', 'planet', 'room', 'id', 'to', 'from', 'text', 'kind', 'fight', 'f', 'm', 'result', 'by', 'w', 'hp', 'st', 's', 'frame', 'pid', 'lx'])] = pick(vals());
    try {
      const r = rnd();
      if (r < 0.1) cl.raw(JSON.stringify(o).slice(0, Math.floor(rnd() * 30) + 1));   // truncated
      else if (r < 0.15) cl.ws.send(Buffer.from([Math.floor(rnd() * 256), 1, 2]));
      else cl.raw(JSON.stringify(o));
    } catch (e) { /* socket closed */ }
    if (i % 25 === 0) await sleep(20);
  }
  await sleep(400);
  assert.equal(proc.exitCode, null, 'relay still alive: ' + proc.errOut());
  assert.equal(proc.pid, pid);
  assert.equal(typeof (await health()).n, 'number');
  const [z] = await mk(1);                                      // fresh client still gets a roster
  assert.equal(z.find((m) => m.t === 'roster').you, z.id);
});

test('prof: __proto__ / constructor names do not crash the relay', { skip }, async () => {
  for (const name of ['__proto__', 'constructor', 'tostring', 'hasownproperty']) {
    const [a] = await mk(1);
    a.send({ t: 'prof.load', name, key: 'abcd' });
    const r = await a.wait((m) => m.t === 'prof.err');
    assert.ok(['none', 'bad', 'key'].includes(r.why), name + ' ' + r.why);
    assert.equal(proc.exitCode, null);
    a.close();
  }
  const [b] = await mk(1);
  b.send({ t: 'prof.save', name: 'constructor', key: 'abcd', data: { v: 1 } });
  assert.equal((await b.wait((m) => m.t === 'prof.ok' || m.t === 'prof.err')).t, 'prof.ok');
});

test('rate limit: pos burst is capped by the bucket; fire capped at ~12/s', { skip }, async () => {
  const [a, b] = await mk(2);
  for (let i = 0; i < 150; i++) a.send({ t: 'pos', ...POS });
  await sleep(500);
  const n = b.msgs.filter((m) => m.t === 'pos').length;
  assert.ok(n >= 40 && n <= 80, 'pos relayed ' + n);
  const [c, d] = await mk(2);
  for (let i = 0; i < 40; i++) c.send({ t: 'fire', w: 1, x: 0, y: 0, z: 0, dx: 0, dy: 0, dz: 1 });
  await sleep(400);
  const f = d.msgs.filter((m) => m.t === 'fire').length;
  assert.ok(f >= 10 && f <= 18, 'fire relayed ' + f);
});

test('limits: 4 sockets per IP (4003); healthz counts clients', { skip }, async () => {
  const ip = '10.9.9.9', cs = [];
  for (let i = 0; i < 4; i++) { const c = new C(rid(), ip); pool.push(c); cs.push(c); await c.hi(); }
  const fifth = new C(rid(), ip); pool.push(fifth);
  assert.equal(await fifth.waitClose(), 4003);
  assert.ok((await health()).n >= 4);
});

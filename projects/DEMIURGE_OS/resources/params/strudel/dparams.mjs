// dparams.mjs -- join the Demiurge parameter pool from node / Strudel. No npm deps (node:dgram).
// Contract: docs/parameters.md (§1 0..1 floats, §11 two-way).
//
//   import { init, add, get, out, close } from './dparams.mjs';
//   init({ stage: 'strudel', port: 0 });      // port 0 -> env DEMIURGE_PARAM_PORT
//   add('/strudel/cutoff', 0.5);              // declare + default
//   get('/strudel/cutoff')                    // 0..1, latest /p from the pool (sync, cheap)
//   out('/strudel/level', 0.3);               // report back: /pout to the pool
//
// In a Strudel pattern (Strudel runs in node/browser JS; use this module in the node host
// that evaluates patterns and expose get as a global, e.g. globalThis.dp = get):
//   s("bd*4").lpf(dp('/strudel/cutoff') * 8000)      // read per evaluation/cycle
//   // or a signal:  const cut = signal(() => dp('/strudel/cutoff'))
// (A browser page has no UDP; there the host process forwards values over its own socket.)
//
// Env: DEMIURGE_PARAM_PORT (listen), DEMIURGE_POOL_HOST (127.0.0.1), DEMIURGE_POOL_PORT (9102).
import dgram from 'node:dgram';

const vals = new Map();
let sock = null, poolHost = '127.0.0.1', poolPort = 9102;

// OSC string: bytes + NUL, zero-padded to a multiple of 4
const str = (s) => { const b = Buffer.from(s); const o = Buffer.alloc((b.length + 4) & ~3); b.copy(o); return o; };

function encode(addr, path, v) {
  const f = Buffer.alloc(4); f.writeFloatBE(v);
  return Buffer.concat([str(addr), str(',sf'), str(path), f]);
}

function readStr(b, i) {
  const j = b.indexOf(0, i);
  return [b.toString('utf8', i, j), (j + 4) & ~3];
}

// -> [address, args[]] or null
export function decode(b) {
  try {
    let [addr, i] = readStr(b, 0);
    let [tags, k] = readStr(b, i);
    const args = [];
    for (const t of tags.slice(1)) {
      if (t === 's') { const [s, n] = readStr(b, k); args.push(s); k = n; }
      else if (t === 'f') { args.push(b.readFloatBE(k)); k += 4; }
      else if (t === 'd') { args.push(b.readDoubleBE(k)); k += 8; }
      else if (t === 'i') { args.push(b.readInt32BE(k)); k += 4; }
      else return null;
    }
    return [addr, args];
  } catch { return null; }
}

export function init({ stage = 'strudel', port = 0 } = {}) {
  if (sock) return;
  port = port > 0 ? port : parseInt(process.env.DEMIURGE_PARAM_PORT || '0', 10);
  poolHost = process.env.DEMIURGE_POOL_HOST || '127.0.0.1';
  poolPort = parseInt(process.env.DEMIURGE_POOL_PORT || '9102', 10);
  sock = dgram.createSocket({ type: 'udp4', reuseAddr: true });
  sock.on('error', (e) => console.error('dparams:', e.message));
  sock.on('message', (m) => {
    const d = decode(m);
    if (!d || d[0] !== '/p' || d[1].length !== 2 || typeof d[1][0] !== 'string') return;
    const [p, v] = d[1];
    if (vals.has(p) && Number.isFinite(v)) vals.set(p, Math.min(1, Math.max(0, v)));
  });
  if (port > 0) sock.bind(port, '127.0.0.1');
  else { console.error('dparams: no port (init arg or DEMIURGE_PARAM_PORT)'); sock.bind(0, '127.0.0.1'); }
  sock.unref?.();   // never keeps a host process alive by itself
}

export const add = (path, dflt = 0) => { vals.set(path, dflt); return dflt; };
export const get = (path) => vals.get(path) ?? 0;
export function out(path, v) {
  if (!sock) init();
  sock.send(encode('/pout', path, v), poolPort, poolHost);
}
export function close() { sock?.close(); sock = null; }
